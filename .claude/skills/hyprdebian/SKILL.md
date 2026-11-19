---
name: hyprdebian
description: How to run and administer HyprDebian systems, this one and the others of the estate over ssh - which hd-* command does what, which man page to read, and which commands only the owner may run. Use for any admin task on a HyprDebian system - upgrades, reboots, network, themes, firmware, backups, Secure Boot, the jobs of the estate. For changes to the code of HyprDebian, use hyprdebian-codebase-map.
---

# Running HyprDebian systems

You help the owner run their HyprDebian systems from this repository: the system you run on, and
the other systems of the estate over ssh, by hostname (`ssh <host> <command>`, never an IP). Use
the words of `GLOSSARY.md`. Each command has a man page, and it is the source of truth: this
skill only says which command fits a task, which page to read, and who may run it.

## Before every task

1. **Know the system.** Find out which packages are installed on the system you work on:

   ```
   dpkg-query -W -f '${Package} ${Version}\n' 'hd-*'
   ```

   Offer only the commands of packages that are installed there. `hd-base` is on every system;
   `hd-desktop`, `hd-laptop`, `hd-screensavers`, `hd-backup`, `hd-agent`, `hd-manager`,
   `hd-dashboard`, `hd-repo` and `hd-buildserver` are optional.
2. **Read the man page** of every command before you run or hand out one: `man <command>`, on the
   system you work on (`ssh <host> man -P cat <command>`). When the command is not installed
   there, read the copy in the repository, `man -l packages/<dir>/man/<page>`, and keep in mind
   that the repository can be newer than the installed package.

## Who runs what

Every command falls in one of three tiers, on this system and over ssh alike.

- **Read-only**: run it freely.
- **User-level**: changes only what belongs to the user who runs it, without sudo. Run it after
  the normal permission prompt.
- **Owner-only**: everything that needs sudo or another user (`sudo`, `sudo -u`, `runuser`),
  everything that asks questions while it runs, and `hd-manager key` and
  `hd-manager schedule`. The owner runs these; your part ends at the command line. Never run an
  owner-only command, also not when the owner says they are sure. Instead:
  1. Warn: what the command changes, and what can go wrong (a lost ssh connection, a wiped drive,
     a system that does not boot, other systems that act on it).
  2. Give the exact command line in a code block, ready to paste. On another system, as
     `ssh -t <host> sudo <command>`. In Claude Code the owner can run it in this session as
     `! <command>`.
  3. After the owner ran it, check the result with read-only commands.

A command that changes things as a user and needs root for the rest is user-level as a user and
owner-only with sudo: `hd-config-update` and `hd-theme` work on the configuration and the theme of
the user who runs them, and on those of the system with sudo.

## The commands

R is read-only, U user-level, O owner-only.

### hd-base, on every system

| Command | What it does | Tier |
| --- | --- | --- |
| `hd-up` | Upgrades the system after a ZFS snapshot. `hd-up.timer` runs `hd-up --unattended` every Tuesday at 04:30. | O |
| `hd-reboot-check` | Prints why a reboot is needed, nothing when none is. | R; `--write` O |
| `hd-reboot`, `hd-poweroff` | Reboot or power off, closing the desktop applications first. | O |
| `hd-config-update`, `hd-config-remove` | Set or remove a key of the HyprDebian configuration; `man hd-config` explains it. | U on `~/.config/hyprdebian/config.json`; O on `/etc/hyprdebian/config.json` |
| `hd-network-iface`, `hd-network-bridge` | The interfaces and bridges in the key `hd-network`; `man hd-network` explains the settings. | `list`, `show` R; `set`, `remove` O |
| `hd-network-render` | Renders the systemd-networkd files from the key `hd-network`. | `-n` R; without it O |
| `hd-link-repo` | Shows or switches the HyprDebian repository and suite of apt. | no arguments R; with a URL or `--suite` O |
| `hd-firmware` | Lists the firmware packages the hardware needs. | R; `--install` O |
| `hd-harden` | Shows whether ssh, sysctl, modules, zfs and ufw are hardened, and hardens them. | no options or `--json` R (ufw shows unknown without root); every option that hardens O |
| `hd-secboot` | The Secure Boot keys of the owner, in the firmware; `man 7 hd-secboot` explains them. | `--status` R; the rest O |
| `hd-issue` | Writes the login banner, `/etc/issue`. | O |
| `hd-sync-skel` | Brings the files of `/etc/skel` into the home directory, asking per file. | O |

Also read-only, and part of every check: `systemctl --failed`, `zpool status`, `zfs list`,
`systemctl list-timers`, `journalctl -u <unit>`.

### hd-desktop, hd-laptop and hd-screensavers

