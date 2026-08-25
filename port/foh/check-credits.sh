#!/usr/bin/env bash
# port/foh/check-credits.sh — HOST-ONLY tooth for punch-list A7, the CREDITS
# screen (upstream src/menus/credits.js, 422 lines; MENU-SPEC §8).
#
# WHAT A7 IS. The Options menu's CREDITS row emitted `deny` and a registered
# refusal. Upstream's credits are not a roll — they are a Star Fox shooting
# gallery: fourteen contributor names scroll up over a 100-star warp field and
# you shoot them with a twin-laser reticle, and hitting one prints that
# person's role and what they did. The port transliterates that, with
# DEVIATION D12 (relative reticle) and DEVIATION D38 (a FOH-local random
# stream) already registered in MENU-SPEC §8/§12.1.
#
# WHAT THIS PROVES, in two independent planes:
#
#  [A] THE NAMES ARE UPSTREAM'S, EXACTLY. The credits are OTHER PEOPLE'S
#      ATTRIBUTION, so getting one wrong or dropping one is the worst defect
#      this ticket can carry. Leg [2] re-extracts all fourteen
#      `new ScrollingText(...)` calls from the PINNED CLONE's own bytes and
#      requires foh.c's table to match row for row, in order — name, role,
#      blurb and starting yPos. Nothing is typed on either side of that
#      comparison: the C table is parsed out of foh.c and the JS table out of
#      credits.js.
#
#  [B] THE SCREEN THE OWNER SEES. Leg [3] runs
#      port/foh/foh_credits_witness.c against the REAL tree: the CREDITS row
#      is opened by the real gestures through the real foh_tick, a real
#      credited name is proved to be ON SCREEN by overdrawing it, a name is
#      SHOT with the real d-pad and A, the info panel is proved to name THE
#      PERSON WHO WAS SHOT, B is proved to return to Options with the cursor
#      still on CREDITS, and the 2500-frame timer exit is proved to fire on
#      its own.
#
#  Then legs [4]-[6] re-run the witness against three PERTURBED copies and
#  require each to fail, so neither plane can rot into a tautology. The three
#  teeth are deliberately ORTHOGONAL — content, reachability, and the seam
#  between the hit and the panel — and each is asserted to fail on ITS OWN
#  assertion and NOT on the others'.
#
# HOST-ONLY. Nothing here needs the device, an OPK or the sim tables. Host
# prerequisites: node (for the pipeline's `assets` stage, which the renderer
# hard-fails without) and the pinned upstream clone at $MELEELIGHT_CLONE.
# `bash port/foh/check-credits.sh` -> `CREDITS CHECK OK`, exit 0.
#
# NOT A GATE. This is a task-level tooth.
#
# DEVICE LEGS OWED (A7): the credits screen has never been rendered ON the
# FunKey-S, and no device rig knows about it. check-device-foh.sh's shot
# inventory and NAV_LINK measurements are untouched by this change and the
# screen is not in any committed flow, so nothing device-side regresses — but
# "it looks right at 240x240 on the real panel" is Chase's acceptance
# playthrough, not this check.
set -euo pipefail

FOH=port/foh
GFX=port/gfx
BUILD=$FOH/build/credits
DATA=$BUILD/data
CLONE="${MELEELIGHT_CLONE:-$HOME/.cache/meleelight-funkey-s/upstream}"

fail() { echo "CREDITS CHECK FAIL: $1" >&2; exit 1; }
grammar_die() { echo "CREDITS CHECK FAIL: $1" >&2; exit 2; }
relay_lines() { sed 's/^/  | /'; }

made() {
  local f
  for f in "$@"; do
    [ -s "$f" ] || fail "expected artifact missing/empty: $f"
  done
}

# --- [0] run lock (mkdir-atomic, NO reclaim — the check-legibility pattern) --
LOCK=$FOH/build/credits.lock
mkdir -p "$FOH/build"
mkdir "$LOCK" 2>/dev/null \
  || fail "another run holds $LOCK (remove it only if you are sure no run is live)"
trap 'rmdir "$LOCK" 2>/dev/null || true' EXIT

