## Introduction
This part of the documentation describes how the hyprdebian installer is built. It is meant for anyone who wants to change or extend the installer.

The installer documentation is split into these documents:
- **Structure** (this document): the layout of the repo and what happens when the installer starts.
- [Dependencies](Dependencies.md): tasks, and how their order is decided.
- [Phases](Phases.md): the phases of the installation, and how a phase script is built.
- [Questions](Questions.md): how the user is asked for input.
- [Defaults](Defaults.md): the default answers per machine type, and how they are resolved.
- [Queues](Queues.md): how packages, files, templates, services and groups are collected and processed.
- [Files and templates](Files%20and%20templates.md): how files end up on the target system.
- [Idempotency](Idempotency.md): how answers and progress are remembered, so the installer can be started again after a failure.
- [Extending](Extending.md): where common changes to the installer go.
- [Upgrade](Upgrade.md): how an installed system is upgraded to a newer version with `upgrade.sh`.
- [Migrations](Migrations.md): the one-off changes the upgrade makes to installed systems, and what each one does.

## Overview
The installer is a set of bash scripts, started with:
```
sudo ./install.sh
```

It must be run from the root of the repo, because all paths in the scripts are relative to it.

The layout of the repo, as far as the installer is concerned:

| Path | Purpose |
| --- | --- |
| `install.sh` | Entry point. Loads the helper scripts and the phases, then starts the installation. |
| `upgrade.sh` | Entry point of the upgrade of an installed system. See [Upgrade](Upgrade.md). |
| `version.sh` | The installer version, shown in the start banner. |
| `scripts/dependencies.sh` | The task and dependency system. |
| `scripts/helpers.sh` | Helper functions: questions, queues, file deployment, template rendering, running commands in the target. |
| `scripts/config.sh` | Loading and saving the installer configuration. |
| `scripts/constants.sh` | Constants such as the target and state directories. |
| `scripts/defaults.sh` | Default answers per machine type. |
| `scripts/<phase>.sh` | One script per phase, with the tasks of that phase. `scripts/upgrade.sh` is the phase of the upgrade. |
| `scripts/upgrade_helpers.sh` | Helper functions of the upgrade, and the installer settings and functions it replaces. |
| `scripts/upgrade_detect.sh` | Works out the answers from the system, for systems installed before the installer saved its config. |
| `migrations/` | One-off changes for the upgrade of installed systems. See [Upgrade](Upgrade.md#migrations). |
| `deploy/` | Files that are copied as-is to the target system. |
| `templates/` | Jinja2 templates that are rendered to files on the target system. |
| `render.py` | Renders one Jinja2 template. |
| `kernels.py` | Downloads kernel packages, when a specific kernel is chosen. |
| `src/` | Sources that are compiled in the target system. |

The target system is mounted at `/mnt` (`TARGET_DIR`) while it is being installed.

## Startup
`install.sh` does the following, in order:
1. Sources `version.sh` and the helper scripts (`config.sh`, `constants.sh`, `defaults.sh`, `dependencies.sh`, `helpers.sh`).
2. Prints the start banner with the installer version.
3. Creates the state directory `/tmp/hyprdebian`.
4. Loads the saved configuration, if there is one (see [Idempotency](Idempotency.md#state)).
5. Defines the root task `install`.
6. Sources the phase scripts, in order. Sourcing a phase script only defines its tasks and registers their dependencies. Nothing is executed yet.
7. Runs `execute_task "install"`, which does the actual installation.

The script runs with `set -eo pipefail`, so the installer stops at the first failing task. It also runs with `set -a`, which exports every variable. This is how `render.py` and other child processes get to see the answers and other variables (see [Files and templates](Files%20and%20templates.md#templates)).

## Running commands in the target
Commands that must run inside the target system use `in_target`:
```
in_target apt install -y build-essential
```

This runs the command in a `chroot` of `/mnt`, with a clean environment that only has `HOME`, `TERM` and `PATH`. The base phase bind mounts `/dev`, `/proc` and `/sys` into the target first (`b_mount`), so tasks that use `in_target` must run after it.
