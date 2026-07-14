#!/usr/bin/env bash
# M2 task 4 done-check: actionStateShortcuts + state-machine scaffolding +
# C mulberry32 + sound-event queue seam — capture + bit-exact replay.
# For each captured golden (g01 g04 g06):
#   - two fresh asshort capture runs produce byte-identical JSONL
#     (determinism; includes the fixed synthetic-domain sweep with its
#     swapped-RNG KO-shout calls and the restored player[3] injection)
#   - BOTH runs' checksum streams pass the unchanged verify-stream.js
#     against the frozen golden (instrumentation non-perturbation guard —
#     also proves the sweep's RNG swap and slot-3 injection perturb
#     nothing)
#   - measured-then-frozen pins hold (record/function counts, undef-ret
#     allowlist, post-state field set) + the rngBoot record pins seed and
#     the 465 boot draws (the qjs boot-pin class)
#   - EVERY record replays through the C translations bit-identically
#     (--strict): ret AND the {dsp,mut,rng,snd} post-state; the seeded
#     mulberry32 stream is chained draw-for-draw across the whole file
#     (incl. the single off-step pre-frame-1 startGame draw, asserted);
#     charAttributes/intangibility reads go through the M1 CTAB1 generated
#     tables (ml_tables), regenerated here by the executed-JS pipeline.
# Leaves run A's artifacts in port/sim/calib/build/ as the canonical
# captures. Prints ASSHORT MATCH, exit 0. Never weakened: exact equality
# only.
set -euo pipefail
cd "$(dirname "$0")/../../.."

CAL=port/sim/calib
BUILD=$CAL/build
TABLES=pipeline/build/asshort-tables
mkdir -p "$BUILD"

# M1 data plane (FORMATS.md section 3): the generated CTAB1 tables the C
# translations read attributes/intangibility from. build-extractor.sh is
# stamp-cached; the pipeline run is executed-JS, deterministic.
bash pipeline/extractor/build-extractor.sh
node pipeline/run.js --only animations,tables --out "$TABLES"
test -f "$TABLES/ml_tables.c"

# build: every TU with -ffp-contract=off (PLAN §2)
cc -O2 -ffp-contract=off -Wall -Wextra -Werror \
  -I"$TABLES" \
  -o "$BUILD/asshort_replay" \
  "$CAL/replay_asshort.c" "$CAL/input_canon.c" "$CAL/canon.c" \
  port/sim/action_state_shortcuts.c port/sim/ml_events.c \
  "$TABLES/ml_tables.c" \
  port/fdlibm/fdlibm.c -lm
echo "build OK: $BUILD/asshort_replay (cc -O2 -ffp-contract=off)"

for id in g01 g04 g06; do
  name=$(node -e "const m=require('./oracle/goldens/manifest.json');console.log(m.goldens.find(g=>g.id==='$id').name)")
  echo "== $id ($name): asshort capture run A"
  node "$CAL/run-capture.js" --spec asshort --golden "$id" \
    --out-jsonl "$BUILD/$id.asshort.jsonl" --out-run "$BUILD/$id.asshort-run.json"
  echo "== $id: asshort capture run B"
  node "$CAL/run-capture.js" --spec asshort --golden "$id" \
    --out-jsonl "$BUILD/$id.asshort.b.jsonl" --out-run "$BUILD/$id.asshort-run.b.json"
  cmp "$BUILD/$id.asshort.jsonl" "$BUILD/$id.asshort.b.jsonl"
  echo "   capture byte-identical across two fresh runs"
  node oracle/harness/verify-stream.js "$BUILD/$id.asshort-run.json"   "oracle/goldens/$name.sha256.json"
  node oracle/harness/verify-stream.js "$BUILD/$id.asshort-run.b.json" "oracle/goldens/$name.sha256.json"
  node "$CAL/check-spec-pins.js" asshort "$id" "$BUILD/$id.asshort.jsonl" "$BUILD/$id.asshort-run.json"
  # rngBoot pin: first record must be rngBoot with THIS golden's seed and
  # the frozen boot-draw count (oracle/CHECKSUM.md section 6 + qjs boot pin)
  node -e '
    const fs = require("fs");
    const exp = require("./port/sim/calib/expected-capture-asshort.json");
    const m = require("./oracle/goldens/manifest.json");
    const g = m.goldens.find((x) => x.id === process.argv[1]);
    const first = fs.readFileSync(process.argv[2], "utf8").split("\n")[0].split("\t");
    const buf = new ArrayBuffer(8);
    const f64 = new Float64Array(buf);
    const u32 = new Uint32Array(buf);
    f64[0] = 1.0;
    const HI = u32[1] === 0x3ff00000 ? 1 : 0, LO = 1 - HI;
    const dhex = (x) => { f64[0] = x;
      return "d:" + (u32[HI] >>> 0).toString(16).padStart(8, "0") +
                    (u32[LO] >>> 0).toString(16).padStart(8, "0"); };
    const want = "[" + dhex(g.seed) + "," + dhex(exp.rngBootDraws) + "]";
    if (first[1] !== "rngBoot" || first[2] !== want) {
      console.error(`RNG BOOT PIN FAIL ${process.argv[1]}: got ${first[1]} ${first[2]}, want rngBoot ${want}`);
      process.exit(1);
    }
    console.log(`${process.argv[1]}: rngBoot pin OK (seed ${g.seed}, ${exp.rngBootDraws} boot draws)`);
  ' "$id" "$BUILD/$id.asshort.jsonl"
  rm -f "$BUILD/$id.asshort.b.jsonl" "$BUILD/$id.asshort-run.b.json"
  # bit-exact replay: ret + post-state {dsp,mut,rng,snd}, chained RNG
  "$BUILD/asshort_replay" "$BUILD/$id.asshort.jsonl" --strict --max-print 5
done

# no-commit guard: captures/tables are build output, never tracked
if git status --porcelain -- "$BUILD" "$TABLES" | grep -q .; then
  echo "ASSHORT FAIL: build output not gitignored" >&2
  exit 1
fi

echo "ASSHORT MATCH"
