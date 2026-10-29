# gkey-config – Concept

Status: **approved on 2026-10-21.** Milestones 1–5 done and tested on the G13 (2026-10-21);
translations and milestone 8 (packages, autostart, release workflow) done for version 1.0.0.
Milestones 6 and 7 are postponed until after 1.0.

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
| G11 | 046d:c225 | treated as G15 | 18 | yes | codes registered by the kernel, no physical keys | – | `g15::kbd_backlight` (0–2) | – |
| G13 | 046d:c21c | G13 | 22 | yes | 4 + round "next page" button | yes | `g13:rgb:kbd_backlight` (RGB) | 160×43, hidraw |
| G15 (v1) | 046d:c222 | G15 | 18 | yes | 4 + round button | – | `g15::kbd_backlight` (0–2) | 160×43, hidraw |
| G15 v2 | 046d:c227 | G15 v2 | 6 | yes | 4 + round button | – | `g15::kbd_backlight` (0–2) | 160×43, hidraw |
| G510 | 046d:c22d | G510 | 18 | yes | 5 | – | `g15::kbd_backlight` (RGB) | 160×43, hidraw |
| G510 + USB audio | 046d:c22e | G510 | 18 | yes | 5 | – | `g15::kbd_backlight` (RGB) | 160×43, hidraw |

The Logitech Z-10 speakers are also handled by `hid-lg-g15`, but are out of scope.

### 1.2 Minimum kernel

**Linux 6.19** for the whole project (first release with G13 support in `hid-lg-g15`). G11, G15,
G15 v2 and G510 are supported by older kernels as well, but packages are only built for
distribution releases that ship 6.19 or newer *(decided)*.

The daemon checks at runtime which models are actually usable and reports missing support instead
of failing silently (see 9.2).

### 1.3 Target platforms

Arch Linux, Debian-based (Debian, Ubuntu and derivatives) and Fedora-based distributions, each only
in releases with kernel ≥ 6.19 *(decided)*. Packages for version 1.0: **Ubuntu 26.04** (`.deb`,
kernel 7.0), **Fedora 44** (`.rpm`, kernel 6.19) and the AUR (`gkey-config`, releases only).
Debian 13 ships kernel 6.12 and is not built; newer releases are added when they appear. KDE Plasma (Wayland) and GNOME (Wayland). X11 is not a target but is expected to
work, because nothing below the GUI depends on the display server.

### 1.3a Test hardware

Only a **G13** is available for testing *(decided)*. The model files for G11, G15, G15 v2 and G510
are derived from the kernel source only and are marked **untested** in the GUI and documentation
until someone with the device confirms them.

**G13 verified on hardware on 2026-10-21** (Arch Linux, kernel 7.2.11, `scripts/probe-g13.sh` – now
`scripts/probe-device.sh` – and
`evtest`):

* Keypad: all event codes of section 2.2 confirmed (G1–G22, L1–L4, round button, M1–M3, MR,
  light button).
* Stick: `BTN_THUMB`, `BTN_BASE`, `BTN_BASE2` confirmed; `ABS_X` and `ABS_Y` reach the full range
  0–255. The **rest position is not 128**: it was measured at X ≈ 137–139, Y ≈ 138–140 with a jitter
  of about ±3 (see 7.1). The calibration in milestone 5 confirmed rest values of 136–140 on both
  axes.
