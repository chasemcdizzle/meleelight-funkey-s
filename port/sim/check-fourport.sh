#!/usr/bin/env bash
# A46 FOUR-PORT WITNESS — sim_setup_match is no longer 2-slot by
# construction, and the widening is verified against the BROWSER, not
# asserted (HARD RULE 1).
#
# check-sim.sh proves the NEGATIVE half: all 8 goldens still bit-identical
# through the 2-port sim_setup_match WRAPPER. This proves the POSITIVE
# half — that a real four-port match runs, that it differs from the
# two-port one, and that its every frame matches what the browser
# oracle produced for the same config.
#
#   [1] CONFIG PIN — port/goldens-m4/manifest-4p.json's q01 row and the
#       FROZEN stream's params must agree on every port cell, and those
#       cells are what this script hands sim_host. verify-stream.js pins
#       nine meta keys and p3/p4 are not among them (it is UNCHANGED and
#       stays that way, HARD RULE 3), so this is the crossing the oracle
#       verifier cannot see and it is checked here instead.
#   [2] CONFORMANCE — the headless C sim replays q01 (fox/falco/puff/marth
#       on battlefield, all four ports human and trace-driven) and the
#       stream is judged by the UNCHANGED oracle/harness/verify-stream.js
#       against port/goldens-m4/q01-*.sha256.json, which was recorded from
#       the real browser by port/goldens-m4/run-4p.js. Exact per-frame
#       equality over the full 3600 frames, plus the rngCalls channels.
#   [3] POSITIVE WITNESS — the frame-1 envelope carries FOUR player blocks
#       p0..p3 (CHECKSUM.md §2 emits one per playerType[i] > -1 slot), and
#       ports 2/3 spawn at upstream's startingPoint[2]/[3] = (-25,5)/(25,5).
#       The same trace/seed/stage replayed WITHOUT --p3/--p4 carries two
#       blocks and a different stream: four ports is a real difference,
#       not a flag wired to nothing.
#   [4] TEETH — three, each must bite ALONE:
#       T1 the 2-port wrapper call (no --p3/--p4) must FAIL against the
#          q01 frozen stream — the widening is load-bearing;
#       T2 swapping the port-2 and port-3 characters must FAIL — ports
#          2/3 really do reach the checksum surface, in port order;
#       T3 a three-port config (--p3 only) must FAIL — port 3's presence
#          is itself observable, not padding.
#
# WHY THERE IS NO "run-4p.js == run.js" LEG: it would be redundant. If
# run-4p.js had drifted from run.js's init pipeline (a missing fdlibm
# shim, an unseeded RNG, a second setupMatch burning a second off-step
# draw) the frozen stream would no longer be an fdlibm/mulberry32 stream
# and leg [2] — the C sim, which knows nothing about run-4p.js — could
# not match it. Recorder fidelity is proven by the conformance, not by
# a second browser run.
#
# Prints FOUR PORT OK, exit 0; any shortfall -> nonzero.
#
# This check CONSUMES check-sim.sh's build products (sim_host,
# simdata.txt) rather than duplicating its TU list — so run
# `bash port/sim/check-sim.sh` first. check-sim.sh's bytes are PINNED
# below (the check-versus-endless.sh / check-ai-live.sh producer-pin
# pattern): it is never edited, and a reviewed change to it must update
# this pin in the same commit.
set -euo pipefail
cd "$(dirname "$0")/../.."

BUILD=port/sim/calib/build
SIM=$BUILD/sim_host
M4G=port/goldens-m4
GOLDEN=q01
fail() { echo "FOUR PORT FAIL: $*" >&2; exit 1; }

# --- [0] producer byte pin ----------------------------------------------------
PIN=ce0882bee2a0bb0ad11ac51366ef467c3811d832f9dc932c4eb10dd3ccc4c8cb
have=$(shasum -a 256 port/sim/check-sim.sh | cut -d' ' -f1)
[ "$have" = "$PIN" ] || fail "check-sim.sh sha256 $have != pinned $PIN"

