## Introduction
The package `hd-kernel` switches a HyprDebian system to the HyprDebian kernel: the kernel of Debian, with the patches and the config of Debian, and the ZFS module of the `zfs-linux` of Debian built against it on the build server. A system with it needs neither DKMS nor a compiler for ZFS, and installs and upgrades its kernel without building anything.

Every build signs the modules of the kernel and the ZFS module with a key that it makes for the build and then throws away. The kernel trusts that key and no other for its modules, so under Secure Boot not even root can load a module that the build did not sign. With DKMS, the key the kernel trusts for the ZFS module lies on every system, readable by root.

For now a system switches when its admin installs `hd-kernel`; `hd-base` still depends on `zfs-dkms`, which builds nothing for these kernels, as they come without headers.

## The packages
`make deb` builds three packages for each suite, `trixie` and `unstable`, each only for the repository of its suite:

| Package | |
| --- | --- |
| `linux-image-<release>` | The kernel and its modules. |
| `zfs-modules-<release>` | `zfs.ko` and `spl.ko` in `/lib/modules/<release>/extra/zfs`. It provides `zfs-modules`, which `zfs-initramfs` takes in place of `zfs-dkms`, and adds the module to the initrd of its kernel. |
| `hd-kernel` | Depends on the exact versions of both, so that the newest build is installed and upgraded with it. It conflicts with `linux-image-amd64` and `linux-headers-amd64`, and installs the man page `man hd-kernel`. |

The release is `<x.y.z>+debNN-hd<id>-amd64`, such as `7.2.12+deb14-hd1a2b3c-amd64`: `debNN` as in the release of the kernel of Debian, and `<id>` six hex digits of a sha256 of everything the packages are made of: the versions of `linux`, `zfs-linux` and `hd-kernel`, `build-kernel.sh` and the files in `deb/` and `man/`. A build of anything else has another release, and so installs next to the kernel that runs, whose modules stay signed with the key it trusts. The versions of the packages are those of `linux` and `zfs-linux` with `+hd` and the version of `hd-kernel`; `hd-kernel` itself also has the version of `zfs-linux`, so that it goes up with each of the three.

The bootloader names the UKI of such a kernel `HyprDebian-Kernel-<x.y.z>-hd.efi`, so the menu tells it apart from the kernel of Debian, see [hd-bootloader](hd-bootloader.md).

## Switching
On a system with `hd-base`:
```
sudo apt install hd-kernel
```

This installs the kernel and its ZFS module, and removes `linux-image-amd64` and `linux-headers-amd64`, so the kernels of Debian get no more upgrades. The new kernel is at the top of the menu of the bootloader. Once it boots, the kernels of Debian can go:
```
sudo apt purge linux-image-<version>-amd64 linux-headers-<version>-amd64
```

To switch back to the kernel of Debian, which gets its ZFS module from `zfs-dkms` again:
```
sudo apt install linux-image-amd64 linux-headers-amd64
```

This removes `hd-kernel`; the HyprDebian kernels stay until they are purged.

## Building the packages
The packages are built from `packages/hd-kernel` in this repository:
```
cd packages/hd-kernel
make deb
```

This builds the packages of both suites, into `build/trixie` and `build/unstable`; `make deb-trixie` or `make deb-unstable` builds one.

In the root of the repository, `make kernel` does the same and puts them in `build/<suite>`; `make all` leaves the kernels out, as they take long to build. The Makefile in the root takes them from `make deb-path`, which prints each as `<suite>:build/<suite>/<package>.deb`. `make update-repo` and `make list-debs` then also give the kernels that `make kernel` built, each only for its suite, but do not build them: run `make kernel` first for new ones. The live ISO does not get them, as the installer does not install them yet.

Raise the version in `version.json` for every change of the packages.

