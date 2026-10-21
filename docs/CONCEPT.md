# gkey-config – Concept

Status: **approved on 2026-10-21.** Milestones 1–3 done and tested on the G13 (2026-10-21); milestone 4 (GUI) implemented, test pending.

gkey-config configures the gaming keys of Logitech G keyboards and keypads that are driven by the
Linux kernel driver `hid-lg-g15`. It does **not** talk to the USB device itself and does not depend on
input-remapper, keyd or similar tools.

Facts marked *(kernel)* were taken from `drivers/hid/hid-lg-g15.c` and `drivers/hid/hid-ids.h`
(torvalds/linux, `master`, read on 2026-10-21). Facts marked *(verify)* still have to be checked on
real hardware with `evtest` before they are relied on.

---

## 1. Scope

### 1.1 Supported devices

| Model | USB ID *(kernel)* | Kernel model | G-keys | M1–M3, MR | LCD keys | Stick | Backlight LED | LCD |
|---|---|---|---|---|---|---|---|---|
| G11 | 046d:c225 | treated as G15 | 18 | yes | codes registered by the kernel; physical keys *(verify)* | – | `g15::kbd_backlight` (0–2) | – |
| G13 | 046d:c21c | G13 | 22 | yes | 4 + round "next page" button | yes | `g13:rgb:kbd_backlight` (RGB) | 160×43, hidraw |
| G15 (v1) | 046d:c222 | G15 | 18 | yes | 4 + round button | – | `g15::kbd_backlight` (0–2) | later |
| G15 v2 | 046d:c227 | G15 v2 | 6 | yes | 4 + round button | – | `g15::kbd_backlight` (0–2) | later |
| G510 | 046d:c22d | G510 | 18 | yes | 5 | – | `g15::kbd_backlight` (RGB) | later |
| G510 + USB audio | 046d:c22e | G510 | 18 | yes | 5 | – | `g15::kbd_backlight` (RGB) | later |

The Logitech Z-10 speakers are also handled by `hid-lg-g15`, but are out of scope.

### 1.2 Minimum kernel

**Linux 6.19** for the whole project (first release with G13 support in `hid-lg-g15`). G11, G15,
G15 v2 and G510 are supported by older kernels as well, but packages are only built for
distribution releases that ship 6.19 or newer *(decided)*.

The daemon checks at runtime which models are actually usable and reports missing support instead
of failing silently (see 9.2).

### 1.3 Target platforms

Arch Linux, Debian-based (Debian, Ubuntu and derivatives) and Fedora-based distributions, each only
in releases with kernel ≥ 6.19 *(decided)*; the exact release list is fixed when packaging starts
(milestone 8). KDE Plasma (Wayland) and GNOME (Wayland). X11 is not a target but is expected to
work, because nothing below the GUI depends on the display server.

### 1.3a Test hardware

Only a **G13** is available for testing *(decided)*. The model files for G11, G15, G15 v2 and G510
are derived from the kernel source only and are marked **untested** in the GUI and documentation
until someone with the device confirms them.

**G13 verified on hardware on 2026-10-21** (Arch Linux, kernel 7.2.11, `scripts/probe-g13.sh` and
`evtest`):

* Keypad: all event codes of section 2.2 confirmed (G1–G22, L1–L4, round button, M1–M3, MR,
  light button).
* Stick: `BTN_THUMB`, `BTN_BASE`, `BTN_BASE2` confirmed; `ABS_X` and `ABS_Y` reach the full range
  0–255. The **rest position is not 128**: it was measured at X ≈ 137–139, Y ≈ 138–140 with a jitter
  of about ±3 (see 7.1).
