# gkey-config

[![Build](https://github.com/Lordbooker/gkey-config/actions/workflows/build.yml/badge.svg?branch=main)](https://github.com/Lordbooker/gkey-config/actions/workflows/build.yml)
[![Release](https://img.shields.io/github/v/release/Lordbooker/gkey-config)](https://github.com/Lordbooker/gkey-config/releases/latest)
[![AUR](https://img.shields.io/aur/version/gkey-config)](https://aur.archlinux.org/packages/gkey-config)
[![License: GPL-2.0-only](https://img.shields.io/badge/license-GPL--2.0--only-blue)](LICENSE)

Configuration tool for the gaming keys of Logitech G keyboards and keypads on Linux.

gkey-config builds on the kernel driver `hid-lg-g15`. A small user daemon (`gkeyd`) takes over the
gaming keys and sends the configured keys, key combinations and macros through a virtual input
device; the Qt6 application *G-Key Configuration* edits profiles and macros. No extra kernel or
USB driver, no input-remapper or keyd.

![G-Key Configuration main window](docs/images/main-window.png)

## Features

- Bind keys, key combinations, mouse buttons, macros, layer and profile switches to every G key
- Three layers (M1–M3) per profile, shown by the M LEDs; any number of profiles
- Macros: record from the keyboard or with MR on the device, edit step by step, run once, while
  held, as a toggle or a fixed number of times
- Backlight colour per profile
- G13 stick as four keys (diagonals press two), mouse pointer, scroll wheel or normal joystick,
  with adjustable dead zone and measured rest position
- G13 LCD: status page (profile and layer), clock, and text from scripts
- Changes apply immediately; profile and macro files are reloaded when edited by hand
- D-Bus API for scripts, tray icon, interface in six languages

## Supported devices

| Model | Status |
|---|---|
| Logitech G13 | supported |
| G11, G15, G15 v2, G510 | planned (no test hardware) |

Requires **Linux 6.19 or newer**, the first kernel with G13 support in `hid-lg-g15`. Target
desktops are KDE Plasma and GNOME on Wayland; nothing below the application depends on the display
server.

## Installation

Packages are attached to every [GitHub release](https://github.com/Lordbooker/gkey-config/releases):

| System | Package |
|---|---|
| Ubuntu 26.04 and derivatives based on it (Linux Mint, Pop!_OS, …) | `gkey-config_<version>-1.ubuntu26.04_amd64.deb` |
| Fedora 44 | `gkey-config-<version>-1.fc44.x86_64.rpm` |
| Arch Linux and derivatives | AUR: [`gkey-config`](https://aur.archlinux.org/packages/gkey-config), development version [`gkey-config-git`](https://aur.archlinux.org/packages/gkey-config-git) |

```bash
# Ubuntu
sudo apt install ./gkey-config_<version>-1.ubuntu26.04_amd64.deb
# Fedora
sudo dnf install ./gkey-config-<version>-1.fc44.x86_64.rpm
# Arch Linux, with an AUR helper such as yay or paru
yay -S gkey-config
```

The package manager installs all dependencies. Recommended packages are installed by default
as well: `pkexec`/`polkit` (LED access from the application), the Qt Wayland plugin and `xdg-utils`.
gkey-config conflicts with [linux-g13-driver](#relation-to-linux-g13-driver); the package manager
removes the old driver or asks you to.

### First start

Open *G-Key Configuration* from the application menu. On its first start it offers to

- start the daemon at every login (`systemctl --user enable --now gkeyd` for your user), and
- allow LED control by adding your user to the group `gkey`. The system asks for the
  administrator password; log out completely and back in afterwards.

Both can be changed at any time in the *Settings* menu, together with *Show the tray icon at
login*. All three settings apply to your user only.

### Upgrade and removal

After an upgrade, restart a running daemon: `systemctl --user restart gkeyd`.

Remove the package with `sudo apt remove gkey-config`, `sudo dnf remove gkey-config` or
`sudo pacman -R gkey-config`. Profiles and macros stay in `~/.config/gkey-config/`.

## Usage

Click a key in the picture – or press it on the device – and choose what it does. Every change is
saved immediately and applied by the daemon. The profile list and the layer buttons M1–M3 switch
the device as well.

| Tab | Purpose |
|---|---|
| Key | binding of the selected key in the current layer |
| Backlight | colour of the profile, LED access |
| Stick | stick mode, dead zone, speeds, rest position |
| LCD | pages and their order; the round key next to the LCD keys pages through them |
| Macros | create, record and edit macros (shared by all profiles and devices) |

Closing the window keeps the tray icon running; the daemon works without the application. GNOME
shows tray icons only with the extension *AppIndicator and KStatusNotifierItem Support* (preinstalled
on Ubuntu); without it the application simply runs without a tray icon.

### Files

| Path | Content |
|---|---|
| `~/.config/gkey-config/profiles/<model>/*.json` | profiles, one file each |
| `~/.config/gkey-config/macros/*.json` | macros |
| `~/.config/gkey-config/settings.json` | LCD pages |
| `~/.local/state/gkey-config/state.json` | active profile and layer, stick rest position |

The files are plain JSON and may be edited by hand; the daemon reloads them on save. The formats
are described in [docs/CONCEPT.md](docs/CONCEPT.md) (profiles in section 5, macros in section 6).

### Scripts (D-Bus)

The daemon offers a D-Bus API on the session bus ([CONCEPT.md, section 10](docs/CONCEPT.md)).
A call starts the daemon on demand. Examples:

```bash
DEST="io.github.lordbooker.GKeyConfig /io/github/lordbooker/GKeyConfig io.github.lordbooker.GKeyConfig1"
busctl --user call $DEST Status                    # state as JSON
busctl --user call $DEST SetProfile ss g13 Gaming  # switch profile
busctl --user call $DEST SetMode si g13 2          # switch to layer M2
busctl --user call $DEST LcdText si "Build done" 5 # text on the LCD for 5 seconds
busctl --user call $DEST RunMacro s hello          # run a macro
```

## How it works

| Part | Role |
|---|---|
| `gkeyd` (systemd user service) | grabs the gaming-key input devices, applies the bindings, drives LEDs and LCD |
| `gkey-config` | configuration application and tray icon |
| `70-gkey-config.rules` (udev) | gives the user of the active local session access to the devices and `/dev/uinput` (`uaccess`) |
| group `gkey` | LED attributes in sysfs cannot carry ACLs; members of this group may change them |

Without membership in `gkey` everything except the LEDs works. To add a user by hand:
`sudo usermod -aG gkey $USER`, then log out completely and back in.

## Troubleshooting

- **Daemon log:** `journalctl --user -u gkeyd -f`
- **Daemon state:** *Settings* menu and status bar of the application, or the `Status` call above
- **Device not connected:** the status bar of the application names the reason when the device is
  plugged in but not usable – kernel older than 6.19, kernel driver `hid-lg-g15` not active, or
  another program (e.g. linux-g13-driver) using the device directly.
- **Keys do nothing:** check that no other tool grabs the device (e.g. input-remapper) and replug
  the device.
- **LEDs do not change:** see the *Backlight* tab; after joining the group `gkey` a complete
  logout is needed.
- **Report a problem:** attach the output of [`scripts/probe-g13.sh`](scripts/probe-g13.sh) and the
  daemon log to the [issue](https://github.com/Lordbooker/gkey-config/issues).

## Languages

The application follows the system language. Included are English, German, French, Italian,
Spanish and Portuguese (Brazilian); other languages fall back to English. The French, Italian,
Spanish and Portuguese texts have not been reviewed by native speakers yet – corrections are
welcome. Standard dialog buttons come from Qt's own translations (`qt6-translations` on Arch,
`qt6-translations-l10n` on Ubuntu, `qt6-qttranslations` on Fedora).

## Building from source

See [CONTRIBUTING.md](CONTRIBUTING.md) for dependencies, build options, tests, translations and
the release process. In short:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr
cmake --build build
sudo cmake --install build
```

## Relation to linux-g13-driver

[linux-g13-driver](https://github.com/Lordbooker/linux-g13-driver) drives the G13 with its own
libusb driver and detaches the kernel driver. gkey-config is its successor for kernels with
`hid-lg-g15` G13 support. Both cannot run at the same time; the packages conflict with each other.

## License

GNU General Public License, version 2 only (GPL-2.0-only) – see [LICENSE](LICENSE).
