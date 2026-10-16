#!/usr/bin/env python3
"""Produces all the files the hyprdebian bootloader uses, in an EFI system partition.

Reads the settings from a JSON config file
(/etc/hyprdebian/bootloader/bootloader.json by default) and writes, below the
directory given with --esp:

    EFI/BOOT/BOOTX64.EFI      the compiled bootloader of the frontend,
                              bootx64-<frontend>.efi in --bootloader-dir,
                              with an SBAT section; rewritten only when it
                              changes
    EFI/BOOT/assets.efi       the assets stub (--assets-stub) with the assets
                              added as PE sections:
                                .config   the bootloader config: the timeout
                                .slotN    for menu row N (1 to 8, top to
                                          bottom): the UKI file name in
                                          EFI/Linux and the profile to boot,
                                          such as
                                          "HyprDebian-Kernel-7.2.10.efi 1"
                                .mok      the DKMS MOK (--mok) as an
                                          EFI_SIGNATURE_LIST, which the
                                          bootloader hands to the kernel
                              and the sections of the frontend, which shows
                              the menu, built by <frontend>.py:
                                graphical the background, the banner and
                                          the label images of the rows; see
                                          graphical.py
    EFI/Linux/HyprDebian-Kernel-x.y.z.efi
                              multi-profile UKIs (mkuki.py) of the 4 most
                              recent kernels in /boot

The menu has a row per profile of each of those kernels, newest kernel
first. With the labels and the file names in assets.efi, Secure Boot covers
everything the menu shows and which UKI each row boots; the UKI itself is
checked when the bootloader starts it.

The ZFS module is built by DKMS and signed with the DKMS MOK, the key pair in
/var/lib/dkms/mok.key and mok.pub. Under Secure Boot the kernel loads it only
if it trusts that key. With shim, shim hands its MOKs to the kernel; here the
bootloader does that with the MOK in assets.efi, so Secure Boot covers it
too. Without a usable assets.efi the bootloader shows an error and does not
boot.

With the assets in a file of their own, a change of the config, the
wallpaper or the texts leaves BOOTX64.EFI alone; a change of the frontend
replaces it. assets.efi is a PE image
that is never started: the bootloader loads it with LoadImage, so that with
Secure Boot on the firmware checks its signature as it does for BOOTX64.EFI,
and reads the sections from the loaded image. For Secure Boot, sign
assets.efi after every run that rewrites it, and BOOTX64.EFI after every run
that rewrites that. As assets.efi names the UKIs, it changes with every new
kernel. ukify adds the sections, so that they are loaded into memory, and an
SBAT entry to each file. A file whose contents would not change is not
rewritten.

The files are written in an order that leaves a bootable ESP if the script
is interrupted: first the UKIs, then assets.efi and BOOTX64.EFI, and only
then are the UKIs of older kernels removed, so assets.efi never names a UKI
that is gone.

The most recent kernels are those whose /boot/vmlinuz-* was modified last.
Other HyprDebian-Kernel-*.efi files in EFI/Linux are removed.

Config keys (all optional; defaults in DEFAULTS below):

    frontend        what shows the menu: graphical
    timeout         seconds before the default entry boots
    cmdline         root, quiet, splash and extra (a list) for the kernel
                    command lines, see mkuki.py
    banner_text     text of the banner

  for the graphical frontend:
    wallpaper       PNG or other image; a relative path is relative to the
                    config file
    res_x, res_y    display resolution of the bootloader: the size of the
                    background
    font            fontconfig name or file of the font for all texts
    color           colour of unselected menu labels
    selected_color  colour of the selected menu label and the menu box border
    border_width    width of the menu box border in pixels

Needs systemd-ukify, and for the UKIs systemd-boot-efi. The graphical frontend
needs python3-pil and fontconfig.

The package hyprdebian-bootloader-<frontend> (make deb) installs this script
and its helpers in /usr/lib/hyprdebian/bootloader, with the module and the
compiled bootloader of its frontend and the assets stub that --bootloader-dir
and --assets-stub default to, links it as /usr/sbin/hd-bootloader-update, and
installs the config for its frontend in /etc/hyprdebian/bootloader. There is
a package per frontend, and only one of them can be installed. From the source tree, make esp passes the files
in build/ and the bootloader.json there instead.

Example:
    hd-bootloader-update --esp /boot/efi
"""

import argparse
import importlib
import json
import os
import struct
import subprocess
import sys
import tempfile
import uuid
from pathlib import Path

# mkuki.py and the frontend modules with their helpers live next to this script,
# also when it is started through the /usr/sbin/hd-bootloader-update link.
# Installed, that is a directory of the package: importing them must not
# leave __pycache__ directories there that dpkg does not know about.
SCRIPTS = Path(__file__).resolve().parent
sys.path.insert(0, str(SCRIPTS))
sys.dont_write_bytecode = True

import mkuki  # noqa: E402

# The frontends, by the name in the config. Each has a module <frontend>.py
# next to this script, with a sections(config, labels) function that builds
# its sections. A package installs only the module of its own frontend, so a
# module is only imported when the config names it.
FRONTENDS = ["graphical"]

