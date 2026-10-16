#!/usr/bin/env python3
"""Produces all the files the hyprdebian bootloader uses, in an EFI system partition.

Reads the settings from a JSON config file (bootloader.json next to this
script by default) and writes, below the directory given with --esp:

    EFI/BOOT/BOOTX64.EFI      the compiled bootloader (--bootloader) with its
                              assets added as PE sections:
                                .config   the bootloader config: the timeout
                                .backgnd  the wallpaper, scaled and cropped to
                                          res_x x res_y, with the work area
                                          (the menu box of menubox.py) over
                                          it; the bootloader takes its
                                          resolution from this image
                                .banner   banner_text in white
    EFI/Linux/HyprDebian-Kernel-x.y.z.efi
                              multi-profile UKIs (mkuki.py) of the 4 most
                              recent kernels in /boot, with the menu labels

With the assets inside BOOTX64.EFI, signing that one file for Secure Boot
covers them too: the firmware checks the signature over all sections before
it starts the bootloader, which then reads them from its own loaded image.
ukify adds the sections, so that they are loaded into memory, and an SBAT
entry for the bootloader.

The work area holds ten rows of text, so the banner and the menu labels in
the UKIs are rendered at a tenth of its height. Images are in the raw image
format of png2bin.py.

The most recent kernels are those whose /boot/vmlinuz-* was modified last.
The UKIs are written oldest first, so that on a FAT partition the newest
kernel also has the newest creation time, which is what the bootloader sorts
by; each UKI also gets the modification time of its kernel. Other
HyprDebian-Kernel-*.efi files in EFI/Linux are removed.

Config keys (all optional; defaults in DEFAULTS below):

    wallpaper       PNG or other image; a relative path is relative to the
                    config file
    res_x, res_y    display resolution of the bootloader: the size of the
                    background
    timeout         seconds before the default entry boots
    banner_text     text of the banner
    font            fontconfig name or file of the font for all texts
    color           colour of unselected menu labels
    selected_color  colour of the selected menu label and the menu box border
    border_width    width of the menu box border in pixels
    cmdline         root, quiet, splash and extra (a list) for the kernel
                    command lines, see mkuki.py

Needs python3-pil, systemd-ukify, and for the UKIs systemd-boot-efi.

Example:
    update-bootloader.py --esp /boot/efi --bootloader build/bootx64.efi
"""

import argparse
import io
import json
import os
import subprocess
import sys
import tempfile
from pathlib import Path

from PIL import Image

# text2png.py, png2bin.py, menubox.py and mkuki.py live next to this script.
SCRIPTS = Path(__file__).resolve().parent
sys.path.insert(0, str(SCRIPTS))

import menubox  # noqa: E402
import png2bin  # noqa: E402
import text2png  # noqa: E402

DEFAULTS = {
    "wallpaper": "../deploy/etc/skel/Pictures/wallpapers/wallpaper.png",
    "res_x": 1920,
    "res_y": 1080,
    "timeout": 5,
    "banner_text": "HyprDebian Bootloader",
    "font": "JetBrainsMono Nerd Font Mono",
    "color": "#0066cc",
    "selected_color": "#ff6600",
    "border_width": 5,
    "cmdline": {"root": "ZFS=rpool/hyprdebian", "quiet": True, "splash": True, "extra": []},
}

# The bootloader menu offers this many kernels (menu_ukis in bootloader.cpp).
KERNEL_COUNT = 4
# Rows of text that fit in the menu box.
ROWS = 10
BANNER_COLOR = "#ffffff"
UKI_PREFIX = "HyprDebian-Kernel-"

# The SBAT section of the bootloader: the standard header line, then our
# entry. Raise the generation of our entry (its second field) when a security
# fix must make older bootloaders revocable.
SBAT = (
    "sbat,1,SBAT Version,sbat,1,https://github.com/rhboot/shim/blob/main/SBAT.md\n"
    "hyprdebian-boot,1,HyprDebian,hyprdebian-boot,1,HyprDebian bootloader\n"
)

# Assets that earlier versions of this script wrote as loose files.
OLD_ASSETS = ["EFI/BOOT/hyprdebian.conf", "EFI/background.bin", "EFI/banner.bin", "EFI/BOOT/tux.bin"]


def load_config(path):
    with open(path) as f:
        config = json.load(f)
    unknown = set(config) - set(DEFAULTS)
    if unknown:
        sys.exit(f"{path}: unknown settings: {', '.join(sorted(unknown))}")
    cmdline = {**DEFAULTS["cmdline"], **config.get("cmdline", {})}
    config = {**DEFAULTS, **config, "cmdline": cmdline}

    wallpaper = Path(config["wallpaper"])
    if not wallpaper.is_absolute():
        wallpaper = Path(path).resolve().parent / wallpaper
    config["wallpaper"] = wallpaper
    return config


def to_bin(image):
    """Converts a Pillow image to the bootloader's raw image format with png2bin.py."""
    png = io.BytesIO()
    image.save(png, "PNG")
    return png2bin.convert(png.getvalue())


def scale_to_cover(image, width, height):
    """Scales the image to cover width x height and crops it to that, centred."""
    scale = max(width / image.width, height / image.height)
    scaled = image.resize((round(image.width * scale), round(image.height * scale)), Image.LANCZOS)
    left = (scaled.width - width) // 2
    top = (scaled.height - height) // 2
    return scaled.crop((left, top, left + width, top + height))


