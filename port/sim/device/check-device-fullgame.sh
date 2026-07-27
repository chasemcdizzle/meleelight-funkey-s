#!/usr/bin/env bash
# port/sim/device/check-device-fullgame.sh — the M4 EXIT GATE's LEG-1
# ENGINE (fix_plan §M4 task 14; assembled iter 109). Replays EVERY
# match/scenario golden of leg [1] ON the FunKey-S with live render +
# the SFX mixer + SD music streaming, pulls all evidence, and judges
# ALL of it HOST-side. The device never self-reports.
#
# THE 12 GOLDENS (pinned both directions against both manifests):
#   oracle/goldens/manifest.json      g01 g02 g03 g04 g05 g06 g07 g08
#   port/goldens-m4/manifest.json     m01 m02 s01 s02
# The 2 TARGET goldens (t01/t02) are leg 1's other half and belong to
# port/sim/target/check-device-target.sh — not this engine.
#
# LIVE AI (the M4 binding, not AIBRIDGE1): the four CPU goldens
# g07/g08 (difficulty 5), m01 (1) and m02 (9) run on the REAL C ai.c
# linked into foh_device (riglib.sh:1366-1368 — sim_ai_live.c installs
# the ml_sim_runai_live seam by constructor). This check asserts, per
# leg and by construction, that NO --ai-bridge argument appears in any
# device argv: an AIBRIDGE1 replay would satisfy the stream but NOT the
# gate's "g07/g08 driven by the LIVE C ai.c" clause.
#
# WHY THE DIRECT-MATCH ENTRY EXISTS (measured, iter 109 — this is the
# reason leg 1 could not be assembled before): the FOH's CPU-difficulty
# domain is the UPSTREAM SLIDER's 1..4 (foh.h:79-82), so difficulty 5
# (g07/g08) and 9 (m02) are STRUCTURALLY UNREACHABLE through any menu
# flow — no `.flow` can ever drive them. foh_dev.c's direct-match entry
# (`--p1 --p2 --p2type --difficulty --stage`) writes the SAME FohState
# fields the SSS-A launch arm writes and then runs the UNTOUCHED launch
# seam, so these legs exercise the product binary's real
# sim_setup_match path while bypassing only the menus (leg [2] owns
# the menus). Proof that the bypass reaches an identical launch state:
# section [3] cmp's the direct BRIDGE-STATE witness for g01 against the
# frozen MENU-DRIVEN witness port/foh/flows/f01-vs-g01.bstate.expect.
#
# JUDGMENT FORMS (all host-side; pre-registered .loop/m4-t109-prereg.md):
#  - STREAM: the UNCHANGED oracle/harness/verify-stream.js (BY PATH,
#    never copied) over wrap-run.js output vs the frozen
#    <golden>.sha256.json — exact per-frame equality, FULL 3600-frame
#    length, rngCalls/rngCallsOutsideStep/specVersion pins.
#  - PERF: judge-render-timing.js over the per-frame timing artifact —
#    full p99 < 16.67 ms (integer ns, no float arithmetic), skips == 0,
#    rendered == frames.
#  - AUDIO: anchored full-line grammars over foh_dev's own `foh_dev
#    audio:` summary (NOT judge-audio-summary.js, which pins the
#    `gfx_app audio:` producer prefix and does not match this binary) —
#    underruns == 0, badlen == 0, and voice starts/stops == the
#    freshly-run host twin's. A both-zero self-consistency pass is
#    impossible: the twin must ALSO report starts > 0 for every golden.
#    MEASURED SPLIT: platform_headless has no audio device, so the twin
#    reports rate=0 samples=0 channels=0 and 0 callbacks — the twin is
#    the reference for the sim-event-driven MIXER counters only, and the
#    callback/underrun/streaming evidence is DEVICE-ONLY by construction.
#  - MUSIC: on the DEVICE, starves == 0 and refills != 0 (the SD
#    streamer really ran); on BOTH, the mustrack line names the
#    PROGRAMMED stage track by path, so a menu track that never switched
#    cannot pass.
#  - PRESENTS: failed presents == 0 in the match summary.
#  - BRIDGE-STATE: the device witness == the host twin's, byte-exact.
#
# Prints `FULLGAME CONFORMS 12/12 (...)`, exit 0; ANY divergence, pin
# mismatch, grammar violation, perf/audio/music shortfall, or missing
# artifact -> nonzero. The verdict line is parsed by verify_m4.sh with
# an anchored full-line grammar plus proper-prefix corruption guards —
# it is emitted EXACTLY ONCE, as the last line, and never echoed
# anywhere else in this script.
#
# Env: FUNKEY_ADB_ID (device id), MLFK_FORCE_ARM=1 (shared arm rebuild),
#      MLFK_DEADMAN_S (frontend-park deadman seconds, default 1800),
#      MLFK_FULLGAME_ATTRIB=1 (M4 task 14 increment 3a — arm the
#      skip-attribution instrument, see the block below; DEFAULT OFF and
#      structurally NON-AUTHORITATIVE when on).
set -euo pipefail
cd "$(dirname "$0")/../../.."

FOH=port/foh
GFX=port/gfx
M4G=port/goldens-m4
SIMD=port/sim/sim
CAL=port/sim/calib
BUILD=$FOH/build/device-fullgame
DEVB=port/sim/calib/build/device
TABLES=pipeline/build/sim-tables
FDC=oracle/fdlibm-crosscheck
AUDIO_OUT=pipeline/build/audio-fullgame
DTMP=/tmp/mlfk
DSD=/mnt/mlfk-scratch

# --- pins (named constants; measured-then-frozen, reviewed re-freeze only) ----
PINNED_GOLDEN_SET="g01 g02 g03 g04 g05 g06 g07 g08 m01 m02 s01 s02"
PINNED_LIVE_AI_SET="g07 g08 m01 m02"   # == the verdict's live-ai= token
# The verdict's live-ai token, built by PURE PARAMETER EXPANSION
# (review-109-4 L6): the old form ran `printf | tr` inside the verdict's
# own argument list, so a `tr` that emitted usable bytes and THEN failed
# was invisible — a decision-bearing pipeline hidden inside a larger
# successful command. No subprocess, no status to launder.
LIVE_AI_CSV="${PINNED_LIVE_AI_SET// /,}"
N_GOLDENS_PIN=12
FRAMES_PIN=3600
BUDGET_NS=16666667
P99_FULL_LIMIT_NS=16670000             # the 16.67 ms frame budget, integer ns
WALL_MIN_MS=58000                      # 3600 paced frames == 60.0 s nominal
WALL_MAX_MS=78000
READY_TRIES=90
DEADMAN_S="${MLFK_DEADMAN_S:-1800}"

# --- THE SWAP-PRESSURE EVIDENCE BAR (M4 task 14 increment 3d) ----------------
# The stall class attributed in iter-110 is: memory pressure -> tmpfs/anon
# swap-out to SD -> SD-controller IRQ storms -> involuntary-preemption bursts
# -> an isolated frame over budget. Increment 3d removes that pressure AT ITS
# CAUSE by moving the suite's own artifact plane off tmpfs (see [4/9]), so
# `d_pswpout` per leg is the number that says whether the cause is gone.
#
# 200 pages (800 kB) per ~71 s leg. DERIVED from the frozen per-leg tables,
# not picked: run-7's mid-suite floor is literally 0 on five legs
# (g02/m01/m02/s01/s02) and the largest other floor-tier value is 160 (g06);
# 200 is the smallest round number strictly above it. It is a HARD bar — only
# 6 of run 7's 12 legs would meet it, and run-7's leg 1 (4495) misses by 22x.
#
# DECISION-INERT, deliberately (iter-111 round-2 H1, settled): the sd-diag arm
# must never be able to fail a leg the app itself passed, and the M4 EXIT
# gate's frozen leg-[1] conditions carry no swap-counter pin. This value
# thresholds a PRINTED summary line, nothing else.
BAR_PAGES=200

# --- SKIP-ATTRIBUTION ARM (M4 task 14 increment 3a) --------------------------
# The gate fails on a stochastic ~1-2-per-pass frame stall (measured
# 15-29 ms, migrating leg/frame/phase). Attribution is only valid if the
# measured run reproduces the LEG conditions exactly — same per-leg
# quiesce window, same launcher, same paced 3600-frame match with
# render+sfx+music live — so the instrument is armed HERE rather than in
# a separate rig that would measure a different question.
#
# When armed, each leg additionally: passes `--attrib` to foh_device (the
# per-frame row sampler in port/gfx/attrib.h — sampled OUTSIDE the
# sim/render/present brackets, so it cannot inflate any number
# judge-render-timing.js computes), brackets the paced window with
# /proc kernel-counter snapshots taken OUTSIDE it, and runs the UNCHANGED
# port/sim/device/skip-attrib/correlate-skips.js over the pulled
# evidence. sk_sampler is deliberately NOT armed: its 250 ms /proc poll
# is itself a periodic CPU consumer on this single-core A7 and could
# MANUFACTURE the event class under investigation (it stays available for
# a targeted follow-up run where that trade is worth making).
#
# STRUCTURALLY NON-AUTHORITATIVE: an armed run suffixes its verdict line
# with ` [ATTRIB-ARMED]`, which verify_m4.sh's anchored full-line
# FULLGAME_RE cannot match. An armed pass can therefore never mint the
# authoritative M4 gate result, exactly as a dev override cannot.
# LEVELS: 0 = off (the gate configuration). 1 = per-frame rows + pre/post
# /proc snapshots only. 2 = level 1 PLUS a concurrent sk_sampler, whose
# 250 ms /proc windows are the ONLY way to name WHICH irq/process caused
# a specific frame's preemption burst — paid for with a periodic poller
# on a single-core A7, so it is opt-in above level 1 rather than default.
ATTRIB="${MLFK_FULLGAME_ATTRIB:-0}"
case "$ATTRIB" in
  0|1|2) ;;
  *) echo "FULLGAME FAIL: MLFK_FULLGAME_ATTRIB must be 0, 1 or 2, got '$ATTRIB'" >&2
     exit 1 ;;
esac
ATTRIB_TAG=""
[ "$ATTRIB" != 0 ] && ATTRIB_TAG=" [ATTRIB-ARMED]"
SKA=port/sim/device/skip-attrib
SAMPLER_PERIOD_MS=250       # the iter-74 pre-registered cadence, verbatim
# 175 s cap at 250 ms. check-skip-attrib.sh uses 400 (100 s) for a ~63 s
# run; a fullgame leg can wait up to READY_TRIES=90 s for the ready
# marker BEFORE its ~60 s match, so 400 could run the sampler dry
# mid-match and turn uncovered events into silent `win=none` rows
# (review-110-1 finding 3). Sized so the cap cannot bite, and the
# uncovered-event count is REPORTED per leg so a gap is never silent.
SAMPLER_MAX=700
# THE DEADMAN LEASE (review-109-5 H2). The deadman fires only after BOTH
# its countdown has elapsed AND the host has been silent for this long —
# so it can never act concurrently with a live host that is mid-leg
# (the interleaving that let it consume a leg's quiesce marker between the
# marker write and the daemon stop). 300 s is ~4x the longest gap between
# renewals (one paced leg is ~75 s and the host renews at both ends of it).
LEASE_STALE_S=300
# The heartbeat is a MONOTONIC SEQUENCE NUMBER, not a timestamp
# (review-109-6 H1): the deadman watches for CHANGE rather than computing
# an age, so neither a forward nor a backward RTC jump on this
# non-wall-synced device can make a live host look silent (or the reverse).
LEASE_SEQ=0
# (There is deliberately NO cap on the gate — review-109-8 H1. The cap
# existed to bound a clock-BASED gate against a backwards RTC jump; the
# gate is clock-free now, so a cap could only ever force a fire against a
# still-live host or make the watchdog abandon the device. Removed.)
# EXACT-QUIESCE-WINDOW SLACKS (review-109-4 M; the reviewed sibling values
# — check-device-foh.sh:121-122, check-device-target.sh:93-94 — adopted
# unchanged along with the per-leg protocol they belong to).
QW_PRE_SLACK_S=10
QW_POST_SLACK_S=10
# THE FROZEN TEETH COUNT (review-109-4 L5). The verdict advertises
# `teeth=<n>`; without an exact assertion a deleted or skipped tooth just
# lowers the number and still passes every grammar. Reconciliation of the
# round-4 22-vs-21 discrepancy: T1 and T1b are ONE composite tooth (the
# SAME nibble-flipped stream copy, judged first by the raw verify-stream.js
# divergence class and then by the production judge_stream), so the raw
# half is a precondition of the composite and no longer increments the
# counter — it keeps its own exact rc-2 assertion. Current inventory:
# 18 tooth_expect calls (T1b T2-T12 T15-T20) + 3 counted controls
# (T13 T14 T21) = 21.
TEETH_PIN=21
# The DIRECT-ENTRY zero-tick producer line, MEASURED identical across all
# 24 host+device logs. Held as ONE literal (see assert_direct_bypass).
FOH_BYPASS_LINE='foh_dev foh: 0 ticks, 0 transitions, 0 shots, 0 render skips, 0 failed presents, launched=1'
AUDIO_RATE=44100
AUDIO_SAMPLES=512
AUDIO_CHANNELS=2
SNDPACK_COUNT=180
# the frozen MENU-DRIVEN launch witness the direct entry must reproduce
G01_MENU_BSTATE=$FOH/flows/f01-vs-g01.bstate.expect

# --- SUSTAINED-PLAYBACK WINDOWS (review-109-2 H3; measured-then-frozen) ------
# The verdict has to prove the audio and music planes ran for the WHOLE
# 60-second match, not merely that they started. Derivation and the
# measured spread over ALL 12 device legs (pass 2, .loop/m4-t109-host12.log
# and port/foh/build/device-fullgame/*.dev-applog.txt):
#   callbacks   nominal = rate*60/samples = 44100*60/512 = 5168
#               MEASURED 5185 .. 5216      -> window [5000, 5400]
#   music out   nominal = rate*60          = 2,646,000 frames
#               MEASURED 2,644,480 .. 2,646,528 -> window [2,600,000, 2,700,000]
#   refills     MEASURED exactly 80 on every one of the 12 legs
#                                          -> window [70, 90]
# The windows are deliberately wider than the measured spread (device
# scheduling jitter is real) but FAR narrower than any early-stop
# regression: music stopping after one refill yields refills=1 and
# mu_out ~ 44,100, both of which die here.
CB_MIN=5000
CB_MAX=5400
MUSOUT_MIN=2600000
MUSOUT_MAX=2700000
REFILL_MIN=70
REFILL_MAX=90

# --- FROZEN PER-GOLDEN SFX PINS (review-109-2 M7; measured-then-frozen) -----
# `<id> <voice starts> <voice stops>`, MEASURED on pass 2 with twin and
# device agreeing exactly on all 12. These are the INDEPENDENT reference
# that makes the twin comparison more than self-consistency: a deleted
# stop arm moves twin and device together, but not these. A legitimate
# sim/mixer change re-freezes this table as a REVIEWED edit.
SFX_PINS="g01 274 0
g02 234 0
g03 204 0
g04 221 0
g05 304 12
g06 227 0
g07 150 0
g08 302 0
m01 114 0
m02 229 0
s01 60 4
s02 27 2"

NUM12='(0|[1-9][0-9]{0,11})'

fail() { echo "FULLGAME FAIL: $1" >&2; exit 1; }
grammar_die() { echo "FULLGAME FAIL: $1" >&2; exit 2; }

# --- ERREXIT DISCIPLINE (review-109-2 H2) ------------------------------------
# MEASURED on the required macOS system bash 3.2.57 — do NOT "simplify" this:
#
#   set -e; rc=0; ( set -e; false; echo SURVIVED ) || rc=$?   -> prints
#   SURVIVED, rc=0.  The inner `set -e` does NOT re-arm, because the WHOLE
#   subshell is an operand of an AND-OR list and bash suppresses errexit for
#   its entire dynamic extent. Round 1's H2 "fix" (an inner `set -e`) was
#   therefore INEFFECTIVE, and so was the same re-arm in tooth_expect.
#
#   set +e; ( set -e; false; echo SURVIVED ); rc=$?; set -e   -> prints
#   nothing, rc=1.  A command substitution death inside a called function
#   propagates too (rc=2 for grammar_die).
#
# So: every guarded subshell in this script runs STANDALONE with the parent
# temporarily at `set +e`, and its status is captured immediately afterwards.
# run_guarded <rc-var-name> <cmd...> is that pattern in one place.
# INTERFACE SAFETY (review-109-3 L6): the out-parameter is dynamically
# scoped, so a caller naming it `__rcvar`/`__rc` would write this
# function's own local and silently keep its previous value (typically
# 0 — a swallowed failure). Those names are REFUSED. The caller's
# original `errexit` state is snapshotted and restored rather than
# unconditionally enabled.
run_guarded() {
  local __rcvar="$1"; shift
  # The guard is PRE-EXECUTION by contract: every rejected name must die
  # BEFORE "$@" runs. review-109-4 L4: the old charset test accepted
  # digit-leading names such as `9rc`, so the guarded command ran and only
  # then did `printf -v` refuse the identifier — the failure was reported
  # after the side effects, and with the out-parameter left unwritten.
  # The shell-identifier grammar is now enforced in full.
  # ORDINARY-VARIABLE ALLOWLIST (review-109-5 L5). Beyond this function's
  # own locals, bash has SPECIAL variables that silently refuse or discard
  # an assignment: `printf -v PIPESTATUS 1` writes nothing the caller can
  # read back, and RANDOM/SECONDS/LINENO reinterpret whatever is stored.
  # Every one of them is UPPERCASE, and every legitimate out-parameter in
  # this script is an ordinary lowercase name, so requiring
  # [a-z][a-z0-9_]* is an allowlist that excludes the entire class by
  # construction rather than by an enumeration that will go stale.
  case "$__rcvar" in
    __rc|__rcvar|__eopt)
      fail "run_guarded: reserved out-parameter name '$__rcvar' would alias this function's own local" ;;
    "")
      fail "run_guarded: empty out-parameter name" ;;
    # LOCALE TRAP (MEASURED while validating this guard): a shell range
    # like [!a-z] is COLLATION-ordered, and under the default en_US.UTF-8
    # collation `a-z` contains the UPPERCASE letters too (aAbBcC...), so
    # `[!a-z]*` did NOT reject `PIPESTATUS` — the exact class this guard
    # exists for. The sets below are enumerated explicitly, so they mean
    # the same thing in every locale.
    [!abcdefghijklmnopqrstuvwxyz]*)
      fail "run_guarded: out-parameter name '$__rcvar' must start with a lowercase letter (digits and bash's uppercase special variables cannot hold a status)" ;;
    *[!abcdefghijklmnopqrstuvwxyz0123456789_]*)
      fail "run_guarded: out-parameter name '$__rcvar' outside [a-z0-9_]" ;;
  esac
  local __rc=0 __eopt
  case "$-" in *e*) __eopt=1 ;; *) __eopt=0 ;; esac
  set +e
  ( set -e; "$@" )
  __rc=$?
  [ "$__eopt" = 1 ] && set -e
  printf -v "$__rcvar" '%s' "$__rc"
}

# --- whitelist-grammar plumbing (review-109-1 M4/M6) --------------------------
# nl_terminated <file> <label> — a producer artifact whose FINAL line lost
# its newline is a TORN WRITE, not a short file. Command substitution
# strips trailing newlines, so without this check `RC=0` (torn) and
# `RC=0\n` (canonical) are indistinguishable downstream.
nl_terminated() {
  local f="$1" label="$2" n
  [ -f "$f" ] || grammar_die "$label: $f missing"
  [ -s "$f" ] || grammar_die "$label: $f is empty"
  # STANDALONE, STATUS-CHECKED capture (review-109-3 M2): inside
  # `[ "$(...)" = 1 ]` the surrounding `[` reports success on the TEXT
  # alone, so a producer that printed `1` and then failed still certified
  # newline termination. The assignment is its own command, so `set -e`
  # (or the explicit guard) sees the pipeline's real status.
  n="$(tail -c 1 "$f" | wc -l | tr -d ' ')" \
    || grammar_die "$label: could not read the final byte of $f"
  [ "$n" = 1 ] \
    || grammar_die "$label: $f does not end in a newline (torn write)"
}

# grep_count <re> <file> <label> — grep -c with its STATUS PRESERVED.
# `grep -c ... || true` launders EVERY nonzero rc into a parseable count,
# so a read error (rc 2) reads as "0 matches" — i.e. as clean absence.
# Here rc 1 (no match) is the only tolerated failure; anything else dies.
grep_count() {
  local re="$1" f="$2" label="$3" n rc=0
  n="$(grep -cE -e "$re" "$f")" || rc=$?
  [ "$rc" -le 1 ] || grammar_die "$label: grep failed reading $f (rc $rc)"
  printf '%s\n' "$n"
}

# grammar_exactly_one <file> <label> <loose-re> <exact-re>
# THE WHITELIST-GRAMMAR RULE (PROCESS §3) in FULL. The earlier parsers
# applied only its second half: they counted EXACT matches, so one valid
# producer line could coexist with a torn or duplicated line that merely
# RESEMBLES it and the pair still read as "exactly 1 — clean". Both counts
# must be 1: a resembling line that does not match exactly is CORRUPTION
# and dies, never "the other one was fine". The loose patterns are
# MEASURED from the real corpus (port/foh/build/device-fullgame/*.dev-applog.txt
# + twin-*/log.txt): the only lines beginning `foh_dev <sect>: <digit>`
# are the summaries themselves; every other foh_dev producer line uses a
# different section word or a non-digit first token.
grammar_exactly_one() {
  local f="$1" label="$2" loose="$3" exact="$4" nloose nexact
  nl_terminated "$f" "$label"
  nloose="$(grep_count "$loose" "$f" "$label")"
  nexact="$(grep_count "$exact" "$f" "$label")"
  [ "$nexact" = 1 ] \
    || grammar_die "$label: $nexact lines match the anchored grammar exactly (want 1) in $f"
  [ "$nloose" = 1 ] \
    || grammar_die "$label: $nloose lines RESEMBLE the producer grammar but only 1 matches it exactly — torn/duplicated evidence in $f"
  grep -E "$exact" "$f"
}

# kv_lookup <file> <key> — bash 3.2 has NO associative arrays (this repo
# runs the macOS system bash), so keyed state lives in flat files read
# through an anchored, EXACTLY-ONE-MATCH lookup. That is strictly better
# than an array here: a duplicated or missing key is corruption and dies
# loudly instead of silently taking the last write / an empty string.
# (review-109-2 H2: the count now goes through grep_count, which preserves
# grep's rc — the old `|| true` laundered a read error into "0 matches".)
kv_lookup() { # -> prints the remainder of the single matching line
  local f="$1" k="$2" n line
  [ -f "$f" ] || fail "kv lookup '$k': $f missing"
  case "$k" in
    *[!A-Za-z0-9_]*) fail "kv lookup key '$k' outside [A-Za-z0-9_]" ;;
  esac
  n="$(grep_count "^${k} " "$f" "kv lookup '$k'")"
  [ "$n" = 1 ] || fail "kv lookup '$k' in $f matched $n lines (want exactly 1)"
  line="$(grep -E "^${k} " "$f")"
  printf '%s\n' "${line#"$k" }"
}

source port/sim/device/adbsh.sh
source port/sim/device/riglib.sh

