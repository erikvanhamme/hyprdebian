#!/usr/bin/python3
"""Shows what each package costs on the system it runs on. A tool of the test vm, see
.claude/skills/hyprdebian-test-vm.

The roots are the packages marked as installed by hand and the Essential and required ones. For
each dependency of the package given (hd-base by default), it shows how many packages and how much
space would go when that dependency were dropped from it: the packages that are only reachable
through it, by Depends and Pre-Depends, with an alternative met by the first installed one. It also
lists the largest installed packages, each with a package that pulls it in.

    dep-weights.py [package] [--drop a,b,c ...]   also: what goes when these deps go together
"""

import collections
import re
import subprocess
import sys

FIELDS = ["Package", "Status", "Essential", "Priority", "Installed-Size", "Provides", "Depends",
          "Pre-Depends"]


def read_status():
    fmt = "\t".join(f"${{{f}}}" for f in FIELDS) + "\n"
    out = subprocess.run(["dpkg-query", "-W", "-f", fmt], capture_output=True, text=True,
                         check=True).stdout
    pkgs = {}
    for line in out.splitlines():
        rec = dict(zip(FIELDS, line.split("\t")))
        if rec["Status"] == "install ok installed":
            pkgs[rec["Package"]] = rec
    return pkgs


def names(field):
    """The groups of a dependency field, each a list of the names of its alternatives."""
    groups = []
    for group in filter(None, (g.strip() for g in field.split(","))):
        groups.append([re.split(r"[\s(:]", alt.strip())[0] for alt in group.split("|")])
    return groups


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--drop")]
    target = args[0] if args else "hd-base"
    drops = [a.split("=", 1)[1].split(",") for a in sys.argv[1:] if a.startswith("--drop=")]
    pkgs = read_status()
    providers = collections.defaultdict(list)
    for name, rec in pkgs.items():
        providers[name].append(name)
        for group in names(rec["Provides"]):
            providers[group[0]].append(name)

    def deps(name):
        rec = pkgs[name]
        result = []
        for group in names(rec["Depends"] + "," + rec["Pre-Depends"]):
            for alt in group:
                if providers.get(alt):
                    result.append(providers[alt][0])
                    break
        return result

    manual = set(subprocess.run(["apt-mark", "showmanual"], capture_output=True, text=True,
                                check=True).stdout.split()) & set(pkgs)
    base = {n for n, r in pkgs.items() if r["Essential"] == "yes" or r["Priority"] == "required"}

    def reach(roots, skip=frozenset(), drop_edges=()):
        seen, todo = set(), list(roots)
        while todo:
            n = todo.pop()
            if n in seen or n in skip:
                continue
            seen.add(n)
            for d in deps(n):
                if (n, d) not in drop_edges:
                    todo.append(d)
        return seen

    size = lambda ns: sum(int(pkgs[n]["Installed-Size"] or 0) for n in ns) / 1024
    roots = manual | base
    everything = reach(roots)
    print(f"{len(pkgs)} installed, {size(pkgs):.0f} MiB; roots: {len(manual)} manual, "
          f"{len(base)} essential or required\n")
    print(f"{'dependency of ' + target:40} {'pkgs':>5} {'MiB':>8}  (gone when dropped)")
    rows = []
    for d in sorted(set(deps(target))):
        without = reach(roots - {d}, drop_edges={(target, d)})
        gone = everything - without
        rows.append((size(gone), len(gone), d, gone))
    for mib, count, d, gone in sorted(rows, reverse=True):
        print(f"{d:40} {count:5} {mib:8.1f}")

    for group in drops:
        without = reach(roots - set(group), drop_edges={(target, d) for d in group})
        gone = everything - without
        print(f"\ndropping {'+'.join(group)} together: {len(gone)} packages, {size(gone):.1f} MiB: "
              + " ".join(sorted(gone, key=lambda n: -int(pkgs[n]['Installed-Size'] or 0))[:25]))
    if drops:
        return
    print("\nmanual packages other than the target:", " ".join(sorted(manual - {target})))
    print("\nlargest installed packages, with one package that depends on each:")
    rdeps = collections.defaultdict(list)
    for n in pkgs:
        for d in deps(n):
            rdeps[d].append(n)
    for n in sorted(pkgs, key=lambda n: -int(pkgs[n]["Installed-Size"] or 0))[:40]:
        why = "manual" if n in manual else ("required" if n in base else
                                           ", ".join(sorted(rdeps[n])[:3]))
        print(f"{int(pkgs[n]['Installed-Size'] or 0) / 1024:8.1f} MiB  {n:34} <- {why}")


if __name__ == "__main__":
    main()
