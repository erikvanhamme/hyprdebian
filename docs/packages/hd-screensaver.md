## Introduction
The package `hd-desktop` holds the screensaver of the HyprDebian desktop. It shows a text with random effects of terminal text effects (`tte`) in a full screen kitty window on every monitor. See [hd-desktop](hd-desktop.md).

The key `hd-screensaver` of the configuration can name another screensaver in its place, such as `hd-pipes` or `hd-logo` of the package [hd-screensavers](hd-screensavers.md); see [The key](#the-key).

hypridle starts it after 2 minutes without input, and stops it before it locks the screen with hyprlock, by `~/.config/hypr/hypridle.conf` of `hd-desktop`. It also stops on a key press, or when another window gets the focus.

`hd-desktop` comes with the man page `man 7 hd-screensaver`.

## Building the package
The screensaver is part of the package `hd-desktop`, built from `packages/hd-desktop`; see [hd-desktop](hd-desktop.md). It needs `hyprland`, `kitty`, `python3-terminaltexteffects`, `jq` and `procps`. The files involved are:
```
packages/hd-desktop/hd-launch-screensaver
packages/hd-desktop/hd-cmd-screensaver
packages/hd-desktop/hd-kill-screensaver
packages/hd-desktop/screensaver.txt
packages/hd-desktop/man/hd-screensaver.7
```

## Commands
| Command | |
| --- | --- |
| `hd-launch-screensaver` | Starts the screensaver that the key names, unless it already runs. Without the key, when that program is not installed, or when it fails at its start, it starts the text screensaver on every monitor, unless it already runs. It runs `hyprctl`, so it must run in the Hyprland session. |
| `hd-cmd-screensaver` | Shows the text in a loop of random effects. `hd-launch-screensaver` runs it in each kitty window of the class `hyprdebian.screensaver`. |
| `hd-kill-screensaver` | Stops the screensaver that the key names, with SIGTERM, and the text screensaver on every monitor. |

## The key
The key `hd-screensaver` is a string, the name of a screensaver: a program in the `PATH` of letters, digits, `-` and `_`, which is started without arguments, covers every monitor, ends on input and ends on SIGTERM. It is read in the configuration of the user, `~/.config/hyprdebian/config.json`, first and, when that does not have it, in the configuration of the system, `/etc/hyprdebian/config.json`; see [hd-config](hd-config.md).

`hd-launch-screensaver` waits 2 seconds after it starts the screensaver. When the program ends with an error in that time, the screensaver failed at its start, and it starts the text screensaver in its place.

The package `hd-screensavers` sets the key of the system to `"hd-pipes"` when it is installed and the system has no key yet, and removes it when it is removed and the key still is `"hd-pipes"`. A user who wants the text screensaver while the system has another sets a name that is not installed, and follows the screensaver of the system again by removing the key:
```
hd-config-update hd-screensaver '"none"'
hd-config-remove hd-screensaver
```

## The text
The screensaver shows `~/.config/hyprdebian/screensaver.txt` of the user, or `/usr/share/hd-screensaver/screensaver.txt` when the user has none. To change it:
```
mkdir -p ~/.config/hyprdebian
cp /usr/share/hd-screensaver/screensaver.txt ~/.config/hyprdebian/
nano ~/.config/hyprdebian/screensaver.txt
```

## Files
| Path | Purpose |
| --- | --- |
| `/usr/bin/hd-launch-screensaver` | Starts the screensaver. |
| `/usr/bin/hd-cmd-screensaver` | Runs the screensaver in its kitty window. |
| `/usr/bin/hd-kill-screensaver` | Stops the screensaver. |
| `/usr/share/hd-screensaver/screensaver.txt` | The text, when the user has none. |
| `~/.config/hyprdebian/screensaver.txt` | The text of the user. |
| `~/.config/hyprdebian/config.json` | The configuration of the user, with the key `hd-screensaver`. |
| `/etc/hyprdebian/config.json` | The configuration of the system, with the key `hd-screensaver`. |