# lease_renew — the deadman heartbeat (review-109-6 H1). ATOMIC: written to
# a temp path and mv'd into place, so the deadman can never observe a
# half-written (empty) heartbeat and commit to firing while this host is
# alive. MONOTONIC: a sequence number this host owns, so every renewal
# CHANGES the value even if the device clock is frozen or jumps.
# SESSION-FENCED (review-109-8 H2): the value is <run nonce>:<sequence>,
# so a deadman from an EARLIER run can tell a successor's heartbeat from
# its own frozen one and retires itself instead of acting on it.
lease_renew() {
  LEASE_SEQ=$((LEASE_SEQ + 1))
  dsh "printf '%s\n' '$DM_NONCE:$LEASE_SEQ' > $DTMP/deadman.lease.tmp && mv -f $DTMP/deadman.lease.tmp $DTMP/deadman.lease" >/dev/null \
    || fail "could not renew the deadman lease heartbeat (seq $LEASE_SEQ) — refusing to continue with an unprovable liveness signal"
}
mkdir -p "$BUILD" "$DEVB"
# the frozen SFX pin table, materialised for kv_lookup's exactly-one-match
# reader (bash 3.2 has no associative arrays)
SFX_PIN_FILE=$BUILD/sfx-pins.txt
rm -f "$SFX_PIN_FILE"
printf '%s\n' "$SFX_PINS" > "$SFX_PIN_FILE"
rig_lock_acquire
# PESSIMISTIC until the inherited device state has been normalized
# (review-109-2 H1; the check-device-foh.sh / check-device-target.sh
# protocol, adopted verbatim in shape). rig_cleanup's orphan reap wipes
# $DTMP — including a LIVE deadman's nonce — so $DTMP must be preserved
# for as long as a deadman may be the device's only recovery net.
RIG_PRESERVE_DTMP=1
PARKED=0
DEADMAN_ARMED=0
LBC_STOPPED=0
LBC_CLAIMED=0
DM_NONCE="$RANDOM$RANDOM$$"

# cleanup_all — restoration is VERIFIED, never assumed (review-109-1 H1).
# The earlier version cleared PARKED / DEADMAN_ARMED / LBC_STOPPED
# unconditionally and cancelled the deadman BEFORE confirming the
# frontend was actually unparked: a failed restore then looked identical
# to a clean one, and the deadman — the only remaining safety net — was
# disarmed anyway, leaving the device parked with nothing to recover it.
# Now each flag is cleared ONLY on a verified restore, the deadman is
# cancelled ONLY after the unpark is confirmed absent, and any
# unrestored state is a LOUD warning naming what a human must fix.
#
# ROUND-2 CORRECTIONS (review-109-2 H1), all measured, not assumed:
#  - ORDER. The daemon is restored BEFORE the deadman is cancelled, and
#    the deadman is cancelled ONLY when BOTH the unpark AND the daemon
#    restore are verified. Previously a failed daemon restore happened
#    after cancellation, with nothing left to recover it.
#  - $DTMP. When either restore is unverified the deadman must stay ARMED,
#    so RIG_PRESERVE_DTMP is re-armed before rig_cleanup — otherwise
#    rig_cleanup's orphan reap kills the live deadman and wipes its nonce,
#    i.e. destroys the very recovery net the warning promises.
#  - EXIT STATUS. MEASURED on bash 3.2: an EXIT trap that merely `return`s
#    a status does NOT change the script's exit status (`trap cl EXIT;
#    exit 0` with `cl(){ return 7; }` still exits 0). Forcing a failure
#    requires an explicit `exit`.
cleanup_all() {
  local rc=$? unparked=0 lbc_ok=0
  # THE CLAIM IS HELD THROUGH THE RESTORE, NOT RELEASED BEFORE IT
  # (review-109-7 H1). Releasing first opened the exact window the claim
  # exists to close: a deadman waiting on BUSY would take the claim the
  # instant cleanup dropped it and race this restore into the
  # non-idempotent START channel. If this run died BEFORE taking a claim
  # (e.g. outside a leg) but still owes a daemon restore, take one now —
  # bounded, and a failure is only a warning, because cleanup must never
  # hang or mask the run's real exit code.
  # OWNERSHIP IS REVALIDATED, NEVER INHERITED FROM MEMORY (review-109-9
  # H1): cleanup can run after an arbitrarily long block, so a claim this
  # run *thinks* it holds may have been taken over. rig_qd_reassert
  # re-reads the device's token and re-acquires when it is no longer ours;
  # if this run held no claim at all but owes a restore, it takes one.
  if [ "$LBC_STOPPED" = 1 ]; then
    if rig_qd_reassert; then
      LBC_CLAIMED=1
    else
      LBC_CLAIMED=0
      echo "DEVICE WARN: could not (re-)take the recovery claim during cleanup — restoring low_bat_check anyway (an unclaimed restore is still better than leaving the daemon down); rig_daemon_restore re-derives cardinality by comm-scan and refuses any inventory != 0/1" >&2
    fi
  fi
  # our own processes first (idempotent, rc-tolerant)
  rig_dsh_retry "pkill foh_device; true" >/dev/null 2>&1 \
    || echo "DEVICE WARN: could not pkill foh_device on device" >&2
  # sk_sampler only when THIS run armed it (review-110-1 finding 1): the
  # disarmed path must be inert, and an unconditional pkill would also
  # reach a sampler some OTHER rig started.
  if [ "$ATTRIB" = 2 ]; then
    rig_dsh_retry "pkill sk_sampler; true" >/dev/null 2>&1 \
      || echo "DEVICE WARN: could not pkill sk_sampler on device" >&2
  fi
  # daemon FIRST, so its outcome can gate the deadman cancellation
  if [ "$LBC_STOPPED" = 1 ]; then
    if rig_daemon_restore low_bat_check /etc/init.d/S12low-bat-check; then
      # MARKER LIFECYCLE (review-109-3 H1). The quiesce marker means
      # "this daemon MAY still be down". Leaving it in place after a
      # VERIFIED host restore is not merely untidy: if transport is then
      # lost before the deadman is cancelled, the preserved deadman sees
      # the stale marker and starts a SECOND low_bat_check (this init
      # channel is not idempotent). So the marker is removed and its
      # ABSENCE verified before LBC_STOPPED is cleared; if it cannot be
      # removed, the daemon is up but the recovery state is wrong, and
      # that is NOT a clean restore.
      if rig_dsh_retry "rm -f $DTMP/qd.low_bat_check.$DM_NONCE" >/dev/null 2>&1 \
         && rig_dsh_retry "test ! -f $DTMP/qd.low_bat_check.$DM_NONCE" >/dev/null 2>&1; then
        lbc_ok=1
        LBC_STOPPED=0
      else
        echo "DEVICE WARN: low_bat_check restarted but its quiesce marker $DTMP/qd.low_bat_check.$DM_NONCE could NOT be removed/verified-absent — a firing deadman could start a SECOND instance; remove that file before the next run" >&2
      fi
    else
      echo "DEVICE WARN: low_bat_check was NOT restored to its boot cardinality — restore it with '/etc/init.d/S12low-bat-check start' before the next run (rig_daemon_stop refuses any inventory != 1)" >&2
    fi
  else
    lbc_ok=1
  fi
  # ...and ONLY NOW is the claim released — after the restore AND the
  # marker removal, so no waiting deadman can enter the daemon plane while
  # this cleanup is still working in it (review-109-7 H1).
  if [ "$LBC_CLAIMED" = 1 ]; then
    rig_qd_unclaim
    LBC_CLAIMED=0
  fi
  if [ "$PARKED" = 1 ]; then
    rig_dsh_retry "rm -f /mnt/disable_frontend" >/dev/null 2>&1 \
      || echo "DEVICE WARN: could not remove /mnt/disable_frontend" >&2
    # VERIFY, then clear
    if rig_dsh_retry "test ! -f /mnt/disable_frontend" >/dev/null 2>&1; then
      unparked=1
      PARKED=0
    else
      echo "DEVICE WARN: frontend still parked after restore attempt — /mnt/disable_frontend is PRESENT; the deadman is being LEFT ARMED so it can recover the device" >&2
    fi
  else
    unparked=1
  fi
  if [ "$DEADMAN_ARMED" = 1 ]; then
    if [ "$unparked" = 1 ] && [ "$lbc_ok" = 1 ]; then
      if rig_dsh_retry "touch $DTMP/deadman.cancel" >/dev/null 2>&1; then
        DEADMAN_ARMED=0
      else
        echo "DEVICE WARN: could not cancel the park deadman — it will fire on its own (idempotent) within ${DEADMAN_S}s" >&2
      fi
    else
      echo "DEVICE WARN: frontend/daemon restore UNVERIFIED — leaving the deadman ARMED as the backstop (that is its purpose) and preserving $DTMP so its nonce survives" >&2
      RIG_PRESERVE_DTMP=1
    fi
  fi
  rig_cleanup
  if [ "$unparked" != 1 ] || [ "$lbc_ok" != 1 ]; then
    echo "DEVICE WARN: cleanup left device state UNRESTORED (unparked=$unparked low_bat_check_restored=$lbc_ok) — see the warnings above" >&2
    [ "$rc" != 0 ] || rc=1
  fi
  # explicit exit: a bare `return` would silently keep the ORIGINAL status
  exit "$rc"
}
trap cleanup_all EXIT

require_device

# --- [0/9] startup normalization + inherited-state ownership ------------------
# (review-109-2 H1) Adopt an inherited quiesce marker THROUGH rig_qd_normalize
# BEFORE anything in this script wipes $DTMP — otherwise a prior orphaned
# run's daemon record is erased and its daemon never gets restored.
echo "== [0/9] startup normalization + inherited-state ownership =="
rig_qd_normalize
stale_marker=0
stale_deadman=0
nrc=0
dsh "test -f /mnt/disable_frontend" >/dev/null || nrc=$?
case "$nrc" in
  0) stale_marker=1 ;;
  1) : ;;
  *) fail "startup normalization could not probe the frontend marker (rc $nrc)" ;;
esac
nrc=0
dsh "test -e $DTMP/deadman.nonce -o -e $DTMP/deadman.pid" >/dev/null || nrc=$?
case "$nrc" in
  0) stale_deadman=1 ;;
  1) : ;;
  *) fail "startup normalization could not probe stale deadman state (rc $nrc)" ;;
esac
if [ "$stale_marker" = 1 ] || [ "$stale_deadman" = 1 ]; then
  echo "WARN: stale prior-run state on device (marker=$stale_marker deadman-state=$stale_deadman) — normalizing before any parking" >&2
  if [ "$stale_marker" = 1 ]; then
    PARKED=1
    dsh "rm -f /mnt/disable_frontend"
    dsh "test ! -f /mnt/disable_frontend"
    PARKED=0
    echo "   stale /mnt/disable_frontend removed (RC-verified absent)"
  fi
  if [ "$stale_deadman" = 1 ]; then
    dsh "mkdir -p $DTMP && touch $DTMP/deadman.cancel"
    sdm_gone=0
    for _ in $(seq 1 6); do
      if dsh "test ! -f $DTMP/deadman.pid" >/dev/null 2>&1; then sdm_gone=1; break; fi
      sleep 2
    done
    if [ "$sdm_gone" != 1 ]; then
      sdm_pid="$(dsh "cat $DTMP/deadman.pid")"
      [[ "$sdm_pid" =~ ^(0|[1-9][0-9]{0,6})$ ]] \
        || fail "stale deadman.pid is not a bounded pid ('$sdm_pid')"
      nrc=0
      dsh "test -d /proc/$sdm_pid" >/dev/null || nrc=$?
      case "$nrc" in
        0) fail "a stale deadman (pid $sdm_pid) is STILL RUNNING and ignored its cancel — inspect the device" ;;
        1) echo "   stale deadman.pid was orphaned (pid $sdm_pid already dead)" ;;
        *) fail "could not probe pid $sdm_pid liveness (rc $nrc)" ;;
      esac
    fi
    dsh "rm -rf $DTMP"
    echo "   stale deadman state wiped ($DTMP)"
  fi
fi
RIG_PRESERVE_DTMP=0
rig_devsha_selftest
echo "   device state clean; sha tool self-tested"

# --- [1/9] golden matrix pin + producer pins ---------------------------------
echo "== [1/9] golden matrix pin (both manifests, both directions) =="
[ -f oracle/goldens/manifest.json ] || fail "oracle golden manifest missing"
[ -f "$M4G/manifest.json" ] || fail "M4 golden manifest missing"

# The matrix is validated INSIDE node over the parsed JSON and emitted as
# ONE sorted id line, consumed QUOTED (the check-device-conform.sh
# construction): count, id grammar, uniqueness and the CPU role are all
# proven before a single byte is built or pushed.
manifest_ids="$(node -e '
const fs = require("fs");
const die = (m) => { console.error("matrix: " + m); process.exit(1); };
const want = new Set(process.argv[1].split(" "));
const cpuWant = new Set(process.argv[2].split(" "));
// EXACT SCHEMA (review-109-1 M5): the golden row key set is CLOSED and
// fully typed. JSON.parse is tolerant — a missing field reads as
// undefined and an extra field is invisible — so both directions are
// asserted here instead of only the fields this script happens to read.
const ROW_KEYS = ["id", "name", "trace", "frames", "seed",
                  "p1", "p2", "stage", "cpu", "difficulty"];
const TOP_KEYS = ["comment", "goldens"];
const ids = [];
for (const [f, re] of [["oracle/goldens/manifest.json", /^g0[1-8]$/],
                       ["port/goldens-m4/manifest.json", /^[ms][0-9]{2}$/]]) {
  const text = fs.readFileSync(f, "utf8");
  const j = JSON.parse(text);
  const topKeys = Object.keys(j).slice().sort();
  if (topKeys.join(",") !== TOP_KEYS.slice().sort().join(",")) {
    die("top-level key set of " + f + " is {" + topKeys + "}, want {" + TOP_KEYS + "}");
  }
  // TOP-LEVEL duplicate keys and types (review-109-2 M5): the row-level
  // detector below missed the top level entirely, so a duplicated
  // "comment", or an EMPTY first "goldens" followed by the canonical
  // array, survived JSON.parse last-wins with the expected parsed key set
  // AND the expected row-token counts. MEASURED: each of these tokens
  // occurs exactly once in each real manifest (their long prose comments
  // never contain the quoted `"key":` form).
  for (const k of TOP_KEYS) {
    const n = (text.match(new RegExp("\"" + k + "\"\\s*:", "g")) || []).length;
    if (n !== 1) die(f + ": " + n + " top-level `\"" + k + "\":` tokens (want exactly 1)");
  }
  if (typeof j.comment !== "string" || j.comment.length === 0) {
    die(f + ": top-level comment is not a non-empty string");
  }
  if (!Array.isArray(j.goldens)) die("goldens not an array in " + f);
  // DUPLICATE-KEY DETECTION (review-109-1 M5). JSON.parse silently keeps
  // the LAST of duplicated properties, so a hand-edit that repeats a key
  // inside one golden row is invisible to any parsed-object check. The
  // schema is fixed at ROW_KEYS per row + TOP_KEYS once, so the raw text
  // must carry EXACTLY that many `"<key>":` tokens. MEASURED against both
  // real manifests before shipping: their long `comment` prose mentions
  // key names but never in the quoted `"key":` form, so this is exact
  // here and has zero false rejections on the genuine corpus.
  for (const k of ROW_KEYS) {
    const n = (text.match(new RegExp("\"" + k + "\"\\s*:", "g")) || []).length;
    if (n !== j.goldens.length) {
      die(f + ": " + n + " `\"" + k + "\":` tokens for " + j.goldens.length +
          " goldens — duplicated or missing key");
    }
  }
  for (const g of j.goldens) {
    const gk = Object.keys(g).slice().sort();
    if (gk.join(",") !== ROW_KEYS.slice().sort().join(",")) {
      die("golden " + g.id + " key set is {" + gk + "}, want {" + ROW_KEYS + "}");
    }
    if (typeof g.id !== "string" || !re.test(g.id)) die("bad id " + g.id + " in " + f);
    if (ids.includes(g.id)) die("duplicate id " + g.id);
    if (typeof g.name !== "string" || !/^[A-Za-z0-9._-]+$/.test(g.name)) die(g.id + " name grammar");
    if (typeof g.trace !== "string" || !/^[A-Za-z0-9._-]+$/.test(g.trace)) die(g.id + " trace grammar");
    if (g.frames !== 3600) die(g.id + " frames != 3600");
    if (!Number.isInteger(g.seed) || g.seed < 0) die(g.id + " seed not a non-negative integer");
    for (const k of ["p1", "p2"]) {
      if (!Number.isInteger(g[k]) || g[k] < 0 || g[k] > 4) die(g.id + " " + k + " out of domain");
    }
    if (!Number.isInteger(g.stage) || g.stage < 0 || g.stage > 5) die(g.id + " stage out of domain");
    if (typeof g.cpu !== "boolean") die(g.id + " cpu is not a boolean");
    const isCpu = g.cpu === true;
    if (isCpu !== cpuWant.has(g.id)) die(g.id + " cpu role disagrees with the pin");
    if (isCpu && !(Number.isInteger(g.difficulty) && g.difficulty >= 1 && g.difficulty <= 9)) {
      die(g.id + " cpu golden without an in-domain difficulty");
    }
    if (!isCpu && g.difficulty !== null) {
      die(g.id + " non-cpu golden carries a difficulty");
    }
    ids.push(g.id);
  }
}
if (ids.length !== Number(process.argv[3])) die("golden count " + ids.length);
for (const id of ids) if (!want.has(id)) die("unpinned golden " + id);
console.log(ids.slice().sort().join(" "));
' "$PINNED_GOLDEN_SET" "$PINNED_LIVE_AI_SET" "$N_GOLDENS_PIN")" \
  || fail "golden matrix validation failed"
pinned_sorted="$(printf '%s\n' $PINNED_GOLDEN_SET | LC_ALL=C sort | tr '\n' ' ')"
pinned_sorted="${pinned_sorted% }"
[ "$manifest_ids" = "$pinned_sorted" ] \
  || fail "manifest golden set {$manifest_ids} != pinned {$pinned_sorted}"
echo "   matrix OK: $N_GOLDENS_PIN goldens, live-AI subset {$PINNED_LIVE_AI_SET}"

# PRODUCER IDENTITY PINS (PROCESS §4; review-109-2 M6). These were
# EXISTENCE-ONLY checks, which prove nothing: "hashes prove identity, not
# approval", but no hash at all proves neither. Every DECISION-BEARING
# tool this verdict rests on — the stream judge, the run wrapper, the
# trace converter, the timing judge, the sound packer, and (new in
# round 1's M9 fix) the pipeline artifact/contract verifiers plus the
# frozen expected.json contract itself — is sha256-pinned to its reviewed
# bytes. An accidental unreviewed edit that WEAKENS any of them now dies
# here instead of silently certifying bad evidence.
# NOTE for the gate layer: these same paths must also join
# port/sim/device/m4-freeze-manifest.txt (increment 3, driver-owned) —
# this in-script pin is the engine's own guarantee, not the gate's.
# json-dup-key-scan.js joined this table in iter 117 (review-117-plib-4o
# [L]): both plib verifiers now `require` it to scan raw JSON bytes before
# parsing, so it became decision-bearing on THIS path — a weakened scanner
# would re-open the last-wins duplicate-key hole it exists to close.
PRODUCER_PINS="$(cat <<'EOF'
f420723433b19166b53a80aedf54931ffdfbc6d2505c773fd73b7a13bbcdf60e oracle/harness/verify-stream.js
0bc801ea46b06a63e79377aae164636a5e9f649ee45835748e5f2387b9e04281 oracle/harness/streamlib.js
b835b5f886225e0015dae152576eea5a42fa69d7ba0699f4de0e31438d05c5b9 port/sim/sim/wrap-run.js
4160a35b36e8d3d6896ad2c3c6239d4a4860a0d7f43814a7a9b53b7c136742ab port/sim/sim/trace-to-txt.js
4b68fba5a804b281a73003b29eac1a0290707f2b6260ee39c900a0262962f421 port/gfx/judge-render-timing.js
bf29fa7cba83708cfff093195f48ad20a8eda2ad794fe25b270f211b08876eee port/gfx/pack-snd.js
826e16854bdbfb6061052d1a0dbdcc4675f282754b04fb5262f34b5b30283a45 pipeline/lib/verify-artifacts.js
ec578b42d0490448d61bef3f21c958105a48d99c4cc1a5d403a742e7627b8e18 pipeline/lib/check-expected.js
46606bed441ae2923c7c67355f12b789f5854543295c5e718fa679bf23a0d533 pipeline/lib/manifest.js
31e5946a0269095f7895b01aaf7f78e9c3496aae75c638b8cd1b4a678c4bd29b pipeline/expected.json
624956898890e749170a4768af0f8ef86e05ce4dd75046d084701747c9d9121f port/goldens-m4/json-dup-key-scan.js
a574fec40685b6770e85d55ee1aaabd35553caab00c9aeadec0ea234b4173590 pipeline/lib/tables-anim-xref.js
EOF
)"
nprod=0
while IFS=' ' read -r want p extra; do
  [ -n "$p" ] || continue
  [ -z "${extra:-}" ] || fail "producer pin row has extra fields ('$extra')"
  [[ "$want" =~ ^[0-9a-f]{64}$ ]] || fail "producer pin for $p is not a sha256 ('$want')"
  [ -f "$p" ] || fail "producer $p missing"
  got="$(rig_host_sha256 "$p")" || exit 1
  [ "$got" = "$want" ] \
    || fail "producer $p is NOT the reviewed bytes (got $got, pinned $want) — a decision-bearing tool changed; re-pin only after its review reaches GO"
  nprod=$((nprod + 1))
done <<< "$PRODUCER_PINS"
[ "$nprod" = 12 ] || fail "producer pin table has $nprod rows (want 12)"
echo "   $nprod decision-bearing producers sha256-verified against their reviewed pins"

