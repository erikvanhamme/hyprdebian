#!/bin/bash
# Checks of the answers about the user account.

# A user name as adduser accepts it by default: a lower case letter, then lower case letters,
# digits, - and _, at most 32 characters.
valid_user() {
    [[ "$1" =~ ^[a-z][-a-z0-9_]{0,31}$ ]]
}

# A full name, for the GECOS field of /etc/passwd, whose fields are separated by : and ,.
valid_fullname() {
    [[ -n "$1" && "$1" != *[:,]* ]]
}
