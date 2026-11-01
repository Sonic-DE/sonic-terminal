#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>
# SPDX-License-Identifier: GPL-2.0-or-later
"""Self-test of compare.py: each broken run must fail, an identical one must pass."""

import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

import numpy
from PIL import Image

HERE = Path(__file__).resolve().parent


def image(path, colour=(40, 40, 40, 255), patch=None):
    pixels = numpy.zeros((40, 60, 4), dtype=numpy.uint8)
    pixels[:, :] = colour
    pixels[5:15, 5:25] = (200, 200, 200, 255)
    if patch:
        (y0, y1, x0, x1), value = patch
        pixels[y0:y1, x0:x1] = value
    Image.fromarray(pixels).save(path)


def shifted(path, rows=2, patch=None):
    """window.client.png as a QML window with taller chrome draws it: the pane 2 rows lower."""
    image(path, patch=patch)
    pixels = numpy.asarray(Image.open(path)).copy()
    pixels[rows:, :40] = pixels[:-rows, :40].copy()
    pixels[:rows, :40] = (90, 90, 90, 255)
    Image.fromarray(pixels).save(path)
    (path.parent / "window.client.png.panes").write_text(f"pane 0 {rows} 40 {40 - rows}\n")


def scenario(directory):
    directory.mkdir(parents=True)
    image(directory / "window.client.png")
    image(directory / "dialog-configure-Tab_Bar___Splitters.png")
    (directory / "window.client.png.overlays").write_text("40 0 20 40\n")
    (directory / "window.client.png.panes").write_text("pane 0 0 40 40\n")
    (directory / "xprop-main.txt").write_text("_NET_WM_ICON_NAME(UTF8_STRING) = \nWM_NAME(STRING) = \"x\"\n")
    (directory / "trace.txt").write_text("capture window\n  saved window.client.png\n"
                                         "dialog configure Configure\n  page Tab Bar / Splitters\n"
                                         "  saved dialog-configure-Tab_Bar___Splitters.png\n")


def compare(baseline, run, qml_window=False):
    return subprocess.run([sys.executable, "-I", str(HERE / "compare.py"), str(baseline), str(run)] + (["--qml-window"] if qml_window else []),
                          capture_output=True, text=True).returncode


def scroll_bar_in_shorter_pane(run):
    """A scroll bar spanning its pane (the overlay x 40-60), the QML pane 2 rows shorter."""
    baseline = run.parent / "baseline"
    for directory, height in ((baseline, 40), (run, 38)):
        pixels = numpy.zeros((40, 60, 4), dtype=numpy.uint8)
        pixels[:, :] = (40, 40, 40, 255)
        pixels[5:15, 5:25] = (200, 200, 200, 255)
        pixels[1:height - 1, 48:52] = (120, 120, 120, 255)
        Image.fromarray(pixels).save(directory / "window.client.png")
        (directory / "window.client.png.panes").write_text(f"pane 0 0 60 {height}\n")


def qml_dialog(run, background=(40, 40, 40, 255), control=(200, 200, 200, 255)):
    """The Configure page as a QML dialog: other size and layout, given colours."""
    pixels = numpy.zeros((50, 70, 4), dtype=numpy.uint8)
    pixels[:, :] = background
    pixels[20:32, 30:60] = control
    Image.fromarray(pixels).save(run / "dialog-configure-Tab_Bar___Splitters.png")
    (run / "dialog-configure-Tab_Bar___Splitters.png.qml").write_text("")

def dialog_shadow(run, qml=True):
    """A dialog's screen capture: the baseline's frame-only cover and its shadow beside it."""
    baseline = run.parent / "baseline"
    for directory, shadow in ((baseline, True), (run, False)):
        pixels = numpy.zeros((40, 60, 4), dtype=numpy.uint8)
        pixels[:, :] = (40, 40, 40, 255)
        pixels[5:15, 5:25] = (200, 200, 200, 255)
        if shadow:
            pixels[20:30, 26:30] = (39, 39, 39, 255)
        Image.fromarray(pixels).save(directory / "dialog-configure-Tab_Bar___Splitters.screen.png")
        (directory / "dialog-configure-Tab_Bar___Splitters.screen.png.panes").write_text("pane 0 0 60 40\ncovered 30 20 20 10\n")
        (directory / "dialog-configure-Tab_Bar___Splitters.screen.png.overlays").write_text("\n")
    if qml:
        (run / "dialog-configure-Tab_Bar___Splitters.png.qml").write_text("")
        image(run / "dialog-configure-Tab_Bar___Splitters.png", patch=((20, 30, 30, 50), (40, 40, 40, 255)))


