# Parity tests

Checks that sonic-terminal's QML interface looks and behaves exactly like the widget
interface it replaces, and that everything outside programs rely on keeps working.
Each scenario drives konsole through a script and captures images and text; `compare.py`
compares a run with the baseline taken from the widget build.

## Environment

Nothing is built into, installed into or loaded from the live system. `setup-env.py`
builds sonic-terminal with the `--parity-script` hook into its own directory, stages its
data files, and gives Qt only the dev-qt packages' own plugins and QML modules (through
`qt.conf`); the builder prefix is only read, for the frameworks.

```sh
tools/parity/setup-env.py --env <builder>/state/parity/env --prefix <builder>/build
```

Every run uses the real SonicWin compositor on a private X server, with a private
D-Bus, `HOME` and XDG directories, no CUPS and no PulseAudio. It fails if `konsole` or
`sonic-win` loads any file of an installed `sonicde-*` package (`forbidden.txt`).

## Running

```sh
tools/parity/run-all.py --env <env> --prefix <builder>/build \
    --plugin-baseline <builder>/state/parity/plugin-baseline --output <dir> \
    [--baseline <builder>/state/parity/baseline] [--only <scenario> ...]
```

Each scenario runs twice: normally and with a color scheme whose text colors equal
their backgrounds; the pixels that differ form the text mask. `scenarios/list.txt`
names the scenarios and their options.

Baselines are local to the machine that made them (fonts, Qt build, installed
applications in context menus) and live in `<builder>/state/parity/`: `baseline/`,
`abi-baseline/` (see `abicheck.sh`) and `plugin-baseline/` (the widget-interface test
plugin, `src/parity/compatplugin`).

Other checks:

- `abicheck.sh <baseline dir> <new lib dir>`: plugin-facing classes keep their ABI.
- `filescheck.py --staged <prefix>`: installed files and integration file contents.
- `ctest-isolated.py --build <build dir> --prefix <builder>/build`: the autotests,
  including `PartCompatTest` (label `compat`), on a private X server and D-Bus.

## Comparison rules

- Text files (`trace.txt`, `dbus.txt`, `env-masked.txt`, `xprop-*.txt`,
  `accessible-*.txt`, `describe-*.txt`, `stdout.txt`, plugin logs) must match exactly.
- Outside the text mask, images must be identical.
- Inside the text mask, at most 10 % of the mask's pixels may differ (anti-aliasing and
  DPI handling). Symbolic icons take the text color, so they're part of the mask.
- Differences are drawn into `<run>/report/diff-*.png` (red: differing pixels, blue
  tint: text mask).

## Scenario format

One JSON object per line; lines starting with `#` are comments.

| op | fields | does |
| --- | --- | --- |
| `waitWindow` | `window` | waits for a main window and its terminal |
| `useWindow` | `window` | makes later steps act on that window |
| `geometry` | `x`, `y`, `width`, `height` | moves and resizes the window |
| `windowState` | `state` (`maximized`, `normal`) | changes the window state |
| `backdrop` | `fill` (color, `checker`, empty to remove) | draws a window behind the terminal |
| `type` | `text`, `target` (`focus`) | key presses to the terminal or the focused widget (`\r` is Return) |
| `key` | `sequence` | a key combination through the window system, so shortcuts fire |
| `waitText` | `text`, `timeout` | waits until the screen contains `text` (trailing spaces trimmed) |
| `wait` | `ms` | waits |
| `mouse` | `event` (`press`, `move`, `release`, `double`), `column`, `line`, `modifiers` | mouse event at a cell |
| `selection` | `label` | writes the selected text to the trace |
| `action` | `id`, `delay` | triggers an action by its name (`listActions` lists them) |
| `menu` | `name` | opens a menu by its KXmlGui container or action name, captures it |
| `popup` | `name` | captures the popup menu that is open |
| `contextMenu` | `name`, `column`, `line` | opens the terminal's context menu at a cell and captures it |
| `dialog` | `name`, `close`, `sortColumn` | captures the visible dialog, every page of a page dialog |
| `capture` | `name` | the window (`.client.png`) and the composited screen (`.screen.png`) |
| `introspect` | `paths` | the application's D-Bus names and objects |
| `xprop` | `name` | the main window's X11 properties, per-run values masked |
| `accessible` | `name` | what screen readers see of the terminal |
| `env` | `file` | the shell's environment, written by the shell to `~/<file>`, masked |
| `describe` | `name` | windows, tabs, profiles and directories |
| `homeFile` | `file` | copies a file from the private HOME into the output |
| `listActions` | | writes every action name to the trace |
| `quit` | | ends the run |
