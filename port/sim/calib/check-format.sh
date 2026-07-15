#!/usr/bin/env bash
# M2 task 15 done-check: the ECMAScript shortest-round-trip float
# formatter (port/sim/ml_fmt.c: vendored Ryu core + the ECMA-262
# §6.1.6.1.20 formatting layer) + the CHECKSUM.md §3 `ser` / §4 hash
# (port/sim/ml_ser.c) — proven byte-identical to the JS oracle by
# DIFFERENTIAL testing, never by construction:
#   0. the vendored Ryu files are byte-verbatim at the pinned commit
#      (port/ryu/PROVENANCE.sha256; NOTICES entry)
#   1. self-test: SHA-256 NIST vectors + frozen String/ser/escape anchors
#      (incl. -0, NaN payloads, the 1e21 / 1e-7 exponent thresholds,
#      5e-324, 2^53-boundary integers)
#   2. adversarial corpus (~5.47M patterns, deterministic seeded
#      generator, corpus sha256 pinned): every double formatted by C
#      (String(x) AND the ser -0 rule) vs node/V8 + the ORACLE'S OWN
#      numStr extracted from oracle/harness/pagelib.js source bytes —
#      cmp, byte-exact; C output also byte-stable across two runs
#   3. captured doubles: every unique `d:<hex16>` bit pattern in EVERY
#      capture file under port/sim/calib/build/ (all specs, all goldens
#      present; records g01 player+article first when absent, STREAM
#      MATCH guarded) through the same C-vs-JS differential
#   4. composite ser: every g01 player post snapshot + article envelope
#      re-serialized from parsed canon by the C ser AND full CHECKSUM.md
#      §2/§3.1 frame envelopes (fixed-literal key order, p0..p3 +
#      articles) built by C vs the oracle's actual __serializeState /
#      __sha256 (pagelib.js:41-73) — per-case SHA-256 + byte length, cmp
#   5. measured-then-frozen pins (expected-format.json)
# Prints FORMAT MATCH, exit 0. Never weakened: exact equality only.
set -euo pipefail
cd "$(dirname "$0")/../../.."

CAL=port/sim/calib
BUILD=$CAL/build
mkdir -p "$BUILD"

# 0. vendored-Ryu provenance (byte-verbatim at the pinned commit)
shasum -a 256 -c port/ryu/PROVENANCE.sha256 >/dev/null
echo "ryu provenance OK (port/ryu/PROVENANCE.sha256)"

# 1. build: every TU with -ffp-contract=off (PLAN §2)
cc -O2 -ffp-contract=off -Wall -Wextra -Werror \
  -Iport/ryu -Iport/sim -Ioracle/qjs \
  -o "$BUILD/fmt_diff" \
  "$CAL/fmt_diff.c" "$CAL/canon.c" port/sim/ml_ser.c port/sim/ml_fmt.c \
  oracle/qjs/sha256.c -lm
echo "build OK: $BUILD/fmt_diff (cc -O2 -ffp-contract=off)"

# 2. self-test: SHA-256 NIST vectors + frozen formatter/ser anchors
"$BUILD/fmt_diff" --self-test

# 3. adversarial differential (C formatter vs V8 String(x) + oracle numStr)
"$BUILD/fmt_diff" --gen "$BUILD/fmt-adv.hex"
node "$CAL/check-format-pins.js" adversarial "$BUILD/fmt-adv.hex"
"$BUILD/fmt_diff" --format "$BUILD/fmt-adv.hex" "$BUILD/fmt-adv.c.txt"
"$BUILD/fmt_diff" --format "$BUILD/fmt-adv.hex" "$BUILD/fmt-adv.c2.txt"
cmp "$BUILD/fmt-adv.c.txt" "$BUILD/fmt-adv.c2.txt"
echo "   C formatter output byte-stable across two runs"
node "$CAL/fmt-js-ref.js" "$BUILD/fmt-adv.hex" "$BUILD/fmt-adv.js.txt"
cmp "$BUILD/fmt-adv.c.txt" "$BUILD/fmt-adv.js.txt"
echo "   adversarial differential: bit-identical (String + ser columns)"

# 4. captured-doubles differential — g01 player+article recorded first if
# absent (STREAM MATCH + spec pins guarded, the standard rig recipe)
gname=$(node -e "const m=require('./oracle/goldens/manifest.json');console.log(m.goldens.find(g=>g.id==='g01').name)")
for spec in player article; do
  if [ ! -f "$BUILD/g01.$spec.jsonl" ]; then
    echo "== recording missing g01 $spec capture"
    node "$CAL/run-capture.js" --spec "$spec" --golden g01 \
      --out-jsonl "$BUILD/g01.$spec.jsonl" --out-run "$BUILD/g01.$spec-run.json"
    node oracle/harness/verify-stream.js "$BUILD/g01.$spec-run.json" \
      "oracle/goldens/$gname.sha256.json"
    node "$CAL/check-spec-pins.js" "$spec" g01 \
      "$BUILD/g01.$spec.jsonl" "$BUILD/g01.$spec-run.json"
  fi
done
"$BUILD/fmt_diff" --extract "$BUILD/fmt-cap.hex" "$BUILD"/*.jsonl
node "$CAL/check-format-pins.js" captured "$BUILD/fmt-cap.hex"
"$BUILD/fmt_diff" --format "$BUILD/fmt-cap.hex" "$BUILD/fmt-cap.c.txt"
node "$CAL/fmt-js-ref.js" "$BUILD/fmt-cap.hex" "$BUILD/fmt-cap.js.txt"
cmp "$BUILD/fmt-cap.c.txt" "$BUILD/fmt-cap.js.txt"
echo "   captured-doubles differential: bit-identical (String + ser columns)"

# 5. composite ser differential (C ser/envelope/SHA-256 vs the oracle's
# own pagelib.js code over the g01 captures)
node "$CAL/fmt-composite.js" gen \
  "$BUILD/g01.player.jsonl" "$BUILD/g01.article.jsonl" "$BUILD/fmt-comp-in.txt"
node "$CAL/check-format-pins.js" composite "$BUILD/fmt-comp-in.txt"
"$BUILD/fmt_diff" --composite "$BUILD/fmt-comp-in.txt" "$BUILD/fmt-comp-c.txt"
node "$CAL/fmt-composite.js" ref "$BUILD/fmt-comp-in.txt" "$BUILD/fmt-comp-js.txt"
cmp "$BUILD/fmt-comp-c.txt" "$BUILD/fmt-comp-js.txt"
echo "   composite ser differential: per-case SHA-256 + length bit-identical"

# tidy the large derivable intermediates (captures stay: canonical artifacts)
rm -f "$BUILD"/fmt-adv.hex "$BUILD"/fmt-adv.c.txt "$BUILD"/fmt-adv.c2.txt \
      "$BUILD"/fmt-adv.js.txt "$BUILD"/fmt-cap.hex "$BUILD"/fmt-cap.c.txt \
      "$BUILD"/fmt-cap.js.txt "$BUILD"/fmt-comp-in.txt "$BUILD"/fmt-comp-c.txt \
      "$BUILD"/fmt-comp-js.txt

# no-commit guard: build output is never tracked
if git status --porcelain -- "$BUILD" | grep -q .; then
  echo "FORMAT FAIL: build output not gitignored" >&2
  exit 1
fi

echo "FORMAT MATCH"