* LEDs: names, types and ranges as in section 2.3; power-on backlight was `multi_intensity` 0 255 0.
* Permissions without our udev rule: keypad and hidraw root/`input` only; the stick node already
  carries an ACL for the session user (set by the system's joystick rules); LEDs root-only.

### 1.4 Version 1 includes

Keys, M1–M3 modes, MR, LEDs, profiles, macros, G13 stick modes, G13 LCD, per-application profiles,
GUI, packages. LCD support for G15/G510 is a later stage (protocol not documented yet).

---

## 2. What the kernel provides *(kernel)*

### 2.1 Input devices

`hid-lg-g15` creates **separate input devices for the gaming keys**. The normal keys of G11/G15/G510
come from a different USB interface (generic HID keyboard) and are never touched by gkey-config.

| Model | Input device name | Notes |
|---|---|---|
| G13 | `Logitech G13 Gaming Keypad` | keys |
| G13 | `Logitech G13 Thumbstick` | `ABS_X`, `ABS_Y` 0–255, `BTN_BASE`, `BTN_BASE2`, `BTN_THUMB` |
| G11, G15, G15 v2, G510 | `Logitech Gaming Keyboard Gaming Keys` | keys |

On the G510 the driver also connects generic hid-input for its interface, so further input devices
may exist on the same HID device *(verify)*; gkey-config only grabs the "Gaming Keys" device.

### 2.2 Event codes

| Control | Code |
|---|---|
| G1 … Gn | `KEY_MACRO1` … `KEY_MACROn` |
| M1, M2, M3 | `KEY_MACRO_PRESET1` … `KEY_MACRO_PRESET3` |
| MR | `KEY_MACRO_RECORD_START` |
| G13: LCD keys left → right | `KEY_KBD_LCD_MENU1` … `MENU4` |
| G13: round "next page" button (BD) | `KEY_KBD_LCD_MENU5` |
| G15, G15 v2: round button left of the LCD | `KEY_KBD_LCD_MENU1` |
| G15, G15 v2: 4 keys below the LCD | `KEY_KBD_LCD_MENU2` … `MENU5` |
| G510: LCD keys | `KEY_KBD_LCD_MENU1` … `MENU5` (physical order *(verify)*) |
| G13: backlight button | `KEY_LIGHTS_TOGGLE` (hardware toggles the light itself) |
| G13 stick: left button, bottom button, stick press | `BTN_BASE`, `BTN_BASE2`, `BTN_THUMB` |
| G510: headphone mute, mic mute | `KEY_MUTE`, `KEY_F20` (only without USB audio) |

The same `KEY_KBD_LCD_MENUx` code means different physical keys on different models. This is the
main reason for the per-model description files (section 4).

### 2.3 LEDs (`/sys/class/leds/`)

| Model | Names | Type |
|---|---|---|
| G13 | `g13:rgb:kbd_backlight` | multicolor (`multi_intensity`, `brightness` 0–255) |
| G13 | `g13:red:macro_preset_1..3`, `g13:red:macro_record` | 0/1 |
| G11, G15, G15 v2 | `g15::kbd_backlight`, `g15::lcd_backlight` | 0–2 |
| G510 | `g15::kbd_backlight` | multicolor; plus `g15::power_on_backlight_val` |
| G11, G15, G15 v2, G510 | `g15::macro_preset1..3`, `g15::macro_record` | 0/1 |

With two devices of the same family the LED class core renames the second set (suffix). LEDs are
therefore never addressed by fixed path but found below the device's HID node in sysfs.

The G13 keeps no state across power cycles: the daemon reapplies all LED states on every connect.

### 2.4 G13 LCD

Written through the device's `hidraw` node as output report 3 (160×43, monochrome). The frame format
is documented and was validated on hardware by the g13d project (MIT, see section 12); it is verified
again on our own G13 before use.

---

## 3. Architecture

```
                      Kernel (hid-lg-g15)
  /dev/input/eventX  /dev/input/eventY   /sys/class/leds/…   /dev/hidrawN (G13 LCD)
          │ EVIOCGRAB        │ EVIOCGRAB          ▲                    ▲
          ▼                  ▼                    │                    │
  ┌──────────────────────────────────────────────────────────────────────────┐
  │ gkeyd  (systemd --user service, Qt6 Core + DBus, libevdev, libudev)      │
  │  DeviceManager ─ Device (per connected device)                           │
  │     ├ InputReader   → BindingEngine → ActionRunner → VirtualInput(uinput)│
  │     ├ LedController                                                       │
  │     ├ StickProcessor (G13)                                                │
  │     └ LcdController  (G13)                                                │
  │  ProfileStore (files, live reload)   FocusTracker   DBusService           │
  └──────────────────────────────────────────────────────────────────────────┘
          ▲ D-Bus (session bus)                    ▲ D-Bus
          │                                        │
  gkey-config (Qt6 Widgets GUI, tray)      KWin script / GNOME Shell extension
                                           (report the focused application)
```

### 3.1 Components

| Component | Binary / file | Content |
|---|---|---|
| Daemon | `gkeyd` | device handling, bindings, macros, LEDs, LCD, D-Bus API |
| GUI | `gkey-config` | device image, profile and macro editing, tray icon |
| Model files | `/usr/share/gkey-config/models/*.json` | per-model description (section 4) |
| KWin script | `/usr/share/kwin/scripts/gkey-config/` | focus reporting on KDE Plasma |
| GNOME Shell extension | `/usr/share/gnome-shell/extensions/…` | focus reporting on GNOME |
| udev rule | `/usr/lib/udev/rules.d/70-gkey-config.rules` | permissions (section 9) |
| systemd user unit | `/usr/lib/systemd/user/gkeyd.service` | daemon service |

A command line tool (`gkeyctl`) is not part of version 1; the D-Bus API can be used with `busctl`
or `qdbus` in the meantime.

### 3.2 Technology

| Layer | Choice |
|---|---|
| Language / build | C++23, CMake ≥ 3.20, CPack (deb/rpm), AUR `PKGBUILD` |
| Input / output | libevdev (read, grab, `libevdev_uinput`) |
| Device discovery, hotplug | libudev |
| LEDs | sysfs, plain file access |
| LCD | hidraw, plain file access; rendering into an own 1-bit buffer |
| IPC | QtDBus on the session bus |
| GUI | Qt6 Widgets, Qt Linguist translations (en source; de, fr, it, es, pt) |
| License | GPL-2.0-only |

The daemon links Qt6 Core and DBus only, no GUI modules.

### 3.3 Threads

The daemon is single-threaded: one Qt event loop handles input, LEDs, D-Bus, file watching and
macro playback. Macros are driven by timers instead of threads, so a wait can be cancelled at once
and all uinput writes happen on one thread without locking. *(Changed during milestone 3; the
draft planned one thread per macro.)*

---

## 4. Device model files

Each supported model is described by a data file; adding a model needs no code change as long as
the kernel uses the known event codes.

```json
{
  "id": "g13",
  "name": "Logitech G13",
  "usb": [{ "vendor": "046d", "product": "c21c" }],
  "minKernel": "6.19",
  "inputs": {
    "keys":  "Logitech G13 Gaming Keypad",
    "stick": "Logitech G13 Thumbstick"
  },
  "keys": [
    { "id": "G1",  "code": "KEY_MACRO1" },
    { "id": "L1",  "code": "KEY_KBD_LCD_MENU1" },
    { "id": "BD",  "code": "KEY_KBD_LCD_MENU5" },
    { "id": "M1",  "code": "KEY_MACRO_PRESET1", "role": "mode1" },
    { "id": "MR",  "code": "KEY_MACRO_RECORD_START", "role": "record" },
    { "id": "STICK_PRESS", "code": "BTN_THUMB", "input": "stick" }
  ],
  "leds": {
    "backlight": { "suffix": "kbd_backlight", "type": "rgb" },
    "mode1": { "suffix": "macro_preset_1" }
  },
  "lcd": { "type": "g13-hidraw", "width": 160, "height": 43 },
  "image": "g13.png",
  "areas": [
    { "key": "G1", "shape": "rect", "x": 0, "y": 0, "w": 0, "h": 0 }
  ]
}
```

(Excerpt; coordinates are placeholders.)

* `keys` maps kernel event codes to stable **key IDs** used in profiles (`G1`, `L1`, `BD`, `M1`,
  `STICK_PRESS`, …). Key IDs are the physical labels, not bit positions.
* `role` marks keys with a fixed function (mode keys, record key). Everything else is bindable.
* `image` and `areas` describe the clickable device picture in the GUI.
* `KEY_LIGHTS_TOGGLE` (G13) is not listed as bindable: the hardware switches the light itself.

---

## 5. Profiles

### 5.1 Model

```
Profile (one file per profile and model)
├── name, linked applications (optional)
├── backlight colour or level
├── stick settings (G13)
├── M1: { key ID → action, … }
├── M2: { … }
└── M3: { … }
```

* **M1–M3 are modes inside a profile.** Pressing M1/M2/M3 switches the mode; the M LEDs show the
  active mode. The current profile and mode are remembered across restarts.
* **MR** starts/stops macro recording in the GUI (section 6.3).
* All other keys (G keys, LCD keys, G13 stick buttons and stick directions) are bindable per mode.
* Keys without a binding in the active mode do nothing. Exception: on the G510 `KEY_MUTE` and
  `KEY_F20` are passed through unchanged unless they are bound, so the mute keys keep working.
* Profiles are switched from the GUI, the tray, by a key action, via D-Bus, or automatically by the
  focused application (section 8).

### 5.2 Actions

| Action | Effect |
|---|---|
| `key` | one key (evdev code), press and release follow the G key |
| `combo` | several keys pressed together (e.g. Ctrl+C) |
| `mouse` | mouse button |
| `macro` | macro from the library, with repeat mode |
| `mode` | switch to M1/M2/M3 |
| `profile` | switch to a profile (by name, or next/previous) |
| `lcd` | LCD page next/previous (G13) |
| `none` | unbound |

Actions store **evdev key codes**, not characters. The compositor applies the keyboard layout as
usual, so a binding behaves like the same physical key on the normal keyboard. The GUI shows the
label of the user's own layout.

### 5.3 Storage

```
~/.config/gkey-config/
├── settings.json              global settings
├── profiles/<model>/<name>.json
└── macros/<name>.json         shared by all models
```

The daemon watches the directory (inotify via `QFileSystemWatcher`) and reloads changed profiles
and macros immediately; files are written atomically (write to temp file, rename).

Runtime state that is not configuration (active profile and mode per model) lives in
`$XDG_STATE_HOME/gkey-config/state.json` (default `~/.local/state/gkey-config/state.json`).

### 5.3a Profile file format

```json
{
  "name": "Default",
  "backlight": [51, 153, 255],
  "modes": {
    "M1": {
      "G1":  { "type": "key",   "key": "KEY_1" },
      "G2":  { "type": "combo", "keys": ["KEY_LEFTCTRL", "KEY_C"] },
      "G3":  { "type": "mouse", "button": "BTN_LEFT" },
      "G4":  { "type": "macro", "macro": "hello", "repeat": "once" },
      "L1":  { "type": "mode",  "mode": 2 },
      "L2":  { "type": "profile", "profile": "next" },
      "BD":  { "type": "lcd",   "page": "next" }
    },
    "M2": {},
    "M3": {}
  },
  "stick": {}
}
```

* `name` identifies the profile; the file name is only derived from it when the profile is created.
* Key IDs are those of the model file; bindings for keys the model cannot bind are ignored with a
  warning in the log.
* `key` and `combo` accept keyboard keys only, `mouse` only `BTN_LEFT` … `BTN_TASK`. Key names are
  evdev names and mean physical key positions; the keyboard layout is applied by the compositor.
* `repeat` (macros): `once`, `while_held`, `toggle`, `count` (with `"count": n`).
* `profile`: a profile name, `next` or `previous`.
* An invalid binding only disables that binding, not the profile; an invalid file is skipped.
* Members the daemon does not know are kept when the file is written again.

### 5.4 Import from linux-g13-driver

The GUI offers a one-time import of `~/.config/g13/bindings-0..3.properties` and
`macro-*.properties` into G13 profiles (L1–L4 profile → one profile each, bindings → mode M1).
The old file keys `G0`…`G39` are bit positions and are translated with the table from the
linux-g13-driver README.

---

## 6. Macros

### 6.1 Format

One file per macro in the shared library (`~/.config/gkey-config/macros/*.json`), referenced by
its `name`:

```json
{
  "name": "copy-line",
  "description": "Select the current line and copy it",
  "steps": [
    { "tap": "KEY_HOME" },
    { "press": "KEY_LEFTSHIFT" },
    { "tap": "KEY_END", "hold": 20 },
    { "release": "KEY_LEFTSHIFT" },
    { "delay": 30 },
    { "press": "KEY_LEFTCTRL" }, { "tap": "KEY_C" }, { "release": "KEY_LEFTCTRL" }
  ]
}
```

* Steps: `press`, `release`, `tap` (press, wait `hold` ms – default 10 –, release), `delay` (ms,
  at most 60000). Keyboard keys and mouse buttons are allowed.
* The repeat mode belongs to the binding, not to the macro, so one macro can be bound in different
  ways: `once`, `while_held` (the current pass completes), `toggle`, `count` (n times).
* A new press of the key stops a running macro.

### 6.2 Playback

Playback is timer-driven (see 3.3). Keys still held when a macro ends or is stopped are released.
Switching profile or mode stops running macros and releases all keys. A repeating macro without any
wait gets a 10 ms pause between passes, so it cannot flood the output or block the daemon.

### 6.3 Recording

Recording happens in the GUI window (as in linux-g13-driver), because only the GUI sees the normal
keyboard; the daemon only reads the gaming-keys devices. MR on the device starts/stops recording
when the GUI is open (daemon emits a D-Bus signal) and toggles the MR LED. Until the GUI exists
(milestone 4) MR only toggles the recording state and the LED.

---

## 7. G13 stick and LCD

### 7.1 Stick modes (per profile)

| Mode | Behaviour |
|---|---|
| `keys` | four directions as bindable keys `STICK_UP/DOWN/LEFT/RIGHT`, adjustable dead zone (default WASD, same zone logic as linux-g13-driver) |
| `mouse` | relative pointer movement, speed and dead zone |
| `scroll` | wheel |
| `passthrough` | stick device is not grabbed; games see the joystick |

The stick buttons (`BTN_BASE`, `BTN_BASE2`, `BTN_THUMB`) are always bindable keys.

The dead zone is centred on the stick's **rest position**, not on 128 (the measured rest position of
the test G13 is about 138/139). Default: the position read when the device is connected, if it is
plausible (within ±24 of 128), otherwise 128; a calibration in the GUI follows with milestone 5.

