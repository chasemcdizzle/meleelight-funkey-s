#!/usr/bin/env bash
# port/foh/check-controls-labels.sh — HOST-ONLY tooth for punch-list A24 (the
# Controls menu's wrong label, wrong name and wrong order) and for the
# owner-visible half of A4 (the control style named "NORMAL" reads "CLASSIC").
#
# WHAT THE TICKET WAS
# -------------------
# The owner, on playthrough #3: the Options row says "Keyboard Controls" but it
# opens a CHOOSER, so it should just say "Controls"; the chooser's "Keyboard"
# entry is not a keyboard at all, it is the FunKey-S's own buttons; and the
# unusable "Controller" entry sits FIRST. DEVIATION D25 (MENU-SPEC §12.1) makes
# the three renames: `CONTROLS`, `HANDHELD`, and HANDHELD first.
#
# WHY THE ORDER HALF IS NOT A STRING CHANGE. The chooser is INDEX-selected:
# foh.c's step_menu maps menuSelected 0/1 onto FOH_CTRL_KEY / FOH_CTRL_PAD.
# Swapping the two labels in foh_render.c WITHOUT swapping that ternary paints
# "HANDHELD" on a row wired to the controller screen — a defect that no
# assertion about strings can see, and the exact failure mode a lazy version of
# this check would ship. So the witness BINDS each row to its destination: it
# reads the row's label off the frame, presses A on that row through the real
# foh_tick, and reads the HEADER off the frame the machine lands on.
#
# WHAT THIS PROVES. It builds port/foh/foh_controls_witness.c against the REAL
# tree and runs it (the assertions are listed in that file's header). Then it
# re-runs the witness against FOUR perturbed copies — one per moving part — and
# requires each to fail, AND to fail only where that part is:
#   T1  the chooser labels swapped back      -> the two ROW assertions fail,
#                                               the destinations do NOT
#   T2  the routing ternary reverted alone   -> the DESTINATION assertions fail,
#                                               the row labels do NOT   <-- the
#                                               half-swap trap, in one test
#   T3  the Options row back to the old name -> only that row fails
#   T4  ctl_style_name back to "Normal"      -> only the style row fails
# Without T2 this check would pass a build whose first row lies about where it
# goes, which is the whole of the owner's third complaint.
#
# HOST-ONLY. Nothing here needs the device, an OPK or the sim tables; the one
# host prerequisite is node, for the pipeline's `assets` stage (the renderer
# hard-fails without menu.img1), which this check builds into its OWN dir.
# `bash port/foh/check-controls-labels.sh` -> `CONTROLS LABELS OK`, exit 0.
#
# NOT A GATE. This is a task-level tooth.
set -euo pipefail

FOH=port/foh
GFX=port/gfx
BUILD=$FOH/build/controls
DATA=$BUILD/data

fail() { echo "CONTROLS LABELS FAIL: $1" >&2; exit 1; }
grammar_die() { echo "CONTROLS LABELS FAIL: $1" >&2; exit 2; }
relay_lines() { sed 's/^/  | /'; }

made() { # every artifact must exist and be non-empty
  local f
  for f in "$@"; do
    [ -s "$f" ] || fail "expected artifact missing/empty: $f"
  done
}

# --- [0] run lock (mkdir-atomic, NO reclaim — the check-legibility.sh pattern)
LOCK=$FOH/build/controls.lock
mkdir -p "$FOH/build"
mkdir "$LOCK" 2>/dev/null \
  || fail "another run holds $LOCK (remove it only if you are sure no run is live)"
trap 'rmdir "$LOCK" 2>/dev/null || true' EXIT

# --- [0a] no-commit guard ----------------------------------------------------
# A check must never write a tracked file. Carried verbatim from
# check-legibility.sh, including its two corrections: the `-f` test (a wholly
# untracked nested checkout is reported as a bare DIRECTORY) and the name list
# (so a directory appearing or disappearing stays visible even though its bytes
# are not walked).
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

