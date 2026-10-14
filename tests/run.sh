#!/bin/bash
# Runs the tests. Every directory in tests/ is a test suite of Python unittest tests.
#
#   tests/run.sh                          runs all suites
#   tests/run.sh hd-backup                runs one suite
#   tests/run.sh --coverage [suite...]    also measures coverage, for the suites that have a .coveragerc
#
# With --coverage, a report is printed after each suite, and an HTML report is written to
# tests/<suite>/htmlcov/index.html. It needs python3-coverage.
set -euo pipefail

cd "$(dirname "$0")"

coverage=false
if [[ $# -gt 0 && "$1" == "--coverage" ]]; then
    coverage=true
    shift
fi

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

    # Each suite runs from its own directory. -B: do not leave __pycache__ directories in the checkout.
    if [[ "${coverage}" == "true" && -f "${suite}/.coveragerc" ]]; then
        if ! (cd "${suite}" && rm -rf .coverage .coverage.* htmlcov && python3 -B -m coverage run -m unittest discover -s . -t .); then
            status=1
        fi
        (cd "${suite}" && python3 -m coverage combine --quiet && python3 -m coverage report && python3 -m coverage html --quiet)
        echo "HTML report: tests/${suite}/htmlcov/index.html"
    else
        if ! (cd "${suite}" && python3 -B -m unittest discover -s . -t .); then
            status=1
        fi
    fi
done

exit ${status}