# --- [0a] no-commit guard ----------------------------------------------------
# A check must never write a tracked file. Carried verbatim from
# check-legibility.sh, including its two corrections: the `-f` test (a wholly
# untracked nested checkout is reported as a bare DIRECTORY) and the name list
# (so a directory appearing or disappearing stays visible even though its
# bytes are not walked).
tree_fingerprint() {
  local status diff files hashes f
  status="$(git status --porcelain)" || return 1
  diff="$(git diff)" || return 1
  files="$(git ls-files -o --exclude-standard)" || return 1
  files="$(printf '%s\n' "$files" | grep -v '^\.tokensave/' || true)"
  hashes=""
  while IFS= read -r f; do
    [ -n "$f" ] || continue
    [ -f "$f" ] || continue
    hashes="$hashes$(shasum -a 256 "$f")" || return 1
    hashes="$hashes"$'\n'
  done <<< "$files"
  printf '%s\n%s\n%s\n%s\n' "$status" "$diff" "$files" "$hashes" \
    | shasum -a 256 | cut -d' ' -f1
}
git_dirty_before="$(tree_fingerprint)" \
  || fail "could not fingerprint the working tree (guard fails CLOSED)"
[[ "$git_dirty_before" =~ ^[0-9a-f]{64}$ ]] \
  || fail "tree fingerprint is not a sha256 (guard fails CLOSED)"

rm -rf "$BUILD"
mkdir -p "$BUILD"

# --- [1] grammar pins --------------------------------------------------------
# The witness HAND COPIES two palette entries and the info panel's text
# geometry out of foh_render.c, because both are file-static there and a
# witness that guessed either would assert against a screen that no longer
# exists. Every copied line is pinned to EXACTLY ONE occurrence, the
# check-legibility.sh discipline.
echo "=== [1] grammar pins (what the witness hand-copies out of foh_render.c)"
made "$FOH/foh_render.c" "$FOH/foh_credits_witness.c" "$FOH/foh.c"
pin() { # <line> <what it is>
  local n
  n="$(grep -cxF "$1" "$FOH/foh_render.c")" || true
  [ "$n" = 1 ] || grammar_die "foh_render.c has $n copies of
  |$1|
  (want exactly 1) — $2. Re-read render_credits and re-sync
  port/foh/foh_credits_witness.c."
}
pin 'static const RastCol kCredWhite = {255, 255, 255, 256};' \
    'the colour an UNSHOT name and every panel string is drawn in'
pin 'static const RastCol kCredShotName = {227, 89, 89, 256}; // :365' \
    'the colour a SHOT name is repainted in'
pin '    cred_text_center(rz, 120, 181, c->name, kCredWhite);     // :303' \
    'the info panel`s NAME row — its centre x and its text y'
pin '    cred_text_center(rz, 120, 192, c->position, kCredWhite); // :305' \
    'the info panel`s ROLE row'
pin '    cred_text_center(rz, 215, 20, sc, kCredWhite);' \
    'the score readout`s centre x and text y'
