#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
# SPDX-License-Identifier: GPL-2.0-or-later
"""Run every scenario in scenarios/list.txt (normal and text-hidden), then optionally compare.

Usage: run-all.py --env <setup-env.py dir> --prefix <builder>/build --output <dir>
                  [--baseline <dir>] [--only <name> ...]
"""

import argparse
import shlex
import shutil
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent


def scenarios():
    for line in (HERE / "scenarios/list.txt").read_text().splitlines():
        if line.strip() and not line.startswith("#"):
            name, script, extra = (part.strip() for part in line.split("|"))
            yield name, HERE / "scenarios" / script, shlex.split(extra)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--env", required=True, help="environment made by setup-env.py")
    parser.add_argument("--prefix", required=True)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--baseline", type=Path)
    parser.add_argument("--only", nargs="*", default=[])
    parser.add_argument("--plugin-baseline", required=True,
                        help="directory holding konsoleplugins/konsole_compatplugin.so from phase 0")
    parser.add_argument("--build", default="build", help="build directory inside --env (see run.py)")
    args = parser.parse_args()
    failures = []
    for name, script, extra in scenarios():
        if args.only and name not in args.only:
            continue
        extra = [part.replace("{plugin_baseline}", args.plugin_baseline) for part in extra]
        run = args.output / name
        hidden = args.output / (name + ".hidden")
        for target, more in ((run, []), (hidden, ["--hide-text"])):
            code = subprocess.run([sys.executable, "-I", str(HERE / "run.py"), "--scenario", str(script), "--env",
                                   args.env, "--prefix", args.prefix, "--output", str(target), "--build", args.build,
                                   *extra, *more]).returncode
            if code != 0:
                failures.append(f"{target.name}: run failed ({code})")
        texthidden = run / "texthidden"
        texthidden.mkdir(exist_ok=True)
        for image in hidden.glob("*.png"):
            shutil.copy(image, texthidden / image.name)
        if args.baseline:
            # The application window is the QML window; the baseline is the widget window's.
            compare = [sys.executable, "-I", str(HERE / "compare.py"), str(args.baseline / name), str(run), "--qml-window"]
            code = subprocess.run(compare).returncode
            if code != 0:
                failures.append(f"{name}: differs from baseline")
        print(f"== {name} done", flush=True)
    for failure in failures:
        print("FAIL", failure)
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
