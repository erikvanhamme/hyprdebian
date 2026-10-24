#!/usr/bin/python3
"""Lists, on the system it runs on, the Recommends of installed packages that are not installed,
with the packages that recommend them. Alternatives count as met when one of them is installed.
A tool of the test vm, see .claude/skills/hyprdebian-test-vm."""
import collections
import re
import subprocess

out = subprocess.run(["dpkg-query", "-W", "-f", "${Package}\t${Status}\t${Provides}\t${Recommends}\n"],
                     capture_output=True, text=True, check=True).stdout
installed, recommends = set(), {}
for line in out.splitlines():
    pkg, status, provides, recs = (line.split("\t") + ["", "", ""])[:4]
    if status != "install ok installed":
        continue
    installed.add(pkg)
    for p in provides.split(","):
        if p.strip():
            installed.add(p.split()[0])
    recommends[pkg] = recs

missing = collections.defaultdict(list)
for pkg, recs in recommends.items():
    for group in filter(None, (g.strip() for g in recs.split(","))):
        names = [re.split(r"[\s(:]", alt.strip())[0] for alt in group.split("|")]
        if not any(name in installed for name in names):
            missing[group].append(pkg)

for group in sorted(missing):
    print(f"{group}\t<- {', '.join(sorted(missing[group]))}")
