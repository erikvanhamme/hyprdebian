---
name: hyprdebian-test-vm
description: How to test HyprDebian changes on the test vm - start from a clean install in seconds, install from scratch with hd-installer, deploy packages, save and roll back states, run commands and take screenshots in the Hyprland session, measure package sizes. Use when a change to a package, the installer or the desktop has to be tried on a real system.
---

# Testing on the HyprDebian test vm

The test vm installs HyprDebian on a disk of its own from an installer system on its other disk, so
a test needs no live ISO. The test disk is a zvol on the host, and a snapshot of it brings back a
whole system, clean install or later state, in about 20 seconds. The tools are in `local-testvm/`;
run them from the root of the repository. Each tool prints only a short result, and the end of the
output when a step fails; the full output of a step is in `local-testvm/log/`. `--help` shows the
options. Only start a test when the owner asks, and test a set of changes once at the end, not every
step.

## The setup

- libvirt domain `debian13` on `qemu:///system`, with two zvols of the host:
  - `rpool/vmdk/installer` (vda): plain Debian trixie on ext4, host `test-installer`. ssh as the
    user with passwordless sudo. It has what the live cd has for `hd-installer` and the local
    repository `/srv/hyprdebian/repo`.
  - `rpool/vmdk/test` (vdb): HyprDebian, host `test`, the answers of `local-testvm/test.json`
    (in `.gitignore`; `test.json.example` shows it without the password). The user has sudo with
    a password.
- Firmware without Secure Boot, no serial port (with a serial console plymouth falls back to text),
  3D acceleration on the GPU of the host (virgl).
- Reach both by host name, never by IP. The owner allows shutting down and redefining the vm.

## The tools

| Tool | What it does |
| --- | --- |
| `zvol-snapshot.py save\|rollback\|destroy\|list <name> [--boot-test] [--wait session]` | Snapshots of the whole test disk on the host; shuts the vm down first. |
| `install-test.py [--no-update] [--no-desktop]` | Installs from scratch and saves the snapshots `clean` and, with hd-desktop, `clean-desktop`. |
| `deploy-test.py <package dir\|.deb>... [--no-build]` | `make <package>`, copies the packages and those of `build/` they depend on to `test`, installs them; prints the versions. |
| `ssh-test [--sudo] [--session] [--hx] '<command>'` | ssh to `test`, with the key or the password; see below. |
| `shot-test [--scale 0.5] <file.png>` | Screenshot of the Hyprland session with grim; turns a screen that is off on first. |
| `boot-vm.py installer\|test [--wait ssh\|session\|none]` | Boots the vm from that disk and waits for ssh. |
| `wait-test.py installer\|test\|session` | Waits until the system answers ssh, or the session hyprctl. |
| `update-installer-vm.py` | `make all`, the local repository on the installer system, and `hd-installer` from it. |
| `test-snapshot.py save\|rollback\|list <name>` | Snapshots of the pool inside the test system, from the installer system; slower than `zvol-snapshot.py`. |
| `type-test-password.py` | Types the password and Enter on the console, as for hyprlock. |
| `dep-weights.py`, `missing-recommends.py` | On `test`: what each dependency costs; the recommends that are not installed. |

`ssh-test` options, before the command:
- `--sudo`: the command gets a function `sudo` that gives sudo the password:
  `ssh-test --sudo 'sudo systemctl restart x && sudo journalctl -u x -n 20'`. It is a function of
  the shell that runs the command, so put `sh -c '...'` after `sudo`, not around it. stdin still
  reaches the command, but not a program run with `sudo`.
- `--session`: `XDG_RUNTIME_DIR`, `DBUS_SESSION_BUS_ADDRESS`, `WAYLAND_DISPLAY` and
  `HYPRLAND_INSTANCE_SIGNATURE` of the Hyprland session are set: `hyprctl clients -j`,
  `systemctl --user --failed`, `journalctl --user`.
- `--hx`: starts the command as a program in the session: `ssh-test --hx 'uwsm app -- kitty'`.

## A test run

1. Start clean: `python3 local-testvm/zvol-snapshot.py rollback clean-desktop --boot-test --wait session`
   (`clean` is the system as `hd-installer` leaves it, without hd-desktop). When the change is in
   `hd-installer` or in what it installs, install from scratch instead:
   `python3 local-testvm/install-test.py` (about 10 minutes; run it in the background). It saves
   new snapshots `clean` and `clean-desktop`.
2. Put the change on it: `python3 local-testvm/deploy-test.py hd-desktop hd-base`.
3. Check it with `ssh-test` and `shot-test`.
4. Before a step that is hard to undo, `zvol-snapshot.py save <name>`; roll back to it instead of
   starting again.

Snapshots of the zvol go back in a line: a rollback to a snapshot with newer ones refuses unless
`--destroy-newer`, which destroys them, so a rollback to `clean` loses `clean-desktop`.
Destroying a snapshot, rolling back past newer ones and replacing `clean` need the zfs permissions
destroy and mount on `rpool/vmdk/test`; the owner gives them, see the head of `zvol-snapshot.py`.

## Running a test in a subagent

A run with long output or long waits, such as an installation, a deploy with many checks, or a
size measurement, goes to the agent `hyprdebian-test-runner` on a cheaper model. A run the owner
asked for may go there without asking again; a check of two or three commands is quicker inline.
The brief gives:

- the start state: the snapshot to roll back to, or an installation from scratch;
- each step as the exact command, with the quoting of `ssh-test` worked out, and what it should
  give;
- what to collect: the output, lines or values, and the screenshots to take, with file names;
- the steps that destroy snapshots that it may run, when there are any.

It reports each step, the first failure and the data word for word. Read the screenshots it names
yourself, and decide yourself what the results mean and what comes next.

## Check a desktop

- Keys: `virsh -c qemu:///system send-key debian13 KEY_ESC`; the lock screen with
  `type-test-password.py`.
- `virsh screenshot` does not work while the vm renders on the GPU, but it does for the boot and
  the console when the vm runs without one.
- Look at screenshots at `--scale 0.5` unless a detail needs the full size.

## Measure size

On `test`: `dpkg-query -W -f '${Installed-Size}\n' | awk '{s+=$1} END {print s/1024 " MiB in " NR " packages"}'`,
`sudo zfs list rpool/hyprdebian` for the disk, `dep-weights.py` and `missing-recommends.py` for the
why. Compare with and without recommends with `apt-get -s install [-o APT::Install-Recommends=true]`.

## Pitfalls

- Measure with `apt-get -s`: `apt-get install --assume-no` does not ask, and so installs, when a
  package needs nothing else.
- `pkill -f '<pattern>'` over ssh also matches the shell that runs it; use `pkill -x <name>`, a
  PID, or a pattern like `'titl[e] x'` that does not match itself.
- `test-snapshot.py` keeps the ESP of a state as a tar on the installer disk, which a zvol rollback
  of the test disk does not bring back; its states can only be trusted until the next zvol rollback.
  A new installation makes the pool again, and its snapshots are gone.
- Mounting the test pool under `/mnt` on the installer system: timesyncd, udevd and logind keep a
  copy of the mounts and the export fails; make `/mnt` private first, or restart them.
- plymouthd started by hand keeps the ssh session open; run it detached, and stop it with
  `plymouth quit`.
- Only `hd-installer` goes on the installer system; every other package goes on `test`.
