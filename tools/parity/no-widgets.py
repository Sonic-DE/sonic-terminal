#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
# SPDX-License-Identifier: GPL-2.0-or-later
"""NoWidgetsTest: the QML build keeps no widgets but its compat windows, plugin docks, menus and
the framework dialogs opened inside a FrameworkDialogScope.

Runs the dialog scenarios, the bookmark menu's (no-widgets-bookmarks.jsonl) and Configure
Shortcuts' (shortcuts-sync.jsonl, which also checks the other window's shortcuts) with the
hook's widget check after every step, then a scenario
that creates a stray dialog and label, which must fail naming both.
Usage: no-widgets.py --env <setup-env.py dir> --prefix <builder>/state/install-stage/usr --output <dir>
"""

import argparse
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
SCENARIOS = ["dialogs", "dialogs-extra", "no-widgets-bookmarks", "shortcuts-sync"]


def run(args, name):
    output = Path(args.output) / name
    command = [sys.executable, "-I", str(HERE / "run.py"), "--scenario", str(HERE / "scenarios" / f"{name}.jsonl"),
               "--output", str(output), "--env", args.env, "--prefix", args.prefix, "--build", args.build, "--check-widgets"]
    result = subprocess.run(command)
    trace = output / "trace.txt"
    failures = [line for line in trace.read_text().splitlines() if line.startswith("FAIL ")] if trace.exists() else []
    return result.returncode, failures


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--env", required=True)
    parser.add_argument("--prefix", required=True)
    parser.add_argument("--output", required=True)
    parser.add_argument("--build", default="build")
    args = parser.parse_args()
    failed = False
    for name in SCENARIOS:
        code, failures = run(args, name)
        if code != 0:
            failed = True
            print(f"FAIL {name}: exit {code}", *failures, sep="\n", flush=True)
        else:
            print(f"PASS {name}", flush=True)
    code, failures = run(args, "no-widgets-stray")
    caught = code != 0 and any("strayDialog" in line and "strayLabel" in line for line in failures)
    print(f"{'PASS' if caught else 'FAIL'} no-widgets-stray: the stray dialog and label {'fail' if caught else 'pass'} the check",
          flush=True)
    return 1 if failed or not caught else 0


if __name__ == "__main__":
    sys.exit(main())
