#!/usr/bin/env bash
# port/gfx/check-ctl-input.sh — the L-SHOULDER check (fix_plan A25b + A3
# + A30(a) / DEVIATION D30). HOST ONLY: it needs no FunKey, which is the
# point — the defect it locks down is one the device rig is structurally
# blind to (every device check injects through our OWN uinput node, while
# the physical buttons come from fkgpiod, which the rig's quiesce bracket
# stops for the duration of a run).
#
# Leg [1] builds port/gfx/ctl_input_witness.c against the REAL tree and
# runs its assertions (the device translation arm, the KEYMAP1 SSOT,
# L-only shielding, the R C-layer, the D33 face plane INCLUDING grab on
# BOX, L-char-steps-in-target-select, the Mod-shoulder default).
# Leg [2] rebuilds the witness against PERTURBED COPIES of the tree — one
# file changed per copy — and requires each to FAIL. The teeth are
# orthogonal by construction: each perturbs a different file and each
# fails a different assertion, so no single leg can be carrying the whole
# check.
#
# Leg [4] CROSSES THE SEAM (fix_plan A42; /CONTEXT.md "Seam"), and it is
# the reason this file was reopened. Legs [1]-[3] assert what the resolver
# EMITS. On 2026-08-24 that was not enough: DEVIATION D33 wired physical X
# to `in.z` believing `z` was grab, leg [3e] asserted the bit, both planes
# were green, and the owner pressed X and NOTHING HAPPENED — `z` is an
# alternate ATTACK button in this engine and dispatches GRAB zero times.
# So leg [4] builds port/gfx/ctl_seam_witness.c against the REAL headless
# sim and drives physical button -> real resolver -> REAL sim tick ->
# the actionState the engine actually enters, for every role D33 moved.
# A bit that reaches no action now fails here.
#
# Leg [5] is the same shape one plane over (/CONTEXT.md "Emitted vs
# renderable"): every vfx name the sim can EMIT must have a template the
# renderer can DRAW. It was a hand-run one-liner; a hand-run one-liner is
# not a check.
#
# Usage: bash port/gfx/check-ctl-input.sh
set -eu

cd "$(dirname "$0")/../.."
ROOT=$PWD
BUILD=.loop/ctl-input
GFX=port/gfx
FOH=port/foh
FROZEN=$FOH/keymap-frozen.txt

fail() { echo "CTL INPUT CHECK FAIL: $*" >&2; exit 1; }

CFLAGS=(-O2 -ffp-contract=off -Wall -Wextra -Werror
        -Iport/ryu -Iport/sim -Ioracle/qjs)
# The witness's own TU list. foh_render.c/foh_font.c/raster.c/img1.c are
# here for ONE symbol — foh_anim_tick, which foh_tick calls — and the
# ml_ser/ml_fmt/sha256/fdlibm tail is what foh_render.c needs in turn.
# Linking them is how leg [3b] gets to drive the REAL foh_tick instead of
# a re-implementation of it.
srcs_for() { # $1 = tree root (the real tree, or a perturbed copy)
  echo "$1/$GFX/ctl_input_witness.c $1/$GFX/ctl_style.c $1/$FOH/foh.c \
$1/$FOH/foh_render.c $1/$FOH/foh_font.c $1/$GFX/gfx_glyphs.c \
$1/$GFX/raster.c $1/$GFX/img1.c \
$ROOT/oracle/qjs/sha256.c $ROOT/port/sim/ml_ser.c $ROOT/port/sim/ml_fmt.c \
$ROOT/port/fdlibm/fdlibm.c"
}

# --- leg [4]'s data plane and TU list ---------------------------------------
# The seam witness links the WHOLE headless sim, so it needs exactly what
# port/sim/check-sim.sh needs: the M1 generated CTAB1/STAB1 tables and the
# SIMDATA1 executed move-data plane. Regenerated every run (both are cheap
# and stamp-cached) rather than reused, because a stale data plane would
# make leg [4] assert against a sim that is not this tree's.
TABLES=pipeline/build/sim-tables
SIMDATA=port/sim/calib/build/simdata.txt
SIM=port/sim/sim
CAL=port/sim/calib
SEAM_CFLAGS=(-O2 -ffp-contract=off -Wall -Wextra -Werror
             -I"$ROOT/$TABLES" -I"$ROOT/port/ryu" -I"$ROOT/port/sim"
             -I"$ROOT/oracle/qjs")
