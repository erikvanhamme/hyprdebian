#!/usr/bin/python3
"""Uploads the HyprDebian packages to a HyprDebian package repository, each for its suites.

Each argument is a package with the suites it goes to, as <suites>:<path>, the suites separated by
commas, such as trixie,unstable:build/hd-base_1.8_all.deb. It asks for the host name of the
repository, copies the packages to the inboxes of their suites of the user repo there,
inbox/trixie and inbox/unstable, then runs hd-repo-update there for the suites that got packages,
which moves them into the pools and signs the indices, see hd-repo(7). It logs in as repo with
ssh, so the ssh key of the user must be in the authorized_keys of repo on that host.

    update-repo.py PACKAGE...             ask the host name, then upload and update
    update-repo.py --dry-run PACKAGE...   ask the host name, then only print the commands
    update-repo.py --host HOST PACKAGE... upload to HOST, without asking for the host name
    update-repo.py --yes PACKAGE...       upload without asking for a go-ahead
    update-repo.py --only-new PACKAGE...  leave out the packages that are already in the pool
                                          of their suite, by file name
    update-repo.py --iso ISO PACKAGE...   also upload the live ISO to the folder iso of the
                                          repository, with its SHA256SUMS

    update-repo.py --clean                remove every package of every suite, the packages in
                                          the inboxes and the ISO, then sign the empty indices

With --host and --yes it asks nothing, so it can run unattended, as on a build server. --clean
takes no packages; it asks to type the host name to go ahead, also with --yes, and keeps the
signing key, so the clients stay linked to the empty repository.

The ISO is copied under a temporary name, checked against its sha256 on the repository and only
then renamed into place, so downloads never get a half uploaded ISO. SHA256SUMS is written next
to it.

Run it with make update-repo in the root of the repository, which builds the packages first and
takes the suites of each package from the deploy.json of its package directory.
"""

import argparse
import hashlib
import os
import re
import shlex
import subprocess
import sys

REPO_USER = "repo"
SUITES = ["trixie", "unstable"]
# The host name that update-repo.py offers when it asks for the repository.
DEFAULT_HOST = "repo"
# A host name, a fully qualified domain name or an IPv4 address.
HOST_PATTERN = re.compile(r"^[A-Za-z0-9]([A-Za-z0-9-]*[A-Za-z0-9])?(\.[A-Za-z0-9]([A-Za-z0-9-]*[A-Za-z0-9])?)*$")
# The repository on the host, which nginx serves, see hd-repo(7).
REPO_DIR = "/srv/hd-repo"
# The folder of the live ISO in the repository.
ISO_DIR = f"{REPO_DIR}/iso"


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


def published(host, suite):
    """The file names of the packages in the pool of a suite on the repository."""
    pool = f"{REPO_DIR}/pool/{suite}/main"
    command = ["ssh", f"{REPO_USER}@{host}", f"ls -1 -- {shlex.quote(pool)}"]
    result = subprocess.run(command, capture_output=True, text=True)
    if result.returncode != 0:
        raise UpdateError(f"could not list {pool} on {host}: {result.stderr.strip()}")
    return set(result.stdout.split())


def only_new(host, per_suite):
    """The packages of each suite that are not in its pool yet, leaving out suites without any."""
    new = {}
    for suite, debs in per_suite.items():
        names = published(host, suite)
        debs = [deb for deb in debs if os.path.basename(deb) not in names]
        if debs:
            new[suite] = debs
    return new


def sha256(path):
    """The sha256 of a file, in hexadecimal."""
    with open(path, "rb") as f:
        return hashlib.file_digest(f, "sha256").hexdigest()


def iso_commands(host, iso, digest):
    """The commands that upload the ISO: make the folder, copy it, then check and rename it."""
    target = f"{REPO_USER}@{host}"
    name = os.path.basename(iso)
    upload = f".{name}.upload"
    # Checks the upload against the sha256 of the local ISO before it replaces the old one.
    publish = (f"cd {shlex.quote(ISO_DIR)}"
               f" && echo {shlex.quote(f'{digest}  {upload}')} | sha256sum -c --quiet"
               f" && mv -f -- {shlex.quote(upload)} {shlex.quote(name)}"
               f" && echo {shlex.quote(f'{digest}  {name}')} > .SHA256SUMS.upload"
               f" && mv -f .SHA256SUMS.upload SHA256SUMS")
    return [
        ["ssh", target, f"mkdir -p -- {shlex.quote(ISO_DIR)}"],
        ["scp", "-q", "--", iso, f"{target}:{ISO_DIR}/{upload}"],
        ["ssh", target, publish],
    ]


