## Introduction
The package `hd-cups` installs CUPS, from `cups`, the print server. Printers are set up through the web interface of CUPS, on http://localhost:631.

It conflicts with `avahi-daemon`, which `cups` recommends, and `sane-utils`, the scanner tools, so they are not installed with it. Without `avahi-daemon`, printers on the network are not found by themselves; add them in the web interface by their address.

It does not add anyone to the group `lpadmin`. To manage the printers, add your user to it and log in again:
```
sudo usermod -a -G lpadmin <user>
```

The package comes with a man page: `man hd-cups`.

## Building the package
The package is built from `hd-cups` in this repository:
```
cd hd-cups
make deb
```

This builds `build/hd-cups_<version>_all.deb`, with the version from `version.json`. Raise the version there for every package that is handed out. It depends on `cups`. Install it with:
```
sudo apt install ./build/hd-cups_<version>_all.deb
```

The files involved are:
```
hd-cups/Makefile
hd-cups/version.json
hd-cups/deb/control
hd-cups/man/hd-cups.7
```
