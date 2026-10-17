## Introduction
The hyprdebian bootloader is a small UEFI bootloader that shows a menu of the unified kernel images (UKIs) of the four most recent kernels, each in a normal and a ZFS recovery profile, and boots the chosen one. It comes in two packages, of which only one can be installed:

| Package | Frontend |
| --- | --- |
| `hd-bootloader-gfx` | The graphical frontend: the menu on the wallpaper of `hd-art`, in the JetBrains Mono Nerd Font of `hd-fonts`. |
| `hd-bootloader-txt` | The text frontend: the menu on the text console of the firmware. |

`hd-bootloader-update` builds the UKIs and writes them, the bootloader and `assets.efi` to an EFI system partition (ESP). `assets.efi` holds the settings, the menu and the DKMS MOK, which the bootloader hands to the kernel so that it loads the DKMS-built ZFS module under Secure Boot.

`/usr/lib/hd-bootloader/update-esps` runs `hd-bootloader-update` for `/boot/efi` and, when it exists, `/boot/efi2`. The kernel and initramfs hooks of the package run it when a kernel or an initrd is installed or removed. All ESPs are checked before any is written; a missing one makes the hook, and with it the kernel package or apt, fail: a system with a missing drive should not be upgraded.

The settings are the key `hd-bootloader` in the hyprdebian configuration, `/etc/hyprdebian/config.json` (see [hd-config](hd-config.md)). Both packages depend on `hd-config`.

The packages come with two man pages: `man hd-bootloader` describes the bootloader and its settings, `man hd-bootloader-update` the command that writes it to an ESP.

## Building the packages
The packages are built from `hd-bootloader` in this repository:
```
cd hd-bootloader
make deb
```

This builds `build/hd-bootloader-gfx_<version>_amd64.deb` and `build/hd-bootloader-txt_<version>_amd64.deb`, with the version from `version.json`. Raise the version there for every package that is handed out. Building needs `clang`, `lld`, `gnu-efi` and `systemd-ukify`.

| Package | Depends on |
| --- | --- |
| `hd-bootloader-gfx` | `python3`, `python3-pil`, `systemd-ukify`, `systemd-boot-efi`, `fontconfig`, `hd-config`, `hd-art`, `hd-fonts` |
| `hd-bootloader-txt` | `python3`, `systemd-ukify`, `systemd-boot-efi`, `hd-config` |

Both recommend `dkms`, `openssl` and `sbsigntool`, and suggest `hd-secboot`. Install one of them with:
```
sudo apt install ./build/hd-bootloader-gfx_<version>_amd64.deb
```

For development, `make esp` writes the bootloader and the UKIs of the kernels in `/boot` to `build/esp/` with the defaults of `deb/gfx/defaults.json`, and `make image` makes `build/esp.img`, a FAT32 image to boot in QEMU. `make esp ESP_PACKAGE=txt` uses the text frontend.

