#!/usr/bin/python3
"""Saves and restores states of the HyprDebian test system, from the installer system of the vm.

A tool of the test vm, see .claude/skills/hyprdebian-test-vm. The vm debian13 has the
installer system on one disk and the test system on the other (see boot-vm.py). A state is a
recursive ZFS snapshot rpool@<name> of the pool of the test system, and a tar of its EFI system
partition in /var/lib/hd-test on the installer system, as the bootloader writes the kernels there.
The work is done on the installer system, with the pool imported without mounting it, and the pool
is always exported again, so the test system imports it at its next boot as its own.

    test-snapshot.py save <name>       save the current state of the test system as <name>
    test-snapshot.py rollback <name>   bring the test system back to the state <name>
    test-snapshot.py list              list the saved states
    test-snapshot.py ... --boot-test   boot the test system afterwards

The vm is booted into the installer system first when it runs the test system.
"""

import argparse
import os
import re
import shlex
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
INSTALLER = "test-installer"
POOL = "rpool"
ESP = "/dev/disk/by-id/virtio-vmdk-test-part1"
STATE_DIR = "/var/lib/hd-test"
NAME_PATTERN = re.compile(r"^[a-z0-9][a-z0-9_-]*$")


def reachable(host):
    return subprocess.run(["ssh", "-o", "BatchMode=yes", "-o", "ConnectTimeout=3", host, "true"],
                          capture_output=True).returncode == 0


def boot(system):
    """Boots the vm into the system with boot-vm.py and waits until ssh answers."""
    subprocess.run([sys.executable, os.path.join(HERE, "boot-vm.py"), system], check=True)
    host = INSTALLER if system == "installer" else "test"
    for _ in range(90):
        time.sleep(2)
        if system == "installer" and reachable(host):
            return
        if system == "test":
            result = subprocess.run(["ssh", "-o", "BatchMode=yes", "-o", "ConnectTimeout=3",
                                     "-o", "StrictHostKeyChecking=no",
                                     "-o", "UserKnownHostsFile=/dev/null", "-o", "LogLevel=ERROR",
                                     host, "true"], capture_output=True, text=True)
            if result.returncode == 0 or "Permission denied" in result.stderr:
                return
    sys.exit(f"Error: {host} does not answer.")


def on_installer(script):
    """Runs a shell script as root on the installer system; the pool is exported at the end, also
    when the script fails."""
    wrapped = f"""set -e
trap 'umount /tmp/hd-test-esp 2>/dev/null || true; zpool export {POOL} 2>/dev/null || true' EXIT
zpool import -f -N {POOL}
mkdir -p {STATE_DIR} /tmp/hd-test-esp
{script}
"""
    result = subprocess.run(["ssh", INSTALLER, "sudo bash -c " + shlex.quote(wrapped)])
    if result.returncode != 0:
        sys.exit("Error: the work on the installer system failed.")


def save(name):
    on_installer(f"""
zfs snapshot -r {POOL}@{name}
mount -o ro {ESP} /tmp/hd-test-esp
tar -C /tmp/hd-test-esp -cf {STATE_DIR}/esp-{name}.tar .
echo "Saved {name}."
""")


def rollback(name):
    """A new installation makes the pool again, which loses its snapshots; the ESP is only
    restored when the snapshot of the state is there."""
    on_installer(f"""
test -f {STATE_DIR}/esp-{name}.tar
if ! zfs list -H -t snapshot {POOL}@{name} > /dev/null 2>&1; then
    echo "Error: the pool has no snapshot {name}; a new installation removed it." >&2
    exit 1
fi
for dataset in $(zfs list -H -t snapshot -o name -r {POOL} | sed -n 's/@{name}$//p'); do
    zfs rollback -r "$dataset@{name}"
done
mount {ESP} /tmp/hd-test-esp
find /tmp/hd-test-esp -mindepth 1 -delete
tar -C /tmp/hd-test-esp -xf {STATE_DIR}/esp-{name}.tar
echo "Rolled back to {name}."
""")


def list_states():
    on_installer(f"""
zfs list -H -t snapshot -o name,creation {POOL} | sed 's/^{POOL}@//'
ls -l {STATE_DIR}
""")


def main():
    parser = argparse.ArgumentParser(description="Saves and restores states of the test system.")
    parser.add_argument("action", choices=["save", "rollback", "list"])
    parser.add_argument("name", nargs="?", help="the name of the state")
    parser.add_argument("--boot-test", action="store_true",
                        help="boot the test system afterwards")
    args = parser.parse_args()

    if args.action != "list" and (args.name is None or not NAME_PATTERN.match(args.name)):
        parser.error("give a name of lower case letters, digits, - and _")

    if not reachable(INSTALLER):
        boot("installer")

    if args.action == "save":
        save(args.name)
    elif args.action == "rollback":
        rollback(args.name)
    else:
        list_states()

    if args.boot_test:
        boot("test")
    return 0


if __name__ == "__main__":
    sys.exit(main())