| Command | What it does | Tier |
| --- | --- | --- |
| `hd-theme` | Lists, selects and applies the themes; `man 7 hd-theme` explains them. | `--list` R; `--select <name>`, `--apply` U; `--add`, `--delete`, `--cards` and all as root O |
| `hd-theme-card` | Renders a theme file into a picture. | U |
| `hd-launcher-cache` | Prints or writes the menu of the application launcher. | no options, `-n` R; `-u` U; `-a` O |
| `hd-desktop-render` | Writes the greetd config that logs the user in to Hyprland. | `--print-user` R; the rest O |
| `hd-plymouth-render` | Renders the boot splash in the theme of the system. | O |
| `hd-mount`, `hd-unmount` | Menus that mount and unmount drives, with sudo, in the desktop session. | O |
| `hd-battery` | Shows the battery level in a notification. | R |
| `hd-pipes` | The screensaver of the 3D pipes, on every monitor until a key, the mouse, a touch or SIGTERM ends it; `--window` shows it in one window, which Escape or closing it ends. The options of `pipes.c`: `--joints`, `--cycles`, `--size`, `--delay`, `--[no-]fisheye`, `--[no-]tightturns`, `--[no-]rotatepipes`, and `--framerate` (0, the refresh rate) and `--msaa` (8), which set what it costs the GPU; `hd-pipes --help` lists them. The key `hd-pipes` of the configuration, the user's over the system's, sets them too; the command line wins. `man 6 hd-pipes`. | `--window` U; without it O, as it covers the monitors of the owner |

`hd-menu`, `hd-message`, `hd-yazi`, `hd-volume-*`, `hd-ask-*`, `hd-mode-select` and the
screensaver commands are what the key bindings run, for the owner at the keyboard. The key
`hd-screensaver` of the configuration, the user's over the system's, names the screensaver that
`hd-launch-screensaver` starts (`hd-pipes` once `hd-screensavers` is installed); without it, or
when that one fails, the text screensaver; `man 7 hd-screensaver`.

### hd-backup

| Command | What it does | Tier |
| --- | --- | --- |
| `hd-backup` | Backs up filesets to drives and tape, with a database per user. | `summary`, `list-*`, `manifest` R; the commands that add, rename, remove or write U |
| `hd-backup-prep` | Wipes a drive and makes it a backup carrier. | O |
| `hd-backup-load`, `hd-backup-unload` | Connect and disconnect a backup drive. | O |
| `hd-backup-vm`, `hd-backup-vms` | Back up virtual machines on ZFS volumes. | O |
| `hd-restore-vm` | Restores a virtual machine, overwriting its datasets. | O |

### The estate

| Command | What it does | Tier |
| --- | --- | --- |
| `hd-agent` | Sends the report to the dashboard and runs the jobs; `hd-agent.timer` runs it every 15 minutes. | O (the token is root's) |
| `hd-manager` | Makes, signs and lists the jobs, run as the user with the manager key. | `jobs` R; `schedule`, `key` O; its settings with `hd-config-update hd-manager` U |
| `hd-dashboard` | Runs the dashboard, lists and forgets its hosts. | O |

### Other servers

| Command | What it does | Tier |
| --- | --- | --- |
| `hd-repo-update` | Adds uploaded packages to the repository, as the user `repo`. | O |
| `hd-buildserver` | Builds HyprDebian and updates a repository with it. | O |
| `hd-installer` | Installs HyprDebian from a live system. | O |
| `hd-bootloader-update` | Writes the bootloader and its UKIs to an ESP. | O |

## Workflows

### Is anything wrong?

Run on the system, or for each host over ssh: `hd-reboot-check`, `systemctl --failed`,
`zpool status -x`, `hd-harden`, `hd-firmware`. Report what each found; "nothing" is a finding too.

### Upgrade and reboot

1. Check each host: `hd-reboot-check`, `zpool status -x`, `systemctl --failed`. A host whose
   `rpool` is not ONLINE is not upgraded unattended; `man hd-up` lists the checks.
2. Choose how to upgrade. When the host has `hd-agent` installed, and the system you run on has
   `hd-manager` and the manager key (`~/.ssh/hd-manager`), suggest a job:

   ```
   hd-manager schedule upgrade <host>... --at <time>
   ```

   `all` instead of the hosts schedules every system that runs `hd-agent`. Otherwise give
   `ssh -t <host> sudo hd-up`.
3. Follow the jobs with `hd-manager jobs <host>`; the agents pick them up within 15 minutes of
   their time.
4. For the reboot, run `hd-reboot-check` on the host, and when it gives a reason, give
   `ssh -t <host> sudo hd-reboot`. Jobs have no reboot command yet.

### Change the network

1. Read the settings: `hd-network-iface list`, `hd-network-iface show <name>`, and the same for
   `hd-network-bridge`.
2. Give the `sudo hd-network-iface set <name> ...` line, from `man hd-network-iface`, and
   `sudo hd-network-render -n` to see the files it would write. Warn that a change of the
   interface that carries the ssh connection can cut it.
3. Give `sudo hd-network-render` to write them.

### Change the theme

For the user: `hd-theme --list`, then `hd-theme --select <name> --apply`. For the system theme,
which the boot splash and the bootloader use, give `sudo hd-theme --select <name>`; a new theme
file first needs `sudo hd-theme --add <file>`.

### Missing firmware

Run `hd-firmware`; when it lists packages that are not installed, give
`sudo hd-firmware --install`, and after new hardware, run `hd-firmware` again.

### Backups

Read the state with `hd-backup summary` and the `list-*` commands. Run the user-level
`hd-backup` commands that the task needs. For a drive, give `sudo hd-backup-load <pool>` before and
`sudo hd-backup-unload <pool>` after; for virtual machines, `sudo hd-backup-vm` or
`sudo hd-backup-vms`. `docs/HyprDebian/Backup tool.md` describes the whole tool.
