#!/bin/sh
# SPDX-FileCopyrightText: 2026 Lordbooker
# SPDX-License-Identifier: GPL-2.0-only
#
# Builds the SteamOS archive gkey-config-<version>-steamos-x86_64.tar.gz in a
# container prepared by setup-container.sh (SteamOS package versions):
#
#   packaging/steamos/setup-container.sh base-devel cmake git pkgconf qt6-base qt6-tools libevdev systemd
#   packaging/steamos/build-package.sh dist
#
# Argument: output directory (default: dist/ in the repository)
# Optional: GKEY_PACKAGE_VERSION, e.g. 1.5.0 (default: the version of the build)
#
# SteamOS keeps /usr read-only and replaces it with every update, so the
# archive is installed into the home directory by install.sh (see there).
set -eu

REPO_DIR=$(cd "$(dirname "$0")/../.." && pwd)
OUT_DIR=${1:-"$REPO_DIR/dist"}
BUILD_DIR=$(mktemp -d)
STAGE=$(mktemp -d)

# In the container the repository belongs to another user: allow it for git describe.
git config --global --add safe.directory '*'

set -- -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr -DBUILD_TESTS=OFF
if [ -n "${GKEY_PACKAGE_VERSION:-}" ]; then
    set -- "$@" -DGKEY_VERSION="$GKEY_PACKAGE_VERSION"
fi
cmake -S "$REPO_DIR" -B "$BUILD_DIR" "$@"
cmake --build "$BUILD_DIR" -j "$(nproc)"
DESTDIR="$STAGE" cmake --install "$BUILD_DIR"

VERSION=${GKEY_PACKAGE_VERSION:-$("$STAGE/usr/bin/gkeyd" --version | awk '{print $NF}')}
NAME="gkey-config-$VERSION-steamos-x86_64"
ROOT="$BUILD_DIR/$NAME"

mkdir -p "$ROOT/bin" "$ROOT/share" "$ROOT/system"
cp "$STAGE/usr/bin/gkeyd" "$STAGE/usr/bin/gkey-config" "$ROOT/bin/"
cp -r "$STAGE/usr/share/gkey-config" "$STAGE/usr/share/applications" "$STAGE/usr/share/icons" \
    "$STAGE/usr/share/metainfo" "$ROOT/share/"
# Templates with the paths of a /usr installation; install.sh puts in the
# paths below the home directory.
cp "$STAGE/usr/lib/systemd/user/gkeyd.service" "$ROOT/system/"
cp "$STAGE/usr/share/dbus-1/services/io.github.lordbooker.GKeyConfig.service" "$ROOT/system/"
cp "$REPO_DIR/data/desktop/gkey-config-tray.desktop" "$ROOT/system/"
cp "$REPO_DIR/data/udev/70-gkey-config.rules" "$ROOT/system/"
cp "$REPO_DIR/packaging/steamos/install.sh" "$REPO_DIR/packaging/steamos/uninstall.sh" "$ROOT/"
cp "$REPO_DIR/packaging/steamos/README.md" "$ROOT/README-SteamOS.md"
cp "$REPO_DIR/LICENSE" "$ROOT/"
echo "$VERSION" > "$ROOT/VERSION"

mkdir -p "$OUT_DIR"
tar -C "$BUILD_DIR" -czf "$OUT_DIR/$NAME.tar.gz" "$NAME"
echo "Archive: $OUT_DIR/$NAME.tar.gz"
tar -tzf "$OUT_DIR/$NAME.tar.gz"
