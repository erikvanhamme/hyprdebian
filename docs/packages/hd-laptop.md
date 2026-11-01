## Introduction
The package `hd-laptop` holds the parts of HyprDebian that only laptops need. It depends on `hd-desktop`, as its notifications are shown by mako in the desktop session.

## Battery
`hd-battery` shows the level of the battery in a notification of mako, with whether the charger is plugged in and the battery charges, and the time left until it is empty or full. The key binding `SUPER + SHIFT + B` runs it; the binding is in the config of Hyprland of every system, and does nothing without `hd-laptop`.

The user unit `hd-battery-monitor.service` runs `hd-battery --monitor` in the desktop session. It shows a notification:

- when the battery falls to 50%, 25%, 10% and 5% on battery power, with the time left until it is empty;
- when the charger is pulled out, with the level and the time left until the battery is empty;
- when the charger is plugged in, with the level and the time until the battery is full, or that it is full or not charging;
- at the login, when the laptop runs on battery power at 50% or less.

The notifications on battery power at 10% and below are critical and have the warning icon. Each warning is shown once until the charger is plugged in again; the warnings of the levels that the battery is already at or below when the charger is pulled out are not shown, as that notification gives the level. Each notification takes the place of the one before it.

The monitor reads the batteries and the chargers in `/sys/class/power_supply`, when the kernel sends an event of a power supply and every 30 seconds. Batteries of devices, such as a mouse, do not count; two batteries in a laptop count as one. The time left comes from the power that the batteries give, as the firmware tells it, so it changes with the load. A battery that does not tell its power gives only its level.

The package enables the unit for every user on its first installation; it starts at the next login. Its messages are in the journal of the user:
```
journalctl --user -u hd-battery-monitor
```

To turn it off for a user:
```
systemctl --user disable --now hd-battery-monitor
```

The package comes with man pages: `man hd-laptop` and `man hd-battery`.

## Building the package
The package is built from `packages/hd-laptop` in this repository:
```
cd packages/hd-laptop
make deb
```

This builds `build/hd-laptop_<version>_all.deb`, with the version from `version.json`. Raise the version there for every package that is handed out. It depends on `python3`, `hd-desktop` and `libnotify-bin`. Install it with:
```
sudo apt install ./build/hd-laptop_<version>_all.deb
```

The files involved are:
```
packages/hd-laptop/Makefile
packages/hd-laptop/version.json
packages/hd-laptop/hd-battery
packages/hd-laptop/usr/lib/systemd/user/hd-battery-monitor.service
packages/hd-laptop/deb/control
packages/hd-laptop/deb/postinst
packages/hd-laptop/deb/postrm
packages/hd-laptop/man/hd-laptop.7
packages/hd-laptop/man/hd-battery.1
```