# --- [1] the grammar pins ----------------------------------------------------
# The witness HAND COPIES the menu-bar geometry, three palette entries, two
# text origins and the whole blurb table out of the renderer, because all of
# them are file-static there and a witness that guessed any of them would
# assert against a screen that no longer exists. Every copied line is pinned to
# EXACTLY ONE occurrence (the check-legibility.sh discipline), except the one
# noted below that upstream's two-pass bar draw writes twice.
echo "=== [1] grammar pins (what the witness hand-copies out of the tree)"
made "$FOH/foh_render.c" "$FOH/foh.c" "$GFX/ctl_style.c" \
     "$FOH/foh_controls_witness.c"
pin_n() { # <file> <count> <line> <what it is>
  local f="$1" k="$2" line="$3" what="$4" n
  n="$(grep -cxF "$line" "$f")" || true
  [ "$n" = "$k" ] || grammar_die "$f has $n copies of
  |$line|
  (want exactly $k) — $what. Re-read it and re-sync
  port/foh/foh_controls_witness.c."
}
pin() { pin_n "$FOH/foh_render.c" 1 "$1" "$2"; }

# the palette the witness overdraws with
pin 'static const RastCol kAccent = {255, 200, 60, 256};' \
    'the header/accent colour the witness overdraws headers and the style row in'
pin '    const RastCol sel = {254, 238, 27, 256};      // rgb(254,238,27)' \
    'the colour an UNSELECTED menu-bar label is drawn in'
pin '    const RastCol selTx = {0, 0, 0, 256};' \
    'the colour the SELECTED menu-bar label is drawn in'
# the menu-bar geometry (both label passes land on it)
pin '#define FOH_BAR_STEP 11' 'the bars` diagonal cascade per row'
pin '#define FOH_BAR_TOP 58' 'the first bar`s top'
pin '#define FOH_BAR_PITCH 31' 'the bar-to-bar pitch'
pin_n "$FOH/foh_render.c" 2 \
    '      foh_text2(rz, (int)(123.0f - (float)(FOH_BAR_STEP * k)) - tw / 2,' \
    'the label x expression — TWO copies by design (menu.js` unselected pass and
  its selected overlay); the witness centres on the same 123 - 11*k'
pin '                (int)bar_top(k) + 6, 1, 1, label, sel);' \
    'the UNSELECTED label`s y, face, italic flag and colour'
pin '                (int)bar_top(k) + 6, 1, 1, label, selTx);' \
    'the SELECTED label`s y, face, italic flag and colour'
# the screen header (both destinations draw through it)
pin '  text_center(rz, 6, 2, title, kAccent);' \
    'header()`s y, scale and colour — how the witness reads a destination'
# the D25 labels themselves
pin '    {"AUDIO", "GAMEPLAY", "CONTROLS", "CREDITS"},' \
    'the Options page rows — row 2 is the renamed one'
pin '    {"HANDHELD", "CONTROLLER"},' \
    'the chooser rows, in their D25 order (HANDHELD first)'
pin '  header(rz, "HANDHELD");' 'the row-0 destination`s header'
pin '  header(rz, "CONTROLLER");' 'the row-1 destination`s header'
# the control-style row (A4)
pin '    const int yRow[2] = {176, 190};' 'the two settable rows` y table'
pin '    foh_text(rz, 16, yRow[0], 1, buf,' \
    'the STYLE row`s x, y, face and buffer'
# the explanation bar — the witness re-measures the width claim, so its
# geometry has to be the geometry that claim serves
pin '    fill_rect(rz, 4, 196, 232, 20, back);' \
    'the explanation bar rect the 230 px blurb pin sizes'
