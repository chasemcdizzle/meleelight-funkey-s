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
# AND SINCE D27, TWO CONFIGURATIONS. The owner's second A24 ruling
# (2026-08-23) collapsed the chooser: "collapse now - make easily revertable
# though please." The revert is foh.h's `#define FOH_CTL_CHOOSER` — ONE digit —
# and a flag whose OFF path nobody runs is revertible in name only. So this
# check builds the witness at BOTH values:
#   CFG-0  the SHIPPED build (the tree's own value, pinned 0 below): the
#          Options CONTROLS row opens the HANDHELD screen DIRECTLY and its B
#          returns to Options on the CONTROLS row. The chooser page is
#          unreachable and still WHOLE, so the witness seats it directly and
#          asserts every D25 claim about it anyway.
#   CFG-1  the RESTORED build, derived by rewriting that one digit in a COPY
#          of foh.h: the chooser sits back in the path and B walks home
#          through it. Same witness, same binary assertions, both routes.
#
# WHAT THIS PROVES. It builds port/foh/foh_controls_witness.c against the REAL
# tree and runs it (the assertions are listed in that file's header). Then it
# re-runs the witness against FIVE perturbed copies — one per moving part — and
# requires each to fail, AND to fail only where that part is:
#   T1  the chooser labels swapped back      -> the two ROW assertions fail,
#                                               the destinations do NOT
#   T2  the routing ternary reverted alone   -> the DESTINATION assertions fail,
#                                               the row labels do NOT   <-- the
#                                               half-swap trap, in one test
#   T3  the Options row back to the old name -> only that row fails
#   T4  ctl_style_name back to "Normal"      -> only the style row fails
#   T5  the D27 dispatch pointed back at the chooser -> only the COLLAPSED
#                                               route fails; the chooser page's
#                                               own assertions do NOT   <-- T2's
#                                               twin, for the new arm
# Without T2 this check would pass a build whose first row lies about where it
# goes, which is the whole of the owner's third complaint; without T5 it would
# pass a build that quietly put the dead chooser back in the owner's way.
# T2 SURVIVES THE COLLAPSE UNCHANGED, and that is not luck: the chooser's A
# ternary is compiled at either flag value, so seating the page directly
# exercises the same line the flag-1 build navigates to.
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
# A31 rewrote this screen: the two settable rows became eleven (nine action
# rows + style + reset), so the old `yRow[2]` table is gone. What the witness
# hand-copies is unchanged in SUBSTANCE — the STYLE row is still x=16, y=176 —
# so the pin follows the line that now carries those coordinates. NOT a
# weakening: it is still one exact full-line match on the coordinate source,
# and the witness still overdraws the style row at those exact coordinates.
pin '  const int yStyle = 176, yReset = 190;' 'the two settable rows` y table'
pin '    foh_text(rz, 16, yStyle, 1, buf,' \
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
# D27's switch and its collapsed dispatch. The flag is pinned to the SHIPPED
# value because leg [3] runs the tree AS COMMITTED and calls that CFG-0: if the
# digit flips, the configurations below silently swap identity. The dispatch line
# is what T5 perturbs.
pin_n "$FOH/foh.h" 1 '#define FOH_CTL_CHOOSER 0' \
    'DEVIATION D27`s switch, at its shipped value — leg [3] judges the tree as
  committed, and leg [8] derives the OTHER value from a copy of this line'
pin_n "$FOH/foh.c" 1 \
    '          ev_trans(s, sc, FOH_CTRL_KEY, "a"); // changeGamemode(12), :159-161' \
    'the D27 collapsed dispatch — the whole of what the flag`s OFF arm does,
  and what T5 reverts'
echo "   28 hand-copied lines still read exactly as the witness assumes"

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

# derive <dir> <source-file> <pairs...> — write a COPY of ONE tree source into
# $BUILD/<dir>/, with exact-string replacements, refusing any that is a no-op or
# that is not unique. Generic in the file because the moving parts of this
# ticket live in four different places (renderer, machine, style table, and
# since D27 the header that carries the switch).
derive() {
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
  return 0
}

# run_variant <dir> <want-rc> — run a built variant and require EXACTLY that
# exit code. rc 0 must also carry exactly one verdict line; rc 1 must carry
# none. rc 0 where 1 was wanted means the witness is BLIND to the defect it
# exists to guard.
run_variant() {
  local dir="$1" want="$2" rc=0 n
  MLFK_DATA_DIR="$DATA" "$BUILD/$dir/ctl" > "$BUILD/$dir/ctl.out" 2>&1 || rc=$?
  [ "$rc" = "$want" ] \
    || { relay_lines < "$BUILD/$dir/ctl.out"
         fail "$dir: the variant exited rc $rc (want exactly $want)"; }
  if [ "$want" = 0 ]; then
    n="$(grep -cE "$CTL_OK" "$BUILD/$dir/ctl.out")" || true
    [ "$n" = 1 ] || grammar_die "$dir: $n line(s) match the OK verdict exactly (want 1)"
  else
    grep -qE "$CTL_OK" "$BUILD/$dir/ctl.out" \
      && fail "$dir: the failing variant still printed the OK verdict"
  fi
  return 0
}

