#!/usr/bin/env bash
# port/foh/check-tbuild.sh — HOST-ONLY. A45 T3 + T4: the TARGET BUILDER and
# the target-select CUSTOM page.
#
# WHAT THIS PROVES, AND WHY IT IS SHAPED THIS WAY
# ----------------------------------------------
# The owner clicked TARGET BUILDER and reported *"nothing happened"*. He was
# right: menu-top row 2 played `deny` and emitted `refused targetbuilder`.
# So the assertion here is not "the editor compiles" — it is THE OWNER'S OWN
# JOURNEY, driven through the real `foh_tick` from a cold `foh_init`:
#
#     TARGET BUILDER -> place a target -> save it to a slot -> PLAY that slot
#
# and every refusal along the way must be a STRING ON SCREEN, never a sound
# he cannot hear. Nothing is hand-poked: the menu is navigated, the crosshair
# is moved by holding the d-pad, the pause menu is walked row by row, and the
# launch is a real A press on a real target-select slot.
#
# THE DIFFERENTIAL. The builder has its own .mlstage reader, because
# `custom_stage.h` transitively includes `sim/sim.h` and therefore the
# GENERATED ml_stages.h — measured: port/foh cannot include it, let alone
# link it, without dragging the whole sim into sixteen check scripts. Two
# implementations of one grammar are only safe if something proves they
# agree, so leg [4] runs BOTH over one corpus — good, bad SUM, stale SUM,
# truncated, bad magic, split code line, trailing junk, a re-SUMmed stage
# carrying a DAMAGE digit, empty, absent — and requires the same verdict and
# the same parsed content on every one. The sim's side is its OWN UNMODIFIED
# `mlk_slot_load`; nothing in port/sim is touched by this ticket.
#
# That leg is not decoration. It FOUND A CRASH PATH while this ticket was
# being built: the FOH decides what target-select draws as a playable custom
# slot, and the builder's reader was waving through a damage-carrying stage
# that `mlk_stage_playable` refuses — so the slot would have drawn as
# present, launched on A, and died inside the sim. foh_tbuild.c now mirrors
# all three of the sim's playability rules at the READ, and leg [4] is what
# keeps the two in step.
#
# HOST-ONLY. No device, no OPK, no ADB. It does need `node`, for the
# pipeline's table stages (the probe links generated CTAB1/STAB1), which it
# builds into its OWN dir.
#
# `bash port/foh/check-tbuild.sh` -> `TBUILD CHECK OK`, exit 0.
#
# NOT A GATE. This is a task-level check with teeth.
set -euo pipefail

FOH=port/foh
GFX=port/gfx
SIM=port/sim
BUILD=$FOH/build/tbuild
DATA=$BUILD/data
CORP=$BUILD/corpus
RUN=$BUILD/run

fail() { echo "TBUILD CHECK FAIL: $1" >&2; exit 1; }
grammar_die() { echo "TBUILD CHECK FAIL: $1" >&2; exit 2; }
relay_lines() { sed 's/^/  | /'; }
made() {
  local f
  for f in "$@"; do
    [ -s "$f" ] || fail "expected artifact missing/empty: $f"
  done
}

# --- [0] run lock (mkdir-atomic, NO reclaim — check-legibility.sh pattern) ---
LOCK=$FOH/build/tbuild.lock
mkdir -p "$FOH/build"
mkdir "$LOCK" 2>/dev/null \
  || fail "another run holds $LOCK (remove it only if you are sure no run is live)"
trap 'rmdir "$LOCK" 2>/dev/null || true' EXIT

