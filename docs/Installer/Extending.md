## Introduction
This document lists a few common changes to the installer, and where they go.

## Common changes
- **A new question:** add a `q_` task to `scripts/questions.sh`, and add it to `q_main` (asked for every OS) or to `q_os_questions` in `q_os` (only for some OSes). If it has a default, add it to the tables in `scripts/defaults.sh`. See [Questions](Questions.md) and [Defaults](Defaults.md).
- **A new optional component:** add an `o_` task to `scripts/optional.sh` that queues its packages, files, templates, services and groups. Then have its question add it to `o_main` when the user selects it. See [Queues](Queues.md).
- **A step that must run after the packages are installed, or after the user exists:** add a task to `scripts/packages.sh` or `scripts/user.sh` (or keep it in the phase it belongs to) and attach it to `pkg_post`, `svc_post` or `u_post` with `add_dependencies`. See [Phases](Phases.md#phase-hooks).
- **A new configuration file:** put it under `deploy/` (fixed content) or `templates/` (content that depends on the answers), and queue it with `add_files` or `add_template` from the task that needs it. See [Files and templates](Files%20and%20templates.md).
- **A new phase:** see [Phases](Phases.md#phase-script-template).
- **A change for systems that are already installed,** like masking a service, or removing a package, service or file that is no longer used: add a migration. See [Upgrade](Upgrade.md#migrations).

Keep in mind that every task may run again after a failure. See [Idempotency](Idempotency.md). The tasks that queue things also run during every upgrade. See [Upgrade](Upgrade.md#changing-the-installer-with-the-upgrade-in-mind).
