## Migrations
A migration is a one-off change that `upgrade.sh` makes to systems that were installed or upgraded before the change, like masking a service, removing a file or a package that is no longer used, or moving a config file.

Changes that the queues describe do not need a migration: new packages, services, user groups and files are picked up by the upgrade itself. A migration is needed for everything else the installer does with commands, and for anything that is removed.

## Writing a migration
Add a file `NNNN-short-description.sh`, with the next free number, that defines a `migrate` function:
```
#!/bin/bash

# Stops using the foo service, it is replaced by bar.
migrate() {
    # Not needed when the service is already gone.
    if [[ ! -e ${TARGET_DIR}/etc/systemd/system/foo.service ]]; then
        return 0
    fi

    in_target systemctl disable --now foo.service || return 1
    rm -f ${TARGET_DIR}/etc/systemd/system/foo.service
}
```

- `migrate` is written like a task: it returns non-zero on failure, so use `|| return 1` between commands.
- The answers (`Q_*`) are available, so a migration can check whether a feature is enabled.
- The queues of the new version (`PACKAGES`, `SERVICES`, ...) are worked out before the migrations run, so a migration can check what the new version installs on the system.
- Use `in_target` and `${TARGET_DIR}`, like the installer tasks.
- A migration that needs a package the same version adds, installs it itself: migrations run before the new packages are installed.
- Start `migrate` with a check whether the change is needed, and return 0 when it is not. A migration also runs on systems that already have the change, like systems installed after the change went into the installer but before the migration was added. Returning 0 records it as done, just like a migration that made the change.
- Never change or renumber a migration after it was committed. Systems that already ran it will not run it again.
- Add a row for the migration to [docs/Installer/Migrations.md](../docs/Installer/Migrations.md).

## How they run
The upgrade runs the migrations that did not run on the system yet, in the order of their numbers, before it installs the new packages and updates the files. Each migration that succeeds is added to `/etc/hyprdebian/migrations`. When a migration fails, the upgrade stops, and the migration runs again on the next upgrade.

A new installation records all migrations that exist at that time as done, because it already has their changes.
