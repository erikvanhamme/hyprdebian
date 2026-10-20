#!/usr/bin/python3
"""Provisions a HyprDebian vm in libvirt on this computer, to install with the live cd.

It asks for the name, the zvol, the number of cpus, the memory and the bridge of the vm, whether
it runs with a GPU and, if so, which one, renders domain.xml.j2 next to it, defines the vm in
qemu:///system and starts it from the live ISO. The zvol must exist; make it first, for example
with zfs create -V 64G rpool/vmdk/<name>.

    provision-vm.py             ask the questions, then define and start the vm
    provision-vm.py --dry-run   ask the questions, then only print the domain XML

Run it with make provision-vm in the root of the repository.
"""

import argparse
import os
import re
import sys

import jinja2

from vmcommon import (URI, VmError, ask, ask_gpu, ask_with_gpu, choose, define, domains, gpus,
                      run, virsh)

HERE = os.path.dirname(os.path.abspath(__file__))
TEMPLATE = "domain.xml.j2"
ISO = os.path.join(os.path.dirname(HERE), "livecd", "out", "hyprdebian-live-amd64.iso")
ZVOL_DIR = "/dev/zvol"
# The memory of the vm, in GiB: the powers of two from 4 to 32.
MEMORY_GIB = [4, 8, 16, 32]
DEFAULT_CPUS = 2
DEFAULT_BRIDGE = "br0"
NAME_PATTERN = re.compile(r"^[a-z][a-z0-9-]*$")


def disks_in_use():
    """The disks of the vms in libvirt, as {path: name of the vm}."""
    used = {}
    for domain in domains():
        # The lines of domblklist --details are: type device target source.
        for line in virsh("domblklist", domain, "--details").splitlines()[2:]:
            fields = line.split(None, 3)
            if len(fields) == 4 and fields[1] == "disk":
                used[fields[3]] = domain
    return used


def zvols():
    """The zvols of this computer, as a list of (name, size in GiB)."""
    volumes = []
    for line in run("zfs", "list", "-H", "-p", "-t", "volume", "-o", "name,volsize").splitlines():
        name, size = line.split("\t")
        volumes.append((name, int(size) / 2**30))
    return volumes


def ask_name(existing):
    """Asks for the name of the vm, which must be new to libvirt."""
    while True:
        name = ask("\nName of the vm")
        if not NAME_PATTERN.match(name):
            print("Use a lower case letter, then lower case letters, digits and -.")
        elif name in existing:
            print(f"There already is a vm {name}.")
        else:
            return name


def ask_zvol(name, volumes, used):
    """Asks for the zvol of the vm, and returns its device path."""
    if not volumes:
        raise VmError("there are no zvols; make one first with zfs create -V")

    options = []
    default = 0
    for index, (volume, size) in enumerate(volumes):
        path = os.path.join(ZVOL_DIR, volume)
        error = f"used by {used[path]}" if path in used else None
        options.append((f"{volume} ({size:g} GiB)", error))
        if volume.rsplit("/", 1)[-1] == name:
            default = index

    return os.path.join(ZVOL_DIR, volumes[choose("Zvol of the vm:", options, default)][0])


def ask_cpus():
    """Asks for the number of cpus of the vm, at most the number of this computer."""
    maximum = os.cpu_count() or 1
    while True:
        answer = ask(f"\nNumber of cpus (1-{maximum})", min(DEFAULT_CPUS, maximum))
        if answer.isdigit() and 1 <= int(answer) <= maximum:
            return int(answer)
        print(f"Give a number from 1 to {maximum}.")


def ask_memory():
    """Asks for the memory of the vm, in GiB."""
    options = [(f"{size} GiB", None) for size in MEMORY_GIB]
    return MEMORY_GIB[choose("Memory of the vm:", options)]


def ask_bridge():
    """Asks for the network bridge of this computer that the vm is connected to."""
    while True:
        bridge = ask("\nNetwork bridge", DEFAULT_BRIDGE)
        if os.path.isdir(f"/sys/class/net/{bridge}/bridge"):
            return bridge
        print(f"{bridge} is not a network bridge of this computer.")


def render(settings):
    """Renders the domain XML of the vm. Autoescape keeps paths with quotes or & valid XML."""
    environment = jinja2.Environment(loader=jinja2.FileSystemLoader(HERE), autoescape=True,
                                     keep_trailing_newline=True)
    return environment.get_template(TEMPLATE).render(settings)


def confirm(settings):
    """Shows the answers and asks whether to provision the vm."""
    print("\nThe vm:")
    print(f"  Name:    {settings['name']}")
    print(f"  Zvol:    {settings['disk']}")
    print(f"  Cpus:    {settings['cpus']}")
    print(f"  Memory:  {settings['memory']} GiB")
    print(f"  Bridge:  {settings['bridge']}")
    print(f"  GPU:     {settings['rendernode'] or 'none, default graphics'}")
    print(f"  Live cd: {settings['iso']}")
    return ask("\nDefine and start it? (y/N)").lower() in ("y", "yes")


def provision(xml, name):
    """Defines the vm in libvirt and starts it."""
    define(xml, name)
    virsh("start", name)


def main():
    parser = argparse.ArgumentParser(
        description="Provisions a HyprDebian vm in libvirt, to install with the live cd.")
    parser.add_argument("--dry-run", action="store_true",
                        help="only print the domain XML, without defining the vm")
    args = parser.parse_args()

    try:
        if not os.path.isfile(ISO):
            raise VmError(f"{ISO} does not exist; build it with make iso")

        name = ask_name(domains())
        found = gpus()
        settings = {
            "name": name,
            "disk": ask_zvol(name, zvols(), disks_in_use()),
            "cpus": ask_cpus(),
            "memory": ask_memory(),
            "bridge": ask_bridge(),
            "iso": ISO,
        }
        settings["rendernode"] = ask_gpu(found) if ask_with_gpu(found) else None
        xml = render(settings)

        if args.dry_run:
            print()
            print(xml, end="")
            return 0

        if not confirm(settings):
            print("Nothing was changed.")
            return 1

        provision(xml, name)
    except VmError as e:
        print(f"Error: {e}", file=sys.stderr)
        return 1
    except (KeyboardInterrupt, EOFError):
        print("\nNothing was changed.")
        return 1

    print(f"\nThe vm {name} is running from the live cd. Open its console with:")
    print(f"    virt-manager --connect {URI} --show-domain-console {name}")
    print("then install HyprDebian with sudo hd-installer.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
