## Introduction
The package `hd-screensavers` holds optional screensavers of the HyprDebian desktop, drawn with Vulkan on every monitor, against the burn-in of OLED monitors. It depends on `hd-desktop`, whose [hd-screensaver](hd-screensaver.md) starts them, on the Vulkan loader `libvulkan1` and on `mesa-vulkan-drivers`, which makes sure there is a Vulkan device: RADV, ANV, or lavapipe, which renders on the CPU. HyprDebian installs no recommends, see [hd-base](hd-base.md), so nothing else pulls a driver in.

Each screensaver is a part of the package, with a page of its own:
- [hd-pipes](hd-pipes.md): the 3D pipes of Windows NT, after `pipes.c` of xscreensaver.

The man pages of the parts are listed on their pages.

Every screensaver covers every monitor with a scene of its own, on the overlay layer of wlr-layer-shell, and follows the monitors that come and go while it runs. It ends on a key press, a move of the mouse, a click, a scroll or a touch, and on SIGTERM, which `hd-kill-screensaver` sends. With `--window` it shows one ordinary window instead, which only Escape or closing it ends, to try it on a desktop.

The key `hd-screensaver` of the configuration names the screensaver that `hd-launch-screensaver` of `hd-desktop` starts, see [hd-screensaver](hd-screensaver.md#the-key). When the package is installed and the configuration of the system has no `hd-screensaver` yet, its postinst sets it to `"hd-pipes"`; a screensaver that the admin chose stays, also over upgrades. When the package is removed and the key still is `"hd-pipes"`, its postrm removes it, so the text screensaver of `hd-desktop` runs again.

The screensavers are for Hyprland, so the package is only in the repository of unstable.

## Building the package
The package is built from `packages/hd-screensavers` in this repository:
```
cd packages/hd-screensavers
make deb
```

This builds `build/hd-screensavers_<version>_<arch>.deb`, with the version from `version.json`. Raise the version there for every package that is handed out. Install it with:
```
sudo apt install ./build/hd-screensavers_<version>_<arch>.deb
```

The screensavers are C++23 programs. `make deb` compiles them in a container of `hd-build:unstable`, the build image of unstable from `buildenv/Dockerfile`, which it builds first, with `common/build.sh`. The image has the build tools, the -dev packages of Wayland and Vulkan and `glslang-tools`; the script runs `make all IN_BUILD=1` in the directory of each screensaver with the hardening flags of Debian. The GLSL shaders are compiled to SPIR-V and built into the binaries; no shader files are installed. The libraries the binaries link to, as `dpkg-shlibdeps` finds them in the container, go into the Depends. The compiled screensavers are kept in `build/programs` until a source changes or `make clean`. See [Build requirements](../HyprDebian/Build%20requirements.md).

The directories are:

| Path | |
| --- | --- |
| `common/src/` | What every screensaver shares: the outputs, their layer surfaces and the input (`screen.cpp`), the Vulkan device (`gpu.cpp`), a swapchain per surface (`swapchain.cpp`), mapped buffers (`buffer.cpp`), the matrices of OpenGL 1.x (`matrix.hpp`), and the options from the configuration and the command line (`options.cpp`, with the JSON reader `json.cpp`). |
| `common/protocols/` | A copy of `wlr-layer-shell-unstable-v1.xml` of wlr-protocols, which is in no Debian package. |
| `common/build.sh` | The build in the container. |
| `common/program.mk` | The rules that compile a screensaver, included by its `Makefile`. |
| `pipes/` | hd-pipes: its `Makefile` and its sources in `src/`. |
| `tests/` | The C++ tests of the parts without Wayland or Vulkan. |
| `man/` | The man pages. |
| `deb/` | The control file, the copyright file and the maintainer scripts `postinst` and `postrm`. |

A new screensaver adds a directory `<name>/` with a `Makefile` that sets `PROGRAM` and includes `../common/program.mk`, and its sources in `<name>/src/`; it is listed in `PROGRAMS` of `common/build.sh`, in the docker run and `PROGRAM_SOURCES` of the `Makefile`, and in `BINARIES`. The -dev packages it needs go in the layer of unstable of `buildenv/Dockerfile`.

To try a screensaver without a package, `make all` in its directory builds it there. It compiles in a container of `hd-build:unstable` too, as the user who runs it, so it needs docker and none of the -dev packages; the binary runs on the computer itself, a HyprDebian system on unstable, with `libvulkan1` and a Vulkan driver:
```
cd packages/hd-screensavers/pipes
make all
build/hd-pipes --window
```

## Tests
`make test` compiles each `tests/test_*.cpp` with the sources it tests into `build/tests/`, with `make test-binaries` in a container of `hd-build:unstable`, and runs them on the computer itself. The tests need no Wayland or Vulkan, only the libstdc++ of unstable. `make test` in the root of the repository runs them too.

| Test | |
| --- | --- |
| `test_walk.cpp` | The grid walk of hd-pipes: pipes never overlap, stay in the grid, end at `size` or when boxed in, follow the turn rules with and without `tightturns`, and the same seed gives the same pipes. |
| `test_mesh.cpp` | The meshes of hd-pipes: the tube, the elbow and the ball. |
| `test_options.cpp` | The options: the defaults, the configuration of the system, that of the user over it, the command line over both, and values out of range or of the wrong type. |

The drawing is checked by eye on the test vm.

## Credit
hd-pipes is based on `hacks/glx/pipes.c` of xscreensaver, from xlockmore, by Marcelo F. Vianna, 1997, under a permissive notice. The copyright file of the package, `/usr/share/doc/hd-screensavers/copyright`, holds that notice as it is in `pipes.c`, and every source file of hd-pipes, its shaders and its tests start with it. The files in `common/` do not come from `pipes.c` and carry no notice.

## Files
| Path | Purpose |
| --- | --- |
| `/usr/bin/hd-pipes` | The 3D pipes. |
| `/usr/share/doc/hd-screensavers/copyright` | The copyright, with the notice of `pipes.c`. |
| `/etc/hyprdebian/config.json` | The configuration of the system, with the key `hd-screensaver` that the package sets. |
