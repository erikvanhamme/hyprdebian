## Introduction
This page lists what a computer needs to build HyprDebian from this repository: the packages with `make all`, the kernels with `make kernel`, their tests with `make test`, the live ISO with `make iso`, and the tools around them. Every build runs as a normal user; none needs root, but the Docker builds need a user that may run docker.

The builds are made for amd64. `hd-backup`, `hd-desktop`, `orcaslicer` and the packages compiled from Rust take the architecture of the build computer or are amd64 only, and the AppImage of OrcaSlicer is run on the build computer to unpack it.

## In short
On a Debian trixie or unstable amd64 computer, this installs everything the package builds and the tests need:
```
sudo apt install build-essential python3 clang lld gnu-efi wget unzip ca-certificates docker.io tar util-linux
```

Then let the user run docker, and log in again:
```
sudo usermod -aG docker $USER
```

The computer needs internet access while it builds: some builds download releases from GitHub, and the Docker builds pull `debian:trixie` (`debian:unstable` for the kernels, `ouch`, `wiremix`, `yazi`, `hd-menu`, `hd-message`, the screensavers and `hd-audacious`) and the packages, toolchains and sources they compile.

## The packages: make all
`make all` in the root of the repository runs `make deb` in every package directory in `packages/` and collects the packages in `build/`, in a folder by the repos of the `deploy.json` of their package directory: `build/both` for trixie and unstable, `build/trixie` and `build/unstable` for one suite. It builds the packages one after the other, each with as many jobs at the same time as the computer has cores; `make all JOBS=1` builds one target at a time.

### Every package
Each package is staged in its `build/deb` and built with `dpkg-deb`. Each Makefile uses:

| Tool | Debian package | Used for |
| --- | --- | --- |
| `make` | make | The Makefiles. |
| `python3` | python3 | Reading the version from `version.json`. Only the standard library. |
| `dpkg-deb` | dpkg | Building the package. `--root-owner-group` needs dpkg 1.19 or newer. |
| `install`, `du`, `cut`, `stat`, `sha256sum` | coreutils | Staging the files, the `Installed-Size` of the control file and checking downloads. |
| `sed`, `gzip`, `find` | sed, gzip, findutils | The control file, the man pages and the lists of files. |

### Per package
| Package | Needs | Why |
| --- | --- | --- |
| `hd-agent`, `hd-base`, `hd-buildserver`, `hd-cups`, `hd-dashboard`, `hd-docker`, `hd-installer`, `hd-laptop`, `hd-qemu-kvm`, `hd-repo` | Only the tools above. | They only stage scripts, configs and man pages. |
| `hd-backup` | `gcc`, `libc6-dev`, `dpkg-buildflags` (dpkg-dev), `strip` (binutils); all in build-essential | Compiles `blockwrite` from `blockwrite.c` with the hardening flags of Debian, and strips it. |
| `hd-bootloader` | `clang++` (clang), `lld-link` (lld), the headers in `/usr/include/efi` (gnu-efi) | Compiles the EFI bootloader for the target `x86_64-unknown-windows` and links it as a PE32+ image. Only the headers of gnu-efi are used. Works with gnu-efi 3.0 (trixie) and 4.0. |
| `orcaslicer` | `wget`, ca-certificates, internet access, an amd64 computer | Downloads the OrcaSlicer 2.4.2 AppImage from GitHub once, checks its sha256 and unpacks it by running it with `--appimage-extract`, which needs no FUSE. |
| `hd-desktop` | `docker` (docker.io), a user that may run docker, `strip` (binutils), `dpkg` for `--print-architecture`, `wget`, `unzip`, ca-certificates, internet access | Compiles hd-menu and hd-message in a container of `hd-build:unstable`, the build image of unstable from `buildenv/Dockerfile`, which it builds first, pulling `debian:unstable`. The image has the build tools and the -dev packages of wayland, xkbcommon, cairo and librsvg. The package gets the architecture of the build computer. `make all` in `menu/` or `message/`, which builds hd-menu or hd-message without a package to try it, compiles in that container too; the binary runs on the computer itself, which needs unstable and `libwayland-client0`, `libxkbcommon0`, `libcairo2` and `librsvg2-2`, as a HyprDebian desktop has. Downloads the JetBrains Mono and Source Code Pro zips of Nerd Fonts 3.4.0 from GitHub once, checks their sha256 and unpacks the fonts. |
| `hd-screensavers` | `docker` (docker.io), a user that may run docker, `strip` (binutils), `dpkg` for `--print-architecture`, internet access | Compiles the screensavers in a container of `hd-build:unstable`, which it builds first, pulling `debian:unstable`. The image has the build tools, the -dev packages of Wayland and Vulkan, and glslang-tools for the shaders. The package gets the architecture of the build computer. `make all` in `pipes/`, which builds hd-pipes without a package to try it, compiles in that container too; the binary runs on the computer itself, which needs unstable, `libwayland-client0`, `libvulkan1` and a Vulkan driver, as `mesa-vulkan-drivers`. |
| `hd-audacious` | `docker` (docker.io), a user that may run docker, `dpkg` for `--print-architecture`, internet access | Compiles the forks of audacious and audacious-plugins in a container of `hd-build:unstable`, which it builds first, pulling `debian:unstable`. The image has the build tools, meson and the -dev packages of the plugins: glib, notcurses, FLAC, mpg123, sndfile and PipeWire. The container clones the commits from GitHub and strips the binaries. The package gets the architecture of the build computer. |
| `hd-kernel`, with `make kernel`, not `make all` | `docker` (docker.io), a user that may run docker, internet access, about 30 GB of disk per suite, and memory: 5/8 GB per core | Builds the kernel of Debian and its ZFS module for each suite, in containers of `hd-build:trixie` and `hd-build:unstable`, the build images from `buildenv/Dockerfile`, whose shared layer has its build tools, Rust and pahole among them; it builds them first, pulling `debian:trixie` and `debian:unstable`. Each container gets the source packages `linux` and `zfs-linux` and the config of Debian from its apt sources. It takes about half an hour per suite on a recent computer, more on an older one; `HD_KERNEL_CACHE` reuses earlier builds, see [hd-kernel](../packages/hd-kernel.md). |
| `ouch`, `wiremix`, `yazi` | `docker` (docker.io), a user that may run docker, `strip` (binutils), `dpkg` for `--print-architecture`, internet access | Compiles the release in a container of `hd-build:unstable`, the build image of unstable from `buildenv/Dockerfile`, which it builds first, pulling `debian:unstable`. The image has the build tools, rustup and `libpipewire-0.3-dev`; the build updates to the latest stable Rust toolchain and clones the sources from GitHub. The packages get the architecture of the build computer. |