* LEDs: names, types and ranges as in section 2.3; power-on backlight was `multi_intensity` 0 255 0.
* Permissions without our udev rule: keypad and hidraw root/`input` only; the stick node already
  carries an ACL for the session user (set by the system's joystick rules); LEDs root-only.

### 1.4 Version 1 includes

Keys, M1–M3 modes, MR, LEDs, profiles, macros, G13 stick modes, G13 LCD, per-application profiles,
GUI, packages. LCD support for G15/G510 was added later with the model files (13.0, 13.0a; untested).

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
| D-Bus activation | `/usr/share/dbus-1/services/io.github.lordbooker.GKeyConfig.service` | starts `gkeyd.service` on a call to the bus name |
| Tray autostart | `/etc/xdg/autostart/gkey-config-tray.desktop` | tray icon at login (per user switchable) |

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
    { "id": "BD",  "code": "KEY_KBD_LCD_MENU5", "role": "lcd" },
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
* `role` marks keys with a fixed function (mode keys, record key, `lcd` for the key that pages
  through the LCD, `hardware` for keys the device handles itself). Everything else is bindable;
  bindings of fixed keys in a profile are ignored with a warning.
* `image` and `areas` describe the clickable device picture in the GUI.
* LED roles: `backlight`, `lcd-backlight` (G15 family), `mode1`–`mode3`, `record`. Types: `onoff`
  (default), `rgb`, `level` with `"max"` = highest brightness (the GUI offers 0 … max; with max 2
  *Off / Dim / Bright*).
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

* `backlight` (RGB models) or `backlightLevel` and `lcdBacklightLevel` (models with brightness
  levels, G15 family): set when the profile becomes active; missing means the LEDs are left as
  they are. Levels above the LED's `max_brightness` are lowered to it.
* `name` identifies the profile; the file name is only derived from it when the profile is created.
* Key IDs are those of the model file; bindings for keys the model cannot bind are ignored with a
  warning in the log.
* `key` and `combo` accept keyboard keys only, `mouse` only `BTN_LEFT` … `BTN_TASK`. Key names are
  evdev names and mean physical key positions; the keyboard layout is applied by the compositor.
* `repeat` (macros): `once`, `while_held`, `toggle`, `count` (with `"count": n`).
* `profile`: a profile name, `next` or `previous`.
* An invalid binding only disables that binding, not the profile; an invalid file is skipped.
* Members the daemon does not know are kept when the file is written again: unknown top-level
  members, unknown modes, and bindings that are invalid or not bindable on the model (they stay
  inactive until the key is bound again in the GUI) *(fixed after 1.2.0; before, saving from the
  GUI dropped them)*.

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
| `passthrough` | stick device is not grabbed; games see the joystick (default for profiles without a `stick` section, so older profiles behave as before) |

The stick buttons (`BTN_BASE`, `BTN_BASE2`, `BTN_THUMB`) are always bindable keys.

The dead zone is centred on the stick's **rest position**, not on 128 (the measured rest position of
the test G13 is about 138/139). Default: the position read when the device is connected, if it is
plausible (within ±24 of 128), otherwise 128. "Measure rest position" in the GUI (D-Bus
`CalibrateStick`) stores the current position per model in the state file.

Implementation (milestone 5): profile member `"stick": { "mode": "keys", "deadzone": 0.25,
"speed": 10, "scrollSpeed": 10 }`. Keys mode presses a direction above the dead zone and releases it
5 % below (hysteresis against jitter); diagonals press two directions. Mouse and scroll modes run
at 100 Hz with a quadratic curve outside the dead zone (`speed` = pixels per 10 ms, `scrollSpeed` =
wheel steps per second at full deflection; stick up = wheel up). In passthrough mode the stick
buttons are left to the system as well.

### 7.2 LCD (version 1)

* Start animation *(added after 1.2.1, decided 2026-10-24)*: the flame intro of linux-g13-driver
  (`LcdIntro`, frames identical to the original): a flame wall rises, burns the start image, covers
  the display and recedes, revealing Tux and the device name; about 9 s at 20 frames per second.
  Start image: the G13 firmware logo; on G15, G15 v2 and G510 a dark display (their firmware logos
  are not known). Final images: G13 from linux-g13-driver; the others generated in the same style
  (`tools/make-lcd-intro-images.py`). It is the page `intro` in the page list: always first in the
  LCD tab and not movable; enabled by default for new installations, unchecked in existing
  settings. When enabled it plays every time a device connects (daemon start, replug, wake-up);
  its final image stays until the page is changed. It is not part of the rotation of "next" and
  "previous". A text sent over D-Bus interrupts it; after a temporary text the animation continues
  where it was. `LcdPage intro` plays it again. At least one other page always stays enabled.
* Profile page *(added after 1.5.0, decided 2026-10-29)*: the page `profile` shows the name of the
  profile for 3 seconds when the profile changes (not for the first profile after connecting and
  not for a change of the layer), then the previous page returns, like a text with a duration; a
  text or a further change in that time keeps the original return page. It is not part of the
  rotation and can be switched off in the LCD tab like all pages; enabled by default for new
  installations, unchecked in existing settings. The name is drawn as large as it fits: one line
  in scale 5 (5 characters, about the size of the G13 logo of the start animation), 4 (6), 3 (8)
  or 2 (13); else two lines in scale 2, broken at the space that balances them; else one line in
  scale 1, cut with "~" after 26 characters. Profile names have no length limit.
* Pages: status (device, profile, layer M1–M3, "REC" while recording), system (see below), cpu,
  amdgpu, nvidia (see below), clock (time, seconds, day and date in the user's locale), text (set
  via D-Bus `LcdText`, up to five lines).
