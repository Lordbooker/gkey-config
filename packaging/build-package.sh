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

# Ubuntu mirrors, tried in this order (the first one is inside Azure, where the
# GitHub runners are). Debian uses its own mirror (deb.debian.org) every time.
UBUNTU_MIRRORS="http://azure.archive.ubuntu.com/ubuntu/ http://archive.ubuntu.com/ubuntu/ http://us.archive.ubuntu.com/ubuntu/"

# Points the Ubuntu archive (not the security archive) to the given mirror.
apt_set_mirror() {
    sed -i "s#^URIs: http://[a-z.]*archive\.ubuntu\.com/ubuntu/*\$#URIs: $1#" \
        /etc/apt/sources.list.d/ubuntu.sources 2>/dev/null || true
}

# Installs packages with apt. A mirror can be so slow that the CI job runs into
# its time limit although data keeps arriving (no timeout of apt applies): the
# index and the downloads get 5 minutes per mirror, then the next mirror is
# tried. Packages already downloaded stay in the cache. Installing from the
# cache has no time limit (a killed dpkg could leave the system half set up).
apt_install() {
    if [ "$ID" = ubuntu ]; then
        mirrors=$UBUNTU_MIRRORS
    else
        mirrors="current current current"
    fi
    for mirror in $mirrors; do
        [ "$mirror" = current ] || apt_set_mirror "$mirror"
        if timeout 300 apt-get update -q &&
            timeout 300 apt-get install -y -q --no-install-recommends --download-only "$@"; then
            apt-get install -y -q --no-install-recommends "$@"
            return
        fi
        echo "apt: mirror $mirror too slow or failing, trying the next one" >&2
        sleep 10
    done
    echo "apt: no mirror delivered the packages" >&2
    return 1
}

case "$ID" in
    debian|ubuntu)
        export DEBIAN_FRONTEND=noninteractive
        # Retry: a mirror in sync can serve inconsistent index files for a while.
        # Timeout: a download that stalls (no data for 30 s) fails and is retried.
        printf 'Acquire::Retries "5";\nAcquire::http::Timeout "30";\nAcquire::https::Timeout "30";\n' \
            > /etc/apt/apt.conf.d/80-retries
        apt_install \
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
        apt_install "$PACKAGE"
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
