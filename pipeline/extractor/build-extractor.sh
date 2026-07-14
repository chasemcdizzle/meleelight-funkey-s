#!/usr/bin/env bash
# pipeline/extractor/build-extractor.sh — build the engine-table extractor
# bundle with the upstream clone's OWN docker node:8 webpack toolchain
# (fix_plan §M1 task 2; recipe lineage: oracle/build-upstream.sh step 5).
#
#   1. copies extractor.entry.js -> $CLONE/src/__extractor__.js and
#      extractor.config.js -> $CLONE/bin/webpack/extractor.config.js
#      (untracked additions to the cache clone; never pushed anywhere)
#   2. docker node:8 webpack --config=bin/webpack/extractor.config.js
#      -> $CLONE/dist/js/extractor.js (assigns window.__tables from the
#      REAL executed modules)
#
# Idempotent: a stamp file records sha256(entry|config|upstream HEAD);
# matching stamp + existing bundle -> exit 0 without rebuilding.
# `--force` rebuilds unconditionally.
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CLONE="${MELEELIGHT_CLONE:-$HOME/.cache/meleelight-funkey-s/upstream}"
FORCE=0
[ "${1:-}" = "--force" ] && FORCE=1

log() { echo "[build-extractor] $*"; }

if [ ! -f "$CLONE/dist/meleelight.html" ]; then
  log "FAIL: $CLONE is not a built upstream clone; run oracle/build-upstream.sh first"
  exit 1
fi

HEAD_SHA="$(git -C "$CLONE" rev-parse HEAD)"
STAMP_WANT="$( (cat "$HERE/extractor.entry.js" "$HERE/extractor.config.js"; echo "$HEAD_SHA") | shasum -a 256 | cut -d' ' -f1)"
STAMP_FILE="$CLONE/dist/js/extractor.stamp"
BUNDLE="$CLONE/dist/js/extractor.js"

if [ "$FORCE" -eq 0 ] && [ -f "$BUNDLE" ] && [ -f "$STAMP_FILE" ] \
   && [ "$(cat "$STAMP_FILE")" = "$STAMP_WANT" ]; then
  log "up to date: $BUNDLE (stamp $STAMP_WANT; --force to rebuild)"
  exit 0
fi

log "copying entry + config into the clone"
cp "$HERE/extractor.entry.js" "$CLONE/src/__extractor__.js"
cp "$HERE/extractor.config.js" "$CLONE/bin/webpack/extractor.config.js"

log "building under docker node:8 (webpack extractor.config.js)"
docker run --rm --platform linux/amd64 -v "$CLONE":/app -w /app node:8 \
  bash -c "./node_modules/.bin/webpack --config=bin/webpack/extractor.config.js"

if ! grep -q "__tables" "$BUNDLE"; then
  log "FAIL: built bundle lacks __tables assignment"
  exit 1
fi
if ! grep -q "__stages" "$BUNDLE"; then
  log "FAIL: built bundle lacks __stages assignment"
  exit 1
fi
# God-module leak guard: the extractor surface is pure data construction
# (no DOM). Any "document." means an externals stub missed and main.js
# (or another DOM-touching module) entered the bundle.
if grep -q "document\." "$BUNDLE"; then
  log "FAIL: bundle references document. — god-module leaked past the externals stubs"
  exit 1
fi
echo "$STAMP_WANT" > "$STAMP_FILE"
log "OK: $BUNDLE"
