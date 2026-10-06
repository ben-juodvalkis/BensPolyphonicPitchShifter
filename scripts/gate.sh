#!/bin/bash
# The gate: what has to pass before a change goes to main. About seven minutes.
#   1. the tools build
#   2. an input sample that is not a number, or is infinite, counts as silence (tests/test_bad_samples.py; a few seconds)
#   3. the C++ engine still matches the Python reference (44.1 and 48 kHz: octave down, +2, and octave up in each response;
#      octave down, +2 and octave up with the quality set to lite, and to eco)
#   4. the scorecard at -12, +2 and +12, at +12 in the balanced and clean responses, and at -12, +2 and +12 with the
#      quality set to lite and to eco, stays inside its limits (tests/scorecard.py GATE, GATE_RESPONSE, GATE_ECO)
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
"$PY" scorecard.py engine lite -12 2 12
"$PY" scorecard.py engine eco -12 2 12
echo "GATE PASSED"
