#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
# SPDX-License-Identifier: GPL-2.0-or-later
"""Build the parity test environment: its own build, prefix and Qt search paths.

Nothing is built into, installed into or loaded from the live system: the builder prefix
(<builder>/state/install-stage/usr) is only read, for the SonicDE frameworks to build and run against; Qt sees
only the plugins and QML modules of the dev-qt packages, never the installed SonicDE
packages under /usr. Layout of --env:
    build/           sonic-terminal built with KONSOLE_PARITY_HOOKS=ON (binaries run from here)
    stage/           DESTDIR install of that build, for its data files
    qt/plugins, qt/qml   symlinks to the dev-qt-owned files under Qt's own directories
    builder-plugins/ the builder prefix's plugins without the ones sonic-terminal installs
    bin/sonic-win    copy of the builder's compositor, next to its qt.conf
    bin/keditbookmarks   a stub that fails to start (Edit Bookmarks' error box)
    xdg/mime         link to /usr/share/mime (shared-mime-info, an OS package)
    forbidden.txt    files of installed sonicde-* packages; loading any of them fails a run
"""

import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
SOURCE = HERE.parent.parent


def qt_paths():
    output = subprocess.run(["qtpaths6", "--query"], capture_output=True, text=True, check=True).stdout
    return dict(line.split(":", 1) for line in output.splitlines() if ":" in line)


def owners(files):
    """Package owning each file, from Portage (qfile)."""
    result = {}
    files = [str(f) for f in files]
    for start in range(0, len(files), 500):
        output = subprocess.run(["qfile", "-C", *files[start:start + 500]], capture_output=True, text=True).stdout
        for line in output.splitlines():
            package, path = line.split(": ", 1)
            result[path] = package
    return result


def mirror_qt(source, target):
    """Symlink only the files that dev-qt packages own, keeping the directory layout."""
    files = [p for p in Path(source).rglob("*") if p.is_file()]
    owned = owners(files)
    kept = 0
    for path in files:
        if owned.get(str(path), "").startswith("dev-qt/"):
            link = target / path.relative_to(source)
            link.parent.mkdir(parents=True, exist_ok=True)
            link.symlink_to(path)
            kept += 1
    print(f"{target.name}: {kept} of {len(files)} files are Qt's own")


