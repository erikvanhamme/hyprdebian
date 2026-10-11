## Introduction
Configuration files reach the target system in two ways: as fixed files from the `deploy` directory, or as Jinja2 templates from the `templates` directory, which are filled in with the answers of the user. Both can be [queued](Queues.md) or written right away.

## Deployed files
The `deploy` directory mirrors the root of the target system. A file is copied to the same path under `/mnt`, with the `deploy/` prefix removed:
```
deploy/etc/default/grub  ->  /mnt/etc/default/grub
```

Use `add_files` to queue a file, or `file_deploy` to copy it right away. Missing directories are created.

## Templates
The `templates` directory also mirrors the root of the target system. Templates are Jinja2 files with a `.j2` extension. They are rendered by `render.py` to the same path under `/mnt`, with the `templates/` prefix and the `.j2` extension removed:
```
templates/etc/hostname.j2  ->  /mnt/etc/hostname
```

Use `add_template` to queue a template, or `template_render` to render it right away. Both take an optional file mode (default `0644`).

The values available in a template are the environment variables of the installer. Because the installer runs with `set -a`, these are all its shell variables, including the `Q_` answers. Values `true`, `false`, `yes`, `no`, `on`, `off`, `1` and `0` are turned into booleans, and other numbers into integers, so a template can write `{% if Q_ENCRYPT %}`. The password is kept out of the environment and cannot be used in templates.

When `template_render` is called directly from a task, the template also sees the local variables of that task. `b_fstab` uses this to pass the swap UUID to `fstab.j2`. Queued templates are rendered later, in the templates phase, and only see the global variables.
