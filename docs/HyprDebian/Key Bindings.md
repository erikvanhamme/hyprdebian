## Introduction
The key bindings are defined in the Hyprland config:
```
hd-desktop/etc/skel/.config/hypr/hyprland.lua
```

`SUPER` is the Windows key.

## Applications
| Keys | Action |
| --- | --- |
| `SUPER + RETURN` | Open a terminal (kitty) |
| `SUPER + SPACE` | Open the application launcher ([hd-menu](../packages/hd-menu.md)) |
| `SUPER + B` | Open the browser (firefox) |
| `SUPER + C` | Open VSCodium |
| `SUPER + A` | Open the audio mixer (wiremix) in a terminal |
| `SUPER + Y` | Open the file manager (yazi) in a terminal |

## System
| Keys | Action |
| --- | --- |
| `SUPER + L` | Lock the screen (hyprlock) |
| `SUPER + SHIFT + R` | Ask to reboot |
| `SUPER + SHIFT + H` | Ask to power off |
| `SUPER + SHIFT + T` | Show the date and time in a notification |
| `SUPER + SHIFT + M` | Select the display mode of a monitor |
| `SUPER + M` | Mount a drive |
| `SUPER + U` | Unmount a drive |

## Windows
| Keys | Action |
| --- | --- |
| `SUPER + W` | Close the active window |
| `SUPER + F` | Toggle fullscreen |
| `SUPER + T` | Toggle floating |
| `SUPER + arrow key` | Move the focus to the window in that direction |
| `SUPER + SHIFT + arrow key` | Swap the active window with the window in that direction |
| `SUPER + left mouse button` | Drag to move the window |
| `SUPER + right mouse button` | Drag to resize the window |

## Workspaces
| Keys | Action |
| --- | --- |
| `SUPER + 1` … `SUPER + 9` | Go to workspace 1 to 9 |
| `SUPER + 0` | Go to workspace 10 |
| `SUPER + SHIFT + 1` … `SUPER + SHIFT + 9` | Move the active window to workspace 1 to 9 |
| `SUPER + SHIFT + 0` | Move the active window to workspace 10 |
| `SUPER + scroll down` | Go to the next existing workspace |
| `SUPER + scroll up` | Go to the previous existing workspace |

## Screenshots
| Keys | Action |
| --- | --- |
| `PRINT` | Take a screenshot of the whole screen and open it in swappy |
| `SUPER + S` | Take a screenshot of a selected area and open it in swappy |
| `SUPER + SHIFT + S` | Take a screenshot of a selected area and save it in `~/Pictures/Screenshots` |

## Media keys
These keys also work while the screen is locked.

| Keys | Action |
| --- | --- |
| Volume up | Raise the volume by 5% (repeats while held) |
| Volume down | Lower the volume by 5% (repeats while held) |
| Mute | Toggle mute |
| Brightness up | Raise the screen brightness by 5% (repeats while held) |
| Brightness down | Lower the screen brightness by 5% (repeats while held) |
| Play / Pause | Play or pause the media player |
| Next | Next track |
| Previous | Previous track |