def write_qt_conf(directory, paths, env):
    keys = {"Prefix": "QT_INSTALL_PREFIX", "ArchData": "QT_INSTALL_ARCHDATA", "Data": "QT_INSTALL_DATA",
            "Documentation": "QT_INSTALL_DOCS", "Headers": "QT_INSTALL_HEADERS", "Libraries": "QT_INSTALL_LIBS",
            "LibraryExecutables": "QT_INSTALL_LIBEXECS", "Binaries": "QT_INSTALL_BINS",
            "Translations": "QT_INSTALL_TRANSLATIONS", "Examples": "QT_INSTALL_EXAMPLES", "Tests": "QT_INSTALL_TESTS"}
    lines = ["[Paths]"] + [f"{key} = {paths[query]}" for key, query in keys.items()]
    lines += [f"Plugins = {env / 'qt/plugins'}", f"QmlImports = {env / 'qt/qml'}"]
    (directory / "qt.conf").write_text("\n".join(lines) + "\n")


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--env", required=True, type=Path, help="environment directory (e.g. <builder>/state/parity/env)")
    parser.add_argument("--prefix", required=True, type=Path, help="builder prefix, read only (<builder>/state/install-stage/usr)")
    parser.add_argument("--jobs", default=str(os.cpu_count()))
    parser.add_argument("--build", default="build",
                        help="build directory name inside --env; a second tree (the phase 0 copy) uses build-phase0")
    args = parser.parse_args()
    env, prefix = args.env.resolve(), args.prefix.resolve()
    stage = env / ("stage" if args.build == "build" else f"stage-{args.build}")
    for part in (stage, env / "qt", env / "builder-plugins", env / "bin", env / "xdg"):
        shutil.rmtree(part, ignore_errors=True)
    env.mkdir(parents=True, exist_ok=True)

    build = env / args.build
    # Fresh: packages found in another prefix stay cached otherwise.
    subprocess.run(["cmake", "--fresh", "-G", "Ninja", "-S", str(SOURCE), "-B", str(build), "-DCMAKE_BUILD_TYPE=Debug",
                    f"-DCMAKE_PREFIX_PATH={prefix}", f"-DCMAKE_INSTALL_PREFIX={env / 'prefix'}",
                    "-DKDE_INSTALL_USE_QT_SYS_PATHS=OFF", "-DBUILD_TESTING=OFF", "-DKONSOLE_PARITY_HOOKS=ON"],
                   check=True, stdout=subprocess.DEVNULL)
    # The prefix's tools (meinproc6 for the docs) need its libraries.
    build_env = {**os.environ, "LD_LIBRARY_PATH": ":".join(filter(None, [str(prefix / "lib64"), os.environ.get("LD_LIBRARY_PATH")]))}
    subprocess.run(["cmake", "--build", str(build), "--parallel", args.jobs], env=build_env, check=True)
    subprocess.run(["cmake", "--build", str(build), "--target", "konsole_compatplugin", "--parallel", args.jobs], env=build_env, check=True)
    subprocess.run(["cmake", "--install", str(build)], env={**os.environ, "DESTDIR": str(stage)}, check=True,
                   stdout=subprocess.DEVNULL)

    paths = qt_paths()
    mirror_qt(paths["QT_INSTALL_PLUGINS"], env / "qt/plugins")
    mirror_qt(paths["QT_INSTALL_QML"], env / "qt/qml")

    # The builder prefix's plugins, minus the ones sonic-terminal installs (we provide fresh ones):
    # lib64/qt6/plugins in the builder's stage, lib64/plugins in an older prefix.
    ours = {line.split(" ", 1)[0].removeprefix("lib64/plugins/")
            for line in (HERE / "baseline-install.txt").read_text().splitlines() if line.startswith("lib64/plugins/")}
    builder_plugins = next(p for p in (prefix / "lib64/qt6/plugins", prefix / "lib64/plugins") if p.is_dir())
    for path in builder_plugins.rglob("*"):
        if path.is_file() and str(path.relative_to(builder_plugins)) not in ours:
            link = env / "builder-plugins" / path.relative_to(builder_plugins)
            link.parent.mkdir(parents=True, exist_ok=True)
            link.symlink_to(path)

    (env / "bin").mkdir()
    shutil.copy2(prefix / "bin/sonic-win", env / "bin/sonic-win")
    # A keditbookmarks that is found but can't start (missing interpreter), so Edit Bookmarks
    # shows its error box whether or not keditbookmarks is installed.
    stub = env / "bin/keditbookmarks"
    stub.write_text("#!/nonexistent/parity-keditbookmarks\n")
    stub.chmod(0o755)
    write_qt_conf(env / "bin", paths, env)
    write_qt_conf(build / "bin", paths, env)
    (env / "xdg").mkdir()
    (env / "xdg/mime").symlink_to("/usr/share/mime")

    packages = [p for p in subprocess.run(["qlist", "-IC"], capture_output=True, text=True).stdout.split()
                if p.startswith("sonicde-")]
    files = subprocess.run(["qlist", *packages], capture_output=True, text=True).stdout.split() if packages else []
    stale = [str(p) for p in (prefix / "lib64").glob("libkonsole*.so*")]
    (env / "forbidden.txt").write_text("\n".join(sorted(set(files + stale))) + "\n")
    print(f"forbidden: {len(files)} files from {len(packages)} installed sonicde packages, {len(stale)} builder-prefix konsole libraries")
    return 0


if __name__ == "__main__":
    sys.exit(main())