# $1 = tree root. Only the port/gfx TUs come from it — port/sim is never
# ours to perturb, and the copy reaches it through a symlink so the
# witness's own "../sim/..." includes resolve to the REAL sim while its
# quoted port/gfx includes still resolve inside the copy.
seam_srcs_for() {
  echo "$1/$GFX/ctl_seam_witness.c $1/$GFX/ctl_style.c \
$ROOT/$SIM/sim_boot.c $ROOT/$SIM/sim_tick.c $ROOT/$SIM/sim_ser.c \
$ROOT/$SIM/sim_data.c $ROOT/$CAL/canon.c $ROOT/$CAL/player_canon.c \
$ROOT/port/sim/physics.c $ROOT/port/sim/interpolated_collision.c \
$ROOT/port/sim/environmental_collision.c $ROOT/port/sim/hit_detection.c \
$ROOT/port/sim/article.c $ROOT/port/sim/action_state_shortcuts.c \
$ROOT/port/sim/ml_events.c $ROOT/port/sim/ml_fmt.c $ROOT/port/sim/ml_ser.c \
$ROOT/port/sim/ai_bridge.c $ROOT/port/sim/input/interpret_inputs.c \
$ROOT/port/sim/stages/moving_platforms.c $ROOT/port/sim/stages/ystory.c \
$ROOT/port/sim/stages/fountain.c \
$ROOT/port/sim/characters/shared/moves_index.c \
$ROOT/port/sim/characters/shared/moves/*.c \
$ROOT/port/sim/characters/fox/moves_index.c \
$ROOT/port/sim/characters/fox/moves/*.c \
$ROOT/port/sim/characters/falco/moves_index.c \
$ROOT/port/sim/characters/falco/moves/*.c \
$ROOT/port/sim/characters/falcon/moves_index.c \
$ROOT/port/sim/characters/falcon/moves/*.c \
$ROOT/port/sim/characters/marth/moves_index.c \
$ROOT/port/sim/characters/marth/dancing_blade_combo.c \
$ROOT/port/sim/characters/marth/dancing_blade_air_mobility.c \
$ROOT/port/sim/characters/marth/moves/*.c \
$ROOT/port/sim/characters/puff/moves_index.c \
$ROOT/port/sim/characters/puff/puff_multi_jump_drift.c \
$ROOT/port/sim/characters/puff/puff_next_jump.c \
$ROOT/port/sim/characters/puff/moves/*.c \
$ROOT/$TABLES/ml_tables.c $ROOT/$TABLES/ml_stages.c \
$ROOT/oracle/qjs/sha256.c $ROOT/port/fdlibm/fdlibm.c"
}

rm -rf "$BUILD"
mkdir -p "$BUILD"

# --- [1] the witness against the REAL tree ----------------------------------
echo "=== [1] witness vs the real tree"
# shellcheck disable=SC2046
cc "${CFLAGS[@]}" -o "$BUILD/w" $(srcs_for "$ROOT") -lm \
  || fail "the witness did not build against the real tree"
"$BUILD/w" "$ROOT/$FROZEN" || fail "the witness FAILED against the real tree"

# --- [4] THE SEAM: physical button -> resolver -> REAL sim tick -> state -----
# Built and run before the teeth so the teeth have something to bite.
echo "=== [4] the resolver->sim seam"
bash pipeline/extractor/build-extractor.sh > "$BUILD/dataplane.log" 2>&1 \
  || fail "the M1 extractor build failed — see $BUILD/dataplane.log"
node pipeline/run.js --only animations,tables,stages --out "$TABLES" \
  >> "$BUILD/dataplane.log" 2>&1 \
  || fail "the M1 pipeline run failed — see $BUILD/dataplane.log"
mkdir -p "$(dirname "$SIMDATA")"
node "$CAL/dump-sim-data.js" --out "$SIMDATA" >> "$BUILD/dataplane.log" 2>&1 \
  || fail "the SIMDATA1 dump failed — see $BUILD/dataplane.log"
# shellcheck disable=SC2046
cc "${SEAM_CFLAGS[@]}" -o "$BUILD/s" $(seam_srcs_for "$ROOT") -lm \
  || fail "the seam witness did not build against the real tree"
