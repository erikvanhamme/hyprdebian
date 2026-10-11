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
