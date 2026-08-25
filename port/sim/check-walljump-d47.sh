#!/usr/bin/env bash
# MENU-SPEC DEVIATION **D47** — puff walljump under the D20 "Everyone
# Walljumps" house rule (owner ruling 2026-08-24 round 4 item 4, "option 1":
# reuse an EXISTING puff ECB rather than author new collision geometry).
#
# D20 gave the setting real mechanical effect but guarded it on DATA, and puff
# failed that guard: all five characters carry framesData WALLJUMP:40, but
# puff carries no WALLJUMP ECB and no WALLJUMP animation, so "Everyone
# Walljumps" silently meant "everyone except puff". D47 aliases puff's missing
# WALLJUMP ECB (port/sim/physics.c:64) and animation (port/gfx/gfx_render.c)
# to its **WALLTECHJUMP** entries. This check is the evidence:
#
#   [1] REGRESSION — bash port/sim/check-sim.sh: SIM CONFORMS, all 8 goldens
#       bit-identical with the flag off (the flag defaults off forever). Also
#       builds sim_host + the M1 data plane this check consumes, so the TU
#       list is never duplicated here.
#   [2] DATA PINS — the MEASUREMENT that justifies the reuse, re-asserted from
#       the freshly generated CTAB1/ANIM1 rather than trusted from a comment.
#   [3] INSTRUMENT POSITIVE, FIRST — fox walljumps on the witness trace with
#       NO house rule at all (charAttributes.walljump is 1 upstream). This is
#       the check the 2026-08-05 marth attempt skipped, and skipping it is why
#       twelve traces reported "no divergence" while nothing had run.
#   [4] PUFF WITNESS — the same trace, flag off vs flag on.
#   [5] MARTH WITNESS (#16) — marth's walljump path, which had never executed.
#
# Prints WALLJUMP D47 OK, exit 0; any shortfall -> nonzero.
#
# TEETH — run 2026-08-24, both bit, both restored by reverse-edit (never
# `git checkout --`, which would revert unstaged work). Not automated here:
# each needs its own rebuild of the sim, and this check deliberately does not
# carry a second copy of check-sim.sh's TU list.
#   A. delete the WALLTECHJUMP fallback inside walljump_ecb() -> the ability
#      gate rejects puff, arm [4]'s flag-on count goes 3 -> 0, check FAILS.
#   B. keep the gate permissive but delete the fallback inside ecb_state()
#      -> reproduces D20's shipped crash EXACTLY, one frame after the
#      walljump fires: `SIM FATAL frame 187: ecb: unknown action state`,
#      rc 3, and arm [4]'s "run completes" assertion catches it.
set -euo pipefail
cd "$(dirname "$0")/../.."

B=port/sim/calib/build
TABLES=pipeline/build/sim-tables
SEED=1337
STAGE=1 # ystory: the only VS stage whose wallL runs deep enough (y -12..-100,
        # measured from STAB1) for a falling character to reach it at all.
FRAMES=600

fail() { echo "WALLJUMP D47 FAIL: $*" >&2; exit 1; }

# --- [1] regression bar + build ----------------------------------------------
echo "== [1] regression: check-sim.sh (flag off, 8/8 bit-identical)"
# tee, not a pipe into tail: the eight STREAM MATCH lines ARE the evidence and
# swallowing them leaves this arm's pass unciteable.
bash port/sim/check-sim.sh | tee "$B/d47-check-sim.log"
tail -1 "$B/d47-check-sim.log" | grep -qx "SIM CONFORMS" \
  || fail "check-sim.sh did not print SIM CONFORMS"
n=$(grep -c "^STREAM MATCH" "$B/d47-check-sim.log")
[ "$n" -eq 8 ] || fail "check-sim.sh matched $n goldens, expected 8"
test -x "$B/sim_host" || fail "sim_host missing after check-sim.sh"
test -f "$B/simdata.txt" || fail "simdata.txt missing after check-sim.sh"

# --- [2] the measurement the reuse rests on -----------------------------------
echo "== [2] data pins: WALLTECHJUMP is the walljump ECB, per upstream itself"
node -e '
const fs = require("fs");
const T = require("./'"$TABLES"'/tables.json");
const eq = (a, b) => JSON.stringify(a) === JSON.stringify(b);
const die = (m) => { console.error("pin: " + m); process.exit(1); };
const by = {}; for (const c of T.chars) by[c.name] = c;