def clean_commands(host):
    """The commands that empty the repository: remove the packages of the pools and the inboxes
    and the files in the folder of the ISO, then sign the empty indices with hd-repo-update."""
    target = f"{REPO_USER}@{host}"
    debs = [f"{REPO_DIR}/pool/{suite}/main/*.deb" for suite in SUITES]
    debs += [f"inbox/{suite}/*.deb" for suite in SUITES]
    # The folder of the ISO may be a ZFS dataset of its own, so only what is in it goes.
    remove = (f"rm -f -- {' '.join(debs)}"
              f" && if [ -d {shlex.quote(ISO_DIR)} ]; then"
              f" find {shlex.quote(ISO_DIR)} -mindepth 1 -delete; fi")
    return [
        ["ssh", target, remove],
        ["ssh", target, "hd-repo-update"],
    ]


def commands(host, per_suite, iso=None, digest=None):
    """The commands of the update: an scp to the inbox of each suite, then hd-repo-update, then
    the upload of the ISO, when there is one."""
    target = f"{REPO_USER}@{host}"
    steps = [["scp", "-q", "--", *debs, f"{target}:inbox/{suite}/"] for suite, debs in per_suite.items()]
    if per_suite:
        steps.append(["ssh", target, "hd-repo-update", *per_suite])
    if iso is not None:
        steps.extend(iso_commands(host, iso, digest))
    return steps


def get_host(args):
    """The host name of the repository: the one of --host, or asked for."""
    if args.host is None:
        return ask_host()
    if not HOST_PATTERN.match(args.host):
        raise UpdateError(f"{args.host}: give a host name, such as repo or repo.example.com, "
                          "or an IP address")
    return args.host


def run_steps(steps):
    """Runs the commands one by one, and stops at the first that fails."""
    for step in steps:
        print(f"\n{shlex.join([os.path.basename(arg) if arg.endswith(('.deb', '.iso')) else arg for arg in step])}")
        if subprocess.run(step).returncode != 0:
            raise UpdateError(f"{step[0]} failed")


def clean(args):
    """Removes every package and the ISO from the repository, after the host name is typed."""
    host = get_host(args)
    steps = clean_commands(host)

    if args.dry_run:
        for step in steps:
            print(shlex.join(step))
        return 0

    counts = {suite: len(published(host, suite)) for suite in SUITES}
    print(f"\nThis removes every package and the ISO from the repository on {REPO_USER}@{host}:")
    for suite, count in counts.items():
        print(f"  {suite}: {count} packages")
    # Removing everything is not undone by a new upload, so it always asks, also with --yes.
    if ask(f"\nType {host} to empty the repository").strip() != host:
        print("Nothing was changed.")
        return 1

    run_steps(steps)
    print(f"\nThe repository on {host} is empty.")
    return 0


def main():
    parser = argparse.ArgumentParser(
        description="Uploads the HyprDebian packages to a repository, each for its suites.")
    parser.add_argument("--dry-run", action="store_true",
                        help="only print the commands, without uploading")
    parser.add_argument("--host",
                        help="the host name of the repository, instead of asking for it")
    parser.add_argument("--yes", action="store_true",
                        help="upload without asking for a go-ahead")
    parser.add_argument("--only-new", action="store_true",
                        help="leave out the packages that are already in the pool of their suite")
    parser.add_argument("--iso",
                        help="the live ISO, to upload to the folder iso of the repository")
    parser.add_argument("--clean", action="store_true",
                        help="remove every package and the ISO from the repository")
    parser.add_argument("packages", nargs="*", metavar="PACKAGE",
                        help="a package with its suites, as <suites>:<path>, "
                             "such as trixie,unstable:build/hd-base_1.8_all.deb")
    args = parser.parse_args()
    if args.clean:
        if args.packages or args.iso is not None or args.only_new:
            parser.error("--clean takes no packages, --iso or --only-new")
    elif not args.packages:
        parser.error("give the packages to upload, or --clean")

    try:
        if args.clean:
            return clean(args)
        per_suite = packages_per_suite(args.packages)
        if args.iso is not None and not os.path.isfile(args.iso):
            raise UpdateError(f"{args.iso}: there is no such ISO; build it with make iso")
        host = get_host(args)

        if args.only_new:
            per_suite = only_new(host, per_suite)
        if not per_suite and args.iso is None:
            print(f"All packages are already in the repository on {host}, nothing to upload.")
            return 0

        digest = sha256(args.iso) if args.iso is not None else None
        steps = commands(host, per_suite, args.iso, digest)

        if args.dry_run:
            for step in steps:
                print(shlex.join(step))
            return 0

        for suite, debs in per_suite.items():
            print(f"\nThe packages for {suite} on {REPO_USER}@{host}:")
            for deb in debs:
                print(f"  {os.path.basename(deb)}")
        if args.iso is not None:
            print(f"\nThe ISO for {ISO_DIR} on {REPO_USER}@{host}:")
            print(f"  {os.path.basename(args.iso)}")
        if not args.yes:
            if ask("\nUpload them and update the repository? (y/N)").lower() not in ("y", "yes"):
                print("Nothing was changed.")
                return 1

        run_steps(steps)
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
