#!/usr/bin/env bash
# Task-level done-check for fix_plan §M4 task A9 (upstream menu artwork ->
# device-loadable IMG1 + its C loader). Proves, per the M1 contract:
#   1. byte-stability — two FRESH pipeline runs (15 PNG decodes + resamples
#      + one packed file each) produce byte-identical manifests, and the
#      emitted menu.img1 is compared byte-for-byte across the two runs,
#   2. integrity — every artifact re-hashes to its manifest entry,
#   3. coverage + pins — the frozen expected-assets.json contract holds:
#      15 images (5 portraits / 7 stage previews / 3 cursors), each image's
#      measured source size, source colour type, MEASURED alpha class before
#      and after scaling, emitted size, plus the aggregate artifact sha256,
#   4. decoder differential — our stdlib-zlib PNG decoder (lib/png.js)
#      agrees byte-for-byte with ffmpeg's independent decoder on all 15
#      source PNGs (PNG decoding is lossless: disagreement is a bug, not a
#      version difference — contrast §5.2's resampler pin),
#   5. resampler validation (lib/assets-selftest.js) — the production
#      resampler must agree BYTE-FOR-BYTE with an independent exact-rational
#      BigInt area-average on six real images, plus impulse/gradient/
#      tie/halo/constant fixtures and one tooth per decoder rejection arm,
#   6. C loader round trip — port/gfx/img1.c parses the REAL emitted
#      artifact and its canonical dump is byte-identical to the independent
#      JS reader's; the directory ORDER it observes matches the frozen pin
#      (and a name-swapped file is caught); the lookup API resolves both
#      directions; img1_blit is bit-identical to the raster's own
#      rast_blit_rgba over 6 offsets x 2 clip bands x ink on/off, each
#      required to be non-vacuous,
#   7. loader teeth — one corrupted copy per validation arm in img1_open,
#      each asserted to produce that arm's EXACT diagnostic,
#   8. no-commit guard — this Nintendo-derived artwork lives only in
#      gitignored build output; nothing under pipeline/build/ is tracked
#      or staged, and no tracked file anywhere in the repo has the CONTENT
#      of any source PNG or of the emitted menu.img1.
# Prints ASSETS OK and exits 0 on success.
set -euo pipefail
cd "$(dirname "$0")"

DIST="${MELEELIGHT_CLONE:-$HOME/.cache/meleelight-funkey-s/upstream}"
BUILD=build/assets-check

rm -rf build/assets-a build/assets-b "$BUILD"
node run.js --only assets --dist "$DIST" --out build/assets-a
node run.js --only assets --dist "$DIST" --out build/assets-b

cmp build/assets-a/manifest.json build/assets-b/manifest.json \
  || { echo "FAIL: manifests differ between fresh runs (byte-stability)"; exit 1; }
cmp build/assets-a/assets/menu.img1 build/assets-b/assets/menu.img1 \
  || { echo "FAIL: menu.img1 differs between fresh runs (byte-stability)"; exit 1; }
echo "byte-stability: two fresh runs -> identical manifest.json + menu.img1"

node lib/verify-artifacts.js build/assets-a
node lib/verify-artifacts.js build/assets-b
node lib/check-assets-expected.js build/assets-a "$DIST"

# The PNG decoder is ours, so it is judged by an independent implementation.
node lib/assets-decode-diff.js "$DIST"
# ...and the resampler, which has no external partner, is judged by its own
# invariants (exact weights, premultiplied alpha, rejection domain).
node lib/assets-selftest.js

# ---- C loader: parse + blit, against the REAL emitted artifact ------------
# raster.c is the ONE -O3 TU in the shipped build (CLAUDE.md); this is a
# host CHECK binary, not shipped, and every op on this path is integer, so
# it is built at -O2 with the project's usual strict flags.
mkdir -p "$BUILD"
GFX=../port/gfx
cc -std=c99 -O2 -ffp-contract=off -Wall -Wextra -Werror -I"$GFX" \
  -o "$BUILD/img1_check" "$GFX/img1_check.c" "$GFX/img1.c" "$GFX/raster.c" \
  ../port/fdlibm/fdlibm.c -lm

