# Test tools

Checks that everything outside programs rely on keeps working, and that the QML build keeps no
stray widgets. Each scenario drives konsole through a script; a run fails when the hook can't
complete a step.

## Environment

Nothing is built into, installed into or loaded from the live system. `setup-env.py`
builds sonic-terminal with the `--parity-script` hook into its own directory, stages its
data files, and gives Qt only the dev-qt packages' own plugins and QML modules (through
`qt.conf`); the builder prefix is only read, for the frameworks.

```sh
tools/parity/setup-env.py --env <builder>/state/parity/env --prefix <builder>/state/install-stage/usr
```

Every run uses the real SonicWin compositor on a private X server, with a private
D-Bus, `HOME` and XDG directories, no CUPS and no PulseAudio. It fails if `konsole` or
`sonic-win` loads any file of an installed `sonicde-*` package (`forbidden.txt`).

## Running

```sh
tools/parity/run-all.py --env <env> --prefix <builder>/state/install-stage/usr \
    --plugin-baseline <builder>/state/parity/plugin-baseline --output <dir> [--only <scenario> ...]
```

`scenarios/list.txt` names the scenarios and their options.

Baselines are local to the machine that made them and live in `<builder>/state/parity/`:
`abi-baseline/` (see `abicheck.sh`) and `plugin-baseline/` (the widget-interface test
plugin, `src/parity/compatplugin`).

Other checks:

- `abicheck.sh <baseline dir> <new lib dir>`: plugin-facing classes keep their ABI.
- `filescheck.py --staged <prefix>`: installed files and integration file contents.
- `ctest-isolated.py --build <build dir> --prefix <builder>/state/install-stage/usr`: the autotests,
  including `PartCompatTest` (label `compat`), on a private X server and D-Bus.
- `no-widgets.py --env <env> --prefix <builder>/state/install-stage/usr --output <dir>`: the
  dialog scenarios and `scenarios/no-widgets-bookmarks.jsonl` (every bookmark menu dialog, at
  the top level, in a folder and from the context menu) and `scenarios/shortcuts-sync.jsonl`
  (Configure Shortcuts in one window changes another window's shortcuts) with `run.py --check-widgets` (no
  widgets but the compat windows', plugin docks', menus' and framework dialogs opened inside a
  `FrameworkDialogScope`) and `scenarios/shortcuts-editor.jsonl`, and
  `scenarios/no-widgets-stray.jsonl`, which must fail. The
  environment's `bin/keditbookmarks` is a stub that can't start, so Edit Bookmarks shows its
  error box. ctest `NoWidgetsTest` runs it when the
  build is configured with `KONSOLE_PARITY_ENV=<env>` and `KONSOLE_PARITY_PREFIX=<prefix>`.

The hook stops a run with exit status 3 on the first failure: a capture or file it
can't write, a step that finds nothing to act on, an unknown op, action, page or
button. File names keep `[A-Za-z0-9._-]` and turn everything else into `_`.

## Scenario format

One JSON object per line; lines starting with `#` are comments. Any step that acts on
a dialog takes `"in": "<window title>"` to name it when several are open; otherwise the
active dialog, then the active modal dialog, is used. `"panel": "<title>"` acts on a side
panel (a plugin's dock) instead. An `object` of `@Class` names the first visible widget of
that class (or the control inside it) where widgets have no object name; in a QML dialog,
the item whose object name is that class name (the one it replaces).

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
| `menuItem` | `menu` (KXmlGui container or action name), `path` (submenu texts), `text`, `context` | triggers an item of a (sub)menu, or with `context` clicks that entry of its context menu |
| `contextMenu` | `name`, `column`, `line` | opens the terminal's context menu at a cell and captures it |
| `dialog` | `name`, `close`, `sortColumn`, `image` (`false`: opening only), `timeout` | captures the visible dialog, every page of a page dialog |
| `click` | `object` or `text` | clicks a button in the dialog |
| `button` | `role` (`ok`, `cancel`, `apply`) | clicks a button of the dialog's button box |
| `page` | `name` | selects a page of a page dialog |
| `select` | `object`, `row` (negative counts from the end) or `text` (anywhere in a tree) | selects a row of a list or combo box |
| `setText` | `object`, `text` | sets a line edit's or plain text edit's text |
| `clickItem` | `object`, `row` and `column`, or `text` (anywhere in a tree); `double`, or `context` (an entry of its context menu) | clicks a cell of a list, table or tree |
| `clipboard` | `text`, `repeat` | puts text on the clipboard |
| `chmod` | `path` (in the private HOME), `mode` (octal) | changes permissions |
| `loadLayout` | `file` (in the private HOME) | loads a tab layout file |
| `capture` | `name` | the window (`.client.png`) and the composited screen (`.screen.png`) |
| `introspect` | `paths` | the application's D-Bus names and objects |
| `xprop` | `name` | the main window's X11 properties, per-run values masked |
| `accessible` | `name` | what screen readers see of the terminal |
| `env` | `file` | the shell's environment, written by the shell to `~/<file>`, masked |
| `describe` | `name` | windows, tabs, profiles and directories |
| `homeFile` | `file` | copies a file from the private HOME into the output |
| `listActions` | | writes every action name to the trace |
| `shortcut` | `id`, `keys` or `expect` | sets an action's shortcut, or writes it to the trace (and fails if it isn't `expect`) |
| `checkWidgets` | | from now on, fails before a step when a widget no rule allows exists (`run.py --check-widgets` adds it) |
| `strayWidgets` | | shows a parentless dialog and label (the widget check's negative case) |
| `quit` | | ends the run |