# --- [0a] no-commit guard ----------------------------------------------------
# Carried verbatim from check-legibility.sh, including both of its recorded
# corrections: the `-f` test (a wholly-untracked nested checkout is reported
# as a bare DIRECTORY) and the name list (so a directory appearing or
# disappearing stays visible even though its bytes are not walked).
tree_fingerprint() {
  local status diff files hashes f
  status="$(git status --porcelain)" || return 1
  diff="$(git diff)" || return 1
  files="$(git ls-files -o --exclude-standard)" || return 1
  hashes=""
  while IFS= read -r f; do
    [ -n "$f" ] || continue
    if [ -f "$f" ]; then
      hashes="$hashes$(shasum -a 256 "$f" 2>/dev/null | cut -d' ' -f1) $f
"
    fi
  done <<< "$files"
  printf '%s\n%s\n%s\n%s' "$status" "$diff" "$files" "$hashes" \
    | shasum -a 256 | cut -d' ' -f1
}
git_dirty_before="$(tree_fingerprint)" \
  || fail "could not fingerprint the working tree (fails CLOSED)"
[[ "$git_dirty_before" =~ ^[0-9a-f]{64}$ ]] \
  || fail "tree fingerprint malformed before the run (fails CLOSED)"

rm -rf "$BUILD"
mkdir -p "$BUILD" "$CORP" "$RUN"

CFLAGS_COMMON=(-ffp-contract=off -Wall -Wextra -Werror
  -Iport/ryu -Iport/sim -Ioracle/qjs)

# --- [1] grammar pins + the CAP CROSS-CHECK ----------------------------------
# foh_tbuild.c restates two numbers the SIM owns, because it cannot include
# the headers that define them (see the TU note in foh_tbuild.h). A restated
# constant that can drift silently is worse than no constant, so this leg
# compiles BOTH sides together and _Static_asserts they are equal.
echo "=== [1] the two restated caps must equal the sim's own"
cat > "$BUILD/capcheck.c" <<'EOF'
// Compiles the FOH's restated caps against the SIM's originals in ONE TU.
#include "foh.h"                    // FOH_TB_SLOT_CACHE
#include "target/custom_stage.h"    // MLK_MAX_SLOTS
#include "target/target_play.h"     // ML_MAX_TARGETS
#include "stage_types.h"            // ML_MAX_LABELLED_SURFACES
#include <stdio.h>
// foh_tbuild.c's private copy, restated here by the SAME literal it uses.
#define FOH_TB_PLAYABLE_TARGETS 10
_Static_assert(FOH_TB_SLOT_CACHE == MLK_MAX_SLOTS,
               "foh.h's FOH_TB_SLOT_CACHE has drifted from MLK_MAX_SLOTS");
_Static_assert(FOH_TB_PLAYABLE_TARGETS == ML_MAX_TARGETS,
               "foh_tbuild.c's target cap has drifted from ML_MAX_TARGETS");
int main(void) {
  printf("CAPS OK slots=%d targets=%d labelled=%d\n", MLK_MAX_SLOTS,
         ML_MAX_TARGETS, ML_MAX_LABELLED_SURFACES);
  return 0;
}
EOF
# The literal in the check must BE the literal in the source, not a number
# someone hoped matched: pin it.
n="$(grep -cxF '#define FOH_TB_PLAYABLE_TARGETS 10' "$FOH/foh_tbuild.c")" || true
[ "$n" = 1 ] || grammar_die \
  "foh_tbuild.c has $n copies of '#define FOH_TB_PLAYABLE_TARGETS 10' (want 1)
   — the cap moved; re-read foh_tbuild.c and re-sync this check's capcheck.c"

# --- [2] the data planes the probe links -------------------------------------
echo "=== [2] generated tables (own dir; host prerequisite: node)"
node pipeline/run.js --only animations,tables,stages,targets --out "$DATA" \
  > "$BUILD/pipeline.log" 2>&1 \
  || { relay_lines < "$BUILD/pipeline.log"
       fail "the pipeline's table stages did not run (host prerequisite: node)"; }
made "$DATA/ml_tables.c" "$DATA/ml_stages.c" "$DATA/ml_targets.c"

cc -O1 "${CFLAGS_COMMON[@]}" -I"$FOH" -I"$DATA" -o "$BUILD/capcheck" \
  "$BUILD/capcheck.c" \
  || fail "the cap cross-check did not compile — a restated cap has DRIFTED
   from the sim's own (the _Static_assert names which one)"
