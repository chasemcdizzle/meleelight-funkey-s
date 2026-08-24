#!/usr/bin/env bash
# port/foh/check-css-mode.sh — HOST-ONLY tooth for punch-list A27, the
# owner-reported "there's no way to change between stock mode and 'endless ko
# fest'... if you click the 'VS Melee' in the CSS it should change modes".
#
# WHAT IT PROVES, and why it takes two very different kinds of evidence.
# A27 can fail in two independent ways, and a check that only sees one of
# them would pass a half-wired build:
#   (i)  the RIBBON could toggle a field and show nothing, or show a label
#        that does not follow the field — the owner would click and see no
#        answer;
#   (ii) the field could change and never reach the SIMULATION — the ribbon
#        would then be a very convincing stub (HARD RULE 2), which is exactly
#        why lane M refused to build it before A37 made versusMode real.
# So [3]-[5] drive the REAL foh_tick and photograph the REAL foh_render, and
# [6]-[7] launch a REAL match through foh_app's own bridge and read the mode
# back out of the SIMULATION's own checksum stream. Neither half is optional
# and neither substitutes for the other; the negative tests below are the
# A31 T2/T3 pair generalised — one build makes the label lie, one makes the
# plumbing lie, one makes the plumbing lie about its ORDER, and each must
# fail ALONE.
#
# MEASURED against the upstream clone before any code was written (HARD RULE
# 5, and the ticket turned on both of these):
#   * the widget's action is `setVersusMode(1 - versusMode)` on an A rising
#     edge inside `handPos.y > 100 && handPos.y < 160 && handPos.x > 380 &&
#     handPos.x < 910` (css.js:389-394) — a BINARY toggle, no picker, no
#     cycle. Our trigger is upstream's, verbatim; only the RECT is ours,
#     because upstream draws this blurb as loose text at (390,117) on a
#     1200x750 canvas and this FOH is a 240x240 rewrite (D28, MENU-SPEC).
#   * the labels are upstream's own words: `versusMode ? "An endless KO
#     fest!" : "4-man survival test!"` (css.js:715-721).
#
# THE COLD-FRAME ARGUMENT, measured rather than hoped (this is [4a]). Every
# frozen CSS shot in port/foh/flows/ is taken in STOCK mode, so if the STOCK
# ribbon still renders the bytes it rendered before A27 then A27 costs zero
# re-freezes. That is asserted here by cmp against a build whose label is
# unconditional — the only honest way to prove "unchanged" from inside the
# tree that changed it.
#
# HOST-ONLY. No OPK, no device. It DOES need the renderer's art and the whole
# sim data plane, which it builds itself from the pipeline.
#
# `bash port/foh/check-css-mode.sh` -> `CSS MODE CHECK OK`, exit 0.
#
# NOT A GATE. This is a task-level tooth.
set -euo pipefail

FOH=port/foh
GFX=port/gfx
SIM=port/sim/sim
CAL=port/sim/calib
BUILD=$FOH/build/css-mode
DATA=$BUILD/data

fail() { echo "CSS MODE CHECK FAIL: $1" >&2; exit 1; }
grammar_die() { echo "CSS MODE CHECK FAIL: $1" >&2; exit 2; }
relay_lines() { sed 's/^/  | /'; }

made() { # every artifact must exist and be non-empty
  local f
  for f in "$@"; do
    [ -s "$f" ] || fail "expected artifact missing/empty: $f"
  done
}

# one_canonical <file> <needle> <anchored-regex> <ctx> — the PROCESS §3
# permissive-parse guard, carried verbatim from check-css-back.sh.
one_canonical() { # <file> <needle> <regex> <ctx>
  local f="$1" needle="$2" re="$3" ctx="$4" nres nexact
  nres="$(grep -cF "$needle" "$f")" || true
  nexact="$(grep -cE "$re" "$f")" || true
  if [ "$nres" != 1 ] || [ "$nexact" != 1 ]; then
    sed 's/^/  | /' "$f" >&2
    grammar_die "$ctx: $nres line(s) resemble '$needle' and $nexact match it
  exactly (want 1 and 1)."
  fi
}