"$BUILD/s" "$ROOT/$SIMDATA" \
  || fail "the seam witness FAILED against the real tree"

# --- [2] teeth: one perturbed file per copy, each must kill the witness ------
echo "=== [2] teeth (perturbed copies)"
teeth=0

# mk_copy <name> — a full source copy of port/gfx + port/foh, so a quoted
# include inside the copy resolves BESIDE the copy (the T12 idiom from
# check-device-foh.sh). Copies, not symlinks for the two OWNED trees: a
# symlinked TU's quoted includes can resolve through the link and silently
# bypass the tooth.
mk_copy() {
  local d="$BUILD/$1"
  rm -rf "$d"
  mkdir -p "$d/port"
  cp -R "$ROOT/$GFX" "$d/port/gfx"
  cp -R "$ROOT/$FOH" "$d/port/foh"
  # port/sim is NOT copied and NOT perturbable: leg [4]'s witness must
  # measure the real engine. A symlink is right here for the same reason
  # it is wrong above — we WANT "../sim/..." to resolve out of the copy.
  ln -s "$ROOT/port/sim" "$d/port/sim"
  echo "$d"
}

# tooth <name> <file-under-copy> <perl-substitution> <what it should break>
tooth() {
  local name=$1 rel=$2 subst=$3 what=$4 d rc
  d=$(mk_copy "$name")
  perl -0pi -e "$subst" "$d/$rel" || fail "$name: substitution errored"
  if cmp -s "$d/$rel" "$ROOT/$rel"; then
    fail "$name: the perturbation was a NO-OP (dead tooth) on $rel"
  fi
  # shellcheck disable=SC2046
  cc "${CFLAGS[@]}" -o "$d/w" $(srcs_for "$d") -lm 2>"$d/build.log" || {
    fail "$name: the perturbed copy did not BUILD — a compile error is not
  evidence that the assertion bites. See $d/build.log"
  }
  rc=0
  "$d/w" "$d/$FROZEN" > "$d/run.log" 2>&1 || rc=$?
  [ "$rc" = 1 ] \
    || fail "$name: perturbed witness exited $rc (want exactly 1). It was
  supposed to break: $what"
  grep -q "^FAIL " "$d/run.log" \
    || fail "$name: the witness exited 1 with no FAIL line — wrong failure mode"
  echo "   $name OK: $what"
  teeth=$((teeth + 1))
}

# tooth_seam <name> <file-under-copy> <perl-substitution> <what it breaks>
# The same contract against leg [4]'s witness, so a tooth can assert THE
# OUTCOME PROTECTED — an action state the engine fails to enter — instead
# of a bit. That distinction is the whole of A42: the shipped defect
# passed every bit assertion there was.
tooth_seam() {
  local name=$1 rel=$2 subst=$3 what=$4 d rc
  d=$(mk_copy "$name")
  perl -0pi -e "$subst" "$d/$rel" || fail "$name: substitution errored"
  if cmp -s "$d/$rel" "$ROOT/$rel"; then
    fail "$name: the perturbation was a NO-OP (dead tooth) on $rel"
  fi
  # shellcheck disable=SC2046
  cc "${SEAM_CFLAGS[@]}" -o "$d/s" $(seam_srcs_for "$d") -lm \
    2>"$d/seam-build.log" || {
    fail "$name: the perturbed copy did not BUILD — a compile error is not
  evidence that the assertion bites. See $d/seam-build.log"
  }
  rc=0
  "$d/s" "$ROOT/$SIMDATA" > "$d/seam-run.log" 2>&1 || rc=$?
  [ "$rc" = 1 ] \
    || fail "$name: perturbed SEAM witness exited $rc (want exactly 1). It was
  supposed to break: $what"
  grep -q "^FAIL " "$d/seam-run.log" \
    || fail "$name: exited 1 with no FAIL line — wrong failure mode"
  echo "   $name OK: $what"
  teeth=$((teeth + 1))
}

# T1 — THE FIX ITSELF. Put the pre-2026-08-24 keysym back. The witness
# must die: 'm' no longer reaches .l, and the arm/SSOT/target-select legs
# all go red together, which is exactly the shape of the shipped bug.
tooth t1-oldkeysym port/gfx/platform_keymap.h \
  "s!\\{\"l\", 'K', 'm',!\\{\"l\", 'K', 'k',!" \
  "restoring keysym 'k' on the l row kills the witness"