"$BUILD/capcheck" | relay_lines

# --- the two build recipes, defined once -------------------------------------
# ONE list, several consumers (the real build and every tooth), so a tooth can
# never be built against a different set of bodies than the real run.
WIT_SRCS=("$FOH/foh_tbuild_witness.c" "$FOH/foh.c" "$FOH/foh_render.c"
  "$FOH/foh_font.c" "$FOH/foh_persist.c" "$FOH/foh_tbuild.c"
  "$GFX/raster.c" "$GFX/img1.c" "$GFX/ctl_style.c"
  "$SIM/stage_code.c" "$SIM/ml_fmt.c" "$SIM/ml_ser.c"
  oracle/qjs/sha256.c port/fdlibm/fdlibm.c)
PROBE_SRCS=("$FOH/tbuild/mlstage_probe.c" "$SIM/stage_code.c"
  "$SIM/target/custom_stage.c" "$SIM/ml_fmt.c" oracle/qjs/sha256.c)

build_wit() { # <outdir> [extra -I]
  cc -O1 "${CFLAGS_COMMON[@]}" ${2:+-I"$2"} -o "$1/wit" "${WIT_SRCS[@]}" -lm
}

# --- [3] the witness against the REAL tree -----------------------------------
echo "=== [3] the owner's journey through the real foh_tick"
mkdir -p "$BUILD/real"
build_wit "$BUILD/real" || fail "the witness did not build against the real tree"
made "$BUILD/real/wit"
rm -rf "$RUN/real" && mkdir -p "$RUN/real"
MLFK_PERSIST_DIR="$RUN/real" "$BUILD/real/wit" > "$BUILD/real/wit.out" 2>&1 \
  || { relay_lines < "$BUILD/real/wit.out"
       fail "the witness failed against the REAL tree"; }
relay_lines < "$BUILD/real/wit.out"
nok="$(grep -cxF 'TBUILD OK' "$BUILD/real/wit.out")" || true
[ "$nok" = 1 ] || fail "the real witness printed $nok 'TBUILD OK' lines (want 1)"
nasserts="$(grep -c '^  ok  ' "$BUILD/real/wit.out")" || true
[ "$nasserts" -ge 70 ] \
  || fail "the witness made only $nasserts assertions (want >= 70) — a
   silently-skipped leg is how a witness stops witnessing"
echo "  $nasserts assertions passed"

# --- [4] THE DIFFERENTIAL: the FOH reader vs the SIM's own mlk_slot_load -----
echo "=== [4] .mlstage differential — the builder's reader vs the sim's"
cc -O1 "${CFLAGS_COMMON[@]}" -I"$DATA" -o "$BUILD/probe" "${PROBE_SRCS[@]}" \
  || fail "the sim-side probe did not build"
made "$BUILD/probe"

# The corpus is built FROM THE FILE THE BUILDER JUST WROTE, so the "good"
# entry is a real product of the real save path, not a hand-authored fixture.
made "$RUN/real/custom0.mlstage"
node - "$RUN/real/custom0.mlstage" "$CORP" > "$BUILD/corpus.log" 2>&1 <<'EOF'
const fs = require("fs"), crypto = require("crypto");
const [src, dir] = process.argv.slice(2);
const b = fs.readFileSync(src);
const put = (slot, buf) => fs.writeFileSync(`${dir}/custom${slot}.mlstage`, buf);
const seal = body =>
  Buffer.concat([body, Buffer.from("SUM " +
    crypto.createHash("sha256").update(body).digest("hex") + "\n")]);
