#!/bin/sh
# SPDX-FileCopyrightText: 2026 Lordbooker
# SPDX-License-Identifier: GPL-2.0-only

# Collects what the kernel exposes for the Logitech G devices driven by
# hid-lg-g15 (G11, G13, G15, G15 v2, G510), to verify the model files in
# data/models/ on real hardware and to attach to bug reports.
#
# Usage:
#   scripts/probe-device.sh            static information (no root needed)
#   scripts/probe-device.sh --events   additionally records key events for 30 s
#                                      per input device (needs evtest and sudo)

VID=046D
# Product IDs of the supported devices (see data/models/*.json).
PIDS="C21C C222 C225 C227 C22D C22E"

model_name() {
    case "$1" in
        C21C) echo "G13" ;;
        C222) echo "G15" ;;
        C225) echo "G11" ;;
        C227) echo "G15 v2" ;;
        C22D) echo "G510" ;;
        C22E) echo "G510 (headset connected)" ;;
        *) echo "unknown" ;;
    esac
}

# Mode, owner and group of a file.
access() {
    stat -c '%A %U %G' "$1" 2>/dev/null
}

probe_hid() {
    hid=$1
    echo
    echo "== $(model_name "$2"): HID device $(basename "$hid")"
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
            echo "access: $(access "$node")"
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
        echo "-- hidraw: $node ($(access "$node"))"
    done

    for led in "$hid"/leds/*; do
        [ -e "$led" ] || continue
        echo
        echo "-- led: $(basename "$led")"
        for attr in brightness max_brightness multi_index multi_intensity brightness_hw_changed; do
            [ -e "$led/$attr" ] || continue
            echo "$attr = $(cat "$led/$attr" 2>/dev/null) ($(access "$led/$attr"))"
        done
    done
}

record_events() {
    hid=$1
    for ev in "$hid"/input/input*/event*; do
        [ -e "$ev" ] || continue
        node="/dev/input/$(basename "$ev")"
        echo
        echo "== Events from $node ($(cat "$(dirname "$ev")/name")) for 30 seconds:"
        echo "   press every key of the device once (G keys, LCD keys, M1-M3, MR, ...);"
        echo "   on a G13 also the thumb buttons, the stick press and the stick in all directions."
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
}

echo "== System"
echo "kernel: $(uname -r)"
if command -v systemctl >/dev/null 2>&1; then
    echo "gkeyd service: $(systemctl --user is-active gkeyd 2>/dev/null || true)"
    echo "linux-g13-driver service (g13): $(systemctl --user is-active g13 2>/dev/null || true)"
fi
echo "hid_lg_g15 module loaded: $(grep -qs '^hid_lg_g15 ' /proc/modules && echo yes || echo 'no (or built in)')"

found=0
for pid in $PIDS; do
    for hid in /sys/bus/hid/devices/*:"$VID":"$pid".*; do
        [ -e "$hid" ] || continue
        found=1
        probe_hid "$hid" "$pid"
    done
done

if [ "$found" = 0 ]; then
    echo
    echo "No supported HID device found."
    for usb in /sys/bus/usb/devices/*; do
        [ -e "$usb/idProduct" ] || continue
        [ "$(cat "$usb/idVendor")" = "046d" ] || continue
        product=$(tr '[:lower:]' '[:upper:]' < "$usb/idProduct")
        case " $PIDS " in *" $product "*) ;; *) continue ;; esac
        echo "$(model_name "$product") is connected to USB ($(basename "$usb"))."
        for iface in "$usb"/*:1.*; do
            [ -e "$iface/driver" ] && echo "interface $(basename "$iface") bound to: $(basename "$(readlink "$iface/driver")")"
        done
        echo "If it is not bound to usbhid, another driver (e.g. linux-g13-driver) holds it: stop it and replug."
    done
fi

if [ "$1" = "--events" ]; then
    if ! command -v evtest >/dev/null 2>&1; then
        echo
        echo "evtest is not installed (Arch: pacman -S evtest)."
        exit 1
    fi
    for pid in $PIDS; do
        for hid in /sys/bus/hid/devices/*:"$VID":"$pid".*; do
            [ -e "$hid" ] || continue
            record_events "$hid"
        done
    done
fi
