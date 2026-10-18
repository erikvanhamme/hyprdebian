#!/bin/bash
# Tasks: shell functions that run once. A task runs its dependencies first, in the order they
# were added, and is marked done in STATE_DIR when it succeeds, so a rerun skips it. The
# dependencies are kept in the config, so those that answers added survive a rerun.

is_done() {
    [[ -f "${STATE_DIR}/$1.done" ]]
}

mark_done() {
    touch "${STATE_DIR}/$1.done"
}

# Makes a task run again on every run.
always_run() {
    rm -f "${STATE_DIR}/$1.done"
}

# Adds dependencies to a task; one that it already has is not added again.
# Usage: add_dependencies <task> <dependency>...
add_dependencies() {
    local task="$1"
    shift
    local deps="${TASK_DEPS[${task}]:-}"
    local dep

    for dep in "$@"; do
        if [[ ",${deps}," != *",${dep},"* ]]; then
            deps="${deps}${deps:+,}${dep}"
        fi
    done

    TASK_DEPS["${task}"]="${deps}"
    save_config TASK_DEPS ""
}

# Runs a task after its dependencies, unless it is done.
# Usage: execute_task <task>
execute_task() {
    local task="$1"
    local deps dep

    if [[ -n "${TASK_DEPS[${task}]:-}" ]]; then
        IFS=',' read -ra deps <<< "${TASK_DEPS[${task}]}"
        for dep in "${deps[@]}"; do
            execute_task "${dep}" || return 1
        done
    fi

    if is_done "${task}"; then
        log_status "${COLOR_BLUE}" "SKIPPED" "${task}"
        return 0
    fi

    log_status "${COLOR_YELLOW}" "START  " "${task}"
    if "${task}"; then
        mark_done "${task}"
        log_status "${COLOR_GREEN}" "DONE   " "${task}"
    else
        log_status "${COLOR_RED}" "FAIL   " "${task}"
        return 1
    fi
}
