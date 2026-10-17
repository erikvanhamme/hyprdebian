## Introduction
The package `hd-network` renders the systemd-networkd files in `/etc/systemd/network` from the key `hd-network` in the hyprdebian configuration, `/etc/hyprdebian/config.json`. The settings are changed with `hd-network-iface` for wired and wifi interfaces and `hd-network-bridge` for bridges, which write them with `hd-config-update` (see [hd-config](hd-config.md)), then applied with `hd-network-render`. `hd-network` depends on `hd-config` for that.

Installing the package configures nothing: it does not set the key, and it does not run `hd-network-render`.

The package also installs `/usr/lib/systemd/network/10-ethernet.link` and `20-wifi.link`, which name ethernet and wifi interfaces by their path, such as `enp3s0` and `wlp2s0`, and give them a persistent MAC address. They come before `99-default.link` of systemd, so its naming policy (onboard, slot, then path names) is not used for these interfaces. A file with the same name in `/etc/systemd/network` takes their place.

The initramfs holds a copy of the `.link` files, so the interfaces get the same names from the start of the boot. Installing or upgrading the package activates the `update-initramfs` trigger of initramfs-tools, which rebuilds it once dpkg is done; this changes no settings.

The package comes with four man pages: `man hd-network` describes the settings, `man hd-network-iface` and `man hd-network-bridge` the commands that change them, and `man hd-network-render` the command that applies them.

## Building the package
The package is built from `hd-network` in this repository:
```
cd hd-network
make deb
```

This builds `build/hd-network_<version>_all.deb`, with the version from `version.json`. Raise the version there for every package that is handed out. It depends on `python3`, `python3-jinja2`, `systemd` and `hd-config`. Install it with:
```
sudo apt install ./build/hd-network_<version>_all.deb
```

The files involved are:
```
hd-network/Makefile
hd-network/version.json
hd-network/hd-network-render
hd-network/hd-network-iface
hd-network/hd-network-bridge
hd-network/lib/hd_network.py
hd-network/deb/control
hd-network/deb/triggers
hd-network/templates/header.j2
hd-network/templates/bridge.netdev.j2
hd-network/templates/interface.network.j2
hd-network/link/10-ethernet.link
hd-network/link/20-wifi.link
hd-network/man/hd-network.7
hd-network/man/hd-network-render.8
hd-network/man/hd-network-iface.8
hd-network/man/hd-network-bridge.8
```

## Settings
The key `hd-network` holds a JSON object:
```
{
    "hd-network": {
        "interfaces": {
            "enp3s0": { "bridge": "br0" },
            "wlp2s0": { "metric": 200 }
        },
        "bridges": {
            "br0": { "mac": "52:54:00:3a:1f:c2" }
        },
        "wait_online": false
    }
}
```

All its fields are optional:

| Field | Meaning |
| --- | --- |
| `interfaces` | Maps interface names to their settings. A name may be a glob, such as `en*`; an interface whose exact name is listed uses its own settings, not those of a glob. Interfaces that are not listed are left alone. |
| `bridges` | Maps bridge names to their settings. |
| `wait_online` | `false` masks `systemd-networkd-wait-online.service`, so the boot does not wait for the network; `true` unmasks it. Left out, the service is left as it is. |

An interface with the field `bridge` is a port of that bridge, which must be in `bridges`. It has no other fields and no addresses of its own. A wifi interface, one whose name starts with `wl`, cannot be a port.

Every other interface, and every bridge, has these fields, all optional:

| Field | Default | Meaning |
| --- | --- | --- |
| `dhcp` | `true` | DHCPv4, IPv6 router advertisements and DHCPv6, using the search domains they hand out. `false` turns all three off. |
| `addresses` | none | Static addresses with their prefix length, IPv4 and IPv6 mixed, such as `192.168.1.10/24`. A string or a list. They may be combined with DHCP. |
| `gateway` | none | The default gateway, or a list with one for IPv4 and one for IPv6. |
| `dns` | none | DNS servers, a string or a list. |
| `metric` | `100` | The metric of the routes, from DHCP and to the gateways. The route with the lowest metric wins, so give wifi a higher one than ethernet. |

A bridge also has the field `mac`, its MAC address. `hd-network-bridge` gives a new bridge a random, locally administered address, and `hd-network-render` does the same for a bridge without one. The address is stored in the settings, so it never changes, also not when the machine-id changes.

Unknown fields are errors, so typing mistakes are caught.

## Examples
DHCP on every ethernet interface, and on the wifi interface with a higher metric:
```
sudo hd-network-iface set 'en*'
sudo hd-network-iface set wlp2s0 --metric 200
sudo hd-network-render
```

A bridge for virtual machines, with `enp3s0` as its port and DHCP on the bridge:
```
sudo hd-network-bridge set br0
sudo hd-network-iface set enp3s0 --bridge br0
sudo hd-network-render
```

A static address:
```
sudo hd-network-iface set enp3s0 --no-dhcp \
    --address 192.168.1.10/24 --address fd00::10/64 \
    --gateway 192.168.1.1 --gateway fd00::1 --dns 192.168.1.1
sudo hd-network-render
```

`wait_online` has no command of its own; set it with `hd-config-update`, together with the rest of the key.

## hd-network-iface
```
hd-network-iface list
hd-network-iface show <name>
hd-network-iface set <name> [options]
hd-network-iface remove <name>
```

`hd-network-iface` manages the wired and wifi interfaces in the settings. `list` shows every interface with a summary of its settings, `show` the settings of one as JSON. `set` adds an interface or changes its settings; settings that are not given stay as they are, and a new interface without options uses DHCP. `remove` removes it. The name is an interface name, such as `enp3s0`, or a glob, such as `en*`; quote a glob so the shell does not expand it.

