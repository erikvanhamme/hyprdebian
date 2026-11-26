---
name: hyprdebian
description: How to run and administer HyprDebian systems, this one and the others of the estate over ssh - which hd-* command does what, which man page to read, and which commands only the owner may run. Use for any admin task on a HyprDebian system - upgrades, reboots, network, themes, firmware, backups, Secure Boot, the jobs of the estate.
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

## The commands

R is read-only, U user-level, O owner-only. The man page of each command has its options.

### hd-base, on every system

| Command | What it does | Tier |
| --- | --- | --- |
| `hd-up` | Upgrades the system after a ZFS snapshot. | O |
| `hd-reboot-check` | Prints why a reboot is needed. | R; `--write` O |
| `hd-reboot`, `hd-poweroff` | Reboot or power off. | O |
| `hd-config-update`, `hd-config-remove` | Set or remove a key of the configuration (`man hd-config`). | U for the user; O with sudo |
| `hd-network-iface`, `hd-network-bridge` | The interfaces and bridges of the key `hd-network`. | `list`, `show` R; the rest O |
| `hd-network-render` | Writes the systemd-networkd files. | `-n` R; the rest O |
| `hd-link-repo` | Shows or switches the repository and suite of apt. | no arguments R; the rest O |
| `hd-firmware` | Lists the firmware packages the hardware needs. | R; `--install` O |
| `hd-harden` | Shows and hardens ssh, sysctl, modules, zfs and ufw. | no options, `--json` R; the rest O |
| `hd-secboot` | The Secure Boot keys of the owner (`man 7 hd-secboot`). | `--status` R; the rest O |
| `hd-issue` | Writes the login banner. | O |
| `hd-sync-skel` | Brings the files of `/etc/skel` into the home directory. | O |

Also read-only, and part of every check: `systemctl --failed`, `zpool status`, `zfs list`,
`systemctl list-timers`, `journalctl -u <unit>`.

### hd-desktop, hd-laptop and hd-screensavers

| Command | What it does | Tier |
| --- | --- | --- |
| `hd-theme` | Lists, selects and applies themes (`man 7 hd-theme`). | `--list` R; `--select`, `--apply` U; the rest and all with sudo O |
| `hd-theme-card` | Renders a theme file into a picture. | U |
| `hd-launcher-cache` | The menu of the application launcher. | no options, `-n` R; `-u` U; `-a` O |
| `hd-desktop-render` | Writes the greetd config. | `--print-user` R; the rest O |
| `hd-plymouth-render` | Renders the boot splash. | O |
| `hd-mount`, `hd-unmount` | Mount and unmount drives in the desktop session. | O |
| `hd-battery` | Shows the battery level. | R |
| `hd-pipes` | The 3D pipes screensaver (`man 6 hd-pipes`). | `--window` U; the rest O |
| `hd-logo` | The bouncing logo screensaver (`man 6 hd-logo`). | `--window` U; the rest O |

The other `hd-*` commands of the desktop are what the key bindings run, for the owner at the
keyboard. The screensaver is set by the key `hd-screensaver` (`man 7 hd-screensaver`).

### hd-backup

| Command | What it does | Tier |
| --- | --- | --- |
| `hd-backup` | Backs up filesets to drives and tape. | `summary`, `list-*`, `manifest` R; the rest U |
| `hd-backup-prep` | Wipes a drive and makes it a backup carrier. | O |
| `hd-backup-load`, `hd-backup-unload` | Connect and disconnect a backup drive. | O |
| `hd-backup-vm`, `hd-backup-vms` | Back up virtual machines. | O |
| `hd-restore-vm` | Restores a virtual machine, overwriting its datasets. | O |

### The estate

| Command | What it does | Tier |
| --- | --- | --- |
| `hd-agent` | Sends the report and runs the jobs. | O |
| `hd-manager` | Makes, signs and lists the jobs. | `jobs` R; `schedule`, `key` O |
| `hd-dashboard` | Runs the dashboard. | O |

### Other servers

| Command | What it does | Tier |
| --- | --- | --- |
| `hd-repo-update` | Adds uploaded packages to the repository. | O |
| `hd-buildserver` | Builds HyprDebian and updates a repository. | O |
| `hd-installer` | Installs HyprDebian from a live system. | O |
| `hd-bootloader-update` | Writes the bootloader to an ESP. | O |

## Workflows

- **Is anything wrong?** On each host: `hd-reboot-check`, `systemctl --failed`, `zpool status -x`,
  `hd-harden`, `hd-firmware`. Report what each found; "nothing" is a finding too.
- **Upgrade and reboot.** Check each host as above; a host whose `rpool` is not ONLINE is not
  upgraded unattended. When the host runs `hd-agent` and this system has `hd-manager` and the
  manager key (`~/.ssh/hd-manager`), give `hd-manager schedule upgrade <host>... --at <time>`
  (`all` for every agent) and follow it with `hd-manager jobs <host>`; otherwise give
  `ssh -t <host> sudo hd-up`. For the reboot, when `hd-reboot-check` gives a reason, give
  `ssh -t <host> sudo hd-reboot`.
- **Change the network.** Read `hd-network-iface list` and `show <name>`, give the
  `sudo hd-network-iface set ...` line, then `sudo hd-network-render -n` to preview and
  `sudo hd-network-render` to write. Warn that changing the interface of the ssh connection can
  cut it.
- **Change the theme.** For the user `hd-theme --select <name> --apply`; for the system, which the
  boot splash and the bootloader use, `sudo hd-theme --select <name>`.
- **Backups.** Read `hd-backup summary` and the `list-*` commands, and run the user-level
  `hd-backup` commands the task needs. Around a drive, give `sudo hd-backup-load <pool>` and
  `sudo hd-backup-unload <pool>`. `docs/HyprDebian/Backup tool.md` describes the whole tool.