for f in "$SIM" "$BUILD/simdata.txt"; do
  test -f "$f" || fail "missing $f — run 'bash port/sim/check-sim.sh' first"
done

# --- [1] config pin -----------------------------------------------------------
# The manifest row and the frozen params must agree cell-for-cell, and the
# values this script uses come from that same single source.
eval "$(node -e '
  const fs = require("fs"), path = require("path");
  const M4G = "port/goldens-m4";
  const m = JSON.parse(fs.readFileSync(path.join(M4G, "manifest-4p.json"), "utf8"));
  const g = m.goldens.find((x) => x.id === "q01");
  if (!g) { console.error("q01 missing from port/goldens-m4/manifest-4p.json"); process.exit(1); }
  const frozen = JSON.parse(fs.readFileSync(
    path.join(M4G, g.name + ".sha256.json"), "utf8"));
  const p = frozen.params;
  const want = { p1: g.p1, p2: g.p2, p3: g.p3, p4: g.p4, stage: g.stage,
                 seed: g.seed, frames: g.frames, cpu: g.cpu,
                 trace: g.trace };
  for (const k of Object.keys(want)) {
    if (p[k] !== want[k]) {
      console.error("frozen params." + k + " = " + JSON.stringify(p[k]) +
        ", manifest = " + JSON.stringify(want[k]));
      process.exit(1);
    }
  }
  if (g.p3 === null && g.p4 === null) {
    console.error("q01 is not a four-port golden (p3/p4 both null)");
    process.exit(1);
  }
  if (g.cpu !== false) { console.error("q01 must be human-only"); process.exit(1); }
  const emit = (k, v) => console.log(k + "=" + String(v));
  emit("NAME", g.name); emit("TRACE", g.trace); emit("FRAMES", g.frames);
  emit("SEED", g.seed); emit("P1", g.p1); emit("P2", g.p2);
  emit("P3", g.p3); emit("P4", g.p4); emit("STAGE", g.stage);
')" || fail "[1] config pin: manifest/frozen params disagree (see above)"
echo "[1] config pin: $NAME — ports [$P1,$P2,$P3,$P4] stage $STAGE seed $SEED"

# --- [2] conformance ----------------------------------------------------------
node port/sim/sim/trace-to-txt.js "$M4G/$TRACE" "$BUILD/$GOLDEN.trace.txt" \
  >/dev/null || fail "[2] trace-to-txt failed on $TRACE"

run4() { # $1 = extra flags, $2 = stdout file
  # shellcheck disable=SC2086
  $SIM --trace "$BUILD/$GOLDEN.trace.txt" --simdata "$BUILD/simdata.txt" \
    --seed "$SEED" --p1 "$P1" --p2 "$P2" --stage "$STAGE" \
    --frames "$FRAMES" $1 > "$2"
}

run4 "--p3 $P3 --p4 $P4" "$BUILD/$GOLDEN.four.out" ||
  fail "[2] the four-port replay did not exit 0"
node port/sim/sim/wrap-run.js "$GOLDEN" "$BUILD/$GOLDEN.four.out" \
  "$BUILD/$GOLDEN.four-run.json" "$M4G/manifest-4p.json" >/dev/null ||
  fail "[2] wrap-run rejected the four-port sim output"
node oracle/harness/verify-stream.js "$BUILD/$GOLDEN.four-run.json" \
  "$M4G/$NAME.sha256.json" ||
  fail "[2] the four-port stream does not match the browser oracle"

