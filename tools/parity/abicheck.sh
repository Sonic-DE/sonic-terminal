#!/bin/sh
# SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
# SPDX-License-Identifier: GPL-2.0-or-later
# Checks that the plugin-facing classes keep the ABI of the phase 0 baseline.
# Usage: abicheck.sh <baseline dir> <dir with the new libkonsoleprivate.so* and libkonsoleapp.so*>
# The baseline holds the libraries themselves: libabigail 3.0.0 crashes reading an XML dump
# of libkonsoleprivate, while comparing two binaries works. The full report is searched for
# frozen classes, because abidiff reports a class change through whichever function reaches it.
set -u
frozen='Konsole::(IKonsolePlugin|MainWindow|SessionController|Session|TerminalDisplay|ViewProperties|ProcessInfo|Profile|ViewManager)'
baseline=$1
dir=$2
status=0
for lib in konsoleprivate konsoleapp; do
    old=$(ls "$baseline"/lib$lib.so.* 2>/dev/null | head -n 1)
    new=$(ls "$dir"/lib$lib.so.* 2>/dev/null | head -n 1)
    if [ -z "$old" ] || [ -z "$new" ]; then
        echo "FAIL lib$lib missing (baseline: '$old', new: '$new')"
        status=1
        continue
    fi
    # Added functions don't break old plugins, so only changes and removals are reported.
    report=$(abidiff --no-show-locs --no-added-syms "$old" "$new")
    result=$?
    if [ $result -ge 128 ] || [ $((result & 3)) -ne 0 ]; then
        echo "FAIL lib$lib: abidiff failed (exit $result)"
        status=1
    elif [ $((result & 8)) -ne 0 ]; then
        echo "$report"
        echo "FAIL lib$lib: functions or variables removed"
        status=1
    elif echo "$report" | grep -Eq "(class|struct) $frozen'|'$frozen' changed|type '$frozen"; then
        echo "$report"
        echo "FAIL lib$lib: a plugin-facing class changed"
        status=1
    else
        echo "PASS lib$lib"
    fi
done
exit $status
