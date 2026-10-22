# Changelog

All notable changes to gkey-config are documented in this file. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/); versions follow
[Semantic Versioning](https://semver.org/).

## [Unreleased]

### Added

- Logitech G510 (untested, no test hardware): 18 G keys, M1–M3 and MR, LCD keys, RGB backlight,
  M LEDs and the LCD pages, with a simple device picture. The headphone and microphone mute keys
  keep working while gkey-config uses the keyboard's gaming keys.
- `gkey-config --screenshot … --show-model <id>` (documentation).

### Changed

- The round button BD next to the LCD keys always shows the next LCD page and can no longer be
  bound; otherwise there was no way left to page through the LCD on the device. Existing bindings
  of BD are ignored (with a note in the daemon log). L1–L4 remain freely bindable, e.g. for
  "previous page" or a specific page.

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

[Unreleased]: https://github.com/Lordbooker/gkey-config/compare/v1.1.1...HEAD
[1.1.1]: https://github.com/Lordbooker/gkey-config/compare/v1.1.0...v1.1.1
[1.1.0]: https://github.com/Lordbooker/gkey-config/compare/v1.0.0...v1.1.0
[1.0.0]: https://github.com/Lordbooker/gkey-config/releases/tag/v1.0.0
