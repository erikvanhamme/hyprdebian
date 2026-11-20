#!/usr/bin/python3
"""Starts the test vm from one of its two disks.

A tool of the test vm, see .claude/skills/hyprdebian-test-vm. The vm debian13 has the
installer system on the disk with serial vmdk-installer and the HyprDebian test system on the disk
with serial vmdk-test. It shuts the vm down when it runs, gives the chosen disk boot order 1 and
the other one 2 (see boot_order in testvm.py), defines the vm again and starts it. Then it waits
until the system answers ssh, or for the Hyprland session with --wait session.

    boot-vm.py installer                boot the installer system, test-installer
    boot-vm.py test                     boot the HyprDebian test system, test
    boot-vm.py test --wait session      and wait for the Hyprland session
    boot-vm.py test --wait none         do not wait
"""

import argparse
import sys

import testvm


def main():
    parser = argparse.ArgumentParser(description="Starts the test vm from one of its disks.")
    parser.add_argument("system", choices=sorted(testvm.SERIALS), help="the system to boot")
    parser.add_argument("--wait", choices=["ssh", "session", "none"], default="ssh",
                        help="wait for ssh (the default), the Hyprland session of the test "
                             "system, or nothing")
    args = parser.parse_args()

    if args.system == "installer" and args.wait == "session":
        parser.error("the installer system has no Hyprland session")

    wait = None
    if args.wait == "ssh":
        wait = args.system
    elif args.wait == "session":
        wait = "session"
    testvm.boot(args.system, wait)
    return 0


if __name__ == "__main__":
    sys.exit(main())
