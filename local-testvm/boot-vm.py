#!/usr/bin/python3
"""Starts the installer vm from one of its two disks.

A tool of the test vm, see .claude/skills/hyprdebian-test-vm. The vm debian13 has the
installer system on the disk with serial vmdk-installer and the HyprDebian test system on the disk
with serial vmdk-test. It shuts the vm down when it runs, gives the chosen disk boot order 1 and
the other one 2, defines the vm again and starts it.

Each disk has a boot order of its own, not <boot dev='hd'/> in <os>: OVMF deletes the firmware boot
entries of the disks that are not in the boot order of QEMU, and with <boot dev='hd'/> only the
first disk is in it, so the entry of the second disk is lost on every boot. With both disks in the
boot order, the firmware boots the fallback path EFI/BOOT/BOOTX64.EFI of the first disk.

    boot-vm.py installer    boot the installer system, test-installer
    boot-vm.py test         boot the HyprDebian test system, test
"""

import argparse
import io
import re
import subprocess
import sys
import time
import xml.etree.ElementTree as ET

URI = "qemu:///system"
DOMAIN = "debian13"
SERIALS = {"installer": "vmdk-installer", "test": "vmdk-test"}
SHUTDOWN_SECONDS = 120


def virsh(*args, stdin=None):
    """Runs virsh and returns its output; exits with the error of virsh when it fails."""
    result = subprocess.run(["virsh", "-c", URI, *args], input=stdin, capture_output=True,
                            text=True)
    if result.returncode != 0:
        sys.exit(f"Error: virsh {' '.join(args)} failed: {result.stderr.strip()}")
    return result.stdout


def shut_down():
    """Shuts the vm down when it runs, and waits until it is off."""
    if virsh("domstate", DOMAIN).strip() == "shut off":
        return
    virsh("shutdown", DOMAIN)
    for _ in range(SHUTDOWN_SECONDS):
        if virsh("domstate", DOMAIN).strip() == "shut off":
            return
        time.sleep(1)
    sys.exit(f"Error: {DOMAIN} did not shut down in {SHUTDOWN_SECONDS} seconds.")


def boot_order(xml, first):
    """The domain XML with the disk of the system first in the boot order, the other second.

    It keeps the prefixes of the namespaces, such as libosinfo in the metadata, except ns0, ns1
    and so on, which ElementTree reserves and names so itself when it writes them.
    """
    for _, (prefix, uri) in ET.iterparse(io.StringIO(xml), events=["start-ns"]):
        if not re.fullmatch(r"ns\d+", prefix):
            ET.register_namespace(prefix, uri)
    root = ET.fromstring(xml)
    for boot in root.findall("./os/boot"):
        root.find("os").remove(boot)

    found = set()
    for disk in root.findall("./devices/disk"):
        serial = disk.findtext("serial")
        for boot in disk.findall("boot"):
            disk.remove(boot)
        if serial in SERIALS.values():
            ET.SubElement(disk, "boot", order="1" if serial == SERIALS[first] else "2")
            found.add(serial)

    if found != set(SERIALS.values()):
        sys.exit(f"Error: {DOMAIN} has no disks with the serials {', '.join(SERIALS.values())}.")
    return ET.tostring(root, encoding="unicode")


def main():
    parser = argparse.ArgumentParser(description="Starts the installer vm from one of its disks.")
    parser.add_argument("system", choices=sorted(SERIALS), help="the system to boot")
    args = parser.parse_args()

    shut_down()
    virsh("define", "/dev/stdin", stdin=boot_order(virsh("dumpxml", "--inactive", DOMAIN),
                                                   args.system))
    virsh("start", DOMAIN)
    print(f"{DOMAIN} boots the {args.system} system.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
