## Introduction
The package `hd-buildserver` turns a HyprDebian system into the build server of HyprDebian. Every night it checks the git repository, and when the branch has moved since the last build, it builds every package and the live ISO in a clean build environment and updates a package repository of [hd-repo](hd-repo.md) with them. Only the packages that are not in the repository yet are uploaded.

Installing the package sets everything up:
- the default settings, in the key `hd-buildserver` of `/etc/hyprdebian/config.json`, when there are none yet: the git repository on GitHub, the branch `main` and the package repository `repo`;
- the user `hd-buildserver`, in the group `docker`, with its home directory `/var/lib/hd-buildserver`;
- its ssh key, `/var/lib/hd-buildserver/.ssh/id_ed25519`, when there is none yet, and an ssh config for running unattended;
- `hd-buildserver.timer`, which is enabled and started.

The package comes with the man page `man hd-buildserver`.

## Building the package
The package is built from `packages/hd-buildserver` in this repository:
```
cd packages/hd-buildserver
make deb
```

This builds `build/hd-buildserver_<version>_all.deb`, with the version from `version.json`. Raise the version there for every package that is handed out. It depends on `python3`, `git`, `make`, `openssh-client`, `passwd`, `systemd`, `hd-docker` and `hd-base`. Install it with:
```
sudo apt install ./build/hd-buildserver_<version>_all.deb
```

The files involved are:
```
packages/hd-buildserver/Makefile
packages/hd-buildserver/version.json
packages/hd-buildserver/hd-buildserver
packages/hd-buildserver/defaults.json
packages/hd-buildserver/hd-buildserver.service
packages/hd-buildserver/hd-buildserver.timer
packages/hd-buildserver/deb/control
packages/hd-buildserver/deb/postinst
packages/hd-buildserver/deb/prerm
packages/hd-buildserver/deb/postrm
packages/hd-buildserver/man/hd-buildserver.8
buildenv/Dockerfile
```

## Setting up
The installation prints the public key of `hd-buildserver`. Add it to the `authorized_keys` of the user `repo` on the package repository, see [hd-repo](hd-repo.md#setting-up):
```
sudo cat /var/lib/hd-buildserver/.ssh/id_ed25519.pub
```

Then set the host name of the package repository, when it is not `repo`, and try a build:
```
sudo hd-config-update hd-buildserver '{"repo": "repo.example"}'
sudo systemctl start hd-buildserver.service
journalctl -fu hd-buildserver.service
```

The first build takes long: it pulls the images and compiles every package from its sources.

## A build
`hd-buildserver` runs these steps:
1. It updates the checkout in `/var/lib/hd-buildserver/checkout` from the git repository with `git fetch`, and clones it the first time.
2. When the head of the branch is the commit of the last build, in `/var/lib/hd-buildserver/last-built`, it stops there.
3. Otherwise it checks out the head and removes everything git does not track with `git clean -fdx`, also the downloads and the compiled releases that earlier builds left in `build/`, so every build starts from the sources alone. It builds the image `hd-buildserver-env` of `buildenv/Dockerfile` of the checkout, pulling `debian:trixie` first, and runs `make iso` in a container of it, which builds every package and the live ISO.
4. It runs `update-repo/update-repo.py` of the checkout with `--host <repo> --yes --only-new --iso livecd/out/hyprdebian-live-amd64.iso`, which uploads the packages that are not in the repository yet and the ISO, see [hd-repo](hd-repo.md#the-hyprdebian-packages). Then it stores the commit as the last build.

A build that fails leaves the last build as it was, so the next night tries the same commit again. Only one build runs at a time.

Packages of a version that is already in the repository are never uploaded again: a commit that changes a package without raising its version only reaches the repository with the next raise. The ISO is uploaded with every build.

### The build environment
`buildenv/Dockerfile` is in the git repository, not in the package, so a new package brings its build dependencies along: add them to the Dockerfile in the same change. It installs what [Build requirements](../HyprDebian/Build%20requirements.md) lists, on trixie, so what is compiled outside a container of its own, such as `blockwrite` of `hd-backup`, also runs on trixie.

The Rust packages, `hd-desktop`, `hd-audacious` and the live cd build in containers of their own. The build container gets the docker socket of the host for them, and the checkout is at the same path in the container as on the host, so the paths those builds hand to docker are right. With the docker socket, the build can do anything on the build server that root can: only build branches you trust.

## The timer
`hd-buildserver.timer` runs `hd-buildserver.service` every night at 3:00, with up to 30 minutes of random delay, and catches up on a night the system was off. The service runs `hd-buildserver` as the user `hd-buildserver`, and stops a build after 12 hours. The timer is enabled on the first installation, by its preset, see [hd-base](hd-base.md#services); after that, it stays as the admin left it. To stop the nightly builds:
```
sudo systemctl disable --now hd-buildserver.timer
```

When a build fails, `hd-buildserver.service` fails, and the output of the build is in the journal:
```
journalctl -u hd-buildserver.service
```

## Settings
The settings are the key `hd-buildserver` of `/etc/hyprdebian/config.json`. The installation stores the defaults there, from `/usr/share/hd-buildserver/defaults.json`, when the key is missing; a setting that is missing from the key has its default value. The next build uses a change.

| Key | Default | |
| --- | --- | --- |
| `git` | `"https://github.com/erikvanhamme/hyprdebian.git"` | The URL of the git repository. |
| `branch` | `"main"` | The branch to build. |
| `repo` | `"repo"` | The host name of the package repository. |

```
sudo hd-config-update hd-buildserver '{"repo": "repo.example"}'
```

## The ssh key
The uploads log in as `repo` on the package repository with `/var/lib/hd-buildserver/.ssh/id_ed25519`, which has no passphrase. The installation generates it when there is none and never replaces it. The ssh config next to it takes the host key of a host it has not seen yet without asking, and never asks for a password, so a build that cannot log in fails instead of waiting. A git repository that needs a login over ssh takes the same key.

## hd-buildserver
```
hd-buildserver [--force]
```

Run it as `hd-buildserver`, easiest by starting the service. With `--force`, it builds and uploads also when the branch has not moved:
```
sudo runuser -u hd-buildserver -- hd-buildserver --force
```

It exits with 0 when the build was uploaded or there was nothing to build, 1 on an error and 2 on wrong arguments.

## Removing the package
Removing the package stops the timer and keeps the settings, the user, the checkout and the ssh key. Purging it also removes the key `hd-buildserver` of `/etc/hyprdebian/config.json`, the user `hd-buildserver` and `/var/lib/hd-buildserver`, with the checkout and the ssh key. The docker images stay; `docker image prune -a` removes them.

## Files
| Path | Purpose |
| --- | --- |
| `/usr/bin/hd-buildserver` | Builds and uploads. |
| `/usr/lib/systemd/system/hd-buildserver.service` | Runs `hd-buildserver` as `hd-buildserver`. |
| `/usr/lib/systemd/system/hd-buildserver.timer` | Runs the service every night. |
| `/usr/share/hd-buildserver/defaults.json` | The default settings. |
| `/etc/hyprdebian/config.json` | The settings, in the key `hd-buildserver`. |
| `/var/lib/hd-buildserver/checkout` | The checkout of the git repository. |
| `/var/lib/hd-buildserver/last-built` | The commit of the last build that was uploaded. |
| `/var/lib/hd-buildserver/build.lock` | Keeps two builds from running at the same time. |
| `/var/lib/hd-buildserver/.ssh` | The ssh key and the ssh config. |
