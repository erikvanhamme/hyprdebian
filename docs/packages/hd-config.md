## Introduction
The package `hd-config` keeps the configuration of hyprdebian in one file, `/etc/hyprdebian/config.json`. It holds a JSON object. Each hyprdebian package that has settings keeps them under a key of its own in that object, usually its package name:
```
{
    "hd-example": {
        "enabled": true,
        "port": 8080
    }
}
```

The file is changed with two commands, both run as root:
- `hd-config-update` adds a key, or replaces its value;
- `hd-config-remove` removes keys.

The package comes with three man pages: `man hd-config` describes the configuration and how packages use it, `man hd-config-update` and `man hd-config-remove` the commands.

## Building the package
The package is built from `hd-config` in this repository:
```
cd hd-config
make deb
```

This builds `build/hd-config_<version>_all.deb`, with the version from `version.json`. Raise the version there for every package that is handed out. It depends on `python3` only. Install it with:
```
sudo apt install ./build/hd-config_<version>_all.deb
```

The files involved are:
```
hd-config/Makefile
hd-config/version.json
hd-config/hd-config-update
hd-config/hd-config-remove
hd-config/deb/control
hd-config/man/hd-config.7
hd-config/man/hd-config-update.8
hd-config/man/hd-config-remove.8
```

## hd-config-update
```
hd-config-update <key> [json]
```

`hd-config-update` sets `key` in the configuration to the JSON value `json`. When the key is not there yet, it is added; when it is, its old value is replaced as a whole. The other keys stay as they are. Without `json`, the value is read from standard input.

The value is JSON text: an object, an array, a string in double quotes, a number, `true`, `false` or `null`. The key may not be empty.

When `/etc/hyprdebian/config.json` does not exist yet, it is made, and `/etc/hyprdebian` with it. The file is written with mode 0644.

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
Both commands write the configuration as a whole to `/etc/hyprdebian/config.json.tmp` and move that into place, so the file is never seen half written. They hold a lock on `/etc/hyprdebian` while they change it, so two changes at the same time do not lose each other's work.

The file is made by the first `hd-config-update`; the package itself installs no configuration. It is not a conffile: removing or purging `hd-config` keeps it.

## Using it from a package
A package that keeps its settings in the configuration depends on `hd-config` and sets its key in its postinst:
```
hd-config-update hd-example '{"enabled": true, "port": 8080}'
```

It removes the key in its postrm on a purge. By then `hd-config` may already be removed, so it checks first:
```
if [[ "${1:-}" == "purge" && -x /usr/sbin/hd-config-remove ]]; then
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
| `/etc/hyprdebian/config.json` | The configuration, a JSON object. |
| `/usr/sbin/hd-config-update` | Adds a key or replaces its value. |
| `/usr/sbin/hd-config-remove` | Removes keys. |