* Enabled pages and their order are global settings in `~/.config/gkey-config/settings.json`
  (`{ "lcd": { "pages": ["status", "system", "clock", "text"] } }`), edited in the GUI's LCD tab.
  Without the file the pages intro, profile, status, system, clock and text are enabled in this order
  (`LcdPages::defaultPages()`); the hardware pages cpu, amdgpu and nvidia are off, as they are
  only useful with that hardware. A page added in a later version appears unchecked in existing
  settings.
* The round button (BD) always shows the next page *(fixed after 1.1.1, decided 2026-10-22: it is the
  only way through the pages on the device)*; it has the role `lcd` and cannot be bound. Model
  files can still give a bindable key a `default` action that applies while the key is unbound in
  the active layer. Any bindable key can be bound to `lcd` actions (`next`, `previous`, page name).
* `LcdText` with a duration shows the text page temporarily and returns to the previous page.
* Output: output report 3 on the G13's hidraw node (report ID, 31 zero bytes, 160 × 48 pixels in
  bands of 8 rows). Rendering uses an own 1-bit frame and the 5×7 font from linux-g13-driver, so the
  daemon needs no Qt GUI modules; the GUI uses the same code for its preview. The LCD is cleared
  when the daemon stops.
* The reports are written by `LcdWriter` in a thread of its own *(after 1.2.0)*: a hidraw write is
  a synchronous USB transfer that can block for seconds on a hanging device and must not stop the
  key handling. Only the newest report is kept. The hidraw node is looked up when writing (it may
  appear after the input devices) and reopened after an error.
* System page *(added after 1.0, decided 2026-10-22)*: rows CPU, RAM, GPU, VRM, each with a bar,
  the percentage and on the right the temperature (CPU, GPU) or the used amount in GiB (RAM,
  VRAM); four rows 11 px apart. Without a supported GPU the rows are CPU, RAM and swap (if
  configured), 14 px apart. Sources, readable without special rights: `/proc/stat` (load since the
  previous refresh), `/proc/meminfo` (used = total − available), hwmon `k10temp`/`zenpower`
  (`Tctl`/`Tdie`) or `coretemp` (`Package id 0`) for the CPU temperature, and for AMD GPUs
  (`amdgpu`) `gpu_busy_percent`, `mem_info_vram_used/total` and the hwmon temperature `edge` of
  the card with the most VRAM. Intel GPUs have no load value in sysfs; NVIDIA is not supported
  yet. The values are read once per second, only while the page is shown. The font got a degree
  sign (code 127).
* Hardware pages *(added after 1.3.0, decided 2026-10-24)*, redrawn once per second while shown:
  * cpu: load and all CPU temperatures in the top row (hwmon `k10temp`/`zenpower`/`coretemp`;
    `Tccd1` shortened to `CCD1`; if they do not fit, the values after the first are combined as
    `61/63/60°`, without the first name if needed). Intel *(added 2026-10-26, untested)*:
    `coretemp` has one value per core besides `Package id 0`; of the cores only the hottest is
    shown, as `Max` (`CPU 23% Pkg 61° Max 68°`). Below, over the whole width, one bar per
    **logical CPU** (each SMT thread, in the order of the CPU numbers; `cpufreq`
    `scaling_cur_freq`), scaled from 0 to the highest `cpuinfo_max_freq` of all CPUs (Intel
    hybrid CPUs: P- and E-cores differ), without clock labels *(changed 2026-10-25: first one bar
    per physical core, scaled from `cpuinfo_min_freq`, with labels)*.
    The bar width follows the number of CPUs (one-pixel gap up to 52 CPUs, single pixels up to
    158); with more CPUs than pixel columns a column stands for several CPUs and shows the
    highest clock.
  * amdgpu: the AMD GPU with the most VRAM: load bar (`gpu_busy_percent`), core and memory clock
    (hwmon `sclk`/`mclk`), all hwmon temperatures (edge, junction, mem), VRAM bar with used/total,
    power (`power1_average`, else `power1_input`) and fan (`fan1_input`, rpm).
  * nvidia: the first NVIDIA GPU through NVML (`libnvidia-ml.so.1`, loaded at runtime with
    `QLibrary`, no build or package dependency): load, graphics and memory clock, GPU temperature
    (NVML has no hotspot or memory temperature for consumer cards), VRAM, power and fan (percent).
    Untested on hardware.
  * Values the driver does not report are shown as "--" or left out; without the GPU the page
    shows the reason (e.g. "no NVIDIA driver (NVML)").
