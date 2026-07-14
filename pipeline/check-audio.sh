#!/usr/bin/env bash
# Task-level done-check for fix_plan §M1 task 4 (audio conversion +
# executed-JS sound map). Proves, per the M1 contract:
#   1. extractor bundle present/current — main/sfx + main/music bundled
#      with upstream's own docker node:8 webpack toolchain and executed
#      (Howl capture shim; browser-parity window shim) -> window.__sounds,
#   2. byte-stability — two FRESH pipeline runs (204 wav + 8 ogg ffmpeg
#      conversions each) produce byte-identical manifests, hence identical
#      PCM/blob bytes,
#   3. integrity — every artifact re-hashes to its manifest entry,
#   4. coverage + pins — the frozen expected.json audio contract holds:
#      204 sfx blobs / 180 mapped sounds / 8 tracks, every blob's byte
#      length ≡ 0 mod frame size with sample counts in the manifest,
#      ffmpeg version + exact argv pinned (a different ffmpeg FAILS,
#      never drifts), aggregate artifact sha256 pinned, and provenance
#      marks the content Nintendo-derived PRIVATE USE ONLY,
#   5. no-commit guard — blobs live only in gitignored build output;
#      nothing under pipeline/build/ is tracked or staged.
# Prints AUDIO OK and exits 0 on success.
set -euo pipefail
cd "$(dirname "$0")"

DIST="${MELEELIGHT_CLONE:-$HOME/.cache/meleelight-funkey-s/upstream}"

bash extractor/build-extractor.sh

rm -rf build/audio-a build/audio-b
node run.js --only audio --dist "$DIST" --out build/audio-a
node run.js --only audio --dist "$DIST" --out build/audio-b

cmp build/audio-a/manifest.json build/audio-b/manifest.json \
  || { echo "FAIL: manifests differ between fresh runs (byte-stability)"; exit 1; }
echo "byte-stability: two fresh runs -> identical manifest.json"

node lib/verify-artifacts.js build/audio-a
node lib/verify-artifacts.js build/audio-b
node lib/check-expected.js build/audio-a "$DIST" audio

# Nintendo-derived blobs must never enter git: the build dir is ignored
# and nothing under it is tracked or staged.
git check-ignore -q build/audio-a \
  || { echo "FAIL: pipeline/build is not gitignored"; exit 1; }
TRACKED="$(git ls-files build; git status --porcelain -- build)"
[ -z "$TRACKED" ] \
  || { echo "FAIL: files under pipeline/build are tracked/staged:"; echo "$TRACKED"; exit 1; }
echo "no-commit guard: build output gitignored, nothing tracked"

echo "AUDIO OK"
