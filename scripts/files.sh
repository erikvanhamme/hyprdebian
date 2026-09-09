#!/bin/bash

fil_pre() {
    return 0
}

fil_main() {
    return 0
}

fil_post() {
    return 0
}

fil_deploy() {
    file_deploy_queue
}

add_dependencies "fil_main" \
    "fil_deploy" \

add_dependencies "install" "fil_pre" "fil_main" "fil_post"