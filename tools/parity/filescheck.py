#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
# SPDX-License-Identifier: GPL-2.0-or-later
"""Check sonic-terminal's installed files against the phase 0 list.

Other programs read the integration files (.desktop with their actions, notifyrc event ids,
KNewStuff, kglobalaccel, service menus, AppStream, shell completion), so their content must
stay the same; every other file only has to keep existing. Stage an install first:
    DESTDIR=<dir> cmake --install <build dir>
    filescheck.py --staged <dir>/<prefix> [--write]   # --write records a new baseline
"""

import argparse
import hashlib
import sys
from pathlib import Path

BASELINE = Path(__file__).resolve().parent / "baseline-install.txt"
INTEGRATION = (".desktop", ".notifyrc", ".knsrc", ".xml", ".rc", ".json", "_konsole", "konsoleprofile")


def listing(prefix):
    entries = {}
    for path in sorted(p for p in prefix.rglob("*") if p.is_file() or p.is_symlink()):
        relative = path.relative_to(prefix).as_posix()
        digest = hashlib.sha256(path.read_bytes()).hexdigest() if path.name.endswith(INTEGRATION) else "-"
        entries[relative] = digest
    return entries


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--staged", required=True, type=Path, help="staged install prefix")
    parser.add_argument("--write", action="store_true", help="write the baseline instead of checking")
    args = parser.parse_args()
    current = listing(args.staged)
    if args.write:
        BASELINE.write_text("".join(f"{path} {digest}\n" for path, digest in current.items()))
        print(f"wrote {len(current)} entries to {BASELINE}")
        return 0
    baseline = dict(line.rsplit(" ", 1) for line in BASELINE.read_text().splitlines() if line)
    failures = [f"missing: {path}" for path in baseline if path not in current]
    failures += [f"content changed: {path}" for path, digest in baseline.items()
                 if digest != "-" and current.get(path, digest) != digest]
    for path in current:
        if path not in baseline:
            print(f"note: new file {path}")
    for failure in failures:
        print("FAIL", failure)
    print("PASS" if not failures else f"{len(failures)} failure(s)")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
