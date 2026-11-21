# Agent guidelines for hyprdebian

This file contains the rules for AI agents and other automated helpers working in this repository.

## Roles

The project owner (Erik Van Hamme) has final say on all changes. Agents are helpers: they propose and make changes in the working tree, and the owner reviews and accepts them.

## Commits

- **Agents never commit.** Do not run `git commit`, `git commit --amend`, `git merge`, `git rebase`, `git cherry-pick`, `git revert` or anything else that creates or rewrites commits.
- **Agents never push.** Do not run `git push` or open pull requests.
- **Leave changes in the working tree.** Do not stage files with `git add` unless the owner asks. The owner reviews the changes and commits them.
- **Do not alter history or discard work.** No `git reset`, `git checkout -- <file>`, `git restore`, `git stash` or `git clean` on changes the agent did not make itself in the current task.
- **Report what changed.** When a task is done, list the files that were changed and give a short summary, so the owner can review the diff.
- Read-only git commands (`git status`, `git diff`, `git log`, `git show`, `git blame`) are always allowed.

## Versions

The version of HyprDebian is in `version.json` in the root of the repository (`{"major": 1, "minor": N}`). It is also the version of the packages `hd-base` and `hd-desktop`. `hd-base` installs a copy as `/etc/hyprdebian/version.json`; the login banner and the message after a login show it. `hd-desktop` depends on `hd-base` of exactly the same version.

- **Bump it when a change affects a package or the installer.** That covers every package folder in `packages/`, `packages/hd-installer/` among them, and anything else that ends up in a package or on an installed system.
- **Bump it once per set of working changes, not once per edit.** Before finishing a task, run `git diff HEAD -- version.json`. If it already shows an uncommitted bump, or the file is not committed yet, leave it; otherwise bump. Never assume from earlier in the session: the owner may have committed in the meantime.
- **Only increment the minor number** (e.g. `1.4` → `1.5`). Never change the major number; that decision belongs to the owner.

Every other `hd-*` package also has a `version.json` of its own, in its folder in `packages/`, with the version of that package. Bump its minor number when that package changes, once per set of working changes, in the same way. `hd-base` and `hd-desktop` have none: their version is the one in the root.

## What else a change touches

Finish a change with every one of these that applies:

- The version bumps of `Versions` above: the root `version.json` and the `version.json` of the package directory.
- The doc page in `docs/packages/` and the man page in `man/` of the package directory that describe what changed.
- The `Depends` of `packages/hd-base/deb/control` when every system needs a new package.
- A new build dependency in `buildenv/Dockerfile`, in the layer of the suite the package is built on; it also holds those of the containers of `make iso` and hd-buildserver. Nothing compiles on the host, so the host gets no new compiler or -dev package.
- A new runtime dependency of a binary that `make test` or a tryout compiles in a build image and runs on the host in the list of what the host needs to run them in `docs/HyprDebian/Build requirements.md`.
- `make test` in the package directory when it has `tests/`, and new tests there for new behaviour.

A new package directory also needs:

- An entry in `PACKAGES` (or `KERNEL_PACKAGES`) of the root `Makefile`, and in `TESTS` when it has a `test` target.
- `deploy.json`, `version.json`, a `.gitignore` with `build`, a doc page and a man page.

## Agent skills

### Running the systems

`.claude/skills/hyprdebian` tells which command does what on an installed system, which man page to read, and which commands only the owner may run. Update it in the same change that adds, removes or renames a command, or changes its tier. Options and behaviour belong in the man page, not in the skill.

### Issue tracker

Issues are tracked as local markdown files under `.scratch/<feature>/`. See `docs/agents/issue-tracker.md`.

### Triage labels

The five default triage labels are used as-is (`needs-triage`, `needs-info`, `ready-for-agent`, `ready-for-human`, `wontfix`). See `docs/agents/triage-labels.md`.

### Domain docs

Single-context: one `GLOSSARY.md` and `docs/adr/` at the repo root. See `docs/agents/domain.md`.
