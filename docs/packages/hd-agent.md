## Introduction
The package `hd-agent` sends the state of a hyprdebian system to the hyprdebian dashboard every 15 minutes, see [hd-dashboard](hd-dashboard.md). The system only makes outgoing connections, so it needs no open port for it. The reply of the dashboard has the jobs for the system, made by [hd-manager](hd-manager.md), which `hd-agent` accepts and runs at their time, see [Jobs](#jobs).

Installing the package sets everything up:
- the default settings, in the key `hd-agent` of `/etc/hyprdebian/config.json`, when there are none yet: the URL of the dashboard, `http://dash.rsq-online.ddns.net/`, and no `manager_keys`, so no job runs;
- the token of the system in `/etc/hyprdebian/hd-agent/token`, when there is none yet;
- `hd-agent.timer`, which is enabled and started.

The timer starts right away, and as the system has been up for more than 2 minutes, the first report goes out within a minute of the installation. Installed by the installer, without a running systemd, the first report goes out 2 minutes after the first boot.

It replaces `hd-report`, see [Switching from hd-report](#switching-from-hd-report).

The package comes with the man page `man hd-agent`.

## Building the package
The package is built from `packages/hd-agent` in this repository:
```
cd packages/hd-agent
make deb
```

This builds `build/hd-agent_<version>_all.deb`, with the version from `version.json`. Raise the version there for every package that is handed out. It depends on `python3`, `systemd` and `hd-base` 1.91 or later, whose module `hyprdebian.job` checks the signatures of the jobs, see [hd-base](hd-base.md#jobs), and suggests `hd-dashboard`. On a system with `hd-base` before 1.1, which kept services from being enabled on their installation, enable `hd-agent.timer` by hand after installing it: `sudo systemctl enable --now hd-agent.timer`. Install it with:
```
sudo apt install ./build/hd-agent_<version>_all.deb
```

Run from this repository, `hd-agent` takes the module `hyprdebian.job` from `packages/hd-base/lib`.

The files involved are:
```
packages/hd-agent/Makefile
packages/hd-agent/version.json
packages/hd-agent/hd-agent
packages/hd-agent/defaults.json
packages/hd-agent/hd-agent.service
packages/hd-agent/hd-agent.timer
packages/hd-agent/deb/control
packages/hd-agent/deb/postinst
packages/hd-agent/deb/prerm
packages/hd-agent/deb/postrm
packages/hd-agent/man/hd-agent.8
packages/hd-agent/tests/test_agent.py
```

## Switching from hd-report
`hd-agent` replaces `hd-report`; the systems are switched by hand, one by one. The package conflicts with `hd-report`, replaces it and provides it, so installing it removes `hd-report`:
```
sudo apt install hd-agent
```

On its first installation, it takes over what `hd-report` leaves behind:
- the token in `/etc/hyprdebian/hd-report/token`, which moves to `/etc/hyprdebian/hd-agent/token`, so the dashboard keeps accepting the reports of the system;
- the key `hd-report` of `/etc/hyprdebian/config.json`, which becomes the key `hd-agent`, unless that one is there already;
- the enabled state of `hd-report.timer`: `hd-agent.timer` is only enabled when `hd-report.timer` was.

The report stays the same, so the dashboard shows the system as before. It also accepts the reports of the systems that still run `hd-report`.

## The timer
`hd-agent.timer` runs `hd-agent.service` 2 minutes after the boot and then every 15 minutes, with up to a minute of random delay. The service runs `hd-agent` as root. The jobs do not run in the service, but in transient units of their own, see [Jobs](#jobs), so stopping the timer leaves a running job alone. The timer is enabled on the first installation, as Debian enables services, by its preset, see [hd-base](hd-base.md#services); after that, it stays as the admin left it, also over upgrades. To stop sending reports:
```
sudo systemctl disable --now hd-agent.timer
```

When the dashboard cannot be reached or refuses the report, `hd-agent.service` fails, and the error is in the journal:
```
journalctl -u hd-agent.service
```

## Settings
The settings are the key `hd-agent` of `/etc/hyprdebian/config.json`. The installation stores the defaults there, from `/usr/share/hd-agent/defaults.json`, when the key is missing; a setting that is missing from the key has its default value. The next report uses a change.

| Key | Default | |
| --- | --- | --- |
| `url` | `"http://dash.rsq-online.ddns.net/"` | The URL of the dashboard, `http://` or `https://`. The report goes to `<url>/report`. |
| `manager_keys` | `[]` | The public keys of the managers, as `hd-manager key` prints them: Ed25519 keys, one per string. A job must be signed by one of them. Without keys the agent sends its reports, but accepts no jobs. |

```
sudo hd-config-update hd-agent '{"url": "http://dash.example/", "manager_keys": ["ssh-ed25519 AAAA... hd-manager@beast"]}'
```

`hd-agent run` reads `manager_keys` again, so a job whose key is taken off the list no longer runs.

## The token
The token of the system is in `/etc/hyprdebian/hd-agent/token`, 64 hexadecimal digits, readable by root only. It is not in `config.json`, which every user can read. The installation generates it when there is none, and never replaces it: the dashboard accepts the first token it sees for a host name, and only that token afterwards. A purge keeps it too, so a new installation takes it over.

When the token is lost, as on a reinstalled system, the dashboard refuses the reports with 403. Forget the host on the dashboard, and the next report registers the new token:
```
sudo hd-dashboard forget <host>
```

## hd-agent
```
hd-agent [--print]
hd-agent run <sequence>
```

`hd-agent` brings the stored jobs up to date, collects the report and POSTs it as JSON to `<url>/report`, with the header `Authorization: Bearer <token>`, and accepts the jobs in the reply, see [Jobs](#jobs). Run it as root. With `--print`, it only prints the report, without sending it; that also works as a normal user, with some parts missing, the jobs among them:
```
sudo hd-agent --print
```

It exits with 0 when the report was sent or printed, 1 on an error and 2 on wrong arguments. A job that is ignored is no error.

`hd-agent run <sequence>` runs a stored job; its timer does that, see [Jobs](#jobs). It exits with 0 when the job ended ok, and 1 when it did not run or did not end ok.

### What a report holds
The layout is described in [hd-dashboard](hd-dashboard.md#reports); `hd-agent` fills it in like this:

| Part | Source |
| --- | --- |
| Host name, FQDN | The host name, by which the dashboard knows the system, and the FQDN from the name resolution, empty when the system has none. |
| HyprDebian version, suite | `/etc/hyprdebian/version.json` of `hd-base`, and the suite in `/etc/apt/sources.list.d/hyprdebian.sources`. |
| Debian version, kernel | `/etc/debian_version`, the running kernel, and the newest kernel in `/boot`, the one whose `vmlinuz` was modified last, as `hd-reboot-check` picks it. |
| Secure Boot | The EFI variables `SecureBoot` and `SetupMode` in `/sys/firmware/efi/efivars`, read as `hd-secboot --status` reads them, see [hd-secboot](hd-secboot.md#--status). |
| Boot time, load, CPUs, memory | `/proc`. |
| Pools | `zpool list`, and the last completed scrub from the scan line of `zpool status`: the bytes it repaired, the errors it found and when it ended. A scrub that is running, paused or was canceled, and a resilver, leave it out. |
| Arrays | The mdadm arrays in `/sys/block`, named as in `/dev/md`. |
| Units | `systemctl is-system-running` and the failed units. |
| Updates | The packages `apt full-upgrade` would upgrade, from the package lists apt has now (`hd-agent` does not run `apt update`), when the lists were updated, the last upgrade in the apt history, and whether a reboot is pending, as `hd-reboot-check` of hd-base finds it: a newer kernel, a new ZFS module, or `/run/reboot-required`. |
| Snapshot | The newest `hd-up-` snapshot of `rpool/hyprdebian`, see `hd-up` in [hd-base](hd-base.md). |
| Timers | The enabled timers in `/etc/systemd/system`, those of the admin, and the `hd-*` timers of the packages in `/usr/lib/systemd/system`, with the last and next run and the result of their service. `hd-agent.timer` itself is left out. |
| Jobs | The stored jobs, see [Jobs](#jobs): those that have not ended as `scheduled`, also a running one, and the ended ones with their result, start and end time. |

The report is of `FORMAT` 2, which `hd-dashboard` knows from version 0.11 on; update the dashboard before the systems.

## Jobs
A job is one command for one system, to run once, not before `not_before` and not after `expires`, signed by a manager with [hd-manager](hd-manager.md); its format is in [hd-base](hd-base.md#jobs). The dashboard hands out the jobs of the system in the reply to its report, and `hd-agent` accepts a job when:
- its signature verifies against one of `manager_keys`;
- its host is the host name of this system;
- its sequence number is higher than that of the last job it accepted. That is checked on the acceptance, not on the run, as the jobs may run in another order.

It ignores any other job; a job it accepted before silently, the others with a line in the journal. Without `manager_keys` it accepts no jobs.

An accepted job is stored, signature included, in `/var/lib/hd-agent/jobs/<sequence>.json`, as `scheduled`, and the highest sequence number accepted in `/var/lib/hd-agent/sequence`. A newer job with the same command cancels a job that is still scheduled; a running job is never cancelled.

The jobs run in transient units of `systemd-run`: the timer `hd-agent-job-<sequence>.timer` starts the service `hd-agent-job-<sequence>.service` at `not_before`, which runs `hd-agent run <sequence>`. A reboot loses the transient timers, so every run of `hd-agent`, also the first one 2 minutes after the boot, makes them again for the stored jobs that are scheduled, before it sends the report; also when the dashboard cannot be reached. A job whose `not_before` passed, as while the system was off, runs at once, and one whose expiry passed is `expired`. A job that was running when the system went down, as by a power cut, `failed`.

`hd-agent run` checks the signature and the expiry again, runs the command, and stores the state, the start and the end time:

| Command | Runs |
| --- | --- |
| `upgrade` | `hd-up --unattended`, see [hd-base](hd-base.md). It never reboots. |

| State | |
| --- | --- |
| `scheduled` | Accepted, its timer set, not ended. A job that runs is reported as scheduled too. |
| `ok` | Ran and ended with 0. |
| `failed` | Ran and did not end with 0, its run was broken off, or its signature no longer verifies on the run. |
| `expired` | Its expiry passed before it could run. |
| `cancelled` | Dropped while scheduled, for a newer job with the same command. |

The output of a job is in the journal, on the host:
```
journalctl -u hd-agent-job-<sequence>.service
```

The report has every stored job, see [What a report holds](#what-a-report-holds). The reply has in `open` the jobs the dashboard has no end for; an ended job is removed from the store once it is no longer in `open`, so the dashboard learns every end, also of a job that is accepted and ends between two reports.

## Removing the package
Removing the package stops the timer and the timers of the jobs, and keeps the settings, the token and the stored jobs; a job that is running is left alone. Purging it also removes the key `hd-agent` of `/etc/hyprdebian/config.json` and the stored jobs in `/var/lib/hd-agent`; the token stays.

## Tests
The tests are in `packages/hd-agent/tests`: parsing what the system tools print, the version and suite, the scrubs of the pools, the state of Secure Boot, the apt history with rotated logs, the settings, a report of the system the tests run on, which `hd-dashboard` must accept, and sending a report to a local test server. For the jobs: accepting them and each refusal, cancelling, expiry, a job whose time passed, the timers made again after a reboot, a run that was broken off, running a job and checking it again, the jobs in the report and their removal, and the states the dashboard learns from the reports. They sign the jobs with test keys that they make in a temporary directory, keep the store in one too and fake the transient units, so they need nothing beyond Python and `ssh-keygen`, and run as a normal user:
```
cd packages/hd-agent
make test
```

`make test` in the root of this repository runs them with the tests of the other packages.

## Files
| Path | Purpose |
| --- | --- |
| `/usr/sbin/hd-agent` | Collects and sends the report. |
| `/usr/lib/systemd/system/hd-agent.service` | Runs `hd-agent` as root. |
| `/usr/lib/systemd/system/hd-agent.timer` | Runs the service every 15 minutes. |
| `/usr/share/hd-agent/defaults.json` | The default settings. |
| `/etc/hyprdebian/config.json` | The settings, in the key `hd-agent`. |
| `/etc/hyprdebian/hd-agent/token` | The token of the system, mode 0600. |
| `/var/lib/hd-agent/jobs/<sequence>.json` | A stored job, with its state. |
| `/var/lib/hd-agent/sequence` | The highest sequence number accepted. |
