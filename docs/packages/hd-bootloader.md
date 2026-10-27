## Introduction
The hyprdebian bootloader is a small UEFI bootloader that shows a menu of the unified kernel images (UKIs) of the four most recent kernels, each in a normal and a ZFS recovery profile, and boots the chosen one. It comes in two packages, of which only one can be installed:

| Package | Frontend |
| --- | --- |
| `hd-bootloader-gfx` | The graphical frontend: the menu on the background of the theme of the system of `hd-desktop`, in its colours and font, as [hd-menu](hd-menu.md) shows a menu. |
| `hd-bootloader-txt` | The text frontend: the menu on the text console of the firmware. |

`hd-bootloader-update` builds the UKIs and writes them, the bootloader and `assets.efi` to an EFI system partition (ESP). `assets.efi` holds the settings, the menu and the DKMS MOK, which the bootloader hands to the kernel so that it loads the DKMS-built ZFS module under Secure Boot. A system with only the kernels of [hd-kernel](hd-kernel.md) has no DKMS MOK: their ZFS module is signed with the key the kernel was built with, and `assets.efi` then has no MOK.

`/usr/lib/hd-bootloader/update-esps` runs `hd-bootloader-update` for `/boot/efi` and, when it exists, `/boot/efi2`. The kernel and initramfs hooks of the package run it when a kernel or an initrd is installed or removed. All ESPs are checked before any is written; a missing one makes the hook, and with it the kernel package or apt, fail: a system with a missing drive should not be upgraded.

The settings are the key `hd-bootloader` in the hyprdebian configuration, `/etc/hyprdebian/config.json` (see [hd-config](hd-config.md)). Both packages depend on `hd-base`.

The packages come with two man pages: `man hd-bootloader` describes the bootloader and its settings, `man hd-bootloader-update` the command that writes it to an ESP.

## Building the packages
The packages are built from `packages/hd-bootloader` in this repository:
```
cd packages/hd-bootloader
make deb
```

This builds `build/hd-bootloader-gfx_<version>_amd64.deb` and `build/hd-bootloader-txt_<version>_amd64.deb`, with the version from `version.json`. Raise the version there for every package that is handed out. Building needs `clang`, `lld`, `gnu-efi` and `systemd-ukify`.

| Package | Depends on |
| --- | --- |
| `hd-bootloader-gfx` | `python3`, `python3-pil`, `systemd-ukify`, `systemd-boot-efi`, `fontconfig`, `hd-base`, `hd-desktop` |
| `hd-bootloader-txt` | `python3`, `systemd-ukify`, `systemd-boot-efi`, `hd-base` |

Both recommend `dkms`. Install one of them with:
```
sudo apt install ./build/hd-bootloader-gfx_<version>_amd64.deb
```

For development, `make esp` writes the bootloader and the UKIs of the kernels in `/boot` to `build/esp/` with the defaults of `deb/gfx/defaults.json`, and `make image` makes `build/esp.img`, a FAT32 image to boot in QEMU. `make esp ESP_PACKAGE=txt` uses the text frontend.

The files involved are:
```
packages/hd-bootloader/Makefile
packages/hd-bootloader/version.json
packages/hd-bootloader/bootloader.cpp
packages/hd-bootloader/bootloader.hpp
packages/hd-bootloader/graphical.cpp
packages/hd-bootloader/text.cpp
packages/hd-bootloader/assets.cpp
packages/hd-bootloader/update-bootloader.py
packages/hd-bootloader/mkuki.py
packages/hd-bootloader/graphical.py
packages/hd-bootloader/text2png.py
packages/hd-bootloader/png2bin.py
packages/hd-bootloader/menubox.py
packages/hd-bootloader/text.py
packages/hd-bootloader/update-esps
packages/hd-bootloader/deb/postinst
packages/hd-bootloader/deb/gfx/control
packages/hd-bootloader/deb/gfx/defaults.json
packages/hd-bootloader/deb/txt/control
packages/hd-bootloader/deb/txt/defaults.json
packages/hd-bootloader/deb/etc/kernel/postinst.d/zz-hd-bootloader
packages/hd-bootloader/deb/etc/kernel/postrm.d/zz-hd-bootloader
packages/hd-bootloader/deb/etc/initramfs/post-update.d/zz-hd-bootloader
packages/hd-bootloader/man/hd-bootloader.7
packages/hd-bootloader/man/hd-bootloader-update.8
```

