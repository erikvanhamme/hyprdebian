#!/usr/bin/env python3

import gzip
import os
import re
import requests
import argparse

BASE_URL = "http://deb.debian.org/debian/"

TARGET_PATTERNS = [
    r"^linux-image-",
    r"^linux-headers-.*-amd64",
    r"^linux-headers-.*-common",
    r"^linux-kbuild-",
    r"^linux-base-",
    r"^linux-modules-",
    r"^linux-binary-"
]

EXCLUDE_PATTERNS = [
    r"cloud",
    r"rt",
    r"unsigned",
    r"dbg"
]

def build_repo_url(distribution):
    return f"{BASE_URL}dists/{distribution}/main/binary-amd64/Packages.gz"

def fetch_index(repo_url):
    print(f"Downloading Packages.gz from {repo_url}...")
    r = requests.get(repo_url)
    r.raise_for_status()
    return gzip.decompress(r.content).decode("utf-8", errors="ignore")

def parse_packages(text):
    packages = []
    current = {}

    for line in text.splitlines():
        if not line.strip():
            if current:
                packages.append(current)
                current = {}
            continue

        if ":" in line:
            key, value = line.split(":", 1)
            current[key.strip()] = value.strip()

    return packages

def is_excluded(name):
    return any(re.search(p, name) for p in EXCLUDE_PATTERNS)

def extract_kernel_versions(packages):
    versions = set()

    for pkg in packages:
        name = pkg.get("Package", "")

        if not name.startswith("linux-image-"):
            continue

        if is_excluded(name):
            continue

        match = re.search(r"linux-image-(\d+\.\d+\.\d+)", name)
        if match:
            versions.add(match.group(1))

    return sorted(versions)

def matches(pkg_name, kernel_version):
    if kernel_version not in pkg_name:
        return False

    if is_excluded(pkg_name):
        return False

    for pattern in TARGET_PATTERNS:
        if re.search(pattern, pkg_name):
            return True

    return False

def download(url, path):
    print(f"Downloading {os.path.basename(path)}")
    r = requests.get(url, stream=True)
    r.raise_for_status()

    with open(path, "wb") as f:
        for chunk in r.iter_content(8192):
            f.write(chunk)

def main():
    parser = argparse.ArgumentParser(description="Download Debian kernel packages")
    parser.add_argument(
        "distribution",
        help="Debian distribution (stable, testing, unstable, etc.)"
    )

    args = parser.parse_args()
    repo_url = build_repo_url(args.distribution)

    index_text = fetch_index(repo_url)
    packages = parse_packages(index_text)

    versions = extract_kernel_versions(packages)

    if not versions:
        print("No kernel versions found.")
        return

    print("\nAvailable kernel versions (filtered):\n")
    for i, v in enumerate(versions):
        print(f"{i}: {v}")

    choice = int(input("\nSelect a version by number: "))
    kernel_version = versions[choice]

    print(f"\nSelected kernel: {kernel_version}\n")

    selected = []

    for pkg in packages:
        name = pkg.get("Package")
        filename = pkg.get("Filename")

        if not name or not filename:
            continue

        if matches(name, kernel_version):
            selected.append((name, filename))

    if not selected:
        print("No matching packages found.")
        return

    os.makedirs("kernels", exist_ok=True)

    for name, filename in selected:
        url = BASE_URL + filename
        path = os.path.join("kernels", os.path.basename(filename))
        download(url, path)

    print(f"\nDownloaded {len(selected)} packages.")

if __name__ == "__main__":
    main()