# per-golden params, parsed by a no-eval strict line parser
golden_params() { # <id> -> name trace frames seed p1 p2 stage cpu difficulty gdir mfarg
  local id="$1" out
  out="$(node -e '
const fs = require("fs");
const id = process.argv[1];
for (const [f, dir] of [["oracle/goldens/manifest.json", "oracle/goldens"],
                        ["port/goldens-m4/manifest.json", "port/goldens-m4"]]) {
  const j = JSON.parse(fs.readFileSync(f, "utf8"));
  for (const g of j.goldens) {
    if (g.id !== id) continue;
    console.log("name=" + g.name);
    console.log("trace=" + g.trace);
    console.log("frames=" + g.frames);
    console.log("seed=" + g.seed);
    console.log("p1=" + g.p1);
    console.log("p2=" + g.p2);
    console.log("stage=" + g.stage);
    console.log("cpu=" + (g.cpu ? 1 : 0));
    console.log("difficulty=" + (g.difficulty == null ? 3 : g.difficulty));
    console.log("gdir=" + dir);
    console.log("mfarg=" + (dir === "oracle/goldens" ? "-" : f));
    process.exit(0);
  }
}
console.error("golden " + id + " not found");
process.exit(1);
' "$id")" || fail "cannot read params for $id"
  unset name trace frames seed p1 p2 stage cpu difficulty gdir mfarg
  # SEEN-KEY + CARDINALITY (review-109-1 M5): the previous loop took the
  # LAST value for a repeated key and never noticed a MISSING one (the
  # `: "$name" ...` line below only catches unset-under-`set -u`, which a
  # duplicate cannot trip). Every key must appear EXACTLY once and the
  # emitted line count must equal the closed key set's size.
  local seen="" nlines=0
  while IFS='=' read -r gk gv; do
    case " $seen " in *" $gk "*) fail "$id: param key '$gk' emitted more than once" ;; esac
    case "$gk" in
      name|trace|gdir|mfarg)
        [[ "$gv" =~ ^[A-Za-z0-9._/-]+$ ]] || fail "$id: $gk grammar ('$gv')" ;;
      frames|seed|p1|p2|stage|cpu|difficulty)
        [[ "$gv" =~ ^(0|[1-9][0-9]{0,5})$ ]] || fail "$id: $gk grammar ('$gv')" ;;
      *) fail "$id: unexpected param key '$gk'" ;;
    esac
    seen="$seen $gk"
    nlines=$((nlines + 1))
    printf -v "$gk" '%s' "$gv"
  done <<< "$out"
  [ "$nlines" = 11 ] \
    || fail "$id: golden_params emitted $nlines lines (want exactly 11, one per key)"
  : "$name" "$trace" "$frames" "$seed" "$p1" "$p2" "$stage" "$cpu" \
    "$difficulty" "$gdir" "$mfarg"
  [ "$frames" = "$FRAMES_PIN" ] || fail "$id: frames $frames != $FRAMES_PIN"
}

# --- [2/9] host data plane ----------------------------------------------------
echo "== [2/9] host data plane (tables, simdata, traces, audio) =="
bash pipeline/extractor/build-extractor.sh >/dev/null
# FRESHNESS, WHOLE-OUTPUT (review-109-1 M7). The earlier guard removed and
# `made`-checked only the three generated .c files, so the generated
# HEADERS and all five ANIM1 binaries could survive from an earlier run
# whenever generation partially succeeded (or exited 0 without rewriting
# them) — and those stale bytes were then compiled and rendered. The whole
# output directory is rebuilt from scratch and EVERY consumed artifact is
# freshness-checked, headers and animation binaries included, then the
# run's own manifest is re-hashed by the reviewed pipeline verifier
# (which also rejects stray files the manifest does not list).
ANIM_BINS="anim_0_marth.bin anim_1_puff.bin anim_2_fox.bin anim_3_falco.bin anim_4_falcon.bin"
GEN_TABLES="ml_tables.c ml_tables.h ml_stages.c ml_stages.h ml_targets.c ml_targets.h"
rm -rf "$TABLES"
node pipeline/run.js --only animations,tables,stages,targets --out "$TABLES" >/dev/null
for g in $GEN_TABLES $ANIM_BINS manifest.json; do made "$TABLES/$g"; done
node pipeline/lib/verify-artifacts.js "$TABLES" >/dev/null \
  || fail "generated tables/animations fail their own pipeline manifest re-hash"
# EXTERNAL identity, not just self-consistency (review-117-plib-1 [H]1):
# the re-hash above proves only that the run agrees with the manifest the
# same run wrote, so a deterministic generator regression that preserves
# every count passed it. check-expected.js compares each stage's
# path+sha256 aggregate to the pin frozen in pipeline/expected.json and
# asserts the coverage contract for exactly the four stages this partial
# run produces; tables-anim-xref.js asserts the 72 framesData/ECB<->ANIM1
# reconciliation pins, which no other arm on this path ever read.
node pipeline/lib/check-expected.js "$TABLES" "${MELEELIGHT_CLONE:-$HOME/.cache/meleelight-funkey-s/upstream}" animations,tables,stages,targets >/dev/null \
  || fail "generated tables/animations fail the frozen pipeline coverage contract (pipeline/expected.json)"
node pipeline/lib/tables-anim-xref.js "$TABLES" >/dev/null \
  || fail "generated tables fail the frozen framesData/ECB <-> ANIM1 reconciliation pins"
rm -f "$BUILD/simdata.txt"
node "$CAL/dump-sim-data.js" --out "$BUILD/simdata.txt" >/dev/null
made "$BUILD/simdata.txt"
mkdir -p "$BUILD/traces"
for id in $PINNED_GOLDEN_SET; do
  golden_params "$id"
  rm -f "$BUILD/traces/$id.trace.txt"
  node "$SIMD/trace-to-txt.js" "$gdir/$trace" "$BUILD/traces/$id.trace.txt" >/dev/null
  made "$BUILD/traces/$id.trace.txt"
done
echo "   tables + simdata + $N_GOLDENS_PIN input traces built"

DIST="${MELEELIGHT_CLONE:-$HOME/.cache/meleelight-funkey-s/upstream}"
rm -rf "$AUDIO_OUT"
node pipeline/run.js --only audio --dist "$DIST" --out "$AUDIO_OUT" >/dev/null
made "$AUDIO_OUT/sounds.json" "$AUDIO_OUT/manifest.json"
# REVIEWED AUDIO PINS (review-109-1 M9). Byte-stability x2 over the SAME
# pipeline output proves only determinism: a wrong upstream clone, or
# silent/altered PCM, is perfectly byte-stable, hashes identically
# host-vs-device, produces identical voice-start counters and leaves every
# simulation hash untouched — so the whole audio plane could be wrong and
# every other assertion in this check would still pass. These are the M1
# gate's OWN reviewed verifiers, invoked BY PATH: verify-artifacts.js
# re-hashes every artifact against the manifest, and check-expected.js
# asserts pipeline/expected.json's frozen audio contract — the ffmpeg
# version AND exact argv, the 204/180/8 coverage counts, the blob shape
# rules, and the AGGREGATE artifact sha256 pin that a different clone or
# altered PCM cannot survive.
node pipeline/lib/verify-artifacts.js "$AUDIO_OUT" >/dev/null \
  || fail "audio artifacts fail their own pipeline manifest re-hash"
node pipeline/lib/check-expected.js "$AUDIO_OUT" "$DIST" audio >/dev/null \
  || fail "audio plane fails the frozen pipeline/expected.json audio contract (ffmpeg pin, coverage, aggregate sha256)"
# sndpack x2 byte-stable, anchored producer grammar (sole-line: a
# canonical line coexisting with malformed output is corruption, M5)
pack_re="^pack-snd OK count=${SNDPACK_COUNT} dataBytes=${NUM12} fileBytes=${NUM12}\$"
for side in a b; do
  rm -f "$BUILD/sndpack-$side.bin" "$BUILD/pack-out-$side.txt"
  node "$GFX/pack-snd.js" "$AUDIO_OUT" "$BUILD/sndpack-$side.bin" \
    > "$BUILD/pack-out-$side.txt" || fail "pack-snd.js failed (side $side)"
  made "$BUILD/sndpack-$side.bin" "$BUILD/pack-out-$side.txt"
  grammar_exactly_one "$BUILD/pack-out-$side.txt" "pack-snd (side $side)" \
    '^pack-snd ' "$pack_re" >/dev/null
  packlines="$(wc -l < "$BUILD/pack-out-$side.txt" | tr -d ' ')" \
    || grammar_die "pack-snd (side $side): could not count output lines"
  [ "$packlines" = 1 ] \
    || grammar_die "pack-snd (side $side): output is not a SOLE line ($packlines lines)"
done
cmp "$BUILD/sndpack-a.bin" "$BUILD/sndpack-b.bin" || fail "sndpack not byte-stable x2"
rm -f "$BUILD/sndpack.bin"; cp "$BUILD/sndpack-a.bin" "$BUILD/sndpack.bin"
made "$BUILD/sndpack.bin"

# music: the MENU track (programmed at load, foh_dev.c:1340) plus all six
# VS stage tracks (mus_stage_track maps stage 0..5 -> tokens 1..6). The
# token -> PCM basename map is upstream's own naming, MEASURED from the
# pipeline audio stage, not guessed.
MUS_TOKENS="menu battlefield ystory pstadium dreamland fdest fountain"
mus_pcm_for() { # <token> -> pipeline basename
  case "$1" in
    menu) echo menu ;; battlefield) echo battlefield ;;
    ystory) echo yStory ;; pstadium) echo pStadium ;;
    dreamland) echo dreamland ;; fdest) echo finald ;;
    fountain) echo fod ;;
    *) fail "unknown music token '$1'" ;;
  esac
}
# NOTE (measured, not assumed): the sounds.json music KEY is the upstream
# PCM basename (yStory / pStadium / finald / fod), NOT the mixer's token
# name (ystory / pstadium / fdest / fountain). mus_pcm_for is the one
# mapping site; the manifest carries the TOKEN, the lookup uses the KEY.
MUS_KEYS=""
for tok in $MUS_TOKENS; do MUS_KEYS="$MUS_KEYS $(mus_pcm_for "$tok")"; done
MUS_KEYS="${MUS_KEYS# }"
mcfg="$(node -e '
const fs = require("fs");
const die = (m) => { console.error(m); process.exit(1); };
const s = JSON.parse(fs.readFileSync(process.argv[1], "utf8"));
if (s.formatVersion !== 1) die("sounds.json formatVersion != 1");
for (const key of process.argv[2].split(" ")) {
  const e = s.music && s.music[key];
  if (!e) die("music." + key + " missing");
  if (e.blob !== "audio/music/" + key + ".pcm") die("non-canonical blob path for " + key);
  const sp = e.sprite;
  if (!sp || !Array.isArray(sp.start) || sp.start.length !== 2 ||
      !Array.isArray(sp.loop) || sp.loop.length !== 2) die("sprite shape " + key);
  for (const v of [...sp.start, ...sp.loop]) {
    if (!Number.isInteger(v) || v < 0) die("sprite window not a non-negative integer");
  }
  if (!/^[0-9a-f]{16}$/.test(e.volume.bits)) die("volume bits grammar " + key);
  console.log(key + " " + e.volume.bits + " " + sp.start[0] + " " + sp.start[1] +
              " " + sp.loop[0] + " " + sp.loop[1]);
}
' "$AUDIO_OUT/sounds.json" "$MUS_KEYS")" || fail "music cfg extraction failed"
rm -f "$BUILD/mus-cfg.txt"
# EXACT EXPECTED ROW SET (review-109-1 M5): the loop used to accept any
# number of rows in any order and skip blanks, so an extra/unknown row was
# invisible. The extractor emits one row per requested key IN REQUEST
# ORDER; that is asserted position by position, and the row count must
# equal the key count exactly.
mus_want_keys="$MUS_KEYS"
nmus_want="$(printf '%s\n' $mus_want_keys | wc -l | tr -d ' ')"
mrow=0
while IFS=' ' read -r mkey mvb mso msd mlo mld mextra; do
  mrow=$((mrow + 1))
  [ -z "${mextra:-}" ] || grammar_die "music cfg row $mrow carries extra fields ('$mextra')"
  expk="$(printf '%s\n' $mus_want_keys | sed -n "${mrow}p")"
  [ -n "$expk" ] || grammar_die "music cfg emitted more rows than the $nmus_want requested keys"
  [ "$mkey" = "$expk" ] \
    || grammar_die "music cfg row $mrow is key '$mkey', want '$expk' (request order)"
  [[ "$mvb" =~ ^[0-9a-f]{16}$ ]] || grammar_die "music volbits grammar ($mkey)"
  for v in "$mso" "$msd" "$mlo" "$mld"; do
    [[ "$v" =~ ^(0|[1-9][0-9]{0,11})$ ]] || grammar_die "music window grammar ($mkey)"
  done
  echo "$mkey $mvb $mso $msd $mlo $mld" >> "$BUILD/mus-cfg.txt"
done <<< "$mcfg"
[ "$mrow" = "$nmus_want" ] \
  || grammar_die "music cfg emitted $mrow rows for $nmus_want requested keys"
made "$BUILD/mus-cfg.txt"
rm -f "$BUILD/mus-host.txt" "$BUILD/mus-dev.txt"
nmus=0
for mtok in $MUS_TOKENS; do
  base="$(mus_pcm_for "$mtok")"
  cfg="$(kv_lookup "$BUILD/mus-cfg.txt" "$base")" || exit 1
  [ -n "$cfg" ] || fail "music cfg for token $mtok (key $base) absent"
  [ -f "$AUDIO_OUT/audio/music/$base.pcm" ] || fail "music pcm $base.pcm missing"
  echo "track $mtok $AUDIO_OUT/audio/music/$base.pcm $cfg" >> "$BUILD/mus-host.txt"
  echo "track $mtok $DSD/$base.pcm $cfg" >> "$BUILD/mus-dev.txt"
  nmus=$((nmus + 1))
done
made "$BUILD/mus-host.txt" "$BUILD/mus-dev.txt"
[ "$nmus" = 7 ] || fail "music manifest has $nmus tracks (want 7: menu + 6 VS stages)"
echo "   audio OK (sndpack x2 byte-stable, $nmus music tracks)"

GFXDATA_FROZEN=$GFX/gfxdata-frozen.txt
VFXDATA_FROZEN=$GFX/vfxdata-frozen.txt
VFXGLYPHS_FROZEN=$GFX/vfxglyphs-frozen.txt
for f in "$GFXDATA_FROZEN" "$VFXDATA_FROZEN" "$VFXGLYPHS_FROZEN"; do
  [ -f "$f" ] || fail "frozen data plane $f missing"
done

# --- shared judgment helpers (used by the twins, the device legs AND the
# --- teeth — the teeth must exercise the PRODUCTION judges, not copies)
judge_stream() { # <label> <sim-out> <id> <name> <gdir> <mfarg> <workdir>
  local label="$1" out="$2" id="$3" nm="$4" gd="$5" mf="$6" wd="$7"
  rm -f "$wd/$id.run.json"
  if [ "$mf" = "-" ]; then
    node "$SIMD/wrap-run.js" "$id" "$out" "$wd/$id.run.json" >/dev/null \
      || fail "$label: wrap-run.js failed"
  else
    node "$SIMD/wrap-run.js" "$id" "$out" "$wd/$id.run.json" "$mf" >/dev/null \
      || fail "$label: wrap-run.js failed"
  fi
  made "$wd/$id.run.json"
  node oracle/harness/verify-stream.js "$wd/$id.run.json" "$gd/$nm.sha256.json" \
    || fail "$label: stream does not match the frozen golden"
}

parse_match_summary() { # <log> <frames>
  local lg="$1" fr="$2" re ln
  re="^foh_dev match: ${fr} frames, ${NUM12} render skips, ${NUM12} failed presents, wall ${NUM12} ms, pace=1 budget=${BUDGET_NS} ns\$"
  ln="$(grammar_exactly_one "$lg" "match summary" '^foh_dev match:' "$re")"
  [[ "$ln" =~ ^foh_dev\ match:\ [0-9]+\ frames,\ ([0-9]+)\ render\ skips,\ ([0-9]+)\ failed\ presents,\ wall\ ([0-9]+)\ ms ]] \
    || grammar_die "match summary re-parse failed"
  match_skips="${BASH_REMATCH[1]}"
  match_fails="${BASH_REMATCH[2]}"
  match_wall_ms="${BASH_REMATCH[3]}"
}

# parse_audio_summary <log> <rate> <samples> <channels>
# The device pins the REAL SDL parameters (44100/512/2). The HOST twin
# runs platform_headless, which has no audio device at all: it reports
# rate=0 samples=0 channels=0 and 0 callbacks (MEASURED, not assumed —
# port/foh/build/device-fullgame/twin-*/log.txt). The twin is therefore
# only a reference for the MIXER-side counters (voice starts/stops,
# which are sim-event driven and real in both builds); the callback,
# underrun and music-streaming evidence is DEVICE-ONLY by construction.
# The parameters stay PINNED per call site rather than wildcarded, so a
# device leg that silently opened a different rate still dies.
parse_audio_summary() {
  local lg="$1" rate="$2" samples="$3" channels="$4" re ln
  re="^foh_dev audio: ${NUM12} callbacks, ${NUM12} underruns, ${NUM12} badlen, ${NUM12} voice starts, ${NUM12} voice stops, ${NUM12} steals, rate=${rate} samples=${samples} channels=${channels}\$"
  ln="$(grammar_exactly_one "$lg" "audio summary" '^foh_dev audio:' "$re")"
  [[ "$ln" =~ ^foh_dev\ audio:\ ([0-9]+)\ callbacks,\ ([0-9]+)\ underruns,\ ([0-9]+)\ badlen,\ ([0-9]+)\ voice\ starts,\ ([0-9]+)\ voice\ stops,\ ([0-9]+)\ steals ]] \
    || grammar_die "audio summary re-parse failed"
  au_cbs="${BASH_REMATCH[1]}"; au_underruns="${BASH_REMATCH[2]}"
  au_badlen="${BASH_REMATCH[3]}"; au_starts="${BASH_REMATCH[4]}"
  au_stops="${BASH_REMATCH[5]}"; au_steals="${BASH_REMATCH[6]}"
}

parse_music_summary() { # <log>
  local lg="$1" re ln
  re="^foh_dev music: ${NUM12} out frames, ${NUM12} starves, ${NUM12} refills, ring=32768 chunk=16384\$"
  ln="$(grammar_exactly_one "$lg" "music summary" '^foh_dev music:' "$re")"
  [[ "$ln" =~ ^foh_dev\ music:\ ([0-9]+)\ out\ frames,\ ([0-9]+)\ starves,\ ([0-9]+)\ refills ]] \
    || grammar_die "music summary re-parse failed"
  mu_out="${BASH_REMATCH[1]}"; mu_starves="${BASH_REMATCH[2]}"
  mu_refills="${BASH_REMATCH[3]}"
}

# assert_direct_bypass — the DIRECT-ENTRY CONTRACT, asserted rather than
# assumed (review-109-3 L8). Direct mode's whole claim is that it drives
# the SAME launch seam the menus do while running ZERO FOH ticks; nothing
# was checking that. Making direct mode run one neutral startup tick
# would leave the BRIDGE-STATE witness and every frozen match stream
# untouched, so the gate would still pass while the contract was broken.
# The producer line (MEASURED, identical across all 24 host+device logs)
# is a fixed literal, held once as $FOH_BYPASS_LINE.
# FIXED-LITERAL REGEX (review-109-4 L6): this used to build its anchored
# pattern with `sed` INSIDE the argument list of a larger successful
# command, so a sed that emitted usable text and then failed was invisible
# — a decision-bearing pipeline with no status check. The measured
# producer line contains no ERE metacharacter at all, so anchoring the
# literal IS the grammar. The charset guard below is a pure shell pattern
# match (no subprocess, no status to launder) and fails CLOSED in the
# grammar class if a future re-freeze introduces one.
assert_direct_bypass() { # <log> <label>
  local lg="$1" label="$2"
  case "$FOH_BYPASS_LINE" in
    *[!A-Za-z0-9_' :,=']*)
      grammar_die "$label foh summary: the frozen zero-tick literal contains a character outside the measured [A-Za-z0-9_ :,=] set — it can no longer be anchored as a literal ERE" ;;
  esac
  grammar_exactly_one "$lg" "$label foh summary" '^foh_dev foh:' \
    "^${FOH_BYPASS_LINE}\$" >/dev/null
}

assert_mustrack() { # <log> <pcm-dir> <stage-token> <label>
  local lg="$1" dir="$2" tok="$3" label="$4" base n
  base="$(mus_pcm_for "$tok")"
  nl_terminated "$lg" "$label mustrack"
  # EXACT INVENTORY, IN ORDER (review-109-1 M4). Counting only the stage
  # publish let an EXTRA, LATER track switch pass unseen — precisely the
  # "the music plane did something else" evidence this assert exists to
  # catch. The producer emits exactly two mustrack lines per run
  # (MEASURED on the real corpus): the load-time MENU program
  # `from=none to=menu on=0` (foh_dev.c's mus_track_program(0,0)) and the
  # launch-seam stage publish. The FULL inventory is compared line for
  # line against those two.
  n="$(grep_count '^foh_dev mustrack:' "$lg" "$label mustrack")"
  [ "$n" = 2 ] \
    || fail "$label: $n mustrack publishes in $lg (want exactly 2: the menu program then the $tok stage switch)"
  rm -f "$lg.mustrack.got" "$lg.mustrack.want"
  grep -E '^foh_dev mustrack:' "$lg" > "$lg.mustrack.got"
  { printf 'foh_dev mustrack: from=none to=menu on=0 pcm=%s/menu.pcm\n' "$dir"
    printf 'foh_dev mustrack: from=menu to=%s on=1 pcm=%s/%s.pcm\n' "$tok" "$dir" "$base"
  } > "$lg.mustrack.want"
  cmp -s "$lg.mustrack.got" "$lg.mustrack.want" \
    || fail "$label: mustrack inventory != the expected menu program + $tok stage switch (got: $(tr '\n' '|' < "$lg.mustrack.got"))"
}

# judge_applog — THE per-run app-log policy judgment, in ONE place
# (review-109-1 M11). These assertions used to live INLINE in the leg
# loop while the teeth REIMPLEMENTED them, so deleting a real underrun /
# starve / failed-present assertion left its tooth green: the teeth
# proved their own copies, not the production judge. Legs and teeth now
# both call THIS function, so weakening an assertion mechanically kills
# its own tooth.
# EXIT CLASSES are part of the contract the teeth pin: a POLICY violation
# dies through fail() == rc 1; a GRAMMAR violation (torn, duplicated or
# missing producer line) dies through grammar_die() == rc 2.
judge_applog() { # <label> <log> <frames> <rate> <samples> <channels> <stage-token> <pcm-dir> <twin-starts> <twin-stops> [golden-id for the frozen SFX pin]
  local label="$1" lg="$2" fr="$3" rate="$4" samples="$5" chans="$6"
  local tok="$7" dir="$8" tstarts="$9" tstops="${10}" pinid="${11:-}"
  assert_direct_bypass "$lg" "$label"
  parse_match_summary "$lg" "$fr"
  [ "$match_skips" = 0 ] || fail "$label: match summary reports $match_skips render skips"
  [ "$match_fails" = 0 ] || fail "$label: match summary reports $match_fails failed presents"
  [ "$match_wall_ms" -ge "$WALL_MIN_MS" ] && [ "$match_wall_ms" -le "$WALL_MAX_MS" ] \
    || fail "$label: match wall ${match_wall_ms} ms outside [$WALL_MIN_MS,$WALL_MAX_MS] — not a 60 fps paced run"
  parse_audio_summary "$lg" "$rate" "$samples" "$chans"
  [ "$au_underruns" = 0 ] || fail "$label: $au_underruns audio underruns (want 0)"
  [ "$au_badlen" = 0 ] || fail "$label: $au_badlen audio badlen callbacks (want 0)"
  [ "$au_starts" = "$tstarts" ] \
    || fail "$label: device voice starts $au_starts != twin $tstarts"
  [ "$au_stops" = "$tstops" ] \
    || fail "$label: device voice stops $au_stops != twin $tstops"
  # FROZEN, INDEPENDENT SFX PIN (review-109-2 M7). Comparing the device
  # only against a twin built from the SAME current sim/mixer code is
  # self-consistency: deleting a shared stop arm moves BOTH sides
  # together (s01 60/4 -> 60/3) while the gameplay stream stays exact and
  # the leg passes. The reviewed per-golden tuples in SFX_PINS are the
  # external reference; BOTH sides must equal them.
  if [ -n "$pinid" ]; then
    local want_sfx
    want_sfx="$(kv_lookup "$SFX_PIN_FILE" "$pinid")"
    [[ "$want_sfx" =~ ^([0-9]{1,12})\ ([0-9]{1,12})$ ]] \
      || grammar_die "$label: frozen SFX pin row for $pinid fails its grammar ('$want_sfx')"
    [ "$au_starts" = "${BASH_REMATCH[1]}" ] \
      || fail "$label: voice starts $au_starts != the frozen pin ${BASH_REMATCH[1]}"
    [ "$au_stops" = "${BASH_REMATCH[2]}" ] \
      || fail "$label: voice stops $au_stops != the frozen pin ${BASH_REMATCH[2]}"
  fi
  # SUSTAINED LIVE AUDIO (review-109-2 H3). `au_cbs` and `mu_out` were
  # parsed but never asserted, so a regression that stopped the music
  # after one refill still reported `starves=0 refills=1` with a correct
  # mustrack publish and passed — music for under a second of a
  # 60-second match. Windows are MEASURED over all 12 legs and frozen
  # (see the CB_/MUSOUT_/REFILL_ pins), and they bracket the values a
  # genuinely sustained run must produce.
  if [ "$rate" != 0 ]; then   # device only; the headless twin has no audio device
    [ "$au_cbs" -ge "$CB_MIN" ] && [ "$au_cbs" -le "$CB_MAX" ] \
      || fail "$label: $au_cbs audio callbacks outside the sustained-playback window [$CB_MIN,$CB_MAX]"
  fi
  parse_music_summary "$lg"
  [ "$mu_starves" = 0 ] || fail "$label: $mu_starves music starves (want 0)"
  [ "$mu_refills" != 0 ] || fail "$label: music refills == 0 (the SD streamer never ran)"
  if [ "$rate" != 0 ]; then
    [ "$mu_out" -ge "$MUSOUT_MIN" ] && [ "$mu_out" -le "$MUSOUT_MAX" ] \
      || fail "$label: $mu_out music out frames outside the sustained-playback window [$MUSOUT_MIN,$MUSOUT_MAX] — the track did not stream for the whole match"
    [ "$mu_refills" -ge "$REFILL_MIN" ] && [ "$mu_refills" -le "$REFILL_MAX" ] \
      || fail "$label: $mu_refills music refills outside the sustained-playback window [$REFILL_MIN,$REFILL_MAX]"
  fi
  assert_mustrack "$lg" "$dir" "$tok" "$label"
}

