#!/bin/sh
# SPDX-FileCopyrightText: 2026 Lordbooker
# SPDX-License-Identifier: GPL-2.0-only

#
# Builds a .deb (Ubuntu) or .rpm (Fedora) for the running system. Intended for
# a fresh container of the target distribution (as root):
#
#   docker run --rm -v "$PWD:/src" -w /src ubuntu:26.04 packaging/build-package.sh dist
#   docker run --rm -v "$PWD:/src" -w /src fedora:44    packaging/build-package.sh dist
#
# Argument: output directory (default: dist/ in the repository)
# Optional: GKEY_PACKAGE_VERSION, e.g. 1.0.0 (default: from the git tag)
#
set -eu

# shellcheck source=/dev/null
. /etc/os-release

REPO_DIR=$(cd "$(dirname "$0")/.." && pwd)
OUT_DIR=${1:-"$REPO_DIR/dist"}
BUILD_DIR=$(mktemp -d)

case "$ID" in
    debian|ubuntu)
        export DEBIAN_FRONTEND=noninteractive
        apt-get update -q
        apt-get install -y -q --no-install-recommends \
            build-essential cmake pkg-config git ca-certificates file dpkg-dev \
            qt6-base-dev qt6-tools-dev qt6-tools-dev-tools qt6-l10n-tools libgl-dev \
            libevdev-dev libudev-dev
        GENERATOR=DEB
        # e.g. 1~ubuntu26.04: unique file names per system
        RELEASE="1~${ID}${VERSION_ID}"
        ;;
    fedora)
        dnf install -y -q \
            gcc-c++ make cmake pkgconf-pkg-config git rpm-build \
            qt6-qtbase-devel qt6-qttools-devel libevdev-devel systemd-devel
        GENERATOR=RPM
        RELEASE="1"          # CPack appends %{?dist} (e.g. .fc44)
        ;;
    *)
        echo "Unsupported distribution: $ID" >&2
        exit 1
        ;;
esac

# In the container the repository belongs to another user: allow it for git describe.
git config --global --add safe.directory '*'

set -- -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr -DBUILD_TESTS=OFF \
       -DGKEY_PACKAGE_RELEASE="$RELEASE"
if [ -n "${GKEY_PACKAGE_VERSION:-}" ]; then
    set -- "$@" -DGKEY_PACKAGE_VERSION="$GKEY_PACKAGE_VERSION" -DGKEY_VERSION="$GKEY_PACKAGE_VERSION"
fi

cmake -S "$REPO_DIR" -B "$BUILD_DIR" "$@"
cmake --build "$BUILD_DIR" -j "$(nproc)"
(cd "$BUILD_DIR" && cpack -G "$GENERATOR")

# Check the package: it installs with its dependencies (and runs its scripts),
# and every recommended package exists in the distribution.
PACKAGE=$(find "$BUILD_DIR" -maxdepth 1 \( -name '*.deb' -o -name '*.rpm' \) | head -n 1)
case "$GENERATOR" in
    DEB)
        dpkg-deb -I "$PACKAGE"
        apt-get install -y -q --no-install-recommends "$PACKAGE"
        for dep in $(dpkg-deb -f "$PACKAGE" Recommends | tr -d ' ' | tr ',' ' '); do
            apt-cache show "$dep" >/dev/null 2>&1 || { echo "Recommended package not found: $dep" >&2; exit 1; }
        done
        ;;
    RPM)
        rpm -qpi "$PACKAGE"
        echo "Requires:";   rpm -qp --requires "$PACKAGE"
        echo "Recommends:"; rpm -qp --recommends "$PACKAGE"
        echo "Suggests:";   rpm -qp --suggests "$PACKAGE"
        dnf install -y -q --setopt=install_weak_deps=False "$PACKAGE"
        for dep in $(rpm -qp --recommends "$PACKAGE") $(rpm -qp --suggests "$PACKAGE"); do
            dnf -q repoquery --whatprovides "$dep" | grep -q . || { echo "Recommended package not found: $dep" >&2; exit 1; }
        done
        ;;
esac
gkeyd --list-models

mkdir -p "$OUT_DIR"
cp "$PACKAGE" "$OUT_DIR"/
echo "Packages in $OUT_DIR:"
ls -1 "$OUT_DIR"
