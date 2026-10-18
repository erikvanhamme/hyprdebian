#!/bin/bash
# Checks of the answers about the name of the host.

# A host name is one DNS label: letters, digits and -, not at the start or the end, at most 63
# characters. Lower case, as DNS ignores the case anyway.
valid_hostname() {
    [[ "$1" =~ ^[a-z0-9]([-a-z0-9]{0,61}[a-z0-9])?$ ]]
}

# The fully qualified domain name: the host name followed by its domain, labels like the host
# name joined by dots, at most 253 characters.
valid_fqdn() {
    local label="[a-z0-9]([-a-z0-9]{0,61}[a-z0-9])?"

    [[ "$1" == "${Q_HOSTNAME}."* && ${#1} -le 253 && "$1" =~ ^${label}(\.${label})+$ ]]
}
