## Introduction
The hyprdebian dashboard shows the state of every hyprdebian system on one page. Each system sends a status report to the dashboard every 15 minutes. The systems only make outgoing connections, so no system needs an open port for it.

The dashboard runs on its own Debian VM. It keeps the latest report of every system, and a 30-day history of the load, memory and fullest pool.

The files involved are:
```
deploy/usr/local/bin/hd-report                    the reporter, on every system
deploy/etc/systemd/system/hd-report.service
deploy/etc/systemd/system/hd-report.timer
templates/etc/hyprdebian/report.conf.j2           the dashboard URL and the token of the system
dashboard/hd-dash                                 the dashboard
dashboard/scripts/setup_hyprdebian_dash.sh        sets up the dashboard VM
```

## On the systems
The installer asks `Send status reports to the hyprdebian dashboard` (`Q_REPORT`, default yes) and the dashboard URL (`Q_REPORT_URL`, default `http://dash.rsq-online.ddns.net/`). On yes, `o_report`:
- generates a random token, `REPORT_TOKEN`, saved in the installer config so a rerun of the installer keeps it,
- deploys `hd-report`, `hd-report.service` and `hd-report.timer`, and enables the timer,
- renders `/etc/hyprdebian/report.conf`, mode `0600`, with the URL and the token.

The timer starts after the first boot of the installed system.

`hd-report.timer` runs `hd-report` 2 minutes after boot and then every 15 minutes, with up to a minute of random delay. `hd-report` runs as root, and POSTs a JSON report to `<url>/report`, with `Authorization: Bearer <token>`. When the dashboard cannot be reached or refuses the report, `hd-report.service` fails, and the error is in the journal:
```
journalctl -u hd-report.service
```

To see the report without sending it:
```
sudo hd-report --print
```

### What a report holds
| Part | Source |
| --- | --- |
| Host name, FQDN | The host name, and `Q_FQDN` in the installer config. The machine id changes on every boot, see `rotate-machine-id.service`, so the dashboard knows a system by its host name. |
| Installer version, type, suite | `INSTALLED_VERSION`, `Q_TYPE` and `Q_SUITE` in `/etc/hyprdebian/installer.config`. |
| Debian version, kernel | `/etc/debian_version`, the running kernel, and the newest kernel in `/boot`. |
| Boot time, load, CPUs, memory, swap | `/proc`. |
| Pools | `zpool list` and the scan line of `zpool status`. |
| Arrays | The mdadm arrays in `/sys/block`, named as in `/dev/md`. |
| Units | `systemctl is-system-running` and the failed units. |
| Updates | The packages `apt full-upgrade` would upgrade, from the package lists apt has now (`hd-report` does not run `apt update`), when the lists were updated, the last apt run and the last upgrade in the apt history, and `/run/reboot-required`. |
| Snapshot | The newest `hd-up-` or `upgrade-` snapshot of `rpool/hyprdebian`. |
| Timers | The enabled timers in `/etc/systemd/system`, like `backup-vms.timer`, with the last and next run and the result of their service. |

Every report has a `format` number. Raise `FORMAT` in both `hd-report` and `hd-dash` when the layout changes in a way the dashboard must know about. The dashboard refuses reports with a format newer than its own, so update the dashboard first.

## The dashboard
`hd-dash` is a single Python program without dependencies beyond Python itself. It answers:

| Path | |
| --- | --- |
| `POST /report` | Stores a report. |
| `GET /` | The overview: a card per system, the worst first. Refreshes every minute. |
| `GET /host/<name>` | One system, with the 30-day charts and its last report. |
| `GET /api/hosts` | The systems, their reports and issues as JSON. |

### Tokens
The connection is plain HTTP, meant for the local network. The first report of a host name registers its token. Later reports of that host name must have the same token, otherwise they are refused with 403. The dashboard only stores a hash of the token.

A reinstalled system has a new token. Forget the old one on the dashboard VM, and the next report registers the new one:
```
sudo hd-dash forget <host>
```

`sudo hd-dash hosts` lists the systems and when they last reported.

### Issues
Each system is judged on its last report. The card gets the color of its worst issue.

| Level | Issue |
| --- | --- |
| Error | No report for longer than `STALE_MINUTES` (45). |
| Error | Failed units. |
| Error | A pool that is not `ONLINE`, or 90% full or more. |
| Error | A degraded mdadm array. |
| Error | The last run of a timer service failed, like `backup-vms.service`. |
| Warning | A pool that is 80% full or more. |
| Warning | The system is starting or stopping. |
| Warning | A reboot is pending: `/run/reboot-required`, or a newer kernel in `/boot` than the running one. |
| Warning | No upgrade for 14 days, or never. |
| Warning | An installer version older than the newest version among the systems. |
| Note | Upgradable packages. Notes do not change the color of the card: on unstable there almost always are some. |

## Setting up the dashboard VM
On a fresh Debian trixie VM, clone this repo, then run from `dashboard/scripts`:
```
sudo ./setup_hyprdebian_dash.sh
```

The script installs python3 and `hd-dash` in `/usr/local/bin`, writes `/etc/hyprdebian-dash.conf` and `hd-dash.service`, and starts the service. When ufw is active, it opens port 80. Running it again installs the new `hd-dash`, and keeps the config and the database. Run it again after changing `hd-dash`.

The service runs as a user that systemd makes up (`DynamicUser`), and may bind to port 80 without being root. The database is `/var/lib/hyprdebian-dash/dash.db`.

Point the DNS name of `Q_REPORT_URL`, `dash.rsq-online.ddns.net` by default, at the VM.

### Settings
`/etc/hyprdebian-dash.conf`, restart `hd-dash.service` after a change:

| Key | Default | |
| --- | --- | --- |
| `LISTEN` | `0.0.0.0` | Address to listen on. |
| `PORT` | `80` | Port to listen on. |
| `DATABASE` | `/var/lib/hyprdebian-dash/dash.db` | The SQLite database. |
| `STALE_MINUTES` | `45` | A system that did not report for this long is an error. |
| `HISTORY_DAYS` | `30` | How long the history is kept. |

## Trying it locally
`hd-dash` reads its settings from the file in `$HD_DASH_CONFIG` when it is set:
```
printf 'LISTEN=127.0.0.1\nPORT=18080\nDATABASE=/tmp/dash.db\n' > /tmp/dash.conf
HD_DASH_CONFIG=/tmp/dash.conf dashboard/hd-dash serve
```