judge_timing() { # <label> <timing> <frames>
  local label="$1" tf="$2" fr="$3" jf ji jk jv
  jf="$tf.judge"
  rm -f "$jf"
  node "$GFX/judge-render-timing.js" "$tf" "$fr" > "$jf" \
    || fail "$label: timing judgment failed"
  made "$jf"
  if ! tail -c 17 "$jf" | cmp -s - <(printf 'judge_complete=1\n'); then
    fail "$label: timing judge output does not END with 'judge_complete=1'"
  fi
  local KEYS=(full_p50_ns full_p50_ms full_p99_ns full_p99_ms
    full_max_ns full_max_ms sim_p50_ns sim_p50_ms sim_p99_ns sim_p99_ms
    render_p50_ns render_p50_ms render_p99_ns render_p99_ms
    render_max_ns render_max_ms present_p50_ns present_p50_ms
    present_p99_ns present_p99_ms skips rendered judge_complete)
  unset full_p99_ns full_p99_ms skips rendered
  ji=0
  while IFS= read -r jline; do
    [ "$ji" -lt "${#KEYS[@]}" ] || fail "$label: timing judge output too long"
    jk="${KEYS[$ji]}"
    case "$jline" in "$jk="*) : ;; *) fail "$label: timing line $((ji+1)) is '$jline' (want key '$jk')" ;; esac
    jv="${jline#"$jk="}"
    case "$jk" in
      judge_complete) [ "$jv" = 1 ] || fail "$label: judge_complete ('$jv')" ;;
      *_ms) [[ "$jv" =~ ^(0|[1-9][0-9]{0,8})\.[0-9]{3}$ ]] || fail "$label: timing $jk grammar ('$jv')" ;;
      *) [[ "$jv" =~ ^(0|[1-9][0-9]{0,11})$ ]] || fail "$label: timing $jk grammar ('$jv')" ;;
    esac
    case "$jk" in full_p99_ns|full_p99_ms|skips|rendered) printf -v "$jk" '%s' "$jv" ;; esac
    ji=$((ji + 1))
  done < "$jf"
  [ "$ji" = "${#KEYS[@]}" ] || fail "$label: timing judge output has $ji lines"
  : "$full_p99_ns" "$full_p99_ms" "$skips" "$rendered"
  # PERSIST THE p99 BEFORE THE skips GATE (review-113-5 [L]). The p99 has
  # already been parsed and grammar-checked at this point, and it is a VALID
  # measurement whether or not the leg then fails `skips==0`. Aggregating it
  # only on the pass path meant a skip-failing leg could carry the run's
  # NARROWEST margin and never appear in the margin summary — and a failing
  # run emitted no margin line at all. Written next to the timing artifact so
  # the caller can pick it up regardless of the leg's verdict. Diagnostic
  # only: nothing reads this to decide anything.
  # `|| true` because this is a DIAGNOSTIC on the `set -e` critical path,
  # AHEAD of the production asserts below (review-113-6 [L]). A read-only or
  # full $BUILD would otherwise abort judge_timing before the skips/p99 gates
  # ever ran — a diagnostic failing a leg the app itself passed, which is the
  # settled iter-111 H1 rule. Losing the margin line is the correct failure.
  printf '%s %s\n' "$full_p99_ns" "$full_p99_ms" > "$2.p99" || true
  [ "$skips" = 0 ] || fail "$label: timing artifact reports $skips render skips (want 0)"
  [ "$rendered" = "$fr" ] || fail "$label: rendered $rendered != $fr"
  [ "$full_p99_ns" -lt "$P99_FULL_LIMIT_NS" ] \
    || fail "$label: p99 ${full_p99_ms} ms >= the 16.67 ms frame budget"
}

# judge_leg — the WHOLE per-leg device judgment, as a named production
# function (review-109-2 H2). It used to be an inline subshell body; a
# function is what `run_guarded` needs, and it also makes the leg
# judgment reusable by a tooth.
# judge_bstate — the BRIDGE-STATE comparison as a named production judge
# (review-109-3 M4). T2 used to run its own silent `cmp` and pin no
# diagnostic, so deleting the production comparison left the tooth green:
# the tooth proved `cmp` works, not that this check uses it. Now both the
# leg and T2 call THIS.
judge_bstate() { # <label> <device-bstate> <twin-bstate>
  local label="$1" dev="$2" twin="$3"
  cmp -s "$dev" "$twin" \
    || fail "$label: DEVICE BRIDGE-STATE != the host twin's"
}

judge_leg() { # <id> <name> <gdir> <mfarg> <frames> <stage-token>
  local id="$1" nm="$2" gd="$3" mf="$4" fr="$5" tok="$6"
  local twin_au twin_starts twin_stops
  judge_bstate "leg $id" "$BUILD/$id.dev-bstate.txt" "$BUILD/twin-$id/bstate.txt"
  judge_stream "device $id" "$BUILD/$id.dev-out.txt" "$id" "$nm" \
    "$gd" "$mf" "$BUILD"
  judge_timing "leg $id" "$BUILD/$id.dev-tim.txt" "$fr"
  twin_au="$(kv_lookup "$BUILD/twin-audio.txt" "$id")"
  # EXACT TWO-INTEGER GRAMMAR (review-109-1 M5): the old prefix/suffix
  # strip read `12 junk 7` as starts=12 stops=7.
  [[ "$twin_au" =~ ^([0-9]{1,12})\ ([0-9]{1,12})$ ]] \
    || grammar_die "leg $id: twin audio row fails its two-integer grammar ('$twin_au')"
  twin_starts="${BASH_REMATCH[1]}"; twin_stops="${BASH_REMATCH[2]}"
  judge_applog "leg $id" "$BUILD/$id.dev-applog.txt" "$fr" \
    "$AUDIO_RATE" "$AUDIO_SAMPLES" "$AUDIO_CHANNELS" "$tok" "$DSD" \
    "$twin_starts" "$twin_stops" "$id"
  # ATOMIC (review-109-1 M6): a leg result torn mid-write must not be
  # readable as a partial PASS. Both files are removed by the caller
  # BEFORE this runs (review-109-2 H2).
  printf 'PASS %s %s %s\n' "$full_p99_ns" "$full_p99_ms" "$au_starts" \
    > "$BUILD/$id.legresult.tmp"
  mv -f "$BUILD/$id.legresult.tmp" "$BUILD/$id.legresult"
}

stage_token() { # <stage id 0-5> -> music token
  case "$1" in
    0) echo battlefield ;; 1) echo ystory ;; 2) echo pstadium ;;
    3) echo dreamland ;; 4) echo fdest ;; 5) echo fountain ;;
    *) fail "stage id out of domain ('$1')" ;;
  esac
}

# --- [3/9] host twin build + twin legs ---------------------------------------
echo "== [3/9] host twin build + $N_GOLDENS_PIN twin legs (audio references) =="
CFLAGS_COMMON=(-ffp-contract=off -Wall -Wextra -Werror -I"$TABLES" -Iport/ryu
  -Iport/sim -Ioracle/qjs)
SIM_TUS=(
  port/sim/sim/sim_boot.c port/sim/sim/sim_tick.c port/sim/sim/sim_ser.c
  port/sim/sim/sim_data.c port/sim/sim/sim_ai_live.c
  port/sim/calib/canon.c port/sim/calib/player_canon.c
  port/sim/ai.c
  port/sim/physics.c port/sim/interpolated_collision.c
  port/sim/environmental_collision.c port/sim/hit_detection.c
  port/sim/article.c port/sim/action_state_shortcuts.c
  port/sim/ml_events.c port/sim/ml_fmt.c port/sim/ml_ser.c
  port/sim/ai_bridge.c port/sim/input/interpret_inputs.c
  port/sim/stages/moving_platforms.c port/sim/stages/ystory.c
  port/sim/stages/fountain.c
  port/sim/characters/shared/moves_index.c
  port/sim/characters/fox/moves_index.c
  port/sim/characters/falco/moves_index.c
  port/sim/characters/falcon/moves_index.c
  port/sim/characters/marth/moves_index.c
  port/sim/characters/marth/dancing_blade_combo.c
  port/sim/characters/marth/dancing_blade_air_mobility.c
  port/sim/characters/puff/moves_index.c
  port/sim/characters/puff/puff_multi_jump_drift.c
  port/sim/characters/puff/puff_next_jump.c
  "$TABLES/ml_tables.c" "$TABLES/ml_stages.c"
  oracle/qjs/sha256.c port/fdlibm/fdlibm.c
)
rm -f "$BUILD/raster.o" "$BUILD/foh_dev_headless"
cc -O3 "${CFLAGS_COMMON[@]}" -c "$GFX/raster.c" -o "$BUILD/raster.o"
cc -O2 "${CFLAGS_COMMON[@]}" -o "$BUILD/foh_dev_headless" \
  "$BUILD/raster.o" "$FOH/foh_dev.c" "$FOH/foh.c" "$FOH/foh_font.c" \
  "$FOH/foh_render.c" "$FOH/foh_persist.c" "$GFX/platform_headless.c" \
  "$GFX/anim1.c" "$GFX/gfx_render.c" "$GFX/gfx_target.c" \
  "$GFX/gfx_vfx.c" "$GFX/gfx_overlay.c" "$GFX/gfx_bg.c" \
  port/sim/target/target_play.c "$TABLES/ml_targets.c" \
  "${SIM_TUS[@]}" \
  port/sim/characters/{shared,fox,falco,falcon,marth,puff}/moves/*.c \
  -lm -lpthread
made "$BUILD/foh_dev_headless"

rm -f "$BUILD/twin-audio.txt"
for id in $PINNED_GOLDEN_SET; do
  golden_params "$id"
  rm -rf "$BUILD/twin-$id"; mkdir -p "$BUILD/twin-$id"
  extra=""
  [ "$cpu" = 1 ] && extra="--cpu-live"
  MLFK_PERSIST_DIR="$PWD/$BUILD/twin-$id/persist" \
  "$BUILD/foh_dev_headless" --p1 "$p1" --p2 "$p2" --p2type "$cpu" \
    --difficulty "$difficulty" --stage "$stage" \
    --bridge verify --simdata "$BUILD/simdata.txt" --seed "$seed" \
    --bstate-out "$BUILD/twin-$id/bstate.txt" \
    --trace "$BUILD/traces/$id.trace.txt" --frames "$frames" \
    --out "$BUILD/twin-$id/stream.txt" --timing "$BUILD/twin-$id/tim.txt" \
    --gfxdata "$GFXDATA_FROZEN" --vfxdata "$VFXDATA_FROZEN" \
    --glyphs "$VFXGLYPHS_FROZEN" --anim-dir "$TABLES" --legible \
    --sndpack "$BUILD/sndpack.bin" --music-manifest "$BUILD/mus-host.txt" \
    --pace 0 $extra 2> "$BUILD/twin-$id/log.txt"
  made "$BUILD/twin-$id/stream.txt" "$BUILD/twin-$id/bstate.txt" \
       "$BUILD/twin-$id/log.txt"
  judge_stream "twin $id" "$BUILD/twin-$id/stream.txt" "$id" "$name" \
    "$gdir" "$mfarg" "$BUILD/twin-$id"
  # headless: rate/samples/channels are 0 and callbacks never run, so the
  # twin contributes ONLY the mixer-side voice counters.
  parse_audio_summary "$BUILD/twin-$id/log.txt" 0 0 0
  # the direct-entry zero-tick contract holds on the HOST twin too
  assert_direct_bypass "$BUILD/twin-$id/log.txt" "twin $id"
  [ "$au_starts" != 0 ] || fail "twin $id: 0 voice starts — the SFX plane is dead, so a device leg matching it would prove nothing"
  # THE TWIN SIDE OF THE FROZEN PIN (review-109-2 M7): the twin must match
  # the reviewed table too, so a sim/mixer regression that moves twin and
  # device TOGETHER is caught here, before any device time is spent.
  twin_pin="$(kv_lookup "$SFX_PIN_FILE" "$id")"
  [[ "$twin_pin" =~ ^([0-9]{1,12})\ ([0-9]{1,12})$ ]] \
    || grammar_die "twin $id: frozen SFX pin row fails its grammar ('$twin_pin')"
  [ "$au_starts" = "${BASH_REMATCH[1]}" ] && [ "$au_stops" = "${BASH_REMATCH[2]}" ] \
    || fail "twin $id: voice starts/stops $au_starts/$au_stops != the frozen pin ${BASH_REMATCH[1]}/${BASH_REMATCH[2]} — the SFX plane changed; re-freeze SFX_PINS as a REVIEWED edit if that is intended"
  echo "$id $au_starts $au_stops" >> "$BUILD/twin-audio.txt"
  # the stage-track PROGRAM is real headless (it is a mixer call, not a
  # callback), so the track-identity witness is meaningful here too; the
  # streaming counters (refills/starves) are asserted on the DEVICE only.
  assert_mustrack "$BUILD/twin-$id/log.txt" "$AUDIO_OUT/audio/music" \
    "$(stage_token "$stage")" "twin $id"
done
# THE BYPASS-IDENTITY PROOF: the direct entry's launch state for g01 must
# equal the frozen MENU-DRIVEN witness byte for byte.
cmp "$BUILD/twin-g01/bstate.txt" "$G01_MENU_BSTATE" \
  || fail "direct-match BRIDGE-STATE != the frozen menu-driven f01 witness — the direct entry does not reach the same launch state the menus do"
echo "   $N_GOLDENS_PIN twin legs OK (streams verdict-exact; SFX/music live; g01 launch state == the frozen MENU witness)"

# --- [4/9] arm build + push ---------------------------------------------------
echo "== [4/9] armv7 build (shared rig stamp) + push + provenance =="
rig_arm_build
rig_stamp_rehash foh_device
dsh "rm -rf $DTMP $DSD && mkdir -p $DTMP $DSD"
# WHICH PLANE GOES WHERE, AND WHY (M4 task 14 increment 3d — the fix for the
# attributed stall class). $DTMP is /tmp = **tmpfs = RAM** on this device
# (measured: `tmpfs on /tmp type tmpfs (rw,relatime,size=131072k)`,
# MemTotal 57196 kB, swap = the 128 MB partition /dev/mmcblk0p3). tmpfs pages
# are anonymous: the ONLY way the kernel can reclaim them is to SWAP THEM OUT
# TO SD — which is the exact first link of the iter-110 chain (pswpout ->
# SD-IRQ storm -> involuntary-preemption burst -> a frame over budget).
#
# The suite used to push its ENTIRE artifact plane there: 33.96 MiB on a
# 57.2 MiB device = 59% of RAM, of which 12 traces (17.78 MB) + 5 ANIM1 bins
# (15.73 MB) + simdata (0.54 MB) are 96%. That is RIG-INDUCED pressure REAL
# PLAY DOES NOT HAVE — the shipped OPK reads its data plane from SD. Two
# independent signatures in the frozen logs fit it and nothing else: the
# pre-3d suite DELETED each leg's trace from tmpfs after its leg, so the
# plane shrank 1.48 MB per leg, and both run 5 and run 7 show d_pswpout
# collapsing to EXACTLY 0 on the last legs while pgpgin decays monotonically
# to an identical 5372 floor on both runs (more free RAM => the music/sndpack
# page cache survives => fewer re-reads).
# (HISTORY, stated in the past tense on purpose — review-113-3 [L]. The
# CURRENT cleanup removes only the STAGED tmpfs copy; the SD originals are
# deliberately retained. Do not "restore the delete" on SD to save space:
# that reintroduces the unsynced-writeback hazard M-2 removed.)
#
# So the big COLD artifacts move to $DSD (SD): read once at app boot / match
# setup, strictly BEFORE the paced loop — gfx_data_load/gfx_load_anim/
# gfx_vfx_load/gfx_glyphs_load run at foh_dev.c:1968-1972, after the ready
# marker at :1561, after which the warm-up primes render and only THEN does
# frame 1 of the timed match start. Their read latency lands in an unjudged
# window and their pages are clean file-backed cache the kernel can drop
# WITHOUT swapping. This also makes the measurement more representative of
# real play, not less.
# ONE EXCEPTION, stated here and not only at the leg site (review-113-3 [L],
# same false-invariant class as round-1's [L]): simdata.txt is read by
# sim_data_load at foh_dev.c:1528, which is BEFORE the ready marker — so
# relocating it can move ready latency. READY_TRIES=90 is therefore bound to
# MEASUREMENT (2 s ready across three device legs, .loop/m4-t113-smoke*.log),
# never to a claim that every relocated read happens post-ready.
#
# What deliberately STAYS in tmpfs: foh_device (exec'd per leg), the three
# frozen gfx/vfx/glyph planes (209 kB combined — 0.6% of the plane, not worth
# the diff), mus-dev.txt, the launcher/deadman state, and EVERY PER-LEG
# OUTPUT. The outputs must never move: they are written DURING the judged
# window, and SD writes there are precisely the hazard the [4/9] sync exists
# to keep out of the paced legs.
#
# ...and ONE trace at a time comes BACK to tmpfs: the current leg's, staged
# and re-verified immediately before its launch (review-113-1 M1, at the leg
# site). A partial SD read of a trace is the single relocation risk that
# degrades into a PASS rather than a failure, so that file is read from RAM.
# Resident tmpfs plane is therefore ~3.2 MB (binary + frozen planes + one
# trace + outputs) against the 33.96 MB this section used to push.
push_files=("$DEVB/foh_device" "$GFXDATA_FROZEN"
  "$VFXDATA_FROZEN" "$VFXGLYPHS_FROZEN" "$BUILD/mus-dev.txt")
push_sd=("$BUILD/simdata.txt")
for id in $PINNED_GOLDEN_SET; do
  push_sd+=("$BUILD/traces/$id.trace.txt")
done
# all five characters are fielded across the 12 goldens, so every ANIM1
# binary ships (named anim_<id>_<char>.bin by the pipeline; the list is
# defined and freshness-checked in section [2])
for a in $ANIM_BINS; do
  [ -f "$TABLES/$a" ] || fail "ANIM1 binary $a missing from $TABLES"
  push_sd+=("$TABLES/$a")
done
adb -s "$DEV" push "${push_files[@]}" "$DTMP/" >/dev/null
adb -s "$DEV" push "${push_sd[@]}" "$DSD/" >/dev/null
adb -s "$DEV" push "$BUILD/sndpack.bin" "$DSD/" >/dev/null
for tok in $MUS_TOKENS; do
  base="$(mus_pcm_for "$tok")"
  adb -s "$DEV" push "$AUDIO_OUT/audio/music/$base.pcm" "$DSD/" >/dev/null
done
rig_push_provenance "$DTMP" foh_device
dsh "chmod +x $DTMP/foh_device"
if [ "$ATTRIB" = 2 ]; then
  # sk_sampler comes out of the SAME shared arm build (it is already in
  # ARMBINS), so it is stamp-covered exactly like foh_device and gets the
  # same rehash-adjacent-to-push + provenance treatment.
  rig_stamp_rehash sk_sampler
  adb -s "$DEV" push "$DEVB/sk_sampler" "$DTMP/" >/dev/null
  rig_push_provenance "$DTMP" sk_sampler
  dsh "chmod +x $DTMP/sk_sampler"
  echo "   sk_sampler pushed (attrib level 2: 250 ms /proc windows)"
fi
# EVERY pushed input re-hashed on the device (never trust push rc). The
# relocation above changes WHERE a byte lands, never WHETHER it is verified:
# each artifact keeps its device-side sha comparison, at its new path.
for f in "$GFXDATA_FROZEN" "$VFXDATA_FROZEN" \
         "$VFXGLYPHS_FROZEN" "$BUILD/mus-dev.txt"; do
  hsum="$(rig_host_sha256 "$f")" || exit 1
  dsum="$(rig_dev_sha256 "$DTMP/${f##*/}")" || exit 1
  [ "$dsum" = "$hsum" ] || fail "pushed ${f##*/} sha mismatch"
done
hsum="$(rig_host_sha256 "$BUILD/simdata.txt")" || exit 1
dsum="$(rig_dev_sha256 "$DSD/simdata.txt")" || exit 1
[ "$dsum" = "$hsum" ] || fail "pushed simdata.txt sha mismatch"
for a in $ANIM_BINS; do
  hsum="$(rig_host_sha256 "$TABLES/$a")" || exit 1
  dsum="$(rig_dev_sha256 "$DSD/$a")" || exit 1
  [ "$dsum" = "$hsum" ] || fail "pushed $a sha mismatch"
done
# EVERY PUSHED TRACE (review-109-1 M8). The 12 input traces were pushed
# but never re-hashed on the device. The loader REPEATS the final
# available row, and the trace tails contain repeated rows, so a
# line-boundary truncation inside such a tail can still produce the exact
# golden stream — a truncated input would degrade silently into a pass
# rather than dying. Now every trace is host/device SHA-compared before a
# single leg executes.
for id in $PINNED_GOLDEN_SET; do
  hsum="$(rig_host_sha256 "$BUILD/traces/$id.trace.txt")" || exit 1
  dsum="$(rig_dev_sha256 "$DSD/$id.trace.txt")" || exit 1
  [ "$dsum" = "$hsum" ] || fail "pushed $id.trace.txt sha mismatch"
