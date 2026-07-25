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
#       {reviewed-go, oracle-frozen, grandfathered-m2} BEFORE any leg
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
MANIFEST_SHA256=0000000000000000000000000000000000000000000000000000000000000000

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
REQUIRED_PRODUCERS="port/sim/device/adbsh.sh
port/sim/device/riglib.sh
port/sim/device/percentiles.js
port/sim/device/check-device-fullgame.sh
port/sim/target/check-device-target.sh
port/goldens-m4/verify-target-stream.js
port/goldens-m4/wrap-target.js
port/goldens-m4/validate-target-manifest.js
port/foh/check-device-foh.sh
port/foh/judge-foh-trace.js
port/foh/normalize-foh-trace.js
port/foh/flow-to-fkscript.js
port/gfx/judge-shot.js
port/gfx/judge-render-timing.js
port/gfx/judge-audio-summary.js
port/gfx/pack-snd.js
port/gfx/check-device-opk.sh
port/gfx/opk/mlfk-foh.sh
port/gfx/opk/meleelight-foh.funkey-s.desktop
port/gfx/opk/icon32.png
port/tools/fk_input.c
port/sim/sim/wrap-run.js
port/sim/sim/trace-to-txt.js
oracle/harness/verify-stream.js
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
  if ! [[ "$mline" =~ ^[0-9a-f]{64}\ [A-Za-z0-9._/-]+\ (reviewed-go|oracle-frozen|grandfathered-m2|arc-in-flight|arc-pending)\ [A-Za-z0-9._/:+-]+$ ]]; then
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
    reviewed-go|oracle-frozen|grandfathered-m2) : ;;
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
    echo "M4 GATE REFUSED: $n_unresolved producer(s) lack review closure — status not in {reviewed-go, oracle-frozen, grandfathered-m2}:" >&2
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
expect_grammar() {
  local logf="$1" full="$2" resem="$3" label="$4" c r line
  c="$(grep -cE "$full" "$logf")" || true
  if [ "$c" != 1 ]; then
    echo "M4 GATE FAIL: expected exactly 1 $label verdict line matching the pinned grammar in $logf, found $c" >&2
    echo "  grammar: $full" >&2
    echo "  — an engine that exits 0 without its exact verdict is CORRUPT evidence" >&2
    exit 1
  fi
  r="$(grep -cE "$resem" "$logf")" || true
  if [ "$r" != 1 ]; then
    echo "M4 GATE FAIL: $logf carries $r lines matching the verdict-resemblance grammar '$resem' but exactly 1 full-grammar verdict — a verdict-RESEMBLING malformed line (truncated/torn duplicate) is CORRUPTION, never ignorable" >&2
    exit 1
  fi
  line="$(grep -E "$full" "$logf")"
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
  while IFS= read -r l || [ -n "$l" ]; do
    # strip a trailing CR before comparing (review-108-3 H-c): a torn
    # CRLF write leaves `F\r`, which is not a proper prefix of the
    # verdict as raw bytes and would slip through both guards.
    l="${l%$'\r'}"
    [ -n "$l" ] || continue
    [ "$l" != "$line" ] || continue
    case "$line" in
      "$l"*)
        echo "M4 GATE FAIL: $logf carries a line that is a PROPER PREFIX of the $label verdict — a truncated/torn duplicate write is CORRUPTION, never ignorable" >&2
        echo "  torn:   '$l'" >&2
        echo "  verdict:'$line'" >&2
        exit 1
        ;;
    esac
  done < "$logf"
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
FULLGAME_RE='^FULLGAME CONFORMS 12/12 \(render\+sfx\+music live; live-ai=g07,g08,m01,m02 p99=[0-9]{1,3}\.[0-9]{3}ms skips=0 underruns=0 starves=0 presentfails=0 teeth=[0-9]{1,3}\)$'
TARGET_RE='^DEVICE TARGET CONFORMS \(goldens=2 flows=2 shots=4 fbwit=[0-9]{1,6} p99=[0-9]{1,3}\.[0-9]{3}ms skips=0 underruns=0 starves=0 starts f06-target-t01=[0-9]{1,6} f07-target-t02=[0-9]{1,6} sfxpin=[0-9]{1,6}/[0-9]{1,6} music=menu>targettest:[0-9]{1,2}/5 teeth=[0-9]{1,3}\)$'
FOH_RE='^DEVICE FOH OK \(flows=5 shots=[0-9]{1,4} bridge=1 states=3 opk=evidence fbwit=[0-9]{1,6} p99=[0-9]{1,3}\.[0-9]{3}ms skips=0 underruns=0 starves=0 starts f01-vs-g01=[0-9]{1,6} f02-cpu-m01=[0-9]{1,6} f03-options=[0-9]{1,6} f04-nav=[0-9]{1,6} f05-vs-g03=[0-9]{1,6} teeth=[0-9]{1,3}\)$'
OPKFOH_RE='^OPK FOH LAUNCH OK \(frontend-launched via gmenu2x into the FOH, boot marker bin-sha == stamp, evidence rc=0, [0-9]{1,3} transitions vs frozen, shot structural\)$'

# RESEMBLANCE discriminators (review-108-1 H3): these must be STRICTLY
# LOOSER than the full grammars, or a torn line cannot be seen. The
# earlier prefixes ended in a SPACE, so a truncation that tore exactly
# at the prefix boundary (`FULLGAME CONFORMS` with nothing after it)
# matched NEITHER the full grammar nor the resemblance guard: the counts
# stayed 1/1 and the corrupt log PASSED. Each discriminator below is the
# shortest stable line-start of its producer's verdict, with no trailing
# space, so every truncation of that verdict is caught.
FULLGAME_RESEM='^FULLGAME CONFORMS'
TARGET_RESEM='^DEVICE TARGET CONFORMS'
FOH_RESEM='^DEVICE FOH OK'
OPKFOH_RESEM='^OPK FOH LAUNCH'

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
