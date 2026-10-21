## Introduction
The package `hd-base` keeps the configuration of hyprdebian in `/etc/hyprdebian/config.json`, the configuration of the system. Every user may also have a configuration of their own, in `~/.config/hyprdebian/config.json`. Each holds a JSON object. Each hyprdebian package that has settings keeps them under a key of its own in that object, usually its package name:
```
{
    "hd-example": {
        "enabled": true,
        "port": 8080
    }
}
```

The files are changed with two commands:
- `hd-config-update` adds a key, or replaces its value;
- `hd-config-remove` removes keys.

Who runs them decides which configuration they change: root changes the configuration of the system, any other user their own configuration. Packages run them as root in their maintainer scripts, so their settings go to the configuration of the system.

Most settings are of the system, and only the configuration of the system is read for them. A program that also reads the configuration of the user says so: it reads the key there first and, when the configuration of the user does not have it, in the configuration of the system. The key `hd-theme` of [hd-theme](hd-theme.md) is read that way.

`hd-base` comes with three man pages for it: `man hd-config` describes the configuration and how packages use it, `man hd-config-update` and `man hd-config-remove` the commands.

## Building the package
The commands are part of the package `hd-base`, built from `packages/hd-base`; see [hd-base](hd-base.md). The files involved are:
```
packages/hd-base/hd-config-update
packages/hd-base/hd-config-remove
packages/hd-base/man/hd-config.7
packages/hd-base/man/hd-config-update.1
packages/hd-base/man/hd-config-remove.1
```
## hd-config-update
```
hd-config-update <key> [json]
```

`hd-config-update` sets `key` in the configuration to the JSON value `json`. When the key is not there yet, it is added; when it is, its old value is replaced as a whole. The other keys stay as they are. Without `json`, the value is read from standard input.

The value is JSON text: an object, an array, a string in double quotes, a number, `true`, `false` or `null`. The key may not be empty.

When the configuration does not exist yet, it is made, and its directory with it: `/etc/hyprdebian` for root, `~/.config/hyprdebian` for other users. The file is written with mode 0644.

```
hd-config-update hd-example '{"enabled": true, "port": 8080}'
hd-config-update hostname '"repo.example"'
hd-config-update hd-example < settings.json
```

## hd-config-remove
```
hd-config-remove <key>...
```

`hd-config-remove` removes each `key` and its value from the configuration. Only keys of the object itself are removed; it does not reach into the values. A key that is not there is skipped, and when the file does not exist, nothing is done. Neither is an error, so a package can remove its keys whether or not it ever set them.

```
hd-config-remove hd-example
```

Both commands exit with 0 on success, 1 on an error and 2 on wrong arguments. On an error the file is left as it was.

## How the file is written
Both commands write the configuration as a whole to `config.json.tmp` next to it and move that into place, so the file is never seen half written. They hold a lock on the directory of the file while they change it, so two changes at the same time do not lose each other's work.

A file is made by the first `hd-config-update` that writes it; the package itself installs no configuration. It is not a conffile: removing or purging `hd-base` keeps it.

## Using it from a package
A package that keeps its settings in the configuration depends on `hd-base` and sets its key in its postinst:
```
hd-config-update hd-example '{"enabled": true, "port": 8080}'
```

It removes the key in its postrm on a purge. By then `hd-base` may already be removed, so it checks first:
```
if [[ "${1:-}" == "purge" && -x /usr/bin/hd-config-remove ]]; then
    hd-config-remove hd-example
fi
```

To read a setting, load the file with any JSON parser:
```
python3 -c 'import json; print(json.load(open("/etc/hyprdebian/config.json"))["hd-example"]["port"])'
```

## Files
| Path | Purpose |
| --- | --- |
| `/etc/hyprdebian/config.json` | The configuration of the system, a JSON object, changed by root. |
| `~/.config/hyprdebian/config.json` | The configuration of a user, a JSON object, changed by that user. |
| `/usr/bin/hd-config-update` | Adds a key or replaces its value. |
| `/usr/bin/hd-config-remove` | Removes keys. |