DEFAULTS = {
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
    "cmdline": {"root": "ZFS=rpool/hyprdebian", "quiet": True, "splash": True, "extra": []},
}

# The bootloader menu has this many rows (slot_count in bootloader.cpp), a
# row per profile of the KERNEL_COUNT most recent kernels.
SLOT_COUNT = 8
KERNEL_COUNT = SLOT_COUNT // len(mkuki.PROFILE_SUFFIXES)
UKI_PREFIX = "HyprDebian-Kernel-"

# The SBAT sections of the bootloader and of assets.efi: the standard header
# line, then our entry. Raise the generation of an entry (its second field)
# when a security fix must make older files revocable.
SBAT_HEADER = "sbat,1,SBAT Version,sbat,1,https://github.com/rhboot/shim/blob/main/SBAT.md\n"
SBAT = SBAT_HEADER + "hyprdebian-boot,1,HyprDebian,hyprdebian-boot,1,HyprDebian bootloader\n"
ASSETS_SBAT = SBAT_HEADER + "hyprdebian-assets,1,HyprDebian,hyprdebian-assets,1,HyprDebian bootloader assets\n"

# EFI_CERT_X509_GUID: the type of an EFI_SIGNATURE_LIST of X.509 certificates.
CERT_X509_GUID = uuid.UUID("a5c059a1-94e4-4aa7-87b5-ab155c2bf072")
# The owner of the MOK in the signature list: SHIM_LOCK_GUID, as shim and
# mokutil use for MOKs.
MOK_OWNER_GUID = uuid.UUID("605dab50-e046-4300-abb6-3dd810dd8b23")

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
    if config["frontend"] not in FRONTENDS:
        sys.exit(f"{path}: unknown frontend {config['frontend']!r}; known: {', '.join(FRONTENDS)}")

    wallpaper = Path(config["wallpaper"])
    if not wallpaper.is_absolute():
        wallpaper = Path(path).resolve().parent / wallpaper
    config["wallpaper"] = wallpaper
    return config


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


def same_efi(a, b):
    """True if the PE files a and b are the same, apart from an Authenticode signature."""
    return without_signature(a.read_bytes()) == without_signature(b.read_bytes())


def build_ukis(config, esp, kernels):
    """Writes the UKIs of the kernels that changed. Returns their file names, newest first."""
    linux = esp / "EFI" / "Linux"
    linux.mkdir(parents=True, exist_ok=True)

    cmdline = config["cmdline"]
    flags = ["--root", cmdline["root"]]
    if not cmdline["quiet"]:
        flags.append("--no-quiet")
    if not cmdline["splash"]:
        flags.append("--no-splash")
    for extra in cmdline["extra"]:
        flags += ["--extra", extra]

    names = []
    for version, vmlinuz, initrd in kernels:
        name = uki_name(version)
        names.append(name)
        target = linux / name
        temporary = linux / f".{name}.tmp"
        try:
            subprocess.run(
                [sys.executable, SCRIPTS / "mkuki.py", vmlinuz, initrd, "--output", temporary, *flags],
                check=True,
            )
            if target.is_file() and same_efi(target, temporary):
                print(f"Unchanged {target}", flush=True)
                continue
            os.replace(temporary, target)
        finally:
            temporary.unlink(missing_ok=True)
        print(f"Wrote {target}", flush=True)
    return names


def mok_signature_list(path):
    """Returns the certificate in path, DER or PEM, as an EFI_SIGNATURE_LIST with that one certificate.

    The list is the header (type, list size, signature header size 0,
    signature size) and one signature: the owner GUID and the DER certificate.
    """
    data = path.read_bytes()
    if data.startswith(b"-----BEGIN"):
        result = subprocess.run(
            ["openssl", "x509", "-in", path, "-outform", "DER"], check=True, capture_output=True
        )
        data = result.stdout
    if not data.startswith(b"\x30"):
        sys.exit(f"{path}: not an X.509 certificate in DER or PEM")
    signature = MOK_OWNER_GUID.bytes_le + data
    header = CERT_X509_GUID.bytes_le + struct.pack("<III", 28 + len(signature), 0, len(signature))
    return header + signature


def remove_old_ukis(esp, keep):
    """Removes the HyprDebian-Kernel-*.efi files in EFI/Linux whose names are not in keep."""
    for old in (esp / "EFI" / "Linux").glob(f"{UKI_PREFIX}*.efi"):
        if old.name not in keep:
            old.unlink()
            print(f"Removed {old}", flush=True)


def menu_rows(uki_names):
    """Returns (UKI file name, profile, label) of the menu rows, at most SLOT_COUNT.

    A row per profile of each UKI, newest UKI first. The label is the UKI
    file name without .efi, followed by the suffix of the profile.
    """
    rows = []
    for name in uki_names:
        for profile, suffix in enumerate(mkuki.PROFILE_SUFFIXES):
            rows.append((name, profile, name.removesuffix(".efi") + suffix))
    return rows[:SLOT_COUNT]


