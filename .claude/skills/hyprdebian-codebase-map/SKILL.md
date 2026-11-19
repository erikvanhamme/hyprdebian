---
name: hyprdebian-codebase-map
description: Map of the HyprDebian repository - every package directory and part, what it does, its files, docs and tests, which files a kind of change touches, and what else to update with it. Use at the start of any task that changes HyprDebian code, to find where to look.
---

# Map of the HyprDebian repository

HyprDebian is a Debian distribution built from packages; this repository holds their sources, the
installer, the live ISO and the build tools. Use the words of `GLOSSARY.md`: a **package
directory** in `packages/` builds one or more **packages**, and a **part** is a piece of a package
with a page of its own. This map routes: it says where to look. What a package does in detail is in
its doc page, `docs/packages/<name>.md`, and its man pages, `packages/<dir>/man/`; read those for
the area you change, and the ADRs in `docs/adr/` that touch it.

## The repository

| Path | What it is |
| --- | --- |
| `packages/` | The package directories, below. |
| `Makefile` | Builds every package directory into `build/` (`make all`, `make <dir>`, `make kernel`, `make iso`, `make test`, `make update-repo`). Its header explains each target. |
| `version.json` | The version of HyprDebian, also of `hd-base` and `hd-desktop`. |
| `buildenv/Dockerfile` | The clean build environment of `hd-buildserver`: everything `make iso` needs. |
| `livecd/` | The live ISO: a trixie live system with `hd-installer` and every package in a local repository. `config/package-lists/`, `config/hooks/`, `config/includes.chroot/`. Doc: `docs/Livecd/Overview.md`. |
| `update-repo/update-repo.py` | Uploads the built packages to an `hd-repo` host (`make update-repo`, `make clean-repo`). |
| `vm/` | Provisions vms of the owner in libvirt from the live ISO (`make provision-vm`, `make switch-vm-gpu`). Doc: `docs/VM/Provisioning.md`. |
| `local-testvm/` | The tools of the test vm; see the skill `hyprdebian-test-vm`. |
| `docs/` | `packages/` (a page per package and part), `HyprDebian/` (About, Key Bindings, Backup tool, Build requirements), `adr/`, `agents/`. An Obsidian vault. |
| `.scratch/` | The issues, see `docs/agents/issue-tracker.md`. |

## What a package directory holds

Most follow one layout; look for these first:

- `Makefile` with the targets `deb`, `deb-path`, `clean` and, with tests, `test`. Its header lists
  what the package installs and where; it is the quickest way to find the source of an installed file.
- `deb/control` and the maintainer scripts `deb/postinst`, `prerm`, `postrm`, `preinst`, `triggers`.
- `deploy.json` (the repos: trixie, unstable or both) and `version.json` (the version of the
  package; none in `hd-base` and `hd-desktop`).
- The commands at the top level, named as installed (`hd-up`, not `hd-up.py`); `lib/` for the
  Python modules they share.
- `templates/*.j2`, rendered with jinja2; `etc/` and `usr/` mirror the installed tree.
- `defaults.json`: the default settings, written under the key of the package in
  `/etc/hyprdebian/config.json` (see the part `hd-config`).
- `tests/test_*.py`: Python `unittest`, run by `make test` in the directory.
- `.gitignore` with `build`; `build/` is the output.

Python is the language of the commands, bash of the installer and some desktop helpers, C++23 of
`hd-menu`, `hd-message` and the bootloader, C of `blockwrite`.

## The package directories

### Every system

