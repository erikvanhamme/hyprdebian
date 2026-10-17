## Introduction
The package `wiremix` holds [wiremix](https://github.com/tsowell/wiremix), the terminal audio mixer for PipeWire, built from release v0.9.0. Debian has no package of it, so HyprDebian builds one from the upstream release, unchanged.

wiremix sets the volume of devices and streams, routes streams between devices, and sets the ports and profiles of devices. Run it in a terminal:
```
wiremix
```

The package also installs the desktop entry of the release, and the readme and the reference of the configuration file in `/usr/share/doc/wiremix`.

## Configuration
wiremix works without a configuration file. To change its settings or keys, make `~/.config/wiremix/wiremix.toml`. Anything in it is merged with the defaults, so start with an empty file and copy only the settings to change from the reference:
```
/usr/share/doc/wiremix/examples/wiremix.toml
```

The options on the command line override the file; see `wiremix --help`.

## Building the package
The package is built from `wiremix` in this repository:
```
cd wiremix
make deb
```

This builds `build/wiremix_<release>-<revision>_<arch>.deb`. Install it with:
```
sudo apt install ./build/wiremix_0.9.0-1_amd64.deb
```

The binary is compiled in a Docker container, so the build needs `docker` and a user that may run it, besides `dpkg-deb`. `build-wiremix.sh` runs in a `debian:trixie` container: it installs the build tools, `libpipewire-0.3-dev` and the latest stable Rust toolchain with rustup, clones the release tag and runs `cargo build --release --locked`. `dpkg-shlibdeps` then finds the packages of the libraries the binary links to, which become the Depends of the package. The results land in `build/dist-<release>`; later builds reuse them, until `make clean` removes them.

The container is trixie because the binary needs the libraries of the release it is compiled on: built on trixie, it also runs on unstable.

To move to a new release of wiremix, change `WIREMIX_VERSION` in the Makefile, set `DEB_REVISION` back to 1, and update the release on this page. For a new package of the same release, raise `DEB_REVISION`.

The files involved are:
```
wiremix/Makefile
wiremix/build-wiremix.sh
wiremix/deb/control
wiremix/deb/copyright
```

## Copyright
wiremix is by Thomas Sowell, under the MIT license or the Apache License 2.0, at your choice. The binary links in Rust crates, each under its own license. See `/usr/share/doc/wiremix/copyright`, and `LICENSE-MIT` and `LICENSE-APACHE` beside it.

## Files
| Path | Purpose |
| --- | --- |
| `/usr/bin/wiremix` | The mixer. |
| `/usr/share/applications/wiremix.desktop` | The desktop entry. |
| `/usr/share/doc/wiremix/examples/wiremix.toml` | The reference of the configuration file, with the defaults. |
| `/usr/share/doc/wiremix/README.md` | The readme of the release. |
| `/usr/share/doc/wiremix/copyright`, `LICENSE-MIT`, `LICENSE-APACHE` | The copyright holder and licenses. |
| `~/.config/wiremix/wiremix.toml` | The configuration file of a user, when there is one. |
