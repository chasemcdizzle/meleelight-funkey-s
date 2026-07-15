#!/usr/bin/env bash
# M2 task 13 done-check: articles (src/physics/article.js) — capture +
# bit-exact replay. For each captured golden (g01 g02 g08 — the article
# carriers, probe-MEASURED live coverage over all six fox/falco goldens:
# g01 fox lasers + 12 live zero-knockback hits on battlefield; g02 falco
# lasers + the ONLY live kb>0 hits (screenShake draws + live dispatch
# domain) on ystory; g08 fox CPU lasers + 21 live hits on fdest; g03/g05
# field ZERO live articles, g07's lasers never connect):
#   - two fresh article-spec capture runs produce byte-identical JSONL
#     (determinism; includes the 72-call rule-11/12 sweep — spawn arms,
#     movement/death ladders, the duplicate-destroy splice quirk, reflect/
#     powershield/shieldbreak/vCancel/crouch/CAPTUREDAMAGE/groundBounce/
#     blunthit arms — and the frame-0 mvData + hdFlags dumps whose
#     post-run finalCheck re-dumps hard-fail on in-match drift)
#   - BOTH runs' checksum streams pass the unchanged verify-stream.js
#     against the frozen golden (instrumentation non-perturbation guard)
#   - measured-then-frozen pins hold (record/function counts incl. the
#     zero pins on resetAArticles + the 6 internal-only collision helpers,
#     undef-ret allowlist, post-state field set)
#   - EVERY record replays through the C translation (port/sim/article.c
#     + the task-7 shared move bodies + task-6 hit_detection as the REAL
#     nested tree) bit-identically (--strict): pre/post queue envelopes
#     (aArticles/destroyArticleQueue/articleHitQueue — CHECKSUM.md §2's
#     `articles` key), the QUEUE CHAIN instrument across in-match records,
#     lean-when-empty envelope shapes, ainit spawn records (the task-8/9
#     article-seam FIFO crossings as real C bodies — the seam-to-body
#     conversion verified bit-exactly), the full seeded-RNG chain
#     draw-for-draw (frame-0 records on the separate sweep chain), every
#     mdispatch seam in call order (FORMAT.md "The article spec");
#     attributes/framesData reads go through the M1 CTAB1 generated
#     tables (ml_tables), regenerated here by the executed-JS pipeline.
# Leaves run A's artifacts in port/sim/calib/build/ as the canonical
# captures. Prints ARTICLE MATCH, exit 0. Never weakened: exact equality
# only.
set -euo pipefail
cd "$(dirname "$0")/../../.."

CAL=port/sim/calib
BUILD=$CAL/build
TABLES=pipeline/build/article-tables
mkdir -p "$BUILD"

# M1 data plane (FORMATS.md section 3): CTAB1 tables.
bash pipeline/extractor/build-extractor.sh
node pipeline/run.js --only animations,tables --out "$TABLES"
test -f "$TABLES/ml_tables.c"

# build: every TU with -ffp-contract=off (PLAN §2)
cc -O2 -ffp-contract=off -Wall -Wextra -Werror \
  -I"$TABLES" \
  -o "$BUILD/article_replay" \
  "$CAL/replay_article.c" "$CAL/player_canon.c" "$CAL/input_canon.c" \
  "$CAL/canon.c" \
  port/sim/article.c \
  port/sim/hit_detection.c port/sim/interpolated_collision.c \
  port/sim/environmental_collision.c \
  port/sim/characters/shared/moves_index.c \
  port/sim/characters/shared/moves/*.c \
  port/sim/action_state_shortcuts.c port/sim/ml_events.c \
  "$TABLES/ml_tables.c" \
  port/fdlibm/fdlibm.c -lm
echo "build OK: $BUILD/article_replay (cc -O2 -ffp-contract=off)"

for id in g01 g02 g08; do
  name=$(node -e "const m=require('./oracle/goldens/manifest.json');console.log(m.goldens.find(g=>g.id==='$id').name)")
  echo "== $id ($name): article capture run A"
  node "$CAL/run-capture.js" --spec article --golden "$id" \
    --out-jsonl "$BUILD/$id.article.jsonl" \
    --out-run "$BUILD/$id.article-run.json"
  echo "== $id: article capture run B"
  node "$CAL/run-capture.js" --spec article --golden "$id" \
    --out-jsonl "$BUILD/$id.article.b.jsonl" \
    --out-run "$BUILD/$id.article-run.b.json"
  cmp "$BUILD/$id.article.jsonl" "$BUILD/$id.article.b.jsonl"
  echo "   capture byte-identical across two fresh runs"
  node oracle/harness/verify-stream.js "$BUILD/$id.article-run.json"   "oracle/goldens/$name.sha256.json"
  node oracle/harness/verify-stream.js "$BUILD/$id.article-run.b.json" "oracle/goldens/$name.sha256.json"
  node "$CAL/check-spec-pins.js" article "$id" \
    "$BUILD/$id.article.jsonl" "$BUILD/$id.article-run.json"
  rm -f "$BUILD/$id.article.b.jsonl" "$BUILD/$id.article-run.b.json"
  # bit-exact replay: queue chain + post envelopes + RNG chains + seams
  "$BUILD/article_replay" "$BUILD/$id.article.jsonl" --strict --max-print 5
done

# no-commit guard: captures/tables are build output, never tracked
if git status --porcelain -- "$BUILD" "$TABLES" | grep -q .; then
  echo "ARTICLE FAIL: build output not gitignored" >&2
  exit 1
fi

echo "ARTICLE MATCH"
