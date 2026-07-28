#!/usr/bin/env bash
# port/sim/device/verify_m4.sh — THE M4 EXIT GATE (CLAUDE.md §Commands
# "M4 EXIT GATE"; PLAN §4/M4 EXIT verbatim; assembled by fix_plan §M4
# task 14, iter 108). Prints `M4 GATE OK`, exit 0, iff ALL of:
#
#   [0] FREEZE MANIFEST (PROCESS §4 reviewed-pin rule — "hashes prove
#       identity, not approval"): FIRST the manifest's own bytes must
#       equal the in-script MANIFEST_SHA256 anchor (the verify_m3.sh
#       mechanics inherited whole — see the anchor's comment block
#       below), THEN every evidence PRODUCER listed in
#       port/sim/device/m4-freeze-manifest.txt (check scripts, riglib,
#       adbsh, judges, the OPK assets, wrap/trace tools,
#       verify-stream.js, the target-plane verifier, this script
#       itself) re-hashes to its pinned sha256 BEFORE any leg runs. ANY
#       mismatch, grammar violation, missing producer, or extra entry
#       = HARD REFUSAL — prior gate evidence is invalid the moment a
#       producer changes; the gate reruns only after the manifest is
#       re-pinned by a REVIEWED change (update path: manifest header).
#   [0b] REVIEW-CLOSURE STATUS ENFORCEMENT: in AUTHORITATIVE mode (the
#       default) every producer's manifest status must be ∈
#       {reviewed-go, oracle-frozen, grandfathered-m1,
#       grandfathered-m2} BEFORE any leg
#       runs; any arc-in-flight/arc-pending producer = HARD REFUSAL
#       naming the producers. MLFK_M4_DEV=1 bypasses ONLY this status
#       enforcement (byte pins, the anchor, and the grammar stay
#       enforced) for engineering runs — and forces the
#       NON-AUTHORITATIVE outcome below.
#
# AUTHORITATIVE vs DEV (SENTINEL LOCKOUT, inherited from verify_m3.sh
# review-58 H1+H2): the exact sentinel line `M4 GATE OK` (exit 0)
# prints ONLY on a fully-authoritative all-real run: MLFK_M4_DEV unset,
# MLFK_M4_FAKE_LEG_DIR unset, all statuses review-closed, all legs real
# and green. Setting EITHER env var forces the banner
# `M4 GATE (DEV — NON-AUTHORITATIVE)` and a NONZERO exit (3) even when
# everything else passes — AUTHORITATIVE is computed once up front and
# made readonly, and the sentinel echo exists only inside its =1
# branch (structurally incapable of printing from a canned-evidence or
# dev run).
#
#   [1] FULL-GAME TRACE CONFORMANCE ON DEVICE AT 60 FPS WITH AUDIO —
#       every match golden in oracle/goldens/manifest.json (g07/g08
#       driven by the LIVE C ai.c, no AIBRIDGE1) plus every M4 golden
#       in port/goldens-m4/manifest.json (m01/m02 CPU-difficulty,
#       s01/s02 scenario) plus every target golden in
#       port/goldens-m4/manifest-target.json, replayed ON the FunKey-S
#       with live render + the audio callback + music streaming, each
#       stream judged by the UNCHANGED oracle/harness/verify-stream.js
#       against its frozen *.sha256.json (target traces additionally by
#       the frozen target-plane verifier) — exact per-frame equality,
#       FULL length, rngCalls/specVersion pins, and per run:
#       p99 frame < 16.67 ms, underruns == 0, music starves == 0,
#       skips == 0. Engines: port/sim/device/check-device-fullgame.sh
#       (the 12 match/scenario goldens) + port/sim/target/
#       check-device-target.sh (the 2 target goldens, both verifiers).
#   [2] MENU FLOWS — every committed port/foh/flows/ script driven on
#       device through the real input path: frozen structural
#       transition trace + screenshot judges green + the launched
#       match's stream prefix == its frozen golden. Engines:
#       port/foh/check-device-foh.sh (f01-f05) + the SAME
#       check-device-target.sh run as leg 1 (f06-f07) — the committed
#       flow set is exactly f01..f07, and the two engines' pinned flow
#       counts (5 + 2) are asserted to cover it with no gap.
#   [3] OPK — packaged with the SDK container's mksquashfs 4.4 ONLY,
#       launched via the FRONTEND into the FOH, boot marker + in-app
#       screenshot pulled and judged. Engine:
#       port/gfx/check-device-opk.sh in its FOH mode.
#
# SUB-CHECK REUSE DISCIPLINE (PROCESS §3): the legs INVOKE the
# arc-hardened check scripts — no device logic is reimplemented here.
# Each engine's PASS verdict is parsed by an EXACT ANCHORED FULL-LINE
# grammar with an exactly-one-match posture (the PROCESS §3
# whitelist-grammar rule at the aggregator); an engine that exits 0
# without exactly its expected verdict line — or with a
# resembling-but-different line — is CORRUPTION, never a pass. Engine
# output goes to port/sim/calib/build/device/verify-m4/leg-*.log
# (gitignored).
#
# The gate does NOT print the LOOP STOP sentinel — a pass is followed
# by the driver's phase-advance duty
# (`LOOP STOP: m4-complete — awaiting Chase acceptance playthrough`).
#
# Env: FUNKEY_ADB_ID (device id), MLFK_FORCE_ARM=1 (forwarded to the
# engines' shared arm build), MLFK_M4_DEV=1 (ENGINEERING ONLY: bypass
# status enforcement — output becomes NON-AUTHORITATIVE, exit nonzero),
# MLFK_M4_FAKE_LEG_DIR (NEGATIVE TESTING ONLY: replay canned engine
# logs/rcs instead of running the real engines — proves the verdict
# parsers' corruption teeth without device time; the freeze manifest +
# anchor are verified regardless, and the run is NON-AUTHORITATIVE +
# nonzero UNCONDITIONALLY).
set -euo pipefail
cd "$(dirname "$0")/../../.."

DEVB=port/sim/calib/build/device
VDIR=$DEVB/verify-m4
MANIFEST=port/sim/device/m4-freeze-manifest.txt
SELF=port/sim/device/verify_m4.sh
mkdir -p "$VDIR"

# MANIFEST INTEGRITY ANCHOR (verify_m3.sh iter-60 mechanics, inherited
# verbatim): the manifest's OWN bytes are pinned here, so a mechanical
# "edit a producer + re-pin its row" can never mint a trusted manifest
# without ALSO changing this reviewed literal.
# UPDATE DISCIPLINE (binding; documented in the manifest header too):
# ANY edit to m4-freeze-manifest.txt requires updating this literal IN
# THE SAME COMMIT — recompute with
#   shasum -a 256 port/sim/device/m4-freeze-manifest.txt
# SELF-REFERENCE NORMALIZATION: the manifest's verify_m4.sh row pins
# sha256 of THIS file's bytes EXCLUDING the single ^MANIFEST_SHA256=
# line (a full-byte self-row plus this anchor would be a two-unknown
# hash fixed point with no solution); the excluded line is protected
# by the anchor equality itself — a wrong literal IS a refusal.
MANIFEST_SHA256=c474752f33d22a2fd8250d052133bbf1b9930bfcb9107ee280ef6bc8b6581a2e

