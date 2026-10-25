## Introduction
The package `hd-desktop` holds the font of hyprdebian: JetBrains Mono, the typeface by JetBrains, with the icons of Nerd Fonts patched in, from the Nerd Fonts release v3.4.0. They are only the fonts; they have no commands and no settings. They have no emoji; fontconfig takes those from Noto Color Emoji, of `fonts-noto-color-emoji`, which `hd-desktop` depends on.

It holds two families, each in 16 styles:

| Family | Icons |
| --- | --- |
| `JetBrainsMono Nerd Font` | About one and a half letters wide. They are easier to see, but can overlap the next letter. |
| `JetBrainsMono Nerd Font Mono` | One letter wide, so every character takes the same width. Use it where the text has to line up, as in terminals. |

The styles are Thin, ExtraLight, Light, Regular, Medium, SemiBold, Bold and ExtraBold, each also in Italic. Both families keep the ligatures of JetBrains Mono.

The fonts are in `/usr/share/fonts/truetype/jetbrains-mono-nerd`. fontconfig finds them there, and refreshes its cache when `hd-desktop` is installed, upgraded or removed.

`hd-desktop` comes with a man page for them: `man hd-fonts`.

## Building the package
The fonts are part of the package `hd-desktop`, built from `packages/hd-desktop`; see [hd-desktop](hd-desktop.md).

The fonts are not in this repository. The first build downloads `JetBrainsMono.zip` of the Nerd Fonts release to `build/` and checks it against the SHA256 in the Makefile; a download that does not match stops the build. Later builds reuse the zip, until `make clean` removes it. The build needs `wget`, `unzip` and `sha256sum`.

The package takes only the families `Nerd Font` and `Nerd Font Mono` from the zip. The zip also has `Nerd Font Propo`, for proportional text, and the `NL` versions of all three, without ligatures; they would make the package three times as large.

To move to a new Nerd Fonts release, change `NERD_FONTS_VERSION` and `ZIP_SHA256` in `packages/hd-desktop/Makefile` together, after checking the new zip and its license. Update the release in `deb/copyright`, in the description in `deb/control`, the man page and this page.

The files involved are:
```
packages/hd-desktop/Makefile
packages/hd-desktop/deb/copyright
packages/hd-desktop/man/hd-fonts.7
```

## Using the fonts
Name the font in the configuration of a program by a family name, with the size where it takes one. The families also have the short names `JetBrainsMono NF` and `JetBrainsMono NFM`, and every style but Regular and Bold has a family name of its own, such as `JetBrainsMono NFM Medium`. Such a name works in every program: in those that look up a family name, and in those that read a style after the family, as GTK and Pango do. HyprDebian uses `JetBrainsMono NFM Medium` everywhere. For example, in `~/.config/kitty/kitty.conf`:
```
font_family JetBrainsMono NFM Medium
```

or, in a GTK font name:
```
JetBrainsMono NFM Medium 11
```

To list the installed styles:
```
fc-list : family style | grep -i 'jetbrainsmono nerd'
```

A program that draws text from a font file instead of through fontconfig takes a file from `/usr/share/fonts/truetype/jetbrains-mono-nerd`, such as `JetBrainsMonoNerdFontMono-Regular.ttf`.

## Credits
The fonts are the work of others; this package only installs them, unchanged.

- **JetBrains Mono**: the typeface, made by The JetBrains Mono Project Authors at JetBrains. <https://github.com/JetBrains/JetBrainsMono>
- **Nerd Fonts**: the icons and the patched fonts, by Ryan L McIntyre and the Nerd Fonts contributors. The icons themselves come from projects such as Font Awesome, Material Design Icons, Codicons, Devicons, Octicons and Powerline. <https://www.nerdfonts.com/>

## Copyright
The fonts are under the SIL Open Font License 1.1, as is JetBrains Mono, which they are derived from. The icons are under the licenses of their projects. See `/usr/share/doc/hd-desktop/copyright` for the copyright holders and every license, and `/usr/share/doc/hd-desktop/nerd-fonts/OFL.txt` for the license file of the release.

`deb/copyright` is in the Debian machine-readable copyright format. It names both projects as upstream, lists the icon sets with their licenses from the [license audit](https://github.com/ryanoasis/nerd-fonts/blob/v3.4.0/license-audit.md) of Nerd Fonts, and holds the full text of the OFL, which Debian does not ship in `/usr/share/common-licenses`.

## Files
| Path | Purpose |
| --- | --- |
| `/usr/share/fonts/truetype/jetbrains-mono-nerd/JetBrainsMonoNerdFont-*.ttf` | The family JetBrainsMono Nerd Font. |
| `/usr/share/fonts/truetype/jetbrains-mono-nerd/JetBrainsMonoNerdFontMono-*.ttf` | The family JetBrainsMono Nerd Font Mono. |
| `/usr/share/doc/hd-desktop/copyright` | The copyright holders and licenses. |
| `/usr/share/doc/hd-desktop/nerd-fonts/OFL.txt`, `/usr/share/doc/hd-desktop/nerd-fonts/README.md` | The license and the readme of the Nerd Fonts release. |
