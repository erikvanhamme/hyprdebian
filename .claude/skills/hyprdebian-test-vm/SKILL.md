---
name: hyprdebian-test-vm
description: How to test HyprDebian changes on the test vm - install from scratch with hd-installer, save and roll back states, run commands and take screenshots in the Hyprland session, measure package sizes. Use when a change to a package, the installer or the desktop has to be tried on a real system.
---

# Testing on the HyprDebian test vm

The test vm installs HyprDebian on a disk of its own from an installer system on its other disk,
so a test needs no live ISO, and it can save and roll back states of the installed system in about
a minute. The tools are in `local-testvm/`; run them from the root of the repository. Only start a
test when the owner asks, and test a set of changes once at the end, not every step.

## The setup

- libvirt domain `debian13` on `qemu:///system`, with two zvols:
  - `virtio-vmdk-installer` (vda): plain Debian trixie on ext4, host `test-installer`. ssh as the
    user with passwordless sudo. It has what the live cd has for `hd-installer` (zfs-dkms,
    debootstrap, gdisk, parted, mdadm, efibootmgr, apt-utils, python3-jinja2) and the local
    repository `/srv/hyprdebian/repo`.
  - `virtio-vmdk-test` (vdb): HyprDebian, host `test`. The user has sudo with a password, the one
    in `local-testvm/test.json`.
- Firmware without Secure Boot (`OVMF_CODE_4M.fd`, `OVMF_VARS_4M.fd`), no serial port (with a
  serial console plymouth falls back to text), 3D acceleration on the GPU of the host (virgl).
- Reach both by host name, never by IP. The owner allows shutting down and redefining the vm.

## The tools

| Tool | What it does |
| --- | --- |
| `boot-vm.py installer\|test` | Shuts the vm down, gives the chosen disk boot order 1, starts it. |
| `update-installer-vm.py` | `make all`, then the local repository on the installer system, and installs `hd-installer` from it. |
| `test-snapshot.py save\|rollback\|list <name> [--boot-test]` | States of the test system: `rpool@<name>` and a tar of its ESP, done from the installer system. |
| `ssh-test '<command>'` | ssh to `test`, with the key or the password; ignores the host key, which every installation changes. |
| `askpass-test` | Prints the password; pipe it in for sudo: `askpass-test \| ssh-test 'read -r PW; printf "%s\n" "$PW" \| sudo -S -p "" <command>'`. |
| `shot-test <file.png>` | Screenshot of the Hyprland session with grim; also copies `hx` to `/tmp/hx` on `test`. |
| `hx` | On `test`: `sh /tmp/hx '<command>'` starts a program in the Hyprland session. |
| `type-test-password.py` | Types the password and Enter on the console of the vm, as for hyprlock. |
| `dep-weights.py [package] [--drop=a,b]` | On `test`: what each dependency of a package costs, alone or together. |
| `missing-recommends.py` | On `test`: the recommends of installed packages that are not installed. |

`test.json` holds the answers of the installation (host name `test`, no encryption, so the system
boots without help); it is in `.gitignore`, `test.json.example` shows it without the password.

## Install from scratch

1. `python3 local-testvm/boot-vm.py installer`, wait until `ssh test-installer true` works.
2. `python3 local-testvm/update-installer-vm.py` builds every package and installs them there.
3. Copy the answers once: `scp local-testvm/test.json test-installer:/tmp/` and
   `ssh test-installer 'sudo install -m 600 /tmp/test.json /root/test.json; rm /tmp/test.json'`.
4. Run the installer as a unit, so it survives a lost ssh connection, with stdin closed so a
   question it was not given fails instead of hanging:
   `ssh test-installer 'sudo rm -rf /tmp/hd-installer; sudo systemd-run --unit=hd-test-install --property=StandardInput=null --setenv=TERM=dumb bash -c "hd-installer --answers /root/test.json > /root/install.log 2>&1; echo EXIT=\$? >> /root/install.log"'`
   (`sudo systemctl reset-failed hd-test-install` first when the unit ran before). Wait in the
   background until `/root/install.log` has `EXIT=`; it takes about 4 minutes.
5. `python3 local-testvm/boot-vm.py test`, then put the key of the user on `test`; `ssh-test` logs
   in with the password this time:
   `local-testvm/ssh-test 'mkdir -p ~/.ssh && chmod 700 ~/.ssh && cat >> ~/.ssh/authorized_keys' < ~/.ssh/id_rsa.pub`.
6. Save the state: `python3 local-testvm/test-snapshot.py save base --boot-test`.

## Try, roll back, try again

Save a state before every change that is hard to undo, and roll back instead of installing again.
`test-snapshot.py` works from the installer system and boots it when needed; a rollback with
`--boot-test` is back on `test` in about a minute. Packages that are not in a repository go over
with `scp` and `apt-get install /tmp/debs/*.deb`.

## Check a desktop

- Programs: `ssh-test "sh /tmp/hx 'uwsm app -- kitty'"`, after one `shot-test`, which copies `hx`.
- Screenshots: `shot-test`; `virsh screenshot` does not work while the vm renders on the GPU, but
  it does for the boot and the console when the vm runs without one.
- Keys: `virsh -c qemu:///system send-key debian13 KEY_ESC`; the lock screen with
  `type-test-password.py`.
- State of the session: `XDG_RUNTIME_DIR=/run/user/$(id -u)`, `systemctl --user --failed`,
  `hyprctl clients -j` with `HYPRLAND_INSTANCE_SIGNATURE=$(ls $XDG_RUNTIME_DIR/hypr)`.

## Measure size

On `test`: `dpkg-query -W -f '${Installed-Size}\n' | awk '{s+=$1} END {print s/1024 " MiB in " NR " packages"}'`,
`sudo zfs list rpool/hyprdebian` for the disk, `dep-weights.py` and `missing-recommends.py` for the
why. Compare with and without recommends with `apt-get -s install [-o APT::Install-Recommends=true]`.

## Pitfalls

- A new installation makes the pool again: all snapshots are gone, and a rollback to an older
  name is refused. Save a new base after each installation.
- Never `apt-get install --assume-no` to measure: apt does not ask, and so installs, when a
  package needs nothing else. Use `apt-get -s`.
- `pkill -f '<pattern>'` over ssh also matches the shell that runs it; use a PID, or a pattern
  like `'titl[e] x'` that does not match itself.
- Mounting the test pool under `/mnt` on the installer system: timesyncd, udevd and logind keep a
  copy of the mounts and the export fails; make `/mnt` private first, or restart them.
- `<boot dev='hd'/>` in the domain makes OVMF delete the boot entry of the second disk; keep the
  boot order per disk, as `boot-vm.py` sets it.
- `/tmp` on `test` is emptied at every boot: copy `hx` again (`shot-test` does).
- Hyprland 0.56 takes Lua: `hyprctl dispatch 'hl.dsp.exec_cmd("...")'`, which `hx` wraps.
- plymouthd started by hand keeps the ssh session open; run it detached, and stop it with
  `plymouth quit`.
- Only `hd-installer` goes on the installer system; install every other package on `test`.
