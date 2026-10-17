## Introduction
The installer can be started again after a failure. Tasks that completed before are skipped, and the installation continues with the task that failed. So when a task fails, fix the cause (or the bug in the script) and run `install.sh` again.

This works because the installer remembers which tasks are done, and saves the answers and other values it needs between runs.

## State
The installer keeps its state in `/tmp/hyprdebian`:

| Path | Contents |
| --- | --- |
| `/tmp/hyprdebian/config` | The answers, the [queues](Queues.md) and the dynamic [dependencies](Dependencies.md), as bash variable assignments. |
| `/tmp/hyprdebian/<task>.done` | One empty file for each task that completed successfully. |

Values are written to the configuration file with `save_config var value`, and the whole file is read back with `load_config` at startup.

Only values that are explicitly saved survive a restart. A variable that a task sets without `save_config` is gone when the installer is started again and that task is skipped. When a later task needs such a value, save it with `save_config`, or have the later task work it out again.

## Tasks that always run
A task that must run on every start, such as one that asks for a value that is never saved, is marked with `always_run`.

## Starting over
To start a completely fresh installation, remove the state directory:
```
sudo rm -rf /tmp/hyprdebian
```

Because the state is kept in `/tmp`, it is also gone after a reboot of the live system.

## Writing tasks
When writing a task, keep in mind that it may be run again after it failed halfway. Where possible, write tasks so that running them a second time works, for example by using `mkdir -p` instead of `mkdir`.
