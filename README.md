# gkey-config

Configuration tool for the gaming keys of Logitech G keyboards and keypads on Linux.

gkey-config builds on the kernel driver `hid-lg-g15`: a user session daemon (`gkeyd`) reads the
gaming-key input devices exclusively and sends the configured keys, key combinations and macros
through a virtual input device; LEDs are set through `/sys/class/leds`. The Qt6 tool `gkey-config`
edits profiles and macros. No libusb driver, no input-remapper or keyd.

> **Status: early development.** Milestone 1 (project skeleton) is done; the daemon only loads the
> model files so far. See [docs/CONCEPT.md](docs/CONCEPT.md) for the design and the milestones.

## Supported devices

| Model | Status |
|---|---|
| Logitech G13 | in development (the only model with test hardware) |
| G11, G15, G15 v2, G510 | planned, will be marked untested |

Requires **Linux 6.19 or newer** (first kernel with G13 support in `hid-lg-g15`).

## Build

Dependencies:

| | Arch Linux | Fedora | Debian / Ubuntu |
|---|---|---|---|
| Build tools | `base-devel cmake pkgconf` | `gcc-c++ cmake pkgconf-pkg-config` | `build-essential cmake pkg-config` |
| Qt6 | `qt6-base` | `qt6-qtbase-devel` | `qt6-base-dev` |
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

## Permissions

The udev rule `70-gkey-config.rules` gives the user of the active local session access to the
supported devices and to `/dev/uinput` (`uaccess`). LED attributes in sysfs cannot carry ACLs;
members of the group `gkey` may change them. To allow LED control:

```bash
sudo usermod -aG gkey $USER    # then log out and back in
```

## Relation to linux-g13-driver

[linux-g13-driver](https://github.com/Lordbooker/linux-g13-driver) drives the G13 with its own
libusb driver and detaches the kernel driver. Both cannot run at the same time; the packages
conflict with each other.

## License

GNU General Public License, version 2 only (GPL-2.0-only) – see [LICENSE](LICENSE).
