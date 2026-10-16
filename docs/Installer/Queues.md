## Introduction
Many tasks do not install or copy things themselves. Instead they add entries to a **queue**, and a later phase processes the whole queue at once. This keeps, for example, all packages in a single `apt install`.

## Queues
The queue functions are in `scripts/helpers.sh`:

| Queue | Add with | Processed by | Phase |
| --- | --- | --- | --- |
| `PACKAGES` | `add_packages pkg...` | `pkg_install` | Packages |
| `FILES` | `add_files deploy/path...` | `fil_deploy` | Files |
| `TEMPLATE_SRCS` / `TEMPLATE_MODES` | `add_template templates/path.j2 [mode]` | `tpl_render` | Templates |
| `SERVICES` | `add_services unit...` | `svc_enable` | Services |
| `USER_GROUPS` | `add_user_groups group...` | `u_groups` | User |

Packages that the deb phase built, such as `fonts-jetbrainsmono-nerd`, are queued by name like any other package. They are in no repo, so `pkg_install` copies their `.deb` files from `/tmp/hyprdebian` into the target and passes them to apt by path, in the same `apt install` as the other packages.

Entries that are already in a queue are not added twice. The queues are saved in the configuration file, so they survive a restart of the installer. See [Idempotency](Idempotency.md).

## Timing
A task can add to a queue as long as the phase that processes it has not run yet (see [Phases](Phases.md) for the order).

Things that are needed earlier, such as the `fstab` that must exist before GRUB is set up, are deployed or rendered directly from the task instead of through a queue. See [Files and templates](Files%20and%20templates.md).
