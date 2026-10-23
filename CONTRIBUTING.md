# Contributing to gkey-config

Bug reports, translations and patches are welcome. Please open an
[issue](https://github.com/Lordbooker/gkey-config/issues) first for larger changes.

## Branches

- `dev` – development; all work happens here, pushes build test packages.
- `main` – released state; updated from `dev` by pull request, releases are tagged here.

Pull requests go against `dev`. Code, comments, commit messages and pull requests are in English.

## Building

Dependencies:

| | Arch Linux | Fedora | Debian / Ubuntu |
|---|---|---|---|
| Build tools | `base-devel cmake pkgconf` | `gcc-c++ cmake pkgconf-pkg-config` | `build-essential cmake pkg-config` |
| Qt6 | `qt6-base` | `qt6-qtbase-devel` | `qt6-base-dev` |
| Qt6 Linguist | `qt6-tools` (+ `qt6-declarative` for `lupdate`) | `qt6-qttools-devel` | `qt6-tools-dev qt6-tools-dev-tools qt6-l10n-tools` |
| libevdev | `libevdev` | `libevdev-devel` | `libevdev-dev` |
| libudev | `systemd-libs` | `systemd-devel` | `libudev-dev` |
| Tests (D-Bus) | `dbus glib2` | `dbus-daemon dbus-tools glib2` | `dbus-daemon libglib2.0-bin` |

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

| CMake option | Default | Meaning |
|---|---|---|
| `BUILD_GUI` | `ON` | build the Qt6 application `gkey-config` |
| `BUILD_TESTS` | `OFF` | build the unit tests (Qt Test) |
| `ENABLE_SANITIZERS` | `OFF` | ASan/UBSan, only for `CMAKE_BUILD_TYPE=Debug` |
| `GKEY_VERSION` | `git describe` | displayed version |
| `GKEY_PACKAGE_VERSION`, `GKEY_PACKAGE_RELEASE` | from the version, `1` | package version and release for CPack |
| `GKEY_UDEV_RULES_DIR`, `GKEY_SYSTEMD_USER_UNIT_DIR`, `GKEY_SYSUSERS_DIR` | from `pkg-config` (udev, systemd), else `lib/...` | install directories, relative to the prefix |

The code is C++23 with Qt 6: Qt types for strings and containers (standard library only at
system interfaces), errors as return values instead of exceptions, comments in English. The build
uses strict warnings (`-Wall -Wextra -Wpedantic -Wshadow -Wconversion …`); new code should not add
any. Before a pull request, run the tests once with `-DENABLE_SANITIZERS=ON` (ASan/UBSan).

Licensing follows the [REUSE specification](https://reuse.software/): each file starts with SPDX
headers (`GPL-2.0-only`), files that cannot carry one are listed in `REUSE.toml`; `reuse lint`
checks it (also in CI).

## Running the development version

1. Stop other drivers for the device (e.g. `systemctl --user stop g13` for linux-g13-driver and
   `systemctl --user stop gkeyd` for an installed gkey-config) and replug the device.
2. Install the udev rule once and replug the device again:

   ```bash
   sudo cp data/udev/70-gkey-config.rules /etc/udev/rules.d/
   sudo udevadm control --reload-rules
   sudo groupadd -r gkey && sudo usermod -aG gkey $USER   # LED access, then log in again
   ```

3. Start the daemon and the application from the build directory:

   ```bash
   GKEY_MODEL_DIR=$PWD/data/models build/src/daemon/gkeyd
   GKEY_MODEL_DIR=$PWD/data/models build/src/gui/gkey-config
   ```

   `gkeyd --list-models` lists the model files found.

`scripts/probe-device.sh` shows what the kernel exposes for the supported devices (input devices,
LEDs, hidraw, access rights); with `--events` it also records key events. It is the starting point
for checking a model file on real hardware.

## Screenshots

The images in `docs/images/` are rendered by the application itself, in English and dark mode,
from a sample configuration (status bar cropped):

```bash
export QT_QPA_PLATFORM=offscreen GKEY_MODEL_DIR=$PWD/data/models LANGUAGE=en LC_ALL=C.UTF-8
export XDG_CONFIG_HOME=/path/to/sample-config   # profiles, macros, settings.json
build/src/gui/gkey-config --dark --screenshot key.png --show-key G7 --show-tab 0
# tabs: 0 Key, 1 Backlight, 2 Stick, 3 LCD, 4 Macros
```

## Design

[docs/CONCEPT.md](docs/CONCEPT.md) describes the architecture, the file formats, the D-Bus API and
the decisions taken. Device models are described by JSON files in `data/models/`.

## Translations

The application follows the system language; translations are built into the binary. To add a
language:

1. Append its code to `GKEY_GUI_LANGUAGES` in `src/gui/CMakeLists.txt`.
2. Run `cmake --build build --target update_translations`; this creates
   `translations/gkey-config_<code>.ts`.
3. Translate it with Qt Linguist (`linguist6` or `linguist`) and rebuild.
4. Add the language to `data/desktop/*.desktop` and the AppStream file in `data/metainfo/`.

After text changes in the code, `update_translations` updates all `.ts` files. The test
`tst_translations` fails while a text is missing, unfinished or has other placeholders than the
source.

## Packages

`packaging/build-package.sh` builds the `.deb` or `.rpm` inside a container of the target
distribution, the same way the release workflow does:

```bash
docker run --rm -v "$PWD:/src" -w /src ubuntu:26.04 packaging/build-package.sh dist
docker run --rm -v "$PWD:/src" -w /src fedora:44    packaging/build-package.sh dist
```

The AUR package `gkey-config` (releases only) is in `packaging/arch/`; on every push to `dev` the
workflow builds it from the pushed commit as a test.

## Releases

1. Set the version in `project(gkey-config VERSION x.y.z)` in `CMakeLists.txt`, add the release to
   `CHANGELOG.md` and to `data/metainfo/io.github.lordbooker.GKeyConfig.metainfo.xml` (on `dev`).
2. Merge `dev` into `main` by pull request.
3. Tag `main` and push the tag:

   ```bash
   git switch main && git pull
   git tag -a vx.y.z -m "gkey-config x.y.z"
   git push origin vx.y.z
   git push origin main:dev   # dev contains the tag again
   ```

   The last line only fast-forwards `dev` to the merge commit, so that development builds show
   versions like `x.y.z-3-gabc1234`.

4. The workflow *Release packages* checks that the tag matches the project version, builds the
   packages, creates the GitHub release and publishes the AUR package `gkey-config`. Publishing to the AUR needs the repository secret `AUR_SSH_PRIVATE_KEY`:
   *Settings → Secrets and variables → Actions → Repository secrets*, the complete private SSH key
   of the AUR account without passphrase, including the `-----BEGIN`/`-----END` lines. Without it
   the AUR step only shows a notice and the files are attached to the run as artifact `aur`; after
   adding the secret, *Re-run all jobs* publishes them (existing release assets are replaced).
