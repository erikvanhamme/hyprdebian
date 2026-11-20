#!/usr/bin/python3
"""Saves and restores states of the whole test disk, as snapshots of its zvol on the host.

A tool of the test vm, see .claude/skills/hyprdebian-test-vm. The test disk of the vm debian13 is
the zvol rpool/vmdk/test of the host. A snapshot of it holds all of the test system: the pool, the
ESP and the partitions, also the snapshots that test-snapshot.py makes in the pool. The vm is shut
down first, so the disk is not in use. zfs runs as the user, who needs the permissions snapshot and
rollback on the zvol, and destroy and mount for destroy and for a rollback past newer snapshots:

    sudo zfs allow -l -u $USER snapshot,rollback,destroy,mount rpool/vmdk/test

    zvol-snapshot.py save <name>        shut the vm down and snapshot the test disk as <name>
    zvol-snapshot.py rollback <name>    shut the vm down and bring the test disk back to <name>;
                                        with snapshots newer than <name> only with
                                        --destroy-newer, which destroys them
    zvol-snapshot.py destroy <name>     destroy the snapshot <name>
    zvol-snapshot.py list               list the snapshots, the oldest first
    zvol-snapshot.py ... --boot-test    boot the test system afterwards, and wait for ssh
    zvol-snapshot.py ... --boot-test --wait session
                                        and wait for the Hyprland session
"""

import argparse
import subprocess
import sys

import testvm

ZVOL = "rpool/vmdk/test"
PERMISSIONS = f"sudo zfs allow -l -u $USER snapshot,rollback,destroy,mount {ZVOL}"


def zfs(*args):
    """Runs zfs and returns its output; exits with the error of zfs when it fails."""
    result = subprocess.run(["zfs", *args], capture_output=True, text=True)
    if result.returncode != 0:
        error = result.stderr.strip()
        if "permission denied" in error:
            error += f"\nThe permissions are given with: {PERMISSIONS}"
        sys.exit(f"Error: zfs {' '.join(args)} failed: {error}")
    return result.stdout


def snapshots():
    """The names of the snapshots of the zvol, the oldest first."""
    out = zfs("list", "-H", "-t", "snapshot", "-o", "name", "-s", "createtxg", ZVOL)
    return [line.split("@", 1)[1] for line in out.splitlines()]


def save(name):
    if name in snapshots():
        sys.exit(f"Error: there is a snapshot {name}; destroy it first.")
    testvm.shut_down()
    zfs("snapshot", f"{ZVOL}@{name}")
    print(f"Saved {name}.")


def rollback(name, destroy_newer):
    names = snapshots()
    if name not in names:
        sys.exit(f"Error: there is no snapshot {name}.")
    newer = names[names.index(name) + 1:]
    if newer and not destroy_newer:
        sys.exit(f"Error: the snapshots {', '.join(newer)} are newer than {name}; a rollback "
                 f"destroys them, so give --destroy-newer.")
    testvm.shut_down()
    if newer:
        zfs("rollback", "-r", f"{ZVOL}@{name}")
        print(f"Destroyed {', '.join(newer)}.")
    else:
        zfs("rollback", f"{ZVOL}@{name}")
    print(f"Rolled back to {name}.")


def destroy(name):
    if name not in snapshots():
        sys.exit(f"Error: there is no snapshot {name}.")
    zfs("destroy", f"{ZVOL}@{name}")
    print(f"Destroyed {name}.")


def main():
    parser = argparse.ArgumentParser(description="Saves and restores states of the test disk.")
    parser.add_argument("action", choices=["save", "rollback", "destroy", "list"])
    parser.add_argument("name", nargs="?", help="the name of the snapshot")
    parser.add_argument("--destroy-newer", action="store_true",
                        help="with rollback, destroy the snapshots newer than the one given")
    parser.add_argument("--boot-test", action="store_true",
                        help="boot the test system afterwards")
    parser.add_argument("--wait", choices=["ssh", "session"], default="ssh",
                        help="with --boot-test, wait for ssh (the default) or the Hyprland "
                             "session")
    args = parser.parse_args()

    if args.action != "list" and (args.name is None or not testvm.NAME_PATTERN.match(args.name)):
        parser.error("give a name of lower case letters, digits, - and _")

    if args.action == "save":
        save(args.name)
    elif args.action == "rollback":
        rollback(args.name, args.destroy_newer)
    elif args.action == "destroy":
        destroy(args.name)
    else:
        print(zfs("list", "-t", "snapshot", "-o", "name,creation,used", "-s", "createtxg", ZVOL),
              end="")

    if args.boot_test:
        testvm.boot("test", "test" if args.wait == "ssh" else "session")
    return 0


if __name__ == "__main__":
    sys.exit(main())
