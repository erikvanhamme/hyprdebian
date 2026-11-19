## Introduction
`hd-pipes` draws the 3D pipes of Windows NT, a screensaver of the package [hd-screensavers](hd-screensavers.md). It is based on `hacks/glx/pipes.c` of xscreensaver, from xlockmore, by Marcelo F. Vianna, and has its grid walk, its light, materials and colours, and its options, with their names and defaults. It is a C++ program that draws with Vulkan on a wlr-layer-shell surface on every monitor, so it needs Hyprland or another compositor with wlr-layer-shell.

When `hd-screensavers` is installed, the key `hd-screensaver` of the configuration of the system names it, and hypridle starts it after 2 minutes without input, with `hd-launch-screensaver` of `hd-desktop`; see [hd-screensaver](hd-screensaver.md).

`hd-screensavers` comes with the man page `man 6 hd-pipes`.

## Building the package
hd-pipes is part of the package `hd-screensavers`, built from `packages/hd-screensavers`; see [hd-screensavers](hd-screensavers.md#building-the-package). Its sources are in `packages/hd-screensavers/pipes/src`, the code it shares with the other screensavers in `packages/hd-screensavers/common`.

To try a change without a package, `make all` in `pipes/` builds `build/hd-pipes` there, on the computer itself, which then needs the -dev packages:
```
sudo apt install build-essential pkgconf libwayland-dev wayland-protocols libvulkan-dev glslang-tools
cd packages/hd-screensavers/pipes
make all
build/hd-pipes --window --joints ball
```

The files involved are:
```
packages/hd-screensavers/pipes/Makefile
packages/hd-screensavers/pipes/src/main.cpp
packages/hd-screensavers/pipes/src/settings.cpp
packages/hd-screensavers/pipes/src/walk.cpp
packages/hd-screensavers/pipes/src/mesh.cpp
packages/hd-screensavers/pipes/src/pipes.cpp
packages/hd-screensavers/pipes/src/pipe.vert
packages/hd-screensavers/pipes/src/pipe.frag
packages/hd-screensavers/common/
packages/hd-screensavers/tests/test_walk.cpp
packages/hd-screensavers/tests/test_mesh.cpp
packages/hd-screensavers/tests/test_options.cpp
packages/hd-screensavers/man/hd-pipes.6
packages/hd-screensavers/deb/copyright
```

| File | |
| --- | --- |
| `main.cpp` | Reads the options and runs the screen. |
| `settings.cpp` | The options: their names, ranges and defaults. |
| `walk.cpp` | The grid walk: where each pipe goes. No Wayland or Vulkan, tested by `test_walk.cpp`. |
| `mesh.cpp` | The shapes: the tube, the elbow and the ball. Tested by `test_mesh.cpp`. |
| `pipes.cpp` | A scene per monitor: the steps on the clock, the restart and the drawing. |
| `pipe.vert`, `pipe.frag` | The shaders, with the lighting of OpenGL 1.x per pixel. |

Each of them, and the tests, starts with the notice of `pipes.c`.

## The pipes
The pipes run through a grid of 33×25×33 cells. A screenful is `cycles` pipes on the same grid, each of one of the 7 colours of `pipes.c`. A pipe starts with a ball in a free cell and goes on from cell to cell, one step per `delay`. Where it turns, by 90°, it gets a joint: an elbow or a ball. It ends, capped with a ball, when it is boxed in or reaches `size`. Without `tightturns` the chance of a turn grows with every step since the last turn; with it, every step has a chance of 20%.

When the last pipe of a screenful is done, the scene shrinks and tilts by 90° and starts again with the next screenful on an empty grid. With `rotatepipes` the scene turns slowly about its vertical axis, from a random angle.

Every monitor has a scene of its own, with a seed of its own. Monitors that come while it runs get a scene; those that go lose theirs.

## The look
At first glance it looks like 1997; a close look shows smooth shapes, good shading and no jaggies:
- the light, the materials, the shininess and the colours of `pipes.c`, lit per pixel as OpenGL 1.x did, in gamma space, on an 8 bit UNORM image, not sRGB;
- tubes of 64 segments, elbows of 64×24 and balls of 64×32, made once at the start and drawn instanced;
- multisampling with the most samples the device has, up to 8, every sample shaded when the device can, against the shimmer of the highlights;
- a frame at every refresh of each monitor (FIFO), with the turn and the shrink in between two steps, so the pipes grow at the same speed at 60 Hz and at 240 Hz.

The device is the first discrete GPU that can show on the compositor, else an integrated one, else a virtual one, as in a vm, else lavapipe of `mesa-vulkan-drivers`, which renders on the CPU. All monitors share it. It needs Vulkan 1.3.

## Input
On the monitors, hd-pipes has the keyboard to itself and ends on a key press, a move of the mouse, a click, a scroll or a touch. The pointer that is on a monitor when it starts does not end it, as the surface appears under a mouse that stands still; the cursor is hidden on its surfaces. It also ends on SIGTERM, which `hd-kill-screensaver` sends, and on SIGINT.

With `--window` it is one ordinary window instead, with the app id `hd-pipes`, which input does not end; Escape or closing the window does. That is for trying the options on a desktop and for screenshots on the test vm.

## Options
| Option | Default | |
| --- | --- | --- |
| `--joints elbow\|ball\|cycle` | `cycle` | The joints where the pipes turn; `cycle` takes balls and elbows by turns, one kind per screenful, balls first. |
| `--cycles N` | 5 | The pipes per screenful, 1–10. |
| `--size N` | 500 | The longest run of a pipe, 10–1000. |
| `--delay µs` | 10000 | The time of one step, in microseconds, 1–1000000. |
| `--fisheye`, `--no-fisheye` | on | The camera closer to the pipes. |
| `--tightturns`, `--no-tightturns` | off | The same chance of a turn at every step. |
| `--rotatepipes`, `--no-rotatepipes` | on | The scene turns slowly. |
| `--window` | | One window instead of every monitor; see [Input](#input). Only on the command line. |
| `--help` | | Prints the options, with their ranges and defaults. |

Each option is `--name value` or `--name=value`; when an option is given more than once, the last wins.

## The configuration
The key `hd-pipes` of the configuration sets the options too, as an object with their names, the flags `true` or `false`:
```
{"hd-pipes": {"joints": "ball", "cycles": 3, "fisheye": false}}
```

It is read in the configuration of the system, `/etc/hyprdebian/config.json`, and then in that of the user, `~/.config/hyprdebian/config.json`, member by member: an option that the user leaves out keeps the value of the system. Run as root, it only reads the configuration of the system. The command line wins over both. See [hd-config](hd-config.md). For a user:
```
hd-config-update hd-pipes '{"joints": "ball"}'
```

In a configuration, a value out of its range or of the wrong type, a member that is not an option, a key that is not an object and a file that is not valid JSON are reported on stderr and skipped, so the value below stays. On the command line a wrong value is an error.

## Exit status
| Status | |
| --- | --- |
| 0 | It ended: on input, on SIGTERM or SIGINT, or after `--help`. |
| 1 | It cannot start, with the reason on stderr: no Vulkan device, no wlr-layer-shell, no monitors; or it lost the compositor. |
| 2 | A wrong argument. |

`hd-launch-screensaver` starts the text screensaver in its place when it ends with an error within 2 seconds.

## Files
| Path | Purpose |
| --- | --- |
| `/usr/bin/hd-pipes` | The screensaver. |
| `~/.config/hyprdebian/config.json` | The configuration of the user, with the key `hd-pipes`. |
| `/etc/hyprdebian/config.json` | The configuration of the system, with the keys `hd-pipes` and `hd-screensaver`. |
| `/usr/share/doc/hd-screensavers/copyright` | The notice of `pipes.c`. |