The options of `set`:

| Option | Meaning |
| --- | --- |
| `--bridge <bridge>` | Make the interface a port of the bridge, which must be set up with `hd-network-bridge` first. This drops the other settings, and cannot be combined with the options below. |
| `--dhcp`, `--no-dhcp` | Use DHCP and router advertisements, or not. |
| `--address <address/prefix>` | A static address. May be given more than once; replaces the addresses. |
| `--gateway <address>` | The default gateway. May be given more than once, for IPv4 and IPv6; replaces the gateways. |
| `--dns <address>` | A DNS server. May be given more than once; replaces the DNS servers. |
| `--metric <n>` | The metric of the routes. |
| `--unset <setting>` | Return `addresses`, `bridge`, `dhcp`, `dns`, `gateway` or `metric` to its default, before the other options are applied. May be given more than once. |

`--unset bridge` turns a port into an interface with settings of its own: `hd-network-iface set enp3s0 --unset bridge` puts it back on DHCP.

## hd-network-bridge
```
hd-network-bridge list
hd-network-bridge show <name>
hd-network-bridge set <name> [options]
hd-network-bridge remove <name>
```

`hd-network-bridge` manages the bridges in the settings, in the same way. `list` also shows the MAC address and the ports of every bridge. A new bridge gets a random MAC address, unless `--mac` is given. `set` takes the options of `hd-network-iface set` except `--bridge`, plus `--mac <mac>`; `--unset` takes `addresses`, `dhcp`, `dns`, `gateway` or `metric`. `remove` refuses a bridge that still has ports; remove or change them with `hd-network-iface` first.

Ports are added with `hd-network-iface set <interface> --bridge <bridge>`.

## How the commands change the settings
`hd-network-iface` and `hd-network-bridge` read all of the key `hd-network` from `/etc/hyprdebian/config.json`, make the change, and check the result the same way `hd-network-render` does. When the check fails, they stop with an error and nothing is written. Otherwise they write the key with `hd-config-update`, or remove it with `hd-config-remove` when nothing is left. `list` and `show` may be run by any user; `set` and `remove` are run as root.

They do not change the network itself: run `hd-network-render` afterwards. The three commands share their code for reading, checking and writing the settings, in `/usr/lib/hd-network/hd_network.py`.

## hd-network-render
```
hd-network-render [-n]
```

`hd-network-render` reads the settings and renders the files from the jinja2 templates in `/usr/share/hd-network/templates`. Run it as root, after changing the settings with `hd-network-iface` or `hd-network-bridge`. With `-n` or `--dry-run`, it shows the files it would write and remove, and what it would do with the wait-online service, and changes nothing; a bridge without a MAC address then shows a random one that is not stored.

A run goes through these steps:
1. The settings are checked first. An error stops the run before anything changes.
2. A bridge without a MAC address gets a random one, which is stored in the settings with `hd-config-update`.
3. The files are written; only files whose content changes are written:

   | File | Content |
   | --- | --- |
   | `30-<bridge>.netdev` | Makes the bridge, with its MAC address. |
   | `40-<bridge>.network` | The addresses of the bridge. |
   | `50-<interface>.network` | An interface with an exact name: its addresses, or the bridge it is a port of. |
   | `59-<glob>.network` | An interface named by a glob, with the characters other than letters, digits, `.`, `_` and `-` replaced by `_`. |

   systemd-networkd uses the first file that matches an interface, so the globs come after the exact names.
4. Every file starts with a header that says it was generated. A file in `/etc/systemd/network` with that header that the settings no longer ask for is removed. Files without the header are left alone.
5. When `wait_online` is set, `systemd-networkd-wait-online.service` is masked or unmasked to match.
6. When files changed and systemd-networkd runs, it is reloaded with `networkctl reload`.

A reload applies new and changed `.network` files, but does not change a bridge that already exists, such as its MAC address; that takes a reboot, or `networkctl delete` of the bridge followed by another reload.

It exits with 0 when the files were rendered, 1 on an error and 2 on wrong arguments.

## Templates
The templates are in `/usr/share/hd-network/templates`, rendered with jinja2:

| Template | Used for |
| --- | --- |
| `header.j2` | The header at the start of every file, included by the others. |
| `bridge.netdev.j2` | `30-<bridge>.netdev`: the bridge with its MAC address. |
| `interface.network.j2` | Every `.network` file: `Bridge=` for a port; otherwise the addresses, DNS servers, DHCP sections and a `[Route]` per gateway. |

They get the settings with the defaults filled in: `name`, `bridge` (empty except for a port), and for the others `dhcp`, `addresses`, `gateways`, `dns` and `metric`, where `addresses`, `gateways` and `dns` are always lists.

## Files
| Path | Purpose |
| --- | --- |
| `/etc/hyprdebian/config.json` | The configuration, with the settings under the key `hd-network`. |
| `/etc/systemd/network` | Where the files are rendered. |
| `/usr/share/hd-network/templates` | The jinja2 templates the files are made from. |
| `/usr/lib/systemd/network/10-ethernet.link`, `/usr/lib/systemd/network/20-wifi.link` | Name ethernet and wifi interfaces by their path. |
| `/usr/sbin/hd-network-render` | Renders the files. |
| `/usr/sbin/hd-network-iface` | Adds, changes and removes interfaces in the settings. |
| `/usr/sbin/hd-network-bridge` | Adds, changes and removes bridges in the settings. |
| `/usr/lib/hd-network/hd_network.py` | The code the three commands share. |
