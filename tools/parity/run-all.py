#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
# SPDX-License-Identifier: GPL-2.0-or-later
"""Run every scenario in scenarios/list.txt; a scenario fails when its run does.

Usage: run-all.py --env <setup-env.py dir> --prefix <builder>/state/install-stage/usr --output <dir>
                  --plugin-baseline <dir> [--only <name> ...]
"""

import argparse
import shlex
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
        code = subprocess.run([sys.executable, "-I", str(HERE / "run.py"), "--scenario", str(script), "--env",
                               args.env, "--prefix", args.prefix, "--output", str(args.output / name), "--build", args.build,
                               *extra]).returncode
        if code != 0:
            failures.append(f"{name}: run failed ({code})")
        print(f"== {name} done", flush=True)
    for failure in failures:
        print("FAIL", failure)
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