The files involved are:
```
hd-bootloader/Makefile
hd-bootloader/version.json
hd-bootloader/bootloader.cpp
hd-bootloader/bootloader.hpp
hd-bootloader/graphical.cpp
hd-bootloader/text.cpp
hd-bootloader/assets.cpp
hd-bootloader/update-bootloader.py
hd-bootloader/mkuki.py
hd-bootloader/graphical.py
hd-bootloader/text2png.py
hd-bootloader/png2bin.py
hd-bootloader/menubox.py
hd-bootloader/text.py
hd-bootloader/update-esps
hd-bootloader/deb/postinst
hd-bootloader/deb/gfx/control
hd-bootloader/deb/gfx/defaults.json
hd-bootloader/deb/txt/control
hd-bootloader/deb/txt/defaults.json
hd-bootloader/deb/etc/kernel/postinst.d/zz-hd-bootloader
hd-bootloader/deb/etc/kernel/postrm.d/zz-hd-bootloader
hd-bootloader/deb/etc/initramfs/post-update.d/zz-hd-bootloader
hd-bootloader/man/hd-bootloader.7
hd-bootloader/man/hd-bootloader-update.8
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
        "wallpaper": "/usr/share/backgrounds/hyprdebian/emerald-1920x1080.png",
        "res_x": 1920,
        "res_y": 1080,
        "timeout": 5,
        "banner_text": "HyprDebian Bootloader",
        "font": "JetBrainsMono Nerd Font Mono",
        "color": "#0066cc",
        "selected_color": "#ff6600",
        "border_width": 5,
        "work_area_size": 80,
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

All fields are optional; a missing field gets its default. A field of the other frontend is an error.

| Field | Frontend | Default | Meaning |
| --- | --- | --- | --- |
| `frontend` | both | `graphical` | `graphical` or `text`, the frontend of the installed package. |
| `timeout` | both | `5` | Seconds before the default entry boots. |
| `banner_text` | both | `HyprDebian Bootloader` | The text above the menu. |
| `cmdline` | both | see above | The kernel command line of the UKIs: `root` (the value of `root=`), `quiet`, `splash`, and `extra`, a list of more arguments. |
| `wallpaper` | graphical | `/usr/share/backgrounds/hyprdebian/emerald-1920x1080.png` | The wallpaper; a relative path is relative to `/etc/hyprdebian`. |
| `res_x`, `res_y` | graphical | `1920`, `1080` | The display resolution of the bootloader. |
| `font` | graphical | `JetBrainsMono Nerd Font Mono` | The fontconfig name or file of the font. |
| `color` | graphical | `#0066cc` | The colour of the menu labels. |
| `selected_color` | graphical | `#ff6600` | The colour of the selected label and of the border of the menu box. |
| `border_width` | graphical | `5` | The width of the border of the menu box in pixels. |
| `work_area_size` | graphical | `80` | The width and height of the menu box in percent of the screen; the texts scale with it. |
| `x_center`, `y_center` | graphical | `50`, `50` | The centre of the menu box in percent of the screen. |

To change the settings, write the whole key with `hd-config-update`, then update the ESPs:
```
sudo hd-config-update hd-bootloader < bootloader.json
sudo /usr/lib/hd-bootloader/update-esps
```

## hd-bootloader-update
```
hd-bootloader-update [options]
```

`hd-bootloader-update` reads the settings and writes, below the ESP given with `--esp` (`/boot/efi` by default):

| File | Content |
| --- | --- |
| `EFI/BOOT/BOOTX64.EFI` | The bootloader of the frontend of the installed package. |
| `EFI/BOOT/assets.efi` | The settings, the menu rows, the DKMS MOK and the assets of the frontend, as sections of a PE image that the bootloader loads with LoadImage, so the firmware checks its signature under Secure Boot. |
| `EFI/Linux/HyprDebian-Kernel-x.y.z.efi` | Multi-profile UKIs of the four most recent kernels in `/boot`, with a normal and a ZFS recovery profile. |

The files are written in an order that leaves a bootable ESP when it is interrupted: first the UKIs, then `assets.efi` and `BOOTX64.EFI`, and only then are the UKIs of older kernels removed. A file whose contents would not change is not rewritten.

| Option | Meaning |
| --- | --- |
| `--esp <dir>` | The root of the ESP to write to; `/boot/efi` by default. |
| `--config <file>` | Read the settings from the JSON object in the file instead of the key `hd-bootloader`. |
| `--boot-dir <dir>` | Where the kernels are; `/boot` by default. |
| `--mok <file>` | The certificate of the DKMS MOK, DER or PEM; `/var/lib/dkms/mok.pub` by default. It is needed to boot with the ZFS module. |
| `--db-key <file>`, `--db-cert <file>` | The Secure Boot db key and its PEM certificate; `/etc/hyprdebian/secboot/db.key` and `db.crt` by default. |
| `--bootloader-dir <dir>`, `--assets-stub <file>` | Where the compiled bootloaders and the assets stub are; `/usr/lib/hd-bootloader` by default. |

Run it as root.

## Secure Boot
The packages do not depend on `hd-secboot` (see [hd-secboot](hd-secboot.md)). When its keys are in `/etc/hyprdebian/secboot`, `hd-bootloader-update` signs the bootloader, `assets.efi` and the UKIs with the db key, using `sbsign`; a file that would not change is signed again when it does not verify with the db certificate, as after the keys were generated. Without the keys, the files are written unsigned, with a message saying so. The message of each file written says whether it is signed or unsigned.

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
