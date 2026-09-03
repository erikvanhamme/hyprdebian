#!/bin/bash

is_done() {
    [[ -f "$STATE_DIR/$1.done" ]]
}

mark_done() {
    touch "$STATE_DIR/$1.done"
}

# Task and dependency handling.
add_dependencies() {
    local task_id=$1
    shift # Remove the first argument (task_id), leaving only dependencies.

    local current_deps=${TASK_DEPS["$task_id"]}

    for new_dep in "$@"; do
        # Check if the dependency is already in the string to avoid duplicates.
        if [[ ! ",$current_deps," =~ ",$new_dep," ]]; then
            current_deps="${current_deps}${current_deps:+,}$new_dep"
        fi
    done

    # Update the array and save.
    TASK_DEPS["$task_id"]="$current_deps"
    save_config "TASK_DEPS" ""
}

execute_task() {
    local func_name=$1
    local display_name=${2:-$func_name}

    # Handle Dependencies first.
    local deps_list=${TASK_DEPS["$func_name"]}
    if [[ -n "$deps_list" ]]; then
        IFS=',' read -ra ADDR <<< "$deps_list"
        for dep in "${ADDR[@]}"; do
            execute_task "$dep"
        done
    fi

    # Idempotency check.
    if is_done "$func_name"; then
        log_status "$COLOR_BLUE" "SKIPPED" "$display_name"
        return 0
    fi

    log_status "$COLOR_YELLOW" "START  " "$display_name"

    if "$func_name"; then
        mark_done "$func_name"
        log_status "$COLOR_GREEN" "DONE   " "$display_name"
    else
        log_status "$COLOR_RED" "FAIL   " "$display_name"
        return 1
    fi
}
