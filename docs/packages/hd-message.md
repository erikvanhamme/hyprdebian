## Introduction
`hd-message` is the message box of the HyprDebian desktop, in the package `hd-desktop`. It reads a message as JSON from stdin, a title, a text and a row of buttons, shows it in the middle of the screen in the hd-theme of the user, and prints the value of the button chosen on stdout. Like [hd-menu](hd-menu.md), with which it shares most of its code, it is a C++ program that draws the message itself, with cairo and librsvg, on a wlr-layer-shell surface, so it needs Hyprland or another compositor with wlr-layer-shell. The namespace of the surface is `hd-message`, for layer rules of Hyprland.

Only one `hd-message` is open at a time: a lock in `/run/user/<uid>/hd-message.lock` makes a second one exit at once with 1.

`hd-desktop` comes with the man page `man 1 hd-message`.

## Building the package
hd-message is part of the package `hd-desktop`, built from `packages/hd-desktop`; see [hd-desktop](hd-desktop.md). Its source is in `packages/hd-desktop/message`, the code it shares with hd-menu in `packages/hd-desktop/common`. `make deb` of hd-desktop compiles both programs in one `debian:unstable` container, with `make all` of `menu/Makefile` and `message/Makefile`, and puts the libraries the binaries link to, as `dpkg-shlibdeps` finds them, in the Depends of `hd-desktop`.

To try a change without a package, `make all` in `message/` builds `build/hd-message` there, on the computer itself, which then needs the -dev packages:
```
sudo apt install build-essential pkgconf libwayland-dev wayland-protocols libxkbcommon-dev libcairo2-dev librsvg2-dev
cd packages/hd-desktop/message
make all
echo '{"title": "Reboot system?", "text": "Unsaved work is lost.", "buttons": [{"label": "No"}, {"label": "Yes"}]}' | build/hd-message
```

The files involved are:
```
packages/hd-desktop/message/Makefile
packages/hd-desktop/message/src/
packages/hd-desktop/common/build.sh
packages/hd-desktop/common/program.mk
packages/hd-desktop/common/protocols/wlr-layer-shell-unstable-v1.xml
packages/hd-desktop/common/src/
packages/hd-desktop/man/hd-message.1
packages/hd-desktop/deb/copyright
```

## The message
```
{
    "title": "Reboot system?",
    "text": "The new kernel is used after a reboot.\nUnsaved work is lost.",
    "width": 600,
    "buttons": [
        {"label": "Later", "value": "later"},
        {"label": "Reboot", "value": "reboot", "icon": "/usr/share/icons/reboot.svg"}
    ],
    "default": "later"
}
```

| Member | |
| --- | --- |
| `title` | Optional: a line of text at the top, in the big font, with a divider under it. |
| `text` | Optional: the message, in the normal font. It is wrapped at the spaces to the width of the box; a word that is wider than the box on its own is broken between characters. A newline (`\n`) starts a new line. |
| `width` | Optional: the width of the box, in logical pixels (a number) or as a percentage of the output (a string such as `"50%"`). Without it, the box is as wide as the title, the text and the buttons need, but the text wraps at half the width of the output. The box is as high as its content needs, and never larger than the output; text that does not fit is cut off. |
| `buttons` | The buttons, from left to right; at least one. All are as wide as the widest. |
| `label` | The text of a button. Glyphs of the Nerd Font can be written as escapes, such as `` (a check mark) and `` (a cross), also in the title and the text. |
| `value` | Optional: what `hd-message` prints when the button is chosen; the label when there is none. |
| `icon` | Optional: the path of a PNG or SVG (`.svg` or `.svgz`) image, shown left of the label at the `icon_size` of the theme. |
| `default` | Optional: the value of the button selected at the start; the first button when there is none. A value that no button has is an error. |

Left, Right, Tab, Shift+Tab, Home and End move the selection, Enter chooses and Escape or `q` closes the message box without a choice. `q` is the key with the letter q in the keyboard layout, with or without Shift. A key that is held repeats at the rate and after the delay of the compositor.

## The theme
`hd-message` takes its look from the `hd-theme` key of the configuration of the user, `~/.config/hyprdebian/config.json`, and when that has none, of the configuration of the system, `/etc/hyprdebian/config.json`, of [hd-config](hd-config.md); [hd-theme](hd-theme.md) sets them when a theme is selected. When neither has the key, it uses defaults of its own. Run as root, it only reads the configuration of the system:

| Setting | Used for |
| --- | --- |
| `font` | All text. |
| `font-size-big` | The title. |
| `font-size-normal` | The text and the labels of the buttons. |
| `dark`, `opacity` | The background of the box; the opacity applies to the background only. |
| `light` | The text. |
| `normal` | The selected button, the outline of the other buttons and the divider under the title. |
| `highlight` | The border of the box. |
| `border`, `radius` | The width and the corner radius of the border. The buttons have the radius minus the border. |
| `icon_size` | The size of the icons. |

The font sizes are in points, like in the other programs of the theme; `hd-message` draws them at 4/3 logical pixels per point. The border, the radius and the icon size are in logical pixels.

A setting that is missing has its default; when the file cannot be read, the whole theme has.

## Exit status
| Status | |
| --- | --- |
| 0 | A button was chosen; its value is on stdout. |
| 1 | The message box was closed without a choice, or another `hd-message` is open. |
| 2 | An error: arguments, invalid JSON, a message without buttons, a `default` that is not the value of a button, or no Wayland compositor with wlr-layer-shell. |

## Files
| Path | Purpose |
| --- | --- |
| `/usr/bin/hd-message` | The message box. |
| `~/.config/hyprdebian/config.json` | The theme of the user, in `hd-theme`. |
| `/etc/hyprdebian/config.json` | The theme of the system, in `hd-theme`, when the user has none. |
| `/run/user/<uid>/hd-message.lock` | The lock that keeps a second `hd-message` from opening. |