| Directory | Builds | What it does | Tests |
| --- | --- | --- | --- |
| `hd-base` | `hd-base` | The base system: depends on what every system has, apt and dpkg settings (no recommends, blocked packages, excludes), `hd-up` with its timer and ZFS snapshot, `hd-reboot-check`, `hd-reboot`, `hd-poweroff`, `hd-issue` and the motd, `hd-link-repo`, `hd-firmware`, `hd-harden` (ssh, sysctl, modprobe, ufw), `hd-sync-skel`, the ZFS scrub and trim timers, zram. Holds the parts `hd-config`, `hd-network` and `hd-secboot`, and the Python package `hyprdebian` (`lib/hyprdebian/job.py`: the jobs of the estate). | yes |
| `hd-installer` | `hd-installer` | Installs HyprDebian from a live system: `hd-installer` (bash) runs a tree of tasks in phases, `scripts/*.sh`; `questions.sh` asks, `defaults.sh` and `answers.sh` with `answers.py` take the answers, `disk`, `partition`, `filesystem`, `base`, `packages`, `user`, `cleanup` do the work; `templates/etc/` are the files rendered into the new system with `render.py`, `deploy/etc/` those copied as they are. `examples/` holds an answers file. | no |
| `hd-bootloader` | `hd-bootloader-gfx`, `hd-bootloader-txt` | The UEFI bootloader that boots the UKIs from the ESPs. `bootloader.cpp` is the core, `graphical.cpp` and `text.cpp` the frontends, `assets.cpp` the stub of `assets.efi`; the Python scripts build the assets (`graphical.py`, `text.py`, `menubox.py`, `text2png.py`, `png2bin.py`), the UKIs (`mkuki.py`) and write the ESPs (`update-bootloader.py`, `update-esps`). `deb/etc/` holds the kernel and initramfs hooks, `deb/gfx/` and `deb/txt/` the control and defaults of each package. | no |
| `hd-kernel` | `hd-kernel`, `zfs-modules-*`, `linux-image-*` per suite | The Debian kernel with a ZFS module built against it and signed with a key thrown away after the build. `build-kernel.sh` does the work in the container of `buildenv/`, `kernel.mk` runs it for all four kernel directories. | no |
| `hd-kernel-vm`, `hd-kernel-server`, `hd-kernel-desktop` | the same, per kind of system | The kernel of `hd-kernel` with only the modules in `keep-modules`. | no |

### The desktop

| Directory | Builds | What it does | Tests |
| --- | --- | --- | --- |
| `hd-desktop` | `hd-desktop` | Depends on the desktop packages and boots into Hyprland with greetd (`templates/greetd.toml.j2`, `hd-desktop-render`). `etc/skel/.config/` holds the configs new users get: `hypr/hyprland.lua` (key bindings), `hypridle.conf`, `hyprlock.conf`, kitty, mako, qt5ct/qt6ct, gtk. The commands of the key bindings: `hd-ask-reboot`, `hd-ask-poweroff`, `hd-shutdown`, `hd-mode-select`, `hd-mount`, `hd-unmount` with `hd-mount-helper` and `etc/sudoers.d/`, `hd-volume-*`, `hd-launcher-cache` (the launcher; `etc/apt/apt.conf.d/90hd-launcher` refreshes it), `hd-reboot-notify` with its user units. `lib/hd_display.py`: the scale on the monitor. Holds the parts `hd-art`, `hd-fonts`, `hd-menu`, `hd-message`, `hd-theme`, `hd-screensaver` and `hd-plymouth`. The C++ programs build in a container (`common/build.sh`). | no |
| `hd-laptop` | `hd-laptop` | `hd-battery` and its user unit `hd-battery-monitor.service`. | no |
| `hd-screensavers` | `hd-screensavers` | Optional screensavers, C++23 with Vulkan on a wlr-layer-shell surface per output, built in a container (`common/build.sh`). `common/src/`: what they share, the outputs, surfaces and input (`screen.cpp`), the Vulkan device (`gpu.cpp`), a swapchain per surface (`swapchain.cpp`), mapped buffers (`buffer.cpp`), the matrices of OpenGL 1.x (`matrix.hpp`), the options from the configuration and the command line (`options.cpp`, with `json.cpp`), the options `framerate` and `msaa` of every screensaver (`render.cpp`); `common/program.mk` compiles each, GLSL shaders included. Holds the part `hd-pipes`. `tests/test_*.cpp`: C++ tests of the parts without Wayland or Vulkan, compiled with g++ on the host by `make test`. | yes |
| `hd-audacious` | `hd-audacious` | Audacious with a terminal interface, built in a container from the forks of the owner (`build-hd-audacious.sh`); `audacious` is the wrapper installed as `/usr/bin/audacious`. | no |

