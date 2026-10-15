## Introduction
Hyprdebian has its own live ISO. It is a Debian trixie live system with everything the installer needs already on it, so the installer can skip its prerequisites phase.

The live cd contains:
- A text console, no desktop. The live user is logged in on the console automatically.
- gpm, so the mouse works on the console for copy and paste.
- An ssh server, so the installation can also be done from another machine.
- NetworkManager. Wired networks come up by themselves, `nmtui` sets up wifi.
- ZFS support. The ZFS kernel module is built with DKMS while the ISO is built and is loaded at boot.
- The tools the installer needs: debootstrap, dosfstools, gdisk, git, mdadm, parted, python3-jinja2, unzip and wget.
- A checkout of the hyprdebian repo in the home folder of the live user.
- A `hyprdebian.release` file in the home folder of the live user. The installer looks for this file to know it is running on the hyprdebian live cd. When it is found, the prerequisites phase is skipped.

The live cd is built with live-build. The build script is:
```
livecd/livecd.sh
```

## Building in docker
The recommended way to build the ISO is in a docker container. This keeps live-build and its dependencies off the desktop system.

The files involved are:
```
livecd/Dockerfile
livecd/docker-entrypoint.sh
```

The Dockerfile makes a debian trixie image with live-build and the tools it needs. The build script is copied into the image, so the image must be rebuilt after `livecd.sh` changes. This is fast because docker caches the package layer.

From the root of the repo, build the image with:
```
docker build -t hyprdebian-livecd livecd/
```

Then build the ISO with:
```
mkdir -p livecd/out
docker run --rm --privileged -e HOST_UID="$(id -u)" -e HOST_GID="$(id -g)" -v "$PWD/livecd/out:/out" hyprdebian-livecd
```

The finished ISO is written to:
```
livecd/out/hyprdebian-live-amd64.iso
```

Notes:
1. The container needs `--privileged` because live-build mounts filesystems and creates device nodes in its chroot.
2. The whole build happens inside the container. Only the ISO is copied out. With `--rm` the build files are thrown away afterwards.
3. HOST_UID and HOST_GID make the ISO owned by your own user instead of root.
4. Every build starts from scratch and downloads all packages again. A build takes a while, mainly because of the ZFS module.
5. `livecd/out` is in .gitignore.

### Following the build
Add `--name livecd-build` to the docker run command to give the container a fixed name. The build output can then be followed from another terminal with:
```
docker logs -f livecd-build
```

A shell inside the running build can be opened with:
```
docker exec -it livecd-build bash
```

The live-build working folder is `/build/live-trixie-hyprdebian` inside the container.

### Expected warnings
The following warnings show up during a successful build and can be ignored:
1. mdadm: "failed to load MD subsystem" and "failed to auto-generate mdadm.conf". There are no arrays to scan inside the build chroot.
2. isohybrid: "more than 1024 cylinders" and "Not all BIOSes will be able to boot this device". This is normal for an ISO of this size and only matters for very old BIOS machines.

## Building without docker
The build script can also be run directly on a Debian system. This needs live-build, debootstrap, syslinux-utils and the other live-build dependencies installed on that system.
```
cd livecd
./livecd.sh
```

The script makes the folder `live-trixie-hyprdebian` in the current folder and builds the ISO in there as `live-image-amd64.hybrid.iso`. It uses sudo for the build itself. The folder must not exist yet when the script starts.

## Using the live cd
Write the ISO to a USB stick, for example with:
```
sudo dd if=livecd/out/hyprdebian-live-amd64.iso of=/dev/sdX bs=4M status=progress oflag=sync
```

Replace /dev/sdX with the USB stick. Everything on it will be lost.

Boot from the USB stick. The live cd boots on both UEFI and BIOS machines.

Turn Secure Boot off in the firmware before booting the live cd: the ZFS module of the live cd is signed with a key the firmware does not trust, so the live cd cannot load it with Secure Boot on. The installed system can use Secure Boot. On its first boot, MokManager asks to enroll the key of its ZFS module, with the password the installer shows at the end. Turn Secure Boot back on after that boot.

The live cd boots to a text console, logged in as the live user `user`. Its password is `live`.

### Copy and paste on the console
gpm runs on the console, so the mouse works there:
- Select text with the left button. Double click selects a word, triple click a line.
- Paste with the middle button.

This is handy for disk wwn's: list them with `ls -l /dev/disk/by-id/`, select one and paste it where the installer asks for it.

### Installing over ssh
The ssh server is started at boot. Find the address of the live system with:
```
ip -br addr
```

Then log in from another machine with:
```
ssh user@<address>
```

The password is `live`. Wifi can be set up with `sudo nmtui`.

The ssh host keys of the live cd can change between boots and between ISO builds. ssh then warns about a changed host key. Remove the old key with `ssh-keygen -R <address>`.

### Running the installer
Run:
```
cd hyprdebian
git pull
sudo ./install.sh
```

The repo on the live cd is the version from the moment the ISO was built. The `git pull` brings it up to date.

## Notes
The admin tools such as mdadm, gdisk and zpool are in /usr/sbin. That folder is not in the PATH of the live user, so running them without sudo gives "command not found". Run them with sudo:
```
sudo mdadm --version
```
