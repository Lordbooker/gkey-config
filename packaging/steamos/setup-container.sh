#!/bin/sh
# SPDX-FileCopyrightText: 2026 Lordbooker
# SPDX-License-Identifier: GPL-2.0-only
#
# Turns a fresh Arch Linux container (as root) into a SteamOS build and test
# environment: the package repositories of a SteamOS release replace those of
# Arch, and all installed packages are moved to their SteamOS versions
# (pacman -Syuu also downgrades). Further packages are given as arguments.
#
#   packaging/steamos/setup-container.sh [package ...]
#
# STEAMOS_VERSION selects the release (default below; raise it when Valve
# releases a new stable SteamOS).
set -eu

STEAMOS_VERSION=${STEAMOS_VERSION:-3.9}
MIRROR=https://steamdeck-packages.steamos.cloud/archlinux-mirror

# Valve's signing keys (holo-keyring), fetched over HTTPS from Valve's server;
# with them pacman checks the signature of every package as usual.
work=$(mktemp -d)
curl -fsS --retry 3 -o "$work/holo.db" "$MIRROR/holo-$STEAMOS_VERSION/os/x86_64/holo-$STEAMOS_VERSION.db"
mkdir "$work/db"
tar -xf "$work/holo.db" -C "$work/db"
desc=$(find "$work/db" -path '*/holo-keyring-[0-9]*/desc' | head -n 1)
[ -n "$desc" ] || { echo "holo-keyring not found in holo-$STEAMOS_VERSION" >&2; exit 1; }
file=$(sed -n '/^%FILENAME%$/{n;p;}' "$desc")
curl -fsS --retry 3 -o "$work/$file" "$MIRROR/holo-$STEAMOS_VERSION/os/x86_64/$file"
mkdir "$work/keyring"
tar -xf "$work/$file" -C "$work/keyring"
cp "$work"/keyring/usr/share/pacman/keyrings/* /usr/share/pacman/keyrings/
pacman-key --init >/dev/null
pacman-key --populate archlinux holo >/dev/null

# Only the SteamOS repositories of this release.
sed -i '/^\[core\]/,$d' /etc/pacman.conf
for repo in core extra; do
    # shellcheck disable=SC2016 # $arch is expanded by pacman, not the shell
    printf '\n[%s]\nServer = %s/%s-%s/os/$arch\n' "$repo" "$MIRROR" "$repo" "$STEAMOS_VERSION" >> /etc/pacman.conf
done
rm -rf "$work"

# -yy: the package lists of SteamOS are older than those of Arch already in the
# container, so they have to be downloaded by force; -uu: downgrade to them.
pacman -Syyuu --noconfirm
if [ "$#" -gt 0 ]; then
    pacman -S --noconfirm --needed "$@"
fi
echo "SteamOS $STEAMOS_VERSION package set:"
pacman -Q qt6-base libevdev systemd glibc 2>/dev/null || true
