# Sonic Terminal Release Notes

SonicDE's terminal app, with tabs, split views and profiles for working on the command line.

## Unreleased

- Minimizing a terminal window works again: the window stays in the taskbar and can be restored,
  instead of disappearing while the terminal keeps running in the background.

## 26.08.3 (2026-11-10)

- The terminal now records its logs through SonicDE's unified logging system.
- The terminal window and its dialogs, such as the settings, profile editor, toolbar and shortcut editors and the
  command bar, are now drawn with QML; they look, work and are translated as before.
- The tab bar's "Use user-defined stylesheet" option has been removed.
- ZModem file transfers are no longer supported.
- Keyboard shortcuts are now saved in `konsolerc`; your existing shortcuts are moved there
  the first time the terminal starts.
- Third-party plugins built for the widget interface still load, but a `KCommandBar` they
  open themselves doesn't show.
- Other programs can now show a terminal through the `org.sonicde.terminal.app` QML module,
  including the whole terminal with its tabs and toolbars, as the Terminal Panel widget does.
- Scripts can now also reach the terminal under a SonicDE D-Bus name, `org.sonicde.terminal-<pid>`.
- With the Terminal Panel widget in a panel, opening a terminal now brings that panel out with a
  new tab, ready for typing. Commands and `--separate` still get their own window, and so does
  "Open a New Window".
- Behind the scenes: new automated checks compare the terminal's look and behaviour, and its compatibility with
  other programs and plugins, against a reference.

## 26.08.2 (2026-10-20)

- Behind the scenes: version number update.

## 26.08.0 (2026-09-12)

- Fewer legacy compatibility packages are needed: the terminal no longer depends on the old Plasma and Qt 5 support libraries.
- Streamlined for SonicDE's X11 desktop: Wayland support has been removed.
- Behind the scenes: version number update.

## 26.04.3.2 (2026-08-26)

- The blurred terminal background works again alongside the Silver window frame's unified blur.

## 26.04.3.1 (2026-08-04)

- More of the terminal's look now uses SonicDE's Silver theme instead of Breeze.
- Behind the scenes: improved code formatting and developer tooling.
