#!/usr/bin/python3
"""Types the password of the test system from test.json next to this script on the console of the
vm, then Enter, with virsh send-key. A tool of the test vm, see .claude/skills/hyprdebian-test-vm."""
import json
import os
import subprocess

HERE = os.path.dirname(os.path.abspath(__file__))
with open(os.path.join(HERE, "test.json")) as f:
    password = json.load(f)["password"]
for char in password + "\n":
    if char == "\n":
        keys = ["KEY_ENTER"]
    elif char.isdigit():
        keys = [f"KEY_{char}"]
    elif char.isupper():
        keys = ["KEY_LEFTSHIFT", f"KEY_{char}"]
    elif char.isalpha():
        keys = [f"KEY_{char.upper()}"]
    else:
        raise SystemExit(f"cannot type {char!r}")
    subprocess.run(["virsh", "-c", "qemu:///system", "send-key", "debian13", *keys],
                   check=True, capture_output=True)