# AUTHORITATIVE — computed ONCE, then readonly (the sentinel lockout).
# Any dev/canned-evidence signal zeroes it; the `M4 GATE OK` sentinel
# exists only inside its =1 branch at the bottom.
AUTHORITATIVE=1
AUTH_REASONS=""
if [ -n "${MLFK_M4_FAKE_LEG_DIR:-}" ]; then
  AUTHORITATIVE=0
  AUTH_REASONS="$AUTH_REASONS canned-leg-evidence(MLFK_M4_FAKE_LEG_DIR);"
fi
if [ "${MLFK_M4_DEV:-0}" = 1 ]; then
  AUTHORITATIVE=0
  AUTH_REASONS="$AUTH_REASONS status-enforcement-bypassed(MLFK_M4_DEV=1);"
fi
readonly AUTHORITATIVE AUTH_REASONS

# rig_host_sha256 (strict full-line shasum grammar) comes from riglib;
# sourcing defines helpers only — this script takes NO rig lock and
# runs NO device command itself (the engines own the lock, serially).
source port/sim/device/adbsh.sh
source port/sim/device/riglib.sh

# LOG-SENTINEL RELAY CONTRACT (verify_m3.sh iter-62 review-60
# H2-residual, inherited): every line this gate RELAYS from sub-content
# — tail'd engine logs, canned-rc bytes, manifest-derived status lists
# — passes through relay_lines, which prints it with the visible
# '  | ' prefix. No relayed byte sequence can therefore ever occupy
# column 0 of the gate's combined output, so a line-anchored scan for
# the sentinel can never match relayed content: the genuine
# `M4 GATE OK` echo at the bottom of this script is the ONLY possible
# unprefixed line-anchored occurrence. Any NEW relay site MUST route
# through relay_lines.
relay_lines() { sed 's/^/  | /'; }

# The pinned producer set: the manifest must list EXACTLY these paths
# (a truncated manifest can never silently narrow gate coverage).
#
# FROZEN REFERENCE DATA IS A PRODUCER (review-117-delta-1 [H1], iter
# 118): the set below covers not only the code that GENERATES evidence
# but the frozen data the judges compare that evidence AGAINST. Before
# this extension, 23 such artifacts sat outside the universe:
#   port/gfx/{gfxdata,vfxdata,vfxglyphs}-frozen.txt — the three frozen
#     graphics planes, previously only EXISTENCE-checked
#     (check-device-fullgame.sh:973 is `[ -f "$f" ] || fail`, no hash);
#   port/foh/flows/*.{flow,expect,bstate.expect} — all 20 files: the
#     flow SCRIPTS leg 2 drives and the frozen transition/button-state
#     EXPECTATIONS leg 2 judges the resulting traces against.
# A hash pin on the engines alone does not close this: an edit that
# changes a flow's expectation AND the trace it is compared to moves
# both sides of the comparison together, so leg 2 stays green while the
# thing it proves silently shrinks. That is common-mode drift, and only
# pinning the reference itself detects it. These rows are DATA — they
# have no logic to review — so their arc is a provenance/freeze-
# integrity review, and their statuses cite it like any other row.
#
# LEG 1's OWN GROUND TRUTH (review-118-delta-3-opus [M4], iter 118):
# the same argument applies with more force to oracle/goldens/ —
# manifest.json DECIDES which goldens leg 1 runs and with what params,
# and the 8 *.sha256.json ARE the frozen per-frame streams leg 1's
# entire conformance claim is judged against. The engines pin the
# golden ID SET and verify-stream.js checks each file's internal
# streamSha256 seal, but both are recomputable by whoever edits a
# golden — exactly the common-mode drift a reviewed pin exists to
# catch. All 17 (manifest + 8 streams + 8 traces) are HARD RULE 3
# surfaces, untouched since 38d6733 (iter 8) and proven by the M0 exit
# gate, so they carry `oracle-frozen` — same status and cite shape as
# the existing oracle/harness rows.
REQUIRED_PRODUCERS="port/sim/device/adbsh.sh
port/sim/device/riglib.sh
port/sim/device/percentiles.js
port/sim/device/check-device-fullgame.sh
port/sim/target/check-device-target.sh
port/goldens-m4/verify-target-stream.js
port/goldens-m4/wrap-target.js
port/goldens-m4/validate-target-manifest.js
port/goldens-m4/json-dup-key-scan.js
pipeline/lib/tables-anim-xref.js
oracle/CHECKSUM.md
pipeline/lib/animbin.js
pipeline/lib/tables-schema.js
port/goldens-m4/manifest.json
port/goldens-m4/manifest-target.json
port/goldens-m4/m01-falcon-marth-d1-ystory.sha256.json
port/goldens-m4/m02-falcon-fox-d9-dreamland.sha256.json
port/goldens-m4/s01-marth-fox-stops-battlefield.sha256.json
port/goldens-m4/s02-marth-fox-guardon-break-battlefield.sha256.json
port/goldens-m4/t01-fox-lasers-tstage1.sha256.json
port/goldens-m4/t01-fox-lasers-tstage1.target.sha256.json
port/goldens-m4/t02-falcon-melee-tstage2.sha256.json
port/goldens-m4/t02-falcon-melee-tstage2.target.sha256.json
port/foh/check-device-foh.sh
port/foh/judge-foh-trace.js
port/foh/normalize-foh-trace.js
port/foh/flow-to-fkscript.js
port/foh/keymap-frozen.txt
port/foh/flows/f01-vs-g01.flow
port/foh/flows/f01-vs-g01.expect
port/foh/flows/f01-vs-g01.bstate.expect
port/foh/flows/f02-cpu-m01.flow
port/foh/flows/f02-cpu-m01.expect
port/foh/flows/f02-cpu-m01.bstate.expect
port/foh/flows/f03-options.flow
port/foh/flows/f03-options.expect
port/foh/flows/f03-options.bstate.expect
port/foh/flows/f04-nav.flow
port/foh/flows/f04-nav.expect
port/foh/flows/f05-vs-g03.flow
port/foh/flows/f05-vs-g03.expect
port/foh/flows/f05-vs-g03.bstate.expect
port/foh/flows/f06-target-t01.flow
port/foh/flows/f06-target-t01.expect
port/foh/flows/f06-target-t01.bstate.expect
port/foh/flows/f07-target-t02.flow
port/foh/flows/f07-target-t02.expect
port/foh/flows/f07-target-t02.bstate.expect
port/gfx/judge-shot.js
port/gfx/judge-render-timing.js
port/gfx/judge-audio-summary.js
port/gfx/pack-snd.js
port/gfx/gfxdata-frozen.txt
port/gfx/vfxdata-frozen.txt
port/gfx/vfxglyphs-frozen.txt
oracle/goldens/manifest.json
oracle/goldens/g01-fox-marth-battlefield.sha256.json
oracle/goldens/g02-falco-puff-ystory.sha256.json
oracle/goldens/g03-falcon-fox-pstadium.sha256.json
oracle/goldens/g04-puff-falcon-dreamland.sha256.json
oracle/goldens/g05-marth-falco-fdest.sha256.json
oracle/goldens/g06-falcon-marth-fountain.sha256.json
oracle/goldens/g07-falco-falcon-battlefield.sha256.json
oracle/goldens/g08-fox-puff-fdest.sha256.json
oracle/goldens/g01-fox-marth-battlefield.trace.json
oracle/goldens/g02-falco-puff-ystory.trace.json
oracle/goldens/g03-falcon-fox-pstadium.trace.json
oracle/goldens/g04-puff-falcon-dreamland.trace.json
oracle/goldens/g05-marth-falco-fdest.trace.json
oracle/goldens/g06-falcon-marth-fountain.trace.json
oracle/goldens/g07-falco-falcon-battlefield.trace.json
oracle/goldens/g08-fox-puff-fdest.trace.json
port/gfx/check-device-opk.sh
port/gfx/opk/mlfk-foh.sh
port/gfx/opk/meleelight-foh.funkey-s.desktop
port/gfx/opk/icon32.png
port/tools/fk_input.c
port/sim/sim/wrap-run.js
port/sim/sim/trace-to-txt.js
port/sim/calib/dump-sim-data.js
oracle/harness/verify-stream.js
oracle/harness/streamlib.js
pipeline/lib/manifest.js
pipeline/lib/verify-artifacts.js
pipeline/lib/check-expected.js
pipeline/expected.json
pipeline/expected-assets.json
pipeline/lib/check-assets-expected.js
port/sim/device/verify_m4.sh"

