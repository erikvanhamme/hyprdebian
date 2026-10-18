## Introduction
This page lists what a computer needs to build HyprDebian from this repository: the packages with `make all`, their tests with `make test`, the live ISO with `make iso`, and the tools around them. Every build runs as a normal user; none needs root, but the Docker builds need a user that may run docker.

The builds are made for amd64. `hd-backup`, `orcaslicer` and the packages compiled from Rust take the architecture of the build computer or are amd64 only, and the AppImage of OrcaSlicer is run on the build computer to unpack it.

## In short
On a Debian trixie or unstable amd64 computer, this installs everything the package builds and the tests need:
```
sudo apt install build-essential python3 clang lld gnu-efi wget unzip ca-certificates docker.io tar util-linux
```

Then let the user run docker, and log in again:
```
sudo usermod -aG docker $USER
```

The computer needs internet access while it builds: some builds download releases from GitHub, and the Docker builds pull `debian:trixie` and the packages, toolchains and sources they compile.

## The packages: make all
`make all` in the root of the repository runs `make deb` in every package directory in `packages/` and collects the packages in `build/`.

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
| `hd-art`, `hd-base`, `hd-config`, `hd-cups`, `hd-dashboard`, `hd-desktop`, `hd-docker`, `hd-installer`, `hd-network`, `hd-qemu-kvm`, `hd-repo`, `hd-report`, `hd-screensaver`, `hd-secboot` | Only the tools above. | They only stage scripts, configs and man pages. |
| `hd-backup` | `gcc`, `libc6-dev`, `dpkg-buildflags` (dpkg-dev), `strip` (binutils); all in build-essential | Compiles `blockwrite` from `blockwrite.c` with the hardening flags of Debian, and strips it. |
| `hd-bootloader` | `clang++` (clang), `lld-link` (lld), the headers in `/usr/include/efi` (gnu-efi) | Compiles the EFI bootloader for the target `x86_64-unknown-windows` and links it as a PE32+ image. Only the headers of gnu-efi are used. Works with gnu-efi 3.0 (trixie) and 4.0. |
| `hd-fonts` | `wget`, `unzip`, ca-certificates, internet access | Downloads the JetBrains Mono zip of Nerd Fonts 3.4.0 from GitHub once, checks its sha256 and unpacks the fonts. |
| `orcaslicer` | `wget`, ca-certificates, internet access, an amd64 computer | Downloads the OrcaSlicer 2.4.2 AppImage from GitHub once, checks its sha256 and unpacks it by running it with `--appimage-extract`, which needs no FUSE. |
| `ouch`, `wiremix`, `yazi` | `docker` (docker.io), a user that may run docker, `strip` (binutils), `dpkg` for `--print-architecture`, internet access | Compiles the release in a `debian:trixie` container, pulled every time. The container installs its own build tools and the latest stable Rust toolchain with rustup, and clones the sources from GitHub. The packages get the architecture of the build computer. |

The downloads and the compiled releases are kept in `build/` of their package directory, so later builds reuse them until `make clean`.

## The tests: make test
`make test` runs the unittest tests of `hd-dashboard`, `hd-report` and `hd-backup`. They need:

| Package | Needs |
| --- | --- |
| `hd-dashboard` | `python3`, only the standard library. |
| `hd-report` | `python3`, only the standard library. It also loads `packages/hd-dashboard/hd-dashboard`. |
| `hd-backup` | `python3` and the C compiler of `make deb`, as it builds `blockwrite` first. Some tests are skipped without `tar`, without `unshare` (util-linux) and working unprivileged user namespaces, or when they run as root. |

## The live ISO: make iso
`make iso` builds every package, then the ISO in a container, see [Live cd](../Livecd/Overview.md). The computer needs everything `make all` needs, and:
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
| The wallpaper and the font | hd-art, hd-fonts | The graphical frontend; not for `ESP_PACKAGE=txt`. |
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

## Uploading to the repository: make update-repo
`make update-repo` builds every package, then runs `update-repo/update-repo.py`, which needs:
- Everything `make all` needs.
- `ssh` and `scp` (openssh-client).
- An ssh key of the user in the `authorized_keys` of the user `repo` on the HyprDebian repository, a computer with `hd-repo`.