def find_kernels(boot_dir):
    """Returns (version, vmlinuz, initrd) of the KERNEL_COUNT most recent kernels, newest first.

    Kernels without an initrd are skipped. Of kernels with the same x.y.z,
    only the most recent is used, as they would get the same UKI name.
    """
    kernels = []
    for vmlinuz in boot_dir.glob("vmlinuz-*"):
        version = vmlinuz.name.removeprefix("vmlinuz-")
        initrd = boot_dir / f"initrd.img-{version}"
        if initrd.is_file():
            kernels.append((vmlinuz.stat().st_mtime, version, vmlinuz, initrd))
    kernels.sort(reverse=True)

    found = []
    names = set()
    for _, version, vmlinuz, initrd in kernels:
        name = uki_name(version)
        if name not in names:
            names.add(name)
            found.append((version, vmlinuz, initrd))
    return found[:KERNEL_COUNT]


def uki_name(version):
    """HyprDebian-Kernel-x.y.z.efi for a kernel version such as 7.2.10+deb14-amd64."""
    short = version.split("+")[0].split("-")[0]
    return f"{UKI_PREFIX}{short}.efi"


def build_ukis(config, esp, boot_dir, label_height):
    linux = esp / "EFI" / "Linux"
    linux.mkdir(parents=True, exist_ok=True)

    kernels = find_kernels(boot_dir)
    if not kernels:
        sys.exit(f"no kernels with an initrd found in {boot_dir}")

    cmdline = config["cmdline"]
    flags = ["--root", cmdline["root"], "--label-height", str(label_height), "--font", config["font"],
             "--color", config["color"], "--selected-color", config["selected_color"]]
    if not cmdline["quiet"]:
        flags.append("--no-quiet")
    if not cmdline["splash"]:
        flags.append("--no-splash")
    for extra in cmdline["extra"]:
        flags += ["--extra", extra]

    # Oldest first: see the creation times in the description above.
    keep = set()
    for version, vmlinuz, initrd in reversed(kernels):
        name = uki_name(version)
        keep.add(name)
        target = linux / name
        temporary = linux / f".{name}.tmp"
        try:
            subprocess.run(
                [sys.executable, SCRIPTS / "mkuki.py", vmlinuz, initrd, "--output", temporary,
                 "--name", name.removesuffix(".efi"), *flags],
                check=True,
            )
            os.replace(temporary, target)
        finally:
            temporary.unlink(missing_ok=True)
        mtime = vmlinuz.stat().st_mtime
        os.utime(target, (mtime, mtime))
        print(f"Wrote {target}", flush=True)

    for old in linux.glob(f"{UKI_PREFIX}*.efi"):
        if old.name not in keep:
            old.unlink()
            print(f"Removed {old}", flush=True)


def build_bootloader(bootloader, esp, sections):
    """Writes EFI/BOOT/BOOTX64.EFI: the bootloader with the given {name: data} sections added."""
    boot = esp / "EFI" / "BOOT"
    boot.mkdir(parents=True, exist_ok=True)
    target = boot / "BOOTX64.EFI"
    temporary = boot / ".BOOTX64.EFI.tmp"
    with tempfile.TemporaryDirectory() as tmp:
        options = []
        for name, data in sections.items():
            path = Path(tmp) / name.lstrip(".")
            path.write_bytes(data)
            options.append(f"--section={name}:@{path}")
        try:
            subprocess.run(
                ["ukify", "build", f"--stub={bootloader}", *options, f"--sbat={SBAT}", f"--output={temporary}"],
                check=True,
            )
            os.replace(temporary, target)
        finally:
            temporary.unlink(missing_ok=True)
    print(f"Wrote {target}", flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--esp", type=Path, required=True, help="root of the EFI system partition to write to")
    parser.add_argument(
        "--config",
        type=Path,
        default=SCRIPTS / "bootloader.json",
        help="JSON config file (default: bootloader.json next to this script)",
    )
    parser.add_argument(
        "--bootloader",
        type=Path,
        default=SCRIPTS / "build" / "bootx64.efi",
        help="the compiled bootloader (default: build/bootx64.efi next to this script)",
    )
    parser.add_argument("--boot-dir", type=Path, default=Path("/boot"), help="where the kernels are (default: /boot)")
    args = parser.parse_args()

    if not args.bootloader.is_file():
        sys.exit(f"{args.bootloader}: not a file; build the bootloader first")

    config = load_config(args.config)
    res_x, res_y = config["res_x"], config["res_y"]
    border_width = config["border_width"]
    config_text = f"timeout {config['timeout']}\n".encode()

    # 1. The background: the wallpaper with the menu box.
    with Image.open(config["wallpaper"]) as wallpaper:
        background = scale_to_cover(wallpaper.convert("RGB"), res_x, res_y)
    background = menubox.draw_box(background, config["selected_color"], border_width).convert("RGB")

    # 2. A tenth of the height of the work area is the height of the texts.
    _, _, _, box_height = menubox.box_geometry(res_x, res_y)
    text_height = box_height // ROWS

    # 3. The banner.
    banner = text2png.render(config["banner_text"], BANNER_COLOR, config["font"], text_height)

    # 4. The UKIs, with their menu labels.
    build_ukis(config, args.esp, args.boot_dir, text_height)

    # 5. The bootloader with the config, the background and the banner.
    build_bootloader(
        args.bootloader,
        args.esp,
        {".config": config_text, ".backgnd": to_bin(background), ".banner": to_bin(banner)},
    )

    for old in OLD_ASSETS:
        path = args.esp / old
        if path.exists():
            path.unlink()
            print(f"Removed {path}", flush=True)


if __name__ == "__main__":
    main()
