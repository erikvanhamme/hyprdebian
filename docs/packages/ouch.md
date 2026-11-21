## Introduction
The package `ouch` holds [ouch](https://github.com/ouch-org/ouch), the compression and decompression tool, built from release 0.8.3. Debian has no package of it, so HyprDebian builds one from the upstream release, unchanged.

ouch compresses and decompresses files and directories in one command, and finds the format from the extension of the file:
```
ouch decompress photos.tar.gz
ouch compress notes/ todo.txt backup.zip
ouch list archive.7z
```

It handles tar, zip, 7z, gzip, snappy, zstd, xz, lzma, lzip, bzip2, bzip3, lz4 and brotli both ways, and RAR for decompressing and listing only.

[hd-desktop](hd-desktop.md) depends on it, and [yazi](yazi.md) previews and extracts archives with it, in place of 7-zip.

The package also installs the man pages of the release (`man ouch`) and its completions for bash, zsh and fish.

## Building the package
The package is built from `packages/ouch` in this repository:
```
cd packages/ouch
make deb
```

This builds `build/ouch_<release>-<revision>_<arch>.deb`. Install it with:
```
sudo apt install ./build/ouch_0.8.3-3_amd64.deb
```

The binary is compiled in a container of the build image of unstable, `hd-build:unstable` of `buildenv/Dockerfile`, so the build needs `docker` and a user that may run it, besides `dpkg-deb`. `make deb` builds the image first, with the latest `debian:unstable`; docker only installs its packages again when that image or the Dockerfile has changed, and removes the build images that a new one replaced. The image has the build tools and rustup, so `build-ouch.sh` installs nothing: it runs `rustup update stable`, so ouch is compiled with the latest stable Rust toolchain and not the `rustc` of Debian, clones the release tag and runs `cargo build --release --locked`, which also writes the man pages and completions. `dpkg-shlibdeps` then finds the packages of the libraries the binary links to, which become the Depends of the package. The results land in `build/dist-<release>`; later builds reuse them, until `make clean` removes them.

The package goes to the repo of unstable only: [hd-desktop](hd-desktop.md), which depends on it, is for unstable only. It is built on unstable, so it gets the libraries of unstable as hd-desktop does.

To move to a new release of ouch, change `OUCH_VERSION` in the Makefile, set `DEB_REVISION` back to 1, and update the release on this page. For a new package of the same release, raise `DEB_REVISION`.

The files involved are:
```
packages/ouch/Makefile
packages/ouch/build-ouch.sh
packages/ouch/deb/control
packages/ouch/deb/copyright
buildenv/Dockerfile
buildenv/buildenv.mk
```

## Copyright
ouch is under the MIT license, by Vinícius R. Miguel, João Marcos P. Bezerra and contributors. The binary links in Rust crates, each under its own license. See `/usr/share/doc/ouch/copyright`, and `LICENSE` beside it.

ouch is built with its default features, so it holds the UnRAR code of Alexander Roshal for RAR archives. Its license forbids using that code to recreate the RAR compression algorithm, which makes the package non-free by the Debian Free Software Guidelines. Building without the feature `unrar` would drop RAR support.

## Files
| Path | Purpose |
| --- | --- |
| `/usr/bin/ouch` | The tool. |
| `/usr/share/man/man1/ouch*.1.gz` | The man pages, of `ouch` and each of its subcommands. |
| `/usr/share/bash-completion/completions/ouch` | The bash completions. |
| `/usr/share/zsh/site-functions/_ouch` | The zsh completions. |
| `/usr/share/fish/vendor_completions.d/ouch.fish` | The fish completions. |
| `/usr/share/doc/ouch/copyright`, `LICENSE`, `README.md` | The copyright holders, license and readme. |
