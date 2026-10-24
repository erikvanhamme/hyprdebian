#!/usr/bin/python3
"""Switches a vm in libvirt on this computer between running with a GPU and without one.

It asks for the vm and shows whether it runs with a GPU now, then switches it: a vm with a GPU
gets the default graphics of libvirt, a virtio gpu without 3D acceleration, so it can be moved to
a server without a GPU; a vm without a GPU gets 3D acceleration (virgl on virtio-vga-gl),
rendered with egl-headless on a GPU of this computer that it asks for, as provision-vm.py makes
it. It changes the definition of the vm in qemu:///system; a running vm keeps its graphics until
it is shut down and started again.

    switch-vm-gpu.py            ask the questions, then switch the vm
    switch-vm-gpu.py --dry-run  ask the questions, then only print the new domain XML

Run it with make switch-vm-gpu in the root of the repository.
"""

import argparse
import io
import re
import sys
import xml.etree.ElementTree as ET

from vmcommon import DRI_BY_PATH, URI, VmError, ask, ask_gpu, choose, define, domains, gpus, virsh


def load(name):
    """The inactive domain XML of the vm, parsed, keeping the prefixes of its namespaces.

    ElementTree reserves the prefixes ns0, ns1 and so on for namespaces it has no prefix for, and
    names them so itself when it writes them, so those are not registered.
    """
    xml = virsh("dumpxml", "--inactive", name)
    for _, (prefix, uri) in ET.iterparse(io.StringIO(xml), events=["start-ns"]):
        if not re.fullmatch(r"ns\d+", prefix):
            ET.register_namespace(prefix, uri)
    return ET.fromstring(xml)


def primary_video(devices):
    """The model of the primary video device of the vm, or None when it has none."""
    models = [video.find("model") for video in devices.findall("video")]
    models = [model for model in models if model is not None]
    for model in models:
        if model.get("primary") == "yes":
            return model
    if models:
        return models[0]
    return None


def has_gpu(devices):
    """Whether the vm runs with a GPU: egl-headless graphics or 3D acceleration on its video."""
    if devices.find("graphics[@type='egl-headless']") is not None:
        return True
    model = primary_video(devices)
    if model is None:
        return False
    acceleration = model.find("acceleration")
    return acceleration is not None and acceleration.get("accel3d") == "yes"


def with_gpu(devices, rendernode):
    """Gives the vm 3D acceleration, rendered with egl-headless on rendernode."""
    for graphics in devices.findall("graphics[@type='egl-headless']"):
        devices.remove(graphics)

    # The egl-headless graphics go right after the last graphics, as in domain.xml.j2.
    graphics = ET.Element("graphics", type="egl-headless")
    ET.SubElement(graphics, "gl", rendernode=rendernode)
    children = list(devices)
    last = max((i for i, child in enumerate(children) if child.tag == "graphics"), default=None)
    if last is None:
        devices.append(graphics)
    else:
        devices.insert(last + 1, graphics)

    model = primary_video(devices)
    if model is None:
        video = ET.SubElement(devices, "video")
        model = ET.SubElement(video, "model")
    for acceleration in model.findall("acceleration"):
        model.remove(acceleration)
    model.set("type", "virtio")
    model.set("primary", "yes")
    model.set("device", "virtio-vga-gl")
    model.insert(0, ET.Element("acceleration", accel3d="yes"))


def without_gpu(devices):
    """Gives the vm the default graphics of libvirt: a virtio gpu without 3D acceleration."""
    for graphics in devices.findall("graphics[@type='egl-headless']"):
        devices.remove(graphics)
    # OpenGL in spice also needs a GPU.
    for graphics in devices.findall("graphics"):
        for gl in graphics.findall("gl"):
            graphics.remove(gl)

    model = primary_video(devices)
    if model is None:
        return
    for acceleration in model.findall("acceleration"):
        model.remove(acceleration)
    model.set("type", "virtio")
    # libvirt keeps a device it once picked; without it, it picks virtio-vga again.
    if "device" in model.attrib:
        del model.attrib["device"]


def state(name):
    """The state of the vm, such as running or shut off."""
    return virsh("domstate", name).strip()


def main():
    parser = argparse.ArgumentParser(
        description="Switches a vm in libvirt between running with a GPU and without one.")
    parser.add_argument("--dry-run", action="store_true",
                        help="only print the new domain XML, without changing the vm")
    args = parser.parse_args()

    try:
        names = domains()
        if not names:
            raise VmError("there are no vms in libvirt")

        options = []
        for name in names:
            mode = "with a GPU" if has_gpu(load(name).find("devices")) else "without a GPU"
            options.append((f"{name} ({state(name)}, {mode})", None))
        name = names[choose("Vm to switch:", options)]

        domain = load(name)
        devices = domain.find("devices")
        if has_gpu(devices):
            print(f"\n{name} runs with a GPU; it will run without one, with the default graphics.")
            without_gpu(devices)
            rendernode = None
        else:
            found = gpus()
            if not found:
                raise VmError(f"there is no GPU with a render node in {DRI_BY_PATH}")
            print(f"\n{name} runs without a GPU; it will run with one, with 3D acceleration.")
            rendernode = ask_gpu(found)
            with_gpu(devices, rendernode)

        ET.indent(domain)
        xml = ET.tostring(domain, encoding="unicode") + "\n"

        if args.dry_run:
            print()
            print(xml, end="")
            return 0

        if ask("\nSwitch it? (y/N)").lower() not in ("y", "yes"):
            print("Nothing was changed.")
            return 1

        define(xml, name)
    except VmError as e:
        print(f"Error: {e}", file=sys.stderr)
        return 1
    except (KeyboardInterrupt, EOFError):
        print("\nNothing was changed.")
        return 1

    if rendernode is None:
        print(f"\nThe vm {name} now runs without a GPU.")
    else:
        print(f"\nThe vm {name} now runs with the GPU {rendernode}.")
    if state(name) != "shut off":
        print("It is running: shut it down and start it again for its new graphics, for example "
              f"with\n    virsh -c {URI} shutdown {name}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
