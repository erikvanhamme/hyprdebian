#!/usr/bin/python3
"""Updates the installer vm: a plain Debian system that installs HyprDebian on another disk.

A tool of the test vm, see .claude/skills/hyprdebian-test-vm. It runs make all in the root of
the repository, then takes every package that make list-debs gives, with the suites it goes to.
It builds the local repository of the packages in a temporary folder, with the same layout as
that of the live cd:
each package in pool/<suite> of every suite it goes to, and the indices of each suite with an
unsigned Release file. It replaces /srv/hyprdebian/repo on the installer vm with it, copied as a
tar stream over ssh, then installs hd-installer from it there. It logs in with ssh as the user, who must have sudo
without a password on the vm.

    update-installer-vm.py                  update the installer vm test-installer
    update-installer-vm.py --host <host>    update another installer vm
    update-installer-vm.py --dry-run        build the repository, then only print the commands
"""

import argparse
import os
import shlex
import shutil
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SUITES = ["trixie", "unstable"]
DEFAULT_HOST = "test-installer"
# The local repository on the vm. hd-installer has the same path in LIVE_REPO_DIR of its
# constants.sh.
REPO_DIR = "/srv/hyprdebian/repo"
INSTALLER = "hd-installer"


class UpdateError(Exception):
    """A problem that stops the update, with the message for the user."""


def packages():
    """Every package that make list-debs gives, as a list of (suites, path in build/): the repos
    it goes to and where make all put it, in build/both, build/trixie or build/unstable."""
    found = []
    for entry in run(["make", "-s", "--no-print-directory", "list-debs"], cwd=ROOT).split():
        repos, _, deb = entry.partition(":")
        suites = repos.split(",")
        for suite in suites:
            if suite not in SUITES:
                raise UpdateError(f"{deb}: unknown suite {suite}")
        path = os.path.join(ROOT, deb)
        if not os.path.isfile(path):
            raise UpdateError(f"{path}: there is no such package; build it with make all")
        found.append((suites, path))
    return found


def run(args, cwd=None):
    """Runs a command and returns its output; raises UpdateError when it fails."""
    try:
        result = subprocess.run(args, cwd=cwd, capture_output=True, text=True)
    except OSError as e:
        raise UpdateError(f"cannot run {args[0]}: {e.strerror}")
    if result.returncode != 0:
        raise UpdateError(f"{shlex.join(args)} failed: {result.stderr.strip()}")
    return result.stdout


def build_repo(repo, debs):
    """Builds the repository in the folder repo, and returns the name of the package of
    hd-installer and a suite it is in."""
    # A temporary folder is only readable by its owner; tar would hand that on to the repository,
    # which apt reads as the user _apt.
    os.chmod(repo, 0o755)

    installer = None
    for suites, deb in debs:
        for suite in suites:
            pool = os.path.join(repo, "pool", suite)
            os.makedirs(pool, exist_ok=True)
            shutil.copy(deb, pool)
        if os.path.basename(deb).startswith(f"{INSTALLER}_"):
            installer = os.path.basename(deb), suites[0]

    if installer is None:
        raise UpdateError(f"there is no package {INSTALLER}")

    for suite in sorted(os.listdir(os.path.join(repo, "pool"))):
        dists = os.path.join(repo, "dists", suite)
        os.makedirs(os.path.join(dists, "main", "binary-amd64"))
        with open(os.path.join(dists, "main", "binary-amd64", "Packages"), "w") as f:
            f.write(run(["apt-ftparchive", "packages", f"pool/{suite}"], cwd=repo))
        release = run(["apt-ftparchive",
                       "-o", "APT::FTPArchive::Release::Origin=HyprDebian",
                       "-o", "APT::FTPArchive::Release::Label=HyprDebian installer vm",
                       "-o", f"APT::FTPArchive::Release::Suite={suite}",
                       "-o", f"APT::FTPArchive::Release::Codename={suite}",
                       "-o", "APT::FTPArchive::Release::Components=main",
                       "-o", "APT::FTPArchive::Release::Architectures=amd64",
                       "release", "."], cwd=dists)
        with open(os.path.join(dists, "Release"), "w") as f:
            f.write(release)

    return installer


def copy_command(host, repo):
    """The command that replaces the repository on the vm: tar packs it here and unpacks it there,
    owned by root."""
    unpack = (f"sudo rm -rf {REPO_DIR} && sudo mkdir -p {REPO_DIR} && "
              f"sudo tar -C {REPO_DIR} --no-same-owner -xf -")
    return f"tar -C {shlex.quote(repo)} -cf - . | ssh {shlex.quote(host)} {shlex.quote(unpack)}"


def install_command(host, installer):
    """The command that installs hd-installer from the repository on the vm. --reinstall installs
    it also when the version is the same, as it is while it is worked on."""
    deb, suite = installer
    return shlex.join(["ssh", host, "sudo", "DEBIAN_FRONTEND=noninteractive", "apt-get", "install",
                       "-qq", "-y", "--reinstall", f"{REPO_DIR}/pool/{suite}/{deb}"])


def main():
    parser = argparse.ArgumentParser(
        description="Builds every HyprDebian package, copies them to the local repository of the "
                    "installer vm, and installs hd-installer there.")
    parser.add_argument("--host", default=DEFAULT_HOST,
                        help=f"the host name of the installer vm (default: {DEFAULT_HOST})")
    parser.add_argument("--dry-run", action="store_true",
                        help="build the repository, then only print the commands")
    args = parser.parse_args()

    try:
        if subprocess.run(["make", "-C", ROOT, "all"]).returncode != 0:
            raise UpdateError("make all failed")
        with tempfile.TemporaryDirectory() as repo:
            installer = build_repo(repo, packages())
            for step in (copy_command(args.host, repo), install_command(args.host, installer)):
                print(step)
                if args.dry_run:
                    continue
                # pipefail, so a failing tar on this side fails the copy too.
                if subprocess.run(["bash", "-o", "pipefail", "-c", step]).returncode != 0:
                    raise UpdateError(f"{step} failed")
    except UpdateError as e:
        print(f"Error: {e}", file=sys.stderr)
        return 1
    except KeyboardInterrupt:
        print("\nStopped.")
        return 1

    if not args.dry_run:
        print(f"\nThe installer vm {args.host} is updated.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
