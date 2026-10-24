## Introduction
The package `hd-audacious` holds the audio player [Audacious](https://audacious-media-player.org/) 4.6.2 with a terminal interface, built from the HyprDebian forks [audacious](https://github.com/erikvanhamme/audacious) and [audacious-plugins](https://github.com/erikvanhamme/audacious-plugins). It is built for a small footprint: it plays FLAC, MP3 and WAV over PipeWire, with ReplayGain, and pulls in no GTK, Qt, X11, Wayland or other graphical library. It replaces the packages `audacious`, `audacious-plugins` and `audacious-plugins-data` of Debian: it conflicts with them, replaces them and provides `audacious`, so installing it removes them. [hd-desktop](hd-desktop.md) depends on it.

The forks add two commits to the release 4.6.2:
- audacious: the option `--tui`, which runs audacious with an interface plugin for the terminal, and sends its output to a log file while that interface runs;
- audacious-plugins: the plugin `tui`, the terminal interface, drawn with the core of notcurses, without its multimedia part, which needs FFmpeg.

The interface shows the controls with the progress of the song, the playlist and the title, artist and album of the song that plays. The package comes with the man pages `man 7 hd-audacious`, `man audacious` and `man audtool`.

## Starting audacious
`/usr/bin/audacious` starts the binary in `/usr/lib/hd-audacious/bin` with `--tui`, and hands on all its arguments unchanged:
- From a terminal, audacious runs in it.
- Without a terminal, as from the application launcher or an opener of yazi, it hands its command line to the audacious that runs, as audacious does itself over D-Bus. When none runs, it starts in a new kitty window of the class `audacious`. The package only recommends kitty, so that it pulls in no graphical libraries; [hd-desktop](hd-desktop.md) depends on it. Without kitty, audacious only starts from a terminal.
- With `--headless`, `--help` or `--version`, it runs without the terminal interface.

```
audacious ~/Music/album           play the album
audacious --enqueue song.flac     add a song to the playlist
```

While the terminal interface runs, the output of audacious goes to `/tmp/audacious-tui-<user>.log`.

## Keys
| Key | |
| --- | --- |
| `Up`, `Down`, `Page Up`, `Page Down` | Move through the playlist. |
| `Enter` | Play the song in the playlist. |
| `Space` | Pause or play. |
| `Left`, `Right` | Seek 5 seconds back or forward. |
| `q` | Quit. |

The plugin mpris2 is enabled from the start, so the media keys of the desktop control audacious with `playerctl`. `audtool` controls it from a shell.

## Plugins
The package has only these plugins; all others are left out, also the ones without libraries of their own:

| Kind | Plugins |
| --- | --- |
| Interface | the terminal interface |
| Inputs | FLAC, MP3 (mpg123), WAV (sndfile) |
| Output | PipeWire |
| General | mpris2, for the media keys |
| Playlists | m3u, pls, and audpl, the format of audacious itself |

Files are read by audacious itself, without the gio transport; there are no internet streams.

## ReplayGain
ReplayGain is part of audacious itself, not a plugin, and is on by default, in track mode. It reads the tags of FLAC files and the ID3v2 and APE tags of MP3 files. The terminal interface has no settings window: the settings are in the section `[audacious]` of `~/.config/audacious/config`, which audacious writes when it quits, so change it while audacious does not run.

| Setting | |
| --- | --- |
| `enable_replay_gain` | `TRUE` or `FALSE`; `TRUE` by default. |
| `replay_gain_mode` | `0` for track gain, the default, `1` for album gain, `2` for album gain, but track gain while the playlist is shuffled. |
| `replay_gain_preamp` | The gain to add, in dB; `0` by default. |

## Dependencies
On an unstable system without anything else, the package pulls in about 60 packages: the libraries of FLAC, mpg123, sndfile, notcurses-core and PipeWire with theirs, python3 and systemd. None of them is a graphical library. kitty, which it recommends, brings the graphical libraries of the desktop; `apt install --no-install-recommends hd-audacious` leaves it out.

## Building the package
The package is built from `packages/hd-audacious` in this repository:
```
cd packages/hd-audacious
make deb
```

This builds `build/hd-audacious_<version>_<arch>.deb`, with the version from `version.json` of the package. Install it with:
```
sudo apt install ./build/hd-audacious_<version>_<arch>.deb
```

The player is compiled in a Docker container, so the build needs `docker` and a user that may run it, besides `dpkg-deb`. `build-hd-audacious.sh` runs in a `debian:unstable` container: it installs the build tools and the -dev packages of the plugins, clones the commits of the forks named in the Makefile, and builds both with meson. It turns off every boolean option of audacious-plugins but those of the plugins above, so no other plugin gets in, also not one that a later release adds. It fails when the plugins differ from those above, as meson leaves out a plugin whose library it does not find. `dpkg-shlibdeps` then finds the packages of the libraries the binaries and plugins link to, which become the Depends of the package. The results land in `build/dist-<commits>`; later builds reuse them, until `make clean` removes them.

The container is unstable because the package is for unstable only, as hd-desktop.

The libraries `libaudcore` and `libaudtag` are private, in `/usr/lib/hd-audacious`, which the binaries and plugins have as RUNPATH, so they never clash with the libraries of the audacious of Debian.

To try changes to the forks before they are committed and pushed, build from local checkouts, in `<dir>/audacious` and `<dir>/audacious-plugins`:
```
make deb LOCAL_SRC=~/Projects/audacious
```

This compiles into `build/dist-local`, again on every build, and the package says it is built from local checkouts. Hand out only packages of commits.

To move to new commits of the forks, change `AUDACIOUS_COMMIT` and `PLUGINS_COMMIT` in the Makefile, and `UPSTREAM_VERSION` for a new release, and raise the version in `version.json`.

The files involved are:
```
packages/hd-audacious/Makefile
packages/hd-audacious/version.json
packages/hd-audacious/build-hd-audacious.sh
packages/hd-audacious/audacious
packages/hd-audacious/deb/control
packages/hd-audacious/deb/copyright
packages/hd-audacious/man/hd-audacious.7
```

## Copyright
Audacious and its plugin framework are by the Audacious developers, under the BSD 2-Clause license. The plugins are each under their own license: BSD 2-Clause, GPL 2 or later, GPL 3 or later, or ISC. See `/usr/share/doc/hd-audacious/copyright`, and `COPYING-audacious` and `COPYING-audacious-plugins` beside it.

## Files
| Path | Purpose |
| --- | --- |
| `/usr/bin/audacious` | Starts audacious with its terminal interface. |
| `/usr/bin/audtool` | The command line remote control. |
| `/usr/lib/hd-audacious/bin` | The binaries `audacious` and `audtool`. |
| `/usr/lib/hd-audacious/libaudcore.so.6`, `libaudtag.so.4` | The private libraries. |
| `/usr/lib/hd-audacious/audacious` | The plugins. |
| `/usr/share/applications/audacious.desktop` | The desktop entry. |
| `/usr/share/doc/hd-audacious/copyright`, `COPYING-audacious`, `COPYING-audacious-plugins` | The copyright holders and licenses. |
| `~/.config/audacious` | The settings and playlists of a user. |
| `/tmp/audacious-tui-<user>.log` | The output of audacious while the terminal interface runs. |
