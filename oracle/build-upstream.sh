#!/usr/bin/env bash
# oracle/build-upstream.sh — pinned upstream meleelight clone + harness
# patch + build, as ONE committed, idempotent tool (M0 task 1).
#
# Recipe provenance: proven twice (determinism spike issue #7, control
# prototype issue #9); see spikes/determinism/README.md §Repro and
# docs/research/determinism-spike.md. Steps:
#   1. clone schmooblidon/meleelight (read-only; NEVER pushed) OUTSIDE the
#      tree at ${MELEELIGHT_CLONE:-$HOME/.cache/meleelight-funkey-s/upstream}
#   2. checkout the project pin 27af171
#   3. git apply oracle/meleelight-harness.patch (harness seams + seeded-PRNG
#      isolation; maintained copy of the frozen spike patch)
#   4. prune the dead 2018 devDeps that break `npm install` in 2026 —
#      deepstream.io (git-URL uws dependency) and electron* — plus the
#      postinstall script, from package.json
#   5. build under docker node:8 (amd64-under-emulation is fine):
#      npm install --ignore-scripts && npm run animations && npm run build
#
# Idempotent: exits 0 without rebuilding when the clone is already at the
# pin, patched, and built. `--force` rebuilds from the pristine pin.
set -euo pipefail

ORACLE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CLONE="${MELEELIGHT_CLONE:-$HOME/.cache/meleelight-funkey-s/upstream}"
PIN=27af171
UPSTREAM_URL=https://github.com/schmooblidon/meleelight
PATCH="$ORACLE_DIR/meleelight-harness.patch"
FORCE=0
[ "${1:-}" = "--force" ] && FORCE=1

log() { echo "[build-upstream] $*"; }

is_built() {
  [ -f "$CLONE/dist/meleelight.html" ] \
    && grep -rq __harnessInputs "$CLONE/dist/js/" 2>/dev/null \
    && case "$(git -C "$CLONE" rev-parse HEAD 2>/dev/null)" in
         "$PIN"*) true ;; *) false ;; esac
}

if [ "$FORCE" -eq 1 ]; then
  log "--force: removing $CLONE"
  rm -rf "$CLONE"
fi

if is_built; then
  log "already at pin $PIN, patched, and built: $CLONE (use --force to rebuild)"
  exit 0
fi

# --- 1. clone (outside the tree) -------------------------------------------
if [ ! -d "$CLONE/.git" ]; then
  mkdir -p "$(dirname "$CLONE")"
  log "cloning $UPSTREAM_URL -> $CLONE"
  git clone "$UPSTREAM_URL" "$CLONE"
fi

# --- 2. pristine pin --------------------------------------------------------
log "checking out pin $PIN (pristine)"
git -C "$CLONE" checkout -f "$PIN"
# drop stale untracked leftovers from prior attempts, but keep the two big
# reusable output dirs (node_modules survives; dist is rebuilt below anyway)
git -C "$CLONE" clean -fd -e node_modules -e dist >/dev/null

# --- 3. harness patch -------------------------------------------------------
log "applying $(basename "$PATCH")"
git -C "$CLONE" apply "$PATCH"

# --- 4. prune dead devDeps + postinstall ------------------------------------
# (node:8 runs the prune too — no host-node dependency)
cat > "$CLONE/.oracle-prune.js" <<'EOF'
// Prune package.json for the 2026 rebuild of the 2018 tree (see
// docs/research/determinism-spike.md): deepstream.io drags a dead git-URL
// dependency (uws), electron* are unfetchable/unneeded, and postinstall
// would re-run the build inside npm install.
var fs = require("fs");
var p = JSON.parse(fs.readFileSync("package.json", "utf8"));
if (p.scripts) { delete p.scripts.postinstall; }
if (p.devDependencies) {
  Object.keys(p.devDependencies).forEach(function (k) {
    if (k === "deepstream.io" || k.indexOf("electron") === 0) {
      delete p.devDependencies[k];
    }
  });
}
fs.writeFileSync("package.json", JSON.stringify(p, null, 2) + "\n");
console.log("[prune] package.json pruned (deepstream.io, electron*, postinstall)");
EOF

# --- 5. build under docker node:8 -------------------------------------------
log "building under docker node:8 (npm install + animations + build; slow under emulation)"
docker run --rm --platform linux/amd64 -v "$CLONE":/app -w /app node:8 \
  bash -c "node .oracle-prune.js && npm install --ignore-scripts && npm run animations && npm run build"
rm -f "$CLONE/.oracle-prune.js"

# --- verify ------------------------------------------------------------------
if is_built; then
  log "OK: $CLONE built at pin $PIN with harness patch"
  exit 0
fi
log "FAIL: post-build verification failed (dist/meleelight.html, dist/js __harnessInputs, or pin mismatch)"
exit 1
