#!/bin/bash

tpl_pre() {
    return 0
}

tpl_main() {
    return 0
}

tpl_post() {
    return 0
}

tpl_render() {
    template_render_queue
}

add_dependencies "tpl_main" \
    "tpl_render" \

add_dependencies "install" \
    "tpl_pre" \
    "tpl_main" \
    "tpl_post" \
    