# T2 — the SSOT comparison is not vacuous. Perturb only the FROZEN FILE,
# leaving the compiled table right: leg [2] must notice the drift while
# the arm keeps working.
tooth t2-frozendrift port/foh/keymap-frozen.txt \
  "s!^map l K m\$!map l K z!m" \
  "a frozen keymap that drifts from the compiled table is caught"

# T3 — the Mod-shoulder default is really read. Flip the cell back.
tooth t3-modonl port/gfx/ctl_style.c \
  "s!static bool g_modOnR = true;!static bool g_modOnR = false;!" \
  "reverting the Mod shoulder to L is caught"

# T4 — L's SHIELD claim is about the real role resolver, not a comment.
# Re-cut for DEVIATION D31: the expression is L-ONLY now, so the tooth
# RESTORES the pre-D31 both-shoulders form. That is the exact regression
# that would silently un-free R and take the C-layer down with it.
tooth t4-bothshields port/gfx/s1_input.h \
  "s!\\*shield = p->l;!*shield = (p->l || p->r);!" \
  "letting R shield again (the pre-D31 form) is caught"

# T5 — the C-layer really moved to the shoulder (D32). Put it back on Y:
# the styles that have a C-layer keep one, so nothing crashes, but R stops
# driving cs and leg [3d] dies.
tooth t5-clayeronY port/gfx/s1_input.h \
  "s!\\*clayer = ctl_style_has_clayer\\(style\\) \\? p->r : false;!*clayer = ctl_style_has_clayer(style) ? p->y : false;!" \
  "moving the C-layer back onto Y is caught"

# T6 — THE SHIPPED DEFECT, AT THE SEAM. Zero the light-shield half of
# X's Z chord and X stops reaching a grab arm entirely: DASH.c:72/80,
# RUN.c:60 and KNEEBEND.c:66 all gate on `lA > 0 || rA > 0`, and WAIT.c:56
# is what carries a standing press into GUARDON. Leg [3e] catches the bit,
# but the point of this tooth is leg [4]: the OWNER'S SYMPTOM — press X,
# nothing happens — is now a mechanical failure, in the sim, on a state.
tooth_seam t6-nograb port/gfx/s1_input.h \
  "s!#define S1_ZGRAB_LA \\(49\\.0 / 140\\.0\\)!#define S1_ZGRAB_LA (0.0)!" \
  "a pad whose X reaches no GRAB state is caught AT THE SIM"

# T7 — and the face plane is not just SOME permutation: swap jump and
# attack, which compiles and emits four live bits, and [3e] still bites.
tooth t7-facescramble port/gfx/s1_input.h \
  "s!  in.a = p->b \\|\\| p->x; // ATTACK \\(and the A half of X's Z synthesis\\)\n  in.b = p->y;         // SPECIAL\n  in.x = p->a;         // JUMP!  in.a = p->a || p->x; // ATTACK\n  in.b = p->y;         // SPECIAL\n  in.x = p->b;         // JUMP!" \
  "scrambling jump/attack across the face plane is caught"

# T8 — and the target-select step is the real foh_tick arm.
tooth t8-nostep port/foh/foh.c \
  "s!s->p1Char = s->p1Char == 0 \\? 4 : s->p1Char - 1;!s->p1Char = s->p1Char;!" \
  "a dead L arm in target-select is caught"

# T9 — leg [4] is not carried by its grab case alone. Swap SPECIAL and
# JUMP: every bit is still live, the resolver still emits four distinct
# roles, and X still grabs — but A no longer reaches KNEEBEND and Y no
# longer reaches NEUTRALSPECIALGROUND. This is the tooth that says the
# next remap of ANY face role cannot ship green.
tooth_seam t9-seamscramble port/gfx/s1_input.h \
  "s!  in.b = p->y;         // SPECIAL\n  in.x = p->a;         // JUMP!  in.b = p->a;         // SPECIAL\n  in.x = p->y;         // JUMP!" \
  "a face role that emits a bit but reaches no ACTION is caught"