### Optional system support

| Directory | Builds | What it does | Tests |
| --- | --- | --- | --- |
| `hd-backup` | `hd-backup` | Backups to drives and LTO tape: `hd-backup`, `hd-backup-prep`, `hd-backup-load`, `hd-backup-unload`, `hd-backup-vm`, `hd-backup-vms` with its timer, `hd-restore-vm`, and `blockwrite.c`. Doc also `docs/HyprDebian/Backup tool.md`. | yes |
| `hd-docker` | `hd-docker` | Docker with ufw: `deb/etc/docker/daemon.json`, `docker-ufw.rules`. | no |
| `hd-qemu-kvm` | `hd-qemu-kvm` | Qemu/KVM and libvirt on the bridge `br0`: `libvirt/` (configs diverted over those of libvirt), the bridge netfilter files, debconf `deb/templates`. | no |
| `hd-cups` | `hd-cups` | CUPS, no code: only `deb/control`. | no |

### The estate

`hd-agent`, `hd-dashboard` and `hd-manager` share the jobs in `hd-base/lib/hyprdebian/job.py`;
read `docs/adr/0001-jobs-signed-by-manager.md` and the estate words of `GLOSSARY.md` first. A change
to the report or the jobs touches both the sender and the receiver.

| Directory | Builds | What it does | Tests |
| --- | --- | --- | --- |
| `hd-agent` | `hd-agent` | Sends the report every 15 minutes (`hd-agent.timer`), accepts the jobs in the reply and runs them with transient timers. Replaces `hd-report`. | yes |
| `hd-dashboard` | `hd-dashboard` | The central server: keeps the reports, shows the page, relays the jobs (`POST /jobs`, `GET /sequence`, `/api/*`). | yes |
| `hd-manager` | `hd-manager` | Makes, signs and sends the jobs, as the user. | yes |

### Infrastructure

| Directory | Builds | What it does | Tests |
| --- | --- | --- | --- |
| `hd-repo` | `hd-repo` | The package repository: nginx, the inboxes, the signing key, `hd-repo-update`. | no |
| `hd-buildserver` | `hd-buildserver` | Builds every package and the ISO each night in `buildenv/` and updates an `hd-repo`. | no |

### Programs Debian lacks

`yazi` (with `hd-yazi` and its config in `etc/`), `ouch`, `wiremix`: Rust, built from the upstream
release in a container by `build-<name>.sh`. `orcaslicer`: the AppImage unpacked, with the wrapper
`orcaslicer`. The version is in the `Makefile`; they have no `version.json`.

## The parts

