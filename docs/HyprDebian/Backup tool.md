## Introduction
`hd-backup` is the backup tool of hyprdebian. It writes backups as tarballs, either to hard drives that are taken offline after the backup, or to LTO tape.

The tool keeps track of every file it protects in a SQLite database. A backup only writes what is not on the backup yet: the first write is a full backup, every following write is incremental. When a carrier (a drive or a tape) is full, the backup continues on the next carrier.

The tool is installed with the package `hd-backup`, together with `blockwrite`, which writes a stream to tape in fixed 4 MiB blocks. See [hd-backup](../packages/hd-backup.md) for the package, its files and the backups of VMs.

## Concepts
| Concept | Meaning |
| --- | --- |
| Fileset | A named set of paths to protect, with optional exclusions. The tool keeps a record of every file, directory and symlink in it. |
| Backup | A named backup of one fileset. A fileset can have several backups, for example one on tape and one on hard drives. Each backup tracks on its own what it already holds. |
| Carrier | A drive or tape that belongs to a backup, identified by a label. A backup has one or more carriers, used one after the other. |
| Tarball | One write to a carrier. Every `write` or `write-tape` adds one tarball to a carrier. |

```
fileset ──< backup ──< carrier ──< tarball ──< files and symlinks
```

## Database
The database is at:
```
~/.local/share/hyprdebian/hd-backup.db
```

It is created on the first run. Because it is in the home directory, every user has their own database. A command run with `sudo` uses the database of root, not the one of the user.

The database has a version number. The tool refuses to work with a database of a different version (`Incompatible database version`). The current versions are shown in the start banner:
```
Backuptool [tool V13, database V8]
```

The database is the only record of what is on which carrier. Keep a copy of it somewhere else than on the system it protects, for example on the backup carriers themselves.

## Commands
```
hd-backup command <args...>
```

| Command | Arguments | What it does |
| --- | --- | --- |
| `add-fileset` | `FILESET PATH...` | Adds a fileset with the given paths, then scans it. |
| `add-paths` | `FILESET PATH...` | Adds paths to an existing fileset, then scans it. |
| `update-filesets` | `FILESET...` | Scans the filesets again, to pick up new, changed and removed files. |
| `add-backup` | `BACKUP FILESET` | Adds a backup of a fileset. |
| `rename-backup` | `OLD_NAME NEW_NAME` | Renames a backup. |
| `add-carrier` | `BACKUP LABEL...` | Adds carriers to a backup, in the given order. |
| `write` | `BACKUP MOUNTPOINT` | Writes a tarball of the backup to a mounted drive. |
| `write-tape` | `BACKUP DEVICE` | Writes a tarball of the backup to a tape drive, like `/dev/nst0`. |
| `summary` | `[BACKUP...]` | Shows the state of the backups. Without arguments it shows all of them. The names can contain `*` as a wildcard. |
| `list-filesets` | | Lists the filesets. |
| `list-backups` | | Lists the backups and their fileset. |
| `list-carrier` | | Lists the carriers. |
| `list-tarballs` | | Lists the tarballs, with their size and when they were written. |
| `manifest` | `LABEL` | Lists the tarballs on a carrier and every file and symlink in them. |
| `remove-carrier` | `LABEL...` | Removes carriers, with their tarballs, from the database. |
| `remove-backups` | `BACKUP...` | Removes backups, with their carriers and tarballs, from the database. |

A command exits with status 1 when it fails, or when it reports an error and goes on with the rest of its work, like a fileset name that does not exist in `update-filesets`.

The remove commands only change the database. They do not touch the files on disk or on the carriers, and they do not remove filesets.

## Filesets
A fileset is created with:
```
hd-backup add-fileset FILESET PATH... ^EXCLUSION...
```

A path that starts with `^` is an exclusion. Every path, exclusions included, must be an existing directory, and is stored as an absolute path. More paths and exclusions can be added later with:
```
hd-backup add-paths FILESET PATH... ^EXCLUSION...
```

For example:
```
hd-backup add-fileset home /home/erik ^/home/erik/.cache ^/home/erik/Downloads
```

Quote an exclusion if the shell could expand it. An exclusion matches every path that starts with the same text. `^/home/erik/tmp` also excludes `/home/erik/tmpfiles`. The scan does not enter excluded directories.

### Scanning
`add-fileset` and `add-paths` scan the fileset right away. After that, scan it again before every write:
```
hd-backup update-filesets FILESET...
```

