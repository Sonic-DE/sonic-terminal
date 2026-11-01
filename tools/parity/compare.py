#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
# SPDX-License-Identifier: GPL-2.0-or-later
"""Compare a parity run with a baseline.

Rules (see README.md): traces must match exactly. Images must be identical outside the
text mask and the overlay mask; inside each, at most 10 % of its pixels may differ
(anti-aliasing and DPI handling); inside the overlay mask a channel off by one level (the
scene graph's extra blend) is no difference. The text mask is where a normal capture differs from the
same capture taken with the text-hidden color scheme, grown by 1 px, in either build. The
overlay mask is the union of both builds' overlay rectangles (<capture>.png.overlays).
Files present on one side only, pages listed in a trace without their capture, and
captures of a single colour fail.

With --qml-window (a run in the QML window against the widget window's baseline): the QML
chrome differs from the widget window's (Kirigami metrics; checked by QmlWindowTest), so a
capture whose <capture>.png.panes lists the same number of terminals on both sides is
compared pane by pane, each at its own position, over the size both share, without the
menus and dialogs covering them; xprop's _NET_WM_ICON_NAME isn't compared (QWindow sets
none, a widget window an empty one).

A capture of a QML dialog (the hook writes <capture>.png.qml next to it) isn't compared
pixel by pixel (the dialogs keep their content, not their layout): its most common colour
must be the baseline's, and every colour covering at least 2 % of it must cover at least
0.5 % of the baseline capture, within 2 levels per channel (the theme's colours, no
others), or be one of INTENDED_QML_COLOURS. Its screen capture is compared by pane, the baseline's dialog with its shadow
left out as the run's is.
"""

import argparse
import re
import sys
from pathlib import Path

import numpy
from PIL import Image

TEXT_DIFFERENCE_LIMIT = 0.10
OVERLAY_DIFFERENCE_LIMIT = 0.10
OVERLAY_BLEND_ROUNDING = 1
# Silver's largest shadow (64 blur, 16 offset), as the hook records it around a dialog.
SHADOW = 80
# Colours a QML dialog may show that its widget dialog didn't, with their source (SilverLight,
# the parity runs' scheme).
INTENDED_QML_COLOURS = {
    # Joseph, 2026-11-01: messages follow the colour scheme; KMessageWidget hard-codes Breeze's
    # blue (kmessagewidget.cpp), Kirigami's InlineMessage uses activeTextColor at 20 %.
    (203, 223, 238): "information message background",
}
PATTERNS = ("*.txt", "*.png", "*.png.overlays", "*.png.panes")
QML_DIALOG_COLOUR_SHARE = 0.02
BASELINE_COLOUR_SHARE = 0.005
COLOUR_TOLERANCE = 2


def safe_name(name):
    """The parity hook's safeName(): every character outside [A-Za-z0-9._-] becomes '_'."""
    return re.sub(r"[^A-Za-z0-9._-]", "_", name)


def load(path):
    return numpy.asarray(Image.open(path).convert("RGBA"), dtype=numpy.int16)


def rects_mask(rects, shape):
    mask = numpy.zeros(shape[:2], dtype=bool)
    for x, y, width, height in rects:
        mask[max(y, 0):max(y + height, 0), max(x, 0):max(x + width, 0)] = True
    return mask

def overlay_mask(path, shape):
    rects = []
    if path.exists():
        rects = [tuple(int(value) for value in line.split()) for line in path.read_text().split("\n") if line.strip()]
    return rects_mask(rects, shape)


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


def read_panes(path):
    """(panes, covered): the terminals' rectangles and the menus and dialogs over them."""
    panes, covered = [], []
    if path.exists():
        for line in path.read_text().split("\n"):
            if line.strip():
                kind, *values = line.split()
                (panes if kind == "pane" else covered).append(tuple(int(value) for value in values))
    return panes, covered