done
hsum="$(rig_host_sha256 "$BUILD/sndpack.bin")" || exit 1
dsum="$(rig_dev_sha256 "$DSD/sndpack.bin")" || exit 1
[ "$dsum" = "$hsum" ] || fail "pushed sndpack.bin sha mismatch"
for tok in $MUS_TOKENS; do
  base="$(mus_pcm_for "$tok")"
  hsum="$(rig_host_sha256 "$AUDIO_OUT/audio/music/$base.pcm")" || exit 1
  dsum="$(rig_dev_sha256 "$DSD/$base.pcm")" || exit 1
  [ "$dsum" = "$hsum" ] || fail "pushed $base.pcm sha mismatch"
done
# FLUSH THE SD WRITEBACK BEFORE ANY PACED LEG (review-109-1 M10 — both
# reviewed siblings do this and this engine did not). ~150 MB of music
# PCM lands on the SD card immediately before the paced matches start;
# without an explicit sync the kernel's deferred writeback (jbd2 /
# mmcqd / writeback, all resident on this single-core device) flushes
# DURING the legs and preempts the paced app. That is the leading
# attributed cause of the iter-109 measured frameskips: isolated 15-40 ms
# sim stalls with ~10 ms medians, MIGRATING between legs across passes
# (pass 1: g06 only; pass 2: g01 x3, g05, m02, g06 clean) and hitting the
# EARLIEST legs hardest — the signature of post-push writeback, not of
# game workload. RC-checked: an unflushed push must never silently
# contaminate the perf evidence.
dsh "sync" || fail "device sync after push failed (SD writeback not flushed — perf evidence would be contaminated)"
echo "   pushed + provenance verified (binary, data plane, sndpack, $nmus music tracks); SD writeback synced"

# --- [5/9] frontend park under a deadman -------------------------------------
echo "== [5/9] frontend park (deadman-armed) =="
rm -f "$BUILD/deadman.sh"
cat > "$BUILD/deadman.sh" << EOF
#!/bin/sh
# generated by check-device-fullgame.sh — frontend-park DEADMAN
#
# EVERY READ IS STATUS-CHECKED (review-109-4 H2). The previous version put
# its reads inside conditions — \`[ "\$(cat f 2>/dev/null)" = X ]\` — which
# turns an I/O FAILURE into a definite negative answer:
#   * a failed /proc/<pid>/comm read counted as "not low_bat_check", so an
#     unreadable LIVE daemon read as zero instances and the non-idempotent
#     init channel started a SECOND one;
#   * a failed nonce read read as "not our nonce" and skipped recovery
#     ENTIRELY, letting the deadman exit with the device still parked and
#     the daemon still stopped.
# Now: capture, check status, and tolerate ONLY a verified-vanished /proc
# entry (the genuine race). Anything unreadable while still present is
# unclassifiable, so the recovery state is RETAINED (marker left in place
# for the next run's rig_qd_normalize) and the reason is appended to
# \$DTMP/deadman.diag.
echo \$\$ > $DTMP/deadman.pid
i=0
while [ \$i -lt $DEADMAN_S ]; do
  sleep 2
  if [ -f $DTMP/deadman.cancel ]; then rm -f $DTMP/deadman.pid; exit 0; fi
  i=\$((i+2))
done
# THE LEASE GATE (review-109-5 H2, hardened per review-109-6 H1).
# A fixed horizon let this deadman fire while the host was still actively
# driving legs — e.g. between the host writing a leg's quiesce marker and
# stopping the daemon: the deadman would see one instance, CONSUME the
# marker and exit, and the host would then stop the daemon with neither
# watcher nor marker left. Firing is therefore gated on host SILENCE.
#
# THE GATE IS CLOCK-FREE (review-109-6 H1). It does NOT read the RTC and
# does NOT do age arithmetic: the host writes a MONOTONIC SEQUENCE NUMBER
# and this loop watches for CHANGE. A forward or backward RTC jump
# (this device's clock is not wall-synced) therefore cannot make a live
# host look silent, nor a silent host look alive. Renewal is atomic on the
# host side (write-temp + mv), and a transient unreadable/empty read is
# RETRIED rather than treated as proof of death — committing to fire on a
# half-written heartbeat was the round-6 race.
#
# Structure: gate -> claim -> RE-CHECK the lease -> fire. If the lease
# moved while the claim was being taken, the host is alive after all: the
# claim is released and the gate restarts. Nothing is irreversible before
# that final re-check.
# THE GATE IS UNBOUNDED (review-109-8 H1). Both loops used to carry caps
# — an outer round limit and a LEASE_GATE_MAX wall — and both were exits
# that ABANDONED the device: falling out of the outer loop skipped daemon
# restoration entirely, and the wall could force a fire against a lease
# that was still moving. The gate cap was inherited from the clock-BASED
# design as protection against a backwards RTC jump; the gate no longer
# reads a clock at all, so it protected nothing and only harmed. This
# watchdog now terminates on exactly two things: its cancellation, or a
# completed recovery it owned.
lprev=""
lstable=0
lbad=0
claimed=0
while :; do
  while :; do
    if [ -f $DTMP/deadman.cancel ]; then rm -f $DTMP/deadman.pid; exit 0; fi
    lv=\$(cat $DTMP/deadman.lease 2>/dev/null)
    lrc=\$?
    lok=1
    if [ \$lrc != 0 ]; then lok=0; fi
    # SESSION FENCING (review-109-8 H2): the heartbeat is
    # <run nonce>:<sequence>, not a bare counter. A lease carrying a
    # DIFFERENT run's nonce means a NEW run owns this device and has armed
    # its own watchdog — this one is superseded and must retire itself
    # rather than reason about a successor's claim (which is how a stale
    # deadman could delete a LIVE new host's claim and race its START).
    case "\$lv" in
      "$DM_NONCE":*) : ;;
      ''|*) lok=0 ;;
    esac
    case "\$lv" in
      "$DM_NONCE":*[!0-9]*) lok=0 ;;
    esac
    if [ \$lok = 0 ] && [ \$lrc = 0 ] && [ -n "\$lv" ]; then
      case "\$lv" in
        *:*)
          echo lease-foreign-run-superseded >> $DTMP/deadman.diag
          rm -f $DTMP/deadman.pid
          exit 0
          ;;
      esac
    fi
    if [ \$lok = 0 ]; then
      lbad=\$((lbad+1))
      if [ \$lbad -ge 5 ]; then echo lease-unreadable >> $DTMP/deadman.diag; break; fi
    else
      lbad=0
      if [ "\$lv" = "\$lprev" ]; then lstable=\$((lstable+2)); else lstable=0; lprev="\$lv"; fi
      if [ \$lstable -ge $LEASE_STALE_S ]; then break; fi
    fi
    sleep 2
  done
  # THE CLAIM, taken BEFORE the final re-check and before any daemon work
  # (review-109-6 H2): owned, so the host can tell a live holder from a
  # dead one, and never stolen from anybody.
  # ORPHANED-CLAIM RECLAIM (review-109-7 H1). A BUSY claim used to be
  # retried a fixed number of times and then given up on — which is
  # exactly the case that matters: a host that died holding the claim
  # AFTER stopping the daemon left the daemon down, the claim held, and
  # this watchdog walking away. The deadman now never abandons the device.
  # It distinguishes the holders symmetrically with the host's own rule:
  #   * DM:<pid>  -> another deadman is recovering: wait, do not touch it;
  #   * HOST:...  -> a host holds it. The host's OWN liveness proof is the
  #     lease, and this code only runs after the lease has been UNCHANGED
  #     for the whole silence window, so a still-held host claim is an
  #     ORPHAN. Re-confirm the lease is still frozen, then reclaim it.
  # Every reclaim is recorded in deadman.diag, and the daemon plane is
  # re-derived by lbc_count afterwards — nothing is inferred from a claim.
  ctry=0
  claimed=0
  standdown=0
  while :; do
    if mkdir $DTMP/qd.claim 2>/dev/null; then
      # status-checked publication (review-109-8 L): an unowned claim is
      # worse than none — nobody would release it and the next actor would
      # have to take it over.
      if printf 'DM:%s\n' \$\$ > $DTMP/qd.claim/owner; then
        claimed=1
        break
      fi
      echo claim-owner-write-failed >> $DTMP/deadman.diag
      rm -rf $DTMP/qd.claim
      sleep 2
      continue
    fi
    if [ -f $DTMP/deadman.cancel ]; then rm -f $DTMP/deadman.pid; exit 0; fi
    co=\$(cat $DTMP/qd.claim/owner 2>/dev/null)
    corc=\$?
    ctry=\$((ctry+1))
    if [ \$ctry -ge 10 ]; then
      lvz=\$(cat $DTMP/deadman.lease 2>/dev/null)
      lvzrc=\$?
      case "\$co" in
        DM:*)
          echo claim-busy-other-deadman >> $DTMP/deadman.diag
          ;;
        *)
          # HOST:... , an unreadable owner, or no owner at all: reclaim ONLY
          # while the host is still provably silent (its lease unmoved).
          if [ \$lvzrc != 0 ] || [ "\$lvz" = "\$lprev" ]; then
            echo claim-orphan-reclaimed >> $DTMP/deadman.diag
            rm -rf $DTMP/qd.claim
          else
            echo lease-resumed-standdown >> $DTMP/deadman.diag
            lstable=0
            lprev="\$lvz"
            standdown=1
            break
          fi
          ;;
      esac
      if [ \$corc != 0 ]; then echo claim-owner-unreadable >> $DTMP/deadman.diag; fi
      ctry=0
    fi
    echo claim-busy-observing >> $DTMP/deadman.diag
    sleep 2
  done
  # a stand-down decided inside the claim loop restarts the gate with a
  # fresh silence window (the host proved liveness while we waited)
  if [ \$standdown = 1 ]; then
    if [ \$claimed = 1 ]; then rm -rf $DTMP/qd.claim; claimed=0; fi
    continue
  fi
  # FINAL RE-CHECK: did the host prove liveness while we were claiming?
  lv2=\$(cat $DTMP/deadman.lease 2>/dev/null)
  lrc2=\$?
  if [ \$lrc2 = 0 ] && [ -n "\$lv2" ] && [ "\$lv2" != "\$lprev" ]; then
    echo lease-resumed-standdown >> $DTMP/deadman.diag
    if [ \$claimed = 1 ]; then rm -rf $DTMP/qd.claim; claimed=0; fi
    lstable=0
    lprev="\$lv2"
    continue
  fi
  break
done
# lbc_count -> lbc_n (instances), lbc_bad (unclassifiable entries)
# WHOLE-SCAN RETRY (review-109-7 L, mirroring the shared rig_comm_pids
# rule): a pid that genuinely vanished is simply absent from the NEXT
# snapshot, while one that is live-but-unreadable keeps reporting — so a
# failed read is never downgraded by a single boolean probe. Only an
# all-readable snapshot is accepted; otherwise lbc_bad stays nonzero and
# every caller RETAINS the quiesce marker.
lbc_count() {
  lbc_try=0
  while [ \$lbc_try -lt 3 ]; do
    lbc_try=\$((lbc_try+1))
    lbc_scan
    if [ \$lbc_bad = 0 ]; then return; fi
    sleep 1
  done
}
lbc_scan() {
  lbc_n=0
  lbc_bad=0
  for pd in /proc/[0-9]*; do
    c=\$(cat \$pd/comm 2>/dev/null)
    crc=\$?
    if [ \$crc != 0 ]; then
      # UNCONDITIONAL (review-109-8 L): a \`[ -d \$pd ]\` re-probe is itself
      # false on a stat failure, so the old form could accept an
      # incomplete snapshot as clean. Every failed read is unclassifiable;
      # the whole-scan retry above is what resolves a genuinely vanished
      # pid (it is simply absent from the next snapshot).
      lbc_bad=\$((lbc_bad+1))
      continue
    fi
    if [ "\$c" = low_bat_check ]; then lbc_n=\$((lbc_n+1)); fi
  done
}
# NONCE: bounded retry, then FAIL TOWARD RECOVERY when the file is present
# but unreadable. Every action below is scoped by THIS run's own
# nonce-named files, so acting on an unreadable nonce cannot touch another
# run's state; refusing to act would strand the device, which is worse.
nonce=""
nrc=1
j=0
while [ \$j -lt 3 ]; do
  nonce=\$(cat $DTMP/deadman.nonce 2>/dev/null)
  nrc=\$?
  if [ \$nrc = 0 ]; then break; fi
  sleep 1
  j=\$((j+1))
done
own=0
if [ \$nrc = 0 ]; then
  if [ "\$nonce" = "$DM_NONCE" ]; then own=1; fi
elif [ -e $DTMP/deadman.nonce ]; then
  echo nonce-unreadable-assuming-ours >> $DTMP/deadman.diag
  own=1
fi
if [ \$own = 1 ] && [ ! -f $DTMP/deadman.cancel ]; then
  echo fired > $DTMP/deadman.fired
  rm -f /mnt/disable_frontend
  if [ -f /mnt/disable_frontend ]; then echo unpark-failed >> $DTMP/deadman.diag; fi
  # The claim was taken (or refused) above, BEFORE the final lease
  # re-check — the daemon arm runs only while this deadman owns it. If the
  # host holds it, the marker is RETAINED and the daemon plane untouched
  # (fail-closed); the unpark above is idempotent and stays either way.
  if [ \$claimed != 1 ]; then echo claim-busy-daemon-arm-skipped >> $DTMP/deadman.diag; fi
  if [ \$claimed = 1 ]; then
  # DAEMON RESTORATION (review-109-2 H1): the deadman used to restore only
  # the frontend, so an abandoned run left low_bat_check stopped with no
  # recovery path at all. Keyed off the quiesce marker this run owns, so a
  # daemon we never stopped is never started.
  # EXACT CARDINALITY (review-109-3 H1), mirroring rig_daemon_restore's
  # host-side contract: this init channel is NOT idempotent, so start
  # ONLY from a measured zero, then re-count and clear the marker ONLY on
  # exactly one. Every other outcome RETAINS the marker so the next run's
  # rig_qd_normalize still sees "this daemon may be down".
  if [ -f $DTMP/qd.low_bat_check.$DM_NONCE ]; then
    lbc_count
    if [ \$lbc_bad != 0 ]; then
      echo lbc-scan-unreadable >> $DTMP/deadman.diag
    elif [ \$lbc_n = 1 ]; then
      rm -f $DTMP/qd.low_bat_check.$DM_NONCE
    elif [ \$lbc_n = 0 ]; then
      /etc/init.d/S12low-bat-check start >/dev/null 2>&1
      sleep 3
      lbc_count
      if [ \$lbc_bad = 0 ] && [ \$lbc_n = 1 ]; then
        rm -f $DTMP/qd.low_bat_check.$DM_NONCE
      else
        echo lbc-restore-unverified >> $DTMP/deadman.diag
      fi
    else
      echo lbc-multiple-instances >> $DTMP/deadman.diag
    fi
  fi
  fi
  # OWNED RELEASE (review-109-6 H2): only ever remove a claim whose owner
  # token is still ours — an unconditional rm could delete the HOST's
  # replacement claim (the ABA race).
  if [ \$claimed = 1 ]; then
    co=\$(cat $DTMP/qd.claim/owner 2>/dev/null)
    if [ "\$co" = "DM:\$\$" ]; then rm -rf $DTMP/qd.claim
    else echo claim-owner-changed-not-released >> $DTMP/deadman.diag; fi
  fi
  gp=\$(cat $DTMP/foh.pid.$DM_NONCE 2>/dev/null)
  grc=\$?
  if [ \$grc != 0 ]; then
    if [ -e $DTMP/foh.pid.$DM_NONCE ]; then echo pidfile-unreadable >> $DTMP/deadman.diag; fi
  else
    case "\$gp" in
      ''|*[!0-9]*) : ;;
      *)
        grep -q foh_device /proc/\$gp/cmdline 2>/dev/null
        krc=\$?
        if [ \$krc = 0 ]; then
          kill \$gp
        elif [ \$krc != 1 ]; then
          # a read ERROR: only a vanished /proc entry is benign. Present
          # but unreadable means the identity is unverifiable, and killing
          # an unverified pid is worse than leaving it — record, never kill.
          if [ -d /proc/\$gp ]; then echo cmdline-unreadable >> $DTMP/deadman.diag; fi
        fi
        ;;
    esac
  fi
fi
rm -f $DTMP/deadman.pid
exit 0
EOF
made "$BUILD/deadman.sh"
adb -s "$DEV" push "$BUILD/deadman.sh" "$DTMP/" >/dev/null
hsum="$(rig_host_sha256 "$BUILD/deadman.sh")" || exit 1
dsum="$(rig_dev_sha256 "$DTMP/deadman.sh")" || exit 1
[ "$dsum" = "$hsum" ] || fail "pushed deadman.sh sha mismatch"
# the lease must EXIST before the deadman starts (an absent lease means
# "the host is not proving liveness", which licenses firing)
lease_renew
dsh "printf '%s' '$DM_NONCE' > $DTMP/deadman.nonce; rm -f $DTMP/deadman.cancel $DTMP/deadman.fired $DTMP/deadman.diag"
dsh "setsid sh $DTMP/deadman.sh </dev/null >/dev/null 2>&1 & sleep 1"
dsh "test -f $DTMP/deadman.pid" >/dev/null 2>&1 || fail "park deadman did not start"
DEADMAN_ARMED=1
PARKED=1
dsh "touch /mnt/disable_frontend"
prc=0
dsh "pkill gmenu2x" >/dev/null 2>&1 || prc=$?
case "$prc" in
  0) : ;;
  1) echo "WARN: gmenu2x was not running at park time" >&2 ;;
  *) fail "pkill gmenu2x failed (rc $prc)" ;;
esac
# NOTE (review-109-4 M): low_bat_check is NOT stopped here. The quiesce
# window is PER LEG — see the leg loop below. A single batch-wide stop
# held the daemon down for the whole ~12-minute suite including every
# pull and every host-side judgment, and a host stall (or a laptop sleep)
# extended that to the deadman's 30-minute horizon, with no bracket
# assertion able to see it.
echo "   deadman armed (${DEADMAN_S}s) + frontend parked"

# --- [6/9] device legs --------------------------------------------------------
# attrib_snapshot <id> <pre|post> — the kernel-counter dumps the
# correlator's --pre-dir/--post-dir consume. Lifted from the reviewed
# check-skip-attrib.sh snapshot() (same five files, same names, same
# stderr-silencing rationale) and, like it, run ONLY outside the paced
# window. Every dump goes through the RC-checked dsh; `made` kills the
# run on an empty file, so a transport failure can never present as a
# quiet kernel.
attrib_snapshot() {
  local sid stag d
  sid="$1"
  stag="$2"
  d="$BUILD/$sid.attrib-$stag"
  rm -rf "$d"
  mkdir -p "$d"
  dsh "cat /proc/interrupts" > "$d/interrupts.txt"
  dsh "cat /proc/stat" > "$d/stat.txt"
  dsh "cat /proc/vmstat" > "$d/vmstat.txt"
  dsh "cat /proc/softirqs" > "$d/softirqs.txt"
  # cat's stderr is silenced: this adbd MERGES device stderr into the
  # stream, and a pid vanishing between glob and cat is expected churn,
  # not corruption (measured, check-skip-attrib.sh's same call).
  # `|| true` per ITERATION (review-110-1 finding 4): a bare loop
  # propagates only the LAST cat's status, so a pid vanishing at exactly
  # the wrong moment made dsh fail and aborted the whole host script
  # under set -e — while a REAL transport failure still yields an empty
  # file, which `made` below kills on.
  dsh 'for p in /proc/[0-9]*/stat; do cat "$p" 2>/dev/null || true; done' > "$d/pidstat.txt"
  made "$d/interrupts.txt" "$d/stat.txt" "$d/vmstat.txt" \
    "$d/softirqs.txt" "$d/pidstat.txt"
}

# --- SD-PRESSURE DIAGNOSTIC (M4 task 14 increment 3b) ------------------------
# Per-leg swap + SD-controller-IRQ deltas, at EVERY attrib level including 0.
# The iter-110 attribution named the stall class (involuntary-preemption
# bursts co-occurring with SD IRQ storms, driven by swap-out to the 128 MB
# swap FILE on the SD card of a 57 MB device), and the in-app fix targets the
# one file we stream inside the paced window. This arm is how a VANILLA pass
# carries the mechanism evidence — the level-2 sampler's signal without the
# level-2 sampler's in-run contamination.
#
# STRICTLY DIAGNOSTIC, three ways, none of which rely on reviewer goodwill:
#  (a) both snapshots are taken BETWEEN legs — pre BEFORE the leg's quiesce
#      window opens (deliberately OUTSIDE the bounded qstop->launch bracket,
#      review-111-2 H1: a diagnostic must not be able to spend that slack),
#      post after every pull — so no read of /proc ever lands inside a paced
#      frame. The safety property is "between legs, never in a paced frame";
#      it is NOT "inside the quiesce window", and moving pre back in there to
#      tighten the bracket would reintroduce H1 (review-111-4 L1);
#  (b) nothing judged reads these files: judge_leg, the verdict, and the
#      correlator (which keeps its OWN untouched attrib-{pre,post} dirs) have
#      no reference to `sddiag` anywhere;
#  (c) both entry points go through run_guarded, so a transport failure
#      prints "unavailable" and CANNOT fail a leg, and the printed line is
#      `   -> sd-diag ...`, which cannot match verify_m4.sh's anchored
#      FULLGAME_RE.
# It adds no teeth: the verdict grammar (including `teeth=21`) is unchanged.
sd_diag_snap() {
  local d="$BUILD/$1.sddiag-$2" rc=0
  rm -rf "$d"
  mkdir -p "$d"
  # THE RC IS THE CONTRACT, NOT THE BYTES ON DISK (review-113-1 M2). `dsh`
  # PRINTS whatever stdout it received BEFORE returning 71 for a missing RC
  # marker (adbsh.sh:49), so a FAILED snapshot can still leave a
  # plausible-looking /proc/vmstat behind. Any later reducer that merely
  # reparses those bytes would silently turn a transport failure into a
  # reassuring number — which is exactly how an evidence summary reports
  # `bar=met` off a snapshot that never happened. Both reads are rc-checked
  # and the OK marker is written LAST, only once everything held;
  # sd_diag_pswpout refuses without both markers.
  dsh "cat /proc/vmstat" > "$d/vmstat.txt" || rc=$?
  [ "$rc" = 0 ] || return "$rc"
  dsh "cat /proc/interrupts" > "$d/interrupts.txt" || rc=$?
  [ "$rc" = 0 ] || return "$rc"
  # non-empty is still required (an rc-0 but truncated read is not a snapshot)
  if [ ! -s "$d/vmstat.txt" ] || [ ! -s "$d/interrupts.txt" ]; then return 1; fi
  # THE MARKER CARRIES THIS RUN'S NONCE (review-113-5 [M]). $BUILD PERSISTS
  # ACROSS RUNS, so a presence-only marker is a stale-state trap: if this
  # function's own `rm -rf "$d"` ever fails, run_guarded contains that
  # failure, the current capture correctly reports unavailable — and a
  # PREVIOUS run's vmstat.txt plus its empty OK marker survive and satisfy a
  # presence test, letting the summary print `bar=met` off bytes from a
  # different run. A nonce the reducer must match byte-exactly closes that:
  # a stale marker can be present but can never be THIS run's.
  printf '%s\n' "$DM_NONCE" > "$d/OK"
}

