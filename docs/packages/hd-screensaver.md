## Introduction
The package `hd-desktop` holds the screensaver of the HyprDebian desktop. It shows a text with random effects of terminal text effects (`tte`) in a full screen kitty window on every monitor. See [hd-desktop](hd-desktop.md).

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
| `hd-launch-screensaver` | Starts the screensaver on every monitor, unless it already runs. It runs `hyprctl`, so it must run in the Hyprland session. |
| `hd-cmd-screensaver` | Shows the text in a loop of random effects. `hd-launch-screensaver` runs it in each kitty window of the class `hyprdebian.screensaver`. |
| `hd-kill-screensaver` | Stops the screensaver on every monitor. |

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