def compare_panes(name, baseline_dir, run_dir, base, new, base_panes, new_panes):
    """Each pane at its own position on each side, over the size both share."""
    hidden_name = Path("texthidden") / name
    sides = []
    for directory, image, (panes, covered) in ((baseline_dir, base, base_panes), (run_dir, new, new_panes)):
        hidden = load(directory / hidden_name) if (directory / hidden_name).exists() else None
        sides.append((image, text_mask(image, hidden), overlay_mask(directory / f"{name}.overlays", image.shape),
                      rects_mask(covered, image.shape), panes))
    outside = inside = masked = in_overlays = overlaid = 0
    for (bx, by, bw, bh), (nx, ny, nw, nh) in zip(sides[0][4], sides[1][4]):
        width, height = min(bw, nw), min(bh, nh)
        crops = []
        for (image, text, overlays, covered, _), (x, y) in zip(sides, ((bx, by), (nx, ny))):
            # Clipped to the image (a pane can reach past a screen edge).
            x0, y0 = max(x, 0), max(y, 0)
            x1, y1 = min(x + width, image.shape[1]), min(y + height, image.shape[0])
            crops.append((x0 - x, y0 - y, x1 - x, y1 - y, image, text, overlays, covered, x, y))
        left = max(crop[0] for crop in crops)
        top = max(crop[1] for crop in crops)
        right = min(crop[2] for crop in crops)
        bottom = min(crop[3] for crop in crops)
        if right <= left or bottom <= top:
            continue
        cut = [(image[y + top:y + bottom, x + left:x + right], text[y + top:y + bottom, x + left:x + right],
                overlays[y + top:y + bottom, x + left:x + right], covered[y + top:y + bottom, x + left:x + right])
               for _, _, _, _, image, text, overlays, covered, x, y in crops]
        (b, bt, bo, bc), (n, nt, no, nc) = cut
        mask = bt | nt
        ignored = bc | nc
        overlays_here = (bo | no) & ~mask
        differs = (b != n).any(axis=2) & ~ignored
        outside += int((differs & ~mask & ~overlays_here).sum())
        inside += int((differs & mask).sum())
        masked += int((mask & ~ignored).sum())
        # Scroll bars span their pane: with a shorter pane their lower end moves up, so an
        # overlay pixel differs only if it also differs with both panes' bottoms aligned.
        overlay_differs = (numpy.abs(b - n) > OVERLAY_BLEND_ROUNDING).any(axis=2)
        shift_base, shift_new = bh - height, nh - height
        if shift_base or shift_new:
            rows = bottom - top
            b_low = base[by + top + shift_base:by + top + shift_base + rows, bx + left:bx + right]
            n_low = new[ny + top + shift_new:ny + top + shift_new + rows, nx + left:nx + right]
            if b_low.shape == n_low.shape == b.shape:
                overlay_differs &= (numpy.abs(b_low - n_low) > OVERLAY_BLEND_ROUNDING).any(axis=2)
        in_overlays += int((overlay_differs & overlays_here & ~ignored).sum())
        overlaid += int((overlays_here & ~ignored).sum())
    print(f"{name}: by pane, {outside} differing pixels outside text and overlays, {inside} of {masked} text pixels and "
          f"{in_overlays} of {overlaid} overlay pixels differ")
    if outside:
        return f"{name}: {outside} pixels differ outside text and overlays (by pane)"
    if masked and inside / masked > TEXT_DIFFERENCE_LIMIT:
        return f"{name}: {inside / masked:.1%} of text pixels differ (limit {TEXT_DIFFERENCE_LIMIT:.0%}, by pane)"
    if overlaid and in_overlays / overlaid > OVERLAY_DIFFERENCE_LIMIT:
        return f"{name}: {in_overlays / overlaid:.1%} of overlay pixels differ (limit {OVERLAY_DIFFERENCE_LIMIT:.0%}, by pane)"
    return None