# perturb <src> <dst> <needle> <replacement> <tag> — single-occurrence
# asserted, no-op asserted (check-live-arms.sh's, condensed).
perturb() {
  local src="$1" dst="$2" needle="$3" repl="$4" tag="$5"
  mkdir -p "$(dirname "$dst")"
  node -e '
    const fs = require("fs");
    const [src, dst, needle, repl, tag] = process.argv.slice(1);
    const raw = fs.readFileSync(src, "utf8");
    const n = raw.split(needle).length - 1;
    if (n !== 1) {
      console.error(tag + ": " + n + " occurrences of the target line (want 1)");
      process.exit(1);
    }
    const out = raw.replace(needle, () => repl);
    if (out === raw) { console.error(tag + ": substitution was a no-op"); process.exit(1); }
    fs.writeFileSync(dst, out);
  ' "$src" "$dst" "$needle" "$repl" "$tag" \
    || fail "$tag: could not derive the perturbed copy of $src"
  cmp -s "$dst" "$src" && fail "$tag: the perturbed copy is byte-identical to $src"
  return 0
}

# --- [0] run lock (mkdir-atomic, NO reclaim) ---------------------------------
LOCK=$FOH/build/css-mode.lock
mkdir -p "$FOH/build"
mkdir "$LOCK" 2>/dev/null \
  || fail "another run holds $LOCK (remove it only if you are sure no run is live)"
trap 'rmdir "$LOCK" 2>/dev/null || true' EXIT

# --- [0a] no-commit guard (check-css-back.sh's, verbatim) --------------------
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
mkdir -p "$BUILD" "$DATA"
teeth=0

# --- [1] grammar pins --------------------------------------------------------
# Each perturbation below rewrites exactly ONE line. If a line is not unique
# the perturbation is ambiguous, and a tooth that might be biting something
# else is worse than no tooth.
echo "=== [1] grammar pins (the three lines the negative tests perturb)"
made "$FOH/foh.c" "$FOH/foh_render.c" "$FOH/foh_app.c" \
     "$FOH/foh_cssmode_witness.c"
LABEL_LINE='    if (versusMode) {'
n="$(grep -cxF "$LABEL_LINE" "$FOH/foh_render.c")" || true
[ "$n" = 1 ] || grammar_die "foh_render.c has $n ribbon-label branches (want
  exactly 1) — T1 cannot make the label lie unambiguously"
TOGGLE_LINE='        s->versusMode = 1 - s->versusMode;    // css.js:393'
n="$(grep -cxF "$TOGGLE_LINE" "$FOH/foh.c")" || true
[ "$n" = 1 ] || grammar_die "foh.c has $n ribbon toggle lines (want exactly 1)
  — T2 cannot cut the plumbing unambiguously"
BRIDGE_LINE='  G.sim.versusMode = foh.versusMode;'
n="$(grep -cxF "$BRIDGE_LINE" "$FOH/foh_app.c")" || true
[ "$n" = 1 ] || grammar_die "foh_app.c has $n versusMode bridge lines (want
  exactly 1) — T3 cannot move the write past sim_setup_match unambiguously"
SETUP_LINE='  sim_setup_match(&G, foh.p1Char, foh.p2Char, foh.p2Type, foh.difficulty,'
n="$(grep -cxF "$SETUP_LINE" "$FOH/foh_app.c")" || true
[ "$n" = 1 ] || grammar_die "foh_app.c has $n sim_setup_match call sites (want
  exactly 1) — T3's ordering perturbation would be ambiguous"
# The product binary and the rig binary must carry the SAME bridge, or the
# thing this check proves is true of foh_app only.
n="$(grep -cxF '    G.sim.versusMode = foh.versusMode;' "$FOH/foh_dev.c")" || true
[ "$n" = 1 ] || grammar_die "foh_dev.c carries $n versusMode bridge lines (want
  exactly 1) — the rig binary and the product binary have diverged"
echo "   the label branch, the toggle, and both bridge writes are textually unique"

# --- [2] data planes ---------------------------------------------------------
# The witness needs the renderer's art; the sim leg needs tables/stages/
# targets and a sim-data dump. Built into this check's OWN dir.
echo "=== [2] data planes (pipeline stages + sim-data dump, this check's dir)"
node pipeline/run.js --only animations,tables,stages,targets,assets \
  --out "$DATA" > "$BUILD/pipeline.log" 2>&1 \
  || { relay_lines < "$BUILD/pipeline.log"
       fail "the pipeline data stages did not run (host prerequisite:
  MELEELIGHT_CLONE, see CLAUDE.md)"; }
