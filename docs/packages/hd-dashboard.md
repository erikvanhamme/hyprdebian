## Introduction
The package `hd-dashboard` turns a hyprdebian system into the hyprdebian dashboard, which shows the state of every hyprdebian system on one page. Each system with the package `hd-report` sends a status report to the dashboard every 15 minutes, see [hd-report](hd-report.md). The systems only make outgoing connections, so no system needs an open port for it.

The dashboard keeps the latest report of every system, and a history of the load, the memory and the fullest pool, 30 days by default. It judges each system on its last report and colors its card by its worst issue.

Installing the package sets everything up:
- the default settings, in the key `hd-dashboard` of `/etc/hyprdebian/config.json`, when there are none yet;
- `hd-dashboard.service`, which is enabled and started, and makes the database `/var/lib/hd-dashboard/dashboard.db` on its first start.

The installation does not open the port of the dashboard, 80 by default, in the firewall; that is up to the admin, see [Firewall](#firewall). Once it is open, the dashboard answers on `http://<host>/`.

The package comes with two man pages: `man 7 hd-dashboard` describes the dashboard, its settings and the reports, `man 8 hd-dashboard` the command.

## Building the package
The package is built from `packages/hd-dashboard` in this repository:
```
cd packages/hd-dashboard
make deb
```

This builds `build/hd-dashboard_<version>_all.deb`, with the version from `version.json`. Raise the version there for every package that is handed out. It depends on `python3`, `systemd` and `hd-base`; the dashboard needs nothing beyond Python itself. On a system with `hd-base` before 1.1, which kept services from being enabled on their installation, enable `hd-dashboard.service` by hand after installing it: `sudo systemctl enable --now hd-dashboard.service`. Install it with:
```
sudo apt install ./build/hd-dashboard_<version>_all.deb
```

The files involved are:
```
packages/hd-dashboard/Makefile
packages/hd-dashboard/version.json
packages/hd-dashboard/hd-dashboard
packages/hd-dashboard/defaults.json
packages/hd-dashboard/hd-dashboard.service
packages/hd-dashboard/deb/control
packages/hd-dashboard/deb/postinst
packages/hd-dashboard/deb/prerm
packages/hd-dashboard/deb/postrm
packages/hd-dashboard/man/hd-dashboard.7
packages/hd-dashboard/man/hd-dashboard.8
packages/hd-dashboard/tests/test_dashboard.py
```

## The service
`hd-dashboard.service` runs `hd-dashboard serve`. It runs as a user that systemd makes up (`DynamicUser`), which may bind to port 80 without being root, and may only write its state directory, `/var/lib/hd-dashboard`. systemd keeps that directory in `/var/lib/private/hd-dashboard`, which only root can open.

The service is enabled on the first installation, as Debian enables services, by its preset, see [hd-base](hd-base.md#services); after that, it stays as the admin left it, also over upgrades. An upgrade restarts it when it is enabled. Its log is in the journal; only refused reports and errors get a line there:
```
journalctl -u hd-dashboard.service
```

`hd-dashboard` listens on port 80 by default, the same port as `hd-repo`. To have both on one system, give the dashboard another port, see [Settings](#settings).

## Settings
The settings are the key `hd-dashboard` of `/etc/hyprdebian/config.json`. The installation stores the defaults there, from `/usr/share/hd-dashboard/defaults.json`, when the key is missing; a setting that is missing from the key has its default value. Change them with `hd-config-update`, which replaces the key as a whole, and restart the service:
```
sudo hd-config-update hd-dashboard '{"listen": "0.0.0.0", "port": 8080, "stale_minutes": 45, "offline_days": 14, "history_days": 30}'
sudo systemctl restart hd-dashboard.service
```

| Key | Default | |
| --- | --- | --- |
| `listen` | `"0.0.0.0"` | The address to listen on. |
| `port` | `80` | The port to listen on. |
| `stale_minutes` | `45` | A system that did not report for this long is offline, which is a note. Systems report every 15 minutes. |
| `offline_days` | `14` | A system that did not report for this many days is an error, so a laptop that is off for a while does not turn red. |
| `history_days` | `30` | How long the history is kept. |

The service does not start when a setting has the wrong type; the journal says which one.

## Firewall
The package leaves the firewall alone: the systems can only reach the dashboard once the admin allows its port. With ufw, on the default port:
```
sudo ufw allow 80/tcp
```

To let only the local network in:
```
sudo ufw allow from 192.168.1.0/24 to any port 80 proto tcp
```

After changing the port in the settings, change the rule as well.

## The pages
| Path | |
| --- | --- |
| `POST /report` | Stores a report. |
| `GET /` | The overview: a card per system, the worst first. Refreshes every minute. |
| `GET /host/<name>` | One system, with the charts of its history and its last report. |
| `GET /api/hosts` | The systems, their reports and issues as JSON. |

The connection is plain HTTP, meant for the local network.

## Reports
`hd-report` POSTs the report of a system to `http://<dashboard>/report` as a JSON object, with its token in the header `Authorization: Bearer <token>`. The token is 16 to 128 letters and digits, generated once on the system and kept there. A report is at most 256 KiB.

| Part | Content |
| --- | --- |
| `format` | The layout of the report, now 1. Reports with a newer format than the dashboard knows are refused with 400. |
| `host`, `fqdn` | The host name, by which the dashboard knows the system, and its FQDN. |
| `hyprdebian` | `version` and `suite` of the HyprDebian installation. |
| `os` | `debian`, the Debian version, `kernel`, the running kernel, `kernel_newest`, the newest installed kernel, `boot_time`, and `secure_boot`, the state of Secure Boot as `hd-secboot-check` finds it: `on`, `off`, `setup`, `unsupported` or `legacy`, see [hd-secboot](hd-secboot.md#hd-secboot-check). |
| `load`, `cpus` | The load averages and the number of CPUs. |
| `memory` | `total` and `used`, in bytes. |
| `pools` | Per ZFS pool: `name`, `health`, `size`, `capacity` in percent, and `scrub`, the last completed scrub, with `repaired`, the bytes it repaired, `errors` and `time`, when it ended; `null` when the pool has none. |
| `arrays` | Per mdadm array: `name`, `state` and `degraded`. |
| `units` | `state`, the state of systemd, and `failed`, the failed units. |
| `updates` | `upgradable`, the number of packages that can be upgraded, `lists_time`, `last_upgrade` and `reboot_required`. |
| `snapshot` | `name` and `time` of the newest snapshot of the system. |
| `timers` | Per timer: `unit`, `service`, `last`, `next` and `result`. |

Times are Unix timestamps. Parts that are missing are left out of the pages. Raise `FORMAT` in both `hd-dashboard` and `hd-report` when the layout changes in a way the dashboard must know about, and update the dashboard before the systems.

### Tokens
The first report of a host name registers its token. Later reports of that host name must have the same token, otherwise they are refused with 403. The dashboard only stores a hash of the token.

A reinstalled system has a new token. Forget the old one, and the next report registers the new one:
```
sudo hd-dashboard forget <host>
```

`sudo hd-dashboard hosts` lists the systems and when they last reported.

### Issues
Each system is judged on its last report. The card gets the color of its worst issue.

| Level | Issue |
| --- | --- |
| Error | No report for longer than `offline_days`. |
| Error | Failed units. |
| Error | A pool that is not `ONLINE`, or 90% full or more. |
| Error | The last completed scrub of a pool found errors it could not repair. |
| Error | A degraded mdadm array. |
| Error | The last run of the service of a timer failed. |
| Error | Secure Boot is not on, or the report does not say, as from an `hd-report` before 0.5. |
| Warning | A pool that is 80% full or more. |
| Warning | The last completed scrub of a pool repaired bytes: the pool healed itself, but a drive may be failing. The warning stays until a scrub repairs nothing. |
| Warning | The system is starting or stopping. |
| Warning | A reboot is pending: `reboot_required`, or a newer kernel than the running one. |
| Warning | No upgrade for 14 days, or never. |
| Warning | A HyprDebian version older than the version on the system of the dashboard, from `/etc/hyprdebian/version.json`. A system with a newer version, like a test machine, gets no warning; without that file no versions are judged. |
| Note | No report for longer than `stale_minutes`: the system is offline. |
| Note | Upgradable packages. Notes do not change the color of the card: on unstable there almost always are some. |

## hd-dashboard
```
hd-dashboard [--config <file>] [--database <file>] serve
hd-dashboard [--config <file>] [--database <file>] hosts
hd-dashboard [--config <file>] [--database <file>] forget <host>
```

`serve` runs the dashboard, as the service does. `hosts` lists the systems with their level, version and address, and when they last reported. `forget` removes a system, its token and its history. Run `hosts` and `forget` as root: the database is in `/var/lib/private`. They do not make the database; the service makes it on its first start.

`--config` reads the settings from another file than `/etc/hyprdebian/config.json`, and `--database` uses another database than `/var/lib/hd-dashboard/dashboard.db`. With both, the dashboard can be tried from this repository, without installing it:
```
echo '{"hd-dashboard": {"listen": "127.0.0.1", "port": 18080}}' > /tmp/dashboard.json
packages/hd-dashboard/hd-dashboard --config /tmp/dashboard.json --database /tmp/dashboard.db serve
```

It exits with 0 on success, 1 on an error and 2 on wrong arguments.

## Removing the package
Removing the package stops the service and keeps the settings and the database, so a new installation takes them over. Purging it also removes the key `hd-dashboard` of `/etc/hyprdebian/config.json` and the database. Firewall rules for the dashboard stay; remove them by hand.

## Tests
The tests are in `packages/hd-dashboard/tests`: accepting and refusing reports and tokens, the history, judging the systems, escaping report values in the pages, the settings, the server on a free local port, and `hd-dashboard forget`. They need nothing beyond Python:
```
cd packages/hd-dashboard
make test
```

`make test` in the root of this repository runs them with the tests of the other packages.

## Files
| Path | Purpose |
| --- | --- |
| `/usr/sbin/hd-dashboard` | The dashboard. |
| `/usr/lib/systemd/system/hd-dashboard.service` | Runs the dashboard. |
| `/usr/share/hd-dashboard/defaults.json` | The default settings. |
| `/etc/hyprdebian/config.json` | The settings, in the key `hd-dashboard`. |
| `/var/lib/hd-dashboard/dashboard.db` | The SQLite database, in `/var/lib/private/hd-dashboard`. |
