#!/bin/bash
# Make the macOS release archive: build/release/BensPolyphonicPitchShifter-<version>-macOS.zip, holding the AU and VST3,
# the Max package (polypitch~ and its help) and the Max for Live device, signed with a Developer ID and notarized.
# Usage: scripts/release-macos.sh            (run scripts/gate.sh first; needs C74_SDK for the Max object)
# Needs, once: a "Developer ID Application" certificate in the keychain, and notarization credentials stored with
#   xcrun notarytool store-credentials notarytool-profile --apple-id <apple id> --team-id <team id> --password <app-specific password>
# Override with SIGN_ID=... and NOTARY_PROFILE=... .
# The Max for Live device goes in frozen (carrying polypitch~ inside it, so it works without the package): freeze it in
# Max with the package installed from this build, and save the frozen copy as build/frozen/<device>.amxd (or point
# FROZEN_DEVICE at it). Not the one in max/device, which max/tools/maxpatch.py rewrites. A frozen device older than the
# last change to the engine or the Max object is refused: it would carry an old polypitch~. Without one the plain
# device goes in, which needs the package.
set -e
cd "$(dirname "$0")/.."
V=$(sed -n 's/^project(PolyPitch VERSION \([0-9.]*\).*/\1/p' plugin/CMakeLists.txt)
SIGN_ID="${SIGN_ID:-$(security find-identity -v -p codesigning | sed -n 's/.*"\(Developer ID Application: [^"]*\)".*/\1/p' | head -1)}"
NOTARY_PROFILE="${NOTARY_PROFILE:-notarytool-profile}"
[ -n "$SIGN_ID" ] || { echo "no Developer ID Application certificate found"; exit 1; }
[ -n "$C74_SDK" ] || { echo "set C74_SDK to a checkout of https://github.com/Cycling74/max-sdk-base"; exit 1; }
grep -q "\"version\" : \"$V\"" max/PolyPitch/package-info.json || { echo "max/PolyPitch/package-info.json is not at version $V"; exit 1; }
DEV="Ben's Polyphonic Pitch Shifter.amxd"; FROZEN_DEVICE="${FROZEN_DEVICE:-build/frozen/$DEV}"
if [ -f "$FROZEN_DEVICE" ]; then
  changed=$(git log -1 --format=%ct -- engine max/PolyPitch/source); made=$(stat -f %m "$FROZEN_DEVICE")
  [ "$made" -gt "$changed" ] || { echo "$FROZEN_DEVICE is older than the last change to the engine or the Max object: freeze it again"; exit 1; }
  echo "the device: $FROZEN_DEVICE (frozen)"; NOTE="It is frozen: it carries polypitch~ inside it and works without the package."
else
  FROZEN_DEVICE="max/device/$DEV"; echo "no frozen device: the plain one goes in, and it needs the Max package"; NOTE="It needs the PolyPitch package installed as above; restart Live after installing it."
fi

scripts/build.sh plugin max
R=build/release; D="$R/Ben's Polyphonic Pitch Shifter $V"; Z="$R/BensPolyphonicPitchShifter-$V-macOS.zip"
rm -rf "$R"; mkdir -p "$D/Plug-ins" "$D/Max/PolyPitch"
ditto build/plugin/PolyPitch_artefacts/Release/AU/PolyPitch.component "$D/Plug-ins/PolyPitch.component"
ditto build/plugin/PolyPitch_artefacts/Release/VST3/PolyPitch.vst3 "$D/Plug-ins/PolyPitch.vst3"
ditto max/PolyPitch/externals "$D/Max/PolyPitch/externals"; ditto max/PolyPitch/help "$D/Max/PolyPitch/help"
cp max/PolyPitch/package-info.json "$D/Max/PolyPitch/"; cp "$FROZEN_DEVICE" "$D/Max/$DEV"
cp LICENSE THIRD-PARTY.md "$D/"; sed -e "s/@VERSION@/$V/" -e "s/@DEVICE_NOTE@/$NOTE/" scripts/release-INSTALL.txt > "$D/INSTALL.txt"
find "$D" -name .DS_Store -delete

B=("$D/Plug-ins/PolyPitch.component" "$D/Plug-ins/PolyPitch.vst3" "$D/Max/PolyPitch/externals/polypitch~.mxo")
for b in "${B[@]}"; do codesign --force --options runtime --timestamp --sign "$SIGN_ID" "$b"; codesign --verify --strict "$b"; done
ditto -c -k --keepParent "$D" "$R/notarize.zip"
xcrun notarytool submit "$R/notarize.zip" --keychain-profile "$NOTARY_PROFILE" --wait | tee "$R/notarize.log"
grep -q "status: Accepted" "$R/notarize.log" || { echo "notarization was not accepted (see $R/notarize.log)"; exit 1; }
for b in "${B[@]}"; do xcrun stapler staple "$b"; spctl --assess --type install "$b"; done
rm "$R/notarize.zip"; ditto -c -k --keepParent "$D" "$Z"
shasum -a 256 "$Z"