The scan walks all the paths of the fileset, skips the exclusions, and records:

| Item | Recorded |
| --- | --- |
| File | Path, size, mtime and SHA-1 hash. |
| Directory | Path and mtime. It is stored as a file without a hash, and is written to the backup as an empty directory entry, without its contents. |
| Symlink | Path and target. The link itself is backed up, not what it points to. |

A file counts as unchanged when its path, size and mtime match the database. Only new and changed files are hashed again, so a scan of a large fileset with few changes is fast.

A changed file that is already on a carrier gets a new record, so the old version stays known as part of that carrier. Only the newest record counts as on disk. A changed file that is not on any carrier yet is updated in place.

A file that is gone from disk is removed from the database when it is on no carrier. When it is on a carrier, it is kept and marked as not on disk, so the next write can record the delete. See [Deletes](#deletes).

The scan warns about:
- Possible hardlinks (files with more than one link). They are backed up as separate files. After a restore they are separate copies, not hardlinks.
- FIFOs, sockets and device files. They are skipped.
- Files and directories it cannot read, for example because of their permissions, and names that are not valid UTF-8. They are skipped, and the command exits with status 1. Fix the permissions, or exclude them.

## Backups and carriers
A backup is added with:
```
hd-backup add-backup BACKUP FILESET
```

Then give it carriers:
```
hd-backup add-carrier BACKUP LABEL...
```

A label must be unique over all backups. For tape, use the barcode label of the tape. For a hard drive, use the pool name given to `hd-backup-prep`.

A write always goes to the first carrier of the backup that is not full, by label in alphabetical order. The tool does not check which drive is mounted or which tape is loaded: run `hd-backup summary BACKUP` first, and make sure the right carrier is in place. Labels that sort in the order of use, like `home-01`, `home-02`, avoid surprises.

When a carrier runs full during a write, it is marked full, and the next write goes to the next carrier. When all carriers of a backup are full, the write stops with `There are no carrier available`: add more with `add-carrier`.

## Writing a backup
A write takes every file and symlink of the fileset that is on disk and is not on a carrier of this backup yet, and writes them in one tarball.

### To a hard drive
```
hd-backup write BACKUP MOUNTPOINT
```

The tarball is written as `MOUNTPOINT/backup-N.tar`, where `N` is the index of the tarball on the carrier, starting at `0`.

### To tape
```
hd-backup write-tape BACKUP DEVICE
```

Use the non-rewinding device, like `/dev/nst0`. The tool:
1. Sets the block size of the drive to 4 MiB and turns on hardware compression (`mt-st`).
2. Skips forward over the tarballs already on the tape (`mt-st fsf N`). The tape must be rewound when the write starts.
3. Writes the tarball through `mbuffer` (8 GiB memory buffer, to keep the drive streaming) and `blockwrite` (fixed 4 MiB blocks with direct I/O). The log of `mbuffer` goes to a temporary file in `/tmp`. It is removed after the write, unless the write failed: then the error message names it.
4. Rewinds the tape, also when the write failed.

Every tarball is one file on the tape. Tarball `N` is file number `N`.

### What a write does
1. Picks the carrier and the next tarball index.
2. Writes the delete list to `/tmp/deletes_<date>_<time>_<random>.py`, removed after the write. See [Deletes](#deletes).
3. Writes the list of items to a temporary file in `/tmp`, removed after the write: the delete list first, then the symlinks, then the files, sorted by path. The names are separated by null characters, so a name can contain any character. If a file in the list no longer exists, the write stops with `Update the fileset first`.
4. Runs `tar` in pax format, with 4 MiB records, without recursion (every directory is its own entry), relative to `/` and with hardlinks stored as separate files.
5. Records the tarball and its items in the database.

The time the write started and finished is printed, and at the end the number of files and symlinks backed up.

### When a carrier runs full
When `tar` (on a hard drive) or `blockwrite` (on tape) reports `No space left on device`, or `tar` reports that it `Wrote only` part of a record, the write is not treated as an error. A hard drive that runs full usually takes part of the last record first, so `tar` reports the short write. The tool works out from the number of bytes that reached the carrier which items fit completely, records only those as written, and marks the carrier full. On a hard drive, the count is the size of the tarball file, which includes the record that was written in part. On tape, the count comes from `blockwrite`, because `tar` also counts what is still in the buffer of `mbuffer`. The size of every item in the tarball, headers included, is worked out from its name, link target, size, owner and times when the write starts, the way `tar` writes them. It is exact, except that the times are counted with all 9 decimals: `tar` leaves out trailing zeros, so an item can come out one block smaller than counted, never larger. The items that are not recorded are written by the next write, on the next carrier.

On a hard drive, the incomplete tarball stays on the drive. The items at its end that are not recorded in the database are in it, but may be cut off.

Any other error stops the tool, and the database is not changed. That includes a file that `tar` cannot read: fix its permissions or exclude it, and run the write again. A file that changes while `tar` reads it is only a warning: it is in the tarball, but its contents may be mixed.

### Typical run
```
hd-backup-load home-01
hd-backup update-filesets home
hd-backup summary home-disk
hd-backup write home-disk /mnt/home-01
hd-backup-unload home-01
```

If the drive runs full, connect the next carrier and run `write` again.

## Deletes
Every tarball contains a delete list: a small Python script with the files, directories and symlinks of the fileset that were deleted after they were written to a carrier of this backup. A path that was deleted and later created again is not in it. The list is written in the tarball as `tmp/deletes_<date>_<time>_<random>.py`. Every write has a name of its own, so a restore of several tarballs has all their delete lists in `R/tmp`. The one to run is the one of the last tarball, with the latest time in its name. `tar -tf` on the last tarball shows its name.

The script removes its paths relative to the parent of its own directory. When the tarball is restored into a directory `R`, the list is at `R/tmp/`, and removes paths under `R`. It can be run from any directory:
```
python R/tmp/deletes_<date>_<time>_<random>.py
```

It removes the contents of a directory before the directory itself. It skips paths that do not exist, and reports the paths it cannot remove, for example a directory that still holds files that were not backed up. It is a help for a restore, not a guarantee.

## Status
```
hd-backup summary [BACKUP...]
```

For every backup, the summary shows:
- Whether all files and symlinks of the fileset are on its carriers, or how many are not, and their size.
- The fileset, its number of files, its total size and its paths.
- Every carrier, whether it is full, and the tarballs on it with their number of files and symlinks, their size, their overhead and when they were written.

`manifest LABEL` lists everything on one carrier, file by file. It is the place to look to find which carrier holds a file.

## Restoring
The backups are plain pax tarballs, so a restore needs only `tar`. The paths in them are relative to `/`: extract them into a directory, not into `/` directly, and copy back what is needed.

Extract the tarballs in the order they were written (see `list-tarballs` or `summary`), over all carriers of the backup. A later tarball holds the newer version of a changed file, and overwrites the older one.

From a hard drive:
```
mkdir -p /restore
tar -xpf /mnt/home-01/backup-0.tar -C /restore
```

From tape, for tarball `N`:
```
mt-st -f /dev/nst0 rewind
mt-st -f /dev/nst0 setblk 4194304
mt-st -f /dev/nst0 fsf N
tar -b 8192 -xpf /dev/nst0 -C /restore
```

After the last tarball, run its delete list (see [Deletes](#deletes)) to remove what was deleted before the last backup.

## Preparing a hard drive
`hd-backup-prep` turns a whole drive into a backup carrier. It destroys everything on the drive, after asking:
```
sudo hd-backup-prep DEVICE POOL
```

It wipes the drive and creates a ZFS pool on it, mounted at `/mnt/POOL`:
- Native encryption (`aes-256-gcm`) with a passphrase, asked when the pool is created.
- `zstd` compression and a record size of 4 MiB, to match the tarballs.
- A dataset `POOL/reserved` that reserves a tenth of the drive, to keep part of it free.
- The mountpoint is owned by the user that ran `sudo`, or by `--owner USER`.

Use the pool name as the carrier label. After that, the drive is used with:
```
sudo hd-backup-load POOL
sudo hd-backup-unload POOL
```

`hd-backup-load` imports the pool, asks the passphrase and mounts it at `/mnt/POOL`. `hd-backup-unload` exports the pool, so the drive can be disconnected.

## Limitations
- Hardlinks are backed up as separate files.
- FIFOs, sockets and device files are not backed up.
- Names that are not valid UTF-8 are not supported.
- File permissions and ownership are stored by `tar` in the tarball, but are not tracked in the database: a change of only the permissions of a file is not seen as a change.
- The tool does not check which carrier is mounted or loaded. See [Backups and carriers](#backups-and-carriers).