# perturb_build <dir> <source-file> <pairs...> — the common case: ONE derived
# source, swapped in, required to FAIL.
perturb_build() {
  local dir="$1" src="$2"; shift 2
  derive "$dir" "$src" "$@"
  local srcs=() f
  for f in "${REAL_SRCS[@]}"; do
    if [ "$f" = "$src" ]; then srcs+=("$BUILD/$dir/$(basename "$src")")
    else srcs+=("$f"); fi
  done
  cc -O2 "${CFLAGS_COMMON[@]}" "${CFLAGS_COPY[@]}" -o "$BUILD/$dir/ctl" \
    "${srcs[@]}" -lm \
    || fail "$dir: the perturbed copy did not build"
  made "$BUILD/$dir/ctl"
  run_variant "$dir" 1
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

# --- [7b] T5: the D27 dispatch pointed back at the chooser ------------------
# T2's twin, for the arm D27 added. The labels, the chooser page and the style
# row are all untouched, so ONLY the collapsed route may fail — and it must,
# or a build that quietly put the dead chooser back in front of the owner
# would pass this check.
echo "=== [7b] T5: the collapsed CONTROLS row pointed back at the chooser must fail"
# The reverted arm is spelled the way an actual revert would spell it — the
# chooser destination AND upstream's cursor reset together. Pointing the
# dispatch at the chooser WITHOUT that reset lands the renderer on a menu-mode
# row that does not exist and dies rc 3 (measured), which is a different tooth
# than "the route moved" and would let this leg pass on the wrong failure.
perturb_build t5-uncollapsed "$FOH/foh.c" \
  '          ev_trans(s, sc, FOH_CTRL_KEY, "a"); // changeGamemode(12), :159-161' \
  '          s->menuSelected = 0; ev_trans(s, sc, FOH_MENU_CONTROLS, "a"); // T5'
must_fail t5-uncollapsed '^CONTROLS FAIL: D27: the CONTROLS row opens the HANDHELD screen DIRECTLY' \
  "the COLLAPSED-ROUTE assertion did not fail, so the collapse the owner asked
  for has no tooth at all"
# The style leg is deliberately NOT in this exclusion, and the reason is a
# fact about the tree rather than a hedge: a4_style_names reaches the HANDHELD
# screen THROUGH the collapsed route, so reverting that route legitimately
# takes it with it. What must stay green is everything the perturbation cannot
# reach — the Options row's own label, and the chooser page, which the witness
# seats directly and therefore never routes to.
must_pass t5-uncollapsed '^CONTROLS FAIL: (controls chooser|options page|row [01].s destination)' \
  "this perturbation only touches the collapsed dispatch, so the copy differs
  from foh.c by more than that one line"
echo "   T5: the un-collapsed dispatch fails (rc 1)"

# --- [8] CFG-1: the SAME witness against the RESTORED chooser ---------------
# THE REVERT, EXERCISED. The owner asked for a collapse that is easily
# revertible; the revert is one digit in foh.h, and this leg proves that digit
# is all of it.
#
# HOW THE HEADER IS SWAPPED, and why it is not an -I trick: every TU here says
# `#include "foh.h"`, and a QUOTED include is resolved from the INCLUDING
# FILE'S OWN DIRECTORY before any -I path, so putting a modified copy on the
# include path would silently have no effect and this leg would then "pass"
# against the shipped configuration — a false green, and the exact shape of
# false green this file exists to refuse. So the whole port/foh half of the
# build is COPIED beside the derived header, and the copies pick it up as
# their own directory's foh.h. Their `../gfx/...` includes still resolve,
# through the same $CFLAGS_COPY the perturbed legs above already rely on.
# Every copy is asserted byte-identical to its original, so the ONLY
# difference between this build and leg [3]'s is the one digit.
echo "=== [8] CFG-1: FOH_CTL_CHOOSER 1 restores the chooser route"
mkdir -p "$BUILD/cfg1"
derive cfg1 "$FOH/foh.h" \
  '#define FOH_CTL_CHOOSER 0' '#define FOH_CTL_CHOOSER 1'
CFG1_SRCS=()
for f in "${REAL_SRCS[@]}"; do
  case "$f" in
    "$FOH"/*)
      cp "$f" "$BUILD/cfg1/$(basename "$f")" || fail "cfg1: could not copy $f"
      cmp -s "$f" "$BUILD/cfg1/$(basename "$f")" \
        || fail "cfg1: the copy of $f is not byte-identical to it"
      CFG1_SRCS+=("$BUILD/cfg1/$(basename "$f")") ;;
    *) CFG1_SRCS+=("$f") ;;
  esac
done
cmp -s "$FOH/foh.h" "$BUILD/cfg1/foh.h" \
  && fail "cfg1: the derived header is byte-identical to the tree's (dead leg)"
cc -O2 "${CFLAGS_COMMON[@]}" "${CFLAGS_COPY[@]}" -o "$BUILD/cfg1/ctl" \
  "${CFG1_SRCS[@]}" -lm \
  || fail "cfg1: the witness did not build against the restored-chooser header"
made "$BUILD/cfg1/ctl"
run_variant cfg1 0
relay_lines < "$BUILD/cfg1/ctl.out"
# The verdict alone does NOT prove which configuration ran, so the two routes
# are read off the output by name: the flag-1 line must be there and the
# flag-0 line must not.
grep -qF "FOH_CTL_CHOOSER 1: the CONTROLS row opens the CHOOSER" "$BUILD/cfg1/ctl.out" \
  || grammar_die "cfg1 passed but never printed the flag-1 route assertion — the
  derived header did not reach the compiler, so this leg proved nothing"
grep -qF "D27: the CONTROLS row opens the HANDHELD screen DIRECTLY" "$BUILD/cfg1/ctl.out" \
  && grammar_die "cfg1 printed the COLLAPSED route assertion, so the build took
  the flag-0 arms while claiming to be the restored configuration"
echo "   CFG-1: the restored chooser route passes the same witness (rc 0)"

# --- [8b] T2 UNDER CFG-1: the half-swap trap, in the configuration that has -
# ---      a chooser to half-swap -------------------------------------------
# T2 already bites at CFG-0 (leg [5]), because the chooser's A ternary is
# compiled at either flag value and the witness seats the page directly there.
# It must bite HERE too, where that ternary is also NAVIGATED to: this is the
# configuration a future restorer would ship, and the half-swap is exactly the
# defect the owner filed in the first place.
echo "=== [8b] T2 under CFG-1: the half-swap must fail there too"
mkdir -p "$BUILD/cfg1-t2"
cp "$BUILD/cfg1/foh.h" "$BUILD/cfg1-t2/foh.h" || fail "cfg1-t2: header copy failed"
derive cfg1-t2 "$FOH/foh.c" \
  '        ev_trans(s, sc, s->menuSelected == 0 ? FOH_CTRL_KEY : FOH_CTRL_PAD,' \
  '        ev_trans(s, sc, s->menuSelected == 0 ? FOH_CTRL_PAD : FOH_CTRL_KEY, // T2'
CFG1T2_SRCS=()
for f in "${REAL_SRCS[@]}"; do
  case "$f" in
    "$FOH/foh.c") CFG1T2_SRCS+=("$BUILD/cfg1-t2/foh.c") ;;
    "$FOH"/*)
      cp "$f" "$BUILD/cfg1-t2/$(basename "$f")" || fail "cfg1-t2: could not copy $f"
      CFG1T2_SRCS+=("$BUILD/cfg1-t2/$(basename "$f")") ;;
    *) CFG1T2_SRCS+=("$f") ;;
  esac
done
cc -O2 "${CFLAGS_COMMON[@]}" "${CFLAGS_COPY[@]}" -o "$BUILD/cfg1-t2/ctl" \
  "${CFG1T2_SRCS[@]}" -lm \
  || fail "cfg1-t2: the variant did not build"
made "$BUILD/cfg1-t2/ctl"
run_variant cfg1-t2 1
must_fail cfg1-t2 '^CONTROLS FAIL: A on row 0 lands on FOH_CTRL_KEY' \
  "the destination assertion did not fail under CFG-1, so the restored route
  could be wired anywhere"
must_fail cfg1-t2 '^CONTROLS FAIL: row 0.s destination: the screen HEADER reads "HANDHELD"' \
  "the HEADER read at the destination did not fail under CFG-1"
must_pass cfg1-t2 '^CONTROLS FAIL: controls chooser' \
  "a ROW-LABEL assertion failed too. It must NOT: this perturbation moves only
  the ternary and the labels are still in their D25 order"
echo "   T2 under CFG-1: labels intact, routing reverted -> the destinations fail (rc 1)"

# --- [8] no-commit guard -----------------------------------------------------
git_dirty_after="$(tree_fingerprint)" \
  || fail "could not fingerprint the working tree after the run (fails CLOSED)"
[ "$git_dirty_before" = "$git_dirty_after" ] \
  || fail "this check modified the working tree (it must only write $BUILD,
  which is ignored) — before=$git_dirty_before after=$git_dirty_after"

echo "CONTROLS LABELS OK"
