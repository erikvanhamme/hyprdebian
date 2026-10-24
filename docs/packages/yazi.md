## Introduction
The package `yazi` holds [Yazi](https://yazi-rs.github.io/), the terminal file manager, and its command line tool `ya`, built from release v26.9.1. Debian has no package of it, so HyprDebian builds one from the upstream release, unchanged.

Besides the two binaries, the package installs the desktop entry and icon of the release, and the completions of `yazi` and `ya` for bash, zsh and fish. It also holds `hd-yazi`, which starts Yazi where it was left the last time, and the configuration of Yazi that new users get (see below). It recommends the programs Yazi uses for previews and search: `ffmpeg`, `7zip`, `jq`, `poppler-utils`, `fd-find`, `ripgrep`, `fzf` and `zoxide`.

## Starting where Yazi was left
`hd-yazi` starts `yazi` in the directory it was in when it quit the last time. Yazi writes that directory to `$XDG_STATE_HOME/yazi/last-cwd` (by default `~/.local/state/yazi/last-cwd`) when it quits, through its option `--cwd-file`. Without that file, or when the directory in it no longer exists, Yazi starts in the home directory. Every user has a file of their own.

Bind it to a key to open Yazi in a terminal, for example:
```
kitty -e hd-yazi
```

`hd-yazi` comes with a man page: `man hd-yazi`.

## Configuration
New users get `~/.config/yazi/yazi.toml` from `/etc/skel/.config/yazi/yazi.toml`, a conffile, so local changes are kept on upgrades. Yazi merges it with its defaults. It:

- opens videos with `mpv`, audio with `audacious` of [hd-audacious](hd-audacious.md) (open or enqueue) and images with `imv-wayland`; audacious takes the files in the instance that runs, or starts in a kitty window;
- shows no previews and preloads nothing under `/stor`, the network storage, so browsing it stays fast;
- shows hidden files.

Users that exist already do not get the file; copy it to `~/.config/yazi/yazi.toml` for them.

## Building the package
The package is built from `packages/yazi` in this repository:
```
cd packages/yazi
make deb
```

This builds `build/yazi_<release>-<revision>_<arch>.deb`. Install it with:
```
sudo apt install ./build/yazi_26.9.1-1_amd64.deb
```

The binaries are compiled in a Docker container, so the build needs `docker` and a user that may run it, besides `dpkg-deb`. `build-yazi.sh` runs in a `debian:trixie` container: it installs the build tools and the latest stable Rust toolchain with rustup, as the `rustc` in Debian is too old for Yazi, clones the release tag and runs `cargo build --release --locked`. The results land in `build/dist-<release>`; later builds reuse them, until `make clean` removes them.

The container is trixie because the binaries need the libc of the release they are compiled on: built on trixie, they also run on unstable.

To move to a new release of Yazi, change `YAZI_VERSION` in the Makefile, set `DEB_REVISION` back to 1, and update the release on this page. For a new package of the same release, raise `DEB_REVISION`.

The files involved are:
```
packages/yazi/Makefile
packages/yazi/build-yazi.sh
packages/yazi/hd-yazi
packages/yazi/man/hd-yazi.1
packages/yazi/etc/skel/.config/yazi/yazi.toml
packages/yazi/deb/control
packages/yazi/deb/copyright
```

## Copyright
Yazi is under the MIT license, by sxyazi; its file icons come from nvim-web-devicons, under the MIT license by nvim-tree. The binaries link in Rust crates, each under its own license. See `/usr/share/doc/yazi/copyright`, and `LICENSE` and `LICENSE-ICONS` beside it.

## Files
| Path | Purpose |
| --- | --- |
| `/usr/bin/yazi` | The file manager. |
| `/usr/bin/ya` | The command line tool of Yazi, for packages and messages. |
| `/usr/bin/hd-yazi` | Starts Yazi where it was left the last time. |
| `/usr/share/man/man1/hd-yazi.1.gz` | The man page of `hd-yazi`. |
| `~/.local/state/yazi/last-cwd` | The last directory of Yazi, per user. |
| `/etc/skel/.config/yazi/yazi.toml` | The configuration of Yazi new users get, a conffile. |
| `/usr/share/applications/yazi.desktop`, `/usr/share/pixmaps/yazi.png` | The desktop entry and its icon. |
| `/usr/share/bash-completion/completions/yazi`, `ya` | The bash completions. |
| `/usr/share/zsh/site-functions/_yazi`, `_ya` | The zsh completions. |
| `/usr/share/fish/vendor_completions.d/yazi.fish`, `ya.fish` | The fish completions. |
| `/usr/share/doc/yazi/copyright`, `LICENSE`, `LICENSE-ICONS` | The copyright holders and licenses. |
