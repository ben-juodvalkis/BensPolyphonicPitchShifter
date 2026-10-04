#!/bin/bash
# The gate: what has to pass before a change goes to main. About two minutes.
#   1. the tools build
#   2. the C++ engine still matches the Python reference (44.1 and 48 kHz, octave down and up)
#   3. the scorecard at -12 and +12 stays inside its limits (tests/scorecard.py GATE)
set -e
cd "$(dirname "$0")/.."
PY="${PYTHON:-$PWD/.venv/bin/python}"
scripts/build.sh tools
cd tests
"$PY" test_engine_vs_reference.py
"$PY" scorecard.py engine -12 12
echo "GATE PASSED"
