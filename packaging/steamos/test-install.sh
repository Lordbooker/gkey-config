#!/bin/sh
# SPDX-FileCopyrightText: 2026 Lordbooker
# SPDX-License-Identifier: GPL-2.0-only
#
# CI test of the SteamOS archive in a container prepared by setup-container.sh
# (as root): installs it as the user "deck" with sudo, as on a Steam Deck,
# checks the result and uninstalls it again.
#
#   packaging/steamos/test-install.sh gkey-config-<version>-steamos-x86_64.tar.gz
#
# Not covered (no device, udev or systemd session in a container): the kernel
# driver (install.sh skips its check here), device access and the daemon at
# work.
set -eu

ARCHIVE=$(realpath "$1")
DECK_HOME=/home/deck
RULE=/etc/udev/rules.d/70-gkey-config.rules
KEEP=/etc/atomic-update.conf.d/gkey-config.conf

fail() {
    echo "FAIL: $*" >&2
    exit 1
}
as_deck() {
    su deck -c "$1"
}

id deck >/dev/null 2>&1 || useradd -m deck
echo 'deck ALL=(ALL) NOPASSWD: ALL' > /etc/sudoers.d/deck
chmod 0440 /etc/sudoers.d/deck
install -d -o deck -g deck "$DECK_HOME/Downloads"
install -m 0644 -o deck -g deck "$ARCHIVE" "$DECK_HOME/Downloads/"

echo "== install"
as_deck "cd ~/Downloads && tar -xzf $(basename "$ARCHIVE") && cd gkey-config-*-steamos-x86_64 \
         && GKEY_SKIP_KERNEL_CHECK=1 ./install.sh"

echo "== check"
for f in .local/bin/gkeyd .local/bin/gkey-config .local/share/gkey-config/uninstall.sh \
         .local/share/gkey-config/models/g13.json .local/share/applications/io.github.lordbooker.GKeyConfig.desktop \
         .local/share/dbus-1/services/io.github.lordbooker.GKeyConfig.service \
         .local/share/icons/hicolor/scalable/apps/gkey-config.svg \
         .config/systemd/user/gkeyd.service .config/autostart/gkey-config-tray.desktop; do
    [ -e "$DECK_HOME/$f" ] || fail "missing: ~/$f"
done
# Every program path written by install.sh is absolute and executable.
grep -h -o '^Exec[A-Za-z]*=[^ ]*' "$DECK_HOME/.local/share/applications/io.github.lordbooker.GKeyConfig.desktop" \
    "$DECK_HOME/.config/autostart/gkey-config-tray.desktop" "$DECK_HOME/.config/systemd/user/gkeyd.service" \
    "$DECK_HOME/.local/share/dbus-1/services/io.github.lordbooker.GKeyConfig.service" |
    while read -r line; do
        program=${line#*=}
        case "$program" in
            "$DECK_HOME"/.local/bin/*) [ -x "$program" ] || fail "not executable: $line" ;;
            *) fail "not an installed absolute path: $line" ;;
        esac
    done
[ -f "$RULE" ] || fail "udev rule not installed"
grep -qx "$RULE" "$KEEP" || fail "udev rule not in the SteamOS keep list $KEEP"
id -nG deck | tr ' ' '\n' | grep -qx gkey || fail "deck is not in the group gkey"
models=$(as_deck "$DECK_HOME/.local/bin/gkeyd --list-models" | cut -f1 | tr '\n' ' ')
echo "models: $models"
[ "$models" = "g11 g13 g15 g15v2 g510 " ] || fail "gkeyd does not find the device models"
as_deck "QT_QPA_PLATFORM=offscreen $DECK_HOME/.local/bin/gkey-config --version" || fail "gkey-config does not start"

echo "== uninstall"
as_deck "$DECK_HOME/.local/share/gkey-config/uninstall.sh"
left=$(cd "$DECK_HOME" && find .local .config -type f ! -path '.config/gkey-config/*' 2>/dev/null || true)
[ -z "$left" ] || fail "files left after uninstall: $left"
[ ! -e "$RULE" ] && [ ! -e "$KEEP" ] || fail "udev rule or keep list left after uninstall"
! getent group gkey >/dev/null || fail "group gkey left after uninstall"
echo "SteamOS installation test passed"