# gate_norm_sha256 — sha256 of THIS script's bytes EXCLUDING the single
# ^MANIFEST_SHA256= line (the manifest's verify_m4.sh row pins this
# normalized digest — see the anchor comment block). Strict whole-line
# shasum stdin grammar (`<64hex>  -`), mirror of rig_host_sha256.
gate_norm_sha256() {
  local out sum
  out="$(grep -v '^MANIFEST_SHA256=' "$SELF" | shasum -a 256)" || {
    echo "M4 GATE REFUSED: could not compute the normalized self-hash" >&2
    return 1
  }
  sum="${out%%  *}"
  if [ "${#sum}" -ne 64 ] || [ "$out" != "$sum  -" ]; then
    echo "M4 GATE REFUSED: normalized self-hash output malformed ('$out')" >&2
    return 1
  fi
  case "$sum" in
    *[!0-9a-f]*)
      echo "M4 GATE REFUSED: normalized self-hash digest not lowercase hex ('$sum')" >&2
      return 1
      ;;
  esac
  printf '%s\n' "$sum"
}

echo "== [0] freeze-manifest verification (PROCESS §4 — refusal before any leg) =="
if [ ! -f "$MANIFEST" ]; then
  echo "M4 GATE REFUSED: freeze manifest $MANIFEST missing" >&2
  exit 1
fi
# anchor FIRST: the manifest's own bytes must match the reviewed
# in-script literal before a single row is trusted.
if [ "$(grep -c '^MANIFEST_SHA256=' "$SELF")" != 1 ]; then
  echo "M4 GATE REFUSED: expected exactly one ^MANIFEST_SHA256= line in $SELF (anchor integrity)" >&2
  exit 1
fi
msha_cur="$(rig_host_sha256 "$MANIFEST")" || {
  echo "M4 GATE REFUSED: could not hash the freeze manifest $MANIFEST" >&2
  exit 1
}
if [ "$msha_cur" != "$MANIFEST_SHA256" ]; then
  echo "M4 GATE REFUSED: $MANIFEST bytes do not match the in-script anchor" >&2
  echo "  anchor:  $MANIFEST_SHA256" >&2
  echo "  current: $msha_cur" >&2
  echo "  A manifest edit is a REVIEWED change: update MANIFEST_SHA256 in" >&2
  echo "  $SELF in the SAME commit (update discipline — both file headers)." >&2
  exit 1
fi
# grammar pass: every non-comment, non-empty line must be EXACTLY
#   <64-lowercase-hex><SP><path><SP><status><SP><cite>
# status in the closed token set; anything that merely resembles a
# record is corruption -> refusal.
n_data=0
# `|| [ -n "$mline" ]` (review-108-1 H4): a final record with NO trailing
# newline is still a record. Without it, `read` returns nonzero on the
# last unterminated line and the loop DROPS it — an anchored-valid
# manifest could then carry an extra malformed row that neither the
# grammar pass nor the per-producer awk queries ever see, leaving
# n_data == n_req so the extra-entry check below also passes.
while IFS= read -r mline || [ -n "$mline" ]; do
  case "$mline" in
    '#'*|'') continue ;;
  esac
  if ! [[ "$mline" =~ ^[0-9a-f]{64}\ [A-Za-z0-9._/-]+\ (reviewed-go|oracle-frozen|grandfathered-m1|grandfathered-m2|arc-in-flight|arc-pending)\ [A-Za-z0-9._/:+-]+$ ]]; then
    echo "M4 GATE REFUSED: manifest line fails the anchored grammar: '$mline'" >&2
    exit 1
  fi
  n_data=$((n_data + 1))
done < "$MANIFEST"
n_req=0
n_unresolved=0
UNRESOLVED=""
while IFS= read -r prod; do
  [ -n "$prod" ] || continue
  n_req=$((n_req + 1))
  # exactly ONE record per required producer (duplicates = corruption)
  mcount="$(awk -v p="$prod" 'substr($0,1,1) != "#" && $2 == p' "$MANIFEST" | grep -c '' )" || true
  if [ "$mcount" != 1 ]; then
    echo "M4 GATE REFUSED: manifest has $mcount records for producer $prod (want exactly 1)" >&2
    exit 1
  fi
  mrec="$(awk -v p="$prod" 'substr($0,1,1) != "#" && $2 == p' "$MANIFEST")"
  msha="${mrec%% *}"
  mstatus="$(printf '%s\n' "$mrec" | awk '{print $3}')"
  if [ ! -f "$prod" ]; then
    echo "M4 GATE REFUSED: pinned producer $prod is missing from the tree" >&2
    exit 1
  fi
  if [ "$prod" = "$SELF" ]; then
    # normalized self-reference (anchor comment block): this row pins
    # the gate's bytes EXCLUDING the ^MANIFEST_SHA256= line.
    cursha="$(gate_norm_sha256)" || exit 1
  else
    cursha="$(rig_host_sha256 "$prod")" || {
      echo "M4 GATE REFUSED: could not hash producer $prod" >&2
      exit 1
    }
  fi
  if [ "$cursha" != "$msha" ]; then
    echo "M4 GATE REFUSED: producer $prod bytes do not match its pinned sha" >&2
    echo "  pinned:  $msha ($mstatus)" >&2
    echo "  current: $cursha" >&2
    echo "  A producer change invalidates prior gate evidence — land the change" >&2
    echo "  through its review arc and re-pin the manifest (reviewed change only)." >&2
    exit 1
  fi
  # status collection for [0b]
  case "$mstatus" in
    # grandfathered-m1 (iter-118, review-117-delta-1 [H2] + r9b [L]
    # dissolved by TRUTHFUL VOCABULARY): the pre-PROCESS grandfather
    # token was M2-only, so pinning an M1-era checker forced a choice
    # between a false era label and a false refusal. The era is now
    # named accurately instead — an M1-era surface proven by the M1
    # EXIT GATE (PIPELINE OK) rather than the M2 exit corpus. Same
    # closure semantics, same enforcement strength; only the cite's
    # era claim changes, and it changes to the true one.
    reviewed-go|oracle-frozen|grandfathered-m1|grandfathered-m2) : ;;
    *)
      n_unresolved=$((n_unresolved + 1))
      UNRESOLVED="$UNRESOLVED  $prod ($mstatus)"$'\n'
      ;;
  esac