# --- [3] positive witness ------------------------------------------------------
envelope() { # $1 = extra flags -> the frame-1 envelope on stdout
  # shellcheck disable=SC2086
  $SIM --trace "$BUILD/$GOLDEN.trace.txt" --simdata "$BUILD/simdata.txt" \
    --seed "$SEED" --p1 "$P1" --p2 "$P2" --stage "$STAGE" --frames 2 \
    --dump-frames 1 $1 2>&1 >/dev/null | sed -n 's/^E 1	//p'
}
four_env=$(envelope "--p3 $P3 --p4 $P4")
two_env=$(envelope "")
four_blocks=$(printf '%s' "$four_env" | grep -o '"p[0-3]":{"actionState"' | wc -l | tr -d ' ')
two_blocks=$(printf '%s' "$two_env" | grep -o '"p[0-3]":{"actionState"' | wc -l | tr -d ' ')
[ "$four_blocks" = 4 ] ||
  fail "[3] four-port frame-1 envelope carries $four_blocks player blocks, want 4"
[ "$two_blocks" = 2 ] ||
  fail "[3] two-port frame-1 envelope carries $two_blocks player blocks, want 2"
# ports 2/3 spawn at upstream startingPoint[2]/[3] = (-25,5)/(25,5)
# (main.js:168). The envelope's phys.pos for p2/p3 must be those points
# at frame 1 (ENTRANCE; nothing has moved them yet).
# (the envelope is CHECKSUM.md §3 text, not JSON — T/F and undef tokens —
# so slice each player block by its "pN": key and read the phys.pos that
# follows, rather than regexing across the whole line)
printf '%s' "$four_env" | node -e '
  let s = ""; process.stdin.on("data", (d) => { s += d; });
  process.stdin.on("end", () => {
    const want = { p2: -25, p3: 25 };
    for (const blk of Object.keys(want)) {
      const start = s.indexOf("\"" + blk + "\":{");
      if (start < 0) { console.error("no " + blk + " block in the envelope"); process.exit(1); }
      const nxt = s.indexOf("\"p" + (+blk[1] + 1) + "\":{", start);
      const body = nxt < 0 ? s.slice(start) : s.slice(start, nxt);
      const m = /"pos":\{"x":(-?[0-9.eE+-]+),"y":(-?[0-9.eE+-]+)\}/.exec(body);
      if (!m) { console.error("no phys.pos in the " + blk + " block"); process.exit(1); }
      if (Number(m[1]) !== want[blk]) {
        console.error(blk + " spawns at x=" + m[1] + ", upstream startingPoint says " + want[blk]);
        process.exit(1);
      }
    }
  });
' || fail "[3] ports 2/3 are not at upstream's startingPoint (see above)"
echo "[3] four ports live: frame-1 envelope has p0..p3 (two-port has p0..p1);" \
     "ports 2/3 spawn at upstream startingPoint[2]/[3] x=-25/+25"

# --- [4] teeth: each must bite ALONE -------------------------------------------
bites() { # $1 = label, $2 = extra flags
  local out="$BUILD/$GOLDEN.tooth.out" rc=0
  run4 "$2" "$out" || fail "[4] $1: the sim itself failed; the tooth proves nothing"
  node port/sim/sim/wrap-run.js "$GOLDEN" "$out" \
    "$BUILD/$GOLDEN.tooth-run.json" "$M4G/manifest-4p.json" >/dev/null ||
    fail "[4] $1: wrap-run rejected the output; the tooth proves nothing"
  node oracle/harness/verify-stream.js "$BUILD/$GOLDEN.tooth-run.json" \
    "$M4G/$NAME.sha256.json" >/dev/null 2>&1 && rc=0 || rc=$?
  [ "$rc" != 0 ] || fail "[4] $1 did NOT bite — the check is vacuous"
  echo "[4] $1 bites (verify-stream exit $rc)"
}
bites "T1 two-port wrapper call" ""
bites "T2 ports 2/3 characters swapped" "--p3 $P4 --p4 $P3"
bites "T3 three-port config (port 3 dropped)" "--p3 $P3"

# no-commit guard: build output is never tracked
if git status --porcelain -- "$BUILD" | grep -q .; then
  fail "build output not gitignored"
fi

echo "FOUR PORT OK"
