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

The version of HyprDebian is in `version.json` in the root of the repository (`{"major": 1, "minor": N}`). It is also the version of the package `hd-base`, which installs a copy as `/etc/hyprdebian/version.json`; the login banner and the message after a login show it.

- **Bump it when a change affects a package or the installer.** That covers every `hd-*` package folder, `hd-installer/` among them, and anything else that ends up in a package or on an installed system.
- **Bump it once per set of working changes, not once per edit.** Before finishing a task, run `git diff HEAD -- version.json`. If it already shows an uncommitted bump, or the file is not committed yet, leave it; otherwise bump. Never assume from earlier in the session: the owner may have committed in the meantime.
- **Only increment the minor number** (e.g. `1.4` → `1.5`). Never change the major number; that decision belongs to the owner.

Every other `hd-*` package also has a `version.json` of its own, in its folder, with the version of that package. Bump its minor number when that package changes, once per set of working changes, in the same way. `hd-base` has none: its version is the one in the root.