# T10 — the UPPER bound on the light shield is load-bearing too, and it
# is the half a "just make it 1.0" edit would take out: GUARDON.c:21 arms
# powerShieldActive on `max(lA,rA) === 1`, so a full-strength value would
# powershield on every grab press. Leg [3e]'s `in.lA < 1` is what holds it.
tooth t10-powershield port/gfx/s1_input.h \
  "s!#define S1_ZGRAB_LA \\(49\\.0 / 140\\.0\\)!#define S1_ZGRAB_LA (1.0)!" \
  "a light shield at full strength (powershield on every grab) is caught"

[ "$teeth" = 10 ] || fail "expected 10 teeth, ran $teeth"

# --- [5] EMITTED vs RENDERABLE — the sim->renderer seam ---------------------
# Same defect shape as leg [4], one plane over: the sim EMITS vfx names,
# the renderer KNOWS templates, and an effect only appears if both are
# true. This was verified by hand on 2026-08-24 and left uncommitted,
# which is how it would have rotted.
#
# The names are taken from the CALL, never from a bare string match:
# `falconpunchbird` reads exactly like a vfx name and is a SOUND
# (ml_sound_play). /CONTEXT.md "vfx name and sound name".
echo "=== [5] emitted vfx vs renderable templates"
VFXDATA=$GFX/vfxdata-frozen.txt

emitted_vfx() {
  grep -rhoE 'ml_drawVfx[a-z_]*\("[A-Za-z0-9_]+"' "$ROOT/port/sim" \
    | sed -E 's/.*"([^"]+)"/\1/' | sort -u
}
renderable_vfx() { # $1 = a vfxdata file
  grep -h '^TPL ' "$1" | awk '{print $2}' | sort -u
}

# The extraction above only sees STRING-LITERAL first arguments, so it is
# blind to any emitter called with a variable. Assert there are none —
# outside ml_events.{c,h}, which is where the `const char *name` parameter
# itself lives. Without this guard, adding one variable-named emit site
# would make the whole leg quietly vacuous.
nonlit=$(grep -rnE 'ml_drawVfx[a-z_]*\(' "$ROOT/port/sim" \
  | grep -v '/ml_events\.[ch]:' \
  | grep -vE 'ml_drawVfx[a-z_]*\("' || true)
[ -z "$nonlit" ] || fail "[5] a vfx emit site does not pass a string literal,
  so the name extraction is blind to it:
$nonlit"

emitted_vfx > "$BUILD/vfx-emitted.txt"
renderable_vfx "$ROOT/$VFXDATA" > "$BUILD/vfx-renderable.txt"
missing=$(comm -23 "$BUILD/vfx-emitted.txt" "$BUILD/vfx-renderable.txt")
[ -z "$missing" ] || fail "[5] the sim EMITS vfx the renderer cannot DRAW —
  every name below would be silently dropped at runtime:
$missing"
echo "   [5] OK: $(wc -l < "$BUILD/vfx-emitted.txt" | tr -d ' ') emitted names,\
 all renderable ($(wc -l < "$BUILD/vfx-renderable.txt" | tr -d ' ') templates)"

# T11 — and leg [5] can fail. Drop the template for a name the sim really
# emits and the comparison must find it. Perturbs a COPY; the frozen file
# on disk is never touched.
first_emitted=$(head -1 "$BUILD/vfx-emitted.txt")
[ -n "$first_emitted" ] || fail "[5] no emitted vfx names found at all"
grep -v "^TPL $first_emitted\$" "$ROOT/$VFXDATA" > "$BUILD/vfxdata-tooth.txt"
cmp -s "$BUILD/vfxdata-tooth.txt" "$ROOT/$VFXDATA" \
  && fail "T11: the perturbation was a NO-OP (dead tooth)"
if [ -z "$(comm -23 "$BUILD/vfx-emitted.txt" \
           <(renderable_vfx "$BUILD/vfxdata-tooth.txt"))" ]; then
  fail "T11: removing the '$first_emitted' template did NOT show up as
  emitted-but-not-renderable — leg [5] is vacuous"
fi
echo "   T11 OK: a missing renderer template is caught"
teeth=$((teeth + 1))

[ "$teeth" = 11 ] || fail "expected 11 teeth, ran $teeth"
echo "CTL INPUT CHECK OK ($teeth teeth)"
