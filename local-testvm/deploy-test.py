#!/usr/bin/python3
"""Builds packages and installs them on the test system of the test vm.

A tool of the test vm, see .claude/skills/hyprdebian-test-vm. Each argument is a package directory
in packages/, which it builds with make <package> in the root of the repository, or a .deb. Of a
package directory it takes the packages that make deb-path gives, except those built for another
suite than the one of the test system, the suite of test.json. As no apt source of the test system
has the HyprDebian packages, it adds those in build/ that they depend on, also through each other,
when the test system does not have that version: from build/both and the folder of the suite,
which make all fills. It copies them all to /tmp/hd-debs on the test system and installs them
together with apt-get, also when the same version is installed. It shows the versions installed,
or the end of the output when something fails; the full output is in log/ next to this script.

    deploy-test.py hd-desktop hd-base       build both and install them
    deploy-test.py build/both/hd-x.deb      install a package that is built already
    deploy-test.py --no-build hd-desktop    install what packages/hd-desktop built last
    deploy-test.py --no-deps hd-desktop     without the packages of build/ it depends on
    deploy-test.py --update hd-desktop      run apt-get update first
"""

import argparse
import glob
import os
import re
import shlex
import subprocess
import sys

import testvm

TARGET = "/tmp/hd-debs"


def run(args, what):
    """Runs a command in the root of the repository and returns its output; exits with the end of
    the output when it fails."""
    result = subprocess.run(args, cwd=testvm.ROOT, capture_output=True, text=True)
    if result.returncode != 0:
        path = testvm.save_log(what, result.stdout + result.stderr)
        sys.exit(f"Error: {shlex.join(args)} failed, see {path}:\n"
                 f"{testvm.tail(result.stdout + result.stderr)}")
    return result.stdout


def field(path, name):
    """A field of the control file of a package."""
    return run(["dpkg-deb", "-f", path, name], f"dpkg-deb-{os.path.basename(path)}").strip()


def debs(directory, suite):
    """The packages of the package directory for the suite."""
    found = []
    out = run(["make", "-s", "--no-print-directory", "-C", f"packages/{directory}", "deb-path"],
              f"deb-path-{directory}")
    for entry in out.split():
        repos, _, path = entry.rpartition(":")
        if repos and suite not in repos.split(","):
            continue
        found.append(os.path.join(testvm.ROOT, "packages", directory, path))
    return found


def installed():
    """The packages installed on the test system, with their versions."""
    result = testvm.run_test("dpkg-query -W -f '${Package} ${Version} ${db:Status-Status}\\n'",
                             timeout=60)
    if result.returncode != 0:
        sys.exit(f"Error: test does not answer: {testvm.tail(result.stderr)}")
    versions = {}
    for line in result.stdout.decode().splitlines():
        package, version, status = line.split(" ", 2)
        if status == "installed":
            versions[package] = version
    return versions


def local_deps(files, suite):
    """The packages in build/ that the packages in files depend on, also through each other, and
    that the test system does not have in that version."""
    built = {}
    for folder in ("both", suite):
        for path in glob.glob(os.path.join(testvm.ROOT, "build", folder, "*.deb")):
            built[os.path.basename(path).split("_")[0]] = path
    versions = installed()

    seen = {field(path, "Package") for path in files}
    todo = list(files)
    deps = []
    while todo:
        path = todo.pop()
        relations = field(path, "Pre-Depends") + "," + field(path, "Depends")
        for name in re.findall(r"(?:^|[,|])\s*([a-z0-9][a-z0-9+.-]*)", relations):
            if name in seen or name not in built:
                continue
            seen.add(name)
            dep = built[name]
            if versions.get(name) == field(dep, "Version"):
                continue
            deps.append(dep)
            todo.append(dep)
    return deps


def main():
    parser = argparse.ArgumentParser(description="Builds packages and installs them on the test "
                                                 "system.")
    parser.add_argument("packages", nargs="+", help="package directories in packages/, or .debs")
    parser.add_argument("--no-build", action="store_true", help="do not build them first")
    parser.add_argument("--no-deps", action="store_true",
                        help="do not add the packages of build/ they depend on")
    parser.add_argument("--update", action="store_true", help="run apt-get update first")
    args = parser.parse_args()

    suite = testvm.answers()["suite"]
    files = []
    for package in args.packages:
        if package.endswith(".deb"):
            files.append(os.path.abspath(package))
            continue
        if not os.path.isdir(os.path.join(testvm.ROOT, "packages", package)):
            sys.exit(f"Error: {package} is no .deb and no directory in packages/.")
        if not args.no_build:
            print(f"Building {package}.", flush=True)
            run(["make", package], f"make-{package}")
        files += debs(package, suite)
    for path in files:
        if not os.path.isfile(path):
            sys.exit(f"Error: {path}: there is no such package.")
    if not args.no_deps:
        files += local_deps(files, suite)

    result = testvm.run_test(f"rm -rf {TARGET} && mkdir {TARGET}", timeout=60)
    if result.returncode != 0:
        sys.exit(f"Error: test does not answer: {testvm.tail(result.stderr)}")
    testvm.scp_to_test(files, TARGET)

    names = [field(path, "Package") for path in files]
    remote = [f"{TARGET}/{os.path.basename(path)}" for path in files]
    command = ("sudo env DEBIAN_FRONTEND=noninteractive apt-get install -y -q --reinstall "
               + shlex.join(remote))
    if args.update:
        command = "sudo apt-get update -q && " + command
    print(f"Installing {', '.join(names)}.", flush=True)
    result = testvm.run_test(command, sudo=True)
    output = (result.stdout + result.stderr).decode(errors="replace")
    path = testvm.save_log("deploy", output)
    if result.returncode != 0:
        sys.exit(f"Error: apt-get failed, see {path}:\n{testvm.tail(output)}")

    for line in output.splitlines():
        if line.startswith(("W:", "E:", "dpkg: warning")):
            print(line)
    versions = testvm.run_test("dpkg-query -W " + shlex.join(names))
    print(versions.stdout.decode(), end="")
    return 0


if __name__ == "__main__":
    sys.exit(main())