## Installation
The postinst stores the default settings of the package, `/usr/share/hd-bootloader/defaults.json`, in the key `hd-bootloader` with `hd-config-update`:
- when there are no settings yet, the defaults of the package;
- when the settings are those of the other frontend, as when switching from `hd-bootloader-gfx` to `hd-bootloader-txt`, the defaults of the package, keeping `timeout`, `banner_text` and `cmdline`;
- otherwise, the settings are left as they are.

The postinst does not write the ESPs. That happens with the next kernel or initrd, or by running:
```
sudo /usr/lib/hd-bootloader/update-esps
```

## Settings
The key `hd-bootloader` holds a JSON object, as `hd-bootloader-gfx` stores it:
```
{
    "hd-bootloader": {
        "frontend": "graphical",
        "res_x": 1920,
        "res_y": 1080,
        "timeout": 5,
        "banner_text": "HyprDebian Bootloader",
        "work_area_size": 50,
        "x_center": 50,
        "y_center": 50,
        "cmdline": {
            "root": "ZFS=rpool/hyprdebian",
            "quiet": true,
            "splash": true,
            "extra": []
        }
    }
}
```

All fields are optional; a missing field gets its default. A field of the other frontend is an error. The fields `wallpaper`, `font`, `color`, `selected_color` and `border_width` of versions before 0.10 are ignored with a warning; the upgrade removes them, as the graphical frontend now takes its look from the theme, see [The look](#the-look).

| Field | Frontend | Default | Meaning |
| --- | --- | --- | --- |
| `frontend` | both | `graphical` | `graphical` or `text`, the frontend of the installed package. |
| `timeout` | both | `5` | Seconds before the default entry boots. |
| `banner_text` | both | `HyprDebian Bootloader` | The text above the menu. |
| `cmdline` | both | see above | The kernel command line of the UKIs: `root` (the value of `root=`), `quiet`, `splash`, and `extra`, a list of more arguments. With `splash`, the UKIs also get `vt.global_cursor_default=0`, which hides the cursor of the text console, so it does not blink before the splash shows; the recovery entry shows it again for its shell, and `vt.global_cursor_default=` in `extra` comes after it and wins. The graphical frontend adds `video=<res_x>x<res_y>` before those of `extra`, which can override it. |
| `res_x`, `res_y` | graphical | `1920`, `1080` | The display resolution of the bootloader, also passed to the kernel as `video=<res_x>x<res_y>`, so that the plymouth boot splash and the desktop start with the same resolution. |
| `work_area_size` | graphical | `50` | The width and height of the menu box in percent of the screen; the texts scale with it. |
| `x_center`, `y_center` | graphical | `50`, `50` | The centre of the menu box in percent of the screen. |

To change the settings, write the whole key with `hd-config-update`, then update the ESPs:
```
sudo hd-config-update hd-bootloader < bootloader.json
sudo /usr/lib/hd-bootloader/update-esps
```

## The look
The graphical frontend shows the menu as [hd-menu](hd-menu.md) does, in the theme of the system: the key `hd-theme` of `/etc/hyprdebian/config.json`, which `sudo hd-theme --select <name>` sets, see [hd-theme](hd-theme.md). The themes of the users are not used. A member that is missing or not valid takes the value of the theme default, which is also used without a theme.

| Member | Use |
| --- | --- |
| `background` | The background: of the images of the family in `/usr/share/backgrounds/hyprdebian`, the smallest that covers `res_x` x `res_y`, else the largest, scaled and cropped to it. Without one, the screen is `dark`. |
| `dark`, `opacity` | The menu box, `dark` at `opacity`. |
| `highlight` | The border of the menu box. |
| `border`, `radius` | The width of that border and the radius of its corners; the box and the bar of the selected label have the inner radius, `radius` minus `border`. |
| `light` | The banner and the labels. |
| `normal` | The divider under the banner, and the bar behind the selected label. |
| `font` | The font of the texts. |

The sizes of the theme are pixels at a height of 1080: they are scaled with `res_y`, so the menu looks the same at every resolution. The texts take 80% of the height of a row, a tenth of the menu box.

The menu is in `assets.efi`, so a new theme reaches the bootloader when the ESPs are written again. `sudo hd-theme --select` does that, unless it is given `--no-initramfs`; otherwise the next kernel or initrd does, or run `sudo /usr/lib/hd-bootloader/update-esps`.

## hd-bootloader-update
```
hd-bootloader-update [options]
```

`hd-bootloader-update` reads the settings and writes, below the ESP given with `--esp` (`/boot/efi` by default):

| File | Content |
| --- | --- |
| `EFI/BOOT/BOOTX64.EFI` | The bootloader of the frontend of the installed package. |
| `EFI/BOOT/assets.efi` | The settings, the menu rows, the DKMS MOK when there is one and the assets of the frontend, as sections of a PE image that the bootloader loads with LoadImage, so the firmware checks its signature under Secure Boot. |
| `EFI/Linux/HyprDebian-Kernel-x.y.z.efi` | Multi-profile UKIs of the four most recent kernels in `/boot` with a ZFS module, with a normal and a ZFS recovery profile; `HyprDebian-Kernel-x.y.z-hd.efi` for a kernel of [hd-kernel](hd-kernel.md). A kernel without a ZFS module in `/lib/modules/<version>`, built in or as `zfs.ko` in any directory below it, cannot mount the root and gets no UKI. |

The files are written in an order that leaves a bootable ESP when it is interrupted: first the UKIs, then `assets.efi` and `BOOTX64.EFI`, and only then are the UKIs of older kernels removed. A file whose contents would not change is not rewritten.

| Option | Meaning |
| --- | --- |
| `--esp <dir>` | The root of the ESP to write to; `/boot/efi` by default. |
| `--config <file>` | Read the settings from the JSON object in the file instead of the key `hd-bootloader`. |
| `--boot-dir <dir>` | Where the kernels are; `/boot` by default. |
| `--modules-dir <dir>` | Where the modules of the kernels are, to find their ZFS module; `/lib/modules` by default. |
| `--mok <file>` | The certificate of the DKMS MOK, DER or PEM; `/var/lib/dkms/mok.pub` by default. The kernel of Debian needs it to load the ZFS module that DKMS builds under Secure Boot. Without the file, `assets.efi` gets no MOK. |
| `--db-key <file>`, `--db-cert <file>` | The Secure Boot db key and its PEM certificate; `/etc/hyprdebian/secboot/db.key` and `db.crt` by default. |
| `--bootloader-dir <dir>`, `--assets-stub <file>` | Where the compiled bootloaders and the assets stub are; `/usr/lib/hd-bootloader` by default. |

Run it as root.

## Secure Boot
`hd-base` generates the Secure Boot keys of the owner in `/etc/hyprdebian/secboot` (see [hd-secboot](hd-secboot.md)). When the keys are there, `hd-bootloader-update` signs the bootloader, `assets.efi` and the UKIs with the db key, using `sbsign`; a file that would not change is signed again when it does not verify with the db certificate, as after the keys were generated. Without the keys, the files are written unsigned, with a message saying so. The message of each file written says whether it is signed or unsigned.

## Files
| Path | Purpose |
| --- | --- |
| `/etc/hyprdebian/config.json` | The configuration, with the settings under the key `hd-bootloader`. |
| `/usr/share/hd-bootloader/defaults.json` | The default settings of the installed package. |
| `/usr/lib/hd-bootloader` | The bootloader, the assets stub and the scripts. |
| `/usr/lib/hd-bootloader/update-esps` | Updates every ESP. |
| `/usr/sbin/hd-bootloader-update` | A link to `/usr/lib/hd-bootloader/update-bootloader.py`. |
| `/etc/kernel/postinst.d/zz-hd-bootloader`, `/etc/kernel/postrm.d/zz-hd-bootloader`, `/etc/initramfs/post-update.d/zz-hd-bootloader` | The hooks that run `update-esps` when a kernel or initrd changes, conffiles. |
| `/var/lib/dkms/mok.pub` | The DKMS MOK. |
| `/etc/hyprdebian/secboot/db.key`, `/etc/hyprdebian/secboot/db.crt` | The Secure Boot db key and its certificate, when they are there. |