# every blurb the witness restates in its widest-of-set measurement
pin '    {"MULTIPLAYER BATTLES!", "SMASH TEN TARGETS!", "BUILD TARGET TEST STAGES!",' 'page 0 blurbs'
pin '     "GAME SETUP."},' 'page 0 blurbs (cont)'
pin '    {"SELECT AUDIO LEVELS.", "CHANGE GAMEPLAY SETTINGS.",' 'page 1 blurbs'
pin '     "CUSTOMIZE & CALIBRATE CONTROLS.", "WHO DID THIS?"},' 'page 1 blurbs (cont)'
pin '    {"ONE BOX THIS SCREEN.", "RANKED MODE", "HOSTLESS MULIPLAYER",' 'page 2 blurbs'
pin '     "HOSTED MULTIPLAYER"},' 'page 2 blurbs (cont)'
pin '    {"CUSTOMIZE THE HANDHELD CONTROLS.", "CUSTOMIZE & CALIBRATE CONTROLLER.",' 'page 3 blurbs (D25)'
# the OTHER two moving parts, in their own files
pin_n "$FOH/foh.c" 1 \
    '        ev_trans(s, sc, s->menuSelected == 0 ? FOH_CTRL_KEY : FOH_CTRL_PAD,' \
    'the D25 routing ternary — the half of the rename that is NOT paint'
pin_n "$GFX/ctl_style.c" 1 \
    '  case CTL_STYLE_NORMAL: return "Classic";' \
    'the A4 display name (C31) — the ENUM VALUE stays 0, only the string moved'
echo "   26 hand-copied lines still read exactly as the witness assumes"

# --- [2] renderer art (pipeline 'assets' stage, this check's own dir) --------
# foh_render hard-fails without menu.img1 (measured: the menu backdrop cache
# opens it before any bar is drawn). Built into this check's OWN dir; nothing
# outside $BUILD is written.
echo "=== [2] renderer art (pipeline 'assets' stage, this check's own dir)"
node pipeline/run.js --only assets --out "$DATA" > "$BUILD/assets.log" 2>&1 \
  || { relay_lines < "$BUILD/assets.log"
       fail "the pipeline's assets stage did not run (host prerequisite: node)"; }
made "$DATA/assets/menu.img1"
echo "   $DATA/assets/menu.img1 built"

# --- [3] the witness against the REAL tree ----------------------------------
# ctl_style.c is REQUIRED (foh.c's chooser destinations and the witness's own
# A4 leg both call into it); fdlibm supplies the renderer's sin/cos.
CFLAGS_COMMON=(-ffp-contract=off -Wall -Wextra -Werror -Iport/ryu -Iport/sim
               -Ioracle/qjs)
# The perturbed copies live outside their own directory, so their quoted
# includes ("foh.h", "../gfx/ctl_style.h", "ctl_style.h") need both roots.
CFLAGS_COPY=(-Iport/foh -Iport/gfx)
REAL_SRCS=("$FOH/foh_controls_witness.c" "$FOH/foh_render.c" "$FOH/foh.c"
           "$FOH/foh_font.c" "$GFX/raster.c" "$GFX/img1.c" "$GFX/ctl_style.c"
           port/fdlibm/fdlibm.c)
CTL_OK='^CONTROLS OK$'

echo "=== [3] controls witness (real gestures through real foh_tick)"
mkdir -p "$BUILD/real"
cc -O2 "${CFLAGS_COMMON[@]}" -o "$BUILD/real/ctl" "${REAL_SRCS[@]}" -lm \
  || fail "the witness did not build against the real tree"
made "$BUILD/real/ctl"
MLFK_DATA_DIR="$DATA" "$BUILD/real/ctl" > "$BUILD/real/ctl.out" 2>&1 \
  || { relay_lines < "$BUILD/real/ctl.out"
       fail "the witness failed against the REAL tree"; }
relay_lines < "$BUILD/real/ctl.out"
nres="$(grep -cF 'CONTROLS OK' "$BUILD/real/ctl.out")" || true
nexact="$(grep -cE "$CTL_OK" "$BUILD/real/ctl.out")" || true
[ "$nres" = 1 ] && [ "$nexact" = 1 ] \
  || grammar_die "witness verdict: $nres line(s) resemble 'CONTROLS OK' and
  $nexact match it exactly (want 1 and 1)"

