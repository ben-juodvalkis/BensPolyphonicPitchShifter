#!/bin/bash
# The gate: what has to pass before a change goes to main. About four minutes.
#   1. the tools build
#   2. an input sample that is not a number, or is infinite, counts as silence (tests/test_bad_samples.py; a few seconds)
#   3. the C++ engine still matches the Python reference (44.1 and 48 kHz: octave down, +2, and octave up in each response)
#   4. the scorecard at -12, +2 and +12, and at +12 in the balanced and clean responses, stays inside its limits
#      (tests/scorecard.py GATE and GATE_RESPONSE)
set -e
cd "$(dirname "$0")/.."
PY="${PYTHON:-$PWD/.venv/bin/python}"
scripts/build.sh tools
cd tests
"$PY" test_bad_samples.py
"$PY" test_engine_vs_reference.py
"$PY" scorecard.py engine -12 2 12
"$PY" scorecard.py engine balanced 12
"$PY" scorecard.py engine clean 12
echo "GATE PASSED"
