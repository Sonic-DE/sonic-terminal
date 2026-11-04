# Sonic Terminal Release Notes

SonicDE's terminal app, with tabs, split views and profiles for working on the command line.

## Unreleased

- The terminal now records its logs through SonicDE's unified logging system.
- The terminal window is now drawn with QML; it looks and works as before.
- The tab bar's "Use user-defined stylesheet" option has been removed.
- ZModem file transfers are no longer supported.
- Keyboard shortcuts are now saved in `konsolerc`; your existing shortcuts are moved there
  the first time the terminal starts.

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
