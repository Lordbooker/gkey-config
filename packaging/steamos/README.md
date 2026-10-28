# gkey-config on SteamOS (Steam Deck)

Not tested on a Steam Deck yet: the archive is built and its installation is tested against the
package versions of SteamOS in CI, but without a device and without game mode. Reports are welcome
in the [issue tracker](https://github.com/Lordbooker/gkey-config/issues).

## Requirements

* SteamOS 3.9 or newer. Its kernel contains the driver `hid-lg-g15` for G11, G13, G15, G15 v2 and
  G510; the G13 needs Linux 6.19 or newer, which SteamOS 3.9 has.
* A password for the user `deck` (set it in Konsole with `passwd`): the installation needs root
  once for the device access.

## Installation

In desktop mode, open Konsole in the folder with the downloaded archive:

```sh
tar -xzf gkey-config-*-steamos-x86_64.tar.gz
cd gkey-config-*-steamos-x86_64
./install.sh
```

Then log out and back in (or restart) once, and open *G-Key Configuration* from the application
menu.

SteamOS replaces `/usr` with every update, so the programs are installed into your home directory
(`~/.local`), and the daemon `gkeyd` runs as a systemd user service. Only the udev rule (device
access) and the group `gkey` (LED control) are set up in `/etc`; the rule is listed in
`/etc/atomic-update.conf.d/gkey-config.conf`, so SteamOS keeps it over updates.

To update, run `install.sh` of the new archive. To remove gkey-config:

```sh
~/.local/share/gkey-config/uninstall.sh
```

Profiles and macros stay in `~/.config/gkey-config/`.
