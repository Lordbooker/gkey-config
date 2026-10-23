# Changelog

All notable changes to gkey-config are documented in this file. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/); versions follow
[Semantic Versioning](https://semver.org/).

## [Unreleased]

### Security

- The configuration tool's single-instance socket is now in the user's runtime directory
  (`$XDG_RUNTIME_DIR`) and only accessible by the user. Before, it was in `/tmp`, where another
  local user could block the configuration tool from starting.

### Changed

- The daemon writes the LCD in a thread of its own: a hanging USB device can no longer stop the
  key handling (a write to the LCD can block for seconds).

### Fixed

- The LCD stayed off for the whole connection when its device node appeared after the keys;
  the daemon now looks for it when writing and reopens it after an error.
- Errors when sending keys through the virtual input device are logged (once per kind of error).
- Saving a profile from the configuration tool no longer drops bindings that this version cannot
  use (e.g. key names of a newer version, or keys that cannot be bound on the device); they are
  kept unchanged in the file.
- Logitech G11: the minimum kernel is Linux 5.7, not 5.5 (the G11 was added to `hid-lg-g15` after
  the G15 and G510). The status bar now reports Linux 5.5 and 5.6 as too old for the G11.

## [1.2.0] - 2026-10-23

### Added

- Logitech G510 (untested, no test hardware): 18 G keys, M1–M3 and MR, LCD keys, RGB backlight,
  M LEDs and the LCD pages, with a simple device picture. The headphone and microphone mute keys
  keep working while gkey-config uses the keyboard's gaming keys.
- Logitech G15, first version (untested, no test hardware): 18 G keys, M1–M3 and MR, LCD keys,
  M LEDs and the LCD pages. The backlight of the keys and of the LCD can be set per profile in
  three steps (off, dim, bright).
- Logitech G11 (untested, no test hardware): 18 G keys, M1–M3 and MR, M LEDs and the backlight
  brightness per profile.
- Logitech G15 v2 (untested, no test hardware): 6 G keys, M1–M3 and MR, LCD keys, M LEDs, the
  LCD pages and the backlight brightness of keys and LCD per profile.
- `gkey-config --screenshot … --show-model <id>` (documentation).
- Documentation moved to the [wiki](https://github.com/Lordbooker/gkey-config/wiki), with one page
  per device; the README is a short overview.

### Changed

- The round button BD next to the LCD keys always shows the next LCD page and can no longer be
  bound; otherwise there was no way left to page through the LCD on the device. Existing bindings
  of BD are ignored (with a note in the daemon log). L1–L4 remain freely bindable, e.g. for
  "previous page" or a specific page.

### Fixed

- G13 stick in key mode: after a restart of the computer a key bound to the stick (e.g. A for
  "left") stayed pressed and did not work on the normal keyboard until the daemon was restarted.
  Before the G13 reports for the first time, the kernel shows the stick at 0/0 ("left" and "up");
  the daemon no longer takes this as the stick position.

## [1.1.1] - 2026-10-22

### Fixed

- LCD page *System*: the RAM row showed no value. Files in `/proc` report a size of 0, so
  `/proc/meminfo` was read as empty.

## [1.1.0] - 2026-10-22

### Added

- LCD page *System*: load of CPU, RAM, GPU and VRAM as bars with percentages, temperatures of CPU
  and GPU, used RAM and VRAM. GPU values for AMD graphics cards (`amdgpu`); without one the page
  shows CPU, RAM and swap. Enabled by default for new installations; existing settings list it
  unchecked in the LCD tab.
- Degree sign in the LCD font.
- `gkey-config --screenshot … --dark` renders the window in a dark colour scheme (documentation).

### Fixed

- The status bar of the configuration tool was several lines high when a long diagnosis text was
  shown.

## [1.0.0] - 2026-10-22

First release.

### Added

- Daemon `gkeyd` for the Logitech G13 on the kernel driver `hid-lg-g15`: exclusive access to the
  gaming keys, output through a virtual input device, hotplug, live reload of profiles and macros.
- Bindings: key, key combination, mouse button, macro, layer, profile and LCD page.
- Three layers (M1–M3) per profile with the M LEDs, any number of profiles, backlight colour per
  profile.
- Macros with recording (keyboard or MR on the device) and the modes once, while held, toggle and
  count.
- G13 stick as keys (diagonals press two), mouse pointer, scroll wheel or joystick; dead zone,
  speeds and measured rest position.
- G13 LCD: status page, clock and text from scripts; pages selectable and ordered.
- D-Bus API on the session bus, with D-Bus activation of the daemon.
- Application *G-Key Configuration* with drawn device picture, tray icon, first-start setup of
  the daemon autostart and LED access, per-user autostart settings.
- Diagnosis in the status bar when a plugged-in device is not usable (kernel too old, kernel driver
  not active, device used by another program).
- Translations: English, German, French, Italian, Spanish, Portuguese.
- Packages for Ubuntu 26.04 and Fedora 44, AUR package `gkey-config`.

[Unreleased]: https://github.com/Lordbooker/gkey-config/compare/v1.2.0...HEAD
[1.2.0]: https://github.com/Lordbooker/gkey-config/compare/v1.1.1...v1.2.0
[1.1.1]: https://github.com/Lordbooker/gkey-config/compare/v1.1.0...v1.1.1
[1.1.0]: https://github.com/Lordbooker/gkey-config/compare/v1.0.0...v1.1.0
[1.0.0]: https://github.com/Lordbooker/gkey-config/releases/tag/v1.0.0
