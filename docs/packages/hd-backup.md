## Introduction
The package `hd-backup` holds the backup tools of HyprDebian:

| Command | What it does |
| --- | --- |
| `hd-backup` | Writes backups of filesets as tarballs to hard drives or LTO tape, and keeps track of what is on which carrier in a SQLite database per user. |
| `blockwrite` | Writes the tape stream of `hd-backup` in fixed blocks of 4 MiB. |
| `hd-backup-prep` | Turns a whole drive into an encrypted ZFS pool, a carrier for `hd-backup`. |
| `hd-backup-load` | Imports, unlocks and mounts such a drive. |
| `hd-backup-unload` | Exports it, so it can be disconnected. |
| `hd-backup-vm` | Backs up a virtual machine of libvirt whose disks are ZFS volumes. |
| `hd-backup-vms` | Backs up every VM in the key `hd-backup` of `/etc/hyprdebian/config.json`. |
| `hd-restore-vm` | Restores a VM from a backup of `hd-backup-vm`. |

Installing the package stores an empty list of VMs in the key `hd-backup` of `/etc/hyprdebian/config.json`, when the key is missing, and installs `hd-backup-vms.timer`, which runs `hd-backup-vms` every night at 3:30. The timer is not enabled: add the VMs first, then enable it:
```
sudo systemctl enable --now hd-backup-vms.timer
```

Each command has a man page: `man hd-backup`, `man blockwrite`, `man hd-backup-prep`, `man hd-backup-load` (also `man hd-backup-unload`), `man hd-backup-vm`, `man hd-backup-vms` and `man hd-restore-vm`.

## Building the package
The package is built from `hd-backup` in this repository:
```
cd hd-backup
make deb
```

This builds `build/hd-backup_<version>_amd64.deb`, with the version from `version.json`. Raise the version there for every package that is handed out. The package is for amd64, as it holds `blockwrite`, compiled from `blockwrite.c` with `gcc` and the hardening flags of `dpkg-buildflags`; building it needs `gcc` and `dpkg-dev`. It depends on `libc6`, `python3`, `python3-colorama`, `python3-tabulate`, `systemd`, `hd-config`, `zfsutils-linux`, `zstd`, `libvirt-clients` and `udev`, and recommends `mbuffer` and `mt-st`, which `hd-backup` needs for tape. Install it with:
```
sudo apt install ./build/hd-backup_<version>_all.deb
```

The files involved are:
```
hd-backup/Makefile
hd-backup/version.json
hd-backup/hd-backup
hd-backup/blockwrite.c
hd-backup/hd-backup-prep
hd-backup/hd-backup-load
hd-backup/hd-backup-unload
hd-backup/hd-backup-vm
hd-backup/hd-backup-vms
hd-backup/hd-restore-vm
hd-backup/hd-backup-vms.service
hd-backup/hd-backup-vms.timer
hd-backup/deb/control
hd-backup/deb/postinst
hd-backup/deb/prerm
hd-backup/deb/postrm
hd-backup/man/hd-backup.1
hd-backup/man/blockwrite.1
hd-backup/man/hd-backup-prep.8
hd-backup/man/hd-backup-load.8
hd-backup/man/hd-backup-unload.8
hd-backup/man/hd-backup-vm.8
hd-backup/man/hd-backup-vms.8
hd-backup/man/hd-restore-vm.8
hd-backup/tests/test_vm.py
hd-backup/tests/test_blockwrite.py
hd-backup/tests/test_drive.py
hd-backup/tests/fake-command
```

## hd-backup
```
hd-backup command <args...>
```

`hd-backup` backs up filesets, named sets of paths with exclusions, to carriers: mounted hard drives (`write`) or tapes (`write-tape`). The first write of a backup is a full backup, every following write only holds what is not on the backup yet. Its database is `~/.local/share/hyprdebian/hd-backup.db`, one per user; with `sudo`, it is the one of root. The commands are in `man hd-backup`.

### blockwrite
```
blockwrite <device>
```

`hd-backup write-tape` writes its tarballs through `mbuffer` and `blockwrite`. `blockwrite` reads a stream from its standard input and writes it to the device in blocks of exactly 4 MiB, one `write` per block, with `O_DIRECT`, so every block is one record on the tape. The last block is filled up with zeros. A short write is an error and is never continued. At the end, also on an error, it prints `Total bytes written: <N>` on standard error, the bytes written in full blocks, from which `hd-backup` knows how much of a tarball is on a tape that ran full.

## Backup drives
A hard drive for `hd-backup` is a ZFS pool of its own, made with `hd-backup-prep`, which wipes the whole drive:
```
sudo hd-backup-prep /dev/disk/by-id/<drive> <pool>
```

It shows the drive, its size and the pool, and asks before it changes anything. It removes the ZFS labels, the signatures and the partition table, zeroes the first 100 MiB, and creates the pool with native encryption (`aes-256-gcm`, with a passphrase that `zpool` asks for), `zstd` compression and a record size of 4 MiB, mounted at `/mnt/<pool>`. The dataset `<pool>/reserved` reserves a tenth of the drive, to keep part of it free. The mountpoint is given to the user who ran `sudo`, or to `--owner <user>`, so that user can write the backups. Use the name of the pool as the label of the carrier.

After that, the drive is connected and disconnected with:
```
sudo hd-backup-load <pool>
sudo hd-backup-unload <pool>
```

`hd-backup-load` imports the pool with `-f`, so a drive that was pulled without unloading it can be loaded again, asks the passphrase and mounts it. `hd-backup-unload` exports the pool, which unmounts it and forgets its key.

## Backing up VMs
```
sudo hd-backup-vm <vm> <dir> <owner>
```