# sd_diag_pswpout <id> — JUST the pswpout delta, for the [8/9] evidence-bar
# summary. Same fail-closed contract as sd_diag_report's reducer (exactly one
# key on each side, decimal, no decrease) so a summary can never invent a
# reassuring number; the caller prints `?` when this refuses.
sd_diag_pswpout() {
  # BOTH HALVES MUST HAVE SUCCEEDED (review-113-1 M2). The per-leg statuses
  # live in leg-local variables that are gone by the time the summary runs,
  # so the success markers sd_diag_snap writes LAST are what carry that fact
  # here. Without them a failed capture's leftover bytes could be reparsed
  # into a confident delta.
  # Presence is NOT enough — the marker must be THIS run's (review-113-5 [M]);
  # $BUILD persists across runs and a survived marker would otherwise certify
  # a previous run's bytes.
  local wantn
  wantn="$(printf '%s\n' "$DM_NONCE")"
  if [ ! -f "$BUILD/$1.sddiag-pre/OK" ] || [ ! -f "$BUILD/$1.sddiag-post/OK" ] \
     || [ "$(cat "$BUILD/$1.sddiag-pre/OK")" != "$wantn" ] \
     || [ "$(cat "$BUILD/$1.sddiag-post/OK")" != "$wantn" ]; then
    return 1
  fi
  # `bad` flag, not a bare `exit`: awk runs END even after `exit` inside a
  # rule, so the duplicate-key arm used to REFUSE (rc 1) and still PRINT a
  # number — safe only for callers that check rc. Same trap, same fix, as
  # sd_diag_report's reducers (iter-111 round 2 M3).
  awk '
    FNR == NR { if ($1 == "pswpout") { if (seen_p) bad = 1; p = $2; seen_p = 1 } next }
              { if ($1 == "pswpout") { if (seen_q) bad = 1; q = $2; seen_q = 1 } }
    END {
      if (bad || !seen_p || !seen_q) exit 1
      if (p !~ /^[0-9]+$/ || q !~ /^[0-9]+$/) exit 1
      if (q < p) exit 1
      print q - p
    }' "$BUILD/$1.sddiag-pre/vmstat.txt" "$BUILD/$1.sddiag-post/vmstat.txt"
}

# sd_diag_report <id> — one line of pre->post deltas. pswpin/pswpout are in
# PAGES, pgpgin/pgpgout in KB (kernel units, carried verbatim, never
# converted); mmc = the sunxi-mmc line of /proc/interrupts (the SD host
# controller whose IRQ storms the attribution named).
#
# BOTH reducers REFUSE rather than improvise (review-111-2 M3): a missing or
# duplicated key, a non-numeric counter, a changed CPU-column count, or a
# DECREASE (reboot, counter rollover, truncated pull) exits non-zero, which
# surfaces as `unavailable` instead of a plausible-looking zero. Evidence
# that quietly reads 0 when the producer changed shape is worse than no
# evidence — this line is the whole reason the pass carries a mechanism
# story. `exit` inside an awk rule still runs END, so failures set `bad` and
# END prints ONLY when bad is unset.
sd_diag_report() {
  local a="$BUILD/$1.sddiag-pre" b="$BUILD/$1.sddiag-post" v i
  v="$(awk -v keys="pswpout pswpin pgpgin pgpgout" '
        BEGIN { n = split(keys, K, " "); for (j = 1; j <= n; j++) want[K[j]] = 1 }
        FNR == NR { if ($1 in want) { if ($1 in p) bad = 1; p[$1] = $2 } next }
                  { if ($1 in want) { if ($1 in q) bad = 1; q[$1] = $2 } }
        END {
          if (bad) exit 1
          for (j = 1; j <= n; j++) {
            k = K[j]
            if (!(k in p) || !(k in q)) exit 1
            if (p[k] !~ /^[0-9]+$/ || q[k] !~ /^[0-9]+$/) exit 1
            d = q[k] - p[k]
            if (d < 0) exit 1
            s = s sprintf("%s%s=%d", (j > 1 ? " " : ""), k, d)
          }
          print s
        }' "$a/vmstat.txt" "$b/vmstat.txt")" || return 1
  # CPU-column count comes from the header line, so only the real per-CPU
  # counters are summed — the old `every numeric field` form also swept up
  # the hwirq number (constant, so the delta hid it) and would silently
  # absorb any future column.
  i="$(awk '
        function tot(nc,   s, k) {
          s = 0
          for (k = 2; k <= 1 + nc; k++) {
            if ($k !~ /^[0-9]+$/) { bad = 1; return 0 }
            s += $k
          }
          return s
        }
        FNR == NR { if (FNR == 1) { pn = NF; next }
                    if ($NF == "sunxi-mmc") { pm = tot(pn); pc++ } next }
                  { if (FNR == 1) { qn = NF; next }
                    if ($NF == "sunxi-mmc") { qm = tot(qn); qc++ } }
        END {
          if (bad || pc != 1 || qc != 1 || pn != qn || pn < 1) exit 1
          if (qm < pm) exit 1
          printf "mmcirq=%d\n", qm - pm
        }' "$a/interrupts.txt" "$b/interrupts.txt")" || return 1
  # explicit early return: `[ -n "$v" ] && [ -n "$i" ]` mid-function would be
  # errexit-EXEMPT (non-final command of an AND-OR list) and fall through to
  # print an empty delta line as if it were a measurement.
  if [ -z "$v" ] || [ -z "$i" ]; then return 1; fi
  echo "   -> sd-diag $1: $v $i"
}

echo "== [6/9] device legs: $N_GOLDENS_PIN paced matches, render+sfx+music live =="
P99_WORST_NS=0
P99_WORST_MS=""
# The MARGIN trackers (review-113-5 [L]) — every timing-valid leg, passing or
# not. P99_WORST_* keeps its existing meaning (worst among PASSING legs; it is
# what the verdict line prints), so the gate's semantics are untouched.
MARGIN_WORST_NS=0
MARGIN_WORST_MS=""
MARGIN_WORST_LEG=""
pass=0
FAILED_LEGS=""
for id in $PINNED_GOLDEN_SET; do
  golden_params "$id"
  tok="$(stage_token "$stage")"
  extra=""
  [ "$cpu" = 1 ] && extra=" --cpu-live"
  args="--p1 $p1 --p2 $p2 --p2type $cpu --difficulty $difficulty --stage $stage"
  # READ-ONCE INPUTS FROM SD, OUTPUTS TO TMPFS (increment 3d — see the [4/9]
  # plane note). --simdata and --anim-dir are consumed at boot and at match
  # setup, before the paced loop exists; --trace is STAGED back into tmpfs
  # per leg (M1 above); --out/--timing/--bstate-out/--ready-file are written
  # while the match runs and stay on tmpfs.
  #
  # READY_TIMEOUT, stated correctly (review-113-1 [L]): it is NOT true that
  # every relocated read happens after the ready marker — sim_data_load runs
  # at foh_dev.c:1528, BEFORE the marker is written at :1561, so relocating
  # simdata.txt genuinely can move the ready latency. READY_TRIES=90 is left
  # unchanged because it is bound to MEASUREMENT, not to that (false)
  # invariant: three hand-driven device legs on the relocated plane reached
  # the ready marker in 2 s (.loop/m4-t113-smoke{,2,3}.log), against a 90 s
  # allowance. The anim/trace reads, which are the large ones, do land after
  # the marker and inside the host's `sleep 55`.
  args="$args --bridge verify --simdata $DSD/simdata.txt --seed $seed"
  args="$args --bstate-out $DTMP/$id.bstate.txt"
  args="$args --trace $DTMP/$id.trace.txt --frames $frames"
  args="$args --out $DTMP/$id.out.txt --timing $DTMP/$id.tim.txt"
  args="$args --ready-file $DTMP/$id.ready --pace 1 --budget-ns $BUDGET_NS"
  args="$args --sndpack $DSD/sndpack.bin --music-manifest $DTMP/mus-dev.txt"
  args="$args --gfxdata $DTMP/gfxdata-frozen.txt --vfxdata $DTMP/vfxdata-frozen.txt"
  args="$args --glyphs $DTMP/vfxglyphs-frozen.txt --anim-dir $DSD --legible$extra"
  # the attribution arm (default off; see the MLFK_FULLGAME_ATTRIB block)
  [ "$ATTRIB" != 0 ] && args="$args --attrib $DTMP/$id.attrib.txt"
  rm -f "$BUILD/$id.argv"; printf '%s\n' "$args" > "$BUILD/$id.argv"
  # THE LIVE-AI BINDING, asserted per leg: an AIBRIDGE1 replay would
  # satisfy the stream but not the gate's live-C-AI clause.
  # ABSENCE THROUGH grep_count (review-109-3 M2): `! grep -q` treats a
  # read error (rc 2) exactly like "not found", so an unreadable argv
  # file would certify the live-AI binding it cannot even read.
  nbridge="$(grep_count '--ai-bridge' "$BUILD/$id.argv" "leg $id argv")"
  [ "$nbridge" = 0 ] \
    || fail "leg $id: device argv carries --ai-bridge (leg 1 requires the LIVE C ai.c)"
  rig_argv_assert_once "$BUILD/$id.argv" "--bridge" || exit 1
  rig_argv_assert_once "$BUILD/$id.argv" "--difficulty" || exit 1
  if [ "$cpu" = 1 ]; then
    rig_argv_assert_once "$BUILD/$id.argv" "--cpu-live" || exit 1
  else
    c="$(grep_count '--cpu-live' "$BUILD/$id.argv" "leg $id argv")"
    [ "$c" = 0 ] || fail "leg $id: non-CPU golden carries --cpu-live"
  fi

  # attrib level 2: the sampler starts in the SAME launcher, immediately
  # before the app, so its windows bracket the whole paced run (its own
  # stop file ends it; check-skip-attrib.sh's designed channel, verbatim).
  SAMPLER_LINES=""
  if [ "$ATTRIB" = 2 ]; then
    SAMPLER_LINES="rm -f $DTMP/sk.pid $DTMP/sk.stop $DTMP/$id.sampler.txt
setsid ./sk_sampler --out $DTMP/$id.sampler.txt --pid-file $DTMP/sk.pid \\
  --stop-file $DTMP/sk.stop --period-ms $SAMPLER_PERIOD_MS \\
  --max-samples $SAMPLER_MAX </dev/null >/dev/null 2>&1 &"
  fi
  rm -f "$BUILD/$id-launch.sh"
  cat > "$BUILD/$id-launch.sh" << EOF
#!/bin/sh
# generated by check-device-fullgame.sh — leg launcher for $id
cd $DTMP || exit 9
rm -rf $id.apprc $id.ready $id-persist foh.pid.$DM_NONCE app.start.ts app.end.ts
$SAMPLER_LINES
setsid sh -c 'date +%s > $DTMP/app.start.ts; MLFK_PERSIST_DIR=$DTMP/$id-persist ./foh_device $args \\
  2> $DTMP/$id.applog.txt & \\
  echo \$! > $DTMP/foh.pid.$DM_NONCE; \\
  wait \$!; arc=\$?; \\
  date +%s > $DTMP/app.end.ts; \\
  echo "RC=\$arc" > $DTMP/$id.apprc' \\
  </dev/null >/dev/null 2>&1 &
sleep 2
EOF
  made "$BUILD/$id-launch.sh"
  adb -s "$DEV" push "$BUILD/$id-launch.sh" "$DTMP/" >/dev/null
  hsum="$(rig_host_sha256 "$BUILD/$id-launch.sh")" || exit 1
  dsum="$(rig_dev_sha256 "$DTMP/$id-launch.sh")" || exit 1
  [ "$dsum" = "$hsum" ] || fail "pushed $id-launch.sh sha mismatch"
  dsh "chmod +x $DTMP/$id-launch.sh"
  # SD-PRESSURE DIAGNOSTIC, PRE HALF — TAKEN HERE, ABOVE THE STAGING COPY
  # (review-113-5 [M], found independently by two reviewers). The M1 staging
  # below allocates a 1,482,000-byte trace into tmpfs every leg = 362 ANON
  # pages, plus as much newly-populated SD page cache. That is the largest
  # RAM event the rig performs between legs, it is the one thing this
  # increment ADDED, and with the pre snapshot below it, it landed in the gap
  # between the previous leg's post and this leg's pre — a window no bracket
  # covers. 362 pages is 1.8x BAR_PAGES, so `bar=met` could be reported by a
  # suite displacing more than the bar in a window the bar cannot see, caused
  # by this change's own machinery. The per-leg deltas now TILE the staging.
  # Still strictly OUTSIDE the quiesce window (which opens at the qd. marker
  # below): this moves the snapshot FURTHER from that bracket, not into it,
  # so the settled iter-111 H1 ruling is untouched and the arm stays
  # decision-inert. The existing disclosure below already states that the
  # pre->post delta deliberately spans more than the paced window.
  run_guarded sdpre sd_diag_snap "$id" pre
  [ "$sdpre" = 0 ] || echo "   -> sd-diag $id: pre snapshot unavailable (rc $sdpre)"
  # STAGE THIS LEG'S TRACE INTO TMPFS, RE-VERIFIED (review-113-1 M1).
  # The trace is the one relocated artifact whose PARTIAL read degrades
  # silently into a PASS instead of a failure: load_trace (foh_dev.c:385)
  # treats a read error as EOF, only a WHOLLY empty trace is rejected
  # (:423), and the frame loop REPEATS the final loaded row (:2182) — and
  # s01/s02 genuinely end in long runs of an identical final row, so a
  # truncated read can still emit the exact conforming stream. On tmpfs
  # that risk did not exist; moving the file to SD created it, and the
  # [4/9] sha proves only the bytes that landed before leg 1, not the read
  # the app performs 10 legs later.
  # So the leg's own trace is copied SD -> tmpfs and sha-compared against
  # the HOST file immediately before launch, and the app reads it from RAM.
  # Cost: ONE 1.48 MB trace resident at a time instead of all twelve
  # (17.78 MB) — 96% of the relocation win kept, integrity restored.
  dsh "cp $DSD/$id.trace.txt $DTMP/$id.trace.txt" \
    || fail "leg $id: staging the trace SD -> tmpfs failed"
  hsum="$(rig_host_sha256 "$BUILD/traces/$id.trace.txt")" || exit 1
  dsum="$(rig_dev_sha256 "$DTMP/$id.trace.txt")" || exit 1
  [ "$dsum" = "$hsum" ] \
    || fail "leg $id: staged trace sha mismatch — the SD -> tmpfs copy is not the pushed bytes"

  echo "== leg $id ($name, stage $stage/$tok, cpu=$cpu difficulty=$difficulty)"
  # SD-pressure diagnostic, PRE half. Taken BEFORE the quiesce window opens,
  # never inside it (review-111-2 H1): rig_quiesce_bracket_assert bounds the
  # daemon-stop -> app-start gap by QW_PRE_SLACK_S, and run_guarded contains
  # a failure's STATUS but not its LATENCY — two slow adb round trips inside
  # that bracket could fail a leg the app itself passed, which is exactly
  # what a diagnostic must never be able to do. Consequence, stated so it is
  # never read as signal: the pre->post deltas therefore span the daemon
  # stop AND its restore (the same disclosure attrib_snapshot's post side
  # already carries).
  # PER-LEG QUIESCE WINDOW (review-109-4 M; the reviewed sibling protocol,
  # check-device-foh.sh:1183-1229 / check-device-target.sh, adopted whole).
  # The daemon goes down IMMEDIATELY before this leg's launch and comes
  # back as the FIRST post-app action — before any pull, hash, cmp or
  # host-side judgment. The MARKER is written first (rig_qd_normalize and
  # the deadman both key their restore off it, so an orphaned run's daemon
  # is still recoverable), LBC_STOPPED is armed BEFORE the stop call so a
  # failure inside it still restores, and the coupled qrestore stamp +
  # rig_quiesce_bracket_assert make the window's exactness MEASURED
  # evidence rather than a claim.
  # RENEW THE LEASE FIRST (review-109-5 H2): proving the host is alive
  # BEFORE the marker exists is what makes the marker-write -> daemon-stop
  # window safe from a concurrently firing deadman.
  lease_renew
  # ...and HOLD THE RECOVERY CLAIM across marker -> stop -> restore ->
  # marker removal (review-109-6 H1's last clause). The lease alone leaves
  # a narrow window in which a deadman that has already decided to fire
  # could consume this leg's freshly written marker; owning the claim for
  # the whole critical section closes it, because the deadman's daemon arm
  # is claim-gated and its final lease re-check sees this host alive.
  rig_qd_claim || exit 1
  LBC_CLAIMED=1
  dsh "printf '' > $DTMP/qd.low_bat_check.$DM_NONCE"
  LBC_STOPPED=1
  lbc_pid="$(rig_daemon_stop low_bat_check)"
  dsh "date +%s > $DTMP/qstop.ts"
  echo "   low_bat_check quiesced for this leg only (pid $lbc_pid)"
  # PRE snapshot INSIDE the quiesce window and immediately before launch,
  # so the pre/post bracket sees the same daemon state the leg runs under.
  [ "$ATTRIB" != 0 ] && attrib_snapshot "$id" pre
  dsh "sh -lc $DTMP/$id-launch.sh"
  ready=0
  for _ in $(seq 1 "$READY_TRIES"); do
    if dsh "test -f $DTMP/$id.ready" >/dev/null 2>&1; then ready=1; break; fi
    sleep 1
  done
  if [ "$ready" != 1 ]; then
    dsh "cat $DTMP/$id.applog.txt" >&2 || true
    fail "leg $id: app ready marker never appeared (${READY_TRIES}s)"
  fi
  sleep 55
  done_f=0
  for _ in $(seq 1 40); do
    if dsh "test -f $DTMP/$id.apprc" >/dev/null 2>&1; then done_f=1; break; fi
    sleep 2
  done
  [ "$done_f" = 1 ] || fail "leg $id: the app never wrote its rc marker"
  # RESTORE-FIRST (review-109-4 M): the daemon comes back BEFORE any pull,
  # hash or judgment. MARKER LIFECYCLE (review-109-4 H2): the quiesce
  # marker means "this daemon MAY still be down", so it is removed and its
  # ABSENCE VERIFIED *before* LBC_STOPPED is cleared — otherwise a
  # transport loss right here leaves the deadman acting on stale recovery
  # state and starting a second (non-idempotent) instance.
  # REVALIDATE OWNERSHIP FIRST (review-109-9 H1). The app ran for ~60 s and
  # the poll loops above can block arbitrarily longer (a host stall or a
  # laptop sleep), which is exactly long enough for the deadman to declare
  # this host dead and take the claim. `LBC_CLAIMED` is an in-memory flag
  # and cannot survive that, so the DEVICE's own owner token is re-read
  # before any daemon-plane action.
  lease_renew
  rig_qd_reassert || exit 1
  LBC_CLAIMED=1
  if rig_daemon_restore low_bat_check /etc/init.d/S12low-bat-check "$DTMP/qrestore.ts"; then
    dsh "rm -f $DTMP/qd.low_bat_check.$DM_NONCE"
    dsh "test ! -f $DTMP/qd.low_bat_check.$DM_NONCE"
    LBC_STOPPED=0
    # the critical section is over: release the claim we have held since
    # before the marker was written (review-109-6 H1)
    rig_qd_unclaim
    LBC_CLAIMED=0
  else
    fail "leg $id: low_bat_check did not verify as running after its per-leg restore"
  fi
  qstop_ts="$(rig_dev_ts "$DTMP/qstop.ts")" || exit 1
  appstart_ts="$(rig_dev_ts "$DTMP/app.start.ts")" || exit 1
  append_ts="$(rig_dev_ts "$DTMP/app.end.ts")" || exit 1
  qrestore_ts="$(rig_dev_ts "$DTMP/qrestore.ts")" || exit 1
  rig_quiesce_bracket_assert "fullgame $id low_bat_check" \
    "$qstop_ts" "$appstart_ts" "$append_ts" "$qrestore_ts" \
    "$QW_PRE_SLACK_S" "$QW_POST_SLACK_S" || exit 1
  # BYTE-EXACT RC MARKER (review-109-1 M6). `$(dsh "cat ...")` strips ALL
  # trailing newlines, so a TORN `RC=0` (write interrupted before its
  # newline) was indistinguishable from the canonical `RC=0\n` — a
  # partially written marker read as a clean exit. The marker is now
  # pulled (sha-verified by pullv) and byte-compared against exactly
  # `RC=0\n`.
  pullv "$DTMP/$id.apprc" "$BUILD/$id.apprc"
  printf 'RC=0\n' > "$BUILD/$id.apprc.want"
  cmp -s "$BUILD/$id.apprc" "$BUILD/$id.apprc.want" \
    || fail "leg $id: rc marker is not exactly 'RC=0<LF>' (bytes: $(od -An -c < "$BUILD/$id.apprc" | tr -s ' \n' ' '))"

  pullv "$DTMP/$id.out.txt" "$BUILD/$id.dev-out.txt"
  pullv "$DTMP/$id.tim.txt" "$BUILD/$id.dev-tim.txt"
  pullv "$DTMP/$id.bstate.txt" "$BUILD/$id.dev-bstate.txt"
  pullv "$DTMP/$id.applog.txt" "$BUILD/$id.dev-applog.txt"
  if [ "$ATTRIB" != 0 ]; then
    # POST snapshot AFTER the daemon restore, following the reviewed
    # restore-first discipline (review-109-4) rather than delaying it for
    # five more adb round trips. CONSEQUENCE, stated so it is never read
    # as signal: the run-level `PID|`/`IRQ|` deltas this produces include
    # the low_bat_check stop AND its restore. The PER-FRAME `EV|` counters
    # — which are what actually attributes a stall — are unaffected: they
    # come from the app's own rusage inside the paced window.
    if [ "$ATTRIB" = 2 ]; then
      # stop through the sampler's DESIGNED channel and verify it exited
      # (a killed sampler leaves a truncated capture, which the
      # correlator's SAMPLER DONE terminator check would reject anyway —
      # this makes the failure say what actually happened).
      dsh "touch $DTMP/sk.stop"
      skgone=0
      for _ in $(seq 1 8); do
        if dsh "test ! -f $DTMP/sk.pid" >/dev/null 2>&1; then skgone=1; break; fi
        sleep 1
      done
      [ "$skgone" = 1 ] \
        || fail "leg $id: sk_sampler did not exit within 8 s of its stop file"
      pullv "$DTMP/$id.sampler.txt" "$BUILD/$id.dev-sampler.txt"
      dsh "rm -f $DTMP/$id.sampler.txt $DTMP/sk.stop"
    fi
    attrib_snapshot "$id" post
    pullv "$DTMP/$id.attrib.txt" "$BUILD/$id.dev-attrib.txt"
    dsh "rm -f $DTMP/$id.attrib.txt"
  fi
  # POST half. The pre status is carried SEPARATELY (review-111-2 M2): with
  # one shared variable a successful post overwrote a failed pre, and the
  # report then differenced an empty/stale pre dir into confident-looking
  # numbers. Report only when BOTH halves landed.
  run_guarded sdpost sd_diag_snap "$id" post
  if [ "$sdpre" = 0 ] && [ "$sdpost" = 0 ]; then
    run_guarded sdrep sd_diag_report "$id"
    [ "$sdrep" = 0 ] || echo "   -> sd-diag $id: unusable snapshots (rc $sdrep)"
  else
    echo "   -> sd-diag $id: unavailable (pre rc $sdpre, post rc $sdpost)"
  fi
  # The STAGED tmpfs copy goes (that is what keeps only one trace resident);
  # the SD original is deliberately KEPT (review-112 supplemental M-2): on SD
  # an `rm` is an UNSYNCED WRITE whose deferred writeback would land inside
  # the NEXT leg's paced window — precisely the SD-writeback-during-a-leg
  # mechanism this increment exists to remove. Deleting the write beats
  # adding a `sync` after it, and staleness is impossible anyway ([4/9] wipes
  # and recreates $DSD, and every trace is sha-compared before leg 1 and
  # again at its own staging step). These three removals are tmpfs-only.
  dsh "rm -f $DTMP/$id.out.txt $DTMP/$id.tim.txt $DTMP/$id.trace.txt"

  # JUDGMENTS RUN IN A SUBSHELL AND ARE COLLECT-AND-CONTINUE.
  # Rationale (budget, not leniency): the suite is 12 paced legs and a
  # device pass is expensive. Aborting the whole run at the first failing
  # leg throws away the evidence for every leg after it — a single
  # environmental frameskip on leg 6 cost the g07/g08/m01/m02/s01/s02
  # evidence entirely. So a JUDGMENT failure is recorded and the run
  # continues to gather the rest; the suite still FAILS at the end unless
  # all 12 legs passed, and no assertion is weakened in any way. Device
  # TRANSPORT/launch failures above still abort immediately (a wedged
  # device makes the remaining legs meaningless).
  # BOTH the result and its temp are removed BEFORE the judgment
  # (review-109-2 H2): otherwise a leftover read-only `.legresult.tmp`
  # from a prior run makes this run's write fail, and the `mv` then
  # renames the STALE record into place as this run's PASS.
  # `.p99` JOINS THIS LIST (review-113-6 [M]). $BUILD persists across runs and
  # judge_leg runs bstate -> stream -> timing, so ANY failure before
  # judge_timing used to leave a PREVIOUS run's `.p99` in place for the
  # presence-only fold below to report as this run's margin — the same
  # stale-artifact class the sd-diag OK markers just had, with different
  # bytes. Clearing it here makes "file present" mean "this judgment wrote
  # it", which is exactly what the fold assumes.
  rm -f "$BUILD/$id.legresult" "$BUILD/$id.legresult.tmp" \
        "$BUILD/$id.dev-tim.txt.p99" \
        "$BUILD/$id.dev-applog.txt.mustrack.got" \
        "$BUILD/$id.dev-applog.txt.mustrack.want"
  lrc=0
  run_guarded lrc judge_leg "$id" "$name" "$gdir" "$mfarg" "$frames" "$tok"
  # THE MARGIN, gathered from EVERY leg whose timing parsed (review-113-5 [L]).
  # Read outside the pass branch on purpose: a leg that failed `skips==0` still
  # measured a real p99, and it may be the narrowest one in the run.
  if [ -f "$BUILD/$id.dev-tim.txt.p99" ]; then
    mline="$(head -1 "$BUILD/$id.dev-tim.txt.p99")"
    if [[ "$mline" =~ ^([0-9]{1,15})\ ([0-9]+\.[0-9]{3})$ ]] \
       && [ "${BASH_REMATCH[1]}" -gt "$MARGIN_WORST_NS" ]; then
      MARGIN_WORST_NS="${BASH_REMATCH[1]}"
      MARGIN_WORST_MS="${BASH_REMATCH[2]}"
      MARGIN_WORST_LEG="$id"
    fi
  fi
  if [ "$lrc" = 0 ] && [ -f "$BUILD/$id.legresult" ]; then
    # ONE line plus EXACTLY one terminating newline, validated BEFORE any
    # field extraction (review-109-1 M6: command substitution strips
    # trailing newlines, so a torn record or extra blank lines still
    # matched the field regex).
    nl_terminated "$BUILD/$id.legresult" "leg $id result"
    lrn="$(wc -l < "$BUILD/$id.legresult" | tr -d ' ')"
    [ "$lrn" = 1 ] \
      || fail "leg $id: leg result carries $lrn lines (want exactly 1)"
    lres="$(head -1 "$BUILD/$id.legresult")"
    [[ "$lres" =~ ^PASS\ ([0-9]+)\ ([0-9]+\.[0-9]{3})\ ([0-9]+)$ ]] \
      || fail "leg $id: leg result line fails its grammar ('$lres')"
    lp99ns="${BASH_REMATCH[1]}"; lp99ms="${BASH_REMATCH[2]}"; lstarts="${BASH_REMATCH[3]}"
    if [ "$lp99ns" -gt "$P99_WORST_NS" ]; then
      P99_WORST_NS="$lp99ns"; P99_WORST_MS="$lp99ms"
    fi
    pass=$((pass + 1))
    echo "   -> leg $id OK (stream verdict-exact, p99 ${lp99ms} ms, skips 0, underruns 0, starves 0, starts $lstarts)"
  else
    printf 'FAIL rc=%s\n' "$lrc" > "$BUILD/$id.legresult"
    FAILED_LEGS="$FAILED_LEGS $id"
    echo "   -> leg $id JUDGMENT FAILED (rc $lrc) — continuing to gather the remaining legs' evidence"
  fi
  # ATTRIBUTION runs AFTER the leg verdict and INDEPENDENTLY of it: the
  # leg we most need attributed is a FAILING one, so this must not sit
  # behind the pass branch. The correlator
  # (port/sim/device/skip-attrib/correlate-skips.js) runs UNMODIFIED over
  # UNMODIFIED artifacts — the fullgame `--timing` grammar and gfx_app's
  # are the same four columns, verified against a real artifact before
  # this arm was written, so no adapter exists anywhere in this path.
  # It is NOT a pinned gate producer, and this comment used to claim it
  # was (corrected iter 117, driver ruling 4). Measured facts behind the
  # correction: it is absent from verify_m4.sh's REQUIRED_PRODUCERS and
  # from port/sim/device/m4-freeze-manifest.txt; no VERDICT: GO has ever
  # covered its current bytes (its arcs — .loop/review-{73,76,78}-1.log,
  # diff packets review-{73-74,76,78}-diff.txt — all closed NO-GO, and
  # its current sha256 appears only in the NO-GO .loop/review-78-1.log);
  # and this whole arm is unreachable under the gate, which never sets
  # MLFK_FULLGAME_ATTRIB, so ATTRIB is 0 (line 153) and no gate verdict
  # can depend on it. An ARMED run is structurally non-authoritative
  # anyway (the ` [ATTRIB-ARMED]` suffix cannot match FULLGAME_RE).
  # Pinning it would therefore add an unapproved row that refuses the
  # gate for evidence the gate does not use — the honest fix is this
  # comment, not a manufactured status.
  if [ "$ATTRIB" != 0 ]; then
    rm -f "$BUILD/$id.corr.txt"
    corr_args=(--timing "$BUILD/$id.dev-tim.txt"
      --attrib "$BUILD/$id.dev-attrib.txt"
      --frames "$frames" --budget-ns "$BUDGET_NS"
      --pre-dir "$BUILD/$id.attrib-pre"
      --post-dir "$BUILD/$id.attrib-post")
    [ "$ATTRIB" = 2 ] && corr_args+=(--sampler "$BUILD/$id.dev-sampler.txt")
    if node "$SKA/correlate-skips.js" "${corr_args[@]}" > "$BUILD/$id.corr.txt"; then
      made "$BUILD/$id.corr.txt"
      # the correlator's own completeness seal (the iter-62 judge pattern)
      tail -c 18 "$BUILD/$id.corr.txt" | cmp -s - <(printf 'attrib_complete=1\n') \
        || fail "leg $id: correlator output does not end with the exact bytes 'attrib_complete=1<LF>' — corrupt evidence"
      # printed with plain commands, never inside another command's
      # argument list (review-109-4 L6: no decision-bearing — or
      # evidence-bearing — pipeline hidden inside a larger success).
      echo "   -> attrib $id (correlator summary + every EV row):"
      grep -E '^(skips|over_budget_frames|late_start_frames|events|nivcsw_total|nvcsw_total|minflt_total|majflt_total|nivcsw_per_frame_median|minflt_per_frame_median|mono_raw_drift_ns)=' \
        "$BUILD/$id.corr.txt" | sed 's/^/      /' || true
      grep -E '^EV\|' "$BUILD/$id.corr.txt" | sed 's/^/      /' || true
      if [ "$ATTRIB" = 2 ]; then
        # uncovered events are a COVERAGE GAP, not an absence of signal
        # (review-110-1 finding 3) — count them out loud.
        # through grep_count (review-110-3 finding 4): the bare
        # `|| nowin=0` form accepted grep's "no match" (rc 1) AND every
        # real read failure (rc 2+) as "zero uncovered events" — a
        # coverage gap and an unreadable file read identically.
        nowin="$(grep_count '^EV\|.*\|win=none$' "$BUILD/$id.corr.txt" "leg $id correlator output")"
        echo "      events_without_sampler_window=$nowin"
      fi
    else
      fail "leg $id: correlate-skips.js failed on the pulled attribution evidence"
    fi
  fi
  # renew at the END of the leg too, so a long host-side judgment can never
  # age the lease into the deadman's firing window (review-109-5 H2)
  lease_renew
