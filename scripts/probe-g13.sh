#!/bin/sh
# SPDX-FileCopyrightText: 2026 Lordbooker
# SPDX-License-Identifier: GPL-2.0-only

# Collects what the kernel exposes for a Logitech G13 (hid-lg-g15), to verify
# the model file data/models/g13.json on real hardware.
#
# Usage:
#   scripts/probe-g13.sh            static information (no root needed)
#   scripts/probe-g13.sh --events   additionally records key events for 30 s
#                                   per input device (needs evtest and sudo)

VID=046D
PID=C21C

echo "== System"
echo "kernel: $(uname -r)"
if command -v systemctl >/dev/null 2>&1; then
    echo "linux-g13-driver service (g13): $(systemctl --user is-active g13 2>/dev/null || true)"
fi
echo "hid_lg_g15 module loaded: $(grep -qs '^hid_lg_g15 ' /proc/modules && echo yes || echo 'no (or built in)')"

found=0
for hid in /sys/bus/hid/devices/*:$VID:$PID.*; do
    [ -e "$hid" ] || continue
    found=1
    echo
    echo "== HID device $(basename "$hid")"
    if [ -e "$hid/driver" ]; then
        echo "driver: $(basename "$(readlink "$hid/driver")")"
    else
        echo "driver: none"
    fi

    for input in "$hid"/input/input*; do
        [ -e "$input" ] || continue
        echo
        echo "-- input: $(cat "$input/name")"
        for ev in "$input"/event*; do
            [ -e "$ev" ] || continue
            node="/dev/input/$(basename "$ev")"
            echo "node: $node"
            echo "access: $(ls -l "$node" | cut -d' ' -f1,3,4)"
            command -v getfacl >/dev/null 2>&1 && getfacl -p "$node" 2>/dev/null | grep '^user:' | sed 's/^/acl: /'
        done
        echo "capabilities/ev:  $(cat "$input/capabilities/ev")"
        echo "capabilities/key: $(cat "$input/capabilities/key")"
        echo "capabilities/abs: $(cat "$input/capabilities/abs")"
    done

    for raw in "$hid"/hidraw/hidraw*; do
        [ -e "$raw" ] || continue
        node="/dev/$(basename "$raw")"
        echo
        echo "-- hidraw: $node ($(ls -l "$node" | cut -d' ' -f1,3,4))"
    done

    for led in "$hid"/leds/*; do
        [ -e "$led" ] || continue
        echo
        echo "-- led: $(basename "$led")"
        for attr in brightness max_brightness multi_index multi_intensity brightness_hw_changed; do
            [ -e "$led/$attr" ] || continue
            echo "$attr = $(cat "$led/$attr" 2>/dev/null) ($(ls -l "$led/$attr" | cut -d' ' -f1,3,4))"
        done
    done
done

if [ "$found" = 0 ]; then
    echo
    echo "No HID device $VID:$PID found."
    for usb in /sys/bus/usb/devices/*; do
        [ -e "$usb/idProduct" ] || continue
        if [ "$(cat "$usb/idVendor")" = "046d" ] && [ "$(cat "$usb/idProduct")" = "c21c" ]; then
            echo "G13 is connected to USB ($(basename "$usb"))."
            for iface in "$usb"/*:1.*; do
                [ -e "$iface/driver" ] && echo "interface $(basename "$iface") bound to: $(basename "$(readlink "$iface/driver")")"
            done
            echo "If it is not bound to usbhid, another driver (e.g. linux-g13-driver) holds it: stop it and replug."
        fi
    done
fi

if [ "$1" = "--events" ]; then
    if ! command -v evtest >/dev/null 2>&1; then
        echo
        echo "evtest is not installed (Arch: pacman -S evtest)."
        exit 1
    fi
    for hid in /sys/bus/hid/devices/*:$VID:$PID.*; do
        for ev in "$hid"/input/input*/event*; do
            [ -e "$ev" ] || continue
            node="/dev/input/$(basename "$ev")"
            echo
            echo "== Events from $node ($(cat "$(dirname "$ev")/name")) for 30 seconds:"
            echo "   press every key once (G1-G22, L1-L4, round button, M1-M3, MR, light button,"
            echo "   both thumb buttons, stick press) and move the stick in all directions."
            # Prints every key/button once (in press order) and the range of each axis.
            sudo timeout 30 evtest "$node" | awk '
                /type 1 \(EV_KEY\)/ && / value 1$/ {
                    name = $9; sub(/\),?$/, "", name); sub(/^\(/, "", name)
                    if (!(name in seen)) { seen[name] = 1; print "key: " name " (" $8 ")" }
                }
                /type 3 \(EV_ABS\)/ {
                    name = $9; sub(/\),?$/, "", name); sub(/^\(/, "", name); v = $NF + 0
                    if (!(name in min) || v < min[name]) min[name] = v
                    if (!(name in max) || v > max[name]) max[name] = v
                }
                END { for (a in min) print "axis: " a " min=" min[a] " max=" max[a] }'
        done
    done
fi