made "$DATA/assets/menu.img1" "$DATA/ml_tables.c" "$DATA/ml_stages.c" \
     "$DATA/ml_targets.c"
node "$CAL/dump-sim-data.js" --out "$BUILD/simdata.txt" \
  > "$BUILD/simdata.log" 2>&1 \
  || { relay_lines < "$BUILD/simdata.log"; fail "the sim-data dump failed"; }
made "$BUILD/simdata.txt"
# foh_render's artwork resolution is fatal on a MISSING file by design, so
# point it at THIS run's freshly generated artifact.
export MLFK_MENU_IMG1="$PWD/$DATA/assets/menu.img1"
echo "   data planes built under $DATA"

# --- [3] the witness against the REAL tree -----------------------------------
echo "=== [3] CSS mode witness (real gestures + real frames through foh_tick)"
CFLAGS_COMMON=(-ffp-contract=off -Wall -Wextra -Werror -I"$DATA" -Iport/ryu
               -Iport/sim -Ioracle/qjs)
WIT_SRCS=("$FOH/foh_cssmode_witness.c" "$FOH/foh_font.c" "$GFX/raster.c"
          "$GFX/img1.c" "$GFX/ctl_style.c" port/fdlibm/fdlibm.c)
WIT_OK='^CSS MODE OK$'
mkdir -p "$BUILD/real"
cc -O2 "${CFLAGS_COMMON[@]}" -o "$BUILD/real/cm" \
  "$FOH/foh.c" "$FOH/foh_render.c" "${WIT_SRCS[@]}" -lm \
  || fail "the witness did not build against the real tree"
MLFK_DATA_DIR="$DATA" "$BUILD/real/cm" "$BUILD/real" > "$BUILD/real/cm.out" 2>&1 \
  || { relay_lines < "$BUILD/real/cm.out"
       fail "the witness failed against the REAL tree"; }
relay_lines < "$BUILD/real/cm.out"
one_canonical "$BUILD/real/cm.out" 'CSS MODE OK' "$WIT_OK" "witness verdict"
made "$BUILD/real/cold.fb" "$BUILD/real/endless.fb"

# --- [4] T1: the label made unconditional ------------------------------------
# The copy always prints the STOCK label, i.e. the pre-A27 ribbon. It must
# (a) FAIL the witness on the LABEL assertion — not on the toggle, which it
# does not touch; (b) render the COLD frame BYTE-IDENTICAL to the real
# tree's, which is the no-re-freeze argument as a measurement; and (c)
# render the ENDLESS frame DIFFERENTLY, so (b) is a fact about the cold
# state and not about two builds that never differed.
echo "=== [4] T1: a ribbon whose label ignores the mode"
perturb "$FOH/foh_render.c" "$BUILD/t1/foh_render.c" "$LABEL_LINE" \
  '    if (versusMode && 0) { // T1: the label ignores the mode' 'T1'
cc -O2 "${CFLAGS_COMMON[@]}" -Iport/foh -o "$BUILD/t1/cm" \
  "$FOH/foh.c" "$BUILD/t1/foh_render.c" "${WIT_SRCS[@]}" -lm \
  || fail "T1: the label-less copy did not build"
t1rc=0
MLFK_DATA_DIR="$DATA" "$BUILD/t1/cm" "$BUILD/t1" > "$BUILD/t1/cm.out" 2>&1 \
  || t1rc=$?
[ "$t1rc" = 1 ] \
  || fail "T1: the copy exited rc $t1rc (want exactly 1 — rc 0 means the
  witness is BLIND to a ribbon that never changes its label)"
