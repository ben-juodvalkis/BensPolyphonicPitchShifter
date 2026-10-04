# Third-party software and what it means for you

Everything in this repository is under the MIT License (see `LICENSE`). Nothing from a third party is copied into the
repository. Two of the three things you can build from it link against someone else's code, and that code has its
own terms.

| What you build | Links against | Their terms | What follows |
|---|---|---|---|
| The engine, the command-line tools, the Python reference | nothing | | MIT, no strings |
| The plug-in (`plugin/`) | [JUCE 8](https://juce.com) and, through it, Steinberg's VST3 SDK | JUCE: AGPLv3, or a commercial licence from JUCE | A plug-in binary built with JUCE under its AGPLv3 option has to be distributed under the AGPLv3: whoever gets the binary must be able to get the complete source, which this repository is. If you build a closed product on the plug-in wrapper you need a JUCE licence. The engine itself carries no such condition. |
| The Max object (`max/`) | Cycling '74's [max-sdk-base](https://github.com/Cycling74/max-sdk-base) | MIT | Keep their copyright notice with copies of their SDK. No condition on the object. |

The Python tests use numpy, scipy, numba, soundfile and (for hosting the plug-in) pedalboard, each under its own
open-source licence; none of them is redistributed here.

## Names

VST is a trademark of Steinberg Media Technologies GmbH. Audio Units is a trademark of Apple Inc. Max and Max for
Live are products of Cycling '74 and Ableton. Helix and Helix Native are products of Line 6 (Yamaha Guitar Group);
they are named in `docs/benchmarks.md` only as the device this project was measured against. Ben's Polyphonic Pitch Shifter is an
independent project and is not affiliated with or endorsed by any of them.