A suite is only built when it has to be. `make deb` starts a container of each suite, which looks up the versions of `linux` and `zfs-linux` and works out the release; when `build/<suite>` already has the three packages of that release, it stops there, which takes a few seconds. So a second `make deb` or `make kernel` builds no kernel, until Debian has a new `linux` or `zfs-linux`, `build-kernel.sh`, `deb/`, `man/` or the version changes, or `make clean` removes the packages. The packages only get to `build/<suite>` at the end of a build that went through, so a build that stops halfway is done again.

`build-kernel.sh` does the work, in a `debian:trixie` or `debian:sid` container, pulled every time:
1. It takes the most recent versions of the source packages `linux` and `zfs-linux` of the suite, also of `trixie-security`, and gets them with `apt source`, with the patches of Debian, and the config of Debian for amd64 from `linux-config-<x.y>` of the same version.
2. It changes the config only where it has to: the kernel trusts no keys of Debian (`SYSTEM_TRUSTED_KEYS`), signs every module with a key that the build makes in `certs/signing_key.pem` (`MODULE_SIG_ALL`), and has the release above (`LOCALVERSION`). It stops when an option that Debian has on is off in the config of the build, as `olddefconfig` silently turns off what the tools in the container cannot build, such as Rust.
3. It builds the kernel with `make bindeb-pkg`, with the ccache of the compiler.
4. It builds the ZFS module against that kernel tree, as DKMS would, and signs `zfs.ko` and `spl.ko` with the key of the build, which it then removes.
5. It packages the ZFS module and `hd-kernel`, with the control files and maintainer scripts in `deb/`.

ZFS is not copied into the kernel tree with `copy-builtin`: then the `configure` of ZFS takes it to be built into the kernel and skips its checks for the symbols that only GPL modules may use, so a `zfs.ko` built that way does not link.

The kernel trees stay in `build/work/<suite>`, with the ccache, so a build of the same versions only compiles what changed; `make clean` removes them. A kernel build takes about half an hour per suite on a recent computer and about 30 GB of disk. Each container gets at most 5/8 GB of memory per core (`MEMORY`), so that when its compilers need more, the kernel kills the build and not other programs; `make deb JOBS=8` builds with fewer jobs and less memory.

With `HD_KERNEL_CACHE` set to a directory, a suite whose release is already built there is copied from it instead of built, and every new build is stored there in place of the one before. [hd-buildserver](hd-buildserver.md) passes its cache, so a commit that does not touch the kernel does not build it again.

`make sources` prints the versions `make deb` would build, a line per suite, which `hd-buildserver` uses to build when Debian has a new kernel or ZFS:
```
make -s sources
```

`make test-build` builds the packages of unstable quicker, into `build/test`: with only the modules of `local-testvm/lsmod-test.txt`, the output of `lsmod` on the test vm, and without debug info, so without BTF. Its release has `hdtest<id>`, with the module list in the sha256 too, and it is skipped in the same way. It is only for trying out on the test vm, never for a repository.

The files involved are:
```
packages/hd-kernel/Makefile
packages/hd-kernel/version.json
packages/hd-kernel/deploy.json
packages/hd-kernel/build-kernel.sh
packages/hd-kernel/deb/hd-kernel/control
packages/hd-kernel/deb/zfs-modules/control
packages/hd-kernel/deb/zfs-modules/postinst
packages/hd-kernel/deb/zfs-modules/postrm
packages/hd-kernel/man/hd-kernel.7
local-testvm/lsmod-test.txt
```

## Copyright
The kernel is under the GPL version 2, with the licenses of its parts; `zfs-modules` holds the copyright file of `zfs-linux`, in `/usr/share/doc/zfs-modules-<release>/copyright`: OpenZFS is under the CDDL.

## Files
| Path | Purpose |
| --- | --- |
| `/boot/vmlinuz-<release>` | The kernel. |
| `/lib/modules/<release>` | Its modules. |
| `/lib/modules/<release>/extra/zfs` | The ZFS module. |
| `/usr/share/man/man7/hd-kernel.7.gz` | The man page. |