* A media page is a possible later addition.
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
| hidraw node of devices with LCD (G13; G15, G15 v2, G510 via the hid-lg-g15 interface) | `TAG+="uaccess"` |
| `/dev/uinput` | `TAG+="uaccess"` |
| LED attributes (`brightness`, `multi_intensity`) of supported devices | group `gkey`, mode 0664 (see 9.1a) |

`uaccess` gives access to the user of the active local session only (systemd-logind), the same
approach as linux-g13-driver.

**Security note on `/dev/uinput`** *(reviewed 2026-10-23, kept)*: with access to `/dev/uinput`
every program of the session user can create virtual input devices and send key presses to any
window, which bypasses the isolation of input that Wayland compositors otherwise provide. The
daemon needs it for its output; Steam's udev rules grant the same access for its controller
support. Restricting it to the group `gkey` was considered: it would protect nothing against
programs of a user in that group and would make the basic function depend on a group membership
and a new login. The access is therefore kept and documented (README, wiki).

### 9.1a LED permissions *(decided)*

sysfs attributes cannot carry ACLs, so `uaccess` does not work for the LEDs. Instead:

* The packages create a system group **`gkey`** via `sysusers.d`.
* The udev rule sets group `gkey` and mode `0664` on `brightness` and `multi_intensity` of the LEDs
  that belong to a supported device (matched below the device's HID node, not by name alone).
* Users who want LED control are added once: the GUI offers it on its first start and in the
  *Settings* menu and runs `pkexec usermod -aG gkey <user>` (polkit asks for the administrator
  password); by hand `sudo usermod -aG gkey $USER`. Then log out completely and back in. Packages
  never add users on their own.
* Existing groups such as `input` are **not** used: membership there allows reading every keyboard.
* Without group membership everything except the LEDs keeps working. The daemon detects the
  missing permission and reports it (log, D-Bus `Status`, GUI hint with the command above).

Rejected alternatives: world-writable attributes (any local user could change the LEDs; contradicts
the "no world-writable nodes" rule from linux-g13-driver), a privileged helper service (root
component and polkit policy only for LEDs), writing LEDs through hidraw (bypasses the kernel's LED
state, G13 only).

### 9.2 Diagnostics and coexistence

Why a connected device is not usable is reported in two places:

* The GUI checks sysfs for the model's USB IDs while the device is not connected and names the
  reason in the status bar *(implemented for 1.0)*: kernel older than the model's `minKernel`; a USB
  interface claimed through `usbfs` (linux-g13-driver or another libusb program); HID device
  missing or not providing the model's input devices (`hid-lg-g15` not active); plugged in and
  provided by the kernel but not used by the daemon (udev rule, see the log).
* The daemon logs a HID device bound to another driver than `lg-g15`, input devices, hidraw nodes
  or LED attributes that cannot be opened, and an inaccessible `/dev/uinput` (exits with an error).
  The LED permission is also part of the D-Bus `Status`.

linux-g13-driver and gkey-config cannot drive the same G13 at the same time; the packages declare a
**package conflict** with `linux-g13-driver` (and the AUR `linux-g13-driver-git`) *(decided)*.

### 9.3 Virtual device

One uinput device named `gkey-config virtual input` with bus type `BUS_VIRTUAL` and vendor/product
ID 0000:0000 (not Logitech's), so it is never mistaken for a real device by our own discovery
(which only takes HID devices) or by udev rules. It
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
| Method | `LcdText(s text, i seconds)` | text page; lines are separated by newline characters (a literal backslash-n is not interpreted); seconds 0 = until the page is changed |
| Method | `LcdPage(s page)` | `next`, `previous`, `status`, `system`, `clock`, `text`, `intro` (plays the start animation) |
| Method | `CalibrateStick(s model) → b` | current stick position becomes the rest position |
| Property | `Recording` (b) | |
| Signals | `DeviceAdded(s model, s syspath)`, `DeviceRemoved(s, s)`, `ProfileChanged(s model, s name)`, `ModeChanged(s model, i mode)`, `KeyEvent(s model, s key, b pressed)`, `RecordingChanged(b)` | |

`KeyEvent` lets the GUI highlight pressed keys and lets the user pick a key by pressing it.
`Status` also lists `lcdPage` and `stickCenter` per device. Planned for milestone 6: `ReportFocus`,
`LockProfile`.

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
* Translations via Qt Linguist, chosen by the system language (`LANGUAGE`, `LC_ALL`, `LC_MESSAGES`,
  `LANG`) without a language setting of its own: English (source and fallback), German, French,
  Italian, Spanish and Portuguese (Brazilian), built into the binary. The desktop files carry the
  same languages. Daemon log messages stay English; LCD day names follow the daemon's locale and are
  reduced to ASCII (é → e, ä → ae). A test fails when a translation is missing or unfinished.
* `gkey-config --screenshot file [--show-key G4 --show-tab 0]` renders the window to an image
  (documentation).

Milestone 5 added the *Stick* tab (mode, dead zone, pointer and scroll speed, "bind W, A, S, D",
measuring the rest position) and the *LCD* tab (enabled pages and their order, preview, show a page
on the device).

---

## 12. Reused work

| Source | What | How |
|---|---|---|
| linux-g13-driver (GPL-2.0-only, own project) | macro engine, actions, stick zone logic, GUI parts (binding editor, colour selector, macro recorder, key capture, key names, tray, single instance, translations), CMake/CPack/release workflow | copied and adapted |
| g13d (MIT, Raphaël Lang) | hardware facts (LCD frame format, LED names), concepts (profile → M modes, focus helper via D-Bus, stick modes) | facts and ideas; if code is ever taken over, its MIT notice is kept |
| Linux kernel `hid-lg-g15` | event codes, device names, LED names, USB IDs | facts only |
| linux-g13-driver: start animation | flame algorithm, G13 firmware logo and final image (`IntroAnimation`) | ported unchanged (frames bit-identical) to `LcdIntro` |
| Tux (Larry Ewing, lewing@isc.tamu.edu, created with The GIMP) | penguin in the final images | as in linux-g13-driver; credited in `LcdIntroImages.h` |
| Black Ops One (SIL Open Font License 1.1) | lettering of the final images | rendered to bitmaps by `tools/make-lcd-intro-images.py`; the font itself is not part of the repository |

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
| 8 | Packages (Arch/AUR, Ubuntu, Fedora), autostart and LED setup in the GUI, release workflow, documentation | – |

Done: 1–5 and 8 (releases 1.0.0–1.1.1). Postponed: 6. Milestone 7: model files for
G510, G15, G11 and G15 v2 are done in 1.2.0 (untested, see 13.0–13.0c); the import from linux-g13-driver is
open.

### 13.0 G510 *(2026-10-22)*

Taken from the kernel driver (`hid-lg-g15.c`) and libg15; no test hardware, so the model is marked
untested:

* USB 046d:c22d, and 046d:c22e while a headset uses the built-in audio. hid-lg-g15 drives the HID
  interface with the gaming keys and the LCD; the normal keyboard is another interface (hid-generic)
  and is never touched. Input device "Logitech Gaming Keyboard Gaming Keys".
* G1–G18 = `KEY_MACRO1`–`18`, M1–M3/MR as on the G13. LCD keys: `KEY_KBD_LCD_MENU1` is the round
  page button (BD, fixed role `lcd`; MENU5 on the G13), MENU2–5 are L1–L4 – assumed from the G15
  comments in the driver, to be confirmed on hardware.
* Headphone and microphone mute (`KEY_MUTE`, `KEY_F20`) arrive on the grabbed device; the role
  `passthrough` sends them on unchanged.
* LEDs `g15::kbd_backlight` (RGB, multicolor like the G13), `g15::macro_preset1`–`3`,
  `g15::macro_record`; `g15::power_on_backlight_val` is not used.
* LCD 160 × 43 with the same 992-byte frame as the G13 (libg15 uses one format for G13, G15 and
  G510); LCD type `g15-hidraw`.
* udev: unlike the G13, access is restricted to the hid-lg-g15 interface (`KERNELS` + `DRIVERS`
  on the HID device), so the normal keyboard is not opened to the session user.
* View: simple schematic (LCD with its keys, M row, 3 × 6 G keys).

### 13.0a G15 *(2026-10-22)*

Same scheme as the G510 (kernel driver, libg15; untested, no hardware), differences only:

* USB 046d:c222, no audio interface; no mute keys.
* LCD keys as stated in the driver for the G15: `KEY_KBD_LCD_MENU1` = round button left of the LCD
  (BD, role `lcd`), MENU2–5 = the four keys below it (L1–L4).
* Backlight of keys and LCD: `g15::kbd_backlight` and `g15::lcd_backlight`, brightness 0–2 each
  (LED type `level`, roles `backlight` and `lcd-backlight`). The profile stores
  `backlightLevel` and `lcdBacklightLevel`; the *Backlight* tab shows two selectors
  *Off / Dim / Bright* instead of the colour. The backlight button of the keyboard changes the
  brightness in the device itself; the profile sets it again when it becomes active.
* LCD and its frame as on the G510 (`g15-hidraw`, 160 × 43).
* udev: `KERNELS=="0003:046D:C222.*"` with `DRIVERS=="lg-g15"`, like the G510.

### 13.0b G11 *(2026-10-22)*

The kernel driver says: "The G11 is a G15 without the LCD, treat it as a G15" (same report
handling, so it also registers the LCD key codes and `g15::lcd_backlight`). The model file
(untested) therefore lists only what the keyboard physically has:

* USB 046d:c225; G1–G18, M1–M3, MR; no LCD keys, no `lcd` section, no `lcd-backlight` LED.
* Kernel: added by commit `b1bd0f75288f` ("HID: logitech: Add support for Logitech G11 extra
  keys"), first in **Linux 5.7** (not 5.5 like G15, G15 v2 and G510); `minKernel` "5.7"
  *(corrected after 1.2.0)*.
* Backlight `g15::kbd_backlight`, brightness 0–2 (type `level`), M LEDs as on the G15.
* udev: only the input device of the hid-lg-g15 interface; the hidraw node stays closed.
* View: M row and 3 × 6 G keys.

### 13.0c G15 v2 *(2026-10-22)*

Like the G15 (13.0a), untested; from the kernel driver (`lg_g15_v2_event`, `gkeys = 6`):

* USB 046d:c227; G1–G6, M1–M3, MR; BD = `KEY_KBD_LCD_MENU1` (round button), L1–L4 = MENU2–5.
* LEDs registered exactly as on the G15: `g15::kbd_backlight` and `g15::lcd_backlight` (0–2),
  M LEDs.
* LCD `g15-hidraw`, 160 × 43.
* View: schematic (LCD with its keys, M row, G1–G6 in 3 × 2); not the physical arrangement.

### 13.1 To do

Agreed ideas that are not scheduled yet:

* **Logitech G710 / G710+** *(noted 2026-10-23, watch)*. A patch adds them to `hid-lg-g15`
  (USB 046d:c24d; "[PATCH] HID: lg-g15: Add support for the Logitech G710/G710+ gaming
  keyboards", Xavier Bestel, posted 2026-04 and again 2026-05-18). Not in the kernel as of
  2026-10-22 (master during the 7.4 merge window). Once merged: model file after the same scheme
  as G510/G15 (untested), `minKernel` = the first release containing it.

* **NVIDIA GPUs on the LCD system page** *(discussed 2026-10-22, postponed)*. Since 2026-10-24
  NVML is read by `NvidiaStats` for the page `nvidia` (7.2); still open for the system page:
  * same rows as for AMD (GPU load, GPU temperature, VRAM used/total); with AMD and NVIDIA in one
    system the GPU with the most VRAM wins;
  * hybrid laptops: if the NVIDIA PCI device is runtime-suspended
    (`/sys/bus/pci/devices/…/power/runtime_status`), NVML is not touched so the GPU is not woken;
    the row shows "--%";
  * nouveau is not supported (no load values);
    (this also applies to the page `nvidia`, which reads NVML every second while shown);
  * tests with a fake NVML library (so far only the case without the library is tested); real
    hardware not available, so marked untested until someone reports back.

---

## 14. Decisions

| # | Topic | Decision (2026-10-21) |
|---|---|---|
| 1 | File format for settings, profiles, macros | JSON (Qt built-in, no extra dependency) |
| 2 | Package targets | only distribution releases with kernel ≥ 6.19 |
| 3 | Test hardware | G13 only; other models shipped as untested |
| 4 | linux-g13-driver | package conflict |
| 5 | LED permissions | dedicated group `gkey` (see 9.1a) |
| 6 | Daemon autostart | per user: the GUI enables `gkeyd.service` with `systemctl --user` (asked on its first start, switch in *Settings*); packages enable nothing. D-Bus activation starts the daemon for scripts; the GUI never activates it. |
| 7 | Installation | GitHub releases with `.deb`/`.rpm` and the AUR package `gkey-config` (releases only, no `-git` package for `dev`, 2026-10-22); no install script |
| 8 | Releases | tag `v*` on `main` runs the release workflow (packages, GitHub release, AUR with secret `AUR_SSH_PRIVATE_KEY`); `dev` pushes build test packages |

## 15. Open points

None.
