#!/usr/bin/python3
"""Uploads the HyprDebian packages to a HyprDebian package repository, for trixie and unstable.

It asks for the host name of the repository, copies every package in packages/ in the root of the
repository to the inboxes of both suites of the user repo there, inbox/trixie and inbox/unstable,
then runs hd-repo-update there, which moves them into the pools and signs the indices, see
hd-repo(7). It logs in as repo with ssh, so the ssh key of the user must be in the
authorized_keys of repo on that host.

    update-repo.py             ask the host name, then upload and update
    update-repo.py --dry-run   ask the host name, then only print the commands

Run it with make update-repo in the root of the repository, which builds the packages first.
"""

import argparse
import glob
import os
import re
import shlex
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
PACKAGES_DIR = os.path.join(os.path.dirname(HERE), "packages")
REPO_USER = "repo"
SUITES = ["trixie", "unstable"]
# The host name of the repository in the default URL of hd-installer, http://repo/.
DEFAULT_HOST = "repo"
# A host name, a fully qualified domain name or an IPv4 address.
HOST_PATTERN = re.compile(r"^[A-Za-z0-9]([A-Za-z0-9-]*[A-Za-z0-9])?(\.[A-Za-z0-9]([A-Za-z0-9-]*[A-Za-z0-9])?)*$")


class UpdateError(Exception):
    """A problem that stops the update, with the message for the user."""


def ask(prompt, default=None):
    """Asks for a line of text; an empty answer gives the default."""
    suffix = f" [{default}]" if default is not None else ""
    answer = input(f"{prompt}{suffix}: ").strip()
    if not answer and default is not None:
        return str(default)
    return answer


def ask_host():
    """Asks for the host name of the repository."""
    while True:
        host = ask("Host name of the repository", DEFAULT_HOST)
        if HOST_PATTERN.match(host):
            return host
        print("Give a host name, such as repo or repo.example.com, or an IP address.")


def packages():
    """The packages to upload, from packages/."""
    debs = sorted(glob.glob(os.path.join(PACKAGES_DIR, "*.deb")))
    if not debs:
        raise UpdateError(f"there are no packages in {PACKAGES_DIR}; build them with make all")
    return debs


def commands(host, debs):
    """The commands of the update: an scp to the inbox of each suite, then hd-repo-update."""
    target = f"{REPO_USER}@{host}"
    steps = [["scp", "-q", "--", *debs, f"{target}:inbox/{suite}/"] for suite in SUITES]
    steps.append(["ssh", target, "hd-repo-update", *SUITES])
    return steps


def main():
    parser = argparse.ArgumentParser(
        description="Uploads the HyprDebian packages to a repository, for trixie and unstable.")
    parser.add_argument("--dry-run", action="store_true",
                        help="only print the commands, without uploading")
    args = parser.parse_args()

    try:
        debs = packages()
        host = ask_host()
        steps = commands(host, debs)

        if args.dry_run:
            for step in steps:
                print(shlex.join(step))
            return 0

        print(f"\nThe packages for {', '.join(SUITES)} on {REPO_USER}@{host}:")
        for deb in debs:
            print(f"  {os.path.basename(deb)}")
        if ask("\nUpload them and update the repository? (y/N)").lower() not in ("y", "yes"):
            print("Nothing was changed.")
            return 1

        for step in steps:
            print(f"\n{shlex.join([os.path.basename(arg) if arg in debs else arg for arg in step])}")
            if subprocess.run(step).returncode != 0:
                raise UpdateError(f"{step[0]} failed")
    except UpdateError as e:
        print(f"Error: {e}", file=sys.stderr)
        return 1
    except (KeyboardInterrupt, EOFError):
        print("\nStopped.")
        return 1

    print(f"\nThe repository on {host} is updated.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
