#!/usr/bin/python3
"""Waits until a system of the test vm is ready.

A tool of the test vm, see .claude/skills/hyprdebian-test-vm.

    wait-test.py installer      until ssh test-installer works
    wait-test.py test           until test answers ssh, also when its key is not there yet
    wait-test.py session        until the Hyprland session of test answers hyprctl
    ... --timeout <seconds>     the longest wait; 180 seconds for ssh, 300 for the session
"""

import argparse
import sys

import testvm


def main():
    parser = argparse.ArgumentParser(description="Waits until a system of the test vm is ready.")
    parser.add_argument("what", choices=testvm.WAITS, help="what to wait for")
    parser.add_argument("--timeout", type=int, help="the longest wait in seconds")
    args = parser.parse_args()

    testvm.wait_for(args.what, args.timeout)
    return 0


if __name__ == "__main__":
    sys.exit(main())
