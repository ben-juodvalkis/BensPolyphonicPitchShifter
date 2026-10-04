#!/bin/bash
# Install what has been built on this Mac. Usage: scripts/install-macos.sh [plugin] [max]     (no arguments = both)
#   plugin  PolyPitch.component and PolyPitch.vst3 into ~/Library/Audio/Plug-Ins
#   max     the PolyPitch package (the polypitch~ object and its help) into Max's Packages folder
# The Max for Live device is max/device/PolyPitch.amxd: drag it into Live, or keep it wherever you keep devices.
# It finds polypitch~ through the installed package. Restart Live (or Max) after installing.
set -e
cd "$(dirname "$0")/.."
what="${@:-plugin max}"
for w in $what; do
  case $w in
    plugin)
      A=build/plugin/PolyPitch_artefacts/Release
      [ -d "$A/AU/PolyPitch.component" ] || { echo "build the plug-in first: scripts/build.sh plugin"; exit 1; }
      mkdir -p ~/Library/Audio/Plug-Ins/Components ~/Library/Audio/Plug-Ins/VST3
      rm -rf ~/Library/Audio/Plug-Ins/Components/PolyPitch.component ~/Library/Audio/Plug-Ins/VST3/PolyPitch.vst3
      ditto "$A/AU/PolyPitch.component" ~/Library/Audio/Plug-Ins/Components/PolyPitch.component
      ditto "$A/VST3/PolyPitch.vst3" ~/Library/Audio/Plug-Ins/VST3/PolyPitch.vst3
      echo "installed the AU and VST3 (check the AU with: auval -v aufx PlyP Bjuo)" ;;
    max)
      [ -d "max/PolyPitch/externals/polypitch~.mxo" ] || { echo "build the Max object first: C74_SDK=... scripts/build.sh max"; exit 1; }
      P=""; for d in "$HOME/Documents/Max 9/Packages" "$HOME/Documents/Max 8/Packages"; do [ -d "$d" ] && { P="$d"; break; }; done
      [ -n "$P" ] || { echo "no Max Packages folder found in ~/Documents (Max 9 or Max 8)"; exit 1; }
      rm -rf "$P/PolyPitch"; mkdir -p "$P/PolyPitch"
      ditto max/PolyPitch/externals "$P/PolyPitch/externals"; ditto max/PolyPitch/help "$P/PolyPitch/help"; cp max/PolyPitch/package-info.json "$P/PolyPitch/"
      echo "installed the PolyPitch package in $P" ;;
    *) echo "unknown target $w"; exit 1 ;;
  esac
done
