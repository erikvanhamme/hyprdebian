#!/usr/bin/python3
"""Reads the answers file of hd-installer, a JSON object, and prints its answers for the installer.

The host name is the name of the file without its extension, such as test for test.json. The
other answers are keys of the object; a question without one is asked as usual. Each answer is
printed as the variable of its question and its value, each followed by a NUL, so a value may
hold any character but NUL. The checks that need the computer, such as whether a disk exists,
and those of the shell, such as the format of a user name, are left to the installer.

    answers.py <file>

An error is printed on stderr, with exit status 1.
"""

import argparse
import json
import os
import sys


class AnswersError(Exception):
    """A problem in the answers file, with the message for the user."""


def expect(key, value, kind, name):
    """Raises AnswersError unless the value of the key is of the kind, named name in the message.
    A bool is not taken as a number, though Python sees it as one."""
    if not isinstance(value, kind) or (kind is int and isinstance(value, bool)):
        raise AnswersError(f"{key} must be {name}")


def text(key, value):
    expect(key, value, str, "a string")
    return [(question_of(key), value)]


def yes_no(key, value):
    expect(key, value, bool, "true or false")
    return [(question_of(key), "true" if value else "false")]


def swap(key, value):
    expect(key, value, int, "a whole number of GiB")
    if value < 0:
        raise AnswersError(f"{key} cannot be negative")
    return [("Q_SWAP", str(value))]


def disks(key, value, prefix):
    """One disk, or two for a mirror: the mirror question and both disk questions."""
    expect(key, value, list, "a list of one or two disks")
    if len(value) not in (1, 2):
        raise AnswersError(f"{key} must name one or two disks")
    for disk in value:
        expect(key, disk, str, "a list of disk paths")

    return [
        (f"Q_{prefix}_REDUNDANT", "true" if len(value) == 2 else "false"),
        (f"Q_{prefix}_DISK_A", value[0]),
        (f"Q_{prefix}_DISK_B", value[1] if len(value) == 2 else ""),
    ]


def system_disks(key, value):
    return disks(key, value, "ROOT")


def esp_disks(key, value):
    return disks(key, value, "ESP")


def wifi(key, value):
    """false for no wifi, or the wifi interface."""
    if value is False:
        return [("Q_WIFI", "false")]
    expect(key, value, str, "false or a wifi interface")
    return [("Q_WIFI", "true"), ("Q_WIFACE", value)]


def debconf(key, value):
    """The lines for debconf-set-selections: owner, question, type and value."""
    expect(key, value, list, "a list of debconf selections")
    for line in value:
        expect(key, line, str, "a list of debconf selections")
        if "\n" in line or len(line.split(None, 3)) < 3:
            raise AnswersError(f"{key} has a line that is not 'owner question type value': "
                               f"{line!r}")
    return [("DEBCONF", "\n".join(value))]


# The keys of the answers file: the function that turns a value into answers, and the variable of
# the question for the functions that answer one question. The domain is turned into the FQDN in
# read, as that needs the host name.
KEYS = {
    "suite": (text, "Q_SUITE"),
    "encrypt": (yes_no, "Q_ENCRYPT"),
    "system_disks": (system_disks, None),
    "swap": (swap, None),
    "esp_disks": (esp_disks, None),
    "user": (text, "Q_USER"),
    "password": (text, "Q_PASSWORD"),
    "fullname": (text, "Q_FULLNAME"),
    "domain": (None, None),
    "iface": (text, "Q_IFACE"),
    "wifi": (wifi, None),
    "wipe": (yes_no, "Q_WIPE"),
    "debconf_defaults": (yes_no, "Q_DEBCONF_DEFAULTS"),
    "debconf": (debconf, None),
}


def question_of(key):
    return KEYS[key][1]


def unique_keys(pairs):
    """The object of the pairs of a JSON object; a key that is given twice is an error, as JSON
    would silently keep the last one."""
    content = {}
    for key, value in pairs:
        if key in content:
            raise AnswersError(f"the key {key} is given twice")
        content[key] = value
    return content


def read(path):
    """The answers of the file, as a list of (variable, value)."""
    hostname = os.path.splitext(os.path.basename(path))[0]
    answers = [("Q_HOSTNAME", hostname)]

    try:
        with open(path) as f:
            content = json.load(f, object_pairs_hook=unique_keys)
    except OSError as e:
        raise AnswersError(f"cannot read {path}: {e.strerror}")
    except json.JSONDecodeError as e:
        raise AnswersError(f"{path} is not valid JSON: {e}")

    if not isinstance(content, dict):
        raise AnswersError(f"{path} must hold a JSON object")

    for key, value in content.items():
        if key not in KEYS:
            raise AnswersError(f"{path} has the unknown key {key}")
        if key == "domain":
            expect(key, value, str, "a string")
            answers.append(("Q_FQDN", f"{hostname}.{value}"))
        else:
            answers.extend(KEYS[key][0](key, value))

    return answers


def main():
    parser = argparse.ArgumentParser(description="Prints the answers of an answers file of "
                                     "hd-installer, as NUL-separated variables and values.")
    parser.add_argument("file", help="the answers file, named <host name>.json")
    args = parser.parse_args()

    try:
        answers = read(args.file)
    except AnswersError as e:
        print(f"Error: {e}.", file=sys.stderr)
        return 1

    for variable, value in answers:
        if "\0" in value:
            print(f"Error: the answer {variable} holds a NUL character.", file=sys.stderr)
            return 1
        sys.stdout.write(f"{variable}\0{value}\0")
    return 0


if __name__ == "__main__":
    sys.exit(main())
