#!/bin/bash
# Runs the tests. Every directory in tests/ is a test suite of Python unittest tests.
#
#   tests/run.sh               runs all suites
#   tests/run.sh hd-backup     runs one suite
set -euo pipefail

cd "$(dirname "$0")"

if [[ $# -gt 0 ]]; then
    suites=("$@")
else
    suites=()
    for dir in */; do
        suites+=("${dir%/}")
    done
fi

status=0
for suite in "${suites[@]}"; do
    echo "== ${suite}"
    # -B: do not leave __pycache__ directories in the checkout.
    if ! python3 -B -m unittest discover -s "${suite}" -t "${suite}"; then
        status=1
    fi
done

exit ${status}
