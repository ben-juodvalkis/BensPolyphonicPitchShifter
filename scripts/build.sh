#!/bin/bash
# Build PolyPitch. Usage: scripts/build.sh [tools] [plugin] [max]     (no arguments = all three)
#   tools   build/tools/polypitch_cli (what the tests run), polypitch_load (the CPU load) and polypitch_profile (where the time goes)
#   plugin  build/plugin: the AU and VST3 (needs JUCE 8: JUCE_DIR=/path/to/JUCE, or ~/JUCE, or it is fetched)
#   max     max/PolyPitch/externals/polypitch~.mxo (needs Cycling '74's max-sdk-base: set C74_SDK=/path/to/max-sdk-base)
set -e
cd "$(dirname "$0")/.."
what="${@:-tools plugin max}"
for w in $what; do
  case $w in
    tools)
      mkdir -p build/tools
      for t in polypitch_cli polypitch_load; do
        c++ -std=c++17 -O3 -ffast-math -fno-finite-math-only -o build/tools/$t tools/$t.cpp
      done
      c++ -std=c++17 -O3 -ffast-math -fno-finite-math-only -DPOLYPITCH_PROFILE -o build/tools/polypitch_profile tools/polypitch_profile.cpp
      echo "built build/tools/polypitch_cli, polypitch_load and polypitch_profile" ;;
    plugin)
      cmake -S plugin -B build/plugin -DCMAKE_BUILD_TYPE=Release ${JUCE_DIR:+-DJUCE_DIR="$JUCE_DIR"} > build/plugin-configure.log 2>&1 || { tail -20 build/plugin-configure.log; exit 1; }
      cmake --build build/plugin -j 8 > build/plugin-build.log 2>&1 || { grep -E "error" build/plugin-build.log | head -20; exit 1; }
      find build/plugin -maxdepth 6 \( -name "PolyPitch.vst3" -o -name "PolyPitch.component" \) -print ;;
    max)
      if [ -z "$C74_SDK" ]; then echo "set C74_SDK to a checkout of https://github.com/Cycling74/max-sdk-base"; exit 1; fi
      cmake -S max/PolyPitch/source/projects/polypitch_tilde -B build/max -DCMAKE_BUILD_TYPE=Release -DC74_SDK="$C74_SDK" -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" > build/max-configure.log 2>&1 || { tail -20 build/max-configure.log; exit 1; }
      cmake --build build/max -j 8 > build/max-build.log 2>&1 || { grep -E "error" build/max-build.log | head -20; exit 1; }
      ls -d max/PolyPitch/externals/polypitch~.mxo ;;
    *) echo "unknown target $w"; exit 1 ;;
  esac
done