// 0  the good file, exactly as the builder published it
put(0, b);
// 1  one SUM digit flipped
{ const x = Buffer.from(b); x[x.length - 5] ^= 1; put(1, x); }
// 2  one CODE byte flipped, SUM left stale
{ const x = Buffer.from(b); x[20] = x[20] === 0x39 ? 0x38 : 0x39; put(2, x); }
// 3  truncated mid-code — the POWER-LOSS shape, a measured risk on this
//    journal-less vfat, so an expected input rather than a hypothetical
put(3, b.subarray(0, b.length >> 1));
// 4  bad magic, SUM RE-COMPUTED so only the grammar rule can catch it
{ const x = Buffer.from(b); x[0] = 0x58;
  put(4, seal(x.subarray(0, x.length - 69))); }
// 5  the code split over two lines, SUM RE-COMPUTED
{ const code = b.toString("latin1").split("\n")[1];
  put(5, seal(Buffer.from("MLSTAGE1\n" + code.slice(0, 10) + "\n" +
                          code.slice(10) + "\n", "latin1"))); }
// 6  trailing junk after the SUM line
put(6, Buffer.concat([b, Buffer.from("junk\n")]));
// 7  a stage carrying a DAMAGE digit, SUM RE-COMPUTED — grammar and SUM are
//    both fine, so ONLY a playability rule can refuse it. This is the entry
//    that found the crash path (see this file's header).
{ const f = b.toString("latin1").split("\n")[1].split("&");
  f[2] = f[2].replace(/,0/g, ",1");
  put(7, seal(Buffer.from("MLSTAGE1\n" + f.join("&") + "\n", "latin1"))); }
// 8  empty file
put(8, Buffer.alloc(0));
// 9  deliberately ABSENT
console.log("corpus: 9 files + one absent slot");
EOF
relay_lines < "$BUILD/corpus.log"
[ -e "$CORP/custom9.mlstage" ] && fail "corpus slot 9 must be ABSENT"

agree=0
for sl in 0 1 2 3 4 5 6 7 8 9; do
  f_out="$(MLFK_PERSIST_DIR="$CORP" "$BUILD/real/wit" --probe "$sl" 2>&1)" || true
  s_out="$("$BUILD/probe" "$CORP" "$sl" 2>&1)" || true
  f_verdict="$(printf '%s' "$f_out" | awk '{print $2}')"
  s_verdict="$(printf '%s' "$s_out" | awk '{print $2}')"
  # The REASON STRINGS legitimately differ — two implementations, and the
  # FOH's have to fit a 240 px line. What must agree is the VERDICT and,
  # when accepted, the PARSED CONTENT.
  f_body="$(printf '%s' "$f_out" | cut -d' ' -f3-)"
  s_body="$(printf '%s' "$s_out" | cut -d' ' -f3-)"
  case "$f_verdict" in
    OK|REFUSED) ;;
    *) fail "slot $sl: the FOH probe printed no verdict: '$f_out'";;
  esac
  [ "$f_verdict" = "$s_verdict" ] || fail \
    "slot $sl DISAGREEMENT — FOH says $f_verdict, the sim says $s_verdict
     FOH: $f_out
     SIM: $s_out
   The builder's reader and mlk_slot_load must accept exactly the same files:
   target-select draws its custom page from the FOH verdict, so a file the
   FOH accepts and the sim refuses is a slot that launches into a loud death."
  if [ "$f_verdict" = "OK" ]; then
    [ "$f_body" = "$s_body" ] || fail \
      "slot $sl: both accepted but PARSED DIFFERENTLY
     FOH: $f_body
     SIM: $s_body"
  fi
  printf '  %s  slot %s  %s / %s\n' "==" "$sl" "$f_verdict" "$s_verdict"
  agree=$((agree + 1))
done
[ "$agree" = 10 ] || fail "the differential judged $agree/10 corpus entries"
# The corpus must not be all-refusals, or the differential agrees vacuously.
nacc=0
for sl in 0 1 2 3 4 5 6 7 8 9; do
  if "$BUILD/probe" "$CORP" "$sl" 2>&1 | grep -q '^SIMLOAD OK'; then
    nacc=$((nacc + 1))
  fi
