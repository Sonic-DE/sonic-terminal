#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
# SPDX-License-Identifier: GPL-2.0-or-later
"""Run a parity scenario: konsole under the real SonicWin compositor on a private X server.

Everything is private (HOME, XDG dirs, D-Bus, X server); nothing reaches the user's
session. See README.md for the scenario format and the comparison rules.
"""

import argparse
import configparser
import os
import select
import shlex
import shutil
import subprocess
import sys
import tempfile
import time
from pathlib import Path

HERE = Path(__file__).resolve().parent
DBUS_CONFIG = HERE / "private-bus.conf"


def stage_name(build):
    """The DESTDIR staging directory that holds a build's data files (setup-env.py)."""
    return "stage" if build == "build" else f"stage-{build}"


def write_ini(path, sections):
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w") as out:
        for name, values in sections.items():
            out.write(f"[{name}]\n")
            for key, value in values.items():
                out.write(f"{key}={value}\n")
            out.write("\n")


def color_scheme(prefix, name, hide_text):
    """The color scheme's groups as kdeglobals expects them; hide_text paints text in its background."""
    parser = configparser.RawConfigParser(strict=False)
    parser.optionxform = str
    parser.read(prefix / "share/color-schemes" / f"{name}.colors")
    sections = {s: dict(parser.items(s)) for s in parser.sections()}
    if hide_text:
        for group, values in sections.items():
            if group.startswith("Colors:") and "BackgroundNormal" in values:
                for key in list(values):
                    if key.startswith(("Foreground", "Decoration")):
                        values[key] = values["BackgroundNormal"]
    return sections


def write_scheme(data, overrides):
    """A copy of the Breeze terminal scheme with [General] overrides such as Opacity or Blur."""
    parser = configparser.RawConfigParser(strict=False)
    parser.optionxform = str
    parser.read(HERE.parent.parent / "data/color-schemes/Breeze.colorscheme")
    sections = {s: dict(parser.items(s)) for s in parser.sections()}
    sections["General"]["Description"] = "Parity"
    for override in overrides:
        key, value = override.split("=", 1)
        sections["General"][key] = value
    write_ini(data / "konsole/Parity.colorscheme", sections)


def prepare_home(home, prefix, hide_text, scheme_overrides):
    config = home / ".config"
    data = home / ".local/share"
    globals_ = color_scheme(prefix, "SilverLight", hide_text)
    globals_.setdefault("General", {}).update(
        ColorScheme="SilverLight", font="Noto Sans,10,-1,5,400,0,0,0,0,0,0,0,0,0,0,1",
        fixed="Hack,10,-1,5,400,0,0,0,0,0,0,0,0,0,0,1", menuFont="Noto Sans,10,-1,5,400,0,0,0,0,0,0,0,0,0,0,1",
        smallestReadableFont="Noto Sans,8,-1,5,400,0,0,0,0,0,0,0,0,0,0,1",
        toolBarFont="Noto Sans,10,-1,5,400,0,0,0,0,0,0,0,0,0,0,1")
    globals_.setdefault("KDE", {}).update(widgetStyle="Silver", AnimationDurationFactor="0")
    globals_.setdefault("Icons", {}).update(Theme="silver")
    write_ini(config / "kdeglobals", globals_)
    write_ini(config / "kwinrc", {
        "Compositing": {"Enabled": "true"},
        "org.kde.kdecoration2": {"library": "org.kde.silver"},
        "Plugins": {"fadeEnabled": "false", "scaleEnabled": "false", "blurEnabled": "true",
                    "slidingpopupsEnabled": "false", "fadingpopupsEnabled": "false"},
        "Effect-blur": {"NoiseStrength": "0"},
    })
    write_ini(config / "silver/silverrc", {"Windeco": {"AnimationsEnabled": "false"}})
    write_ini(config / "konsolerc", {
        "Desktop Entry": {"DefaultProfile": "Parity.profile"},
        "KonsoleWindow": {"RememberWindowSize": "false"},
        "General": {"ConfigVersion": "1"},
    })
    write_ini(data / "konsole/Parity.profile", {
        "General": {"Name": "Parity", "Command": "/bin/bash --norc --noprofile",
                    "Environment": "TERM=xterm-256color,COLORTERM=truecolor,PS1=$ ",
                    "Directory": str(home), "ShowTerminalSizeHint": "false"},
        "Appearance": {"Font": "Hack,10,-1,5,400,0,0,0,0,0,0,0,0,0,0,1", "ColorScheme": "Parity"},
        "Terminal Features": {"BlinkingCursorEnabled": "false", "BlinkingTextEnabled": "false"},
    })
    write_scheme(data, scheme_overrides)
    (home / "tabs.txt").write_text("title: One;; command: /bin/bash --norc --noprofile\n"
                                   "title: Two;; workdir: /tmp;; profile: Parity\n")
    shutil.copy(HERE.parent.parent / "data/layouts/2x2-terminals.json", home / "layout.json")