done
# THE EVIDENCE-BAR SUMMARY (review-112-2 H1, dispositioned — see below).
# The driver's bar for increment 3d is "d_pswpout ≈0 on ALL legs INCLUDING
# leg 1, and skips==0 on all 12". `skips==0` is already a hard pin of the
# verdict. The SWAP half is diagnostic, and it stays diagnostic ON PURPOSE:
#   - the sd-diag arm was ruled DECISION-INERT by a previous review round
#     (iter-111 round 2 H1: a diagnostic must never be able to fail a leg the
#     app itself passed), and making it verdict-bearing reverses that;
#   - the M4 EXIT gate's leg [1] conditions are frozen in CLAUDE.md and
#     verify_m4.sh (untouchable this iteration). A swap-counter pin that the
#     gate spec does not have would fail a genuinely conforming run whenever
#     the DEVICE's ambient state is busy — a strictly worse gate;
#   - the driver rejected re-scoping pins; silently ADDING one is the same
#     class of unilateral move.
# What the finding is RIGHT about is that a printed-only number is easy to
# skip past. So the bar is computed and stated HERE, mechanically, right
# before the verdict: nobody can read this run's output and be unclear on
# whether the swap pressure is gone. `bar=met` vs `bar=UNMET (closed by luck)`
# is the writer's report language, decided by the numbers rather than narrated.
#
# ALL TWELVE LEGS, not just the first two (increment 3d). The iter-111 bar
# watched legs 1-2 because that is where the displacement transient landed
# while the plane lived in tmpfs. Increment 3d claims the pressure is gone
# EVERYWHERE, so the summary reports the WORST leg and names it: a bar that
# only ever looked at g01/g02 could be met by a run whose pressure had merely
# MOVED down the suite.
#
# PLACED BEFORE THE FAILED_LEGS GATE (review-112 supplemental M-1). It used to
# sit in `[8/9]`, downstream of a `fail` — so on exactly the runs that need
# these numbers most (an 11/12 like run 5) the whole table was suppressed.
#
# WHAT THIS BAR DOES *NOT* PROVE (review-112 supplemental M-3/M-4, and the
# driver's amended bar): `pswpout -> skip` is CORRELATIONAL. Run 7 contains
# its own counterexample — s01 swapped ZERO pages and posted the suite's
# WORST p99 (15.977 ms), while g01 swapped 4495 and posted 13.862 with no
# skips. So a flat bar shows the RIG-INDUCED memory pressure is gone and the
# measurement is representative of real play; it is NOT on its own evidence
# that the stall class is closed. That evidence is two zero-skip passes, one
# of them ATTRIB-ARMED so any surviving stall is ATTRIBUTED rather than merely
# absent. `BAR_PAGES` is likewise derived from PRE-relocation data and
# self-obsoletes once the plane is off tmpfs (supplemental [L], accepted): it
# is an A/B threshold for this increment, not a permanent pin.
bar_worst=-1
bar_worst_leg=""
bar_series=""
bar_unknown=0
for id in $PINNED_GOLDEN_SET; do
  v="$(sd_diag_pswpout "$id")" || v="?"
  bar_series="$bar_series $id=$v"
  # a refused/missing reading is UNKNOWN, never a beautifully flat 0 — the
  # same fail-closed discipline the sd_diag reducers use.
  # THE LENGTH GUARD IS NOT COSMETIC (review-113-1 M4): bash 3.2's `[ -gt ]`
  # is a SIGNED 64-bit compare, and a digit-only value too large to represent
  # makes `test` return 2. Inside an `if` condition errexit is suppressed, so
  # that leg would be SILENTLY DROPPED from bar_worst and an earlier small
  # value could still print `bar=met`. 15 digits is far above any physical
  # page count and far below the int64 limit, so anything longer is a
  # corrupt/absurd reading and is marked unknown rather than discarded.
  case "$v" in
    ''|*[!0-9]*) bar_unknown=1 ;;
    *) if [ "${#v}" -gt 15 ]; then
         bar_unknown=1
       elif [ "$v" -gt "$bar_worst" ]; then
         bar_worst="$v"; bar_worst_leg="$id"
       fi ;;
  esac
done
bar_verdict="UNMET (12/12 would be CLOSED BY LUCK — swap-out pressure is still live on the judged legs)"
# explicit `if`, never an `&&` chain: a false AND-OR list is the LAST command
# of its case arm, and under `set -e` that kills the run (the script's
# ERREXIT DISCIPLINE note). A diagnostic must not be able to do that.
#
# A KNOWN BREACH OUTRANKS AN UNKNOWN (review-113-1 M3). If one leg is
# unreadable while another is measured ABOVE the threshold, the outcome is
# already decided — the pre-registered verdict is HYPOTHESIS REFUTED — and
# printing `UNKNOWN` there would hide a fact the run actually established.
# So: breach first, then unknown, then met.
if [ "$bar_worst" -gt "$BAR_PAGES" ]; then
  : # the UNMET default already says it
elif [ "$bar_unknown" = 1 ] || [ "$bar_worst" -lt 0 ]; then
  bar_verdict="UNKNOWN (a leg's sd-diag snapshots are missing or refused)"
else
  bar_verdict="met (every leg <= $BAR_PAGES pages; worst $bar_worst_leg=$bar_worst)"
fi
echo "   -> evidence bar (tmpfs-relocation): worst leg $bar_worst_leg d_pswpout=$bar_worst, flat<=$BAR_PAGES pages -> bar=$bar_verdict"
echo "   -> evidence bar per leg:$bar_series"
# THE p99 MARGIN, printed before the gate so a FAILING run still states it
# (review-113-5 [L]; the driver's standing instruction is that this margin is
# reported prominently, because it is the gate's real standing risk).
if [ -n "$MARGIN_WORST_MS" ]; then
  margin_ns=$((P99_FULL_LIMIT_NS - MARGIN_WORST_NS))
  echo "   -> p99 margin (ALL timing-valid legs): worst $MARGIN_WORST_LEG p99=${MARGIN_WORST_MS} ms vs 16.670 ms budget -> ${margin_ns} ns headroom"
else
  echo "   -> p99 margin: no timing-valid leg produced a p99"
fi

if [ -n "$FAILED_LEGS" ]; then
  fail "legs failed judgment:$FAILED_LEGS ($pass/$N_GOLDENS_PIN passed) — see the per-leg diagnostics above"
fi
[ "$pass" = "$N_GOLDENS_PIN" ] || fail "only $pass/$N_GOLDENS_PIN legs passed"
[ -n "$P99_WORST_MS" ] || fail "no p99 was recorded"

# --- park restore + deadman cancel --------------------------------------------
# low_bat_check is NOT restored here: each leg restored it as its own
# first post-app action and verified the marker gone (review-109-4 M/H2).
# This assertion is the standing control that the per-leg discipline held
# — a leg that somehow left the daemon quiesced must never reach the
# deadman cancellation, because cancelling would remove the last backstop
# for a daemon that is still down.
[ "$LBC_STOPPED" = 0 ] \
  || fail "low_bat_check is still marked quiesced after the last leg — the per-leg restore discipline was violated; refusing to cancel the deadman"
dsh "rm -f /mnt/disable_frontend"
dsh "test ! -f /mnt/disable_frontend"
PARKED=0
dsh "touch $DTMP/deadman.cancel"
dmgone=0
for _ in $(seq 1 6); do
  if dsh "test ! -f $DTMP/deadman.pid" >/dev/null 2>&1; then dmgone=1; break; fi
  sleep 2
done
[ "$dmgone" = 1 ] || fail "park deadman did not exit within 12s of cancellation"
dsh "test ! -f $DTMP/deadman.fired" >/dev/null 2>&1 \
  || fail "park deadman FIRED during a healthy run"
DEADMAN_ARMED=0
echo "   frontend restored; deadman cancelled without firing"

# --- [7/9] teeth (host-side, on COPIES; committed bytes never edited) --------
echo "== [7/9] teeth (COPIES only; every tooth drives a PRODUCTION judge) =="
teeth=0
T=$BUILD/teeth
rm -rf "$T"; mkdir -p "$T"
# THE NAMED EXECUTION LEDGER (review-109-5 L6). A counter alone is not
# enough: the count is an AGGREGATE, so removing an uncounted component
# (T1's raw half) or one T21 sub-case still arrives at 21. Every tooth AND
# every named component appends its own name here as it passes, and the
# sorted ledger is compared BYTE-EXACTLY against the frozen inventory
# below — so a skipped, renamed, deleted or duplicated component dies even
# when the aggregate is untouched. Adding a tooth is a REVIEWED edit that
# re-freezes both the ledger and TEETH_PIN in the same change.
TLEDGER="$T/ledger.txt"
: > "$TLEDGER"
tooth_ledger() { printf '%s\n' "$1" >> "$TLEDGER"; }
TEETH_LEDGER_FROZEN="T10
T11
T12
T13
T14
T15
T16
T17
T18
T19
T1b
T1raw
T2
T20
T21-digit-leading
T21-empty
T21-errexit-restore
T21-reserved-eopt
T21-reserved-rc
T21-reserved-rcvar
T21-uppercase-special
T3
T4
T5
T6
T7
T8
T9"

# tooth_expect <name> <want-rc> <want-diagnostic-re> -- <production judge> [args...]
# THE TEETH CONTRACT (review-109-1 M11). Two defects are closed here:
#   (1) T3-T9 accepted ARBITRARY nonzero status, so a missing, renamed or
#       crashed judge (rc 127 / 139) counted as proof that the judge
#       works — the tooth was measuring nothing;
#   (2) T5-T7 REIMPLEMENTED the per-leg policy assertion inline, so
#       deleting the real assertion left the tooth green.
# A tooth now pins the EXACT expected failure: the exact exit class
# (1 == policy via fail(), 2 == grammar/corruption via grammar_die()) AND
# an anchored FULL-LINE diagnostic — the repo's standing whitelist-grammar
# rule (PROCESS §3) applied to the teeth themselves. And it drives the
# PRODUCTION judge — the same function the legs call — on a perturbed COPY.
tooth_expect() {
  local name="$1" wrc="$2" wre="$3"; shift 3
  [ "$1" = "--" ] || fail "$name: tooth_expect needs '--' before the judge command"
  shift
  local errf="$T/$name.err" rc=0 n
  rm -f "$errf"
  # MEASURED-CORRECT GUARD (review-109-2 H2): round 1 used
  # `( set -e; "$@" ) || rc=$?`, and on bash 3.2 the inner `set -e` does
  # NOT re-arm inside an AND-OR operand — a judge's inner
  # `x="$(grammar_exactly_one ...)"` death was IGNORED and the judge
  # stumbled on to a different, later diagnostic, silently defeating this
  # tooth's pinned-diagnostic check. run_guarded runs the subshell
  # STANDALONE with the parent at `set +e` and captures its real status.
  set +e
  ( set -e; "$@" ) >/dev/null 2>"$errf"
  rc=$?
  set -e
  made "$errf"
  [ "$rc" = "$wrc" ] \
    || fail "$name: the production judge exited $rc, want EXACTLY $wrc (accepting any nonzero would also accept a crashed or missing judge) — diagnostic: $(tr '\n' '|' < "$errf")"
  n="$(grep_count "$wre" "$errf" "$name")"
  [ "$n" = 1 ] \
    || fail "$name: the judge's diagnostic does not match its pinned anchored full line ($n matches) — got: $(tr '\n' '|' < "$errf")"
  tooth_ledger "$name"
  teeth=$((teeth + 1))
}

# the twin reference row for g01, used by every applog tooth
tooth_twin="$(kv_lookup "$BUILD/twin-audio.txt" g01)"
[[ "$tooth_twin" =~ ^([0-9]{1,12})\ ([0-9]{1,12})$ ]] \
  || grammar_die "teeth: twin audio row for g01 fails its grammar"
TT_STARTS="${BASH_REMATCH[1]}"; TT_STOPS="${BASH_REMATCH[2]}"
# judge_applog bound to g01's real parameters — the teeth perturb the LOG,
# never the judge's arguments.
tooth_applog() { # <label> <log>
  judge_applog "$1" "$2" "$FRAMES_PIN" "$AUDIO_RATE" "$AUDIO_SAMPLES" \
    "$AUDIO_CHANNELS" battlefield "$DSD" "$TT_STARTS" "$TT_STOPS" g01
}

# T1 — nibble-flip a pulled device stream COPY -> verify-stream rc 2.
golden_params g01
node -e '
  const fs = require("fs");
  const [src, dst] = process.argv.slice(1);
  const L = fs.readFileSync(src, "utf8").split("\n");
  for (let i = 0; i < L.length; i++) {
    const m = /^F (\d+) ([0-9a-f]{64})$/.exec(L[i]);
    if (m && m[1] === "1800") {
      const h = m[2].split("");
      h[0] = h[0] === "0" ? "1" : "0";
      L[i] = "F 1800 " + h.join("");
      fs.writeFileSync(dst, L.join("\n"));
      process.exit(0);
    }
  }
  console.error("T1: F 1800 line not found"); process.exit(1);
' "$BUILD/g01.dev-out.txt" "$T/t1.out.txt"
made "$T/t1.out.txt"
cmp -s "$T/t1.out.txt" "$BUILD/g01.dev-out.txt" && fail "T1: nibble flip was a no-op (dead tooth)"
# T1a — the RAW judge's own divergence class (verify-stream.js exits 2).
node "$SIMD/wrap-run.js" g01 "$T/t1.out.txt" "$T/t1.run.json" >/dev/null
rc=0
node oracle/harness/verify-stream.js "$T/t1.run.json" \
  "oracle/goldens/$name.sha256.json" >/dev/null 2>&1 || rc=$?
