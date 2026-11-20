"""The parts that the tools of the test vm share, see .claude/skills/hyprdebian-test-vm.

The vm debian13 has the installer system on the disk with serial vmdk-installer, host
test-installer, and the HyprDebian test system on the disk with serial vmdk-test, host test. This
module starts and stops the vm, boots it into one of the two, waits until a system answers, and runs
commands on the test system with ssh: with sudo, and in the Hyprland session.
"""

import io
import json
import os
import re
import shlex
import subprocess
import sys
import time
import xml.etree.ElementTree as ET

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
URI = "qemu:///system"
DOMAIN = "debian13"
SERIALS = {"installer": "vmdk-installer", "test": "vmdk-test"}
HOSTS = {"installer": "test-installer", "test": "test"}
SHUTDOWN_SECONDS = 120
ANSWERS = os.path.join(HERE, "test.json")
LOG_DIR = os.path.join(HERE, "log")
NAME_PATTERN = re.compile(r"^[a-z0-9][a-z0-9_-]*$")
# What wait() can wait for: ssh on the installer system, ssh on the test system, or the Hyprland
# session on the test system.
WAITS = ["installer", "test", "session"]
WAIT_SECONDS = {"installer": 180, "test": 180, "session": 300}
# Every installation gives the test system a new host key.
TEST_SSH_OPTIONS = ["-o", "StrictHostKeyChecking=no", "-o", "UserKnownHostsFile=/dev/null",
                    "-o", "LogLevel=ERROR"]
# The environment of the Hyprland session, for a command over ssh.
SESSION = ('export XDG_RUNTIME_DIR=/run/user/$(id -u); '
           'export DBUS_SESSION_BUS_ADDRESS=unix:path=$XDG_RUNTIME_DIR/bus; '
           'export WAYLAND_DISPLAY=$(ls $XDG_RUNTIME_DIR | grep -m1 -E "^wayland-[0-9]+$"); '
           'export HYPRLAND_INSTANCE_SIGNATURE='
           '$(ls -t $XDG_RUNTIME_DIR/hypr 2>/dev/null | head -1); ')
# A function sudo that gives sudo the password, which comes as the first line of stdin.
SUDO = ('read -r HD_TEST_PW; '
        'sudo() { printf "%s\\n" "$HD_TEST_PW" | command sudo -S -p "" "$@"; }; ')


def answers():
    """The answers of the test installation, test.json."""
    with open(ANSWERS) as f:
        return json.load(f)


def virsh(*args, stdin=None):
    """Runs virsh and returns its output; exits with the error of virsh when it fails."""
    result = subprocess.run(["virsh", "-c", URI, *args], input=stdin, capture_output=True,
                            text=True)
    if result.returncode != 0:
        sys.exit(f"Error: virsh {' '.join(args)} failed: {result.stderr.strip()}")
    return result.stdout


def running():
    return virsh("domstate", DOMAIN).strip() != "shut off"


def shut_down():
    """Shuts the vm down when it runs, and waits until it is off."""
    if not running():
        return
    virsh("shutdown", DOMAIN)
    for _ in range(SHUTDOWN_SECONDS):
        if not running():
            return
        time.sleep(1)
    sys.exit(f"Error: {DOMAIN} did not shut down in {SHUTDOWN_SECONDS} seconds.")


