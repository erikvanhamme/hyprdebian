## Introduction
The package `wiremix` holds [wiremix](https://github.com/tsowell/wiremix), the terminal audio mixer for PipeWire, built from release v0.11.0. Debian has no package of it, so HyprDebian builds one from the upstream release, unchanged.

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
The package is built from `packages/wiremix` in this repository:
```
cd packages/wiremix
make deb
```

This builds `build/wiremix_<release>-<revision>_<arch>.deb`. Install it with:
```
sudo apt install ./build/wiremix_0.11.0-2_amd64.deb
```

The binary is compiled in a container of the build image of unstable, `hd-build:unstable` of `buildenv/Dockerfile`, so the build needs `docker` and a user that may run it, besides `dpkg-deb`. `make deb` builds the image first, with the latest `debian:unstable`; docker only installs its packages again when that image or the Dockerfile has changed, and removes the build images that a new one replaced. The image has the build tools, `libpipewire-0.3-dev`, `libclang-dev` for the bindings of it and rustup, so `build-wiremix.sh` installs nothing: it runs `rustup update stable`, so wiremix is compiled with the latest stable Rust toolchain, clones the release tag and runs `cargo build --release --locked`. `dpkg-shlibdeps` then finds the packages of the libraries the binary links to, which become the Depends of the package. The results land in `build/dist-<release>`; later builds reuse them, until `make clean` removes them.

The package goes to the repo of unstable only: [hd-desktop](hd-desktop.md), which depends on it, is for unstable only. It is built on unstable, so it gets the libraries of unstable as hd-desktop does.

To move to a new release of wiremix, change `WIREMIX_VERSION` in the Makefile, set `DEB_REVISION` back to 1, and update the release on this page. For a new package of the same release, raise `DEB_REVISION`.

The files involved are:
```
packages/wiremix/Makefile
packages/wiremix/build-wiremix.sh
packages/wiremix/deb/control
packages/wiremix/deb/copyright
buildenv/Dockerfile
buildenv/buildenv.mk
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