done <<< "$REQUIRED_PRODUCERS"
if [ "$n_data" != "$n_req" ]; then
  echo "M4 GATE REFUSED: manifest carries $n_data records, pinned producer set has $n_req — extra/unknown entries are corruption" >&2
  exit 1
fi
echo "   freeze manifest OK: $n_req producers, all bytes match their pins (manifest anchor verified)"

echo "== [0b] review-closure status enforcement (refusal before any leg) =="
if [ "$n_unresolved" != 0 ]; then
  if [ "${MLFK_M4_DEV:-0}" = 1 ]; then
    echo "WARN: MLFK_M4_DEV=1 — status enforcement BYPASSED for $n_unresolved producer(s) (engineering run; outcome will be NON-AUTHORITATIVE + nonzero):" >&2
    printf '%s' "$UNRESOLVED" | relay_lines >&2
  else
    echo "M4 GATE REFUSED: $n_unresolved producer(s) lack review closure — status not in {reviewed-go, oracle-frozen, grandfathered-m1, grandfathered-m2}:" >&2
    printf '%s' "$UNRESOLVED" | relay_lines >&2
    echo "  Review closure (PROCESS §3 arc -> §4 reviewed pin) is required BEFORE any leg" >&2
    echo "  runs in AUTHORITATIVE mode. Close the arcs and flip the manifest statuses" >&2
    echo "  (driver duty at phase-advance), or run engineering-only with MLFK_M4_DEV=1" >&2
    echo "  (NON-AUTHORITATIVE banner, nonzero exit — the sentinel cannot print)." >&2
    exit 1
  fi
else
  echo "   all $n_req producer statuses review-closed"
fi

# --- engine machinery --------------------------------------------------------
run_engine() { # <name> <script> [KEY=VAL ...] — runs (or replays) an
  # engine, dies on rc != 0. Any KEY=VAL arguments are passed as the
  # child's environment ONLY (never exported into this shell), so an
  # engine mode selector cannot leak across legs.
  local name="$1" script="$2" rc=0 legf
  shift 2
  legf="$VDIR/leg-$name.log"
  rm -f "$legf"
  if [ -n "${MLFK_M4_FAKE_LEG_DIR:-}" ]; then
    echo "WARN: MLFK_M4_FAKE_LEG_DIR set — replaying canned '$name' engine evidence (NEGATIVE TESTING ONLY)" >&2
    cp "$MLFK_M4_FAKE_LEG_DIR/leg-$name.log" "$legf" 2>/dev/null || {
      echo "M4 GATE FAIL: canned engine log for '$name' missing" >&2
      exit 1
    }
    rc="$(cat "$MLFK_M4_FAKE_LEG_DIR/leg-$name.rc" 2>/dev/null)" || rc=missing
    if ! [[ "$rc" =~ ^[0-9]{1,3}$ ]]; then
      # relayed sub-content: the rc bytes may be arbitrary/multi-line —
      # print them ONLY through the '  | ' relay (log-sentinel contract)
      echo "M4 GATE FAIL: canned engine rc for '$name' malformed (bytes relayed below):" >&2
      printf '%s\n' "$rc" | relay_lines >&2
      exit 1
    fi
  else
    # `env KEY=VAL ... bash script` — the mode selector reaches the
    # child only; with no KEY=VAL args this is a plain `bash script`.
    env "$@" bash "$script" > "$legf" 2>&1 || rc=$?
  fi
  if [ "$rc" != 0 ]; then
    # relayed sub-content: engine-log bytes go out ONLY through the
    # '  | ' relay (log-sentinel contract above)
    tail -30 "$legf" | relay_lines >&2
    echo "M4 GATE FAIL: engine '$name' exited $rc (full log: $legf)" >&2
    exit 1
  fi
}