def without_icon_name(run):
    (run / "xprop-main.txt").write_text("WM_NAME(STRING) = \"x\"\n")


def main():
    cases = {
        "identical": (lambda run: None, 0),
        "image missing from the run": (lambda run: (run / "window.client.png").unlink(), 1),
        "extra image in the run": (lambda run: image(run / "extra.png"), 1),
        "extra text file in the run": (lambda run: (run / "extra.txt").write_text("x\n"), 1),
        "overlay file missing": (lambda run: (run / "window.client.png.overlays").unlink(), 1),
        "trace differs": (lambda run: (run / "trace.txt").write_text("capture other\n"), 1),
        # Applied to both sides (see "both"): only the page check can catch it.
        "page without capture on both sides": (lambda run: (
            (run / "dialog-configure-Tab_Bar___Splitters.png").unlink(),
            (run / "trace.txt").write_text((run / "trace.txt").read_text().replace(
                "  saved dialog-configure-Tab_Bar___Splitters.png\n", ""))), 1),
        "single colour capture": (lambda run: image(run / "window.client.png", patch=((0, 40, 0, 60), (40, 40, 40, 255))), 1),
        "pixel differs outside every mask": (lambda run: image(run / "window.client.png", patch=((30, 31, 2, 3), (0, 0, 0, 255))), 1),
        "a few overlay pixels differ": (lambda run: image(run / "window.client.png", patch=((0, 2, 40, 60), (44, 40, 40, 255))), 0),
        "many overlay pixels differ": (lambda run: image(run / "window.client.png", patch=((0, 20, 40, 60), (44, 40, 40, 255))), 1),
        "blend rounding over a whole overlay": (lambda run: image(run / "window.client.png", patch=((0, 40, 40, 60), (41, 39, 40, 255))), 0),
        "blend rounding outside overlays": (lambda run: image(run / "window.client.png", patch=((30, 31, 2, 3), (41, 40, 40, 255))), 1),
        "panes file missing": (lambda run: (run / "window.client.png.panes").unlink(), 1),
        "icon name missing from xprop": (without_icon_name, 1),
        # --qml-window: panes compared at their own positions.
        "qml: pane lower with taller chrome": (lambda run: shifted(run / "window.client.png"), 0, True),
        "qml: pixel differs inside the pane": (lambda run: shifted(run / "window.client.png", patch=((30, 31, 2, 3), (0, 0, 0, 255))), 1, True),
        "qml: chrome differs, not the pane": (lambda run: image(run / "window.client.png", patch=((0, 40, 40, 60), (0, 0, 0, 255))), 0, True),
        "qml: covered part of the pane differs": (lambda run: (
            image(run / "window.client.png", patch=((30, 31, 2, 3), (0, 0, 0, 255))),
            (run / "window.client.png.panes").write_text("pane 0 0 40 40\ncovered 0 25 10 10\n")), 0, True),
        "qml: icon name missing from xprop": (without_icon_name, 0, True),
        "qml: scroll bar end in a shorter pane": (scroll_bar_in_shorter_pane, 0, True),
        # A QML dialog: by colour, not layout.
        "qml dialog: theme colours, another layout": (qml_dialog, 0, True),
        "qml dialog: a colour outside the theme": (lambda run: qml_dialog(run, control=(0, 120, 215, 255)), 1, True),
        "qml dialog: another main colour": (lambda run: qml_dialog(run, background=(200, 200, 200, 255), control=(40, 40, 40, 255)), 1, True),
        "qml dialog: single colour": (lambda run: qml_dialog(run, control=(40, 40, 40, 255)), 1, True),
        "qml dialog: an intended colour": (lambda run: qml_dialog(run, control=(203, 223, 238, 255)), 0, True),
        "qml dialog: the widget dialog's shadow beside it": (dialog_shadow, 0, True),
        "qml: a widget dialog's shadow outside its cover": (lambda run: dialog_shadow(run, qml=False), 1, True),
    }
    failures = 0
    with tempfile.TemporaryDirectory(prefix="parity-selftest-") as temp:
        reference = Path(temp) / "reference"
        scenario(reference)
        for name, (change, expected, *qml_window) in cases.items():
            baseline, run = Path(temp) / "baseline", Path(temp) / "run"
            for directory in (baseline, run):
                shutil.rmtree(directory, ignore_errors=True)
                shutil.copytree(reference, directory)
            change(run)
            if name.endswith("on both sides"):
                change(baseline)
            code = compare(baseline, run, bool(qml_window))
            ok = code == expected
            failures += not ok
            print(f"{'PASS' if ok else 'FAIL'} {name}: compare.py exited {code}, expected {expected}")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
