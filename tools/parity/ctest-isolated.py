#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
# SPDX-License-Identifier: GPL-2.0-or-later
"""Run sonic-terminal's ctest suite on a private X server, D-Bus, HOME and XDG directories.

Usage: ctest-isolated.py --build <build dir> --prefix <builder>/build [ctest arguments...]
Several autotests open windows or use D-Bus; this keeps them away from the desktop session.
"""

import argparse
import os
import select
import subprocess
import sys
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent


def child(args, ctest_args):
    env = dict(os.environ)
    read_fd, write_fd = os.pipe()
    xvfb = subprocess.Popen(["Xvfb", "-displayfd", str(write_fd), "-screen", "0", "1920x1080x24", "-ac", "-nolisten", "tcp"],
                            env=env, pass_fds=[write_fd], stderr=subprocess.DEVNULL)
    os.close(write_fd)
    try:
        if not select.select([read_fd], [], [], 10)[0]:
            print("FAIL: private X server did not start")
            return 2
        env["DISPLAY"] = ":" + os.read(read_fd, 64).decode().strip()
        # "--exec <command...>" runs a command (e.g. a test under gdb) in the same isolation.
        if ctest_args[:1] == ["--exec"]:
            return subprocess.run(ctest_args[1:], env=env).returncode
        return subprocess.run(["ctest", "--test-dir", args.build, *ctest_args], env=env).returncode
    finally:
        os.close(read_fd)
        xvfb.terminate()
        xvfb.wait(timeout=5)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build", required=True)
    parser.add_argument("--prefix", required=True)
    parser.add_argument("--child", action="store_true", help=argparse.SUPPRESS)
    args, ctest_args = parser.parse_known_args()
    if args.child:
        return child(args, ctest_args)
    prefix = Path(args.prefix).resolve()
    with tempfile.TemporaryDirectory(prefix="konsole-ctest-") as temporary:
        home = Path(temporary)
        runtime = home / "runtime"
        runtime.mkdir(mode=0o700)
        # QT_MESSAGE_PATTERN and QT_LOGGING_RULES only for debugging (e.g. %{backtrace} on an assert).
        env = {key: os.environ[key] for key in ("LANG", "PATH", "QT_MESSAGE_PATTERN", "QT_LOGGING_RULES") if key in os.environ}
        # No LD_LIBRARY_PATH: it would override the tests' RUNPATH into the build tree and load the
        # prefix's installed konsole libraries, which can be older than the build.
        env.update(QT_PLUGIN_PATH=f"{Path(args.build).resolve()}/bin:{prefix}/lib64/plugins",
                   QML_IMPORT_PATH=f"{prefix}/lib64/qml", XDG_DATA_DIRS=f"{prefix}/share:/usr/share",
                   XDG_CONFIG_DIRS="/etc/xdg", HOME=str(home), XDG_CONFIG_HOME=str(home / ".config"),
                   XDG_DATA_HOME=str(home / ".local/share"), XDG_CACHE_HOME=str(home / ".cache"),
                   XDG_STATE_HOME=str(home / ".local/state"), XDG_RUNTIME_DIR=str(runtime), QT_QPA_PLATFORM="xcb",
                   DBUS_SYSTEM_BUS_ADDRESS="unix:path=/nonexistent/konsole-ctest-system")
        command = [sys.executable, "-I", str(Path(__file__).resolve()), "--child", "--build", str(Path(args.build).resolve()),
                   "--prefix", str(prefix), *ctest_args]
        return subprocess.run(["dbus-run-session", f"--config-file={HERE / 'private-bus.conf'}", "--", *command], env=env).returncode


if __name__ == "__main__":
    sys.exit(main())