def without_signature(data):
    """Returns PE file data without its Authenticode signature, as it was before signing.

    sbsign appends the signature (the certificate table) to the end of the
    file, points the security entry of the data directories at it, pads the
    file to 8 bytes before it and sets the checksum. This drops the table,
    clears the entry and the checksum, and strips the trailing padding.
    Returns data unchanged if it is not a PE32+ file.
    """
    if len(data) < 64 or data[:2] != b"MZ":
        return data
    (pe_offset,) = struct.unpack_from("<I", data, 0x3C)
    optional_header = pe_offset + 24
    if data[pe_offset : pe_offset + 4] != b"PE\0\0" or len(data) < optional_header + 152:
        return data
    (magic,) = struct.unpack_from("<H", data, optional_header)
    if magic != 0x20B:
        return data

    checksum = optional_header + 64
    # The security entry is the fifth of the data directories, which start at
    # offset 112 of a PE32+ optional header.
    security = optional_header + 112 + 4 * 8
    table_offset, _ = struct.unpack_from("<II", data, security)
    result = bytearray(data[:table_offset] if table_offset else data)
    result[checksum : checksum + 4] = bytes(4)
    result[security : security + 8] = bytes(8)
    return bytes(result).rstrip(b"\0")


def build_efi(stub, target, sbat, sections):
    """Writes target: the PE file stub with an SBAT section and the given {name: data} sections added.

    If target already holds that, signed or not, it is left alone, so that
    its Secure Boot signature stays valid.
    """
    target.parent.mkdir(parents=True, exist_ok=True)
    temporary = target.with_name(f".{target.name}.tmp")
    with tempfile.TemporaryDirectory() as tmp:
        options = []
        for name, data in sections.items():
            path = Path(tmp) / name.lstrip(".")
            path.write_bytes(data)
            options.append(f"--section={name}:@{path}")
        try:
            subprocess.run(
                ["ukify", "build", f"--stub={stub}", *options, f"--sbat={sbat}", f"--output={temporary}"],
                check=True,
            )
            if target.is_file() and same_efi(target, temporary):
                print(f"Unchanged {target}", flush=True)
                return
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
        default=Path("/etc/hyprdebian/bootloader/bootloader.json"),
        help="JSON config file (default: /etc/hyprdebian/bootloader/bootloader.json)",
    )
    parser.add_argument(
        "--bootloader-dir",
        type=Path,
        default=SCRIPTS,
        help="where the compiled bootloaders, bootx64-<frontend>.efi, are (default: next to this script)",
    )
    parser.add_argument(
        "--assets-stub",
        type=Path,
        default=SCRIPTS / "assets.efi",
        help="the compiled assets stub (default: assets.efi next to this script)",
    )
    parser.add_argument("--boot-dir", type=Path, default=Path("/boot"), help="where the kernels are (default: /boot)")
    parser.add_argument(
        "--mok",
        type=Path,
        default=Path("/var/lib/dkms/mok.pub"),
        help="certificate of the DKMS MOK, DER or PEM (default: /var/lib/dkms/mok.pub)",
    )
    args = parser.parse_args()

    config = load_config(args.config)
    name = config["frontend"]
    if not (SCRIPTS / f"{name}.py").is_file():
        sys.exit(f"{args.config}: the {name} frontend is not installed; it is in the package hyprdebian-bootloader-{name}")
    frontend = importlib.import_module(name)
    bootloader = args.bootloader_dir / f"bootx64-{name}.efi"

    for path in (bootloader, args.assets_stub):
        if not path.is_file():
            sys.exit(f"{path}: not a file; build the bootloader first")
    if not args.mok.is_file():
        sys.exit(f"{args.mok}: not a file; the DKMS MOK is needed to boot with the ZFS module")
    mok = mok_signature_list(args.mok)

    config_text = f"timeout {config['timeout']}\n".encode()

    # 1. The UKIs.
    kernels = find_kernels(args.boot_dir)
    if not kernels:
        sys.exit(f"no kernels with an initrd found in {args.boot_dir}")
    uki_names = build_ukis(config, args.esp, kernels)
    rows = menu_rows(uki_names)

    # 2. The assets: the config, the menu rows, the DKMS MOK and the sections
    # of the frontend.
    boot = args.esp / "EFI" / "BOOT"
    build_efi(
        args.assets_stub,
        boot / "assets.efi",
        ASSETS_SBAT,
        {
            ".config": config_text,
            **{f".slot{slot}": f"{name} {profile}\n".encode() for slot, (name, profile, _) in enumerate(rows, 1)},
            ".mok": mok,
            **frontend.sections(config, [label for _, _, label in rows]),
        },
    )

    # 3. The bootloader.
    build_efi(bootloader, boot / "BOOTX64.EFI", SBAT, {})

    # 4. Only now that assets.efi no longer names them, the UKIs of older kernels.
    remove_old_ukis(args.esp, set(uki_names))

    for old in OLD_ASSETS:
        path = args.esp / old
        if path.exists():
            path.unlink()
            print(f"Removed {path}", flush=True)


if __name__ == "__main__":
    main()
