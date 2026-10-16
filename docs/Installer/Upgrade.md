## Introduction
`upgrade.sh` brings an installed hyprdebian system up to date with a newer version of the installer. It uses the answers that were given during the installation, works out again what the new version installs for those answers, and updates the system where it differs.

The upgrade reuses the installer: the same questions, the same queues and the same task system. It does not track what changed between two versions. It builds the list of files, packages, services and groups the new version wants, and compares that with the system.

## Running the upgrade
On the installed system, get the new version of the repo and run, from the root of the repo:
```
sudo ./upgrade.sh
```

The upgrade must run as root, and from a terminal, because it can ask questions. When it is started from a terminal in the Wayland session, changed files can be merged in meld. Over ssh, or from a console, sdiff is used. See [Changed files](#changed-files).

The upgrade can be started again at any time. It starts fresh on every run, and every part of it only changes what is not up to date yet.

Reboot the system after the upgrade. New services only start on the next boot, and migrations may change how the system boots, like the EFI mounts and the swap. The upgrade reminds you at the end.

## Before the upgrade
Before it works out anything, the upgrade:
1. **Checks that the system is up to date.** It runs `apt update`, and counts the packages `apt full-upgrade` would upgrade. When there are any, it recommends to upgrade the system first, and asks whether to continue anyway. The default is to stop. The upgrade installs new packages with their latest versions, and on a system that is behind, these can pull in upgrades of the packages they depend on, like `systemd`, while the rest of the system stays behind.
2. **Snapshots the system.** It takes the snapshots `rpool/hyprdebian@upgrade-<date>_<time>` and `bpool/hyprdebian@upgrade-<date>_<time>`, like `upgrade-2026-10-15_143005`, so the upgrade can be undone with `zfs rollback`. When a snapshot cannot be created, the upgrade stops. `hd-up` takes the same snapshots before an `apt full-upgrade`, named `hd-up-<date>_<time>`.
3. **Installs the packages the upgrade itself needs**, when they are missing: `python3-jinja2`, which renders the templates.

## State on the installed system
The installer leaves two files on the installed system for the upgrade:

| Path | Written by | Contents |
| --- | --- | --- |
| `/etc/hyprdebian/installer.config` | `c_save_config` | A copy of the installer configuration: the answers, values generated during the installation such as `BR0_MAC_ADDRESS`, the queues, and `INSTALLED_VERSION`. Mode `0600`. The password is never saved. |
| `/etc/hyprdebian/migrations` | `c_save_migrations` | The names of the migrations that already ran on the system, one per line. See [Migrations](#migrations). |

The upgrade writes the config back at the end of every successful run, with the new queues, any new answers and the new `INSTALLED_VERSION`.

The state of the upgrade itself is kept in `/tmp/hyprdebian-upgrade`, and is removed at the start of every run.

## Startup
`upgrade.sh` does the following, in order:
1. Sources `version.sh` and the installer helper scripts, then `scripts/upgrade_helpers.sh` and `scripts/upgrade_detect.sh`. The upgrade helpers replace a few installer settings and functions, see [Upgrade helpers](#upgrade-helpers).
2. Prints the start banner and checks that it runs as root.
3. Creates a fresh state directory `/tmp/hyprdebian-upgrade`.
4. Checks that the system is up to date (`check_up_to_date`), snapshots it (`take_snapshots`), and installs the packages the upgrade needs (`install_prerequisites`). See [Before the upgrade](#before-the-upgrade).
5. Loads the answers of the installation (`load_installed_config`, then `load_config`). The queues and dependencies in the saved config were worked out by the version that wrote it, so they are left out. The old queues are kept apart, to find out what the new version adds. Without a saved config, the answers are worked out from the system, see [Systems installed before the upgrade existed](#systems-installed-before-the-upgrade-existed).
6. Defines the root task `upgrade`.
7. Sources `scripts/questions.sh`, `scripts/base.sh` and `scripts/optional.sh`, and `scripts/packages.sh`, `scripts/services.sh` and `scripts/user.sh` for the tasks on their hooks (see [Hook tasks](#hook-tasks)), and marks the tasks that only make sense during an installation as done, so they are skipped.
8. Adds the tasks that work out the queues to `upgrade`, then sources `scripts/upgrade.sh`, the upgrade phase.
9. Runs `execute_task "upgrade"`, and when it succeeds, recommends a reboot.

## Working out the queues
The upgrade runs the tasks of the installer that decide what is installed, with the code of the new version:

| Task | Queues |
| --- | --- |
| `q_main` | The questions, and through them the `o_` tasks of the selected components. |
| `b_files`, `b_templates` | The base files and templates. |
| `b_systemd`, `b_utilities`, `b_network` | The base packages, files, templates, services and groups. |
| `o_main` | The selected optional components. |

These tasks are skipped: `q_suite` (the suite of an installed system is kept), `q_disk`, `q_password` and `q_kernel` (they ask about the disks, the password and a kernel download), and `o_art` and `o_font` (their packages are built by the deb phase, which the upgrade does not run).

Two of the `o_` tasks do more than queuing, and do that again on every upgrade: `o_wifi` masks `systemd-networkd-wait-online.service`, and `o_backuptools` compiles `blockwrite` into `/usr/local/bin`, from a copy of the source in `/tmp`. Both are safe to repeat.

The questions are answered with the saved answers, without asking. Only a question without a saved answer is asked, like a question that was added after the installation. Its answer is saved with the others at the end of the upgrade.

The other base tasks, like `b_fstab`, `b_kernel`, `b_grub2` and `b_bootloader`, are not run. They set up the disks and the boot loader, and what they queue is not part of the upgrade.

## Upgrade phase
The upgrade phase is in `scripts/upgrade.sh`, and is built like the [phases](Phases.md) of the installer:

| Hook | Task | What it does |
| --- | --- | --- |
| `upg_pre` | `upg_session` | Looks for the Wayland session the upgrade was started from, for merges in meld. |
| `upg_main` | `upg_migrations` | Runs the migrations that did not run on the system yet. |
| | `upg_packages` | Installs the packages the new version adds. |
| | `upg_files` | Copies the files in `/etc/skel` and `/usr/local/bin` that differ from the system. |
| | `upg_etc` | Updates the other deployed files, all in `/etc`. |
| | `upg_templates` | Updates the files rendered from the queued templates. |
| | `upg_groups` | Adds the user to the groups the new version adds. |
| | `upg_services` | Reloads systemd, and enables the services the new version adds. |
| | `upg_home` | Updates the files from `/etc/skel` in the home directories of the user and root. |
| | `upg_hooks` | Runs the tasks the new version attaches to the hooks of the installer phases, when they did not run on the system yet. See [Hook tasks](#hook-tasks). |
| `upg_post` | `upg_save_config` | Writes the config back to `/etc/hyprdebian/installer.config`, and writes `/etc/issue` with the new version (`write_issue`). |

The upgrade stops at the first task that fails. The config is only written back at the end, so a failed upgrade does the same comparisons again on the next run.

## Files
Files are compared by their content. A file that is the same as the new version is left alone.

`/etc/skel` and `/usr/local/bin` are copies that belong to hyprdebian, so `upg_files` overwrites a file there that differs, and adds a missing one, without asking.

The files in `/etc`, the rendered templates and the files in the home directories can be changed on the system. For these, a missing file is added, and for a file that differs the upgrade asks:
```
/etc/hyprdebian/vm-backups differs from the new version. [k]eep mine, [t]ake new, [m]erge:
```

- **Keep mine** leaves the file as it is.
- **Take new** replaces the content with the new version.
- **Merge** opens a merge tool, see [Changed files](#changed-files). A file that is not a text file cannot be merged.

The upgrade writes into the existing file, so its owner and mode stay the same. A file that is added gets the mode of the new version, and in a home directory it is owned by the user.

Without a record of earlier versions, the upgrade cannot tell a change made on the system from a change in the new version. A file that is changed on purpose, like a `hyprland.lua` with extra key bindings, is therefore asked about on every upgrade. Answer `k` to keep it, or merge in the changes of the new version.

### Changed files
A merge shows the new version on the left and the file on the system on the right, so changes are merged from left to right.

- **meld** is used when the upgrade was started from a Wayland session and meld is installed. `sudo` removes `WAYLAND_DISPLAY` from the environment, so `find_session` looks for it in the parent processes of the upgrade. meld runs as the user of that session, on copies of the two files. Save the right side to keep the merge. Closing meld without saving keeps the file as it was.
- **sdiff** is used otherwise. It shows only the lines that differ, and asks for each difference:
  - `l` takes the new version, `r` keeps the line of the system.
  - `e` opens an editor for the difference (`$EDITOR`, or nano when it is not set).
  - `q` stops the merge and leaves the file as it was.

When the merge is not finished, the file is left as it was, and the upgrade continues with the next file.

### Home directories
`upg_home` updates the files from `/etc/skel` that the new version deploys, in `/home/<user>` of the user that was created during the installation. Root only gets `.bash_aliases` and `.config/nano/`, the files `u_root_skel` copies during the installation. Other users are not updated.

### Templates
The queued templates (see [Queues](Queues.md)) are rendered with the saved answers into `/tmp/hyprdebian-upgrade/templates`, and then handled like the other files in `/etc`. Rendering needs `python3-jinja2` on the system, which is one of the base packages.

The templates for the disks, the boot loader and the apt sources (`fstab`, `crypttab`, `mdadm.conf`, the EFI mount units and the sources lists) are not queued, but rendered directly by their tasks during the installation. The upgrade never updates them. Only a migration changes them, like `0005-apt-sources`, `0007-efi-automount` and `0008-swap-encryption`.

## Packages, services and groups
For packages, services and groups, the upgrade only acts on what the new version **adds**: what is in the new queue, but not in the queue saved by the installation or the last upgrade. A package that was removed, a service that was disabled or a group the user was taken out of after the installation is not brought back.

- **Packages** are installed with `apt update` and `apt install --no-upgrade`. Packages that are already installed are not upgraded. Upgrading the system is left to the user. apt asks before it installs extra dependencies. Answering no stops the upgrade. Queue entries that keep a package out, like `avahi-daemon-`, are passed along only for packages that are not installed, so a new package cannot pull them in. An installed package is never removed this way.
- **Services** are only enabled, not started, so they start on the next boot. Starting a service on a running system can disturb it. `rotate-machine-id`, for example, would change the machine id right away. `systemctl daemon-reload` runs on every upgrade, because unit files may have changed.
- **Groups** take effect after the user logs in again.

Packages, services and groups that the new version no longer uses are not removed. That needs a migration.

## Hook tasks
Some queuing tasks attach a task to a hook of a later installer phase, for a step that does more than queuing. `o_firewall` attaches `pkg_enable_firewall` to `pkg_post`, `o_qemu_kvm` attaches `pkg_qemu_kvm` and `svc_qemu_kvm`, `o_desktop` attaches the tasks that create the log file and the directories in the home directory, and so on. The upgrade does not run these phases, so it runs these tasks itself in `upg_hooks`.

The hooks are listed in `UPGRADE_HOOKS` in `scripts/upgrade_helpers.sh`: `pkg_post`, `svc_post` and `u_post`. Like packages, services and groups, a task only runs when the new version **adds** it: when it is attached to one of these hooks now, but was not in the dependencies saved by the installation or the last upgrade. So a task runs once, when its component is enabled for the first time during an upgrade, or when a new version adds the task. The tasks run at the end of `upg_main`, after the packages, files, services and groups, in the order of the hooks.

## Migrations
A migration is a one-off change for systems that were installed or upgraded before the change, like masking a service, removing a file or a package that is no longer used, or moving a config file. Migrations are the files `migrations/NNNN-description.sh`. Each one defines a `migrate` function, written like a task.

`upg_migrations` runs the migrations that are not in `/etc/hyprdebian/migrations` yet, in the order of their numbers, before the packages and files are updated. Each migration that succeeds is added to the file right away, so it never runs twice, even when a later part of the upgrade fails. When a migration fails, the upgrade stops, and the migration runs again on the next upgrade.

A new installation records all migrations that exist at that time as done, because it already has their changes.

See [Migrations](Migrations.md) for the migrations so far and what each one does, and [migrations/README.md](../../migrations/README.md) for how to write a migration.

## Upgrade helpers
`scripts/upgrade_helpers.sh` replaces a few things of the installer:

| Name | In the installer | In the upgrade |
| --- | --- | --- |
| `TARGET_DIR` | `/mnt` | Empty, the upgrade works on the running system. |
| `STATE_DIR`, `CONFIG_FILE` | `/tmp/hyprdebian` | `/tmp/hyprdebian-upgrade` |
| `in_target` | Runs a command in a `chroot` of `/mnt`. | Runs the command directly. |
| `accept_default` | Answers a question with its default when the machine type defaults are used. | Answers a question with its saved answer, when there is one. |

It also has the functions the upgrade uses: `is_installed`, `check_up_to_date`, `install_prerequisites`, `load_installed_config`, `new_items`, `find_session`, `merge_file` and `update_file`. The detection of the answers is in `scripts/upgrade_detect.sh`.

## Changing the installer with the upgrade in mind
Because the upgrade runs the installer tasks that work out the queues, a few rules keep them working for both:

- **Tasks that queue things must be safe to run on an installed system.** The `o_` tasks and the base tasks in [Working out the queues](#working-out-the-queues) also run during every upgrade, on the running system. Keep them to queuing. A step that does something, like masking a service, belongs in a task attached to a hook (see [Extending](Extending.md)).
- **Tasks attached to `pkg_post`, `svc_post` and `u_post` must be safe to run on the running system.** The upgrade runs a new one once, see [Hook tasks](#hook-tasks). A task that already ran does not run again when it changes, so a change for systems that already have it needs a migration. Renaming a task makes it run again on every system that has its component.
- **Generated values must be saved and reused.** A value that is generated during the installation, like `BR0_MAC_ADDRESS`, is saved with `save_config`, and only generated when there is no saved value yet. Otherwise every upgrade would generate a new one.
- **Files go through a queue.** A file that a task deploys directly, with `file_deploy` or `template_render`, is not seen by the upgrade. Queue it with `add_files` or `add_template`, unless it must never be changed by an upgrade.
- **Removing something needs a migration.** The upgrade only adds packages, services, groups and files. When the new version stops using one of them, add a migration that removes it.

## Systems installed before the upgrade existed
A system installed before version v0.152 has no `/etc/hyprdebian/installer.config` and no `/etc/hyprdebian/migrations`.

### Detected answers
The upgrade works out the answers from the state of the system (`detect_config` in `scripts/upgrade_detect.sh`), and then asks **every** question with the detected answer filled in. Press enter to confirm an answer, or correct it. The suite, the disks, the password and the kernel are not asked.

| Answer | Worked out from |
| --- | --- |
| `Q_SUITE` | The suite in `debian.sources`, or in the older `sources.list`. `sid`, or no suite found, is saved as `unstable`. |
| `Q_TYPE` | `hostnamectl chassis`. |
| `Q_REDUNDANT`, `Q_ENCRYPT` | A mirror in `rpool`, and the encryption of `rpool`. |
| `Q_SWAP` | The size of the swap partition in `/proc/swaps`, in GiB. zram swap is not counted. |
| `Q_USER`, `Q_FULLNAME` | The first regular user with a home directory in `/home`. |
| `Q_HOSTNAME`, `Q_FQDN` | `/etc/hostname`, and the `127.0.1.1` line in `/etc/hosts`. |
| `Q_SPECIFIC_KERNEL`, `Q_KERNEL` | Whether `linux-image-amd64` is installed, otherwise the running kernel. |
| `Q_IFACE`, `Q_WIFACE` | The `Name=` in the `50-` and `60-` network files. |
| `Q_QEMU_KVM`, `BR0_MAC_ADDRESS` | `30-br0.netdev` and its MAC address, or the user in the `libvirt` group. The packages alone are not enough: virt-manager is also used to manage other machines. |
| `Q_BACKUPTOOLS` | `hd-backup` in `/usr/local/bin`, or `mbuffer` and `mt-st` installed. |
| `Q_REPO_ENABLED`, `Q_REPO` | `hyprdebian.local.sources` and its URI, or the older `hyprdebian.local.list` and the URL in it. |
| Other components | Their main package: `hyprland`, `iwd`, `ufw`, `docker.io`, `cups`, `rustup`, `openssh-server`. |

The answers are saved at the end of the upgrade, so the next upgrade uses them without asking.

### Other differences
The upgrade also:
- treats every package, service, group and hook task as new, once. Hook tasks like `pkg_enable_firewall` run again. Packages that are already installed are left alone, but services that were disabled are enabled again,
- runs all migrations. Each migration first checks whether its change is needed, and is recorded as done without changing anything when it is not.
