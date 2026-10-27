#!/usr/bin/env python3
"""Produces all the files the hyprdebian bootloader uses, in an EFI system partition.

Reads the settings from the key hd-bootloader in the hyprdebian configuration,
/etc/hyprdebian/config.json, or from the JSON file given with --config, and
writes, below the directory given with --esp (default: /boot/efi):

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
                                          bootloader hands to the kernel;
                                          only when there is one
                              and the sections of the frontend, which shows
                              the menu, built by <frontend>.py:
                                graphical the background, the banner and
                                          the label images of the rows; see
                                          graphical.py
                                text      the banner and the label texts of
                                          the rows; see text.py
    EFI/Linux/HyprDebian-Kernel-x.y.z.efi
                              multi-profile UKIs (mkuki.py) of the 4 most
                              recent kernels in /boot with a ZFS module;
                              HyprDebian-Kernel-x.y.z-hd.efi for a kernel
                              of hd-kernel, whose release has -hd

The menu has a row per profile of each of those kernels, newest kernel
first. With the labels and the file names in assets.efi, Secure Boot covers
everything the menu shows and which UKI each row boots; the UKI itself is
checked when the bootloader starts it.

With the kernel of Debian, the ZFS module is built by DKMS and signed with
the DKMS MOK, the key pair in /var/lib/dkms/mok.key and mok.pub. Under
Secure Boot the kernel loads it only if it trusts that key. With shim, shim
hands its MOKs to the kernel; here the bootloader does that with the MOK in
assets.efi, so Secure Boot covers it too. The kernels of hd-kernel come with
a ZFS module signed with the key they trust, so a system with only those
has no DKMS MOK, and assets.efi then has no .mok. Without a usable
assets.efi the bootloader shows an error and does not boot.

With the assets in a file of their own, a change of the config, the
theme or the texts leaves BOOTX64.EFI alone; a change of the frontend
replaces it. assets.efi is a PE image
that is never started: the bootloader loads it with LoadImage, so that with
Secure Boot on the firmware checks its signature as it does for BOOTX64.EFI,
and reads the sections from the loaded image. As assets.efi names the UKIs,
it changes with every new kernel, and is signed again with it, see below.
ukify adds the sections, so that they are loaded into memory, and an SBAT
entry to each file. A file whose contents would not change is not
rewritten.

The files are written in an order that leaves a bootable ESP if the script
is interrupted: first the UKIs, then assets.efi and BOOTX64.EFI, and only
then are the UKIs of older kernels removed, so assets.efi never names a UKI
that is gone.

The most recent kernels are those whose /boot/vmlinuz-* was modified last.
A kernel without a ZFS module in /lib/modules/<version>, built in or as
zfs.ko in any directory below it, cannot mount the root and gets no UKI,
such as a kernel of Debian whose DKMS module is gone. Other
HyprDebian-Kernel-*.efi files in EFI/Linux are removed.

Config keys of every frontend:

    frontend        what shows the menu: graphical, or text for the text
                    console of the firmware
    timeout         seconds before the default entry boots
    cmdline         root, quiet, splash and extra (a list) for the kernel
                    command lines, see mkuki.py; the frontend may add
                    arguments before those of extra, as the graphical one
                    adds video=<res_x>x<res_y>
    banner_text     text of the banner

and those of the frontend, in DEFAULTS of its module (graphical.py,
text.py). The postinst of the package of each frontend stores its keys and
their defaults, deb/<package>/defaults.json, in the key hd-bootloader. All
keys are optional: a key that is missing from the config gets the default
here or in the module of the frontend, which should match. A key of another
frontend is an error; a key that the frontend no longer uses, in OBSOLETE of
its module, is ignored with a warning.

When the Secure Boot keys of the system are in /etc/hyprdebian/secboot, as
the package hd-base generates them, BOOTX64.EFI, assets.efi and the UKIs
are signed with the db key (--db-key and --db-cert) by sbsign, before they
replace the files on the ESP. A file whose contents would not change is
still signed again when it does not verify with the db certificate, as after
the keys were generated or replaced. Without the keys the files are written
unsigned, with a message saying so.

Needs systemd-ukify, and for the UKIs systemd-boot-efi. Signing needs
sbsigntool. The graphical frontend
needs python3-pil and fontconfig.

The packages hd-bootloader-gfx (graphical) and hd-bootloader-txt (text) install
this script and its helpers in /usr/lib/hd-bootloader, with the module and
the compiled bootloader of their frontend and the assets stub that
--bootloader-dir and --assets-stub default to, and link it as
/usr/sbin/hd-bootloader-update. Only one of them can be installed. From the
source tree, make esp passes the files in build/ and the defaults of a
package in deb/ instead.

Example:
    hd-bootloader-update
"""

