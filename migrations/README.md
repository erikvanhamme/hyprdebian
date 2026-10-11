## Migrations
A migration is a one-off change that `upgrade.sh` makes to systems that were installed or upgraded before the change, like masking a service, removing a file or a package that is no longer used, or moving a config file.

Changes that the queues describe do not need a migration: new packages, services, user groups and files are picked up by the upgrade itself. A migration is needed for everything else the installer does with commands, and for anything that is removed.

## Writing a migration
Add a file `NNNN-short-description.sh`, with the next free number, that defines a `migrate` function:
```
#!/bin/bash

# Stops using the foo service, it is replaced by bar.
migrate() {
    in_target systemctl disable --now foo.service || return 1
    rm -f ${TARGET_DIR}/etc/systemd/system/foo.service
}
```

- `migrate` is written like a task: it returns non-zero on failure, so use `|| return 1` between commands.
- The answers (`Q_*`) are available, so a migration can check whether a feature is enabled.
- Use `in_target` and `${TARGET_DIR}`, like the installer tasks.
- A migration that needs a package the same version adds, installs it itself: migrations run before the new packages are installed.
- Write it so it also works on a system where the change is already done, for example on a system that was installed before the record of migrations existed.
- Never change or renumber a migration after it was committed. Systems that already ran it will not run it again.

## How they run
The upgrade runs the migrations that did not run on the system yet, in the order of their numbers, before it installs the new packages and updates the files. Each migration that succeeds is added to `/etc/hyprdebian/migrations`. When a migration fails, the upgrade stops, and the migration runs again on the next upgrade.

A new installation records all migrations that exist at that time as done, because it already has their changes.
