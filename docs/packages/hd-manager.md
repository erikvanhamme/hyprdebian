## Introduction
The package `hd-manager` makes a system a manager of the estate: it makes the jobs, signs them with the manager key and sends them to the dashboard, see [hd-dashboard](hd-dashboard.md#jobs), which hands them out to the agents of [hd-agent](hd-agent.md). An agent only runs a job that is signed by one of its `manager_keys`, see [ADR 0001](../adr/0001-jobs-signed-by-manager.md). The estate can have several managers; they share one sequence of job numbers.

`hd-manager` runs as the user, never as root. The manager key is a passphrase-protected Ed25519 key in `~/.ssh`, and the settings are those of the user, in `~/.config/hyprdebian/config.json`. Installing the package sets nothing up.

The package comes with the man page `man hd-manager`.

## Building the package
The package is built from `packages/hd-manager` in this repository:
```
cd packages/hd-manager
make deb
```

This builds `build/hd-manager_<version>_all.deb`, with the version from `version.json`. Raise the version there for every package that is handed out. It depends on `python3`, `openssh-client` and `hd-base` 1.93 or later, whose module `hyprdebian.job` signs the jobs, see [hd-base](hd-base.md#jobs), and suggests `hd-dashboard`. Run from this repository, it takes that module from `packages/hd-base/lib`. Install it with:
```
sudo apt install ./build/hd-manager_<version>_all.deb
```

The files involved are:
```
packages/hd-manager/Makefile
packages/hd-manager/version.json
packages/hd-manager/hd-manager
packages/hd-manager/defaults.json
packages/hd-manager/deb/control
packages/hd-manager/man/hd-manager.1
packages/hd-manager/tests/test_manager.py
```

## Setting up a manager
Make the manager key, which asks for its passphrase, and print its public key:
```
hd-manager key
```

Put the public key in `manager_keys` of every agent and of the dashboard; a job signed by any key on the list is valid. On each managed system and on the dashboard, with all the keys of the managers:
```
sudo hd-config-update hd-agent '{"url": "http://dash.example/", "manager_keys": ["ssh-ed25519 AAAA... hd-manager@beast"]}'
sudo hd-config-update hd-dashboard '{..., "manager_keys": ["ssh-ed25519 AAAA... hd-manager@beast"]}'
```

`hd-config-update` replaces the whole key, so give the other settings of it too.

## Settings
The settings are the key `hd-manager` of `~/.config/hyprdebian/config.json`; a setting that is missing has its default value, from `/usr/share/hd-manager/defaults.json`.

| Key | Default | |
| --- | --- | --- |
| `url` | `"http://dash.rsq-online.ddns.net/"` | The URL of the dashboard, `http://` or `https://`. |
| `key` | `"~/.ssh/hd-manager"` | The private manager key; its public key is next to it, with `.pub`. |

They are set without sudo:
```
hd-config-update hd-manager '{"url": "http://dash.example/"}'
```

## hd-manager
```
hd-manager key
hd-manager schedule upgrade <host>... | all [--at <time>] [--expires <time>]
hd-manager jobs [<host>]
```

It exits with 0 on success, 1 on an error and 2 on wrong arguments. It refuses to run as root.

### key
`key` makes the manager key with `ssh-keygen -t ed25519` when there is none, which asks for its passphrase, and prints the public key. When only the private key is there, `ssh-keygen -y` reads the public key from it, which asks for the passphrase too.

### schedule
`schedule` makes a job for each host, with the command `upgrade`, which runs `hd-up --unattended` on the system and never reboots, signs them all and sends them to the dashboard with `POST /jobs`. The dashboard stores them all or none.

- The hosts are those of `GET /api/hosts` of the dashboard. A host the dashboard does not know is refused, and so is a host whose last report is from `hd-report`, of format 1, which runs no jobs. `all` makes a job for every host whose last report is from `hd-agent`, of format 2, and leaves out the systems that still run `hd-report`.
- `--at` is when the job may run, its `not_before`; the default is now. `--expires` is when it expires if it has not run; the default is 24 hours after `--at`. A time is `HH:MM`, the next time the clock shows it, or a date with an optional time, such as `2026-11-19 03:00` or `2026-11-19`, in local time unless it has a time zone. A time in the past is refused.
- The jobs are signed in one run of `ssh-keygen -Y sign`, so the passphrase is asked for once for all of them; with the key in `ssh-agent`, not at all.

```
hd-manager schedule upgrade beast box --at 03:00
hd-manager schedule upgrade all --at "2026-11-21 02:00" --expires "2026-11-21 06:00"
```

#### Sequence numbers
Every job has a sequence number, one sequence for the estate, which only goes up: an agent never accepts a job whose number is not above that of the last one it accepted. `schedule` takes the next numbers above both the highest the dashboard has seen, from `GET /sequence`, and the highest this manager used, in `~/.local/state/hd-manager/sequence`. It stores the highest number it takes there before it sends the jobs, so a number is never used twice, even when the reply is lost.

When another manager took the numbers in the meantime, the dashboard refuses the jobs with 409; `schedule` then takes new numbers and signs again, which asks for the passphrase again, up to 3 times. When `~/.local/state/hd-manager/sequence` is lost, the numbers of the dashboard are enough; when the dashboard lost its jobs, those of the manager.

### jobs
`jobs` lists the jobs of `GET /api/jobs` of the dashboard, of all hosts or of one: their sequence number, host, command and state, `waiting`, `scheduled`, `ok`, `failed`, `expired` or `cancelled`, see [hd-dashboard](hd-dashboard.md#jobs), when they may run and expire, and when they started and ended.

## Tests
The tests are in `packages/hd-manager/tests`: the settings, the times of `--at` and `--expires`, picking the hosts, and scheduling against a local test dashboard, `hd-dashboard` of this repository on a free local port: for hosts and for `all`, a host with `hd-report`, a sequence number that another manager takes in the meantime, a lost counter and a counter above the dashboard, and listing the jobs. They sign the jobs with test keys that they make in a temporary directory, with a temporary `HOME`, and need nothing beyond Python and `ssh-keygen`:
```
cd packages/hd-manager
make test
```

`make test` in the root of this repository runs them with the tests of the other packages.

## Files
| Path | Purpose |
| --- | --- |
| `/usr/bin/hd-manager` | Makes the key, and schedules and lists the jobs. |
| `/usr/share/hd-manager/defaults.json` | The default settings. |
| `~/.config/hyprdebian/config.json` | The settings, in the key `hd-manager`. |
| `~/.ssh/hd-manager`, `~/.ssh/hd-manager.pub` | The manager key. |
| `~/.local/state/hd-manager/sequence` | The highest sequence number this manager used. |