done
[ "$nacc" -ge 1 ] \
  || fail "no corpus entry is ACCEPTED — two readers that refuse everything
   agree vacuously, which proves nothing"
echo "  10/10 verdicts agree ($nacc accepted, $((10 - nacc)) refused)"

# --- TEETH -------------------------------------------------------------------
# Every tooth perturbs a COPY. The committed sources are never written.
# Each is checked for BITING (the right leg fails) and for ORTHOGONALITY
# (the other legs do not), because a tooth that fails everything proves only
# that the build broke.
perturb() { # <dir> <file-under-FOH> <from> <to> [<from> <to> ...]
  # bash 3.2 declares every name in a single `local` BEFORE assigning any of
  # them, so `local a="$1" b="$BUILD/$a"` reads an unset $a and, under
  # `set -u`, dies. One name per statement (CLAUDE.local.md's bash-3.2 note).
  local dir="$1"
  local rel="$2"
  shift 2
  mkdir -p "$BUILD/$dir"
  node -e '
    const fs = require("fs");
    const [src, dst, ...pairs] = process.argv.slice(1);
    let raw = fs.readFileSync(src, "utf8");
    for (let i = 0; i < pairs.length; i += 2) {
      const from = pairs[i], to = pairs[i + 1];
      const hits = raw.split(from).length - 1;
      if (hits !== 1) {
        console.error(`perturb: |${from}| matched ${hits} times (want exactly 1)`);
        process.exit(1);
      }
      raw = raw.replace(from, to);
    }
    fs.writeFileSync(dst, raw);
  ' "$FOH/$rel" "$BUILD/$dir/$(basename "$rel")" "$@" \
    || fail "$dir: the perturbation did not apply"
  cmp -s "$BUILD/$dir/$(basename "$rel")" "$FOH/$rel" \
    && fail "$dir: the perturbed copy is byte-identical to $rel (DEAD TOOTH)"
  return 0
}

# Build a witness with ONE source swapped for its perturbed copy.
build_tooth() { # <dir> <file-under-FOH>
  local dir="$1"
  local rel="$2"
  local out="$BUILD/$dir"
  local srcs=()
  local s
  for s in "${WIT_SRCS[@]}"; do
    if [ "$s" = "$FOH/$rel" ]; then srcs+=("$out/$(basename "$rel")")
    else srcs+=("$s"); fi
  done
  cc -O1 "${CFLAGS_COMMON[@]}" -I"$FOH" -o "$out/wit" "${srcs[@]}" -lm \
    || fail "$dir: the perturbed witness did not build"
}

run_tooth() { # <dir> -> writes <dir>/wit.out, returns the rc
  local dir="$1" rc=0
  rm -rf "$RUN/$dir" && mkdir -p "$RUN/$dir"
  MLFK_PERSIST_DIR="$RUN/$dir" "$BUILD/$dir/wit" > "$BUILD/$dir/wit.out" 2>&1 \
    || rc=$?
  return "$rc"
}

bites() { # <dir> <substring that must appear in the FAIL output>
  local dir="$1" want="$2" rc=0
  run_tooth "$dir" || rc=$?
  [ "$rc" = 1 ] || { relay_lines < "$BUILD/$dir/wit.out"
    fail "$dir: the perturbed witness exited rc $rc (want exactly 1 — rc 0
   means the witness is BLIND to the defect it is supposed to guard)"; }
  grep -qF "TBUILD FAIL: $want" "$BUILD/$dir/wit.out" \
    || { relay_lines < "$BUILD/$dir/wit.out"
         fail "$dir: expected the failure |$want| and did not get it"; }
  grep -qxF 'TBUILD OK' "$BUILD/$dir/wit.out" \
    && fail "$dir: the perturbed build still printed the OK verdict"
  return 0
}

# how many assertions a tooth broke — ORTHOGONALITY is measured, not assumed
nfails() { grep -c '^TBUILD FAIL: ' "$BUILD/$1/wit.out" || true; }

