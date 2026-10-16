#!/usr/bin/env python3
"""Builds a multi-profile unified kernel image (UKI) for hyprdebian with ukify.

The UKI holds the kernel, the initrd and two profiles, matching the grub
entries that hyprdebian installs:

    profile 0  the default boot:
               root=ZFS=rpool/hyprdebian ro quiet splash
    profile 1  the ZFS recovery entry, which stops in the BusyBox shell of
               the initramfs before the root file system is mounted:
               root=ZFS=rpool/hyprdebian ro quiet splash break=premount

The systemd stub boots profile 0 unless the bootloader passes "@1" as the
first word of the command line.

For the bootloader menu, the UKI also holds a pre-rendered label per profile,
in the unselected and the selected colour:

    .label0   .label0s   profile 0: the name of the UKI file without .efi,
                         or --name
    .label1   .label1s   profile 1: the same, followed by " (recovery)"

Each label is rendered by text2png.py and converted by png2bin.py into the
raw image format that the bootloader blits. The font and colours default to
those of the mako config that hyprdebian installs. update-bootloader.py
passes the label height that fits its menu layout.

Needs the packages systemd-ukify, systemd-boot-efi and python3-pil, and the
font (fc-match must find it).

Example:
    mkuki.py /boot/vmlinuz-$(uname -r) /boot/initrd.img-$(uname -r) \\
        --output HyprDebian-Kernel-7.2.10.efi --no-splash --extra video=1920x1080
"""

import argparse
import subprocess
import sys
import tempfile
from pathlib import Path

# text2png.py and png2bin.py live next to this script.
SCRIPTS = Path(__file__).resolve().parent

DEFAULT_ROOT = "ZFS=rpool/hyprdebian"

# From deploy/etc/skel/.config/mako/config: font, text-color (unselected) and
# border-color (selected).
DEFAULT_FONT = "JetBrainsMono Nerd Font Mono"
DEFAULT_COLOR = "#0066cc"
DEFAULT_SELECTED_COLOR = "#ff6600"
DEFAULT_LABEL_HEIGHT = 86

# Profile 0. ukify puts the profile of the base UKI first, and the base holds
# the command line that the extra profiles replace.
DEFAULT_PROFILE = "ID=default\nTITLE=hyprdebian"

# Profiles 1 and up: (profile section text, extra command line arguments,
# suffix of the menu label).
EXTRA_PROFILES = [
    ("ID=recovery\nTITLE=ZFS recovery (BusyBox shell)", ["break=premount"], " (recovery)"),
]


def base_cmdline(args):
    words = [f"root={args.root}", "ro"]
    if args.quiet:
        words.append("quiet")
    if args.splash:
        words.append("splash")
    words.extend(args.extra)
    return words


def render_label(text, color, font, height, output):
    """Renders text into output in the bootloader's raw image format."""
    png = output.with_suffix(".png")
    subprocess.run(
        [sys.executable, SCRIPTS / "text2png.py", "--text", text, "--color", color, "--font", font,
         "--height", str(height), "--output", png],
        check=True,
    )
    subprocess.run([sys.executable, SCRIPTS / "png2bin.py", png, output], check=True)


def ukify(args, *options):
    command = [args.ukify, "build", *options]
    print(" ".join(command), flush=True)
    subprocess.run(command, check=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("kernel", type=Path, help="kernel image, e.g. /boot/vmlinuz-VERSION")
    parser.add_argument("initrd", type=Path, help="initrd, e.g. /boot/initrd.img-VERSION")
    parser.add_argument("-o", "--output", type=Path, required=True, help="UKI to write")
    parser.add_argument("--root", default=DEFAULT_ROOT, help=f"value of root= (default: {DEFAULT_ROOT})")
    parser.add_argument("--no-quiet", dest="quiet", action="store_false", help="leave quiet off the command lines")
    parser.add_argument("--no-splash", dest="splash", action="store_false", help="leave splash off the command lines")
    parser.add_argument(
        "--extra",
        action="append",
        default=[],
        metavar="ARG",
        help="extra kernel argument for all profiles; can be given more than once",
    )
    parser.add_argument(
        "--label-height",
        type=int,
        default=DEFAULT_LABEL_HEIGHT,
        help=f"height of the menu labels in pixels (default: {DEFAULT_LABEL_HEIGHT})",
    )
    parser.add_argument("--font", default=DEFAULT_FONT, help=f"font of the labels (default: {DEFAULT_FONT})")
    parser.add_argument("--color", default=DEFAULT_COLOR, help=f"unselected label colour (default: {DEFAULT_COLOR})")
    parser.add_argument(
        "--selected-color",
        default=DEFAULT_SELECTED_COLOR,
        help=f"selected label colour (default: {DEFAULT_SELECTED_COLOR})",
    )
    parser.add_argument("--name", help="name in the menu labels (default: the output file name without .efi)")
    parser.add_argument("--ukify", default="ukify", help="ukify command (default: ukify)")
    args = parser.parse_args()

    for path in (args.kernel, args.initrd):
        if not path.is_file():
            sys.exit(f"{path}: not a file")

    base = base_cmdline(args)
    name = args.name or args.output.name.removesuffix(".efi")
    label_texts = [name] + [name + suffix for _, _, suffix in EXTRA_PROFILES]

    with tempfile.TemporaryDirectory() as tmp:
        label_sections = []
        for number, text in enumerate(label_texts):
            for section, color in ((f".label{number}", args.color), (f".label{number}s", args.selected_color)):
                label_file = Path(tmp) / f"{section[1:]}.bin"
                render_label(text, color, args.font, args.label_height, label_file)
                label_sections.append(f"--section={section}:@{label_file}")

        profile_files = []
        for number, (profile, extra, _) in enumerate(EXTRA_PROFILES, start=1):
            profile_file = Path(tmp) / f"profile{number}.efi"
            ukify(
                args,
                f"--profile={profile}",
                f"--cmdline={' '.join(base + extra)}",
                f"--output={profile_file}",
            )
            profile_files.append(profile_file)

        ukify(
            args,
            f"--linux={args.kernel}",
            f"--initrd={args.initrd}",
            f"--profile={DEFAULT_PROFILE}",
            f"--cmdline={' '.join(base)}",
            *label_sections,
            *(f"--join-profile={path}" for path in profile_files),
            f"--output={args.output}",
        )


if __name__ == "__main__":
    main()