def boot_order(xml, first):
    """The domain XML with the disk of the system first in the boot order, the other second.

    Each disk has a boot order of its own, not <boot dev='hd'/> in <os>: OVMF deletes the firmware
    boot entries of the disks that are not in the boot order of QEMU, and with <boot dev='hd'/> only
    the first disk is in it, so the entry of the second disk is lost on every boot. With both disks
    in the boot order, the firmware boots the fallback path EFI/BOOT/BOOTX64.EFI of the first disk.

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


def boot(system, wait=None):
    """Shuts the vm down, gives the disk of the system boot order 1 and starts it; then waits for
    wait, one of WAITS, when it is given."""
    shut_down()
    virsh("define", "/dev/stdin", stdin=boot_order(virsh("dumpxml", "--inactive", DOMAIN), system))
    virsh("start", DOMAIN)
    print(f"{DOMAIN} boots the {system} system.", flush=True)
    if wait is not None:
        wait_for(wait)


def test_env():
    """The environment for ssh and scp to the test system: the password from askpass-test when
    the key is not there yet."""
    return dict(os.environ, SSH_ASKPASS=os.path.join(HERE, "askpass-test"),
                SSH_ASKPASS_REQUIRE="force")


def test_command(command, sudo=False, session=False):
    """The command for the shell on the test system, with the function sudo and the environment of
    the session when asked."""
    prefix = ""
    if sudo:
        prefix += SUDO
    if session:
        prefix += SESSION
    return prefix + command


def run_test(command, sudo=False, session=False, stdin=b"", capture=True, timeout=None,
             options=()):
    """Runs a command on the test system and returns the CompletedProcess, with the output as
    bytes when capture is true. With sudo, the password goes first on stdin."""
    if sudo:
        stdin = answers()["password"].encode() + b"\n" + stdin
    args = ["ssh", *TEST_SSH_OPTIONS, *options, HOSTS["test"],
            test_command(command, sudo, session)]
    try:
        return subprocess.run(args, input=stdin, capture_output=capture, env=test_env(),
                              timeout=timeout)
    except subprocess.TimeoutExpired:
        return subprocess.CompletedProcess(args, 255, b"", b"timed out")


def scp_to_test(files, target):
    """Copies the files to the folder target on the test system."""
    result = subprocess.run(["scp", "-q", *TEST_SSH_OPTIONS, *files, f"{HOSTS['test']}:{target}"],
                            env=test_env(), capture_output=True, text=True)
    if result.returncode != 0:
        sys.exit(f"Error: copying to {HOSTS['test']}:{target} failed: {result.stderr.strip()}")


def ready(what):
    """Whether the installer system or the test system answers ssh, or the Hyprland session
    answers hyprctl."""
    quick = ["-o", "ConnectTimeout=3"]
    if what == "installer":
        result = subprocess.run(["ssh", "-o", "BatchMode=yes", *quick, HOSTS["installer"], "true"],
                                capture_output=True)
        return result.returncode == 0
    if what == "test":
        # Before its key is there, a refused password-less login also means that ssh runs.
        result = run_test("true", timeout=20, options=["-o", "BatchMode=yes", *quick])
        return result.returncode == 0 or b"Permission denied" in result.stderr
    result = run_test('test -n "$HYPRLAND_INSTANCE_SIGNATURE" && hyprctl -j monitors > /dev/null',
                      session=True, timeout=20, options=quick)
    return result.returncode == 0


def wait_for(what, timeout=None):
    """Waits until ready(what), timeout seconds at most; exits when it does not get there."""
    if timeout is None:
        timeout = WAIT_SECONDS[what]
    start = time.monotonic()
    while not ready(what):
        if time.monotonic() - start > timeout:
            sys.exit(f"Error: {what} is not ready after {timeout} seconds.")
        time.sleep(2)
    print(f"{what} is ready after {round(time.monotonic() - start)} seconds.", flush=True)


def save_log(name, data):
    """Saves output in log/<name>.log and returns the path."""
    os.makedirs(LOG_DIR, exist_ok=True)
    path = os.path.join(LOG_DIR, f"{name}.log")
    with open(path, "wb") as f:
        f.write(data if isinstance(data, bytes) else data.encode())
    return path


def tail(data, lines=40):
    """The last lines of output, as text."""
    if isinstance(data, bytes):
        data = data.decode(errors="replace")
    return "\n".join(data.rstrip().splitlines()[-lines:])


def lua_string(text):
    """text as a Lua string in double quotes."""
    return '"' + text.replace("\\", "\\\\").replace('"', '\\"').replace("\n", "\\n") + '"'


def hyprland_exec(command):
    """The shell command that starts command in the Hyprland session; Hyprland 0.56 takes Lua for
    dispatch."""
    return "hyprctl dispatch " + shlex.quote(f"hl.dsp.exec_cmd({lua_string(command)})")
