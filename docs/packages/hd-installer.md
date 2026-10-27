## Introduction
The package `hd-installer` installs HyprDebian on the disks of a computer, from a live system. It asks all its questions first, then runs the installation without questions. It installs the base system, the same on every computer; for a desktop, install `hd-desktop` on it afterwards.

It has the questions, disk, partition, filesystem, base, packages, user and cleanup phases; the prerequisites phase, which installs the tools it needs on a live system that lacks them, is still to come.

It depends on `python3` and `python3-jinja2`, to render the files it writes in the new system.

It installs the hd-* packages from the local repository of the HyprDebian live cd, in `/srv/hyprdebian/repo`, and the Debian packages from the Debian mirrors, so it needs internet access but no HyprDebian repository. The installed system keeps only the Debian repositories; to get updates of the hd-* packages, link it to a HyprDebian repository after the first boot with `hd-link-repo <url> --suite <suite>`, see [hd-base](hd-base.md).

The package comes with a man page: `man hd-installer`.

## Building the package
The package is built from `packages/hd-installer` in this repository:
```
cd packages/hd-installer
make deb
```

This builds `build/hd-installer_<version>_all.deb`, with the version from `version.json`. Raise the version there for every package that is handed out. Install it with:
```
sudo apt install ./build/hd-installer_<version>_all.deb
```

The installer also runs from the source tree, as root, on a live system with the local repository of the live cd:
```
sudo ./packages/hd-installer/hd-installer
```

The files involved are:
```
packages/hd-installer/Makefile
packages/hd-installer/version.json
packages/hd-installer/hd-installer
packages/hd-installer/answers.py
packages/hd-installer/scripts/constants.sh
packages/hd-installer/scripts/config.sh
packages/hd-installer/scripts/answers.sh
packages/hd-installer/scripts/defaults.sh
packages/hd-installer/scripts/dependencies.sh
packages/hd-installer/scripts/helpers.sh
packages/hd-installer/scripts/helpers-disk.sh
packages/hd-installer/scripts/helpers-host.sh
packages/hd-installer/scripts/helpers-network.sh
packages/hd-installer/scripts/helpers-user.sh
packages/hd-installer/scripts/questions.sh
packages/hd-installer/deb/control
packages/hd-installer/man/hd-installer.8
```

## Running the installer
```
hd-installer [--reset] [--answers <file>]
```