// Every character can REPRESENT the state (framesData) — that is what made
// puff look eligible to D20 in the first place.
for (const c of T.chars) {
  if (c.framesData.WALLJUMP !== 40) die(c.name + " framesData.WALLJUMP != 40");
}
// ...but only four carry the boxes, and puff is the one that does not.
for (const n of ["marth", "fox", "falco", "falcon"]) {
  const c = by[n];
  if (!c.ecb.WALLJUMP) die(n + " lost its WALLJUMP ECB");
  // THE JUSTIFICATION, four independent instances: upstream authored the
  // wall-tech-jump boxes and the walljump boxes as the SAME 40 frames.
  if (!eq(c.ecb.WALLJUMP, c.ecb.WALLTECHJUMP)) {
    die(n + ": WALLTECHJUMP ECB is no longer byte-identical to WALLJUMP — the "
      + "premise D47 reuses is gone, re-measure before trusting the alias");
  }
}
// If this ever becomes false, the pipeline is being hand-fed and D47 should be
// deleted rather than kept alongside invented data.
if (by.puff.ecb.WALLJUMP) die("puff now HAS a WALLJUMP ECB — where did it come from?");
if (!by.puff.ecb.WALLTECHJUMP) die("puff has no WALLTECHJUMP ECB to reuse");
// framesData clamps the frame index to 40 (physics.c:1456), so the source
// state must carry at least that many frames or the alias trades one domain
// trap for another.
if (by.puff.ecb.WALLTECHJUMP.length < 40) {
  die("puff WALLTECHJUMP has " + by.puff.ecb.WALLTECHJUMP.length + " ECB frames, < 40");
}
// REJECTED CANDIDATE, kept as a live pin: FALL was the owners hypothesis and
// is mechanically impossible, not merely a worse fit.
if (by.puff.ecb.FALL.length >= 40) {
  die("puff FALL now has >= 40 ECB frames — the recorded reason for rejecting "
    + "it (frame 9 would trap) no longer holds; revisit the choice");
}
// The animation half, same reuse, same measurement.
const anim = fs.readFileSync("'"$TABLES"'/anim_1_puff.bin");
const hasName = (b, n) => b.includes(Buffer.from(n, "latin1"));
if (hasName(anim, "WALLJUMP")) die("anim_1_puff.bin now HAS a WALLJUMP state");
if (!hasName(anim, "WALLTECHJUMP")) die("anim_1_puff.bin has no WALLTECHJUMP to reuse");
console.log("  puff: no WALLJUMP ECB, no WALLJUMP anim; WALLTECHJUMP present ("
  + by.puff.ecb.WALLTECHJUMP.length + " ECB frames), FALL only "
  + by.puff.ecb.FALL.length);
console.log("  marth/fox/falco/falcon: WALLTECHJUMP ECB == WALLJUMP ECB, byte-identical");
' || fail "the measurement D47 rests on no longer holds"

# --- witness traces ------------------------------------------------------------
# Two configurations, because the characters do not fall alike: fox and marth
# drop past ystory's wall and SD unless a double jump holds them beside it,
# while the same jump moves puff (far slower faller, far more air drift) off
# the wall entirely. MEASURED, not assumed — see the sweep in the D47 report.
node port/sim/walljump-trace.js "$FRAMES" 100 140 0   > "$B/wj-float.trace.txt"
node port/sim/walljump-trace.js "$FRAMES" 100 140 145 > "$B/wj-jump.trace.txt"

# actionState of player 0 for each dumped frame (awk match() takes the FIRST
# occurrence on the line, and player 0 is serialized first).
p0states() { # trace char flagOrEmpty frames
  "$B/sim_host" --trace "$1" --simdata "$B/simdata.txt" --seed "$SEED" \
    --p1 "$2" --p2 0 --stage "$STAGE" --frames "$FRAMES" ${3:+$3} \
    --dump-frames "$4" 2>&1 >/dev/null \
    | awk 'match($0, /"actionState":"[A-Z0-9]+"/) { print substr($0, RSTART, RLENGTH) }'
}
count_wj() { p0states "$@" | grep -c '"actionState":"WALLJUMP"' || true; }

WIN=190,195,200,210

# --- [3] instrument positive, BEFORE any null is trusted -----------------------
echo "== [3] instrument: fox walljumps with NO house rule (attributes.walljump=1)"
n=$(count_wj "$B/wj-jump.trace.txt" 2 "" "$WIN")
[ "$n" -eq 4 ] || fail "fox did not walljump on the witness trace ($n/4 frames) \
— the trace no longer reaches a wall, so every null below would be meaningless"
echo "  fox flag-off: WALLJUMP on 4/4 sampled frames"

# --- [4] the puff witness -------------------------------------------------------
echo "== [4] puff: the state D20 could not reach"
n=$(count_wj "$B/wj-float.trace.txt" 1 "" 185,186,190,200)
[ "$n" -eq 0 ] || fail "puff walljumped with the flag OFF ($n frames) — the house \
rule must be the only thing that grants it"
n=$(count_wj "$B/wj-float.trace.txt" 1 --walljump-all 186,190,200)
[ "$n" -eq 3 ] || fail "puff did not walljump with the flag ON ($n/3 frames)"
# The run must also COMPLETE: pre-D47 this is where D20 died with
# `ecb: unknown action state` one frame after the walljump fired.
"$B/sim_host" --trace "$B/wj-float.trace.txt" --simdata "$B/simdata.txt" \
  --seed "$SEED" --p1 1 --p2 0 --stage "$STAGE" --frames "$FRAMES" \
  --walljump-all 2>/dev/null | tail -1 | grep -qx "SIM OK" \
  || fail "puff walljump run did not complete (the ECB alias did not hold)"
echo "  puff flag-off: FALL throughout; flag-on: WALLJUMP frames 186-200, run completes"

# --- [5] marth (#16) -------------------------------------------------------------
echo "== [5] marth: the D20 path that had never executed (#16)"
n=$(count_wj "$B/wj-jump.trace.txt" 0 "" "$WIN")
[ "$n" -eq 0 ] || fail "marth walljumped with the flag OFF ($n frames)"
n=$(count_wj "$B/wj-jump.trace.txt" 0 --walljump-all "$WIN")
[ "$n" -eq 4 ] || fail "marth did not walljump with the flag ON ($n/4 frames)"
echo "  marth flag-off: JUMPAERIALB -> FALLAERIAL; flag-on: WALLJUMP from frame 186"

echo "WALLJUMP D47 OK"
