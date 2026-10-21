# gkey-config

Configuration tool for the gaming keys of Logitech G keyboards and keypads on Linux.

gkey-config builds on the kernel driver `hid-lg-g15`: a user session daemon (`gkeyd`) reads the
gaming-key input devices exclusively and sends the configured keys, key combinations and macros
through a virtual input device; LEDs are set through `/sys/class/leds`. The Qt6 tool `gkey-config`
edits profiles and macros. No libusb driver, no input-remapper or keyd.

> **Version 1.0.** Supported on the G13: key, key combination, mouse button and macro bindings,
> M1–M3 layers with LEDs, backlight colour per profile, stick as keys/mouse/scroll/joystick, LCD
> pages (status, clock, text from scripts), profiles and macros with live reload, D-Bus API and the
> configuration tool in six languages. Per-application profiles and the other models follow.

![gkey-config main window](docs/images/main-window.png) See [docs/CONCEPT.md](docs/CONCEPT.md) for the design and the milestones.

## Supported devices

| Model | Status |
|---|---|
| Logitech G13 | in development (the only model with test hardware) |
| G11, G15, G15 v2, G510 | planned, will be marked untested |

Requires **Linux 6.19 or newer** (first kernel with G13 support in `hid-lg-g15`).

## Installation

Ready-made packages are attached to every [GitHub release](https://github.com/Lordbooker/gkey-config/releases)
(Linux 6.19 or newer is required):

| System | Package |
|---|---|
| Ubuntu 26.04 and derivatives (Linux Mint, Pop!_OS, … based on it) | `gkey-config_<version>-1.ubuntu26.04_amd64.deb` |
| Fedora 44 | `gkey-config-<version>-1.fc44.x86_64.rpm` |
| Arch Linux and derivatives | AUR: `gkey-config` (release) or `gkey-config-git` (development version) |

```bash
# Ubuntu
sudo apt install ./gkey-config_<version>-1.ubuntu26.04_amd64.deb
# Fedora
sudo dnf install ./gkey-config-<version>-1.fc44.x86_64.rpm
# Arch Linux (with an AUR helper, e.g. yay or paru)
yay -S gkey-config
```

gkey-config and [linux-g13-driver](#relation-to-linux-g13-driver) conflict with each other; the
package manager removes the old driver or asks you to.

**First start:** open *G-Key Configuration* from the application menu. It offers, once, to

* start the daemon `gkeyd` at every login (`systemctl --user enable --now gkeyd` for your user) and
* allow LED control by adding your user to the group `gkey` (the system asks for the
  administrator password; log out completely and back in afterwards).

Both can be changed at any time in the *Settings* menu, together with *Show the tray icon at
login*; all three settings apply to your user only. Scripts that call the daemon's D-Bus API (e.g.
`LcdText`) start it on demand even without autostart; the configuration tool itself never starts
it unasked.

After an upgrade, restart a running daemon: `systemctl --user restart gkeyd`.

**Uninstall:** remove the package (`sudo apt remove gkey-config`, `sudo dnf remove gkey-config`
or `sudo pacman -R gkey-config`). Your profiles and macros stay in `~/.config/gkey-config/`.

## Build

Dependencies:

| | Arch Linux | Fedora | Debian / Ubuntu |
|---|---|---|---|
| Build tools | `base-devel cmake pkgconf` | `gcc-c++ cmake pkgconf-pkg-config` | `build-essential cmake pkg-config` |
| Qt6 | `qt6-base` | `qt6-qtbase-devel` | `qt6-base-dev` |
| Qt6 Linguist (GUI translations) | `qt6-tools` | `qt6-qttools-devel` | `qt6-tools-dev qt6-tools-dev-tools qt6-l10n-tools` |
| libevdev | `libevdev` | `libevdev-devel` | `libevdev-dev` |
| libudev | `systemd-libs` | `systemd-devel` | `libudev-dev` |

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr
cmake --build build
```

CMake options:

| Option | Default | Meaning |
|---|---|---|
| `BUILD_GUI` | `ON` | build the Qt6 tool `gkey-config` |
| `BUILD_TESTS` | `OFF` | build the unit tests (Qt Test), run with `ctest --test-dir build` |
| `ENABLE_SANITIZERS` | `OFF` | ASan/UBSan, only for `CMAKE_BUILD_TYPE=Debug` |

To run the daemon from the build directory without installing the model files:

```bash
GKEY_MODEL_DIR=$PWD/data/models build/src/daemon/gkeyd --list-models
```

## Trying the development version

1. Stop other G13 drivers (e.g. `systemctl --user stop g13` for linux-g13-driver) and replug the G13.
2. Install the udev rule once and replug the G13 again:

   ```bash
   sudo cp data/udev/70-gkey-config.rules /etc/udev/rules.d/
   sudo udevadm control --reload-rules
   ```

3. Build (see above) and start the daemon in a terminal:

   ```bash
   GKEY_MODEL_DIR=$PWD/data/models build/src/daemon/gkeyd
   ```

4. Edit `~/.config/gkey-config/profiles/g13/default.json` (format: [CONCEPT.md, section 5.3a](docs/CONCEPT.md));
   the daemon reloads it on save. Example:

   ```json
   {
     "name": "Default",
     "modes": {
       "M1": {
         "G1": { "type": "key", "key": "KEY_1" },
         "G2": { "type": "combo", "keys": ["KEY_LEFTCTRL", "KEY_C"] },
         "L1": { "type": "profile", "profile": "next" }
       },
       "M2": { "G1": { "type": "key", "key": "KEY_2" } },
       "M3": {}
     }
   }
   ```

M1–M3 switch between the three binding layers of the active profile; their LEDs show the active
layer. `"backlight": [r, g, b]` in a profile sets the G13 colour. The stick is set per profile with
`"stick": { "mode": "keys" }` (`keys`, `mouse`, `scroll`, `passthrough`; without it the stick stays a
normal joystick) and the directions are bound as `STICK_UP`, `STICK_DOWN`, `STICK_LEFT`,
`STICK_RIGHT`.

5. Macros live in `~/.config/gkey-config/macros/` (format: [CONCEPT.md, section 6.1](docs/CONCEPT.md)),
   e.g. `hello.json`:

   ```json
   { "name": "hello", "steps": [ { "tap": "KEY_H" }, { "tap": "KEY_I" } ] }
   ```

   and are bound with `{ "type": "macro", "macro": "hello", "repeat": "once" }`
   (`once`, `while_held`, `toggle`, `count` with `"count": n`).

6. The daemon can be controlled over D-Bus ([CONCEPT.md, section 10](docs/CONCEPT.md)), e.g.:

   ```bash
   busctl --user call io.github.lordbooker.GKeyConfig /io/github/lordbooker/GKeyConfig \
       io.github.lordbooker.GKeyConfig1 Status
   busctl --user call io.github.lordbooker.GKeyConfig /io/github/lordbooker/GKeyConfig \
       io.github.lordbooker.GKeyConfig1 SetMode si g13 2
   ```

LED control needs membership in the group `gkey` (see [Permissions](#permissions)). For a manual
installation create the group first: `sudo groupadd -r gkey`.

## Configuration tool

`build/src/gui/gkey-config` (installed: *G-Key Configuration* in the application menu). Click a
key in the picture – or press it on the device – and choose what it does; every change is saved
immediately and the daemon picks it up. The profile list and the layer buttons M1–M3 switch the
device as well. Macros are created and recorded in the *Macros* tab; MR on the device starts and
stops a recording. Closing the window keeps the tray icon running.

## Translations

The configuration tool uses the system language (`LANGUAGE`, `LC_ALL`, `LC_MESSAGES`, `LANG`);
there is no language setting of its own. Included are **English** (source and fallback), **German**,
**French**, **Italian**, **Spanish** and **Portuguese** (Brazilian); all other languages use English.
The translations are built into `gkey-config`, no extra files are installed. The French, Italian,
Spanish and Portuguese texts have not been reviewed by native speakers yet – corrections are welcome.

Standard dialog buttons (e.g. *Cancel*) come from Qt's own translations – package
`qt6-translations` (Arch), `qt6-translations-l10n` (Debian/Ubuntu) or `qt6-qttranslations`
(Fedora). The daemon's log messages are English; the day names on the LCD clock follow the language
of the user session.

To add a language, append its code to `GKEY_GUI_LANGUAGES` in `src/gui/CMakeLists.txt`, run
`cmake --build build --target update_translations` to create `translations/gkey-config_<code>.ts`,
translate it with Qt Linguist (`linguist6` or `linguist`) and rebuild. The same target updates all
`.ts` files after text changes in the code; the test `tst_translations` fails while a text is
untranslated. On Arch Linux, `lupdate` (used by both) also needs `qt6-declarative`.

## Permissions

The udev rule `70-gkey-config.rules` gives the user of the active local session access to the
supported devices and to `/dev/uinput` (`uaccess`). LED attributes in sysfs cannot carry ACLs;
members of the group `gkey` may change them. The configuration tool adds your user on request
(*Settings → Allow LED control…*, via `pkexec`); by hand:

```bash
sudo usermod -aG gkey $USER    # then log out completely and back in
```

## Releases (maintainers)

1. Set the version in `project(gkey-config VERSION x.y.z)` in `CMakeLists.txt` on `dev`.
2. Merge `dev` into `main` (pull request) and tag `main`: `git tag vx.y.z && git push origin vx.y.z`.
3. The workflow *Release packages* builds the `.deb`/`.rpm`, creates the GitHub release and
   publishes the AUR packages `gkey-config` and `gkey-config-git`. Publishing to the AUR needs the
   repository secret `AUR_SSH_PRIVATE_KEY` (private SSH key of the AUR account); without it the
   AUR files are only attached to the run as artifact `aur`.

Pushes to `dev` build all packages as artifacts for testing.

## Relation to linux-g13-driver

[linux-g13-driver](https://github.com/Lordbooker/linux-g13-driver) drives the G13 with its own
libusb driver and detaches the kernel driver. Both cannot run at the same time; the packages
conflict with each other.

## License

GNU General Public License, version 2 only (GPL-2.0-only) – see [LICENSE](LICENSE).