Run it as root. A task runs once: when it succeeds it is marked done, and a rerun skips it. After a failure, fix the cause and run `hd-installer` again; it continues where it stopped, and offers the answers given before. `--reset` forgets the answers and the finished tasks, and starts over. `--answers` takes the answers from a JSON file, see [Answers file](#answers-file).

It exits with 0 when the installation is done, 1 when a task failed and 2 on wrong arguments.

## How it works
`hd-installer` sources the functions and constants in `scripts/`, loads the saved answers, then sources the phases in order. Each phase is a file in `scripts/` with the tasks `<phase>_pre`, `<phase>_main` and `<phase>_post`, which are dependencies of the task `install_root`, which runs them all; the tasks of the phase are dependencies of `<phase>_main`. The root task is not named `install`, which would hide the `install` command from every task.

| File | Purpose |
| --- | --- |
| `scripts/constants.sh` | The mount point of the new system, the local repository of the live cd and where it is bound in the new system, the numbers of the partitions, the state directory, the directories of the disk names and of the network interfaces, and the colors. |
| `scripts/config.sh` | `load_config` and `save_config`: the answers and the task dependencies, kept in `/tmp/hd-installer/config` as variable assignments. |
| `scripts/answers.sh` | `load_answers`, which reads the answers file with `answers.py` into `ANSWERS`, by the variable of each question, and saves its path in the config; `answered`, which tells whether the file answers a question; `show_answer`, which shows such an answer in place of the question; and `debconf_answered`, which tells whether the file has debconf selections of a package. |
| `scripts/defaults.sh` | The suites and the defaults of the questions. |
| `scripts/dependencies.sh` | `add_dependencies`, `execute_task` and `always_run`: the tree of tasks, each marked done in `/tmp/hd-installer/<task>.done`. |
| `scripts/helpers.sh` | Asking questions: `ask`, which asks for a line of text; `ask_valid`, which asks until a check function accepts the answer and only stores a valid one; `ask_password`, which asks for a password twice without showing or saving it; `ask_options`, which asks to pick one of a list of options; `ask_yes_no`, which asks a yes or no question and stores `true` or `false`; `confirm`, which asks to confirm something that cannot be undone, without a default and without saving the answer; and `set_answer`, which stores an answer that follows from earlier ones. Also `log_status`, which logs the start and end of a task; `add_packages`, which marks packages for the packages phase; `add_user_groups`, which adds groups for the user; `in_target`, which runs a command in the new system; `file_deploy`, which copies a file from `deploy/` into the new system; and `template_render`, which renders a template from `templates/` into the new system with `render.py`, with the answers except the password and the variables given. |
| `scripts/helpers-disk.sh` | `disk_name`, which names a disk by its WWN, its NVMe EUI or its kernel node; `list_disks`, which lists the whole disks by that name; `ask_disk`, which asks to pick one of them; `unique_disks`, which lists the disks picked, each once; `root_disks`, `esp_disks` and `all_disks`, which list the disks of the system, of the EFI system partitions and both; `part`, which names a partition of a disk; `unmount_target`, which unmounts everything below `/mnt`, deepest first; `can_discard`, which tells whether a disk supports discard; and `valid_swap`, the check of the swap size. |
| `scripts/helpers-host.sh` | `valid_hostname` and `valid_fqdn`, the checks of the host name and the FQDN. |
| `scripts/helpers-network.sh` | `list_ifaces`, which lists the wired or the wifi interfaces by their path name; and `ask_iface`, which asks to pick one of them. |
| `scripts/helpers-user.sh` | `valid_user` and `valid_fullname`, the checks of the user name and the full name. |
| `scripts/questions.sh` | The questions phase: `q_pre`, `q_main` and `q_post`, and a task per question. |
| `scripts/disk.sh` | The disk phase, with the tasks `d_`. |
| `scripts/partition.sh` | The partition phase, with the tasks `p_`. |
| `scripts/filesystem.sh` | The filesystem phase, with the tasks `f_`. |
| `scripts/base.sh` | The base phase, with the tasks `b_`. |
| `scripts/packages.sh` | The packages phase, with the tasks `pkg_`. |
| `scripts/user.sh` | The user phase, with the tasks `u_`. |
| `scripts/cleanup.sh` | The cleanup phase, with the tasks `c_`. |
| `answers.py` | Reads the answers file and prints its answers, by the variable of their question, separated by NUL characters. It checks the keys and the types of the values; the checks that need the computer or the shell are left to the helpers. |
| `render.py` | Renders a jinja2 template with the variables of its environment. |
| `templates/` | The templates of the files written in the new system, at their path there with `.j2`. |
| `deploy/` | The files copied into the new system, at their path there. |

A task is a shell function. Tasks run without `set -e`, as they run inside an `if`, so every command that may fail ends in `|| return 1`.

## Questions
| Question | Variable | Answers |
| --- | --- | --- |
| Suite | `Q_SUITE` | `unstable` or `trixie`, the suites of HyprDebian; `unstable` by default. `hd-desktop` needs packages that are only in `unstable`. |
| System mirror | `Q_ROOT_REDUNDANT` | `true` when the system, the ZFS pool and the swap, is mirrored on two disks; not by default. |
| Encryption | `Q_ENCRYPT` | `true` when the ZFS pool is encrypted, with the password of the user as its passphrase; by default. |
| System disks | `Q_ROOT_DISK_A`, `Q_ROOT_DISK_B` | The disk for the system, and with the mirror the second one; `Q_ROOT_DISK_B` is empty without it. |
| Swap | `Q_SWAP` | The size of the swap partition on the disks of the system, in GiB, a whole number; 2 by default, 0 for no swap. With the system mirrored, the swap is mirrored too. |
| ESP mirror | `Q_ESP_REDUNDANT` | `true` when there is an EFI system partition on two disks; by default when the system is mirrored. |
| ESP disks | `Q_ESP_DISK_A`, `Q_ESP_DISK_B` | The disk for the EFI system partition, and with the mirror the second one; by default the disks of the system. `Q_ESP_DISK_B` is empty without the mirror. |
| User name | `Q_USER` | The user account: a lower case letter, then lower case letters, digits, `-` and `_`, at most 32 characters, as `adduser` accepts by default. |
| Password | `Q_PASSWORD` | The password of the user, and with encryption the passphrase of the ZFS pool; at least 8 characters, the least that ZFS takes, typed twice and not shown. It is never saved, nor passed in the environment of other commands, so it is asked again on every run. |
| Full name | `Q_FULLNAME` | The full name of the user, for the GECOS field of `/etc/passwd`; it cannot be empty, nor contain `:` or `,`. |
| Host name | `Q_HOSTNAME` | `hyprdebian` by default. One DNS label: lower case letters, digits and `-`, not at the start or the end, at most 63 characters. |
| FQDN | `Q_FQDN` | The fully qualified domain name: the host name, a dot and the domain, in labels like the host name, at most 253 characters. By default the host name followed by the domain `Q_DOMAIN` of the defaults. |
| Wired interface | `Q_IFACE` | The wired interface, which gets its address with DHCP; by default the first one. Without a wired interface, it is empty, without the question. |
| Wifi | `Q_WIFI`, `Q_WIFACE` | `true` when wifi is used, not by default, and then the wifi interface, which also gets its address with DHCP; `Q_WIFACE` is empty without wifi. Without a wifi interface, there is no wifi, without the question. |
| Default settings | `Q_DEBCONF_DEFAULTS` | `true`, by default, when the locale, the time zone, the keyboard and the console font take the defaults of `scripts/defaults.sh`, en_US.UTF-8, Europe/Brussels, an English (US) keyboard and TerminusBold 8x16, without their debconf questions. The defaults are shown before the question. Without the question, `false`, when the answers file has debconf selections of all four packages. |
| Wipe | | Every disk picked above is wiped. Each one must be confirmed by typing `y`; the confirmation is not saved. This is the last question, so nothing is wiped before every question is answered. |

The EFI system partitions are asked apart from the system, so they can go on other disks than the ZFS pool, such as a hardware RAID volume next to a ZFS mirror on disks that the RAID controller passes through. An ESP disk may also be one of the system disks.

Interfaces are picked by number, by their path name (`ID_NET_NAME_PATH` of udev), such as `enp3s0`, which is the name they get on the installed system; the live system may name them otherwise. Virtual interfaces, such as bridges, are left out. Each interface is shown with its name on the live system and its IPv4 addresses.

Disks are picked by number from a list of the whole disks, in the order of their kernel names. Each disk is named by the first it has of: its WWN, the `wwn-` link in `/dev/disk/by-id`; its NVMe EUI, the `nvme-eui.` link there; its kernel node, such as `/dev/vda`. The WWN and the EUI stay the same when the kernel names the disks in another order. The variables hold the full path, and the ZFS pool, the swap and the EFI system partitions are made on the partitions of that name. Partitions, device mapper and md devices, devices that are not disks, such as optical drives, and the disks that are not drives, the zram swap, ZFS volumes, network block devices and RAM disks, are left out, as are empty ones. Each disk is shown with its name, and below it its kernel name, size, model and serial number. A disk that is already picked for the other side of a mirror stays in the list, so the numbers do not change, but cannot be picked again.

## Answers file
With `--answers <file>`, the answers come from a JSON file, for an installation without questions, such as that of a virtual machine. The host name is the name of the file without its extension: `test.json` installs the host `test`. Every other answer is a key of the JSON object. A question that the file does not answer is asked as usual, so a file can answer some questions and leave others, such as the password, to be typed.

The ask helpers in `scripts/helpers.sh`, `helpers-disk.sh` and `helpers-network.sh` check whether the file answers their question. If it does, they show the answer with `(answers file)` in place of the question, check it as they would a typed one, and store it; an answer that is not valid fails the question, and with it the installation. `answers.py` rejects unknown keys, keys given twice and values of the wrong type before any question is asked.

| Key | Questions | Value |
| --- | --- | --- |
| `suite` | `Q_SUITE` | `unstable` or `trixie`. |
| `encrypt` | `Q_ENCRYPT` | `true` or `false`. |
| `system_disks` | `Q_ROOT_REDUNDANT`, `Q_ROOT_DISK_A`, `Q_ROOT_DISK_B` | A list of one disk, or two for a mirror. A disk may be named by any path, such as `/dev/vdb`; it is stored by its name in the list of disks. |
| `swap` | `Q_SWAP` | The swap size in GiB, a whole number, 0 for no swap. |
| `esp_disks` | `Q_ESP_REDUNDANT`, `Q_ESP_DISK_A`, `Q_ESP_DISK_B` | A list of one disk, or two for a mirror. |
| `user` | `Q_USER` | The user name. |
| `password` | `Q_PASSWORD` | The password, in plain text, at least 8 characters. Keep the file readable only by root, or leave the key out to have the password asked. |
| `fullname` | `Q_FULLNAME` | The full name. |
| | `Q_HOSTNAME` | Not a key: the name of the file without its extension. |
| `domain` | `Q_FQDN` | The domain; the FQDN is the host name, a dot and the domain. |
| `iface` | `Q_IFACE` | The wired interface, by its path name, such as `enp1s0`. |
| `wifi` | `Q_WIFI`, `Q_WIFACE` | `false` for no wifi, or the wifi interface by its path name. |
| `wipe` | the wipe | `true` confirms the wipe of every disk picked at once; `false` aborts. |
| `debconf_defaults` | `Q_DEBCONF_DEFAULTS` | `true` or `false`. |
| `debconf` | debconf | A list of lines for `debconf-set-selections`, each `owner question type value`. |

The debconf selections are set in the new system by `b_debconf`, after the Debian repositories are added and before any package is installed but those of `debootstrap`: those of the file, and with `Q_DEBCONF_DEFAULTS` the defaults of `locales`, `tzdata`, `keyboard-configuration` and `console-setup` that the file has no selections of, as the selections of the file go first. apt does not ask a question that is set. `b_console` configures these four packages with `dpkg-reconfigure -f noninteractive` when they have selections, from the file or the defaults, and asks them as before when they have none. `tzdata` prefers `/etc/localtime` and `/etc/timezone` over debconf, and `locales` prefers `/etc/locale.gen`, so those files are removed first and written again from the selections. The config of `console-setup` replaces its selections with the defaults of the locale while `console-setup/store_defaults_in_debconf_db` is true, as it is on a new system, so `b_debconf` sets that to false when `console-setup` has selections. Debconf questions without a selection are asked as usual.

An example, `test.json`:
```json
{
    "suite": "unstable",
    "encrypt": false,
    "system_disks": ["/dev/vda"],
    "swap": 2,
    "esp_disks": ["/dev/vda"],
    "user": "rsq",
    "password": "changeme",
    "fullname": "Erik Van Hamme",
    "domain": "example.org",
    "iface": "enp1s0",
    "wifi": false,
    "wipe": true,
    "debconf": [
        "locales locales/locales_to_be_generated multiselect en_US.UTF-8 UTF-8",
        "locales locales/default_environment_locale select en_US.UTF-8",
        "tzdata tzdata/Areas select Europe",
        "tzdata tzdata/Zones/Europe select Brussels",
        "keyboard-configuration keyboard-configuration/xkb-keymap select be",
        "console-setup console-setup/charmap47 select UTF-8"
    ]
}
```

The answers are read again on every run, as the password is never saved. The path of the file is saved in the config as `ANSWERS_FILE`, so a rerun without `--answers` reads the same file. A question that was answered on an earlier run is not asked again, so to change the answers of a file, start over with `--reset`.

## Disk, partition and filesystem
The disk phase turns off the swap, stops the md arrays, unmounts everything below `/mnt` and exports the ZFS pools of the live system, such as those of an earlier attempt, then wipes every disk picked with `wipefs` and `sgdisk --zap-all`, and discards it with `blkdiscard`. A disk without discard, such as a hardware RAID volume, is not discarded.

The partition phase partitions the disks. The numbers of the partitions are the same on every disk, so every partition has a fixed name, such as `/dev/disk/by-id/wwn-<wwn>-part3`, or `/dev/vda3` and `/dev/nvme0n1p3` for a disk named by its kernel node:

| Partition | Disks | Size | Type |
| --- | --- | --- | --- |
| 1, EFI system partition | the ESP disks | 1 GiB | `EF00` |
| 2, swap | the system disks, without it with no swap | `Q_SWAP` GiB | `8200`, or `FD00` (Linux RAID) on mirrored disks |
| 3, ZFS pool | the system disks | the rest | `BF01` |

A disk that is both an ESP disk and a system disk gets all three. A disk that is only one of them has only its own partitions, so the numbers have gaps.

The filesystem phase makes:
- `/mnt` a private mount, so the mounts below it, the pool and those of the base phase, do not show up in the mount namespaces of the services of the live system, such as udevd and NetworkManager. A copy there would keep the pool busy, and the cleanup phase could not export it;
- a FAT32 file system on every EFI system partition;
- the swap: on mirrored disks an md raid1 array `/dev/md/swap` of the swap partitions. Without encryption it is formatted and turned on; with encryption it is left alone, as the encrypted swap gets a new random key on every boot;
- the ZFS pool `rpool` on the third partitions, a mirror on mirrored disks, with `ashift=12`, `autotrim`, lz4 compression, POSIX ACLs and no access times, and with encryption a passphrase, the password. Its cache file is written to `/etc/zfs/zpool.cache`, which the base phase copies into the new system;
- the datasets `rpool/hyprdebian`, the root of the new system, and `rpool/home`, mounted on `/mnt`.

There is no boot pool: `/boot` is a folder of `rpool/hyprdebian`, as the bootloader reads the kernels, in UKIs, from the EFI system partitions.

## Base and packages
The base phase installs Debian on the root of the new system and prepares it:

| Task | What it does |
| --- | --- |
| `b_clock` | Sets the clock of the live system from the `Date` header of `http://deb.debian.org/debian/`, and the hardware clock with `hwclock` when the live system has it. A wrong clock breaks debootstrap and apt, which refuse Release files that are not yet or no longer valid. Plain http works with a wrong clock, and the mirror is the one debootstrap needs anyway. It runs on every run, as the live system may have been booted again with the wrong clock. |
| `b_bootstrap` | Installs the base system of the suite with the `minbase` variant of `debootstrap`: the required packages, apt, `ca-certificates`, `systemd`, which `b_mount_unit` needs, and `whiptail`, so the debconf questions of `b_console` are asked in dialogs. The important packages of the default variant, such as `cron`, `dhcpcd-base` and `vim-tiny`, stay out; `hd-base` depends on the ones HyprDebian uses. |
| `b_templates` | Writes `/etc/hostname` and `/etc/hosts`. |
| `b_fstab` | Writes `/etc/fstab` with the swap: `/dev/mapper/swap` with encryption, otherwise the swap partition or array by its UUID. |
| `b_mdadm` | On mirrored system disks with swap, writes `/etc/mdadm/mdadm.conf` with the swap array, and marks `mdadm`. |
| `b_crypttab` | With encryption and swap, writes `/etc/crypttab`: the swap gets a new random key on every boot, plain `aes-xts-plain64` with a 512 bit key. The swap is named by its PARTUUID, or on mirrored disks by the UUID of the array, so it can never point at another device. Marks `systemd-cryptsetup`. |
| `b_zfs_cache` | Copies the cache file of the ZFS pool into the new system. |
| `b_mount` | Binds `/dev`, `/proc` and `/sys` and the journal socket into the new system. |
| `b_mount_unit` | Writes and enables the automount units of the EFI system partitions on `/boot/efi` and, on mirrored ESP disks, `/boot/efi2`, by their UUID. |
| `b_live_repo` | Binds the local repository of the live cd, `/srv/hyprdebian/repo`, read-only on `/run/hd-installer/repo` in the new system, a tmpfs, and adds it as the trusted apt source `hyprdebian-live.sources` for the suite. Fails when the live system has no such repository or not the suite. |
| `b_apt_init` | Copies `deploy/etc/apt/apt.conf.d/00hd-installer` into the new system, so apt installs no recommends and downloads no translations of the descriptions while the installer runs, as `hd-base` sets for the installed system. Then it adds the Debian repositories, with the updates and security repositories for a stable suite, and updates the package lists. |
| `b_debconf` | Sets the debconf selections of the answers file, if any, and with `Q_DEBCONF_DEFAULTS` the defaults of the packages it has none of, in the new system. |
| `b_console` | Installs and asks the locales, the time zone, the keyboard and the console font; a package with debconf selections, from the answers file or the defaults, is configured with them, without questions. |
| `b_initramfs` | Installs `initramfs-tools` with `zstd`, so every initramfs is compressed with zstd, and makes the initramfs not wait for a resume device. The kernel headers of `b_kernel` bring the compiler that DKMS builds the ZFS module with. |
| `b_kernel` | Installs the kernel and its headers, `linux-image-amd64` and `linux-headers-amd64`. |
| `b_zfs_support` | Installs ZFS, with its module built by DKMS. |
| `b_esp` | Mounts the EFI system partitions on `/boot/efi` and `/boot/efi2` in the new system. |
| `b_firmware` | Adds `pkg_firmware`, before `pkg_esps`, so the UKIs have the firmware in their initrd. |
| `b_bootloader` | Marks `hd-bootloader-txt` and `efibootmgr`. |
| `b_systemd` | Marks `systemd-timesyncd` and `systemd-resolved`, and adds the user to `adm`, which may read the logs in the systemd journal. There is no syslog daemon. |
| `b_utilities` | Marks `hd-base`, which also holds the network commands and generates the Secure Boot keys. |
| `b_firewall` | Adds `pkg_enable_firewall`, which turns on ufw, which comes with hd-base. |
| `b_network` | Marks `iwd` for wifi, and adds `pkg_network`. The firmware of the wifi card comes from `pkg_firmware`. |

The packages phase installs every marked package in one `apt install`, then runs the steps that the base phase added to `pkg_post`:

| Task | What it does |
| --- | --- |
| `pkg_firmware` | Installs the firmware packages that the hardware needs with `hd-firmware --install` of hd-base, which sees the devices of the live system through `/sys` and looks at the modules of the new kernel. It runs `pkg_utilities` first, for the Contents indexes of apt. |
| `pkg_utilities` | Updates the indices of `apt-file` and `command-not-found`. |
| `pkg_enable_firewall` | Allows ssh in ufw, turns the firewall on and its logging off. ssh is allowed first, so an installation over ssh keeps its connection. |
| `pkg_root_skel` | Copies `/etc/skel` into `/root`, so root gets the files of hd-base there, such as the aliases and the nano config, as the user does in `u_skel`. |
| `pkg_network` | Sets the wired interface with `hd-network-iface set`, and the wifi interface with route metric 200, so wired is preferred; with wifi the boot does not wait for the network. Then renders the files with `hd-network-render` and enables `systemd-networkd`. |
| `pkg_esps` | Writes the bootloader, its assets and the UKIs to the EFI system partitions with `update-esps` of the bootloader, signed with the Secure Boot keys of `hd-base`. |
| `pkg_boot_entries` | Adds the firmware boot entries of the bootloader, `HyprDebian`, or `HyprDebian (primary)` and `HyprDebian (secondary)` on mirrored ESP disks. |

## User and cleanup
The user phase makes the user account:

| Task | What it does |
| --- | --- |
| `u_filesystem` | Makes the dataset `rpool/home/<user>`, the home directory of the user. |
| `u_add` | Adds the user with `useradd`, with the full name and `/bin/bash`, and sets the password with `chpasswd`. A rerun keeps the account and sets the password again. |
| `u_skel` | Fills the home directory from `/etc/skel`, with the files of hd-base. |
| `u_chown` | Gives the home directory to the user. |
| `u_groups` | Adds the user to `audio`, `cdrom`, `dip`, `input`, `plugdev`, `sudo` and `video`, and the groups that other phases added with `add_user_groups`, such as `adm`. |

The cleanup phase hands the installed system over:

| Task | What it does |
| --- | --- |
| `c_live_repo` | Removes the apt source of the local repository of the live cd and the apt config `00hd-installer`, as hd-base has the same settings, and updates the package lists, so the new system keeps only the Debian repositories. |
| `c_apt_clean` | Empties the cache of apt with `apt clean`: the packages that apt and `debootstrap` downloaded, and the caches of the package lists, which apt makes again when it needs them. |
| `c_snapshots` | Takes the snapshot `rpool/hyprdebian@install`, the system as it was installed. |
| `c_unmount` | Unmounts everything below `/mnt`, deepest first, and exports the pool, so the new system imports it when it boots. |
| `c_repo_note` | Shows how to link the new system to a HyprDebian repository with `hd-link-repo`. |

When the installation is done, reboot into the new system. With encryption, the initramfs asks the password before it imports the pool.

## Files
| Path | Purpose |
| --- | --- |
| `/tmp/hd-installer/config` | The answers, the path of the answers file, and the tasks that the answers added. |
| `/tmp/hd-installer/*.done` | The finished tasks. |
| `/usr/lib/hd-installer` | The installer, its helpers and its phases. |
| `/usr/sbin/hd-installer` | A link to `/usr/lib/hd-installer/hd-installer`. |
| `/srv/hyprdebian/repo` | The local repository of the live cd, which the hd-* packages are installed from. |