import argparse
import importlib
import json
import os
import shutil
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

CONFIG_FILE = Path("/etc/hyprdebian/config.json")
CONFIG_KEY = "hd-bootloader"

# The frontends, by the name in the config. Each has a module <frontend>.py
# next to this script, with:
#   DEFAULTS                  its config keys with their defaults
#   PATH_KEYS                 those of its keys that are file paths, which
#                             may be relative to the config file
#   sections(config, labels)  builds its sections
#   cmdline(config)           the arguments it adds to the kernel command
#                             lines, before those of extra
# A package installs only the module of its own frontend, so a module is only
# imported when the config names it.
FRONTENDS = ["graphical", "text"]
# The package of each frontend.
PACKAGES = {"graphical": "hd-bootloader-gfx", "text": "hd-bootloader-txt"}

# The config keys of every frontend, with their defaults.
DEFAULTS = {
    "frontend": "graphical",
    "timeout": 5,
    "banner_text": "HyprDebian Bootloader",
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


def read_config(path):
    """Returns the settings: the JSON object in path, or the key hd-bootloader in CONFIG_FILE without it.

    Also returns where they come from, for the messages, and the file that
    relative paths in them are relative to.
    """
    file = path if path is not None else CONFIG_FILE
    try:
        with open(file) as f:
            data = json.load(f)
    except (OSError, ValueError) as error:
        sys.exit(f"{file}: cannot read the settings: {error}")
    if path is None:
        if not isinstance(data, dict) or CONFIG_KEY not in data:
            sys.exit(
                f"{file}: the key {CONFIG_KEY} is missing; dpkg-reconfigure the package of the "
                "bootloader to store its defaults"
            )
        data = data[CONFIG_KEY]
        where = f"{file}: {CONFIG_KEY}"
    else:
        where = str(file)
    if not isinstance(data, dict):
        sys.exit(f"{where}: not a JSON object")
    return data, where, file


def load_config(path):
    """Returns the config, completed with the defaults, and the module of its frontend.

    The config is in the JSON file path, or with path None in the key
    hd-bootloader of CONFIG_FILE.
    """
    config, where, file = read_config(path)

    name = config.get("frontend", DEFAULTS["frontend"])
    if name not in FRONTENDS:
        sys.exit(f"{where}: unknown frontend {name!r}; known: {', '.join(FRONTENDS)}")
    if not (SCRIPTS / f"{name}.py").is_file():
        sys.exit(f"{where}: the {name} frontend is not installed; it is in the package {PACKAGES[name]}")
    frontend = importlib.import_module(name)

    # Settings that a frontend no longer uses are left out with a warning,
    # so that a config of an earlier version still works.
    for key in sorted(set(config) & set(getattr(frontend, "OBSOLETE", []))):
        print(f"{where}: the setting {key} is no longer used by the {name} frontend; ignoring it",
              file=sys.stderr)
        del config[key]

    defaults = {**DEFAULTS, **frontend.DEFAULTS}
    unknown = set(config) - set(defaults)
    if unknown:
        sys.exit(f"{where}: unknown settings for the {name} frontend: {', '.join(sorted(unknown))}")
    cmdline = {**DEFAULTS["cmdline"], **config.get("cmdline", {})}
    config = {**defaults, **config, "cmdline": cmdline}

    for key in frontend.PATH_KEYS:
        value = Path(config[key])
        if not value.is_absolute():
            value = Path(file).resolve().parent / value
        config[key] = value
    return config, frontend


def has_zfs(modules_dir, version):
    """True if the kernel version has a ZFS module in modules_dir/version: built in, or zfs.ko in any directory."""
    directory = modules_dir / version
    builtin = directory / "modules.builtin"
    if builtin.is_file() and any(line.endswith("/zfs.ko") for line in builtin.read_text().split()):
        return True
    return any(directory.rglob("zfs.ko*"))


def find_kernels(boot_dir, modules_dir):
    """Returns (version, vmlinuz, initrd) of the KERNEL_COUNT most recent kernels, newest first.

    Kernels without an initrd or without a ZFS module in modules_dir are
    skipped. Of kernels with the same UKI name, only the most recent is used.
    """
    kernels = []
    for vmlinuz in boot_dir.glob("vmlinuz-*"):
        version = vmlinuz.name.removeprefix("vmlinuz-")
        initrd = boot_dir / f"initrd.img-{version}"
        if not initrd.is_file():
            continue
        if not has_zfs(modules_dir, version):
            print(f"Skipping {version}: no ZFS module in {modules_dir / version}", flush=True)
            continue
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
    """The UKI file name of a kernel version.

    HyprDebian-Kernel-x.y.z.efi for a kernel of Debian, such as
    7.2.10+deb14-amd64, and HyprDebian-Kernel-x.y.z-hd.efi for one of
    hd-kernel, such as 7.2.10+deb14-hd1a2b3c-amd64, so the menu tells them
    apart.
    """
    short = version.split("+")[0].split("-")[0]
    if "-hd" in version:
        return f"{UKI_PREFIX}{short}-hd.efi"
    return f"{UKI_PREFIX}{short}.efi"


def same_efi(a, b):
    """True if the PE files a and b are the same, apart from an Authenticode signature."""
    return without_signature(a.read_bytes()) == without_signature(b.read_bytes())


def secure_boot_signing(key, cert):
    """Returns (key, cert) of the db key to sign with, or None to write the files unsigned.

    Missing or unreadable keys are not an error: the files are written
    unsigned, with a message saying why. With the keys there, sbsign and
    sbverify of sbsigntool are needed.
    """
    for path in (key, cert):
        if not path.is_file():
            print(f"Not signing for Secure Boot: {path} is missing", flush=True)
            return None
        if not os.access(path, os.R_OK):
            print(f"Not signing for Secure Boot: {path} cannot be read", flush=True)
            return None
    for tool in ("sbsign", "sbverify"):
        if shutil.which(tool) is None:
            sys.exit(f"{tool} is missing; install sbsigntool to sign with {key}")
    print(f"Signing for Secure Boot with {key}", flush=True)
    return key, cert


def signed_with(path, signing):
    """True if the PE file path has a signature that verifies with the certificate of signing."""
    _, cert = signing
    result = subprocess.run(["sbverify", "--cert", cert, path], capture_output=True)
    return result.returncode == 0


def unchanged(target, temporary, signing):
    """True if target can stay: it holds what temporary does and, when signing, is signed with the db key."""
    if not target.is_file() or not same_efi(target, temporary):
        return False
    if signing is None:
        return True
    return signed_with(target, signing)


def sign(path, signing):
    """Signs the PE file path in place with the db key of signing, if any."""
    if signing is None:
        return
    key, cert = signing
    signed = path.with_name(f"{path.name}.signed")
    try:
        subprocess.run(
            ["sbsign", "--key", key, "--cert", cert, "--output", signed, path], check=True, capture_output=True
        )
        os.replace(signed, path)
    finally:
        signed.unlink(missing_ok=True)


def written(signing):
    """What a file written with signing is, for the messages: signed or unsigned."""
    return "unsigned" if signing is None else "signed"


def build_ukis(config, frontend, esp, kernels, signing):
    """Writes the UKIs of the kernels that changed, signed if signing. Returns their file names, newest first.

    The arguments that the frontend adds to the command lines come before
    those of extra, so that extra can override them.
    """
    linux = esp / "EFI" / "Linux"
    linux.mkdir(parents=True, exist_ok=True)

    cmdline = config["cmdline"]
    flags = ["--root", cmdline["root"]]
    if not cmdline["quiet"]:
        flags.append("--no-quiet")
    if not cmdline["splash"]:
        flags.append("--no-splash")
    for extra in frontend.cmdline(config) + cmdline["extra"]:
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
            if unchanged(target, temporary, signing):
                print(f"Unchanged {target}", flush=True)
                continue
            sign(temporary, signing)
            os.replace(temporary, target)
        finally:
            temporary.unlink(missing_ok=True)
        print(f"Wrote {written(signing)} {target}", flush=True)
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


def build_efi(stub, target, sbat, sections, signing):
    """Writes target: the PE file stub with an SBAT section and the given {name: data} sections added.

    The new file is signed if signing. If target already holds that, it is
    left alone, so that its Secure Boot signature stays valid: unsigned or
    signed with any key without signing, signed with the db key with it.
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
            if unchanged(target, temporary, signing):
                print(f"Unchanged {target}", flush=True)
                return
            sign(temporary, signing)
            os.replace(temporary, target)
        finally:
            temporary.unlink(missing_ok=True)
    print(f"Wrote {written(signing)} {target}", flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument(
        "--esp",
        type=Path,
        default=Path("/boot/efi"),
        help="root of the EFI system partition to write to (default: /boot/efi)",
    )
    parser.add_argument(
        "--config",
        type=Path,
        help=f"JSON file with the settings (default: the key {CONFIG_KEY} in {CONFIG_FILE})",
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
        "--modules-dir",
        type=Path,
        default=Path("/lib/modules"),
        help="where the modules of the kernels are, to find their ZFS module (default: /lib/modules)",
    )
    parser.add_argument(
        "--mok",
        type=Path,
        default=Path("/var/lib/dkms/mok.pub"),
        help="certificate of the DKMS MOK, DER or PEM, when there is one (default: /var/lib/dkms/mok.pub)",
    )
    parser.add_argument(
        "--db-key",
        type=Path,
        default=Path("/etc/hyprdebian/secboot/db.key"),
        help="Secure Boot db key to sign the files with (default: /etc/hyprdebian/secboot/db.key)",
    )
    parser.add_argument(
        "--db-cert",
        type=Path,
        default=Path("/etc/hyprdebian/secboot/db.crt"),
        help="PEM certificate of the db key (default: /etc/hyprdebian/secboot/db.crt)",
    )
    args = parser.parse_args()

    config, frontend = load_config(args.config)
    bootloader = args.bootloader_dir / f"bootx64-{config['frontend']}.efi"

    for path in (bootloader, args.assets_stub):
        if not path.is_file():
            sys.exit(f"{path}: not a file; build the bootloader first")
    # Without DKMS there is no MOK, and assets.efi gets no .mok.
    mok = {}
    if args.mok.is_file():
        mok = {".mok": mok_signature_list(args.mok)}
    else:
        print(f"No DKMS MOK: {args.mok} is missing", flush=True)
    signing = secure_boot_signing(args.db_key, args.db_cert)

    config_text = f"timeout {config['timeout']}\n".encode()

    # 1. The UKIs.
    kernels = find_kernels(args.boot_dir, args.modules_dir)
    if not kernels:
        sys.exit(f"no kernels with an initrd and a ZFS module found in {args.boot_dir}")
    uki_names = build_ukis(config, frontend, args.esp, kernels, signing)
    rows = menu_rows(uki_names)

    # 2. The assets: the config, the menu rows, the DKMS MOK when there is
    # one and the sections of the frontend.
    boot = args.esp / "EFI" / "BOOT"
    build_efi(
        args.assets_stub,
        boot / "assets.efi",
        ASSETS_SBAT,
        {
            ".config": config_text,
            **{f".slot{slot}": f"{name} {profile}\n".encode() for slot, (name, profile, _) in enumerate(rows, 1)},
            **mok,
            **frontend.sections(config, [label for _, _, label in rows]),
        },
        signing,
    )

    # 3. The bootloader.
    build_efi(bootloader, boot / "BOOTX64.EFI", SBAT, {}, signing)

    # 4. Only now that assets.efi no longer names them, the UKIs of older kernels.
    remove_old_ukis(args.esp, set(uki_names))

    for old in OLD_ASSETS:
        path = args.esp / old
        if path.exists():
            path.unlink()
            print(f"Removed {path}", flush=True)


if __name__ == "__main__":
    main()