grep -qF 'the LABEL changes when the mode does' "$BUILD/t1/cm.out" \
  || { relay_lines < "$BUILD/t1/cm.out"
       fail "T1: the LABEL assertion did not fail — the tooth is failing for
  some other reason and proves nothing"; }
grep -qF 'CSS MODE FAIL: A on the ribbon arms the ENDLESS mode' \
  "$BUILD/t1/cm.out" \
  && { relay_lines < "$BUILD/t1/cm.out"
       fail "T1: the TOGGLE broke in the label-only copy, so the copy differs
  from foh_render.c by more than the label branch"; }
made "$BUILD/t1/cold.fb" "$BUILD/t1/endless.fb"
cmp -s "$BUILD/real/cold.fb" "$BUILD/t1/cold.fb" \
  || fail "T1(b): the COLD CSS frame CHANGED. Every frozen CSS shot is taken
  in STOCK mode, so this means A27 forces a re-freeze — which it must not.
  Re-read the ribbon's label branch: the versusMode==0 arm has to draw
  exactly what the unconditional ribbon drew."
cmp -s "$BUILD/real/endless.fb" "$BUILD/t1/endless.fb" \
  && fail "T1(c): the ENDLESS frame is identical too, so T1(b) proved nothing
  — the two builds are not actually rendering differently"
echo "   T1: label pinned to STOCK -> witness fails on the LABEL assertion;"
echo "       cold frames byte-identical (0 shots to re-freeze), endless differ"
teeth=$((teeth + 1))

# --- [5] T2: the toggle cut out ----------------------------------------------
echo "=== [5] T2: a ribbon that is hit-tested but never writes the mode"
perturb "$FOH/foh.c" "$BUILD/t2/foh.c" "$TOGGLE_LINE" \
  '        s->versusMode = s->versusMode; // T2: the toggle is a no-op' 'T2'
cc -O2 "${CFLAGS_COMMON[@]}" -Iport/foh -o "$BUILD/t2/cm" \
  "$BUILD/t2/foh.c" "$FOH/foh_render.c" "${WIT_SRCS[@]}" -lm \
  || fail "T2: the toggle-less copy did not build"
t2rc=0
MLFK_DATA_DIR="$DATA" "$BUILD/t2/cm" "$BUILD/t2" > "$BUILD/t2/cm.out" 2>&1 \
  || t2rc=$?
[ "$t2rc" = 1 ] \
  || fail "T2: the copy exited rc $t2rc (want exactly 1 — rc 0 means the
  witness cannot see a ribbon that does nothing, which IS the filed symptom)"
grep -qF 'CSS MODE FAIL: A on the ribbon arms the ENDLESS mode' \
  "$BUILD/t2/cm.out" \
  || { relay_lines < "$BUILD/t2/cm.out"
       fail "T2: the toggle assertion did not fail — the tooth is failing for
  some other reason"; }
echo "   T2: without the write, A on the ribbon does nothing and the witness says so"
teeth=$((teeth + 1))

# --- [6] the mode REACHES THE SIMULATION -------------------------------------
# Everything above is pixels and FOH state. This leg launches a real match
# through foh_app's own bridge and reads the answer out of the SIM's checksum
# stream, because the stocks the ribbon buys are IN that stream (upstream's
# main.js:1334, `if (versusMode) stocks = 1`, run inside startGame).
#
# Two runs of the SAME flow with the SAME seed, trace and characters. The
# only difference is four extra gesture rows that walk the hand onto the
# ribbon and press A. If frame 1 of the launched match is not moved by that
# press, the ribbon did not reach the sim.
echo "=== [6] the launched match: does the mode reach the simulation?"
g01line="$(node -e '
  const m = JSON.parse(require("fs").readFileSync("oracle/goldens/manifest.json", "utf8"));
  const g = m.goldens.find((x) => x.id === "g01");
  if (!g || g.cpu !== false) { console.error("g01 shape"); process.exit(1); }
  console.log([g.seed, g.trace].join(" "));
')" || fail "cannot parse g01 params from the oracle manifest"
read -r G01_SEED G01_TRACE <<< "$g01line"
node "$SIM/trace-to-txt.js" "oracle/goldens/$G01_TRACE" "$BUILD/g01.trace.txt" \
  > "$BUILD/trace.log" 2>&1 || { relay_lines < "$BUILD/trace.log"
                                 fail "trace-to-txt.js failed for g01"; }
made "$BUILD/g01.trace.txt"

# The CONTROL flow is the committed f01 minus its SHOT rows — nothing else,
# asserted by diff — so the control leg launches the same match the frozen
# f01 evidence launches.
FLOW=$FOH/flows/f01-vs-g01.flow
made "$FLOW" "$FOH/flows/f01-vs-g01.expect"
grep -v '^SHOT ' "$FLOW" > "$BUILD/ctrl.flow"
diff <(grep -v '^SHOT ' "$FLOW") "$BUILD/ctrl.flow" >/dev/null \
  || fail "the derived control flow is not exactly $FLOW minus its SHOT rows"
nshot="$(grep -c '^SHOT ' "$FLOW")" || true
[ "$nshot" -ge 1 ] || fail "$FLOW carries no SHOT rows — the strip is a no-op"

# The RIBBON flow is the control flow with its launch tail replaced by the
# ribbon gesture and then the same launch tail, later. Positions are
# clamp-anchored exactly as f01's are: UP x30 clamps y to 0 (the hand is at
# y = 101.76 and 30 * 3.84 = 115.2), DOWN x3 -> y = 11.52 (the plate spans
# 4..22, so a +/-1 frame slip lands 7.68 or 15.36 — inside either way), and
# RIGHT x20 -> x = 127.20 from x = 79.20 (the plate spans 104..178, i.e. 23
# px of slack on the left and 51 on the right). A there is upstream's
# setVersusMode click.
tail_from='I 765 S'
grep -qxF "$tail_from" "$BUILD/ctrl.flow" \
  || grammar_die "the control flow's launch tail ('$tail_from') is not where
  this check thinks it is — re-read $FLOW before trusting the ribbon flow"
sed "/^$tail_from\$/,\$d" "$BUILD/ctrl.flow" > "$BUILD/ribbon.flow"
cat >> "$BUILD/ribbon.flow" <<'RIBBON'
# A27: walk the hand onto the mode ribbon and click it (css.js:389-394).
I 765 U
I 795 -
I 800 D
I 803 -
I 805 R
I 825 -
I 830 A
I 833 -
# then the same launch tail f01 has, 75 frames later.
I 840 S
I 843 -
I 850 A
I 853 -
END 860
RIBBON
made "$BUILD/ribbon.flow"
cmp -s "$BUILD/ctrl.flow" "$BUILD/ribbon.flow" \
  && fail "the ribbon flow is byte-identical to the control flow"

# Build foh_app — the PRODUCT binary, with the whole sim behind it. The TU
# list is check-foh-flows.sh's, which is the list the app itself ships.
echo "   building foh_app (product binary + full sim)"
build_foh_app() { # <foh_app.c source> <out>
  # -Iport/foh so a perturbed COPY's quoted "foh.h" and "../gfx/..." resolve
  # to the real tree (check-css-back.sh's T1 copy-build, verbatim).
  cc -O2 "${CFLAGS_COMMON[@]}" -Iport/foh -o "$2" \
    "$BUILD/objs/raster.o" "$BUILD/objs/img1.o" \
    "$BUILD/objs/platform_headless.o" "$BUILD/objs/foh.o" \
    "$BUILD/objs/foh_font.o" "$BUILD/objs/foh_render.o" \
    "$BUILD/objs/foh_persist.o" "$BUILD/objs/ctl_style.o" \
    "$1" \
    "$SIM/sim_boot.c" "$SIM/sim_tick.c" "$SIM/sim_ser.c" \
    "$SIM/sim_data.c" "$SIM/sim_ai_live.c" \
    "$CAL/canon.c" "$CAL/player_canon.c" \
    port/sim/ai.c \
    port/sim/physics.c port/sim/interpolated_collision.c \
    port/sim/environmental_collision.c port/sim/hit_detection.c \
    port/sim/article.c port/sim/action_state_shortcuts.c \
    port/sim/ml_events.c port/sim/ml_fmt.c port/sim/ml_ser.c \
    port/sim/ai_bridge.c port/sim/input/interpret_inputs.c \
    port/sim/stages/moving_platforms.c port/sim/stages/ystory.c \
    port/sim/stages/fountain.c \
    port/sim/characters/shared/moves_index.c port/sim/characters/shared/moves/*.c \
    port/sim/characters/fox/moves_index.c port/sim/characters/fox/moves/*.c \
    port/sim/characters/falco/moves_index.c port/sim/characters/falco/moves/*.c \
    port/sim/characters/falcon/moves_index.c port/sim/characters/falcon/moves/*.c \
    port/sim/characters/marth/moves_index.c \
    port/sim/characters/marth/dancing_blade_combo.c \
    port/sim/characters/marth/dancing_blade_air_mobility.c \
    port/sim/characters/marth/moves/*.c \
    port/sim/characters/puff/moves_index.c \
    port/sim/characters/puff/puff_multi_jump_drift.c \
    port/sim/characters/puff/puff_next_jump.c \
    port/sim/characters/puff/moves/*.c \
    port/sim/target/target_play.c \
    "$DATA/ml_tables.c" "$DATA/ml_stages.c" "$DATA/ml_targets.c" \
    oracle/qjs/sha256.c port/fdlibm/fdlibm.c -lm
}
mkdir -p "$BUILD/objs"
cc -O3 "${CFLAGS_COMMON[@]}" -c "$GFX/raster.c" -o "$BUILD/objs/raster.o"
for tu in "$GFX/img1.c" "$GFX/platform_headless.c" "$GFX/ctl_style.c" \
          "$FOH/foh.c" "$FOH/foh_font.c" "$FOH/foh_render.c" \
          "$FOH/foh_persist.c"; do
  cc -O2 "${CFLAGS_COMMON[@]}" -c "$tu" -o "$BUILD/objs/$(basename "${tu%.c}").o"
done
build_foh_app "$FOH/foh_app.c" "$BUILD/foh_app" \
  || fail "foh_app did not build against the real tree"
made "$BUILD/foh_app"

# One leg: run <binary> <flow> <legId>, launch the g01 match and keep the
# first frames of its checksum stream. --frames 2 is deliberate: stocks are
# written by startGame, so frame 1 already carries the whole answer, and a
# 3600-frame replay would only make this slower.
LAUNCH_RE='^LAUNCH [0-9]+ p1=[0-4] p2=[0-4] p2type=[01] difficulty=[1-4] stage=[0-5] turbo=[01] lcancel=[012] flashlcancel=[01] walljump=[01] tapjump=[01],[01],[01],[01] versus=[01]$'
run_leg() { # <binary> <flow> <legId>
  local bin="$1" flow="$2" id="$3"
  rm -rf "$BUILD/$id" "$BUILD/$id-persist"
  mkdir -p "$BUILD/$id" "$BUILD/$id-persist"
  MLFK_PERSIST_DIR="$PWD/$BUILD/$id-persist" \
    "$bin" --flow "$flow" --flow-out "$BUILD/$id/trace.txt" \
      --bridge verify --simdata "$BUILD/simdata.txt" --seed "$G01_SEED" \
      --trace "$BUILD/g01.trace.txt" --frames 2 --out "$BUILD/$id/stream.txt" \
      --bstate-out "$BUILD/$id/bstate.txt" > "$BUILD/$id/out.txt" 2>&1 \
    || { relay_lines < "$BUILD/$id/out.txt"; fail "leg $id did not complete"; }
  made "$BUILD/$id/stream.txt" "$BUILD/$id/trace.txt"
  one_canonical "$BUILD/$id/trace.txt" 'LAUNCH ' "$LAUNCH_RE" \
    "leg $id LAUNCH record"
}
frame1() { # <legId> -> the frame-1 hash
  grep -E '^F 1 [0-9a-f]{64}$' "$BUILD/$1/stream.txt" | cut -d' ' -f3
}
launch_versus() { # <legId> -> the versus= field of its LAUNCH record
  grep -E "$LAUNCH_RE" "$BUILD/$1/trace.txt" | tr ' ' '\n' \
    | grep -E '^versus=' | cut -d= -f2
}

run_leg "$BUILD/foh_app" "$BUILD/ctrl.flow" ctrl
run_leg "$BUILD/foh_app" "$BUILD/ribbon.flow" ribbon
[ "$(launch_versus ctrl)" = 0 ] \
  || fail "the control leg launched versus=$(launch_versus ctrl) — it never
  touched the ribbon, so this run is not a control at all"
[ "$(launch_versus ribbon)" = 1 ] \
  || fail "the ribbon leg launched versus=$(launch_versus ribbon) — the
  gesture did not reach the widget (re-check the flow's positions against
  FOH_CSS_MODE_* before suspecting the code)"
# The control's LAUNCH record must still be the frozen one, field for field:
# A27 must not have moved what a normal launch launches.
ctrlLaunch="$(grep -E "$LAUNCH_RE" "$BUILD/ctrl/trace.txt")"
frozenLaunch="$(grep -E '^LAUNCH ' "$FOH/flows/f01-vs-g01.expect")"
[ "$ctrlLaunch" = "$frozenLaunch" ] \
  || fail "the control leg's LAUNCH record differs from the frozen f01
  witness:
  got    $ctrlLaunch
  frozen $frozenLaunch"
# ...and the ribbon leg's record must differ from the control's in the versus
# field AND NOWHERE ELSE. Without this the whole leg is soft: the ribbon
# gesture walks the hand UP through the roster band on its way to the header,
# and if that ever grabbed a token or moved a selection, frame 1 would differ
# for a reason that has nothing to do with the mode. Comparing the records
# field for field is what makes the hash difference below attributable.
ribLaunch="$(grep -E "$LAUNCH_RE" "$BUILD/ribbon/trace.txt")"
strip_versus() { # <LAUNCH line> -> its fields after the frame, minus versus=
  printf '%s\n' "$1" | cut -d' ' -f3- | sed 's/ versus=[01]$//'
}
[ "$(strip_versus "$ctrlLaunch")" = "$(strip_versus "$ribLaunch")" ] \
  || fail "the ribbon gesture changed something OTHER than the mode, so a
  difference in the launched match cannot be attributed to it:
  control $ctrlLaunch
  ribbon  $ribLaunch"
c1="$(frame1 ctrl)"; r1="$(frame1 ribbon)"
[ -n "$c1" ] && [ -n "$r1" ] || fail "a leg produced no frame-1 hash"
[ "$c1" != "$r1" ] \
  || fail "frame 1 of the launched match is IDENTICAL with and without the
  ribbon press ($c1). The mode never reached the simulation — the ribbon is
  a stub (HARD RULE 2)."
echo "   the control launch still matches the frozen f01 record (versus=0)"
echo "   the ribbon launch carries versus=1 and MOVES the sim's frame 1"
echo "     stock   $c1"
echo "     endless $r1"

# --- [7] T3: the bridge write moved AFTER sim_setup_match --------------------
# The ordering is the whole point of the bridge line's position: startGame
# READS versusMode (main.js:1334) to give every player 1 stock, so a write
# that lands after the setup produces a 4-stock "endless" match — the mode
# silently half-applied. A build with the line moved down must therefore
# stop moving frame 1, and this check must be the thing that notices.
echo "=== [7] T3: the bridge write moved past sim_setup_match"
perturb "$FOH/foh_app.c" "$BUILD/t3/foh_app.c" \
  "$BRIDGE_LINE"$'\n'"$SETUP_LINE"$'\n'"                  foh.stageSel);" \
  "$SETUP_LINE"$'\n'"                  foh.stageSel);"$'\n'"$BRIDGE_LINE // T3: too late" \
  'T3'
build_foh_app "$BUILD/t3/foh_app.c" "$BUILD/t3/foh_app" \
  || fail "T3: the reordered copy did not build"
run_leg "$BUILD/t3/foh_app" "$BUILD/ribbon.flow" t3ribbon
[ "$(launch_versus t3ribbon)" = 1 ] \
  || fail "T3: the reordered copy did not even record versus=1, so it is
  failing for a reason other than the ordering"
t3="$(frame1 t3ribbon)"
[ "$t3" = "$c1" ] \
  || fail "T3: moving the write past sim_setup_match still moved frame 1
  ($t3 vs stock $c1). Either startGame no longer reads versusMode or this
  check is measuring something else — do not 'fix' it by relaxing [6]."
echo "   T3: with the write one line too late the endless match is a 4-stock"
echo "       match — frame 1 collapses onto the stock stream, as it must"
teeth=$((teeth + 1))

# --- [8] no-commit guard -----------------------------------------------------
git_dirty_after="$(tree_fingerprint)" \
  || fail "could not fingerprint the working tree after the run (fails CLOSED)"
[ "$git_dirty_before" = "$git_dirty_after" ] \
  || fail "this check modified the working tree (it must only write $BUILD,
  which is ignored) — before=$git_dirty_before after=$git_dirty_after"

[ "$teeth" = 3 ] || fail "expected 3 negative tests, counted $teeth"
echo "CSS MODE CHECK OK ($teeth teeth)"