IMG=build/assets-a/assets/menu.img1
"$BUILD/img1_check" --blit "$IMG"
"$BUILD/img1_check" --dump "$IMG" > "$BUILD/c.dump"
node lib/img1-dump.js "$IMG" > "$BUILD/js.dump"
cmp "$BUILD/c.dump" "$BUILD/js.dump" \
  || { echo "FAIL: C loader dump != independent JS reader dump"; exit 1; }
echo "loader round trip: C parse == JS reader ($(wc -l < "$BUILD/c.dump" | tr -d ' ') lines)"

# The directory ORDER the C loader actually sees must equal the frozen pin.
# Nothing else binds them: the C/JS dumps agree with each other whatever the
# order is, and perImage is keyed by name (review-a9-2 [M]). Every dump line
# is validated against an anchored grammar, not scavenged with a prefix
# filter (review-a9-4 [M]). Proven both ways — the real file matches, and a
# file with two names swapped does not.
node lib/img1-dump-order.js "$BUILD/c.dump"

node -e '
  const fs = require("fs");
  const b = fs.readFileSync(process.argv[1]);
  const DIR = 12, E = 24, n0 = Buffer.from(b.slice(DIR, DIR + 16));
  b.copy(b, DIR, DIR + E, DIR + E + 16);   // entry0.name := entry1.name
  n0.copy(b, DIR + E);                     // entry1.name := old entry0.name
  fs.writeFileSync(process.argv[2], b);
' "$IMG" "$BUILD/swapped.img1"
"$BUILD/img1_check" --dump "$BUILD/swapped.img1" > "$BUILD/swapped.dump" \
  || { echo "FAIL: loader rejected the name-swap file (it is structurally legal)"; exit 1; }
node lib/img1-dump-order.js "$BUILD/swapped.dump" --expect-mismatch

# ---- teeth: a damaged file must be REJECTED, for the RIGHT REASON --------
# One mode per validation arm in img1_open, and each tooth asserts the EXACT
# error that arm produces. Rejection alone is not enough: round 2 of review
# found two teeth that corrupted HEADER bytes while claiming to test NAME
# validation — they were rejected as "bad magic"/"implausible image count",
# so deleting the padding or charset checks left every tooth green. Binding
# each tooth to its message is what makes "one tooth per arm" true.
# The expected text is the COMPLETE diagnostic and is compared with string
# EQUALITY, not a substring match: a fragment can be satisfied by a longer
# unrelated message from a different arm.
TEETH="shortfile:img1_open: implausible file size
magic:img1_open: bad magic
truncate:img1_open: header size != file size
zerocount:img1_open: implausible image count
bigcount:img1_open: directory overruns file
noname:img1_open: name field not NUL-terminated
emptyname:img1_open: empty image name
badname:img1_open: image name outside [a-z0-9_]
padding:img1_open: non-zero bytes after name terminator
dupname:img1_open: duplicate image name
zerodim:img1_open: zero-sized image
align:img1_open: pixel data not 4-aligned
dataoff0:img1_open: pixel data overlaps the directory
overlap:img1_open: overlapping image blocks
offset:img1_open: image data overruns file"

