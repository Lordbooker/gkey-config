#!/bin/sh
# SPDX-FileCopyrightText: 2026 Lordbooker
# SPDX-License-Identifier: GPL-2.0-only
#
# Removes gkey-config installed by install.sh on SteamOS (as the normal user):
#
#   ~/.local/share/gkey-config/uninstall.sh
#
# Profiles and macros in ~/.config/gkey-config/ stay. The same test variables
# as in install.sh apply (GKEY_PREFIX, GKEY_SYSROOT).
set -eu

PREFIX=${GKEY_PREFIX:-"$HOME/.local"}
CONFIG=${XDG_CONFIG_HOME:-"$HOME/.config"}
SYSROOT=${GKEY_SYSROOT:-}
RULE=/etc/udev/rules.d/70-gkey-config.rules
KEEP=/etc/atomic-update.conf.d/gkey-config.conf

[ "$(id -u)" -ne 0 ] || { echo "gkey-config: run this as your normal user, not as root" >&2; exit 1; }

systemctl --user disable --now gkeyd.service 2>/dev/null || true

rm -f "$PREFIX/bin/gkeyd" "$PREFIX/bin/gkey-config" \
    "$PREFIX/share/applications/io.github.lordbooker.GKeyConfig.desktop" \
    "$PREFIX/share/metainfo/io.github.lordbooker.GKeyConfig.metainfo.xml" \
    "$PREFIX/share/dbus-1/services/io.github.lordbooker.GKeyConfig.service" \
    "$PREFIX/share/icons/hicolor/scalable/apps/gkey-config.svg" \
    "$CONFIG/systemd/user/gkeyd.service" \
    "$CONFIG/autostart/gkey-config-tray.desktop"
rm -rf "$PREFIX/share/gkey-config"
systemctl --user daemon-reload 2>/dev/null || true

echo "Removing the device access (udev rule, group \"gkey\") needs root: sudo asks for the password."
sudo sh -eu -c '
    sysroot=$1 rule=$2 keep=$3
    rm -f "$sysroot$rule" "$sysroot$keep"
    ! getent group gkey >/dev/null || groupdel gkey
    if command -v udevadm >/dev/null; then
        udevadm control --reload 2>/dev/null || true
    fi
' gkey-config "$SYSROOT" "$RULE" "$KEEP"

echo "gkey-config is removed. Profiles and macros are still in $CONFIG/gkey-config/."