def colour_shares(image):
    """The image's RGB colours, most common first, with the share of pixels each covers."""
    colours, counts = numpy.unique(image[:, :, :3].reshape(-1, 3), axis=0, return_counts=True)
    order = numpy.argsort(-counts)
    return colours[order], counts[order] / counts.sum()

def compare_colours(name, base, new):
    for image, side in ((base, "baseline"), (new, "run")):
        if (image == image[0, 0]).all():
            return f"{name}: the {side} capture is a single colour"
    base_colours, base_shares = colour_shares(base)
    new_colours, new_shares = colour_shares(new)
    near = lambda a, b: bool((numpy.abs(a - b) <= COLOUR_TOLERANCE).all())
    print(f"{name}: QML dialog, compared by colour")
    if not near(base_colours[0], new_colours[0]):
        return f"{name}: main colour {tuple(new_colours[0])} instead of the baseline's {tuple(base_colours[0])} (by colour)"
    theme = list(base_colours[base_shares >= BASELINE_COLOUR_SHARE]) + [numpy.array(colour) for colour in INTENDED_QML_COLOURS]
    for colour, share in zip(new_colours, new_shares):
        if share < QML_DIALOG_COLOUR_SHARE:
            break
        if not any(near(colour, known) for known in theme):
            return f"{name}: colour {tuple(colour)} covers {share:.1%}, not a colour of the baseline (by colour)"
    return None

def compare_image(name, baseline_dir, run_dir, report_dir, qml_window=False):
    base, new = load(baseline_dir / name), load(run_dir / name)
    if (run_dir / f"{name}.qml").exists():
        return compare_colours(name, base, new)
    if qml_window:
        base_panes = read_panes(baseline_dir / f"{name}.panes")
        new_panes = read_panes(run_dir / f"{name}.panes")
        if name.endswith(".screen.png") and (run_dir / name.replace(".screen.png", ".png.qml")).exists():
            # The baseline's covered rectangles are frames only (recorded before the hook added
            # shadows); a QML dialog of another size leaves the widget dialog's shadow beside it.
            base_panes = (base_panes[0], [(x - SHADOW, y - SHADOW, w + 2 * SHADOW, h + 2 * SHADOW) for x, y, w, h in base_panes[1]])
        if base_panes[0] and len(base_panes[0]) == len(new_panes[0]):
            return compare_panes(name, baseline_dir, run_dir, base, new, base_panes, new_panes)
    if base.shape != new.shape:
        return f"{name}: size {base.shape[1]}x{base.shape[0]} became {new.shape[1]}x{new.shape[0]}"
    # A window, menu or dialog capture of one colour means nothing was painted; the whole
    # screen may legitimately be empty (background mode starts hidden).
    if ".screen." not in name:
        for image, side in ((base, "baseline"), (new, "run")):
            if (image == image[0, 0]).all():
                return f"{name}: the {side} capture is a single colour"
    hidden_name = Path("texthidden") / name
    mask = text_mask(base, load(baseline_dir / hidden_name) if (baseline_dir / hidden_name).exists() else None)
    mask |= text_mask(new, load(run_dir / hidden_name) if (run_dir / hidden_name).exists() else None)
    overlays = overlay_mask(baseline_dir / f"{name}.overlays", base.shape)
    overlays |= overlay_mask(run_dir / f"{name}.overlays", new.shape)
    overlays &= ~mask
    differs = (base != new).any(axis=2)
    outside = int((differs & ~mask & ~overlays).sum())
    inside = int((differs & mask).sum())
    masked = int(mask.sum())
    beyond_rounding = (numpy.abs(base - new) > OVERLAY_BLEND_ROUNDING).any(axis=2)
    in_overlays = int((beyond_rounding & overlays).sum())
    overlaid = int(overlays.sum())
    if outside or inside or in_overlays:
        picture = numpy.asarray(Image.open(run_dir / name).convert("RGBA")).copy()
        picture[mask] = (picture[mask] * 0.6 + numpy.array([0, 0, 255, 255]) * 0.4).astype(numpy.uint8)
        picture[overlays] = (picture[overlays] * 0.6 + numpy.array([0, 255, 0, 255]) * 0.4).astype(numpy.uint8)
        picture[differs] = (255, 0, 0, 255)
        Image.fromarray(picture).save(report_dir / f"diff-{name}")
    print(f"{name}: {outside} differing pixels outside text and overlays, {inside} of {masked} text pixels and "
          f"{in_overlays} of {overlaid} overlay pixels differ")
    if outside:
        return f"{name}: {outside} pixels differ outside text and overlays"
    if masked and inside / masked > TEXT_DIFFERENCE_LIMIT:
        return f"{name}: {inside / masked:.1%} of text pixels differ (limit {TEXT_DIFFERENCE_LIMIT:.0%})"
    if overlaid and in_overlays / overlaid > OVERLAY_DIFFERENCE_LIMIT:
        return f"{name}: {in_overlays / overlaid:.1%} of overlay pixels differ (limit {OVERLAY_DIFFERENCE_LIMIT:.0%})"
    return None