# expect_grammar <logf> <full-line-ERE> <resemblance-ERE> <label>
# (the PROCESS §3 whitelist-grammar rule at the aggregator): the FULL
# verdict line must match the anchored grammar exactly once AND no
# other line may RESEMBLE it. The resemblance discriminators are the
# anchored line-start prefixes MEASURED from the real passing corpus
# (each producer emits its prefix exactly once, as the full verdict),
# so a genuine log can never false-reject while a truncated/torn
# duplicate is corruption death. Echoes the matched line on stdout.
# EVERY grep BELOW IS `-a` (review-118-delta-3-opus [H1]): this host's
# grep treats a file containing ANY NUL byte as binary, and then the
# non-counting form returns `Binary file X matches` (or nothing) INSTEAD
# of the matched line. The -c forms still count, so the two count guards
# below stayed green while `line` became garbage — which silently
# disabled the proper-prefix tear loop (nothing can be a prefix of
# `Binary file ... matches`). Legs 1-3 then died late in p99 extraction,
# but the OPK leg has no p99 call, so a torn OPK verdict reached
# `M4 GATE OK`. A leg log picks up NULs from relayed device bytes, so
# this needs no adversary. -a forces the text path unconditionally.
expect_grammar() {
  local logf="$1" full="$2" resem="$3" label="$4" c r line crc rrc nseen nfile nrc
  local ctl_raw ctl_clean ctl_rc tk
  # TORN FINAL WRITE (review-117-delta-1 [M1]): `grep` treats EOF as a
  # line terminator, so a log whose LAST line was written without its
  # newline — the exact byte signature of a truncated/killed write —
  # still matches the full grammar and passes every guard below. A
  # genuine engine log always ends in LF (every producer's verdict is
  # the final `echo`, and the canned-replay path is a byte `cp`), so a
  # missing final LF is CORRUPT evidence, never ignorable. Asserted
  # BEFORE any grep so no torn log is ever parsed at all.
  if [ ! -s "$logf" ]; then
    echo "M4 GATE FAIL: $logf is empty — no $label evidence to judge" >&2
    exit 1
  fi
  # NO CONTROL CHARACTERS except TAB and LF (review-118-delta-3-codex
  # [H] then review-118-delta-4-codex [H] — the SECOND generalisation).
  # Every guard below reasons about LINES, and a control byte breaks line
  # reasoning in ways no line-level guard can see. MEASURED, all three
  # ACCEPTED before this check existed:
  #   \0OPK FOH LAUNCH  — NUL-prefixed torn duplicate
  #   \rOPK FOH LAUNCH  — CR-prefixed  (SOH and ESC behave identically)
  # A junk-PREFIXED tear defeats both guards at once: it does not match
  # the ^-anchored resemblance grammar (the line no longer starts with
  # the verdict's first character), and it is not a proper PREFIX of the
  # verdict either (the verdict starts with 'O'), so the tear loop skips
  # it. NUL additionally desyncs grep (binary mode) and `$( )` vs
  # `read -r`. The first fix here refused NUL only — that closed the
  # instance, not the CLASS, which is exactly what round 4 demonstrated.
  # A control byte in a TEXT evidence log is corruption by definition, so
  # the whole class is refused once, up front, before anything parses it.
  # TAB and LF are the only control bytes a producer legitimately emits.
  # MEASURED SAFE: every archived genuine leg log contains ZERO control
  # bytes other than LF/TAB (0 CR, 0 ESC, 0 other), so this cannot
  # false-reject. Non-ASCII/UTF-8 is deliberately still allowed — the
  # engines' own messages contain em-dashes.
  # STATUS-HONEST (review-119-delta-5b [H2]): the previous shape compared two
  # command substitutions INSIDE `[ ]`, where errexit does not apply and both
  # exit statuses are discarded. A failed read left BOTH sides empty, so
  # `"" != ""` was false and the guard PASSED VACUOUSLY on unreadable
  # evidence. Measured into checked variables instead; an evidence log that
  # cannot be measured is CORRUPT, never assumed clean.
  ctl_raw="$(wc -c < "$logf")" && ctl_rc=0 || ctl_rc=$?
  if [ "$ctl_rc" != 0 ]; then
    echo "M4 GATE FAIL: could not size $logf ($label) — an evidence read that fails mid-judgement is CORRUPT evidence, never ignorable" >&2
    exit 1
  fi
  ctl_clean="$(LC_ALL=C tr -d '\000-\010\013-\037\177' < "$logf" | wc -c)" && ctl_rc=0 || ctl_rc=$?
  if [ "$ctl_rc" != 0 ]; then
    echo "M4 GATE FAIL: could not scan $logf for control bytes ($label) — an evidence read that fails mid-judgement is CORRUPT evidence, never ignorable" >&2
    exit 1
  fi
  ctl_raw="${ctl_raw// /}"; ctl_clean="${ctl_clean// /}"
  if [ -z "$ctl_raw" ] || [ -z "$ctl_clean" ]; then
    echo "M4 GATE FAIL: byte-class measurement of $logf produced no count ($label) — CORRUPT evidence" >&2
    exit 1
  fi
  if [ "$ctl_raw" != "$ctl_clean" ]; then
    echo "M4 GATE FAIL: $logf contains control byte(s) other than TAB/LF — a text evidence log with embedded control bytes is CORRUPT ($label); such bytes defeat line-oriented verdict parsing (junk-prefixed torn writes)" >&2
    exit 1
  fi
  # BYTE-EXACT, never via `$(...)` emptiness: bash's command substitution
  # DROPS NUL bytes, so `[ -n "$(tail -c 1 f)" ]` passes a log whose final
  # byte is NUL — the exact padding a truncated write leaves behind. (zsh
  # keeps the NUL, so the naive test only looks correct interactively;
  # measured both ways.) Compare the final byte's hex to 0a instead.
  if [ "$(tail -c 1 "$logf" | od -An -tx1 | tr -d ' \n')" != 0a ]; then
    echo "M4 GATE FAIL: $logf does not end with a newline — its final line is a torn/truncated write, which is CORRUPT $label evidence, never ignorable" >&2
    exit 1
  fi
  # GREP STATUS IS EVIDENCE (review-118-delta-4-codex [H], residual): the
  # old `|| true` laundered EVERY nonzero status, not just the benign one.
  # grep's contract is rc 0 = matched, rc 1 = no match (legitimate here —
  # the count check below reports it), rc >= 2 = a REAL error (unreadable
  # file, I/O failure, regex blowup) whose stdout is meaningless. Under
  # `|| true` an rc-2 read left `c` holding whatever partial bytes grep had
  # flushed, and judgement continued on it. rc >= 2 is now refused outright:
  # an evidence read that fails mid-judgement is CORRUPT evidence.
  c="$(grep -acE "$full" "$logf")" && crc=0 || crc=$?
  if [ "$crc" -gt 1 ]; then
    echo "M4 GATE FAIL: grep exited $crc while counting $label verdict lines in $logf — an evidence read that fails mid-judgement is CORRUPT evidence, never ignorable (rc 0/1 = matched/not-matched; 2+ = real error)" >&2
    exit 1
  fi
  if [ "$c" != 1 ]; then
    echo "M4 GATE FAIL: expected exactly 1 $label verdict line matching the pinned grammar in $logf, found $c" >&2
    echo "  grammar: $full" >&2
    echo "  — an engine that exits 0 without its exact verdict is CORRUPT evidence" >&2
    exit 1
  fi
  r="$(grep -acE "$resem" "$logf")" && rrc=0 || rrc=$?
  if [ "$rrc" -gt 1 ]; then
    echo "M4 GATE FAIL: grep exited $rrc while counting $label verdict-resemblance lines in $logf — an evidence read that fails mid-judgement is CORRUPT evidence, never ignorable" >&2
    exit 1
  fi
  if [ "$r" != 1 ]; then
    echo "M4 GATE FAIL: $logf carries $r lines matching the verdict-resemblance grammar '$resem' but exactly 1 full-grammar verdict — a verdict-RESEMBLING malformed line (truncated/torn duplicate) is CORRUPTION, never ignorable" >&2
    exit 1
  fi
  line="$(grep -aE "$full" "$logf")" || {
    echo "M4 GATE FAIL: could not re-read the $label verdict line from $logf after counting it — an evidence read that fails mid-judgement is CORRUPT evidence, never ignorable" >&2
    exit 1
  }
  # BOTH round-4 reviewers, independently: the extracting grep runs in a
  # command substitution where bash disables errexit, and an empty `line`
  # silently DISABLES the proper-prefix loop below (nothing is a prefix of
  # ""). Legs 1-3 would still die in p99_under_budget, but the OPK leg has
  # no p99 call, so an empty line there reached the sentinel.
  if [ -z "$line" ]; then
    echo "M4 GATE FAIL: the $label verdict line re-read as EMPTY from $logf despite a count of 1 — CORRUPT evidence" >&2
    exit 1
  fi
  # TORN-DUPLICATE CLOSURE BY CONSTRUCTION (review-108-2 H3): a prefix
  # ANCHOR can only ever catch tears longer than the anchor itself —
  # `FULLGAME CONF` slips under `^FULLGAME CONFORMS`, and lengthening or
  # shortening the anchor just moves the hole. So the tear class is
  # closed structurally instead: ANY other non-empty line that is a
  # PROPER PREFIX of the genuine verdict is a truncated/torn write of
  # that verdict, at EVERY tear length. The resemblance grammar above is
  # retained because it catches the complementary case this cannot —
  # corruption that diverges MID-line rather than truncating.
  local l
  nseen=0
  while IFS= read -r l || [ -n "$l" ]; do
    nseen=$((nseen + 1))
    # strip a trailing CR before comparing (review-108-3 H-c): a torn
    # CRLF write leaves `F\r`, which is not a proper prefix of the
    # verdict as raw bytes and would slip through both guards.
    l="${l%$'\r'}"
    [ -n "$l" ] || continue
    [ "$l" != "$line" ] || continue
    # CLASS-CLOSING TEAR DETECTOR (review-119-delta-6b [H1]). The previous
    # form asked "is this line a proper PREFIX of the verdict", which only
    # sees tears that begin at byte zero; round 6 defeated it with
    # `trailing OPK FOH LAUNC` — junk prepended AND truncated below the
    # resemblance stem, so the unanchored stem count missed it too. Rounds
    # 3/4/5/6 each found a new shape because the question was wrong, not
    # because a byte was missing from a list.
    # The right question is what a TRUNCATED WRITE actually looks like: the
    # line ENDS at the truncation point, so it ends with some prefix of the
    # verdict, whatever junk precedes it and whatever length it reached.
    # So: refuse if any other line ENDS WITH a non-empty prefix of the
    # verdict. This subsumes every shape found so far — plain proper prefix,
    # control-prefixed, printable-prefixed, whitespace-prefixed, and
    # short-truncated — at every tear length, with no byte enumeration.
    # MEASURED, zero false-reject and full margin: over all 18 archived
    # genuine engine leg logs, the longest suffix of any non-verdict line
    # that is a prefix of that log's verdict is ZERO characters. The
    # threshold is therefore 1 (maximum strictness) rather than a tuned
    # constant, and a false reject costs a re-run while a false pass ships
    # a lie — so this fails CLOSED on purpose.
    tk=${#l}
    [ "$tk" -le "${#line}" ] || tk=${#line}
    while [ "$tk" -ge 1 ]; do
      if [ "${l:${#l}-tk}" = "${line:0:tk}" ]; then
        echo "M4 GATE FAIL: $logf carries a line ENDING WITH a $tk-character prefix of the $label verdict — a truncated/torn duplicate write is CORRUPTION, never ignorable" >&2
        echo "  torn:   '$l'" >&2
        echo "  verdict:'$line'" >&2
        exit 1
      fi
      tk=$((tk - 1))
    done
  done < "$logf"
  # TERMINAL STATUS OF THE TEAR LOOP (review-118-delta-4-codex [H],
  # residual): a `read` that fails PART-WAY through the file ends the loop
  # silently and successfully, so every line after the failure point — a
  # torn duplicate among them — is never examined at all. The loop had no
  # completion evidence of any kind. It does now: the number of iterations
  # must equal the number of lines in the file. (The final-LF guard above
  # already makes `grep -c ""` and the read loop agree on what a line is.)
  nfile="$(grep -ac "" "$logf")" && nrc=0 || nrc=$?
  if [ "$nrc" -gt 1 ]; then
    echo "M4 GATE FAIL: grep exited $nrc while re-counting the lines of $logf — an evidence read that fails mid-judgement is CORRUPT evidence, never ignorable" >&2
    exit 1
  fi
  if [ "$nseen" != "$nfile" ]; then
    echo "M4 GATE FAIL: the torn-duplicate scan of $logf consumed $nseen lines but the file has $nfile — the read terminated early, so part of the $label evidence was NEVER judged; an incomplete evidence read is CORRUPT evidence, never ignorable" >&2
    exit 1
  fi
  printf '%s\n' "$line"
}

# p99_under_budget <verdict-line> <label> — the frame budget is
# re-asserted AT THE GATE (defense in depth; the engines assert it too).
# The grammars pin p99 to EXACTLY three decimals, so the comparison is
# exact integer milli-microseconds against 16.670 ms — no float
# arithmetic anywhere in the gate (the check-device-conform.sh rule).
P99_BUDGET_MILLI=16670   # 16.67 ms expressed in thousandths of a ms
p99_under_budget() {
  local line="$1" label="$2" ip fp val
  if ! [[ "$line" =~ p99=([0-9]{1,3})\.([0-9]{3})ms ]]; then
    echo "M4 GATE FAIL: could not extract a 3-decimal p99 from the $label verdict" >&2
    exit 1
  fi
  ip="${BASH_REMATCH[1]}"
  fp="${BASH_REMATCH[2]}"
  # strip leading zeros without octal interpretation
  val=$((10#$ip * 1000 + 10#$fp))
  if [ "$val" -ge "$P99_BUDGET_MILLI" ]; then
    echo "M4 GATE FAIL: $label p99 ${ip}.${fp} ms is not under the 16.67 ms frame budget" >&2
    exit 1
  fi
  printf '%s.%s\n' "$ip" "$fp"
}

# --- MEASURED verdict grammars ----------------------------------------------
# Every grammar below is anchored ^...$, full-line, with the counters
# that carry the gate's decision pinned as LITERAL zeros (skips=0
# underruns=0 starves=0) so a nonzero counter cannot match at all.
#
# PROVENANCE (PROCESS §3 step 1 — measure the producer's exact grammar
# EMPIRICALLY, never from docs or memory): TARGET_RE and FOH_RE were
# measured from the producers' CURRENT emitter lines
# (check-device-target.sh:1240, check-device-foh.sh:1801) and confirmed
# against real archived passing output. NOTE the measured trap
# (review-108-1 H1): older archived logs and AGENT-LOG excerpts carry
# SUPERSEDED forms (`opk=1` with no `fbwit=`; no `sfxpin=`/`music=`) —
# grammars measured from those would false-reject every real run today.
# The freeze manifest is what keeps this sound: a producer that changes
# its emitter changes its bytes, so [0] REFUSES before any grammar is
# applied — the pin and the grammar can never silently desync.
#
# FLOW-ID SEQUENCES ARE PINNED LITERALLY (review-108-1 H2): the earlier
# `( f0[67]-...=N){2}` / `{5}` repetition groups did NOT enforce flow
# IDENTITY — a verdict repeating f06 twice (and silently omitting f07),
# or f01 five times, matched and PASSED. The producers' flow arrays are
# fixed literal sequences in a fixed order (check-device-target.sh:320,
# check-device-foh.sh:374), so the exact ids are pinned in that order
# and a dropped/duplicated flow now dies here.
#
# CONSTANT-VALUED FIELDS ARE PINNED AS LITERALS, NOT WILDCARDS
# (review-117-delta-1 [M2]): a numeric wildcard on a field the producer
# emits as a CONSTANT accepts output the producer is structurally
# incapable of producing — i.e. it accepts only corruption, at the cost
# of the whole field's evidentiary value. Two were measured against the
# live emitters and are now literal:
#   shots=13   — check-device-foh.sh:1801 hardcodes `shots=13` in the
#                verdict string itself (not a variable).
#   1 transitions — check-device-opk.sh:1279 likewise emits the count as
#                a string literal in the OPK-FOH verdict.
#   starts f06-target-t01=15 f07-target-t02=31, sfxpin=15/31 —
#                check-device-target.sh:148 freezes SFX_STARTS_PIN=(15
#                31) and :1031 hard-fails unless the measured starts
#                EQUAL that pin, before :1240 emits them; so all three
#                fields are producer constants too. (FOH's `starts` are
#                deliberately NOT pinned: check-device-foh.sh:1277
#                asserts them against a HOST-TWIN MEASUREMENT, not a
#                frozen literal, so they legitimately vary.)
#                These four were missed by the first pass, which claimed
#                a complete sweep it had not performed — found by
#                review-118-delta-3-opus [M3].
#   fbwit / teeth — SECOND sweep (both round-3 reviewers, independently):
#                fbwit is a pure sum over each engine's FIXED literal
#                FLOW_SHOTS array (target 2+2=4, FOH 13+2=15) and teeth
#                is a count of UNCONDITIONAL column-0 `teeth=$((teeth+1))`
#                sites (target 6, FOH 15 — none inside a loop or
#                conditional). Three-way agreed: two independent static
#                counts plus the archived passing output (fbwit=4/teeth=6
#                and fbwit=15/teeth=15). FULLGAME_RE already pinned
#                teeth=21 literally, so this makes the family uniform.
#   music=menu>targettest:5/5 — check-device-target.sh:1239 hard-fails
#                (`mustrack leg coverage ... != 5`) BEFORE the verdict
#                echo, so the only value that can ever reach the log
#                through a passing run is 5/5. The old N/5 wildcard
#                admitted 0/5..9/5, every one of which means the
#                producer's own coverage assertion was bypassed.
#
# NO LEADING ZEROS IN ANY NUMERIC FIELD (review-117-delta-1 [M2], class
# fix): `[0-9]{1,6}` accepts `000042` and `007.950`, which no shell
# `printf %d`/`%.3f` emitter can produce — again a pattern matching only
# malformed output. Every variable numeric field below is therefore
# `(0|[1-9][0-9]{0,N})`: bare zero, or a nonzero leading digit. The
# FRACTIONAL part of p99 keeps `[0-9]{3}` — leading zeros there are
# genuine digits (7.045 ms), not a malformed integer. OPKFOH_RE's
# transition count is the same class and is fixed with it rather than
# left as the one remaining hole (HARD RULE 8: class fix over one-off).
# Every change in this block is strictly NARROWING: the accepted
# language is a proper subset of what it was, so no run that passed
# before on genuine producer output can fail now.
FULLGAME_RE='^FULLGAME CONFORMS 12/12 \(render\+sfx\+music live; live-ai=g07,g08,m01,m02 p99=(0|[1-9][0-9]{0,2})\.[0-9]{3}ms skips=0 underruns=0 starves=0 presentfails=0 teeth=21\)$'
TARGET_RE='^DEVICE TARGET CONFORMS \(goldens=2 flows=2 shots=4 fbwit=4 p99=(0|[1-9][0-9]{0,2})\.[0-9]{3}ms skips=0 underruns=0 starves=0 starts f06-target-t01=15 f07-target-t02=31 sfxpin=15/31 music=menu>targettest:5/5 live=f08-live-target:(0|[1-9][0-9]{0,5})f/(0|[1-9][0-9]{0,5})rows/opts-ok bound=f09-live-bound:(0|[1-9][0-9]{0,5})f/(0|[1-9][0-9]{0,5})rows teeth=6\)$'
FOH_RE='^DEVICE FOH OK \(flows=5 shots=13 bridge=1 states=3 opk=evidence fbwit=15 p99=(0|[1-9][0-9]{0,2})\.[0-9]{3}ms skips=0 underruns=0 starves=0 starts f01-vs-g01=(0|[1-9][0-9]{0,5}) f02-cpu-m01=(0|[1-9][0-9]{0,5}) f03-options=(0|[1-9][0-9]{0,5}) f04-nav=(0|[1-9][0-9]{0,5}) f05-vs-g03=(0|[1-9][0-9]{0,5}) teeth=15\)$'
OPKFOH_RE='^OPK FOH LAUNCH OK \(frontend-launched via gmenu2x into the FOH, boot marker bin-sha == stamp, evidence rc=0, 1 transitions vs frozen, shot structural\)$'

# RESEMBLANCE discriminators (review-108-1 H3): these must be STRICTLY
# LOOSER than the full grammars, or a torn line cannot be seen. The
# earlier prefixes ended in a SPACE, so a truncation that tore exactly
# at the prefix boundary (`FULLGAME CONFORMS` with nothing after it)
# matched NEITHER the full grammar nor the resemblance guard: the counts
# stayed 1/1 and the corrupt log PASSED. Each discriminator below is the
# shortest stable line-start of its producer's verdict, with no trailing
# space, so every truncation of that verdict is caught.
# UNANCHORED ON PURPOSE (review-119-delta-5b [H1] — the class fix that ends
# the junk-prefix whack-a-mole). These stems were `^`-anchored for four
# rounds, and each round found a new byte that could be PREPENDED to a torn
# verdict to escape every guard at once: the anchored stem no longer matched
# (the line stopped starting with the stem) and the proper-prefix loop only
# looks at prefixes beginning at byte zero. Round 3 found NUL, round 4 found
# CR/SOH/ESC, round 5 found `X`, SPACE and TAB — i.e. the refusable-byte set
# was never the invariant. The invariant is: in a genuine evidence log the
# verdict stem occurs on EXACTLY ONE line, ANYWHERE in that line. Counting
# unanchored makes every junk prefix — printable, control, whitespace, any
# byte at all — push the count to 2 and be refused, without enumerating
# anything. MEASURED, zero false-reject: over all 18 archived genuine engine
# leg logs (FULLGAME 3, TARGET 4, FOH 9, OPKFOH 2) the anchored and
# unanchored counts are both exactly 1 — identical, no exceptions. (Codex
# review transcripts do diverge, because they quote verdicts in prose; those
# are not evidence logs and are never fed to this function.)
FULLGAME_RESEM='FULLGAME CONFORMS'
TARGET_RESEM='DEVICE TARGET CONFORMS'
FOH_RESEM='DEVICE FOH OK'
OPKFOH_RESEM='OPK FOH LAUNCH'

echo "== [E1/4] FULL-GAME TRACE SUITE (engine: check-device-fullgame.sh) =="
run_engine fullgame port/sim/device/check-device-fullgame.sh
FG_LINE="$(expect_grammar "$VDIR/leg-fullgame.log" "$FULLGAME_RE" "$FULLGAME_RESEM" 'full-game suite')"
FG_P99="$(p99_under_budget "$FG_LINE" 'full-game suite')"
echo "   engine E1 PASS: $FG_LINE"

echo "== [E2/4] TARGET GOLDENS + FLOWS f06-f07 (engine: check-device-target.sh) =="
run_engine target port/sim/target/check-device-target.sh
TG_LINE="$(expect_grammar "$VDIR/leg-target.log" "$TARGET_RE" "$TARGET_RESEM" 'target')"
TG_P99="$(p99_under_budget "$TG_LINE" 'target')"
echo "   engine E2 PASS: $TG_LINE"

echo "== [E3/4] MENU FLOWS f01-f05 (engine: check-device-foh.sh) =="
run_engine foh port/foh/check-device-foh.sh
FOH_LINE="$(expect_grammar "$VDIR/leg-foh.log" "$FOH_RE" "$FOH_RESEM" 'FOH')"
FOH_P99="$(p99_under_budget "$FOH_LINE" 'FOH')"
echo "   engine E3 PASS: $FOH_LINE"

echo "== [E4/4] OPK FRONTEND LAUNCH INTO THE FOH (engine: check-device-opk.sh --foh) =="
run_engine opkfoh port/gfx/check-device-opk.sh MLFK_OPK_FOH=1
OPK_LINE="$(expect_grammar "$VDIR/leg-opkfoh.log" "$OPKFOH_RE" "$OPKFOH_RESEM" 'OPK-FOH')"
echo "   engine E4 PASS: $OPK_LINE"

# --- leg composition: the flow-coverage reconciliation -----------------------
# Leg [2] claims EVERY committed flow ran on device. The two engines
# carry PINNED flow counts in their verdicts (flows=5 and flows=2);
# their sum must equal the committed flow count, derived here from the
# tree by an anchored filename grammar. A flow added without extending
# an engine therefore fails the GATE rather than silently narrowing
# menu coverage (the check-device-conform.sh matrix-pin lesson).
# The engines' flow arrays are fixed literal sequences
# (check-device-foh.sh:374 FLOW_IDS, check-device-target.sh:320
# FLOW_IDS) and those exact ids are pinned in the verdict grammars
# above. The committed set must equal their union EXACTLY — pinned as
# identities, not as a count (review-108-1 H5): a count-only check
# passes when f05 is deleted and f08 added, which would narrow menu
# coverage to a flow no engine drives while the gate still went green.
PINNED_FLOW_SET="f01-vs-g01 f02-cpu-m01 f03-options f04-nav f05-vs-g03 f06-target-t01 f07-target-t02"
flow_ids=""
# ENUMERATION RC IS CHECKED (review-108-3 H-a): a process substitution's
# exit status does NOT propagate through `while ... done < <(...)`, even
# under `set -euo pipefail` — a find that emitted the seven expected
# paths and THEN hit an I/O/metadata error would leave the computed set
# equal to the pin and the gate would go green on a partial enumeration.
# So the listing is materialized to a file and its rc checked first.
# SHAPE COVERAGE (review-108-3 H-b): no `-maxdepth`, and symlinks are
# included (`-type f -o -type l`) — a nested `flows/.extra/f08.flow` or
# a committed symlink is a normal repository change this gate exists to
# catch, not a hostile one. Anything enumerated that is not one of the
# seven pinned ids dies at the filename grammar or the set equality.
FLOWLIST="$VDIR/committed-flows.txt"
rm -f "$FLOWLIST"
if ! find port/foh/flows \( -type f -o -type l \) -name '*.flow' \
     | LC_ALL=C sort > "$FLOWLIST"; then
  echo "M4 GATE FAIL: enumeration of committed flows failed (find/sort rc)" >&2
  exit 1
fi
# `find`, NOT a shell glob (review-108-2 H5): `*.flow` silently EXCLUDES
# dotfiles, so a committed `.f08-secret.flow` left the computed set
# equal to the pin — seven driven flows passing while an eighth
# committed flow was never driven, the exact hidden-flow hole. find's
# -name matches leading dots, so a hidden flow now reaches the filename
# grammar below and dies there (the grammar requires a leading `f`).
while IFS= read -r f; do
  [ -n "$f" ] || continue
  bn="${f##*/}"
  if ! [[ "$bn" =~ ^f[0-9]{2}-[a-z0-9-]+\.flow$ ]]; then
    echo "M4 GATE FAIL: committed flow filename fails the anchored grammar: '$bn'" >&2
    exit 1
  fi
  flow_ids="$flow_ids ${bn%.flow}"
done < "$FLOWLIST"
# sorted, space-normalized set equality (implies count AND identity AND
# uniqueness — the check-device-conform.sh matrix-pin construction)
flow_ids="$(printf '%s\n' $flow_ids | LC_ALL=C sort | tr '\n' ' ')"
flow_ids="${flow_ids% }"
if [ "$flow_ids" != "$PINNED_FLOW_SET" ]; then
  echo "M4 GATE FAIL: committed flow set != the set the engines drive" >&2
  echo "  pinned:    {$PINNED_FLOW_SET}" >&2
  echo "  committed: {$flow_ids}" >&2
  echo "  A flow added/removed/renamed without extending an engine would narrow" >&2
  echo "  menu coverage silently — leg [2] claims EVERY committed flow ran." >&2
  exit 1
fi
echo "   flow-coverage reconciliation OK: 5 (FOH) + 2 (target) == the 7 committed flows, by identity"

LEG1="full-game 12/12 (p99 $FG_P99 ms) + targets 2/2 (p99 $TG_P99 ms), all streams host-judged"
LEG2="flows 7/7 on device (f01-f05 FOH p99 $FOH_P99 ms; f06-f07 target)"
LEG3="$OPK_LINE"

echo ""
echo "M4 GATE SUMMARY (all legs judged host-side; device never self-reports):"
echo "  [1] full-game conformance: $LEG1"
echo "  [2] menu flows:            $LEG2"
echo "  [3] opk frontend->FOH:     $LEG3"
# SENTINEL LOCKOUT: the sentinel exists ONLY in the AUTHORITATIVE=1
# branch — a dev/canned run is structurally incapable of printing it
# and always exits nonzero.
# LOG-SENTINEL CONTRACT (emission site): every relayed sub-content line
# in this script's output carries the '  | ' prefix (relay_lines,
# defined at the top), so THIS echo is the only line-anchored
# `M4 GATE OK` the gate can ever emit — a line-anchored scan of the
# combined output is sound.
if [ "$AUTHORITATIVE" = 1 ]; then
  echo "M4 GATE OK"
  exit 0
fi
echo "M4 GATE (DEV — NON-AUTHORITATIVE)"
echo "  reason(s):$AUTH_REASONS"
echo "  the M4 GATE OK sentinel prints ONLY on a fully-authoritative all-real run"
echo "  (no MLFK_M4_DEV, no MLFK_M4_FAKE_LEG_DIR, all statuses review-closed)."
exit 3