class LoadedFiles:
    """Files mapped by our processes; any installed SonicDE package file among them fails the run."""

    def __init__(self, forbidden):
        self.forbidden = set(Path(forbidden).read_text().split())
        self.seen = set()

    def sample(self, pid):
        try:
            for line in Path(f"/proc/{pid}/maps").read_text().splitlines():
                parts = line.split(maxsplit=5)
                if len(parts) == 6 and parts[5].startswith("/"):
                    self.seen.add(os.path.realpath(parts[5]))
        except OSError:
            pass

    def violations(self):
        return sorted(path for path in self.seen if path in self.forbidden)


def run_child(args):
    """Inside the private bus: start Xvfb, the compositor and konsole, then wait for the script."""
    env = dict(os.environ)
    output = Path(args.output)
    read_fd, write_fd = os.pipe()
    xvfb = subprocess.Popen(["Xvfb", "-displayfd", str(write_fd), "-screen", "0", "1920x1080x24", "-dpi", "96",
                             "-ac", "-nolisten", "tcp"], env=env, pass_fds=[write_fd],
                            stderr=subprocess.DEVNULL)
    os.close(write_fd)
    processes = []
    try:
        if not select.select([read_fd], [], [], 10)[0]:
            print("FAIL: private X server did not start", flush=True)
            return 2
        env["DISPLAY"] = ":" + os.read(read_fd, 64).decode().strip()
        logs = output / "logs"
        logs.mkdir(parents=True, exist_ok=True)
        processes.append(subprocess.Popen([str(Path(args.env) / "bin/sonic-win"), "--no-kactivities"], env=env,
                                          stdout=(logs / "sonic-win.log").open("w"), stderr=subprocess.STDOUT))
        for _ in range(200):
            reply = subprocess.run(["dbus-send", "--session", "--print-reply", "--dest=org.kde.KWin", "/Compositor",
                                    "org.freedesktop.DBus.Properties.Get", "string:org.kde.kwin.Compositing",
                                    "string:active"], env=env, capture_output=True, text=True)
            if "boolean true" in reply.stdout:
                break
            time.sleep(0.05)
        else:
            print("FAIL: compositor did not become active", flush=True)
            return 2
        script = output / "script.jsonl"
        script.write_text('{"op": "output", "dir": "%s"}\n' % output + Path(args.scenario).read_text())
        command = [str(Path(args.env) / args.build / "bin/konsole"), "--separate", "--nofork"]
        # Before the scenario's own arguments: -e takes everything after it as the command.
        if not args.no_script:
            command += ["--parity-script", str(script)]
        command += shlex.split(args.konsole_args)
        konsole = subprocess.Popen(command, env=env, cwd=env["HOME"], stdout=(output / "stdout.txt").open("w"),
                                   stderr=(logs / "konsole.log").open("w"))
        processes.append(konsole)
        loaded = LoadedFiles(Path(args.env) / "forbidden.txt")
        deadline = time.monotonic() + args.timeout
        while konsole.poll() is None and time.monotonic() < deadline:
            loaded.sample(konsole.pid)
            loaded.sample(processes[0].pid)
            time.sleep(0.25)
        if konsole.poll() is None:
            print("FAIL: scenario timed out", flush=True)
            return 3
        code = konsole.returncode
        print(f"konsole exited with {code}", flush=True)
        violations = loaded.violations()
        (output / "logs/loaded-files.txt").write_text("\n".join(sorted(loaded.seen)) + "\n")
        if violations:
            print("FAIL: loaded files of installed SonicDE packages:\n  " + "\n  ".join(violations), flush=True)
            return 4
        return code
    finally:
        for process in reversed(processes):
            if process.poll() is None:
                process.kill()
                process.wait()
        os.close(read_fd)
        xvfb.terminate()
        xvfb.wait(timeout=5)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--scenario", required=True, help="scenario .jsonl file")
    parser.add_argument("--output", required=True, help="directory for captures and traces")
    parser.add_argument("--env", required=True, help="environment made by setup-env.py")
    parser.add_argument("--prefix", required=True, help="builder install prefix, read only (<builder>/state/install-stage/usr)")
    parser.add_argument("--hide-text", action="store_true", help="text-hidden color scheme, for the text mask")
    parser.add_argument("--scheme-set", action="append", default=[], metavar="KEY=VALUE",
                        help="terminal color scheme [General] override, e.g. Opacity=0.8 or Blur=true")
    parser.add_argument("--konsole-args", default="", help="extra konsole arguments, space separated")
    parser.add_argument("--plugin-path", default="", help="extra plugin directory (holding konsoleplugins/)")
    parser.add_argument("--no-script", action="store_true", help="run konsole without a script (options that just print)")
    parser.add_argument("--timeout", type=int, default=120)
    parser.add_argument("--build", default="build",
                        help="build directory inside --env (build-phase0 for the phase 0 copy); HOME stays the same")
    parser.add_argument("--child", action="store_true", help=argparse.SUPPRESS)
    args = parser.parse_args()
    if args.child:
        return run_child(args)

    prefix = Path(args.prefix).resolve()
    environment = Path(args.env).resolve()
    stage_share = environment / stage_name(args.build) / str(environment / "prefix").lstrip("/") / "share"
    output = Path(args.output).resolve()
    if output.exists():
        shutil.rmtree(output)
    output.mkdir(parents=True)
    # A fixed HOME path: it shows up in dialogs (initial directory, print-to-file path).
    home = environment / "home"
    # A scenario may make directories read-only (chmod op) and fail before restoring them.
    for directory, _, _ in os.walk(home):
        os.chmod(directory, 0o755)
    shutil.rmtree(home, ignore_errors=True)
    home.mkdir()
    # Short: KIO's worker sockets live here and must fit the Unix socket path limit.
    runtime = Path(tempfile.mkdtemp(prefix="konsole-parity-", dir="/tmp"))
    runtime.chmod(0o700)
    prepare_home(home, prefix, args.hide_text, args.scheme_set)
    # The parity build's libraries before the prefix's: the stage's binaries (sonic-win) and QML
    # plugins are configured for /usr and would otherwise load the system's.
    env = {key: os.environ[key] for key in ("LANG",) if key in os.environ}
    env.update(
        LD_LIBRARY_PATH=f"{environment / args.build / 'bin'}:{prefix}/lib64",
        PATH=f"{environment}/bin:/usr/bin:/bin",
        QT_PLUGIN_PATH=":".join(filter(None, [args.plugin_path and str(Path(args.plugin_path).resolve()),
                                              str(environment / args.build / "bin"), str(environment / "builder-plugins")])),
        # The builder's stage keeps QML modules in lib64/qt6/qml, an older prefix in lib64/qml.
        QML_IMPORT_PATH=f"{prefix}/lib64/qt6/qml:{prefix}/lib64/qml",
        XDG_DATA_DIRS=f"{stage_share}:{prefix}/share:{environment}/xdg", XDG_CONFIG_DIRS="/etc/xdg",
        HOME=str(home), XDG_CONFIG_HOME=str(home / ".config"), XDG_DATA_HOME=str(home / ".local/share"),
        XDG_CACHE_HOME=str(home / ".cache"), XDG_STATE_HOME=str(home / ".local/state"),
        XDG_RUNTIME_DIR=str(runtime), XDG_SESSION_TYPE="x11", XDG_CURRENT_DESKTOP="SonicDE:KDE",
        KDE_FULL_SESSION="true", KDE_SESSION_VERSION="6", QT_QPA_PLATFORM="xcb",
        QT_QPA_PLATFORMTHEME="kde", QT_SCALE_FACTOR="1", QT_X11_NO_MITSHM="1",
        LIBGL_ALWAYS_SOFTWARE="1", GALLIUM_DRIVER="llvmpipe", LANGUAGE="en", LC_ALL="C.UTF-8",
        DBUS_SYSTEM_BUS_ADDRESS="unix:path=/nonexistent/konsole-parity-system",
        CUPS_SERVER="/nonexistent/konsole-parity-cups.sock",
        PULSE_SERVER="unix:/nonexistent/konsole-parity")
    child = [sys.executable, "-I", str(Path(__file__).resolve()), "--child", "--scenario",
             str(Path(args.scenario).resolve()), "--output", str(output), "--env", str(environment),
             "--prefix", str(prefix), "--timeout", str(args.timeout), "--build", args.build,
             f"--konsole-args={args.konsole_args}",
             *(["--no-script"] if args.no_script else [])]
    result = subprocess.run(["dbus-run-session", f"--config-file={DBUS_CONFIG}", "--", *child], env=env,
                            timeout=args.timeout + 60)
    shutil.rmtree(runtime, ignore_errors=True)
    state = home / ".local/state"
    if state.exists():
        shutil.copytree(state, output / "logs/state", dirs_exist_ok=True)
    return result.returncode


if __name__ == "__main__":
    raise SystemExit(main())