The downloads and the compiled releases are kept in `build/` of their package directory, so later builds reuse them until `make clean`.

## The tests: make test
`make test` runs the unittest tests of `hd-dashboard`, `hd-agent`, `hd-backup` and `hd-base`, and the C++ tests of `hd-screensavers`. They need:

| Package | Needs |
| --- | --- |
| `hd-base` | `python3`, only the standard library. |
| `hd-dashboard` | `python3`, only the standard library. |
| `hd-agent` | `python3`, only the standard library. It also loads `packages/hd-dashboard/hd-dashboard`. |
| `hd-screensavers` | `docker` (docker.io) and a user that may run docker: the tests are compiled in a container of `hd-build:unstable`, which `make test` builds first, and run on the computer itself, which needs the libstdc++ of unstable. They need no Wayland or Vulkan. |
| `hd-backup` | `python3` and the C compiler of `make deb`, as it builds `blockwrite` first. Some tests are skipped without `tar`, without `unshare` (util-linux) and working unprivileged user namespaces, or when they run as root. |

## The live ISO: make iso
`make iso` builds every package and the kernels, then the ISO in a container, see [Live cd](../Livecd/Overview.md). The computer needs everything `make all` and `make kernel` need, and:
- `docker` (docker.io) and a user that may run it.
- Privileged containers: the container runs with `--privileged`, because live-build mounts filesystems and makes device nodes in its chroot.
- Internet access, for the `debian:trixie` image, the packages of `livecd/Dockerfile` and the packages of the live system.

live-build, debootstrap, the boot loaders and `apt-ftparchive` (apt-utils), which indexes the local repository of the hd-* packages, are all installed in the image.

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
`hd-buildserver` builds everything every night in a container of `buildenv/Dockerfile`, which installs what this page lists, see [hd-buildserver](../packages/hd-buildserver.md). Add the build dependencies of a new package to that Dockerfile as well.

## Uploading to the repository: make update-repo
`make update-repo` builds every package, then runs `update-repo/update-repo.py` with them and the kernels that `make kernel` built, which needs:
- Everything `make all` needs.
- `ssh` and `scp` (openssh-client).
- An ssh key of the user in the `authorized_keys` of the user `repo` on the HyprDebian repository, a computer with `hd-repo`.
