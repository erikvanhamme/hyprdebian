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

The menu labels of the profiles are not in the UKI: update-bootloader.py puts
them in assets.efi, with a label suffix per profile from PROFILE_SUFFIXES.

Needs the packages systemd-ukify and systemd-boot-efi.

Example:
    mkuki.py /boot/vmlinuz-$(uname -r) /boot/initrd.img-$(uname -r) \\
        --output HyprDebian-Kernel-7.2.10.efi --no-splash --extra video=1920x1080
"""

import argparse
import subprocess
import sys
import tempfile
from pathlib import Path

DEFAULT_ROOT = "ZFS=rpool/hyprdebian"

# Profile 0. ukify puts the profile of the base UKI first, and the base holds
# the command line that the extra profiles replace.
DEFAULT_PROFILE = "ID=default\nTITLE=hyprdebian"

# Profiles 1 and up: (profile section text, extra command line arguments,
# suffix of the menu label).
EXTRA_PROFILES = [
    ("ID=recovery\nTITLE=ZFS recovery (BusyBox shell)", ["break=premount"], " (recovery)"),
]

# The suffix of the menu label of each profile, by profile number.
PROFILE_SUFFIXES = [""] + [suffix for _, _, suffix in EXTRA_PROFILES]


def base_cmdline(args):
    words = [f"root={args.root}", "ro"]
    if args.quiet:
        words.append("quiet")
    if args.splash:
        words.append("splash")
    words.extend(args.extra)
    return words


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
    parser.add_argument("--ukify", default="ukify", help="ukify command (default: ukify)")
    args = parser.parse_args()

    for path in (args.kernel, args.initrd):
        if not path.is_file():
            sys.exit(f"{path}: not a file")

    base = base_cmdline(args)

    with tempfile.TemporaryDirectory() as tmp:
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
            *(f"--join-profile={path}" for path in profile_files),
            f"--output={args.output}",
        )


if __name__ == "__main__":
    main()
