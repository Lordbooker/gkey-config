#!/bin/sh
# SPDX-FileCopyrightText: 2026 Lordbooker
# SPDX-License-Identifier: GPL-2.0-only
#
# Installs gkey-config on SteamOS (Steam Deck, desktop mode, Konsole), as the
# normal user, from the unpacked archive:
#
#   ./install.sh
#
# SteamOS keeps /usr read-only and replaces it with every update, so
# everything goes into the home directory, which updates do not touch:
#   ~/.local/bin, ~/.local/share  programs, device models, menu entry, icon
#   ~/.config/systemd/user        daemon gkeyd as a user service
#   ~/.config/autostart           tray icon at login
# Only the device access needs root (sudo, asks for the password of "deck"):
#   /etc/udev/rules.d/70-gkey-config.rules   access for the logged-in user
#   /etc/atomic-update.conf.d/gkey-config.conf  keeps the rule over updates
#   group "gkey"                              LED control (kept over updates)
# Running it again updates an installation. Profiles and macros stay in
# ~/.config/gkey-config/.
#
# For tests: GKEY_PREFIX (default ~/.local), GKEY_SYSROOT (prefix for /etc),
# GKEY_SKIP_KERNEL_CHECK=1 (no check for the kernel driver).
set -eu

SOURCE=$(cd "$(dirname "$0")" && pwd)
PREFIX=${GKEY_PREFIX:-"$HOME/.local"}
CONFIG=${XDG_CONFIG_HOME:-"$HOME/.config"}
SYSROOT=${GKEY_SYSROOT:-}
RULE=/etc/udev/rules.d/70-gkey-config.rules
KEEP=/etc/atomic-update.conf.d/gkey-config.conf

fail() {
    echo "gkey-config: $*" >&2
    exit 1
}
warn() {
    echo "gkey-config: warning: $*" >&2
}

[ "$(id -u)" -ne 0 ] || fail "run this as your normal user (e.g. deck), not as root; it asks for the password itself"
[ "$(uname -m)" = x86_64 ] || fail "this archive is for x86_64 only"
[ -x "$SOURCE/bin/gkeyd" ] || fail "run install.sh from the unpacked archive"

# The kernel driver hid-lg-g15 drives the devices; gkey-config builds on it.
if [ "${GKEY_SKIP_KERNEL_CHECK:-}" != 1 ] && ! modinfo hid-lg-g15 >/dev/null 2>&1; then
    fail "the kernel driver hid-lg-g15 is missing in this SteamOS version (modinfo hid-lg-g15)"
fi

# The programs use the libraries of SteamOS (Qt 6, libevdev, libudev).
missing=$(ldd "$SOURCE/bin/gkeyd" "$SOURCE/bin/gkey-config" | awk '/not found/ {print $1}' | sort -u)
[ -z "$missing" ] || fail "libraries missing in this SteamOS version: $(echo "$missing" | tr '\n' ' ')"

echo "Installing gkey-config $(cat "$SOURCE/VERSION") into $PREFIX"

# User part: programs, data, menu entry, services.
install -d "$PREFIX/bin" "$PREFIX/share/gkey-config" "$PREFIX/share/applications" "$PREFIX/share/metainfo" \
    "$PREFIX/share/dbus-1/services" "$PREFIX/share/icons/hicolor/scalable/apps" \
    "$CONFIG/systemd/user" "$CONFIG/autostart"
install -m 0755 "$SOURCE/bin/gkeyd" "$SOURCE/bin/gkey-config" "$PREFIX/bin/"
rm -rf "$PREFIX/share/gkey-config/models"
cp -r "$SOURCE/share/gkey-config/models" "$PREFIX/share/gkey-config/"
install -m 0755 "$SOURCE/uninstall.sh" "$PREFIX/share/gkey-config/uninstall.sh"
install -m 0644 "$SOURCE/VERSION" "$PREFIX/share/gkey-config/VERSION"
install -m 0644 "$SOURCE/share/icons/hicolor/scalable/apps/gkey-config.svg" "$PREFIX/share/icons/hicolor/scalable/apps/"
install -m 0644 "$SOURCE"/share/metainfo/*.xml "$PREFIX/share/metainfo/"

# The desktop session does not necessarily have ~/.local/bin in PATH: absolute paths.
sed "s#^Exec=gkey-config#Exec=$PREFIX/bin/gkey-config#" \
    "$SOURCE/share/applications/io.github.lordbooker.GKeyConfig.desktop" \
    > "$PREFIX/share/applications/io.github.lordbooker.GKeyConfig.desktop"
sed "s#^Exec=gkey-config#Exec=$PREFIX/bin/gkey-config#" "$SOURCE/system/gkey-config-tray.desktop" \
    > "$CONFIG/autostart/gkey-config-tray.desktop"
sed "s#^ExecStart=.*#ExecStart=$PREFIX/bin/gkeyd#" "$SOURCE/system/gkeyd.service" \
    > "$CONFIG/systemd/user/gkeyd.service"
sed "s#^Exec=.*#Exec=$PREFIX/bin/gkeyd#" "$SOURCE/system/io.github.lordbooker.GKeyConfig.service" \
    > "$PREFIX/share/dbus-1/services/io.github.lordbooker.GKeyConfig.service"

# System part: device access. SteamOS only keeps changes in /etc over updates
# that are listed in /etc/atomic-update.conf.d; /etc/group is always kept.
echo "Device access (udev rule, group \"gkey\") needs root: sudo asks for the password of $(id -un)."
if ! sudo -v; then
    fail "sudo failed. On a Steam Deck, set a password for the user first with: passwd"
fi
sudo sh -eu -c '
    sysroot=$1 source=$2 rule=$3 keep=$4 user=$5
    install -D -m 0644 "$source/system/70-gkey-config.rules" "$sysroot$rule"
    install -d "$(dirname "$sysroot$keep")"
    printf "%s\n" "# gkey-config: keep the udev rule over SteamOS updates" "$rule" > "$sysroot$keep"
    chmod 0644 "$sysroot$keep"
    getent group gkey >/dev/null || groupadd --system gkey
    usermod -aG gkey "$user"
    if command -v udevadm >/dev/null && udevadm control --reload 2>/dev/null; then
        udevadm trigger --subsystem-match=input --subsystem-match=hidraw --subsystem-match=misc \
            --subsystem-match=leds || true
    else
        echo "gkey-config: warning: udev did not reload the rules; replug the device or restart" >&2
    fi
' gkey-config "$SYSROOT" "$SOURCE" "$RULE" "$KEEP" "$(id -un)"

# Start the daemon now and at every login.
if systemctl --user daemon-reload 2>/dev/null && systemctl --user enable gkeyd.service 2>/dev/null; then
    systemctl --user restart gkeyd.service 2>/dev/null || warn "gkeyd did not start; see: journalctl --user -u gkeyd"
else
    warn "no systemd user session; start the daemon later with: systemctl --user enable --now gkeyd"
fi

cat <<TEXT

gkey-config is installed. Open "G-Key Configuration" from the application menu
(desktop mode). Log out and back in (or restart) once, so that LED control
through the group "gkey" takes effect.

The daemon also runs in game mode; whether games there receive the keys has
not been tested on a Steam Deck yet - reports are welcome:
https://github.com/Lordbooker/gkey-config/issues
To remove gkey-config: $PREFIX/share/gkey-config/uninstall.sh
TEXT
