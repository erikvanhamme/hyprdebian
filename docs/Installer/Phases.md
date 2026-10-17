## Introduction
The installer is split into phases. Each phase has its own script in the `scripts` directory and its own task prefix. The phases run in the order in which they are sourced in `install.sh`:

| # | Phase | Script | Prefix | What it does |
| --- | --- | --- | --- | --- |
| 1 | Questions | `questions.sh` | `q_` | Asks the user for the optional components, and adds the dependencies and queue entries that follow from the answers. See [Questions](Questions.md). |
| 2 | Prerequisites | `prereqs.sh` | `pr_` | Installs the tools the installer needs on the live system. Skipped on the hyprdebian live cd. |
| 3 | Base | `base.sh` | `b_` | Installs a specific kernel, when one was downloaded. Also queues the files and services for the rotation of the machine id and the journal. |
| 4 | Optional | `optional.sh` | `o_` | Queues the optional components the user selected (desktop, docker, Qemu/KVM, ...). |
| 5 | Packages | `packages.sh` | `pkg_` | Installs all queued packages in the target system. |
| 6 | Files | `files.sh` | `fil_` | Copies all queued files from `deploy/` to the target system. |
| 7 | Templates | `templates.sh` | `tpl_` | Renders all queued templates from `templates/` to the target system. |
| 8 | Services | `services.sh` | `svc_` | Enables all queued services in the target system. |
| 9 | User | `user.sh` | `u_` | Clones the checkout that runs the installer into `/usr/src/hyprdebian`, with `origin` pointing at GitHub over https, and gives it to the user. |
| 10 | Cleanup | `cleanup.sh` | `c_` | Saves the config and the migrations for the [upgrade](Upgrade.md#state-on-the-installed-system). |

The disks, the base system, the bootloader, the network, the firewall, the user account and the snapshot are done by `hd-installer` (see [hd-installer](../packages/hd-installer.md)).

Because packages are installed before files and templates are written, the files from `deploy/` and `templates/` overwrite the default configuration files that come with the packages. See [Queues](Queues.md).

## Phase hooks
Each phase has three empty tasks, `<prefix>_pre`, `<prefix>_main` and `<prefix>_post`. They do nothing themselves, but serve as hooks to attach the real work to:
- `<prefix>_main` holds the normal work of the phase.
- `<prefix>_pre` and `<prefix>_post` hold work that must run just before or just after it.

Tasks from one phase often attach work to the hooks of a later phase. For example, `o_qemu_kvm` adds `pkg_qemu_kvm` to `pkg_post`, so it runs right after the packages are installed, and `o_desktop` adds tasks to `u_post` that need the user account to exist.

## Phase script template
A new phase script looks like this (with `x` replaced by the prefix of the phase):
```
#!/bin/bash

x_pre() {
    return 0
}

x_main() {
    return 0
}

x_post() {
    return 0
}

x_something() {
    # The actual work.
}

add_dependencies "x_main" \
    "x_something" \

add_dependencies "install" "x_pre" "x_main" "x_post"
```

The last line hooks the phase into the root task. The phase script must also be sourced in `install.sh`, at the right place in the list:
```
# Load tasks/deps for all the phases, in order.
source scripts/questions.sh
source scripts/prereqs.sh
source scripts/base.sh
source scripts/optional.sh
source scripts/packages.sh
source scripts/files.sh
source scripts/templates.sh
source scripts/services.sh
source scripts/user.sh
source scripts/cleanup.sh
```

Because `add_dependencies` keeps the dependencies in the order they were added, the order of the `source` lines is the order in which the phases run.