`hd-backup-vm` backs up the VM `<vm>` to `<dir>/<vm>`. All disks of the VM must be ZFS volumes (`/dev/zvol/...`); a VM with other disks is refused. CD-ROM and floppy drives are left out. A run:
1. Dumps the XML of the VM, and copies its NVRAM when it boots with UEFI.
2. Freezes the filesystems of a running guest with the QEMU guest agent. Without the agent, it warns and takes a crash-consistent snapshot. A VM that is shut off is not frozen: its disks are consistent as they are. A VM in another state, like paused, is not frozen either; it warns and takes a crash-consistent snapshot.
3. Takes the snapshot `backup-<YYYYMMDD_HHMMSS>` of every disk, atomically per pool, and thaws a frozen guest right away.
4. Writes a ZFS stream of every disk through `zstd`: a full stream for a disk without earlier streams, like the first run or a disk added to the VM, otherwise an incremental one from the snapshot of the last backup.
5. Puts the XML and NVRAM in place, records the next sequence number and the new snapshot in `<vm>.sequence`, destroys the older `backup-` snapshots of the disks, and gives every file of the backup to `<owner>`.

The streams and the sequence file are on disk before the old snapshots are destroyed. When a run does not finish, also on a signal, the guest is thawed and the snapshots and files of the run are removed again, so the next run continues from the last backup that did finish. Only one backup of a VM runs at a time; a second one is refused. When the snapshot of the last backup is gone, the backup cannot be continued: move the folder away, and the next run starts with full streams again.

The backup folder holds:

| File | |
| --- | --- |
| `<vm>.xml` | The XML of the VM at the last finished backup. |
| `<vm>_VARS.fd` | Its NVRAM, for a VM that boots with UEFI. |
| `<dataset>.<NNNN>.zst` | The streams of a disk, with the slashes of the dataset replaced by underscores, numbered from `0000`. |
| `<vm>.sequence` | The next sequence number and the snapshot of the last backup. |

### Every night
`hd-backup-vms` runs `hd-backup-vm` for every VM in the key `hd-backup` of `/etc/hyprdebian/config.json`, a JSON array with an object per VM:

| Key | |
| --- | --- |
| `vm` | The name of the VM in libvirt. |
| `dir` | The folder that holds the backup folder of the VM; the backup goes to `<dir>/<vm>`. |
| `owner` | The user who gets the files of the backup. |

```
sudo hd-config-update hd-backup '[{"vm": "git", "dir": "/dpool/backups/vm", "owner": "erik"}]'
```

A VM whose backup fails does not stop the backups of the others. `hd-backup-vms` fails at the end when a backup failed or an entry is invalid, so `hd-backup-vms.service` shows up as failed, for example on the dashboard of `hd-dashboard`. The output is in the journal:
```
journalctl -u hd-backup-vms.service
```

`hd-backup-vms.timer` runs the service every night at 3:30; a run that was missed while the system was off runs at the next boot. The package never enables the timer itself; once the admin enables it, it keeps that state over upgrades.

## Restoring a VM
```
sudo hd-restore-vm <backup_dir> [<new_name>]
```

`hd-restore-vm` restores the VM from its backup folder `<dir>/<vm>`; the name of the folder is the name of the VM in the backup. It restores the disks in the XML of the backup and leaves the streams of removed disks alone. It asks:
- for each disk, the ZFS dataset to restore into; the default is the dataset of the original VM. The streams are received in order with `zfs receive -F`, which overwrites a dataset that already exists;
- whether the network interfaces get new MAC addresses, so a copy does not clash with the original on the network; the default keeps them.

It then defines the VM in libvirt with the XML of the backup, with the disks pointing at the datasets they were restored into, and copies the NVRAM in place. With `<new_name>`, the VM gets that name, a new UUID, and an NVRAM file of its own next to the one of the original. The VM is not started.

## Removing the package
Removing the package stops the timer and keeps the list of VMs and the backups. Purging it also removes the key `hd-backup` of `/etc/hyprdebian/config.json`. The backups and the snapshot of the last backup of each disk stay.

## Tests
The tests are in `hd-backup/tests`. In the tests of the drive tools, the commands they run are replaced by mocks; in the tests of the VM tools, `virsh`, `zfs` and `zstd` are replaced by `tests/fake-command`, which logs its calls and keeps the snapshots in a temporary folder, so they run as a normal user, without libvirt or ZFS. The tests of `blockwrite` build it first and write to a file in `/var/tmp`, as `O_DIRECT` does not work on tmpfs:
```
cd hd-backup
make test
```

`make test` in the root of this repository runs them with the tests of the other packages.

## Files
| Path | Purpose |
| --- | --- |
| `/usr/bin/hd-backup` | Backs up filesets to hard drives and tape. |
| `/usr/bin/blockwrite` | Writes the tape stream in blocks of 4 MiB. |
| `/usr/sbin/hd-backup-prep` | Turns a drive into a backup carrier. |
| `/usr/sbin/hd-backup-load`, `/usr/sbin/hd-backup-unload` | Connect and disconnect a backup drive. |
| `/usr/sbin/hd-backup-vm` | Backs up a VM. |
| `/usr/sbin/hd-backup-vms` | Backs up the VMs in the configuration. |
| `/usr/sbin/hd-restore-vm` | Restores a VM. |
| `/usr/lib/systemd/system/hd-backup-vms.service` | Runs `hd-backup-vms` as root. |
| `/usr/lib/systemd/system/hd-backup-vms.timer` | Runs the service every night at 3:30; not enabled by the package. |
| `/etc/hyprdebian/config.json` | The VMs to back up, in the key `hd-backup`. |
| `~/.local/share/hyprdebian/hd-backup.db` | The database of `hd-backup`, one per user. |
| `/run/lock/hd-backup-vm-<vm>.lock` | The lock of a running backup of a VM. |