# --- [T1] D43: put upstream's CLOBBER back ----------------------------------
# The owner RULED on this (D43): slot management must not destroy other
# slots. Upstream's own add path writes `customTargetStages[length - 1]`, so
# this tooth makes every save land on ONE fixed slot the way upstream's does.
# Witness leg [9] must notice that slot 0 changed when slot 3 was saved.
echo "=== [T1] tooth: upstream's clobbering save (D43 reverted)"
perturb t1-clobber foh_tbuild.c \
  '  slot_name(slot, name);
  const char *pubWhy = 0;' \
  '  (void)slot; slot_name(0, name); // T1: upstream'"'"'s clobber — one slot
  const char *pubWhy = 0;'
build_tooth t1-clobber foh_tbuild.c
bites t1-clobber '...and custom0.mlstage is BYTE-IDENTICAL (no clobber, no shift)'

# --- [T2] the refusal goes SILENT -------------------------------------------
# THE defect this whole ticket was filed about: a refusal the owner cannot
# see. The `deny` sound stays; only the on-screen string is removed. So this
# tooth fails ONLY the visibility assertion — if it also killed the sound
# assertion it would be proving something coarser than it claims.
echo "=== [T2] tooth: the 11th-target refusal becomes a sound only"
perturb t2-silent foh_tbuild.c \
  '          foh_snd_push(s, "deny"); // :568
          say(s, "10 targets max");' \
  '          foh_snd_push(s, "deny"); // :568 — T2: no on-screen reason'
build_tooth t2-silent foh_tbuild.c
bites t2-silent '...and the refusal is a STRING ON SCREEN, not just a deny sound'
grep -qF 'ok  ...with the deny sound as well' "$BUILD/t2-silent/wit.out" \
  || fail "T2: the deny SOUND assertion also broke — this tooth must remove
   only the VISIBLE half, or it is not measuring visibility"

# --- [T3] the playability mirror is dropped from the READ -------------------
# This is the crash path leg [4] found. Removing the mirror leaves grammar
# and SUM intact, so the WITNESS still passes end to end — and only the
# DIFFERENTIAL notices, on corpus entry 7. That orthogonality is the point:
# it proves leg [4] is carrying its own weight rather than riding on [3].
echo "=== [T3] tooth: the read stops mirroring the sim's playability rules"
perturb t3-nomirror foh_tbuild.c \
  '  {
    const char *bad = doc_unplayable(out);
    if (bad) {
      if (why) *why = bad;
      return false;
    }
  }
  return true;' \
  '  return true; // T3: no playability mirror at the read'
build_tooth t3-nomirror foh_tbuild.c
# the witness itself must STILL PASS — the defect is invisible to it
run_tooth t3-nomirror || fail "T3: the witness broke, so the differential
   below would not be the thing under test (this tooth must be invisible to
   the witness and visible ONLY to leg [4])"
grep -qxF 'TBUILD OK' "$BUILD/t3-nomirror/wit.out" \
  || fail "T3: expected the witness to still print TBUILD OK"
t3f="$(MLFK_PERSIST_DIR="$CORP" "$BUILD/t3-nomirror/wit" --probe 7 2>&1)" || true
t3s="$("$BUILD/probe" "$CORP" 7 2>&1)" || true
case "$t3f" in
  "FOHLOAD OK"*) ;;
  *) fail "T3: the perturbed reader still refused corpus entry 7 ('$t3f') —
   the tooth did not bite where it was aimed";;
esac
case "$t3s" in
  "SIMLOAD REFUSED"*) ;;
  *) fail "T3: the SIM accepted corpus entry 7 ('$t3s') — the corpus entry
   is not exercising the damage refusal any more";;
esac
echo "  T3 bites leg [4] only: FOH says OK, the sim says REFUSED"

