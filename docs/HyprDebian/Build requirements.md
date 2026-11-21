## Introduction
This page lists what a computer needs to build HyprDebian from this repository: the packages with `make all`, the kernels with `make kernel`, their tests with `make test`, the live ISO with `make iso`, and the tools around them. Every build runs as a normal user; none needs root, but the builds that compile need a user that may run docker.

Nothing is compiled on the computer itself. Every compiler, -dev package and build tool is in the build images `hd-build:trixie` and `hd-build:unstable`, which the builds build from `buildenv/Dockerfile` and run their compilers in, see [ADR 0002](../adr/0002-one-build-image-per-suite.md). The computer needs two lists of packages: what it needs to build, and, apart, what it needs to run the binaries that `make test` and the tryouts compile in a build image and run on the computer itself.

The builds are made for amd64. `hd-backup`, `hd-desktop`, `orcaslicer` and the packages compiled from Rust take the architecture of the build computer or are amd64 only, and the AppImage of OrcaSlicer is run on the build computer to unpack it.

## In short
On a Debian trixie or unstable amd64 computer, this installs what the builds need:
```
sudo apt install make python3 docker.io wget unzip ca-certificates
```

Then let the user run docker, and log in again:
```
sudo usermod -aG docker $USER
```

`make test` and the tryouts also need the runtime libraries of their binaries, see [The tests and the tryouts](#the-tests-and-the-tryouts-make-test). On a HyprDebian desktop on unstable they are installed already.

The computer needs internet access while it builds: the build images pull `debian:trixie` and `debian:unstable` and the packages of the Dockerfile, and some builds download releases, sources and toolchains from GitHub, Debian and rustup.

## What the computer needs to build
| Tool | Debian package | Used for |
| --- | --- | --- |
| `make` | make | The Makefiles. |
| `python3` | python3 | Reading the version from `version.json` and the repos from `deploy.json`. Only the standard library. |
| `docker` | docker.io | Building the build images and running the compilers, `dpkg-shlibdeps`, `strip` and live-build in containers of them. The user must be allowed to run docker, and `make iso` needs privileged containers, as live-build mounts filesystems and makes device nodes in its chroot. |
| `wget` | wget, ca-certificates | The downloads of `hd-desktop` (the Nerd Fonts 3.4.0 zips) and `orcaslicer` (the OrcaSlicer 2.4.2 AppImage), each once, checked with its sha256. |
| `unzip` | unzip | Unpacking the fonts of `hd-desktop`. |
| `dpkg-deb`, `dpkg` | dpkg | Building the packages from their staged root in `build/deb`, and the architecture of the build computer for the packages that compile. `--root-owner-group` needs dpkg 1.19 or newer. |
| `install`, `du`, `cut`, `stat`, `sha256sum`, `sed`, `gzip`, `find` | coreutils, sed, gzip, findutils | Staging the files, the `Installed-Size` of the control file, checking downloads, the control file, the man pages and the lists of files. |

The downloads and the compiled releases are kept in `build/` of their package directory, so later builds reuse them until `make clean`. The build images are about 2 GB each; docker keeps their layers, so only a new image of the suite or a change to the Dockerfile installs packages again, and each build removes the build images that a new one replaced.

## The packages: make all
`make all` in the root of the repository runs `make deb` in every package directory in `packages/` and collects the packages in `build/`, in a folder by the repos of the `deploy.json` of their package directory: `build/both` for trixie and unstable, `build/trixie` and `build/unstable` for one suite. It builds the packages one after the other, each with as many jobs at the same time as the computer has cores; `make all JOBS=1` builds one target at a time.

Each package Makefile that compiles builds the image of its suite first, so `make deb` in a package directory works on its own. A package is compiled on the suite it is for: on unstable when it goes to unstable only, on trixie when it goes to both repos, as a binary built on unstable can need a newer glibc than trixie has.

| Package | Built in | What happens in the container |
| --- | --- | --- |
| `hd-agent`, `hd-base`, `hd-buildserver`, `hd-cups`, `hd-dashboard`, `hd-docker`, `hd-installer`, `hd-laptop`, `hd-manager`, `hd-qemu-kvm`, `hd-repo` | No container | They only stage scripts, configs and man pages. |
| `orcaslicer` | No container | Unpacks the AppImage by running it with `--appimage-extract`, which needs no FUSE, on an amd64 computer. |
| `hd-backup` | `hd-build:trixie` | Compiles `blockwrite` from `blockwrite.c` with the hardening flags of Debian and strips it; its `libc6` in the Depends comes from `dpkg-shlibdeps`. |
| `hd-bootloader` | `hd-build:trixie` | Compiles the EFI bootloader for the target `x86_64-unknown-windows` with `clang++` and links it as a PE32+ image with `lld-link`; of gnu-efi only the headers are used. |
| `hd-desktop` | `hd-build:unstable` | Compiles hd-menu and hd-message against wayland, xkbcommon, cairo and librsvg, and strips them. |
| `hd-screensavers` | `hd-build:unstable` | Compiles the screensavers against Wayland and Vulkan, and their shaders with glslang-tools. |
| `hd-audacious` | `hd-build:unstable` | Clones the forks of audacious and audacious-plugins from GitHub and compiles them with meson, with the plugins for notcurses, FLAC, mpg123, sndfile and PipeWire. |
| `ouch`, `wiremix`, `yazi` | `hd-build:unstable` | Clones the release from GitHub, updates to the latest stable Rust toolchain with rustup and compiles it. |
| `hd-kernel`, with `make kernel`, not `make all` | `hd-build:trixie` and `hd-build:unstable` | Gets the source packages `linux` and `zfs-linux` and the config of Debian from the apt sources of the suite and builds the kernel and its ZFS module. It needs about 30 GB of disk per suite and 5/8 GB of memory per core, and takes about half an hour per suite on a recent computer, more on an older one; `HD_KERNEL_CACHE` reuses earlier builds, see [hd-kernel](../packages/hd-kernel.md). |

The packages compiled in a build image get their Depends from `dpkg-shlibdeps` in the container, from the libraries of the suite they are for.

## The tests and the tryouts: make test
`make test` runs the unittest tests of `hd-dashboard`, `hd-agent`, `hd-manager`, `hd-backup` and `hd-base`, and the C++ tests of `hd-screensavers`. The tryouts are `make all` in `menu/` and `message/` of `hd-desktop` and in `pipes/` of `hd-screensavers`: they build hd-menu, hd-message or hd-pipes without a package, to try it.

The test binaries and the tryouts are compiled in the build image of the suite of their package, as `make deb` does, and run on the computer itself. That computer needs their runtime libraries:

| Run | Needs on the computer |
| --- | --- |
| `make test` of `hd-base`, `hd-dashboard`, `hd-agent`, `hd-manager` | `python3`, only the standard library. The tests of `hd-agent` also load `packages/hd-dashboard/hd-dashboard`. |
| `make test` of `hd-backup` | `python3`, and `blockwrite`, compiled on trixie, runs with the libc6 of trixie or unstable. Some tests are skipped without `tar`, without `unshare` (util-linux) and working unprivileged user namespaces, or when they run as root. |
| `make test` of `hd-screensavers` | Unstable, for the libstdc++ of unstable (`libstdc++6`). The tests need no Wayland or Vulkan. |
| `make all` in `menu/` and `message/` of `hd-desktop` | Unstable, with `libwayland-client0`, `libxkbcommon0`, `libcairo2` and `librsvg2-2`, and a Wayland session to show them in. |
| `make all` in `pipes/` of `hd-screensavers` | Unstable, with `libwayland-client0`, `libvulkan1` and a Vulkan driver, as `mesa-vulkan-drivers`, and a Wayland session. |

A HyprDebian desktop on unstable has all of these.

## The live ISO: make iso
`make iso` builds every package and the kernels, then the ISO in a `--privileged` container of `hd-build:trixie`, with `livecd/build.sh` as the command, see [Live cd](../Livecd/Overview.md). live-build, debootstrap, the boot loaders and `apt-ftparchive` (apt-utils), which indexes the local repository of the hd-* packages, are in the image. The computer needs what `make all` and `make kernel` need, and internet access for the packages of the live system.

## The bootloader for development: make esp and make image
`make esp` and `make image` in `packages/hd-bootloader` are not part of `make all`. They build an ESP from the kernels of the build computer, to try the bootloader in QEMU, and also need:

| Needs | Debian package | Why |
| --- | --- | --- |
| `ukify` | systemd-ukify | Adds the assets to the stub and builds the UKIs. |
| The UKI stub | systemd-boot-efi | The UKIs. |
| `PIL` | python3-pil | The graphical frontend draws its assets. |
| `fc-match` | fontconfig | Finds the font of the graphical frontend. |
| The wallpaper and the font | hd-desktop | The graphical frontend; not for `ESP_PACKAGE=txt`. |
| Kernels and initrds in `/boot` | | The UKIs of the 4 most recent kernels. |
| `sbsign`, `sbverify` | sbsigntool | Only when there are Secure Boot keys to sign with. |
| `mkfs.fat` | dosfstools | `make image`. |
| `mcopy` | mtools | `make image`. |

## Provisioning a vm: make provision-vm
`vm/provision-vm.py` defines and starts a vm on the computer, see [VM provisioning](../VM/Provisioning.md). It needs:
- `python3` with `jinja2` (python3-jinja2).
- `virsh` (libvirt-clients), a running libvirtd, QEMU and OVMF; `hd-qemu-kvm` installs all of these.
- `zfs` (zfsutils-linux) and a zvol for the vm.
- A network bridge, `br0` by default.
- The live ISO in `livecd/out/hyprdebian-live-amd64.iso`.

`make switch-vm-gpu` runs `vm/switch-vm-gpu.py`, which switches a vm between running with and without a GPU. It needs `python3` and `virsh` with a running libvirtd.

## On a build server
`hd-buildserver` builds everything every night in a container of `hd-build:trixie`, the build image of `buildenv/Dockerfile`, which starts the builds in the build images with the docker of the host, as on a computer you develop on, see [hd-buildserver](../packages/hd-buildserver.md). The build server needs nothing more than that Dockerfile has, so a new build dependency goes in `buildenv/Dockerfile` only.

## Uploading to the repository: make update-repo
`make update-repo` builds every package, then runs `update-repo/update-repo.py` with them and the kernels that `make kernel` built, which needs:
- Everything `make all` needs.
- `ssh` and `scp` (openssh-client).
- An ssh key of the user in the `authorized_keys` of the user `repo` on the HyprDebian repository, a computer with `hd-repo`.
