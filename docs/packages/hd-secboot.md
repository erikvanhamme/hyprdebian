## Introduction
The package `hd-base` lets a hyprdebian system boot under Secure Boot with keys of its owner, instead of the keys of Microsoft that most firmware ships with. `hd-secboot` generates the keys, writes them in the firmware, turns Secure Boot on and off, adds the hashes of option ROMs to db and shows the state of Secure Boot.

It depends on the tools that make and use the keys, so they are installed with it and stay installed:
- `openssl` makes the keys and their certificates;
- `efitools` turns the certificates into EFI signature lists and signed updates, and enrolls them with `efi-updatevar`;
- `sbsigntool` signs EFI files with `sbsign` and checks them with `sbverify`.

On its first installation, its postinst generates the keys in `/etc/hyprdebian/secboot` with `hd-secboot --generate`. When that directory already exists, on an upgrade, a reinstall or when the keys were put there by hand, the keys are kept: keys that are enrolled in the firmware are never replaced. Removing or purging the package keeps the keys as well.

`hd-base` comes with two man pages for it: `man 7 hd-secboot` describes the keys, and `man 8 hd-secboot`, which `man hd-secboot` shows, the command.

## Building the package
The keys and their commands are part of the package `hd-base`, built from `packages/hd-base`; see [hd-base](hd-base.md). The files involved are:
```
packages/hd-base/hd-secboot
packages/hd-base/deb/postinst
packages/hd-base/man/hd-secboot.7
packages/hd-base/man/hd-secboot.8
packages/hd-base/tests/test_secboot.py
```
## The keys
There are three keys, all with the same owner GUID, a random UUID in `GUID.txt`:

| Key | Purpose |
| --- | --- |
| `PK` | The platform key, which owns the firmware and signs updates of KEK. |
| `KEK` | The key exchange key, which signs updates of db. |
| `db` | The signature database key, which signs the EFI files that may boot. |

Each key is RSA 2048, the size that the UEFI specification requires every firmware to support, with a self-signed certificate that is valid for ten years and named `HyprDebian <hostname> Secure Boot <key>`. For each key, the directory holds:

| File | Content |
| --- | --- |
| `<key>.key` | The private key. |
| `<key>.crt` | The certificate as PEM, for `sbsign`. |
| `<key>.cer` | The certificate as DER, for the key menus of firmware setups. |
| `<key>.esl` | The certificate as EFI signature list. |
| `<key>.auth` | The signature list as signed update, for `efi-updatevar` or KeyTool: PK signs its own and that of KEK, KEK signs that of db. |

The directory and the files are only readable by root. `hd-secboot --generate` makes the keys in `/etc/hyprdebian/secboot.tmp` and moves them into place when they are all there, so a failed installation never leaves half a set behind; the next attempt starts over.

## Enrolling the keys
Put the firmware in setup mode in its setup menu. Most firmware has an entry under Secure Boot to delete the platform key, to clear all keys, or to reset to setup mode. Then write db and KEK, and PK last, since that ends setup mode:
```
sudo hd-secboot --store
sudo hd-secboot --enable
```

The EFI files that are to boot must be signed with the db key; `hd-bootloader-update` does that for the bootloader and the kernels (see [hd-bootloader](hd-bootloader.md)). By hand:
```
cd /etc/hyprdebian/secboot
sudo sbsign --key db.key --cert db.crt --output signed.efi unsigned.efi
```

After the next boot, `hd-secboot --status` says whether Secure Boot is on; when it is not, turn it on in the setup menu of the firmware.

## Option ROMs
Some devices have a driver in their option ROM that the firmware loads before the system boots, such as the driver (GOP) that a graphics card needs to show anything before the kernel starts. Those drivers are signed with a certificate of Microsoft, which db does not hold, so with Secure Boot on the firmware refuses them: the setup menu of the firmware and the bootloader menu stay black. `hd-secboot --append-rom-hash` adds the hashes of exactly those drivers to db instead, so no other image signed by Microsoft can boot:
```
sudo hd-secboot --disable
sudo reboot
sudo hd-secboot --append-rom-hash
sudo hd-secboot --enable
sudo reboot
```

## hd-secboot
```
hd-secboot --status [-q]
hd-secboot --generate | --store | --clear | --enable | --disable
hd-secboot --append-rom-hash
```

`hd-secboot` does one of these at a time. `--status` does not need root; the others run as root. The keys go in in this order: the firmware is put in setup mode, without a platform key, then `--store` writes db and KEK, and `--enable` writes PK, which ends setup mode. `--disable` removes PK again, and `--clear` empties db and KEK, which needs setup mode as well.

It exits with 0 when it is done, 1 on an error, and 2 on wrong arguments; `--status` exits with 0 when Secure Boot is on and 1 when it is not.

### --status
`hd-secboot --status` says whether the system booted with Secure Boot on, from the EFI variables `SecureBoot` and `SetupMode` in `/sys/firmware/efi/efivars`. Every user can read those. When Secure Boot is off, it says why:

| State | Meaning |
| --- | --- |
| on | Secure Boot is on. |
| off | Secure Boot is turned off in the setup menu of the firmware. |
| setup | The firmware is in setup mode, without a platform key: write the keys with `hd-secboot --store` and `hd-secboot --enable`. |
| unsupported | The firmware has no Secure Boot. |
| legacy | The system was not booted with UEFI. |

Then it lists what PK, KEK and db hold: the subject of each certificate and each SHA-256 hash. With `-q` or `--quiet`, it says nothing and only sets the exit status. `hd-report` sends the same state to the dashboard, which shows a system without Secure Boot as an error, see [hd-dashboard](hd-dashboard.md#issues).

### --generate
`hd-secboot --generate` generates the keys in `/etc/hyprdebian/secboot`, unless that directory exists already. The postinst runs it, so the installer has keys to sign the bootloader with.

### --store
`hd-secboot --store` writes db and KEK with `efi-updatevar` from `db.auth` and `KEK.auth`. They replace what db and KEK held, such as the certificates of Microsoft and the hashes of option ROMs. The firmware must be in setup mode.

### --clear
`hd-secboot --clear` empties db and KEK, with updates signed by KEK and PK. The firmware must be in setup mode.

### --enable
`hd-secboot --enable` writes PK from `PK.auth`, which ends setup mode, so Secure Boot is on from the next boot, unless it is turned off in the setup menu of the firmware. The firmware must be in setup mode, and db and KEK must hold the keys, so the firmware does not end up refusing everything that is signed with them. Some firmware only leaves setup mode at the next boot; then it says so.

### --disable
`hd-secboot --disable` removes PK with an update signed by the PK key, which puts the firmware in setup mode: from the next boot, it checks no signatures and Secure Boot is off. db and KEK keep what they hold, so `--enable` turns Secure Boot on again. The PK in the firmware must be the one in `/etc/hyprdebian/secboot`. In setup mode, anyone with root can change db, KEK and PK without a signature.

### --append-rom-hash
`hd-secboot --append-rom-hash` adds the SHA-256 hashes of option ROMs to db. The firmware measures each driver it loads in the TPM event log `/sys/kernel/security/tpm0/binary_bios_measurements`, so while Secure Boot is off, the log has the hash of every option ROM, also of those it would refuse with Secure Boot on. Secure Boot must be off: turned off with `--disable` or in the setup menu of the firmware, followed by a reboot.

It lists the hashes with the devices that hold them: the PCI address, the vendor and device IDs, their names from `pci.ids` when that is installed, and the driver of the kernel. A hash that db holds already is marked:
```
1. sha256 a9a45bbbc656421bef6b763b60fa330045ea3c3479da41a1e5f241db83f0eeae
     0000:03:00.0 [1002:7550] Advanced Micro Devices, Inc. [AMD/ATI] Navi 48 [Radeon RX 9070/9070 XT/9070 GRE] (driver amdgpu)
2. sha256 df10d1ffea217da3e95f836fa20526fbdcf72ce3ad6c1714e456b058912a53a0
     0000:11:00.0 [15b3:1015] Mellanox Technologies MT27710 Family [ConnectX-4 Lx] (driver mlx5_core)
     0000:11:00.1 [15b3:1015] Mellanox Technologies MT27710 Family [ConnectX-4 Lx] (driver mlx5_core)
Hashes to add to db (numbers, all, or nothing for none):
```

The answer is the numbers of the hashes to add, separated by spaces or commas, `all`, or nothing for none. The hashes that were picked are added to db in an update signed by the KEK key; what db held stays. The hash of an option ROM changes with an update of the firmware of the device, such as the VBIOS of a graphics card; until the new hash is added, the firmware refuses the driver with Secure Boot on.

### Writing the variables
The updates that remove PK, db and KEK, and the one with the hashes, are made in a temporary directory and removed again, so no file is left that could do the same a second time. Before each write, `hd-secboot` clears the immutable flag that the kernel puts on the variables in `/sys/firmware/efi/efivars`. When a write fails, the firmware may not let the system write its keys; enroll the `.auth` or `.cer` files from its setup menu instead.

## Tests
The tests are in `packages/hd-base/tests/test_secboot.py`: the state `hd-secboot --status` finds in EFI variables in a temporary directory, the EFI signature lists it reads and writes, the option ROMs it finds in a TPM event log, the names of their devices, and the hashes picked for `--append-rom-hash`. They need nothing beyond Python:
```
cd packages/hd-base
make test
```

`make test` in the root of this repository runs them with the tests of the other packages.

## Backup
Back up `/etc/hyprdebian/secboot` somewhere safe. Without the private keys, no new EFI file can be signed for a firmware that has the keys enrolled; with them, anyone can.

## Files
| Path | Purpose |
| --- | --- |
| `/etc/hyprdebian/secboot` | The keys. |
| `/usr/sbin/hd-secboot` | Generates the keys, writes them in the firmware and turns Secure Boot on and off. |