pin '    foh_text(rz, rect[i].x, rect[i].y, 1, buf,' \
    'the scrolling name draw — it must blit at the SAME rect the witness
  hit-tests and overdraws, which is D4`s whole contract'
echo "   6 hand-copied lines still read exactly as the witness assumes"

# --- [2] the fourteen credits vs the PINNED CLONE ---------------------------
# NOTHING IS TYPED ON EITHER SIDE. The JS side is parsed out of credits.js's
# own bytes; the C side is parsed out of foh.c's `foh_credits` initialiser.
# A dropped contributor, a mis-spelled name, a reordered table or an edited
# blurb fails here.
echo "=== [2] the fourteen credits, re-extracted from $CLONE"
CREDJS="$CLONE/src/menus/credits.js"
[ -f "$CREDJS" ] \
  || fail "the pinned upstream clone is not at \$MELEELIGHT_CLONE ($CLONE) —
  this leg compares against upstream's OWN bytes and refuses to guess. See
  CLAUDE.md's 'Upstream clone + build'."
cat > "$BUILD/extract.js" <<'NODE'
"use strict";
const fs = require("fs");
const js = fs.readFileSync(process.env.CREDJS, "utf8");
const c = fs.readFileSync("port/foh/foh.c", "utf8");

// --- upstream side: every `new ScrollingText(text, y, position, information)`
const reJS = /new ScrollingText\(\s*"((?:[^"\\]|\\.)*)"\s*,\s*(\d+)\s*,\s*"((?:[^"\\]|\\.)*)"\s*,\s*\n?\s*"((?:[^"\\]|\\.)*)"\s*\)/g;
const up = [];
for (let m; (m = reJS.exec(js)); ) up.push([m[1], m[3], m[4], +m[2]]);

// --- port side: the `foh_credits[]` initialiser, string-literal by
// string-literal. Adjacent C literals are NOT concatenated here, because the
// table deliberately does not use that form — a blurb split across a line
// break is written as ONE literal on the next line.
const start = c.indexOf("const FohCredit foh_credits[FOH_CRED_NAMES] = {");
if (start < 0) throw new Error("foh.c has no foh_credits table");
const end = c.indexOf("\n};", start);
if (end < 0) throw new Error("foh.c's foh_credits table is not closed");
const body = c.slice(start, end);
const reC = /\{\s*"((?:[^"\\]|\\.)*)"\s*,\s*"((?:[^"\\]|\\.)*)"\s*,\s*\n?\s*"((?:[^"\\]|\\.)*)"\s*,\s*\n?\s*(\d+)\s*\}/g;
const port = [];
for (let m; (m = reC.exec(body)); ) port.push([m[1], m[2], m[3], +m[4]]);

const die = (s) => { console.error("credits: " + s); process.exit(1); };
if (up.length !== 14) die("upstream credits.js yielded " + up.length + " ScrollingText rows, want 14");
if (port.length !== 14) die("foh.c's foh_credits yielded " + port.length + " rows, want 14");
for (let i = 0; i < 14; i++) {
  for (let f = 0; f < 4; f++) {
    if (up[i][f] !== port[i][f]) {
      die("row " + i + " field " + ["name","role","blurb","yPos"][f] +
          " differs:\n  upstream: " + JSON.stringify(up[i][f]) +
          "\n  port:     " + JSON.stringify(port[i][f]));
    }
  }
}
// The renderer word-wraps a blurb into at most two 40-column lines. Assert
// the wrap is LOSSLESS (rejoining the lines gives the authored blurb back)
// and that two lines are enough for every one of the fourteen — the
// gfx_fatal in cred_wrap is a tripwire, and this is what keeps it one.
const COLS = 40;
for (const [name, , blurb] of port) {
  const words = blurb.toUpperCase().split(" ");
  const lines = [];
  let cur = "";
  for (const w of words) {
    if (w.length > COLS) die("blurb word too long for one line: " + w);
    const cand = cur ? cur + " " + w : w;
    if (cand.length <= COLS) cur = cand;
    else { lines.push(cur); cur = w; }
  }
  if (cur) lines.push(cur);
  if (lines.length > 2) die(name + "'s blurb needs " + lines.length + " lines (max 2)");
  if (lines.join(" ") !== blurb.toUpperCase()) die(name + "'s blurb does not survive the wrap");
}
console.log("CREDITS TABLE MATCHES UPSTREAM (14 rows, 4 fields each)");
for (const r of port) console.log("  " + r[0] + " | " + r[1]);
// The witness asserts UPSTREAM LITERALS, not the table it renders from (see
// its own header). Emit the upstream strings the witness is allowed to carry,
// uppercased for face 1, so the shell can pin the witness's #defines to them.
console.log("LIT NAME0 " + up[0][0].toUpperCase());
console.log("LIT NAME1 " + up[1][0].toUpperCase());
console.log("LIT ROLE1 " + up[1][1].toUpperCase());
NODE
credrc=0
CREDJS="$CREDJS" node "$BUILD/extract.js" > "$BUILD/credits.cmp" \
  2>"$BUILD/credits.err" || credrc=$?
if [ "$credrc" != 0 ]; then
  relay_lines < "$BUILD/credits.err"
  fail "the credits extraction/comparison did not run clean (rc $credrc)"
fi
relay_lines < "$BUILD/credits.cmp"
grep -qx 'CREDITS TABLE MATCHES UPSTREAM (14 rows, 4 fields each)' \
  "$BUILD/credits.cmp" \
  || fail "the credits table comparison did not print its exact verdict"
# Every credited NAME must actually appear in the committed C source, so a
# comparison that somehow matched two empty tables cannot pass.
while IFS= read -r nm; do
  grep -qF "\"$nm\"" "$FOH/foh.c" \
    || fail "credited name '$nm' is not a string literal in port/foh/foh.c"
done < <(sed -n 's/^  \([^|]*\) | .*$/\1/p' "$BUILD/credits.cmp")

# AND THE WITNESS'S OWN LITERALS ARE PINNED TO UPSTREAM. The witness
# deliberately does NOT assert `foh_credits[i].name` — that would be
# self-referential, and a table edited to a placeholder would be rendered as
# the placeholder and asserted as the placeholder. It carries three upstream
# strings as #defines instead, and this is what stops THOSE drifting.
pin_lit() { # <define> <token>
  local wantv
  wantv="$(sed -n "s/^LIT $2 //p" "$BUILD/credits.cmp")"
  [ -n "$wantv" ] || fail "the extractor emitted no LIT $2 row"
  local n
  n="$(grep -cF "#define $1 \"$wantv\"" "$FOH/foh_credits_witness.c")" || true
  [ "$n" = 1 ] \
    || grammar_die "port/foh/foh_credits_witness.c must define $1 as
  |$wantv| (upstream credits.js, uppercased for face 1) exactly once; found
  $n. The witness asserts that literal is ON SCREEN, so a drifted literal
  would assert the wrong person's name."
}
pin_lit CRED_W_NAME0 NAME0
pin_lit CRED_W_NAME1 NAME1
pin_lit CRED_W_ROLE1 ROLE1
echo "   the witness's three asserted literals are upstream's own strings"

# --- [3] renderer art (pipeline 'assets' stage, this check's own dir) -------
echo "=== [3] renderer art (pipeline 'assets' stage, this check's own dir)"
node pipeline/run.js --only assets --out "$DATA" > "$BUILD/assets.log" 2>&1 \
  || { relay_lines < "$BUILD/assets.log"
       fail "the pipeline's assets stage did not run (host prerequisite: node)"; }
made "$DATA/assets/menu.img1"
echo "   $DATA/assets/menu.img1 built"

# --- [4] the witness against the REAL tree ----------------------------------
CFLAGS_COMMON=(-ffp-contract=off -Wall -Wextra -Werror -Iport/ryu -Iport/sim
               -Ioracle/qjs)
CRED_SRCS=("$FOH/foh_credits_witness.c" "$FOH/foh.c" "$FOH/foh_font.c" "$GFX/gfx_glyphs.c"
           "$GFX/raster.c" "$GFX/img1.c" "$GFX/ctl_style.c" port/fdlibm/fdlibm.c)
CRED_OK='^CREDITS OK$'

echo "=== [4] credits witness (real gestures through real foh_tick)"
mkdir -p "$BUILD/real"
cc -O2 "${CFLAGS_COMMON[@]}" -o "$BUILD/real/cred" "$FOH/foh_render.c" \
  "${CRED_SRCS[@]}" -lm \
  || fail "the witness did not build against the real tree"
made "$BUILD/real/cred"
MLFK_DATA_DIR="$DATA" "$BUILD/real/cred" > "$BUILD/real/cred.out" 2>&1 \
  || { relay_lines < "$BUILD/real/cred.out"
       fail "the witness failed against the REAL tree"; }
relay_lines < "$BUILD/real/cred.out"
nres="$(grep -cF 'CREDITS OK' "$BUILD/real/cred.out")" || true
nexact="$(grep -cE "$CRED_OK" "$BUILD/real/cred.out")" || true
[ "$nres" = 1 ] && [ "$nexact" = 1 ] \
  || grammar_die "witness verdict: $nres line(s) resemble 'CREDITS OK' and
  $nexact match it exactly (want 1 and 1)"

# perturb_build <dir> <file> <pairs...> — derive a COPY of ONE source file
# with exact-string replacements, refusing any that is a no-op or that is not
# unique, then build the witness against that copy and require it to FAIL with
# exit code 1. check-legibility.sh's shape, generalised to either source file
# because A7's teeth live in two of them.
perturb_build() { # <dir> <relpath-of-file-to-perturb> <from> <to> [...]
  local dir="$1" src="$2"; shift 2
  local base; base="$(basename "$src")"
  mkdir -p "$BUILD/$dir"
  node -e '
    const fs = require("fs");
    const [src, dst, ...pairs] = process.argv.slice(1);
    let raw = fs.readFileSync(src, "utf8");
    for (let i = 0; i < pairs.length; i += 2) {
      const from = pairs[i], to = pairs[i + 1];
      const n = raw.split(from).length - 1;
      if (n !== 1) {
        console.error("perturb: " + n + " copies of |" + from + "| (want 1)");
        process.exit(1);
      }
      if (from === to) { console.error("perturb: no-op substitution"); process.exit(1); }
      raw = raw.replace(from, to);
    }
    fs.writeFileSync(dst, raw);
  ' "$src" "$BUILD/$dir/$base" "$@" \
    || fail "$dir: could not derive the perturbed $base"
  cmp -s "$BUILD/$dir/$base" "$src" \
    && fail "$dir: the perturbed copy is byte-identical to $src (dead tooth)"
  # Swap the perturbed copy in for the real file in the TU list. -Iport/foh so
  # the copy's quoted "foh.h" resolves to the real tree, exactly as
  # check-legibility.sh's copy-build does it.
  local srcs=() f
  for f in "$FOH/foh_render.c" "${CRED_SRCS[@]}"; do
    if [ "$f" = "$src" ]; then srcs+=("$BUILD/$dir/$base"); else srcs+=("$f"); fi
  done
  cc -O2 "${CFLAGS_COMMON[@]}" -Iport/foh -o "$BUILD/$dir/cred" \
    "${srcs[@]}" -lm \
    || fail "$dir: the perturbed copy did not build"
  local rc=0
  MLFK_DATA_DIR="$DATA" "$BUILD/$dir/cred" > "$BUILD/$dir/cred.out" 2>&1 || rc=$?
  [ "$rc" = 1 ] \
    || { relay_lines < "$BUILD/$dir/cred.out"
         fail "$dir: the perturbed build exited rc $rc (want exactly 1 — rc 0
  means the witness is BLIND to the defect it exists to guard)"; }
  grep -qE "$CRED_OK" "$BUILD/$dir/cred.out" \
    && fail "$dir: the perturbed build still printed the OK verdict"
  return 0
}

# --- [5] T1: a PLACEHOLDER credit ------------------------------------------
# The defect this guards is the one the ticket names outright: a credits
# screen that shows a placeholder instead of a person. The first credit's name
# is replaced and the ON-SCREEN assertion must catch it.
echo "=== [5] T1: a placeholder credit must fail the witness"
perturb_build t1-placeholder "$FOH/foh.c" \
  '    {"Schmoo", "Creator, Main Developer", "Made the game.", 800},' \
  '    {"Placeholder", "Creator, Main Developer", "Made the game.", 800},'
grep -qF 'CREDITS FAIL: the credit "SCHMOO" (credits.js:115) is ON SCREEN' \
  "$BUILD/t1-placeholder/cred.out" \
  || { relay_lines < "$BUILD/t1-placeholder/cred.out"
       fail "T1: the ON-SCREEN name assertion did not fail — a placeholder
  would ship unnoticed"; }
grep -qF 'CREDITS FAIL: TOOTH: the same spot does NOT read a placeholder' \
  "$BUILD/t1-placeholder/cred.out" \
  || { relay_lines < "$BUILD/t1-placeholder/cred.out"
       fail "T1: the placeholder tooth did not fire — the witness looked at
  the right spot and did not see the placeholder that is provably drawn
  there, so the two halves disagree and neither can be trusted"; }
grep -qF 'CREDITS FAIL: B leaves the credits for Options' \
  "$BUILD/t1-placeholder/cred.out" \
  && { relay_lines < "$BUILD/t1-placeholder/cred.out"
       fail "T1: the EXIT assertion failed too. Renaming a credit moves no
  exit arm, so the copy differs from foh.c by more than the one table row"; }
echo "   T1: a placeholder credit is caught on screen (rc 1)"

# --- [6] T2: the B exit disabled -------------------------------------------
# "A credits screen you cannot exit is a stub." This reverts the manual exit
# and requires the exit assertions — and ONLY those — to fail.
echo "=== [6] T2: a credits screen you cannot leave must fail the witness"
perturb_build t2-noexit "$FOH/foh.c" \
  '  if (bE) {
    s->credInit = true;
    snd_push(s, "menuBack"); // :236' \
  '  if (bE && 0) { // T2: the manual exit disabled
    s->credInit = true;
    snd_push(s, "menuBack"); // :236'
grep -qF 'CREDITS FAIL: B leaves the credits for Options' \
  "$BUILD/t2-noexit/cred.out" \
  || { relay_lines < "$BUILD/t2-noexit/cred.out"
       fail "T2: the EXIT assertion did not fail — a screen with no way out
  would ship"; }
grep -qF 'CREDITS FAIL: the credit "SCHMOO" (credits.js:115) is ON SCREEN' \
  "$BUILD/t2-noexit/cred.out" \
  && { relay_lines < "$BUILD/t2-noexit/cred.out"
       fail "T2: the ON-SCREEN NAME assertion failed too. Disabling the B arm
  moves no glyph, so the copy differs from foh.c by more than the exit"; }
echo "   T2: an inescapable credits screen is caught (rc 1)"

# --- [7] T3: the info panel keyed to a constant ----------------------------
# The seam between the HIT and the PANEL: shooting Tatatat0 must print
# Tatatat0's own name and role, not whichever credit the renderer reaches
# first. This is the defect class CONTEXT.md calls a `seam` — both planes'
# own checks pass and nothing asserts the crossing.
echo "=== [7] T3: an info panel that names the wrong person must fail"
perturb_build t3-wrongperson "$FOH/foh_render.c" \
  '    const FohCredit *c = &foh_credits[s->credHitIdx];' \
  '    const FohCredit *c = &foh_credits[0]; // T3: ignores which was shot'
grep -qF 'CREDITS FAIL: the info panel NAMES the credit that was shot' \
  "$BUILD/t3-wrongperson/cred.out" \
  || { relay_lines < "$BUILD/t3-wrongperson/cred.out"
       fail "T3: the panel-names-the-right-person assertion did not fail —
  the hit->panel seam is unguarded"; }
grep -qF 'CREDITS FAIL: the score is exactly 1' \
  "$BUILD/t3-wrongperson/cred.out" \
  && { relay_lines < "$BUILD/t3-wrongperson/cred.out"
       fail "T3: the SCORE assertion failed too. Re-keying the panel changes
  no score, so the copy differs from foh_render.c by more than the one line"; }
grep -qF 'CREDITS FAIL: B leaves the credits for Options' \
  "$BUILD/t3-wrongperson/cred.out" \
  && { relay_lines < "$BUILD/t3-wrongperson/cred.out"
       fail "T3: the EXIT assertion failed too — same reason as above"; }
echo "   T3: a panel that names the wrong person is caught (rc 1)"

# --- [8] no-commit guard -----------------------------------------------------
git_dirty_after="$(tree_fingerprint)" \
  || fail "could not fingerprint the working tree after the run (fails CLOSED)"
[ "$git_dirty_before" = "$git_dirty_after" ] \
  || fail "this check modified the working tree (it must only write $BUILD,
  which is ignored) — before=$git_dirty_before after=$git_dirty_after"

echo "CREDITS CHECK OK"
