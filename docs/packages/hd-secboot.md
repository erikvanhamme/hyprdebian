## Introduction
The package `hd-secboot` lets a hyprdebian system boot under Secure Boot with keys of its owner, instead of, or next to, the keys of Microsoft that most firmware ships with. `hd-secboot-enroll` enrolls the keys in the firmware.

It depends on the tools that make and use the keys, so they are installed with it and stay installed:
- `openssl` makes the keys and their certificates;
- `efitools` turns the certificates into EFI signature lists and signed updates, and enrolls them with `efi-updatevar`;
- `sbsigntool` signs EFI files with `sbsign` and checks them with `sbverify`.

On its first installation, it generates the keys in `/etc/hyprdebian/secboot`. When that directory already exists, on an upgrade, a reinstall or when the keys were put there by hand, the keys are kept: keys that are enrolled in the firmware are never replaced. Removing or purging the package keeps the keys as well.

The package comes with two man pages: `man hd-secboot` describes the keys, `man hd-secboot-enroll` the command that enrolls them.

## Building the package
The package is built from `packages/hd-secboot` in this repository:
```
cd packages/hd-secboot
make deb
```

This builds `build/hd-secboot_<version>_all.deb`, with the version from `version.json`. Raise the version there for every package that is handed out. It depends on `python3`, `openssl`, `efitools` and `sbsigntool`. Install it with:
```
sudo apt install ./build/hd-secboot_<version>_all.deb
```

The files involved are:
```
packages/hd-secboot/Makefile
packages/hd-secboot/version.json
packages/hd-secboot/hd-secboot-enroll
packages/hd-secboot/deb/control
packages/hd-secboot/deb/postinst
packages/hd-secboot/man/hd-secboot.7
packages/hd-secboot/man/hd-secboot-enroll.8
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

The directory and the files are only readable by root. The postinst generates the keys in `/etc/hyprdebian/secboot.tmp` and moves them into place when they are all there, so a failed installation never leaves half a set behind; the next attempt starts over.

## Enrolling the keys
Put the firmware in setup mode in its setup menu. Most firmware has an entry under Secure Boot to delete the platform key, to clear all keys, or to reset to setup mode. Then enroll the keys:
```
sudo hd-secboot-enroll --dry-run
sudo hd-secboot-enroll
```

Sign the EFI files that are to boot with the db key, then turn Secure Boot on in the setup menu:
```
cd /etc/hyprdebian/secboot
sudo sbsign --key db.key --cert db.crt --output signed.efi unsigned.efi
```

Until Secure Boot is turned on, the system boots as before.

## hd-secboot-enroll
```
hd-secboot-enroll [--replace] [-n]
```

`hd-secboot-enroll` enrolls the keys in `/etc/hyprdebian/secboot` in the firmware. Run it as root. It goes through these steps:
1. It checks that `db.auth`, `KEK.auth` and `PK.auth` exist, that the system was booted with UEFI, and that the firmware is in setup mode. Otherwise it stops before it changes anything.
2. It writes db, then KEK, with `efi-updatevar`. By default it adds the keys to the entries the firmware already has in those variables, such as the certificates of Microsoft, which some graphics cards and other devices need to load their firmware. With `--replace`, only the hyprdebian keys are left; only use it when every device in the system boots without the certificates of Microsoft.
3. It writes PK last, since setting the platform key ends setup mode. There is only one platform key, so PK is always replaced.
4. It checks that the firmware has left setup mode.

Before each write, it clears the immutable flag that the kernel puts on the variables in `/sys/firmware/efi/efivars`. When a write fails, the firmware may not let the system write its keys; enroll the `.auth` or `.cer` files from its setup menu instead.

With `-n` or `--dry-run`, it runs the checks of step 1 and shows the commands it would run, and changes nothing. It exits with 0 when the keys were enrolled, 1 on an error and 2 on wrong arguments.

## Backup
Back up `/etc/hyprdebian/secboot` somewhere safe. Without the private keys, no new EFI file can be signed for a firmware that has the keys enrolled; with them, anyone can.

## Files
| Path | Purpose |
| --- | --- |
| `/etc/hyprdebian/secboot` | The keys. |
| `/usr/sbin/hd-secboot-enroll` | Enrolls the keys in the firmware. |
