## Introduction
The package `orcaslicer` holds [OrcaSlicer](https://www.orcaslicer.com/), the slicer for 3D printers, from release v2.4.2. Debian has no package of it, so HyprDebian packages the AppImage of the release, unpacked and unchanged, in `/usr/lib/orcaslicer`. Unpacked, it runs without FUSE.

The AppImage brings its own zlib, liblzma, libexpat and libmspack, in `/usr/lib/orcaslicer/lib/orca-runtime`, and uses the libraries of the system for everything else, such as GTK, WebKitGTK and OpenGL. The package depends on those.

## Software rendering
Release 2.4.2 crashes in the radeonsi driver of Mesa 26, the Mesa of unstable, when it renders the thumbnails of the plates, which happens on the first slice. `/usr/bin/orcaslicer` therefore starts it with `LIBGL_ALWAYS_SOFTWARE=1` when the installed `mesa-libgallium` is version 26 or newer, which renders with llvmpipe on the CPU. That is slower, but fast enough to work with. On trixie, whose Mesa is older, it renders on the GPU as usual. A `LIBGL_ALWAYS_SOFTWARE` that is set already is kept, so to use the GPU anyway, for example to check whether a new Mesa fixes the crash:
```
LIBGL_ALWAYS_SOFTWARE=0 orcaslicer
```

The alpha releases of OrcaSlicer no longer crash. When the next stable release is packaged, `/usr/bin/orcaslicer` can start it without software rendering.

The package goes to both suites of the repository, `trixie` and `unstable`.

## Building the package
The package is built from `orcaslicer` in this repository:
```
cd orcaslicer
make deb
```

This builds `build/orcaslicer_<release>-<revision>_amd64.deb`. Install it with:
```
sudo apt install ./build/orcaslicer_2.4.2-1_amd64.deb
```

The build downloads the AppImage of the release with `wget` and checks it against the SHA-256 in the Makefile; a download that does not match is removed. It then unpacks it with `--appimage-extract` into `build/tree-<release>`. Later builds reuse both, until `make clean` removes them. For a new release, change `ORCA_VERSION` and `APPIMAGE_SHA256` in the Makefile and set `DEB_REVISION` back to 1.

The files involved are:
```
orcaslicer/Makefile
orcaslicer/orcaslicer
orcaslicer/deb/control
orcaslicer/deb/copyright
orcaslicer/deb/orcaslicer.desktop
orcaslicer/deploy.json
```

## Files
| Path | Purpose |
| --- | --- |
| `/usr/bin/orcaslicer` | Starts OrcaSlicer, with software rendering on Mesa 26 or newer. |
| `/usr/lib/orcaslicer/` | The unpacked AppImage. |
| `/usr/share/applications/orcaslicer.desktop` | The desktop entry. |
| `/usr/share/icons/hicolor/192x192/apps/orcaslicer.png` | The icon of the release. |
| `/usr/share/doc/orcaslicer/copyright` | The license, AGPL-3.0. |
