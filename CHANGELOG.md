# Changelog

All notable changes to gkey-config are documented in this file. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/); versions follow
[Semantic Versioning](https://semver.org/).

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

[1.0.0]: https://github.com/Lordbooker/gkey-config/releases/tag/v1.0.0
