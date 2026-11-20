#!/usr/bin/python3
"""Installs HyprDebian from scratch on the test disk of the test vm, with hd-installer.

A tool of the test vm, see .claude/skills/hyprdebian-test-vm. It boots the installer system when it
does not run, updates it with update-installer-vm.py, which builds every package, and puts the
answers test.json in /root/test.json there. It runs hd-installer as the unit hd-test-install, so it
survives a lost ssh connection, with stdin closed so that a question that the answers do not give
fails instead of waiting, and waits until it is done. Then it boots the test system and puts the key
of the user on it, and snapshots the test disk on the host with zvol-snapshot.py as clean: the
system as hd-installer leaves it. Then it installs hd-desktop with deploy-test.py, reboots, waits
for the Hyprland session and snapshots the test disk as clean-desktop. A test starts from either in
about 20 seconds: zvol-snapshot.py rollback clean-desktop --boot-test --wait session. An older
snapshot of the same name is destroyed first. The output of each step is in log/ next to this
script; only its end is shown when a step fails.

    install-test.py                     install, and save the snapshots clean and clean-desktop
    install-test.py --no-update         without updating the installer system first
    install-test.py --no-desktop        without hd-desktop, only the snapshot clean
    install-test.py --snapshot <name>   name the snapshots <name> and <name>-desktop
"""

import argparse
import os
import shlex
import subprocess
import sys
import time

import testvm

INSTALLER = testvm.HOSTS["installer"]
UNIT = "hd-test-install"
LOG = "/root/install.log"
INSTALL_SECONDS = 1800


def on_installer(command, stdin=None):
    """Runs a command on the installer system and returns the CompletedProcess, with the output as
    text."""
    try:
        return subprocess.run(["ssh", "-o", "ConnectTimeout=5", INSTALLER, command], input=stdin,
                              capture_output=True, text=True, timeout=60)
    except subprocess.TimeoutExpired:
        return subprocess.CompletedProcess(command, 255, "", "timed out")


def check(result, what):
    if result.returncode != 0:
        sys.exit(f"Error: {what} failed: {testvm.tail(result.stdout + result.stderr)}")


def update():
    print("Updating the installer system, with make all.", flush=True)
    result = subprocess.run([sys.executable, os.path.join(testvm.HERE, "update-installer-vm.py")],
                            capture_output=True, text=True)
    path = testvm.save_log("update-installer-vm", result.stdout + result.stderr)
    if result.returncode != 0:
        sys.exit(f"Error: update-installer-vm.py failed, see {path}:\n"
                 f"{testvm.tail(result.stdout + result.stderr)}")


def install():
    """Runs hd-installer on the installer system and waits for its end."""
    with open(testvm.ANSWERS) as f:
        check(on_installer("sudo sh -c 'umask 077; cat > /root/test.json'", f.read()),
              "copying the answers")

    inner = f"hd-installer --answers /root/test.json > {LOG} 2>&1; echo EXIT=$? >> {LOG}"
    start = (f"sudo systemctl reset-failed {UNIT} 2>/dev/null; "
             f"sudo rm -rf /tmp/hd-installer {LOG}; "
             f"sudo systemd-run --quiet --unit={UNIT} --property=StandardInput=null "
             f"--setenv=TERM=dumb bash -c {shlex.quote(inner)}")
    check(on_installer(start), "starting hd-installer")
    print("hd-installer runs.", flush=True)

    begin = time.monotonic()
    while True:
        time.sleep(10)
        result = on_installer(f"sudo sed -n 's/^EXIT=//p' {LOG}; "
                              f"if systemctl is-active {UNIT} > /dev/null; then echo ACTIVE; fi")
        lines = result.stdout.split()
        if result.returncode == 0 and lines and lines[0] != "ACTIVE":
            code = lines[0]
            break
        if result.returncode == 0 and not lines:
            code = "none, the unit stopped"
            break
        if time.monotonic() - begin > INSTALL_SECONDS:
            sys.exit(f"Error: hd-installer did not end in {INSTALL_SECONDS} seconds.")

    log = on_installer(f"sudo cat {LOG}")
    path = testvm.save_log("install", log.stdout)
    minutes = (time.monotonic() - begin) / 60
    if code != "0":
        sys.exit(f"Error: hd-installer failed with exit code {code}, see {path}:\n"
                 f"{testvm.tail(log.stdout)}")
    print(f"hd-installer is done after {minutes:.1f} minutes.", flush=True)


def put_key(key):
    with open(key, "rb") as f:
        result = testvm.run_test(
            "mkdir -p ~/.ssh && chmod 700 ~/.ssh && cat >> ~/.ssh/authorized_keys", stdin=f.read(),
            timeout=60)
    if result.returncode != 0:
        sys.exit(f"Error: putting the key on test failed: {testvm.tail(result.stderr)}")


def tool(*args):
    """Runs another tool of the test vm; exits when it fails, which it reported already."""
    if subprocess.run([sys.executable, os.path.join(testvm.HERE, args[0]), *args[1:]]).returncode:
        sys.exit(1)


def snapshot(name, wait):
    """Snapshots the test disk with zvol-snapshot.py, after it destroyed an older one of the same
    name, then boots the test system again and waits for wait, ssh or session."""
    listing = subprocess.run(["zfs", "list", "-H", "-t", "snapshot", "-o", "name",
                              f"rpool/vmdk/test@{name}"], capture_output=True)
    if listing.returncode == 0:
        tool("zvol-snapshot.py", "destroy", name)
    tool("zvol-snapshot.py", "save", name, "--boot-test", "--wait", wait)


def main():
    parser = argparse.ArgumentParser(description="Installs HyprDebian from scratch on the test "
                                                 "disk of the test vm.")
    parser.add_argument("--no-update", action="store_true",
                        help="do not update the installer system first")
    parser.add_argument("--no-desktop", action="store_true", help="do not install hd-desktop")
    parser.add_argument("--snapshot", default="clean",
                        help="the name of the snapshot of the test disk afterwards (clean)")
    parser.add_argument("--key", default=os.path.expanduser("~/.ssh/id_rsa.pub"),
                        help="the public key to put on the test system (~/.ssh/id_rsa.pub)")
    args = parser.parse_args()

    if not testvm.NAME_PATTERN.match(args.snapshot):
        parser.error("give a snapshot name of lower case letters, digits, - and _")

    if not testvm.ready("installer"):
        testvm.boot("installer", "installer")
    if not args.no_update:
        update()
    install()
    testvm.boot("test", "test")
    put_key(args.key)
    snapshot(args.snapshot, "ssh")
    if not args.no_desktop:
        tool("deploy-test.py", "--no-build", "hd-desktop")
        testvm.boot("test", "session")
        snapshot(f"{args.snapshot}-desktop", "session")
    print("The test system is installed.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