### 7.2 LCD (version 1)

* Pages: profile/mode status, clock, free text (set via D-Bus).
* The round button (BD) and/or the LCD keys can be bound to `lcd` actions.
* Further pages (system load, media) are later additions.
* G15/G510 LCD: after the protocol has been analysed on hardware.

---

## 8. Per-application profiles

Wayland gives no generic way to read the focused window, so desktop-specific helpers report it to
the daemon:

| Desktop | Helper |
|---|---|
| KDE Plasma | KWin script, reports activation of a window via D-Bus |
| GNOME | GNOME Shell extension, reports focus changes via D-Bus |

Both call the same daemon method `ReportFocus(appId, windowClass, title)`. A profile can list
matching rules (`app id`, window class, `*` wildcards). When no rule matches, the profile chosen
manually stays active. A profile can be locked, which disables automatic switching. Without a
helper, automatic switching is simply unavailable; everything else works.

The exact KWin scripting API (Plasma 6) and GNOME Shell extension API are checked when this part is
implemented *(verify)*.

---

## 9. Permissions and coexistence

### 9.1 udev rule

| Node | Access |
|---|---|
| input event nodes of supported devices | `TAG+="uaccess"` |
| hidraw node of the G13 (LCD) | `TAG+="uaccess"` |
| `/dev/uinput` | `TAG+="uaccess"` |
| LED attributes (`brightness`, `multi_intensity`) of supported devices | group `gkey`, mode 0664 (see 9.1a) |

