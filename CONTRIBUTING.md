# Contributing

Thank you for looking. This is a small project with one maintainer; the fastest way to get a change in is to keep
it small and to show what it does to the numbers.

## Before you open a pull request

1. Run the gate: `scripts/gate.sh` (about seven minutes). It builds the tools, checks that an input sample that is
   not a number is counted as silence, that the C++ engine still matches the Python reference, and that the
   scorecard stays inside its limits: at octave down, +2 and octave up, at octave up in the Balanced and Clean
   responses, and at octave down, +2 and octave up with Quality on Lite and on Eco.
2. If you changed how the engine sounds, say what moved: paste the scorecard lines before and after
   (`python tests/scorecard.py engine all`). A change that makes one number better and another worse is fine if you
   say so; a change with no numbers is hard to accept.
3. If you changed the engine, change the reference too (or the other way round). They are the same algorithm twice,
   on purpose: `reference/polypitch_ref.py` is where ideas are tried, `engine/PolyPitchEngine.h` is the streaming
   port, and `tests/test_engine_vs_reference.py` holds them together.

## Ground rules

- Measure, then claim. Numbers in the docs come from a run, with the command next to them.
- No audio that is not yours to give. Do not commit recordings, sample-library content or presets of commercial
  products. The tests run on synthetic signals for that reason; `tests/mix_test.py` takes your own files and keeps
  them out of the repository (`audio/` is ignored).
- The engine has no dependencies and stays that way: standard C++17, one header. Host code (JUCE, the Max SDK)
  belongs in the wrappers.
- No allocation, locking or file access where audio is processed.

## Good first things

`ROADMAP.md` is ordered by what would help most. The performance items still listed as left in its section 2,
smoothing the Tone control, and the Windows builds are self-contained places to start.

## License

By contributing you agree that your contribution is released under the MIT License (see `LICENSE`).
