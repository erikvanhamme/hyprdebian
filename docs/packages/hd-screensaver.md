## Introduction
The package `hd-screensaver` is the screensaver of the HyprDebian desktop. It shows a text with random effects of terminal text effects (`tte`) in a full screen kitty window on every monitor. [hd-desktop](hd-desktop.md) recommends it, so it is installed with the desktop unless recommends are left out.

hypridle starts it after 2 minutes without input, and stops it before it locks the screen with hyprlock, by `~/.config/hypr/hypridle.conf` of `hd-desktop`. It also stops on a key press, or when another window gets the focus. Without the package, hypridle skips the screensaver and only locks the screen.

The package comes with the man page `man 7 hd-screensaver`.

## Building the package
The package is built from `hd-screensaver` in this repository:
```
cd hd-screensaver
make deb
```

This builds `build/hd-screensaver_<version>_all.deb`, with the version from `version.json`. Raise the version there for every package that is handed out. It depends on `hyprland`, `kitty`, `python3-terminaltexteffects`, `jq` and `procps`. Install it with:
```
sudo apt install ./build/hd-screensaver_<version>_all.deb
```

The files involved are:
```
hd-screensaver/Makefile
hd-screensaver/version.json
hd-screensaver/deb/control
hd-screensaver/hd-launch-screensaver
hd-screensaver/hd-cmd-screensaver
hd-screensaver/hd-kill-screensaver
hd-screensaver/screensaver.txt
hd-screensaver/man/hd-screensaver.7
```

The commands came with `hd-desktop` before 0.2; the package replaces those versions, so an upgrade moves them over.

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
