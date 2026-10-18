## Introduction
All the work of the installer is done by **tasks**. A task is a plain bash function. The task succeeds when the function returns 0 and fails otherwise.

Tasks are ordered with **dependencies**. A dependency of a task is another task that must run before it. The dependency system lives in:
```
scripts/dependencies.sh
```

The root of all the work is the empty `install` task in `install.sh`:
```
install() {
    return 0
}
```

Every other task is, directly or indirectly, a dependency of `install`. A task that is not in the dependency tree of `install` never runs.

## Functions
The dependency system has these functions:

| Function | Purpose |
| --- | --- |
| `add_dependencies task dep_a dep_b ...` | Adds the dependencies `dep_a`, `dep_b`, ... to `task`. Dependencies that are already there are not added twice. |
| `execute_task task` | Runs all dependencies of `task`, then `task` itself. |
| `always_run task` | Makes `task` run again on every installer run, even if it completed before. |
| `is_done task` | Succeeds when `task` has completed before. |
| `mark_done task` | Marks `task` as completed. |

## How a task is executed
`execute_task task` works as follows:
1. Each dependency of `task` is executed with `execute_task`, in the order in which the dependencies were added. This is recursive: dependencies can have dependencies of their own.
2. If `task` has completed before, it is skipped and shown as `SKIPPED`.
3. Otherwise `task` is shown as `START` and the function is called.
4. When the function succeeds, the task is marked as done and shown as `DONE`. When it fails, it is shown as `FAIL` and the installer stops.

Some things to keep in mind:
- **Dependencies of a skipped task still run.** Step 1 comes before the check in step 2, so a task that completed before still runs its dependencies, and new dependencies added to it are picked up.
- **The dependency list is read when the task starts.** Dependencies that are added to a task while it is already running (for example by one of its own dependencies) are not seen in this run. Dynamic dependencies must therefore be added to a task that runs later.
- **`set -e` does not apply inside a task.** The task function is called in the condition of an `if`, and bash turns off `set -e` there. A failing command in the middle of a task does not stop it: the task only fails when its last command fails or when it returns a non-zero value. Tasks that must stop on an error have to check for it and `return 1`, as for example `b_fstab` does.
- **A task with the same name runs only once.** Tasks are identified by their function name. When a task is the dependency of several other tasks, it runs the first time and is skipped after that.

## Static and dynamic dependencies
Dependencies can be added in two ways:
- **Statically**, in the body of a phase script. These are registered every time the script is sourced.
- **Dynamically**, from within a task, usually in response to an answer of the user. For example, when the user answers yes to "Install backup tools", `q_backuptools` adds `o_backuptools` to `o_main`:
  ```
  if ask_yes_no Q_BACKUPTOOLS "Install backup tools" "${default}"; then
      add_dependencies "o_main" "o_backuptools"
  fi
  ```

Dynamic dependencies are saved in the configuration file (`TASK_DEPS`), so they are still there when the installer is started again after the question was answered. See [Idempotency](Idempotency.md).