| Part | In | Files | Doc |
| --- | --- | --- | --- |
| `hd-config` | `hd-base` | `hd-config-update`, `hd-config-remove`; `/etc/hyprdebian/config.json` and `~/.config/hyprdebian/config.json` | `hd-config.md` |
| `hd-network` | `hd-base` | `hd-network-render`, `-iface`, `-bridge`, `lib/hd_network.py`, `templates/hd-network/`, `usr/lib/systemd/network/*.link` | `hd-network.md` |
| `hd-secboot` | `hd-base` | `hd-secboot` | `hd-secboot.md` |
| `hd-theme` | `hd-desktop` | `hd-theme`, `hd-theme-card`, `themes/*.json`, `templates/hd-theme/*.j2` (one per program it colours) | `hd-theme.md` |
| `hd-menu` | `hd-desktop` | `menu/src/`, with `common/src/` shared with `hd-message` | `hd-menu.md` |
| `hd-message` | `hd-desktop` | `message/src/`, `common/src/` | `hd-message.md` |
| `hd-plymouth` | `hd-desktop` | `plymouth/`, `templates/hd-plymouth/`, `hd-plymouth-render`, `hd-plymouth-handoff` | `hd-plymouth.md` |
| `hd-screensaver` | `hd-desktop` | `hd-launch-screensaver`, `hd-cmd-screensaver`, `hd-kill-screensaver`, `screensaver.txt`; `hd-launch-screensaver` and `hd-kill-screensaver` read the key `hd-screensaver`, which `deb/postinst` and `deb/postrm` of `hd-screensavers` set to `hd-pipes` and remove | `hd-screensaver.md` |
| `hd-art` | `hd-desktop` | `wallpapers/` | `hd-art.md` |
| `hd-fonts` | `hd-desktop` | downloaded by the `Makefile` | `hd-fonts.md` |
| `hd-pipes` | `hd-screensavers` | `pipes/src/`, with the notice of `pipes.c` at the top of each file: `main.cpp`, `settings.cpp` (the options, their names, ranges and defaults, with those of `common/src/render.cpp`, and the key `hd-pipes` of the configuration, tested by `tests/test_options.cpp`), `walk.cpp` (the grid walk, where each pipe goes, tested by `tests/test_walk.cpp`), `mesh.cpp` (the tube, elbow and ball, tested by `tests/test_mesh.cpp`), `pipes.cpp` (a scene per output: the steps on the clock, the restart, the instanced drawing) with the shaders `pipe.vert` and `pipe.frag`; `man/hd-pipes.6` | `hd-pipes.md` |

## Where a change goes

| Change | Look at |
| --- | --- |
| A key binding | `hd-desktop/etc/skel/.config/hypr/hyprland.lua`, `docs/HyprDebian/Key Bindings.md` |
| A colour or size of the desktop | `hd-desktop/themes/*.json`, `templates/hd-theme/`, `hd-theme` |
| A package every system needs | `Depends` of `hd-base/deb/control`; of the desktop, `hd-desktop/deb/control` |
| apt behaviour | `hd-base/etc/apt/` |
| Upgrades | `hd-base/hd-up`, `usr/lib/systemd/system/hd-up.*` |
| Firewall, ssh, sysctl | `hd-base/hd-harden`, `templates/hd-harden-*.j2` |
| Network settings | the part `hd-network` |
| A question or step of the installer | `hd-installer/scripts/`, `answers.py`, `templates/etc/` |
| The boot menu or the ESPs | `hd-bootloader` |
| The boot splash | the part `hd-plymouth` |
| The modules of a kernel | `hd-kernel-*/keep-modules`; the build, `hd-kernel/build-kernel.sh` |
| The report or the jobs | `hd-agent`, `hd-dashboard`, `hd-manager`, `hd-base/lib/hyprdebian/job.py` |
| The live ISO | `livecd/config/` |
| A build dependency | `buildenv/Dockerfile`, `docs/HyprDebian/Build requirements.md` |

## What else a change touches

Finish a change with every one of these that applies:

- The version bumps of `AGENTS.md` (`Versions`): the root `version.json` and the `version.json`
  of the package directory.
- The doc page in `docs/packages/` and the man page in `man/` that describe what changed.
- `hd-base`'s `Depends` when every system needs a new package.
- `buildenv/Dockerfile` and `docs/HyprDebian/Build requirements.md` for a new build dependency.
- `make test` in the package directory when it has `tests/`, and new tests there for new behaviour.

A new package directory also needs:

- An entry in `PACKAGES` (or `KERNEL_PACKAGES`) of the root `Makefile`, and in `TESTS` when it has a
  `test` target.
- `deploy.json`, `version.json`, a `.gitignore` with `build`, a doc page and a man page.
- A row in this map.