# --- [T4] SUM is no longer verified before the parse ------------------------
# A45 T2's contract says SUM is verified BEFORE parsing. This tooth skips the
# comparison entirely, so a corrupt file reaches mlk_parse.
echo "=== [T4] tooth: SUM verified is skipped (corrupt bytes reach the parser)"
perturb t4-nosum foh_tbuild.c \
  '    if (memcmp(hex, buf + sumAt + 4, 64) != 0) RD_FAIL("SUM mismatch");' \
  '    (void)hex; // T4: SUM computed and then ignored'
build_tooth t4-nosum foh_tbuild.c
t4a="$(MLFK_PERSIST_DIR="$CORP" "$BUILD/t4-nosum/wit" --probe 1 2>&1)" || true
t4b="$(MLFK_PERSIST_DIR="$CORP" "$BUILD/t4-nosum/wit" --probe 2 2>&1)" || true
bit=0
case "$t4a" in "FOHLOAD REFUSED SUM mismatch") ;; *) bit=$((bit + 1));; esac
case "$t4b" in "FOHLOAD REFUSED SUM mismatch") ;; *) bit=$((bit + 1));; esac
[ "$bit" -ge 1 ] || fail "T4: dropping the SUM check changed no verdict —
   the corpus's corrupted entries are not reaching the SUM rule
     slot 1: $t4a
     slot 2: $t4b"
echo "  T4 bites: $bit/2 corrupted entries change verdict without the SUM check"

# --- [T5] menu-top row 2 refuses again --------------------------------------
# The literal defect the owner filed. Put it back; witness leg [1] must fail.
echo "=== [T5] tooth: TARGET BUILDER refuses again (the reported defect)"
perturb t5-refuse foh.c \
  '          if (foh_tbuild_ops) foh_tbuild_ops->enter(s, -1);
          snd_push(s, "menuForward"); // menu.js:70
          ev_trans(s, sc, FOH_TBUILD, "a"); // changeGamemode(4), :90' \
  '          snd_push(s, "deny"); // T5: the pre-A45 refusal
          ev_refused(s, "targetbuilder");'
build_tooth t5-refuse foh.c
bites t5-refuse 'TARGET BUILDER -> the target-builder screen'

# --- [T6] the CUSTOM page stops flipping ------------------------------------
# T3's half of the ticket: slot 10 refusing again. Orthogonal to T5 — this
# one leaves the builder entirely alone and breaks only the play path.
echo "=== [T6] tooth: target-select slot 10 refuses again (addcode)"
perturb t6-addcode foh.c \
  '      s->tssPage = s->tssPage ? 0 : 1;
      foh_tss_refresh_slots(s);
      snd_push(s, "menuSelect");
      return;' \
  '      snd_push(s, "deny"); // T6: the pre-A45 refusal
      ev_refused(s, "addcode");
      return;'
build_tooth t6-addcode foh.c
bites t6-addcode 'A flips to the CUSTOM page (was: refused addcode)'

# ORTHOGONALITY, MEASURED. T5 and T6 both live in foh.c and both break a
# single entry point; if either cascaded into the other's legs they would be
# one tooth wearing two names.
grep -qF 'ok  TARGET BUILDER -> the target-builder screen' \
  "$BUILD/t6-addcode/wit.out" \
  || fail "T6 also broke the BUILDER entry — T5 and T6 are not orthogonal"
grep -qF 'ok  A flips to the CUSTOM page' "$BUILD/t1-clobber/wit.out" \
  || fail "T1 also broke the custom page — the clobber tooth is not orthogonal"
echo "  teeth are orthogonal (T1 $(nfails t1-clobber), T2 $(nfails t2-silent), T5 $(nfails t5-refuse), T6 $(nfails t6-addcode) failure(s) each)"

# --- [5] hygiene -------------------------------------------------------------
git_dirty_after="$(tree_fingerprint)" \
  || fail "could not fingerprint the working tree after the run (fails CLOSED)"
[ "$git_dirty_before" = "$git_dirty_after" ] \
  || fail "this check modified the working tree (it must only write $BUILD,
   which is git-ignored) before=$git_dirty_before after=$git_dirty_after"

echo "TBUILD CHECK OK"