def trace_failures(directory, side):
    """Every capture a trace names must exist: saved files, and one capture per dialog page."""
    trace = directory / "trace.txt"
    if not trace.exists():
        return []
    failures = []
    dialog = None
    for line in trace.read_text().split("\n"):
        if line.startswith("dialog "):
            dialog = line.split(" ", 2)[1]
        elif line.startswith("  page ") and dialog:
            page = safe_name(f"dialog-{dialog}-{line[len('  page '):].replace('&', '')}") + ".png"
            if not (directory / page).exists():
                failures.append(f"{page}: page in the {side} trace without a capture")
        elif line.startswith("  saved ") and not (directory / line[len("  saved "):]).exists():
            failures.append(f"{line[len('  saved '):]}: saved in the {side} trace but missing")
    return failures


def xprop_properties(text):
    # The X server lists properties in the order they were last changed; only the set matters.
    properties = []
    for line in text.splitlines():
        if line[:1].isspace() and properties:
            properties[-1] += "\n" + line
        else:
            properties.append(line)
    return sorted(properties)


def same_text(name, baseline, run, qml_window=False):
    if name.startswith("xprop-"):
        expected, actual = xprop_properties(baseline), xprop_properties(run)
        if qml_window:
            expected = [line for line in expected if not line.startswith("_NET_WM_ICON_NAME(")]
            actual = [line for line in actual if not line.startswith("_NET_WM_ICON_NAME(")]
        return expected == actual
    return baseline == run


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("baseline", type=Path)
    parser.add_argument("run", type=Path)
    parser.add_argument("--qml-window", action="store_true", help="the run is in the QML window (see above)")
    args = parser.parse_args()
    report = args.run / "report"
    report.mkdir(exist_ok=True)
    failures = []
    for pattern in PATTERNS:
        baseline_files = {p.name for p in args.baseline.glob(pattern)}
        run_files = {p.name for p in args.run.glob(pattern)}
        failures += [f"{name}: missing from the run" for name in sorted(baseline_files - run_files)]
        failures += [f"{name}: not in the baseline" for name in sorted(run_files - baseline_files)]
    failures += trace_failures(args.baseline, "baseline") + trace_failures(args.run, "run")
    # Traces and the masked compatibility files must match exactly.
    for text in sorted(p.name for p in args.baseline.glob("*.txt")):
        other = args.run / text
        if other.exists() and not same_text(text, (args.baseline / text).read_text(), other.read_text(), args.qml_window):
            failures.append(f"{text} differs")
    for image in sorted(p.name for p in args.baseline.glob("*.png")):
        if not (args.run / image).exists():
            continue
        failure = compare_image(image, args.baseline, args.run, report, args.qml_window)
        if failure:
            failures.append(failure)
    for failure in failures:
        print("FAIL", failure)
    print("PASS" if not failures else f"{len(failures)} failure(s)")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