# perturb_build <dir> <source-file> <pairs...> — derive a COPY of ONE tree
# source with exact-string replacements, refusing any that is a no-op or that
# is not unique, then build the witness against the copy IN PLACE OF the
# original and require rc 1. Generic in the file because the three moving parts
# of this ticket live in three different TUs (renderer, machine, style table).
perturb_build() {
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
  # swap the copy in for the original, everything else untouched
  local srcs=() f
  for f in "${REAL_SRCS[@]}"; do
    if [ "$f" = "$src" ]; then srcs+=("$BUILD/$dir/$base"); else srcs+=("$f"); fi
  done
  cc -O2 "${CFLAGS_COMMON[@]}" "${CFLAGS_COPY[@]}" -o "$BUILD/$dir/ctl" \
    "${srcs[@]}" -lm \
    || fail "$dir: the perturbed copy did not build"
  local rc=0
  MLFK_DATA_DIR="$DATA" "$BUILD/$dir/ctl" > "$BUILD/$dir/ctl.out" 2>&1 || rc=$?
  [ "$rc" = 1 ] \
    || { relay_lines < "$BUILD/$dir/ctl.out"
         fail "$dir: the perturbed build exited rc $rc (want exactly 1 — rc 0
  means the witness is BLIND to the defect it exists to guard)"; }
  grep -qE "$CTL_OK" "$BUILD/$dir/ctl.out" \
    && fail "$dir: the perturbed build still printed the OK verdict"
  return 0
}

# must_fail / must_pass <dir> <grep -E pattern> <why>
must_fail() {
  grep -qE "$2" "$BUILD/$1/ctl.out" \
    || { relay_lines < "$BUILD/$1/ctl.out"
         fail "$1: no failure matched /$2/ — $3"; }
}
must_pass() {
  grep -qE "$2" "$BUILD/$1/ctl.out" \
    && { relay_lines < "$BUILD/$1/ctl.out"
         fail "$1: a failure matched /$2/ — $3"; }
  return 0
}

# --- [4] T1: the chooser labels put back in upstream's order ----------------
# Labels alone. The ROUTING is untouched, so row 0 still GOES to the handheld
# screen — which is exactly why the row assertions must fail here and the
# destination assertions must not: this is the half-swap seen from the other
# side.
echo "=== [4] T1: the pre-A24 chooser labels must fail the witness"
perturb_build t1-oldlabels "$FOH/foh_render.c" \
  '    {"HANDHELD", "CONTROLLER"},' \
  '    {"CONTROLLER", "HANDHELD"}, // T1: pre-A24 order' \
  '  header(rz, "HANDHELD");' '  header(rz, "KEYBOARD"); // T1'
must_fail t1-oldlabels '^CONTROLS FAIL: controls chooser: row 0 on screen reads "HANDHELD"' \
  "the ROW-0 label assertion did not fail, so the tooth is blind to the rename"
must_fail t1-oldlabels '^CONTROLS FAIL: controls chooser: row 1 on screen reads "CONTROLLER"' \
  "the ROW-1 label assertion did not fail"
must_fail t1-oldlabels '^CONTROLS FAIL: row 0.s destination: the screen HEADER reads "HANDHELD"' \
  "the destination HEADER assertion did not fail — the old 'KEYBOARD' header
  would still be on screen unnoticed"
must_pass t1-oldlabels '^CONTROLS FAIL: options page' \
  "this perturbation only touches the chooser and the header, so the copy
  differs from foh_render.c by more than the two T1 lines"
must_pass t1-oldlabels '^CONTROLS FAIL: the style row' \
  "same: the style row is not part of this perturbation"
echo "   T1: the old labels and the old header reproduce the ticket and fail (rc 1)"

