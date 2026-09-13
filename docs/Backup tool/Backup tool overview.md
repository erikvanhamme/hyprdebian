Hyprdebian ships with my own backup tool. This backup tool can store backups both on hard drives intended to be taken offline or on LTO tape.

## Filesets
The backup tool is centered around the concept of a fileset that is to be protected by the backup.

Filesets can be added to the backuptool with:
```
hd-backup add-fileset NAME PATH PATH PATH ^EXCLUSION_PATH ^EXCLUSION_PATH
```

The fileset will look for all files in the paths contained in the fileset. Each file will be checked agains possible exclusions. If the file is part of the fileset, the following properties of it will be stored in the database.

- Filename
- Mtime
- Size
- SHA-1 hash

A fileset can be updated with:
```
hd-backup update-filesets FILESET_NAME FILESET_NAME
```

The update will check the Mtime of the file. If the file is updated, it will be hashed again, and the update stored in the database.

The fileset will also store directories and symlinks properly.

A known weakness of the tool is hardlinks. After a restore of the backup, these will manifest as 2 copies of the same file. The hardlink will not be restored.

Deletes are tracked. These are stored in the deletelist that is added to each backup carries.

A path (or exclusion) can be added to the fileset with:
```
hd-backup add-path FILESET_NAME PATH PATH ^EXCLUSION_PATH
```

## Backups
A backup protects a fileset. It is perfectly possible to have multiple backups of the same fileset. The backup tool will track for the fileset what changes are applicable to the backup since the last update.

You can add multiple backups on a fileset, for example, a backup on tape and one on harddrives/network locations.

Backups can be added with:
```
hd-backup add-backup FILESET_NAME BACKUP_NAME
```

## Carriers
Each backup is written to a number of carriers. The carriers contain the tarballs with the files to be backed up.