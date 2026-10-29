#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
# SPDX-License-Identifier: GPL-2.0-or-later
"""Compare a parity run with a baseline.

Rules (see README.md): traces must match exactly. Images must be identical outside the
text mask; inside it, at most 10 % of the mask's pixels may differ (anti-aliasing and
DPI handling). The text mask is where a normal capture differs from the same capture
taken with the text-hidden color scheme, grown by 1 px, in either build.
"""

import argparse
import sys
from pathlib import Path

import numpy
from PIL import Image

TEXT_DIFFERENCE_LIMIT = 0.10


def load(path):
    return numpy.asarray(Image.open(path).convert("RGBA"), dtype=numpy.int16)


def text_mask(normal, hidden):
    if hidden is None or normal.shape != hidden.shape:
        return numpy.zeros(normal.shape[:2], dtype=bool)
    mask = (normal != hidden).any(axis=2)
    grown = mask.copy()
    grown[1:, :] |= mask[:-1, :]
    grown[:-1, :] |= mask[1:, :]
    grown[:, 1:] |= mask[:, :-1]
    grown[:, :-1] |= mask[:, 1:]
    return grown


def compare_image(name, baseline_dir, run_dir, report_dir):
    base, new = load(baseline_dir / name), load(run_dir / name)
    if base.shape != new.shape:
        return f"{name}: size {base.shape[1]}x{base.shape[0]} became {new.shape[1]}x{new.shape[0]}"
    hidden_name = Path("texthidden") / name
    mask = text_mask(base, load(baseline_dir / hidden_name) if (baseline_dir / hidden_name).exists() else None)
    mask |= text_mask(new, load(run_dir / hidden_name) if (run_dir / hidden_name).exists() else None)
    differs = (base != new).any(axis=2)
    outside = int((differs & ~mask).sum())
    inside = int((differs & mask).sum())
    masked = int(mask.sum())
    if outside or inside:
        overlay = numpy.asarray(Image.open(run_dir / name).convert("RGBA")).copy()
        overlay[mask] = (overlay[mask] * 0.6 + numpy.array([0, 0, 255, 255]) * 0.4).astype(numpy.uint8)
        overlay[differs] = (255, 0, 0, 255)
        Image.fromarray(overlay).save(report_dir / f"diff-{name}")
    print(f"{name}: {outside} differing pixels outside text, {inside} of {masked} text pixels differ")
    if outside:
        return f"{name}: {outside} pixels differ outside text"
    if masked and inside / masked > TEXT_DIFFERENCE_LIMIT:
        return f"{name}: {inside / masked:.1%} of text pixels differ (limit {TEXT_DIFFERENCE_LIMIT:.0%})"
    return None


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("baseline", type=Path)
    parser.add_argument("run", type=Path)
    args = parser.parse_args()
    report = args.run / "report"
    report.mkdir(exist_ok=True)
    failures = []
    # Traces and the masked compatibility files must match exactly.
    for text in sorted(p.name for p in args.baseline.glob("*.txt")):
        other = args.run / text
        if not other.exists():
            failures.append(f"{text}: missing")
        elif (args.baseline / text).read_text() != other.read_text():
            failures.append(f"{text} differs")
    for image in sorted(p.name for p in args.baseline.glob("*.png")):
        if not (args.run / image).exists():
            failures.append(f"{image}: missing")
            continue
        failure = compare_image(image, args.baseline, args.run, report)
        if failure:
            failures.append(failure)
    for failure in failures:
        print("FAIL", failure)
    print("PASS" if not failures else f"{len(failures)} failure(s)")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
