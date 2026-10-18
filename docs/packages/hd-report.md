## Introduction
The package `hd-report` sends the state of a hyprdebian system to the hyprdebian dashboard every 15 minutes, see [hd-dashboard](hd-dashboard.md). The system only makes outgoing connections, so it needs no open port for it.

Installing the package sets everything up:
- the default settings, in the key `hd-report` of `/etc/hyprdebian/config.json`, when there are none yet: the URL of the dashboard, `http://dash.rsq-online.ddns.net/`;
- the token of the system in `/etc/hyprdebian/hd-report/token`, when there is none yet;
- `hd-report.timer`, which is enabled and started.

The timer starts right away, and as the system has been up for more than 2 minutes, the first report goes out within a minute of the installation. Installed by the installer, without a running systemd, the first report goes out 2 minutes after the first boot.

The package comes with the man page `man hd-report`.

## Building the package
The package is built from `packages/hd-report` in this repository:
```
cd packages/hd-report
make deb
```

This builds `build/hd-report_<version>_all.deb`, with the version from `version.json`. Raise the version there for every package that is handed out. It depends on `python3`, `systemd` and `hd-config`, and suggests `hd-dashboard`. On a system with `hd-base` before 1.1, which kept services from being enabled on their installation, enable `hd-report.timer` by hand after installing it: `sudo systemctl enable --now hd-report.timer`. Install it with:
```
sudo apt install ./build/hd-report_<version>_all.deb
```

The files involved are:
```
packages/hd-report/Makefile
packages/hd-report/version.json
packages/hd-report/hd-report
packages/hd-report/defaults.json
packages/hd-report/hd-report.service
packages/hd-report/hd-report.timer
packages/hd-report/deb/control
packages/hd-report/deb/postinst
packages/hd-report/deb/prerm
packages/hd-report/deb/postrm
packages/hd-report/man/hd-report.8
packages/hd-report/tests/test_report.py
```

## The timer
`hd-report.timer` runs `hd-report.service` 2 minutes after the boot and then every 15 minutes, with up to a minute of random delay. The service runs `hd-report` as root. The timer is enabled on the first installation, as Debian enables services, by its preset, see [hd-base](hd-base.md#services); after that, it stays as the admin left it, also over upgrades. To stop sending reports:
```
sudo systemctl disable --now hd-report.timer
```

When the dashboard cannot be reached or refuses the report, `hd-report.service` fails, and the error is in the journal:
```
journalctl -u hd-report.service
```

## Settings
The settings are the key `hd-report` of `/etc/hyprdebian/config.json`. The installation stores the defaults there, from `/usr/share/hd-report/defaults.json`, when the key is missing; a setting that is missing from the key has its default value. The next report uses a change.

| Key | Default | |
| --- | --- | --- |
| `url` | `"http://dash.rsq-online.ddns.net/"` | The URL of the dashboard, `http://` or `https://`. The report goes to `<url>/report`. |

```
sudo hd-config-update hd-report '{"url": "http://dash.example/"}'
```

## The token
The token of the system is in `/etc/hyprdebian/hd-report/token`, 64 hexadecimal digits, readable by root only. It is not in `config.json`, which every user can read. The installation generates it when there is none, and never replaces it: the dashboard accepts the first token it sees for a host name, and only that token afterwards. A purge keeps it too, so a new installation takes it over.

When the token is lost, as on a reinstalled system, the dashboard refuses the reports with 403. Forget the host on the dashboard, and the next report registers the new token:
```
sudo hd-dashboard forget <host>
```

## hd-report
```
hd-report [--print]
```

`hd-report` collects the report and POSTs it as JSON to `<url>/report`, with the header `Authorization: Bearer <token>`. Run it as root. With `--print`, it only prints the report, without sending it; that also works as a normal user, with some parts missing:
```
sudo hd-report --print
```

It exits with 0 when the report was sent or printed, 1 on an error and 2 on wrong arguments.

### What a report holds
The layout is described in [hd-dashboard](hd-dashboard.md#reports); `hd-report` fills it in like this:

| Part | Source |
| --- | --- |
| Host name, FQDN | The host name, by which the dashboard knows the system, and the FQDN from the name resolution, empty when the system has none. |
| HyprDebian version, suite | `/etc/hyprdebian/version.json` of `hd-base`, and the suite in `/etc/apt/sources.list.d/hyprdebian.sources`. |
| Debian version, kernel | `/etc/debian_version`, the running kernel, and the newest kernel in `/boot`. |
| Boot time, load, CPUs, memory, swap | `/proc`. |
| Pools | `zpool list` and the scan line of `zpool status`. |
| Arrays | The mdadm arrays in `/sys/block`, named as in `/dev/md`. |
| Units | `systemctl is-system-running` and the failed units. |
| Updates | The packages `apt full-upgrade` would upgrade, from the package lists apt has now (`hd-report` does not run `apt update`), when the lists were updated, the last apt run and the last upgrade in the apt history, and `/run/reboot-required`. |
| Snapshot | The newest `hd-up-` snapshot of `rpool/hyprdebian`, see `hd-up` in [hd-base](hd-base.md). |
| Timers | The enabled timers in `/etc/systemd/system`, those of the admin, and the `hd-*` timers of the packages in `/usr/lib/systemd/system`, with the last and next run and the result of their service. `hd-report.timer` itself is left out. |

## Removing the package
Removing the package stops the timer and keeps the settings and the token. Purging it also removes the key `hd-report` of `/etc/hyprdebian/config.json`; the token stays.

## Tests
The tests are in `packages/hd-report/tests`: parsing what the system tools print, the version and suite, the apt history with rotated logs, the settings, a report of the system the tests run on, which `hd-dashboard` must accept, and sending a report to a local test server. They run as a normal user:
```
cd packages/hd-report
make test
```

`make test` in the root of this repository runs them with the tests of the other packages.

## Files
| Path | Purpose |
| --- | --- |
| `/usr/sbin/hd-report` | Collects and sends the report. |
| `/usr/lib/systemd/system/hd-report.service` | Runs `hd-report` as root. |
| `/usr/lib/systemd/system/hd-report.timer` | Runs the service every 15 minutes. |
| `/usr/share/hd-report/defaults.json` | The default settings. |
| `/etc/hyprdebian/config.json` | The settings, in the key `hd-report`. |
| `/etc/hyprdebian/hd-report/token` | The token of the system, mode 0600. |
