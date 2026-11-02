# Builds the HyprDebian kernel: the kernel of Debian, with its patches and its
# config, and the ZFS module of the zfs-linux of Debian built against it and
# signed with its key, so a system needs no compiler and no DKMS for ZFS.
# Each suite in SUITES gets a kernel of its own, built from the most recent
# linux and zfs-linux of that suite in a container of it, so the build needs
# docker and a user that may run it. build-kernel.sh does the work, see there,
# and kernel.mk runs it, also for hd-kernel-vm, hd-kernel-server and
# hd-kernel-desktop, whose Makefiles have the same targets.
#
#   make deb    build the packages of every suite, into build/<suite>:
#                 linux-image-<release>_<version>_amd64.deb
#                               the kernel and its modules
#                 zfs-modules-<release>_<version>_amd64.deb
#                               zfs.ko and spl.ko, in
#                               /lib/modules/<release>/extra/zfs
#                 hd-kernel_<version>_amd64.deb
#                               depends on both, with the man page in man/;
#                               installing it switches a system to the
#                               kernel of HyprDebian
#               with the control files and maintainer scripts in deb/. The
#               release is <x.y.z>+debNN-hd<id>-amd64, <id> from the versions
#               of linux, zfs-linux and hd-kernel; the versions of the
#               packages are those of linux and zfs-linux with +hd and the
#               version of version.json. Raise it for every change of the
#               packages. The release also changes with build-kernel.sh and
#               the files in deb/ and man/, so it says what the packages are
#               made of: a suite whose packages of that release are in
#               build/<suite> already is not built again, until make clean
#               removes them. That costs a container start and an apt update
#               per suite. With HD_KERNEL_CACHE set to a directory, a suite
#               whose release is already built there is copied from it
#               instead of built, and every build is stored there.
#   make deb-<suite>
#               build the packages of one suite
#   make deb-path
#               print the packages that make deb built, as
#               <suite>:build/<suite>/<package>.deb, for the Makefile in the
#               root, which puts each only in the repository of its suite;
#               nothing for a suite that is not built
#   make sources
#               print the versions of linux and zfs-linux that make deb would
#               build, a line per suite, as
#               "<suite> linux <version> zfs-linux <version>"; run it with
#               make -s, so make does not print the commands
#   make test-build
#               build the packages of unstable quicker, into build/test: with
#               only the modules in LSMOD (the output of lsmod on the test vm
#               by default, see make localmodconfig) and without debug info;
#               the release has hdtest<id>. Only for trying out on the test
#               vm, never for a repository.
#   make clean  remove build/, with the work of every kernel package when
#               HD_KERNEL_WORK is not set
#
# The kernels of every package, of hd-kernel-vm, hd-kernel-server and
# hd-kernel-desktop too, work in HD_KERNEL_WORK, build/work here by default: a
# directory per suite, with the sources of linux and zfs-linux of the suite,
# which they share, and the kernel tree of each package, and the ccache of the
# compiler of every suite, of at most 64 GB, which also serves a new version
# of linux the files that it did not change. So a build of the same versions
# only compiles what changed, and a new version is unpacked once for every
# package. One build of a suite runs at a time. The container gets at most
# MEMORY of memory, 5/8 GB per job, so that when the compilers need more, the
# kernel kills the build and not other programs; set JOBS lower for a machine
# with less memory free.

PACKAGE := hd-kernel
TAG := hd
# Every module of the config of Debian.
KEEP :=

include kernel.mk
