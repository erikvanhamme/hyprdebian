#!/usr/bin/python3
"""Uploads the HyprDebian packages to a HyprDebian package repository, each for its suites.

Each argument is a package with the suites it goes to, as <suites>:<path>, the suites separated by
commas, such as trixie,unstable:packages/hd-base_1.8_all.deb. It asks for the host name of the
repository, copies the packages to the inboxes of their suites of the user repo there,
inbox/trixie and inbox/unstable, then runs hd-repo-update there for the suites that got packages,
which moves them into the pools and signs the indices, see hd-repo(7). It logs in as repo with
ssh, so the ssh key of the user must be in the authorized_keys of repo on that host.

    update-repo.py PACKAGE...             ask the host name, then upload and update
    update-repo.py --dry-run PACKAGE...   ask the host name, then only print the commands

Run it with make update-repo in the root of the repository, which builds the packages first and
takes the suites of each package from the deploy.json of its package directory.
"""

import argparse
import os
import re
import shlex
import subprocess
import sys

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


def parse_upload(arg):
    """Splits an argument <suites>:<path> into the list of suites and the path of the package."""
    suites, sep, deb = arg.partition(":")
    suites = [suite for suite in suites.split(",") if suite]
    if not sep or not deb:
        raise UpdateError(f"{arg}: give a package as <suites>:<path>")
    if not suites:
        raise UpdateError(f"{deb}: no suites for this package")
    for suite in suites:
        if suite not in SUITES:
            raise UpdateError(f"{deb}: unknown suite {suite}, give {' or '.join(SUITES)}")
    if not os.path.isfile(deb):
        raise UpdateError(f"{deb}: there is no such package; build it with make all")
    return suites, deb


def packages_per_suite(args):
    """The packages to upload for each suite, from the arguments, leaving out suites without any."""
    per_suite = {suite: [] for suite in SUITES}
    for arg in args:
        suites, deb = parse_upload(arg)
        for suite in suites:
            per_suite[suite].append(deb)
    return {suite: debs for suite, debs in per_suite.items() if debs}


def commands(host, per_suite):
    """The commands of the update: an scp to the inbox of each suite, then hd-repo-update."""
    target = f"{REPO_USER}@{host}"
    steps = [["scp", "-q", "--", *debs, f"{target}:inbox/{suite}/"] for suite, debs in per_suite.items()]
    steps.append(["ssh", target, "hd-repo-update", *per_suite])
    return steps


def main():
    parser = argparse.ArgumentParser(
        description="Uploads the HyprDebian packages to a repository, each for its suites.")
    parser.add_argument("--dry-run", action="store_true",
                        help="only print the commands, without uploading")
    parser.add_argument("packages", nargs="+", metavar="PACKAGE",
                        help="a package with its suites, as <suites>:<path>, "
                             "such as trixie,unstable:packages/hd-base_1.8_all.deb")
    args = parser.parse_args()

    try:
        per_suite = packages_per_suite(args.packages)
        host = ask_host()
        steps = commands(host, per_suite)

        if args.dry_run:
            for step in steps:
                print(shlex.join(step))
            return 0

        for suite, debs in per_suite.items():
            print(f"\nThe packages for {suite} on {REPO_USER}@{host}:")
            for deb in debs:
                print(f"  {os.path.basename(deb)}")
        if ask("\nUpload them and update the repository? (y/N)").lower() not in ("y", "yes"):
            print("Nothing was changed.")
            return 1

        for step in steps:
            print(f"\n{shlex.join([os.path.basename(arg) if arg.endswith(".deb") else arg for arg in step])}")
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