# --- [5] T2: the routing ternary reverted, labels left alone ----------------
# THE TRAP. The screen still PAINTS "HANDHELD" first; pressing A on it now
# opens the controller screen. Every row-label assertion still passes, so a
# check that only read strings would go green on the defect the owner filed.
echo "=== [5] T2: the half-swap (labels moved, routing not) must fail"
perturb_build t2-oldrouting "$FOH/foh.c" \
  '        ev_trans(s, sc, s->menuSelected == 0 ? FOH_CTRL_KEY : FOH_CTRL_PAD,' \
  '        ev_trans(s, sc, s->menuSelected == 0 ? FOH_CTRL_PAD : FOH_CTRL_KEY, // T2'
must_fail t2-oldrouting '^CONTROLS FAIL: A on row 0 lands on FOH_CTRL_KEY' \
  "the destination assertion did not fail, so row 0 could be wired anywhere"
must_fail t2-oldrouting '^CONTROLS FAIL: row 0.s destination: the screen HEADER reads "HANDHELD"' \
  "the HEADER read at the destination did not fail — the LABEL/ROUTING BINDING
  is not actually bound, which is the entire point of this witness"
must_pass t2-oldrouting '^CONTROLS FAIL: controls chooser' \
  "a ROW-LABEL assertion failed too. It must NOT: this perturbation moves only
  the ternary, and the labels are still in their D25 order — if the labels fail
  here the copy differs from foh.c by more than that one line"
must_pass t2-oldrouting '^CONTROLS FAIL: options page' \
  "the Options row is not part of this perturbation"
echo "   T2: labels intact, routing reverted -> the destinations fail (rc 1)"

# --- [6] T3: the Options row back to "KEYBOARD CONTROLS" -------------------
echo "=== [6] T3: the pre-A24 Options row must fail the witness"
perturb_build t3-oldoptionsrow "$FOH/foh_render.c" \
  '    {"AUDIO", "GAMEPLAY", "CONTROLS", "CREDITS"},' \
  '    {"AUDIO", "GAMEPLAY", "KEYBOARD CONTROLS", "CREDITS"}, // T3'
must_fail t3-oldoptionsrow '^CONTROLS FAIL: options page: row 2 on screen reads "CONTROLS"' \
  "the Options-row assertion did not fail"
must_pass t3-oldoptionsrow '^CONTROLS FAIL: (controls chooser|row [01].s destination|the style row)' \
  "this perturbation only touches the Options page's label row, so the copy
  differs from foh_render.c by more than that one line"
echo "   T3: the old Options row name fails (rc 1)"

# --- [7] T4: the control style back to the ambiguous "Normal" --------------
echo "=== [7] T4: ctl_style_name back to \"Normal\" must fail the witness"
perturb_build t4-oldstylename "$GFX/ctl_style.c" \
  '  case CTL_STYLE_NORMAL: return "Classic";' \
  '  case CTL_STYLE_NORMAL: return "Normal"; // T4'
must_fail t4-oldstylename '^CONTROLS FAIL: the style row on screen reads "STYLE: CLASSIC"' \
  "the style-name assertion did not fail, so A4's owner-visible half has no tooth"
must_pass t4-oldstylename '^CONTROLS FAIL: (controls chooser|options page|row [01].s destination)' \
  "this perturbation only touches one returned string, so the copy differs from
  ctl_style.c by more than that one line"
echo "   T4: the ambiguous \"Normal\" name fails (rc 1)"

# --- [8] no-commit guard -----------------------------------------------------
git_dirty_after="$(tree_fingerprint)" \
  || fail "could not fingerprint the working tree after the run (fails CLOSED)"
[ "$git_dirty_before" = "$git_dirty_after" ] \
  || fail "this check modified the working tree (it must only write $BUILD,
  which is ignored) — before=$git_dirty_before after=$git_dirty_after"

echo "CONTROLS LABELS OK"
