"""What the scripts in vm/ share: running virsh on the system instance of libvirt, asking
questions and finding the GPUs of this computer that can render the 3D graphics of a vm."""

import os
import subprocess

URI = "qemu:///system"
# The render nodes of the GPUs, by PCI path, and the cards and their connectors in sysfs.
DRI_BY_PATH = "/dev/dri/by-path"
DRM_CLASS = "/sys/class/drm"


class VmError(Exception):
    """A problem that stops the script, with the message for the user."""


def run(*args):
    """Runs a command and returns its output; raises VmError when it fails."""
    try:
        result = subprocess.run(args, capture_output=True, text=True)
    except OSError as e:
        raise VmError(f"cannot run {args[0]}: {e}")

    if result.returncode != 0:
        raise VmError(f"{' '.join(args)} failed: {result.stderr.strip()}")

    return result.stdout


def virsh(*args):
    """Runs virsh on the system instance of libvirt and returns its output."""
    return run("virsh", "-c", URI, *args)


def domains():
    """The names of all vms in libvirt, running or not."""
    return [line for line in virsh("list", "--all", "--name").splitlines() if line]


def ask(prompt, default=None):
    """Asks for a line of text; an empty answer gives the default."""
    suffix = f" [{default}]" if default is not None else ""
    answer = input(f"{prompt}{suffix}: ").strip()
    if not answer and default is not None:
        return str(default)
    return answer


def choose(prompt, options, default=0):
    """Asks to pick one of options by number, and returns its index.

    options is a list of (label, error), where error is None for an option that can be picked, or
    the reason why it cannot.
    """
    print(f"\n{prompt}")
    for number, (label, error) in enumerate(options, 1):
        note = f" ({error})" if error else ""
        print(f"  {number}) {label}{note}")

    while True:
        answer = ask("Number", default + 1)
        if answer.isdigit() and 1 <= int(answer) <= len(options):
            index = int(answer) - 1
            if options[index][1] is None:
                return index
            print(f"That one cannot be used: {options[index][1]}.")
        else:
            print(f"Give a number from 1 to {len(options)}.")


def gpus():
    """The GPUs of this computer, as a list of (render node by PCI path, whether a monitor is
    connected to it)."""
    if not os.path.isdir(DRI_BY_PATH):
        return []

    found = []
    for entry in sorted(os.listdir(DRI_BY_PATH)):
        if not entry.endswith("-render"):
            continue
        card_link = os.path.join(DRI_BY_PATH, entry[:-len("render")] + "card")
        connected = False
        if os.path.exists(card_link):
            card = os.path.basename(os.path.realpath(card_link))
            for connector in os.listdir(DRM_CLASS):
                if not connector.startswith(card + "-"):
                    continue
                with open(os.path.join(DRM_CLASS, connector, "status")) as f:
                    if f.read().strip() == "connected":
                        connected = True
        found.append((os.path.join(DRI_BY_PATH, entry), connected))
    return found


def ask_with_gpu(found):
    """Asks whether the vm runs with a GPU (3D acceleration) or without one.

    A vm without a GPU gets the default graphics of libvirt, a virtio gpu without 3D
    acceleration, so it can be moved to a server that has no GPU. A computer without a GPU only
    allows a vm without one.
    """
    error = None if found else f"there is no GPU with a render node in {DRI_BY_PATH}"
    options = [
        ("With a GPU: 3D acceleration, rendered on a GPU of this computer", error),
        ("Without a GPU: the default graphics, to move to a server without a GPU", None),
    ]
    return choose("Graphics of the vm:", options, 0 if found else 1) == 0


def ask_gpu(found):
    """Asks for the GPU that renders the 3D graphics of the vm, and returns its render node.

    qemu renders with it (egl-headless), so it must be the GPU that shows the console on this
    computer: a GPU that drives no monitor hands its frames to another and the console of the vm
    is scrambled. A monitor connected to it makes it the default. found must not be empty.
    """
    if len(found) == 1:
        return found[0][0]

    options = []
    default = 0
    for index, (node, connected) in enumerate(found):
        options.append((f"{node}{' (monitor connected)' if connected else ''}", None))
        if connected and not found[default][1]:
            default = index

    return found[choose("GPU that renders the vm (the one of the monitor):", options, default)][0]


def define(xml, name):
    """Defines the vm from its domain XML in libvirt, or replaces its definition."""
    try:
        subprocess.run(["virsh", "-c", URI, "define", "/dev/stdin"], input=xml, text=True,
                       check=True)
    except (OSError, subprocess.CalledProcessError):
        raise VmError(f"cannot define the vm {name}")