NTEETH=0
while IFS=: read -r mode want; do
  [ -n "$mode" ] || continue
  node -e '
    const fs = require("fs");
    const b = fs.readFileSync(process.argv[1]);
    const mode = process.argv[3];
    const DIR = 12, E = 24;   // header size, directory entry size
    // Entry 0 is "marth": name bytes DIR+0..DIR+15, terminator at DIR+5.
    switch (mode) {
      case "magic":     b[3] = 0x32; break;
      case "zerocount": b.writeUInt32LE(0, 4); break;
      case "bigcount":  b.writeUInt32LE(4000, 4); break;  // 12+4000*24 > file
      case "shortfile": break;                            // handled below
      case "noname":    b.fill(0x61, DIR, DIR + 16); break;      // no NUL at all
      case "emptyname": b[DIR] = 0; break;                       // empty name
      case "badname":   b[DIR + 1] = 0x2f; break;                // "/" not in [a-z0-9_]
      case "padding":   b[DIR + 6] = 0x41; break;                // after the terminator
      case "dupname":   b.copy(b, DIR + E, DIR, DIR + 16); break; // entry1 name := entry0
      case "zerodim":   b.writeUInt16LE(0, DIR + 16); break;
      case "align":     b.writeUInt32LE(b.readUInt32LE(DIR + 20) + 2, DIR + 20); break;
      case "dataoff0":  b.writeUInt32LE(0, DIR + 20); break;
      case "overlap":   b.writeUInt32LE(b.readUInt32LE(DIR + 20), DIR + E + 20); break;
      case "offset":    b.writeUInt32LE(0x7ffffff0, DIR + 20); break;
      case "truncate":  break;
      default: throw new Error("unknown mode " + mode);
    }
    const out = mode === "truncate" ? b.slice(0, b.length - 1)
      : mode === "shortfile" ? b.slice(0, 8)   // shorter than the 12-byte header
      : b;
    fs.writeFileSync(process.argv[2], out);
  ' "$IMG" "$BUILD/bad-$mode.img1" "$mode"
  if "$BUILD/img1_check" --dump "$BUILD/bad-$mode.img1" > /dev/null 2> "$BUILD/err-$mode.txt"; then
    echo "FAIL: loader ACCEPTED a corrupted file (mode $mode)"; exit 1
  fi
  GOT="$(cat "$BUILD/err-$mode.txt")"
  [ "$GOT" = "img1_check: $want" ] || {
    echo "FAIL: mode $mode was rejected for the WRONG reason"
    echo "  wanted: img1_check: $want"
    echo "  got:    $GOT"
    exit 1
  }
  NTEETH=$((NTEETH + 1))
done <<EOF
$TEETH
EOF
# The COUNT is asserted, not merely reported (review-a9-4 [L]): otherwise
# deleting a tooth from the table still ends in ASSETS OK.
EXPECT_TEETH=15
[ "$NTEETH" -eq "$EXPECT_TEETH" ] \
  || { echo "FAIL: ran $NTEETH loader teeth, expected $EXPECT_TEETH"; exit 1; }
echo "loader teeth: $NTEETH malformed files, each rejected by its OWN validation arm"

# Nintendo-derived artwork must never enter git. Three separate assertions
# (review-a9-1 [M]): the build dir is ignored, nothing under it is tracked
# or staged, and — because a copy ANYWHERE would distribute the artwork just
# as well — no .img1/.png artefact is tracked anywhere in the repo. The two
# git queries are run and rc-checked SEPARATELY: `$(cmd1; cmd2)` reports
# only the last command's status, so a failing first query used to be
# masked into a green "nothing tracked".
git check-ignore -q build/assets-a \
  || { echo "FAIL: pipeline/build is not gitignored"; exit 1; }
LISTED="$(git ls-files build)" \
  || { echo "FAIL: git ls-files build failed"; exit 1; }
STATUS="$(git status --porcelain -- build)" \
  || { echo "FAIL: git status -- build failed"; exit 1; }
[ -z "$LISTED$STATUS" ] \
  || { echo "FAIL: files under pipeline/build are tracked/staged:"; echo "$LISTED$STATUS"; exit 1; }
# Repo-wide, by CONTENT HASH rather than by filename: a name-based scan only
# caught *.img1, so committing an upstream PNG under any other name stayed
# green (review-a9-3 [M]). The forbidden set is the 15 Nintendo-derived
# source PNGs plus the emitted menu.img1; every tracked file in the repo is
# hashed and checked against it.
node lib/assets-nocommit-guard.js build/assets-a

echo "ASSETS OK"