[ "$rc" = 2 ] || fail "T1: nibble-flipped device stream rc $rc (want the divergence class 2)"
# NOT COUNTED (review-109-4 L5): T1's raw probe and T1b below are ONE
# composite tooth over the SAME perturbed copy — the raw half pins
# verify-stream.js's own divergence class (exact rc 2, above) and is a
# precondition of the production-judge half, which pins rc + an anchored
# diagnostic. Counting both inflated the advertised total to 22 while the
# script and its report claimed 21; the composite is the reconciliation,
# and $TEETH_PIN freezes the resulting inventory exactly.
tooth_ledger "T1raw"
echo "    T1 OK: nibble-flipped device stream dies in verify-stream (rc 2) [composite with T1b]"
# T1b — the SAME perturbation through the PRODUCTION stream judge the
# legs call, so the tooth also proves judge_stream's wiring (M11).
mkdir -p "$T/t1b"
tooth_expect T1b 1 \
  '^FULLGAME FAIL: tooth1b: stream does not match the frozen golden$' \
  -- judge_stream "tooth1b" "$T/t1.out.txt" g01 "$name" "$gdir" "$mfarg" "$T/t1b"
echo "    T1b OK: the same flip dies in the PRODUCTION judge_stream (rc 1, pinned diagnostic)"

# T2 — perturbed BRIDGE-STATE COPY -> the PRODUCTION judge_bstate dies
# (review-109-3 M4: this used to be a private `cmp`, so it stayed green
# even with the production comparison deleted).
sed 's/difficulty=3/difficulty=4/' "$BUILD/g01.dev-bstate.txt" > "$T/t2.bstate.txt"
cmp -s "$T/t2.bstate.txt" "$BUILD/g01.dev-bstate.txt" && fail "T2: substitution was a no-op (dead tooth)"
# The apostrophe is matched LITERALLY (review-109-4 L5): the previous
# pattern spelled it `twin.s`, and `.` is an ERE wildcard — the tooth
# would have accepted `twin?s`, `twinXs`, any one-character corruption of
# the production diagnostic it exists to pin.
tooth_expect T2 1 \
  "^FULLGAME FAIL: tooth2: DEVICE BRIDGE-STATE != the host twin's\$" \
  -- judge_bstate "tooth2" "$T/t2.bstate.txt" "$BUILD/twin-g01/bstate.txt"
echo "    T2 OK: a perturbed BRIDGE-STATE copy dies in the PRODUCTION judge_bstate (rc 1, pinned diagnostic)"

# T3 — a skipped frame in a timing COPY -> judge_timing's skips==0 dies.
awk 'NR==7{print $1" 0 0 1"; next}{print}' "$BUILD/g01.dev-tim.txt" > "$T/t3.tim.txt"
cmp -s "$T/t3.tim.txt" "$BUILD/g01.dev-tim.txt" && fail "T3: skip injection was a no-op (dead tooth)"
tooth_expect T3 1 \
  '^FULLGAME FAIL: tooth3: timing artifact reports 1 render skips \(want 0\)$' \
  -- judge_timing "tooth3" "$T/t3.tim.txt" "$FRAMES_PIN"
echo "    T3 OK: an injected render skip dies in the production timing judge (rc 1, pinned diagnostic)"

# T4 — an over-budget frame in a timing COPY -> the p99 assert dies.
# render/present are DELIBERATELY UNEQUAL (1000 vs 2000). They used to be
# both 1000, which made this fixture the ONLY artifact in the corpus with a
# nonzero render==present rate (50%) — and that lone synthetic data point was
# then used to argue the render/present pair could not carry a fractional
# alias bound. It could: the bound is the MAX_COL_ALIAS_FRAC entry for the
# render/present pair in judge-render-timing.js's alias arm (cited by NAME,
# not by line — the previous :335 citation had already rotted), and it
# costs zero judgments (.loop/review-117-jrt-regression-r7.log [R8],
# review-117-jrt-7 [M1] / -7o [M3]). Unequal values keep this tooth exercising
# the arm it is aimed at — the p99 assert — instead of colliding with an
# unrelated one.
awk -v n="$P99_FULL_LIMIT_NS" '{ if (NR % 2 == 0) print (n+5000000)" 1000 2000 0"; else print }' \
  "$BUILD/g01.dev-tim.txt" > "$T/t4.tim.txt"
cmp -s "$T/t4.tim.txt" "$BUILD/g01.dev-tim.txt" && fail "T4: inflation was a no-op (dead tooth)"
tooth_expect T4 1 \
  '^FULLGAME FAIL: tooth4: p99 [0-9]{1,9}\.[0-9]{3} ms >= the 16\.67 ms frame budget$' \
  -- judge_timing "tooth4" "$T/t4.tim.txt" "$FRAMES_PIN"
echo "    T4 OK: an over-budget p99 dies in the production timing judge (rc 1, pinned diagnostic)"

# T5 — underruns=1 in an app-log COPY -> the audio parse+assert dies.
sed 's/ 0 underruns,/ 1 underruns,/' "$BUILD/g01.dev-applog.txt" > "$T/t5.log"
cmp -s "$T/t5.log" "$BUILD/g01.dev-applog.txt" && fail "T5: substitution was a no-op (dead tooth)"
tooth_expect T5 1 \
  '^FULLGAME FAIL: tooth5: 1 audio underruns \(want 0\)$' \
  -- tooth_applog "tooth5" "$T/t5.log"
echo "    T5 OK: a nonzero audio underrun count dies in the PRODUCTION judge_applog (rc 1, pinned diagnostic)"

# T6 — starves=1 in an app-log COPY -> the music assert dies.
sed -E 's/^(foh_dev music: [0-9]+ out frames, )0 starves,/\11 starves,/' \
  "$BUILD/g01.dev-applog.txt" > "$T/t6.log"
cmp -s "$T/t6.log" "$BUILD/g01.dev-applog.txt" && fail "T6: substitution was a no-op (dead tooth)"
tooth_expect T6 1 \
  '^FULLGAME FAIL: tooth6: 1 music starves \(want 0\)$' \
  -- tooth_applog "tooth6" "$T/t6.log"
echo "    T6 OK: a nonzero music starve count dies in the PRODUCTION judge_applog (rc 1, pinned diagnostic)"

# T7 — a failed present in a match-summary COPY -> the presentfails assert dies.
sed -E 's/^(foh_dev match: [0-9]+ frames, [0-9]+ render skips, )0 failed presents,/\11 failed presents,/' \
  "$BUILD/g01.dev-applog.txt" > "$T/t7.log"
cmp -s "$T/t7.log" "$BUILD/g01.dev-applog.txt" && fail "T7: substitution was a no-op (dead tooth)"
tooth_expect T7 1 \
  '^FULLGAME FAIL: tooth7: match summary reports 1 failed presents$' \
  -- tooth_applog "tooth7" "$T/t7.log"
echo "    T7 OK: a nonzero failed-present count dies in the PRODUCTION judge_applog (rc 1, pinned diagnostic)"

# T8 — a mustrack line naming the WRONG track -> the track-identity assert
# dies (a menu track that never switched must never pass as stage music).
sed 's/to=battlefield on=1/to=menu on=1/' "$BUILD/g01.dev-applog.txt" > "$T/t8.log"
cmp -s "$T/t8.log" "$BUILD/g01.dev-applog.txt" && fail "T8: substitution was a no-op (dead tooth)"
tooth_expect T8 1 \
  '^FULLGAME FAIL: tooth8: mustrack inventory != the expected menu program \+ battlefield stage switch \(got: .*\)$' \
  -- tooth_applog "tooth8" "$T/t8.log"
echo "    T8 OK: a wrong-track music publish dies in the PRODUCTION judge_applog (rc 1, pinned diagnostic)"

# T9 — an EXTRA, later track switch appended to an app-log COPY. This is
# the hole the old counting assert had: the expected stage publish was
# present, so `count == 1` passed while the music plane had in fact
# switched again. The exact-inventory assert refuses it.
cp "$BUILD/g01.dev-applog.txt" "$T/t9.log"
printf 'foh_dev mustrack: from=battlefield to=menu on=1 pcm=%s/menu.pcm\n' "$DSD" >> "$T/t9.log"
cmp -s "$T/t9.log" "$BUILD/g01.dev-applog.txt" && fail "T9: append was a no-op (dead tooth)"
tooth_expect T9 1 \
  '^FULLGAME FAIL: tooth9: 3 mustrack publishes in .* \(want exactly 2: the menu program then the battlefield stage switch\)$' \
  -- tooth_applog "tooth9" "$T/t9.log"
echo "    T9 OK: an EXTRA later track switch dies in the exact mustrack inventory (rc 1, pinned diagnostic)"

# T10 — a TORN app log (final line written without its newline) -> the
# grammar class, rc 2. Command substitution strips trailing newlines, so
# without nl_terminated a torn producer line is indistinguishable from a
# canonical one.
tsz="$(wc -c < "$BUILD/g01.dev-applog.txt" | tr -d ' ')"
head -c "$((tsz - 1))" "$BUILD/g01.dev-applog.txt" > "$T/t10.log"
cmp -s "$T/t10.log" "$BUILD/g01.dev-applog.txt" && fail "T10: truncation was a no-op (dead tooth)"
tooth_expect T10 2 \
  '^FULLGAME FAIL: tooth10 foh summary: .*/t10\.log does not end in a newline \(torn write\)$' \
  -- tooth_applog "tooth10" "$T/t10.log"
echo "    T10 OK: a torn (newline-less) app log dies in the GRAMMAR class (rc 2, pinned diagnostic)"

# T11 — a truncated app log that LOSES the summary lines entirely: the
# anchored grammar must refuse it rather than read absence as zero.
head -c 200 "$BUILD/g01.dev-applog.txt" > "$T/t11.log"
printf '\n' >> "$T/t11.log"   # newline-terminated, so ONLY the missing-line class is under test
tooth_expect T11 2 \
  '^FULLGAME FAIL: tooth11 foh summary: 0 lines match the anchored grammar exactly \(want 1\) in .*/t11\.log$' \
  -- tooth_applog "tooth11" "$T/t11.log"
echo "    T11 OK: an app log missing its summaries dies in the GRAMMAR class (rc 2, pinned diagnostic)"

# T12 — a torn DUPLICATE summary line alongside the valid one. This is the
# half of the whitelist-grammar rule the parsers were missing: counting
# only EXACT matches read the pair as "exactly 1 — clean".
cp "$BUILD/g01.dev-applog.txt" "$T/t12.log"
printf 'foh_dev match: 3600 frames, 0 render sk\n' >> "$T/t12.log"
tooth_expect T12 2 \
  '^FULLGAME FAIL: match summary: 2 lines RESEMBLE the producer grammar but only 1 matches it exactly — torn/duplicated evidence in .*/t12\.log$' \
  -- tooth_applog "tooth12" "$T/t12.log"
echo "    T12 OK: a torn DUPLICATE summary line dies as corruption, not as 'the other one was fine' (rc 2)"

# T20 — the DIRECT-ENTRY ZERO-TICK CONTRACT (review-109-3 L8). A direct
# run that executed even ONE FOH tick leaves the bridge state and every
# frozen stream untouched, so only this assertion can see it.
sed -E 's/^(foh_dev foh: )0( ticks,)/\11\2/' "$BUILD/g01.dev-applog.txt" > "$T/t20.log"
cmp -s "$T/t20.log" "$BUILD/g01.dev-applog.txt" && fail "T20: substitution was a no-op (dead tooth)"
tooth_expect T20 2 \
  '^FULLGAME FAIL: tooth20 foh summary: 0 lines match the anchored grammar exactly \(want 1\) in .*/t20\.log$' \
  -- tooth_applog "tooth20" "$T/t20.log"
echo "    T20 OK: a direct run that executed a FOH tick dies on the zero-tick contract (rc 2)"

# T21 — run_guarded's OUT-PARAMETER INTERFACE GUARD (review-109-3 L6,
# widened by review-109-4 L4/L5). A caller naming the out-parameter after
# the helper's own local would silently keep rc 0 — i.e. swallow every
# failure the guard exists to catch — and a digit-leading name used to run
# the guarded command BEFORE `printf -v` refused the identifier, breaking
# the pre-execution contract. All THREE reserved aliases, the digit-leading
# class and the empty name are now covered, each pinning rc 1 AND its
# anchored diagnostic (the round-4 objection: the old control discarded
# stderr, so any rc-1 death passed it).
# Each case pins its OWN exact, END-ANCHORED diagnostic (review-109-5 L5:
# one shared unanchored pattern let any refusal satisfy any case) and is
# recorded in the execution ledger by name, so deleting one case is caught
# by the ledger comparison even though the aggregate count is unchanged
# (review-109-5 L6).
t21_case() { # <label> <bad-name> <anchored-full-line-re>
  local lbl="$1" bad="$2" wre="$3" trc=0 terrf="$T/t21-$1.err" tn
  rm -f "$terrf"
  set +e
  ( run_guarded "$bad" echo RAN ) >"$T/t21-$1.out" 2>"$terrf"
  trc=$?
  set -e
  made "$terrf"
  [ "$trc" = 1 ] \
    || fail "T21 ($lbl): run_guarded accepted the out-parameter name '$bad' (rc $trc, want 1)"
  tn="$(grep_count "$wre" "$terrf" "T21 $lbl")"
  [ "$tn" = 1 ] \
    || fail "T21 ($lbl): run_guarded's refusal diagnostic is not its pinned anchored full line ($tn matches) — got: $(tr '\n' '|' < "$terrf")"
  # PRE-EXECUTION: the guarded command must never have run
  [ -s "$T/t21-$1.out" ] \
    && fail "T21 ($lbl): the guarded command RAN before the name guard rejected '$bad' — the pre-execution contract is broken"
  tooth_ledger "T21-$lbl"
}
t21_case reserved-rc __rc \
  "^FULLGAME FAIL: run_guarded: reserved out-parameter name '__rc' would alias this function's own local\$"
t21_case reserved-rcvar __rcvar \
  "^FULLGAME FAIL: run_guarded: reserved out-parameter name '__rcvar' would alias this function's own local\$"
t21_case reserved-eopt __eopt \
  "^FULLGAME FAIL: run_guarded: reserved out-parameter name '__eopt' would alias this function's own local\$"
t21_case digit-leading 9rc \
  "^FULLGAME FAIL: run_guarded: out-parameter name '9rc' must start with a lowercase letter \(digits and bash's uppercase special variables cannot hold a status\)\$"
t21_case uppercase-special PIPESTATUS \
  "^FULLGAME FAIL: run_guarded: out-parameter name 'PIPESTATUS' must start with a lowercase letter \(digits and bash's uppercase special variables cannot hold a status\)\$"
t21_case empty "" \
  "^FULLGAME FAIL: run_guarded: empty out-parameter name\$"
# ...and the ERREXIT-RESTORATION half: a caller that entered with errexit
# DISABLED must leave run_guarded with it still disabled.
set +e
t21rc=0
run_guarded t21rc true
case "$-" in
  *e*) set -e
       fail "T21 (errexit-restore): run_guarded ENABLED errexit for a caller that had it disabled" ;;
esac
set -e
[ "$t21rc" = 0 ] || fail "T21 (errexit-restore): run_guarded reported rc $t21rc for a successful command"
tooth_ledger "T21-errexit-restore"
teeth=$((teeth + 1))
echo "    T21 OK: run_guarded refuses all 3 reserved aliases, digit-leading, an uppercase special and the empty out-parameter name (rc 1 + each case's own anchored diagnostic, none of them executing the command) and restores a disabled errexit"

# T13 — a DEAD-JUDGE control that isolates the EXACT-STATUS guard
# (review-109-2 L9). The round-1 version used a MISSING command, which
# also fails the diagnostic-regex check — so it stayed green even if the
# exact-rc comparison were deleted, i.e. it did not test what it claimed.
# This fake judge emits the EXPECTED diagnostic and exits 127, so the
# diagnostic check passes and ONLY the exact-status guard can reject it.
fake_judge_127() { echo "FULLGAME FAIL: tooth13: pinned diagnostic" >&2; return 127; }
rc=0
( tooth_expect T13probe 1 '^FULLGAME FAIL: tooth13: pinned diagnostic$' \
    -- fake_judge_127 ) >/dev/null 2>&1 || rc=$?
[ "$rc" = 1 ] || fail "T13: tooth_expect accepted a judge that produced the right diagnostic but exited 127 (rc $rc) — the EXACT-status guard is not load-bearing"
tooth_ledger "T13"
teeth=$((teeth + 1))
echo "    T13 OK: tooth_expect refuses rc 127 even with a matching diagnostic (the any-nonzero hole is closed)"

# T14 — the ERREXIT CONTROL (review-109-2 H2). An intermediate failure
# followed by a success must NOT read as a passing judgment. This is the
# exact shape that made round 1's `( set -e; ... ) || rc=$?` ineffective
# on bash 3.2; if run_guarded's pattern ever regresses, this fires.
mid_fail_then_succeed() { false; echo "SURVIVED-THE-FAILURE"; }
tcrc=0
run_guarded tcrc mid_fail_then_succeed
# EXACT status (review-109-4 L5): `!= 0` also accepted a crashed or missing
# function, i.e. the any-nonzero hole T13 closes elsewhere. `false` exits 1
# and errexit propagates that status verbatim, so 1 is the only right answer.
[ "$tcrc" = 1 ] \
  || fail "T14: a guarded subshell whose intermediate command failed reported rc $tcrc, want EXACTLY 1 — either the failure was SWALLOWED (rc 0, run_guarded's errexit discipline regressed and every leg judgment is unsound) or the control is measuring a different death"
tooth_ledger "T14"
teeth=$((teeth + 1))
echo "    T14 OK: an intermediate failure inside a guarded judgment is NOT swallowed (rc $tcrc)"

# T15/T16 — PROPER-PREFIX corruption (review-109-2 M4). A bare
# `foh_dev match:` / `foh_dev mustrack:` line alongside the canonical one
# resembles the producer but matches no grammar; the old loose patterns
# required a digit after the colon and so counted it as nothing at all.
cp "$BUILD/g01.dev-applog.txt" "$T/t15.log"
printf 'foh_dev match:\n' >> "$T/t15.log"
tooth_expect T15 2 \
  '^FULLGAME FAIL: match summary: 2 lines RESEMBLE the producer grammar but only 1 matches it exactly — torn/duplicated evidence in .*/t15\.log$' \
  -- tooth_applog "tooth15" "$T/t15.log"
echo "    T15 OK: a bare proper-prefix 'foh_dev match:' line dies as corruption (rc 2)"

cp "$BUILD/g01.dev-applog.txt" "$T/t16.log"
printf 'foh_dev mustrack:\n' >> "$T/t16.log"
tooth_expect T16 1 \
  '^FULLGAME FAIL: tooth16: 3 mustrack publishes in .* \(want exactly 2: the menu program then the battlefield stage switch\)$' \
  -- tooth_applog "tooth16" "$T/t16.log"
echo "    T16 OK: a bare proper-prefix 'foh_dev mustrack:' line dies in the exact inventory (rc 1)"

# T17/T18 — SUSTAINED LIVE AUDIO (review-109-2 H3). Music that stops
# after a single refill still reports starves=0 and a correct mustrack
# publish; only the out-frames/refill windows can see it. Likewise a
# collapsed callback count.
sed -E 's/^(foh_dev music: )[0-9]+( out frames, 0 starves, )[0-9]+( refills)/\144100\21\3/' \
  "$BUILD/g01.dev-applog.txt" > "$T/t17.log"
cmp -s "$T/t17.log" "$BUILD/g01.dev-applog.txt" && fail "T17: substitution was a no-op (dead tooth)"
tooth_expect T17 1 \
  '^FULLGAME FAIL: tooth17: 44100 music out frames outside the sustained-playback window \[2600000,2700000\] — the track did not stream for the whole match$' \
  -- tooth_applog "tooth17" "$T/t17.log"
echo "    T17 OK: music that stopped after ~1 s dies on the out-frames window (rc 1)"

sed -E 's/^(foh_dev audio: )[0-9]+( callbacks)/\112\2/' \
  "$BUILD/g01.dev-applog.txt" > "$T/t18.log"
cmp -s "$T/t18.log" "$BUILD/g01.dev-applog.txt" && fail "T18: substitution was a no-op (dead tooth)"
tooth_expect T18 1 \
  '^FULLGAME FAIL: tooth18: 12 audio callbacks outside the sustained-playback window \[5000,5400\]$' \
  -- tooth_applog "tooth18" "$T/t18.log"
echo "    T18 OK: a collapsed audio-callback count dies on the cadence window (rc 1)"

# T19 — the FROZEN SFX PIN (review-109-2 M7). The perturbed device count
# and the TWIN reference are moved TOGETHER to 273 (review-109-3 M5: the
# round-2 version moved only the device side, so it died on the
# device-vs-twin comparison and never reached the pin branch — deleting
# the frozen-pin enforcement left it green). Agreeing-but-wrong is
# exactly the regression shape the independent pin exists to catch.
sed -E 's/^(foh_dev audio: [0-9]+ callbacks, 0 underruns, 0 badlen, )274( voice starts)/\1273\2/' \
  "$BUILD/g01.dev-applog.txt" > "$T/t19.log"
cmp -s "$T/t19.log" "$BUILD/g01.dev-applog.txt" && fail "T19: substitution was a no-op (dead tooth)"
tooth_expect T19 1 \
  '^FULLGAME FAIL: tooth19: voice starts 273 != the frozen pin 274$' \
  -- judge_applog "tooth19" "$T/t19.log" "$FRAMES_PIN" "$AUDIO_RATE" \
     "$AUDIO_SAMPLES" "$AUDIO_CHANNELS" battlefield "$DSD" 273 0 g01
echo "    T19 OK: a device+twin pair that AGREE at the wrong SFX count still dies on the frozen pin (rc 1)"

# THE FROZEN TEETH COUNT (review-109-4 L5). The verdict advertises
# `teeth=<n>`, and without this assertion a deleted, renamed or
# accidentally skipped tooth simply lowers the number — every grammar
# still matches and the suite still passes. Adding a tooth is a REVIEWED
# edit that re-freezes TEETH_PIN in the same change.
[ "$teeth" = "$TEETH_PIN" ] \
  || fail "$teeth teeth executed, want EXACTLY $TEETH_PIN (see TEETH_PIN) — a tooth was deleted, skipped or added without re-freezing the pin"
# ...and the NAMED ledger, byte-exact (review-109-5 L6): the count above is
# an aggregate and cannot see a missing COMPONENT (T1's raw half, one T21
# sub-case). Every component that passed appended its own name; the sorted
# result must equal the frozen inventory exactly.
nl_terminated "$TLEDGER" "teeth ledger"
LC_ALL=C sort "$TLEDGER" > "$T/ledger.sorted"
printf '%s\n' "$TEETH_LEDGER_FROZEN" > "$T/ledger.want"
cmp -s "$T/ledger.sorted" "$T/ledger.want" \
  || fail "the teeth execution ledger != the frozen inventory (a named tooth or component was skipped, renamed, deleted or duplicated). Got: $(tr '\n' ' ' < "$T/ledger.sorted")"

# --- [8/9] hygiene + verdict --------------------------------------------------
echo "== [8/9] hygiene =="
rig_no_commit_guard "$BUILD" "$DEVB" "$TABLES" "$AUDIO_OUT"

echo "FULLGAME CONFORMS ${pass}/${N_GOLDENS_PIN} (render+sfx+music live; live-ai=${LIVE_AI_CSV} p99=${P99_WORST_MS}ms skips=0 underruns=0 starves=0 presentfails=0 teeth=$teeth)${ATTRIB_TAG}"
