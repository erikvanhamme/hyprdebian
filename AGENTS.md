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

## Installer version

The installer version is in the start banner in `install.sh` (`echo "=== Erik's nifty hyprdebian/debian installer v0.NNN ==="`).

- **Bump it when a change affects the installer or the systems it installs.** That covers `install.sh`, `scripts/`, `templates/`, `deploy/`, `render.py`, `kernels.py` and anything else the installer runs or copies into the target system.
- **Bump it once per set of working changes, not once per edit.** If the working tree already holds a bump that hasn't been committed, don't bump again.
- **Only increment the minor number** (e.g. `v0.129` → `v0.130`). Never change the major number; the decision to go to `v1.0` belongs to the owner.