`uaccess` gives access to the user of the active local session only (systemd-logind), the same
approach as linux-g13-driver.

### 9.1a LED permissions *(decided)*

sysfs attributes cannot carry ACLs, so `uaccess` does not work for the LEDs. Instead:

* The packages create a system group **`gkey`** via `sysusers.d`.
* The udev rule sets group `gkey` and mode `0664` on `brightness` and `multi_intensity` of the LEDs
  that belong to a supported device (matched below the device's HID node, not by name alone).
* Users who want LED control are added once by the administrator:
  `sudo usermod -aG gkey $USER`, then log out and back in. Packages never add users on their own.
* Existing groups such as `input` are **not** used: membership there allows reading every keyboard.
* Without group membership everything except the LEDs keeps working. The daemon detects the
  missing permission and reports it (log, D-Bus `Status`, GUI hint with the command above).

Rejected alternatives: world-writable attributes (any local user could change the LEDs; contradicts
the "no world-writable nodes" rule from linux-g13-driver), a privileged helper service (root
component and polkit policy only for LEDs), writing LEDs through hidraw (bypasses the kernel's LED
state, G13 only).

### 9.2 Diagnostics and coexistence

The daemon reports (log, D-Bus status, GUI) why a connected device is not usable:

* device present on USB but not bound to `lg-g15` (e.g. linux-g13-driver or another libusb driver
  has detached the kernel driver, or the kernel is too old for this model),
* input device, hidraw node or LED attributes not accessible (udev rule missing),
* `/dev/uinput` not accessible.

linux-g13-driver and gkey-config cannot drive the same G13 at the same time; the packages declare a
**package conflict** with `linux-g13-driver` (and the AUR `linux-g13-driver-git`) *(decided)*.

### 9.3 Virtual device

One uinput device named `gkey-config virtual input` with its **own** vendor/product ID (not
Logitech's), so it is never mistaken for a real device by our own discovery or by udev rules. It
offers all keyboard keys, mouse buttons, `REL_X`, `REL_Y`, `REL_WHEEL`, `REL_HWHEEL`.

---

## 10. D-Bus API

Service `io.github.lordbooker.GKeyConfig`, object `/io/github/lordbooker/GKeyConfig`, interface
`io.github.lordbooker.GKeyConfig1`. Owning the service name also guarantees a single daemon per
session; the systemd unit is `Type=dbus`.

| Kind | Signature | Notes |
|---|---|---|
| Method | `Status() → s` | JSON: `version`, `recording`, `devices[]` (`model`, `syspath`, `ledPermission`), `models{id: {name, tested, profile, mode, profiles[], profileDir}}`, `macros[]`, `macroDir` |
| Method | `SetProfile(s model, s name) → b` | |
| Method | `SetMode(s model, i mode) → b` | 1–3 |
| Method | `SetBacklight(s model, i r, i g, i b) → b` | temporary, until the next profile change |
| Method | `ResetBacklight(s model) → b` | back to the profile colour |
| Method | `Reload()` | re-reads profiles and macros (normally automatic) |
| Method | `RunMacro(s name) → b` / `StopMacros()` | |
| Method | `SetRecording(b)` | MR LED and `RecordingChanged` |
| Property | `Recording` (b) | |
| Signals | `DeviceAdded(s model, s syspath)`, `DeviceRemoved(s, s)`, `ProfileChanged(s model, s name)`, `ModeChanged(s model, i mode)`, `KeyEvent(s model, s key, b pressed)`, `RecordingChanged(b)` | |

`KeyEvent` lets the GUI highlight pressed keys and lets the user pick a key by pressing it.
Planned for later milestones: `LcdText`, `LcdPage` (milestone 5), `ReportFocus`, `LockProfile`
(milestone 6).

---

## 11. GUI

Qt6 Widgets application `gkey-config` (implemented in milestone 4):

* Device selector (all known models; untested ones are marked).
* Schematic drawing of the device from the model file's `view` section – drawn with QPainter in
  light and dark themes, no bitmap. Bound keys carry a dot in the profile's backlight colour, the
  selected key is outlined, keys pressed on the device light up live (D-Bus `KeyEvent`); pressing
  a key on the device also selects it while the window is active.
* Profile selection (activates the profile on the device) and profile management (new, duplicate,
  rename, delete); layer buttons M1–M3 (switch the device as well and choose the edited layer).
* Binding editor for the selected key: not bound, key, key combination (captured by pressing the
  keys together), mouse button, macro (with repeat mode), layer, profile, LCD page. Every change is
  saved at once; the daemon reloads the file.
* Backlight tab: colour per profile with live preview on the device while choosing.
* Macro tab: library (new, delete), step list with in-place editing of times, insertion of
  press/release/tap/wait steps, moving and removing steps, "set all waits", recording with timing
  (also started/stopped with MR on the device), playback on the device.
* Status bar and tray icon show whether the daemon runs; it can be started, stopped and
  restarted (`systemctl --user`). Single instance; closing the window keeps the tray icon.
* Translations via Qt Linguist: English (source) and German.
* `gkey-config --screenshot file [--show-key G4 --show-tab 0]` renders the window to an image
  (documentation).

The stick keys (thumb buttons, stick directions) can already be bound in the GUI; the daemon uses
these bindings once the stick is handled (milestone 5).

---

## 12. Reused work

| Source | What | How |
|---|---|---|
| linux-g13-driver (GPL-2.0-only, own project) | macro engine, actions, stick zone logic, GUI parts (binding editor, colour selector, macro recorder, key capture, key names, tray, single instance, translations), CMake/CPack/release workflow | copied and adapted |
| g13d (MIT, Raphaël Lang) | hardware facts (LCD frame format, LED names), concepts (profile → M modes, focus helper via D-Bus, stick modes) | facts and ideas; if code is ever taken over, its MIT notice is kept |
| Linux kernel `hid-lg-g15` | event codes, device names, LED names, USB IDs | facts only |

---

## 13. Milestones

| # | Content | Hardware needed |
|---|---|---|
| 1 | Repository skeleton: CMake, CI build, packaging skeleton, model files | – |
| 2 | Daemon: discovery, hotplug, grab, uinput, key bindings, profiles, live reload | G13 |
| 3 | Modes M1–M3, LEDs, macros, D-Bus API | G13 |
| 4 | GUI: device image, bindings, macros, backlight, tray | G13 |
| 5 | G13 stick modes, G13 LCD | G13 |
| 6 | Per-application profiles (KWin script, GNOME extension) | – |
| 7 | G11/G15/G15 v2/G510 model files (untested) and image areas, diagnostics, import from linux-g13-driver | – (G13 only) |
| 8 | Packages (Arch/AUR, Debian/Ubuntu, Fedora), release workflow, documentation | – |

---

## 14. Decisions

| # | Topic | Decision (2026-10-21) |
|---|---|---|
| 1 | File format for settings, profiles, macros | JSON (Qt built-in, no extra dependency) |
| 2 | Package targets | only distribution releases with kernel ≥ 6.19 |
| 3 | Test hardware | G13 only; other models shipped as untested |
| 4 | linux-g13-driver | package conflict |
| 5 | LED permissions | dedicated group `gkey` (see 9.1a) |

## 15. Open points

None.
