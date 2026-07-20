#!/usr/bin/env bash
# check-device-foh.sh — M4 task 10 done-check: FOH ON DEVICE
# (fix_plan §M4 task 10; pre-registration AGENT-LOG iter 93).
#
# THE BINDING (review-88 triage M3, DEFER-BOUND, honored here): every
# COMMITTED flow under port/foh/flows/ is driven ON the FunKey-S through
# the REAL platform_poll/keysym path — fk_input (our own uinput device)
# injects the letter keysyms u/d/l/r/a/b/x/y/s/k/n/q, the kernel input
# core delivers them to SDL1.2, platform_poll reads SDL_GetKeyState —
# and every device trace is judged against the SAME frozen flow traces
# (flows/<id>.expect), so a backend key-translation swap (A/B swapped,
# START dropped, directions reversed) DIES against the frozen bytes.
# The device invocation is pinned to `--input poll` (rig_argv_assert);
# the host twin's flow-fed PlatformInput construction never runs on the
# device.
#
# JUDGMENT FORMS (pre-registered, AGENT-LOG iter 93; hardened iter 95
# per the review-93 triage — H1/H2/M1/M2/M3/M4 all closed here):
#  - device traces: judge-foh-trace.js grammar (frame-agnostic) +
#    NORMALIZED-SEQUENCE byte-equality vs the frozen .expect
#    (normalize-foh-trace.js elision; every structural fact — edges,
#    causes, S values, LAUNCH params, shot names/order, transitions
#    count — is byte-compared; frozen bytes untouched) PLUS the
#    BOUNDED-DELTA judgment (iter 95, review-93 M1): per-event device
#    ticks vs the flow's injection cadence under measured-then-frozen
#    bounds — a multi-second mid-run stall no longer normalizes away;
#  - device shots: BYTE-EXACT vs host twin references (the task-3/
#    iter-74 device-render bit-identity class; tick-indexed shots at
#    identical ticks, q-marker shots at settled identical state) PLUS
#    the PRESENT WITNESS (iter 95, review-93 H1): foh_dev's
#    --fb-witness reads the DISPLAYED kernel-fb page post-present at
#    every sampled shot and dies in-app unless it byte-matches the
#    submitted frame (measured page-policy/transform pins) — a
#    dead/no-op presenter can no longer pass on pre-present RAM shots;
#    witness rows re-judged here (strict FBWIT1 grammar, names in
#    order, count pinned). HONEST COVERAGE: the witness sees the
#    kernel fb page, not the physical panel, and samples FOH-phase
#    shot presents only — match-phase presents stay unwitnessed
#    (task-14 note; the render rung inherits the class at task 14);
#  - keymap SSOT (iter 95 H2; COMPILED-TABLE proof iter 97, review-95
#    M-b): port/foh/keymap-frozen.txt (sha-pinned) is THE frozen
#    logical-button→letter-keysym truth; the COMPILED mapping lives at
#    ONE definition site — port/gfx/platform_keymap.h — consumed by
#    BOTH platform_sdl1.c's platform_poll translation arm and foh_dev
#    --dump-keymap, whose emission must cmp byte-exactly against the
#    frozen file. The old global source-substring scan is DELETED
#    (comments could satisfy it; it proved nothing about compiled
#    behavior). Proof teeth: T12 builds a COPY foh_dev from a
#    PERTURBED copy of the header — its dump must DIVERGE from the
#    frozen file (a blind dump = fatal); the permanent DEVICE tooth
#    drives an A<->B-swapped injector mapping through the REAL
#    uinput→SDL→platform_poll chain and requires the judge to DIE on
#    the device trace;
#  - f01 launch bridge: the FOH-launched match replays g01's trace on
#    device with LIVE render + SFX + MUSIC, FULL 3600-frame stream
#    judged by the UNCHANGED wrap-run.js + verify-stream.js vs the
#    frozen g01 stream (exceeds the conventions' prefix bar); p99 <
#    16.67 ms (judge-render-timing.js), skips==0, presentFails==0,
#    underruns==0, badlen==0, music starves==0;
#  - f02/f03/f05: BRIDGE-STATE byte-exact vs the frozen .bstate.expect
#    (CPU toggle + difficulty, settings plane, p2char/stage — all
#    through the real keysym path);
#  - menu SFX/music wiring: device voice starts/stops == the host
#    twin's per flow (mixer bookkeeping is main-thread deterministic);
#  - OPK: packaged ONLY with the SDK container's mksquashfs 4.4,
#    contents verified through unsquashfs, squashfs-mounted ON DEVICE,
#    mlfk-foh.sh runs foh_device FROM THE MOUNT (boot marker bin sha ==
#    the arm-build stamp record — the launcher enters the FOH, not the
#    direct-match path), bounded no-input evidence run judged vs a
#    constructed startup->title expectation + tick shots byte-exact +
#    the present witness on both shots, PLUS (iter 97, review-95 M-d)
#    the SAME bounded-delta judgment as the flow legs (constructed
#    expectation = the frozen f01.expect's own pre-input prefix +
#    END==foh-max exact; explicit input-free declaration — no injector
#    runs in the evidence leg) and the SAME strict skip-summary parse
#    (mlfk-foh.log pulled; skips==0, failed presents==0) under the
#    SAME low_bat_check quiesce bracket. The verdict token is
#    `opk=evidence` (iter 95, review-93 M3): this leg proves ONLY the
#    mount-and-run evidence path — FRONTEND-NAV discovery/launch AND
#    the mlfk-foh.sh live branch are task 14's gate leg (iter-73
#    registered stale-nav note), NOT proven here. The evidence-mode
#    sentinel $DSD/foh-args is explicitly removed + absence-verified
#    BEFORE the verdict (iter 95, review-93 M2; trap-covered too). The
#    evidence OPK lives under the UNIQUE title "MeleeLight FOH" and is
#    REMOVED at cleanup — the play install
#    /mnt/Applications/meleelight.opk is never touched.
#
# Rig plumbing INHERITED from port/sim/device/riglib.sh (lock, nonce
# dsh, pullv, made, stamp-cached shared arm build, push provenance,
# qd-normalize, daemon quiesce + bracket, no-commit guard); park +
# deadman + quiesce shapes from check-device-render.sh; the fk_input
# handshake from check-device-input.sh.
#
# Prints `DEVICE FOH OK (...)`, exit 0; ANY divergence, pin mismatch,
# grammar violation, perf/audio shortfall, or missing artifact ->
# nonzero.
set -euo pipefail
cd "$(dirname "$0")/../.."

GFX=port/gfx
FOH=port/foh
FLOWS=$FOH/flows
SIM=port/sim/sim
CAL=port/sim/calib
DEVB=port/sim/calib/build/device
TABLES=pipeline/build/sim-tables
AUDIO_OUT=pipeline/build/audio-foh
BUILD=$FOH/build/device-foh
FDC=oracle/fdlibm-crosscheck
DTMP=/tmp/mlfk
DSD=/mnt/mlfk-scratch
DEVAPPS=/mnt/Applications
OPK_NAME=meleelight-foh-evidence.opk
MKSQ_VERSION_LINE='mksquashfs version 4.4 (2019/08/29)'

BUDGET_NS=16666667
FRAMES_PIN=3600
P99_FULL_LIMIT_NS=16670000
WALL_MIN_MS=58000
WALL_MAX_MS=66000
QW_PRE_SLACK_S=10
QW_POST_SLACK_S=10
READY_TRIES=30
DEADMAN_S="${MLFK_DEADMAN_S:-900}" # whole device phase (7 legs) backstop
# present-witness envelope pins (measured, iter-95 probe
# .loop/m4-foh95-probe.log; must equal foh_dev.c's FBWIT_* pins — the
# FBWIT1 header carries them and is judged byte-anchored below)
FBWIT_XFORM_PIN=0
FBWIT_LL_PIN=480
FBWIT_VYRES_PIN=720

# --- committed-input + producer pins ------------------------------------------
# Producer byte pins (the check-foh-flows.sh discipline: a producer
# change invalidates this check's evidence — reviewed pin update in the
# same commit).
PRODUCER_PINS="\
b835b5f886225e0015dae152576eea5a42fa69d7ba0699f4de0e31438d05c5b9 port/sim/sim/wrap-run.js
f420723433b19166b53a80aedf54931ffdfbc6d2505c773fd73b7a13bbcdf60e oracle/harness/verify-stream.js
4160a35b36e8d3d6896ad2c3c6239d4a4860a0d7f43814a7a9b53b7c136742ab port/sim/sim/trace-to-txt.js
7186734f8c3ff9bfad04f59bf9e13f201663e82481e399911433136673721bba port/sim/calib/dump-sim-data.js
2267f8b796b1881d6ef749b5931a5fb08ae9f914b7a67a0e2608d4cada99616e port/foh/judge-foh-trace.js
e034539d69e1f55338e87f89c8c6573410c40a5bcd8dbc91066751f60c9c9fd4 port/gfx/judge-render-timing.js
2b208cfe18c9e5aac370e0212fc74721489fd404aeb67c9deeddee88ba1bfc1e port/foh/keymap-frozen.txt"
N_PINS_WANT=7
# Audio artifact pins: sndpack + battlefield.pcm are TWIN-PINNED to the
# reviewed literals in the sibling checks (asserted below via
# rig_pin_assert_once — drift at either site is a loud death);
# menu.pcm is NEW here (measured-then-frozen iter 93).
SNDPACK_SHA256=f69579082fe569249879faa5ceccb7a810d94d8092695ddc8bb543f3bda3ccb4
MUSIC_BF_SHA256=c7c1fa2262389496beaba8854d9ffa254861a14f89741cde0432e61197649f44
MUSIC_MENU_SHA256=bbf52720a559ca7b0cf21837a1425a42fd612719442a006b041c913d5f8c4856
# menu track metadata pins (sounds.json music.menu — upstream
# music.js:8-22 sprite windows; effective volume = the global 0.3
# changeVolume, sfx.js:624). Cross-checked against the FRESH sounds.json
# every run.
MUSIC_MENU_VOLBITS=3fd3333333333333
MUSIC_MENU_START='0,7425'
MUSIC_MENU_LOOP='7425,173500'
MUSIC_BF_VOLBITS=3fd3333333333333
MUSIC_BF_START='0,12366'
MUSIC_BF_LOOP='12366,184256'

fail() { echo "DEVICE FOH FAIL: $1" >&2; exit 1; }
grammar_die() { echo "DEVICE FOH FAIL: $1" >&2; exit 2; }

# canonical-decimal grammars (iter 97, review-95 M-e — class fix at
# EVERY numeric acceptance site in this file): 0 or no-leading-zero
# bounded decimal; '00' and '007' are corruption, matching the C/node
# producers' canonical integer emission exactly.
NUM12='(0|[1-9][0-9]{0,11})'
NUM19='(0|[1-9][0-9]{0,18})'

source port/sim/device/adbsh.sh
require_device
source port/sim/device/riglib.sh
mkdir -p "$BUILD" "$DEVB"
rig_lock_acquire
RIG_PRESERVE_DTMP=1 # pessimistic until inherited state is normalized
PARKED=0
DEADMAN_ARMED=0
LBC_STOPPED=0
OPK_INSTALLED=0
OPK_MOUNTED=0
DM_NONCE=""
cleanup() {
  # our own processes first (kill is idempotent; rc-tolerant)
  rig_dsh_retry "pkill foh_device; pkill fk_input; true" \
    || echo "WARN: could not pkill foh_device/fk_input on the device" >&2
  # evidence-mode sentinel (review-93 M2): best-effort trap-side removal
  # (the success path removes + absence-verifies it BEFORE the verdict;
  # rig_cleanup's $DSD wipe is the last backstop)
  rig_dsh_retry "rm -f $DSD/foh-args" \
    || echo "WARN: could not remove the evidence sentinel $DSD/foh-args" >&2
  if [ "$OPK_MOUNTED" = 1 ]; then
    rig_dsh_retry "umount $DTMP/opkmnt" \
      || echo "WARN: could not umount the evidence OPK at $DTMP/opkmnt" >&2
  fi
  if [ "$OPK_INSTALLED" = 1 ]; then
    rig_dsh_retry "rm -f $DEVAPPS/$OPK_NAME" \
      || echo "WARN: could not remove the evidence OPK from $DEVAPPS" >&2
  fi
  restore_verified=0
  if [ "$LBC_STOPPED" = 1 ]; then
    if rig_daemon_restore low_bat_check /etc/init.d/S12low-bat-check; then
      rig_dsh_retry "rm -f $DTMP/qd.low_bat_check.$DM_NONCE" \
        || echo "WARN: could not remove the quiesce marker (the deadman's restore arm is comm-scan-guarded and idempotent)" >&2
      LBC_STOPPED=0
    else
      echo "WARN: low_bat_check did NOT verify as running after restart — the armed deadman will comm-scan-restore it within ${DEADMAN_S}s, or run '/etc/init.d/S12low-bat-check start' on the device manually" >&2
    fi
  fi
  if [ "$PARKED" = 1 ]; then
    rig_dsh_retry "rm -f /mnt/disable_frontend" \
      || echo "WARN: could not remove /mnt/disable_frontend" >&2
    if rig_dsh_retry "test ! -f /mnt/disable_frontend"; then
      PARKED=0
      restore_verified=1
    else
      echo "WARN: could not VERIFY /mnt/disable_frontend is gone — the armed deadman will remove it on-device within ${DEADMAN_S}s of the park (or remove it by hand)" >&2
    fi
  else
    restore_verified=1
  fi
  if [ "$DEADMAN_ARMED" = 1 ]; then
    if [ "$restore_verified" = 1 ] && [ "$LBC_STOPPED" = 0 ]; then
      rig_dsh_retry "touch $DTMP/deadman.cancel" \
        || echo "WARN: could not cancel the park deadman — it will fire once (idempotent actions) within ${DEADMAN_S}s" >&2
    else
      echo "WARN: frontend/daemon restore unverified — leaving the deadman ARMED as the backstop (its purpose)" >&2
      RIG_PRESERVE_DTMP=1
    fi
  fi
  rig_cleanup
}
trap cleanup EXIT

# --- [0] startup normalization (the check-device-render chokepoint) -----------
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
  *) fail "startup normalization could not probe for stale deadman state (rc $nrc)" ;;
esac
if [ "$stale_marker" = 1 ] || [ "$stale_deadman" = 1 ]; then
  echo "WARN: stale prior-run state on the device (marker=$stale_marker deadman-state=$stale_deadman) — normalizing before any parking" >&2
  if [ "$stale_marker" = 1 ]; then
    PARKED=1
    dsh "rm -f /mnt/disable_frontend"
    dsh "test ! -f /mnt/disable_frontend"
    PARKED=0
    echo "   stale /mnt/disable_frontend removed (RC-verified gone)"
  fi
  if [ "$stale_deadman" = 1 ]; then
    dsh "mkdir -p $DTMP && touch $DTMP/deadman.cancel"
    sdm_gone=0
    for _ in $(seq 1 6); do
      if dsh "test ! -f $DTMP/deadman.pid" >/dev/null 2>&1; then sdm_gone=1; break; fi
      sleep 2
    done
    if [ "$sdm_gone" != 1 ]; then
      sdm_pid="$(dsh "cat $DTMP/deadman.pid")" || fail "could not read the stale deadman pid file"
      sdm_pid="${sdm_pid%$'\n'}"
      [[ "$sdm_pid" =~ ^(0|[1-9][0-9]{0,6})$ ]] || fail "stale deadman.pid is not a canonical bounded decimal pid ('$sdm_pid')"
      nrc=0
      dsh "test -d /proc/$sdm_pid" >/dev/null || nrc=$?
      case "$nrc" in
        0) fail "stale deadman (pid $sdm_pid) is STILL RUNNING and ignored its cancel — inspect the device" ;;
        1) echo "   stale deadman.pid was orphaned (pid $sdm_pid dead)" ;;
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

# --- [1] pins + params + data planes -------------------------------------------
echo "== [1/9] pins + golden params + tables/simdata/trace =="
n_pins=0
while IFS= read -r pline; do
  [ -n "$pline" ] || continue
  if ! [[ "$pline" =~ ^[0-9a-f]{64}\ [A-Za-z0-9._/-]+$ ]]; then
    fail "producer pin table — line fails the anchored grammar: '$pline'"
  fi
  psha="${pline%% *}"
  ppath="${pline#* }"
  test -f "$ppath" || fail "pinned producer $ppath is missing from the tree"
  have="$(rig_host_sha256 "$ppath")" || fail "cannot hash producer $ppath"
  [ "$have" = "$psha" ] || fail "producer byte pin — $ppath sha256 $have != pinned $psha (reviewed pin update in the same commit)"
  n_pins=$((n_pins + 1))
done <<< "$PRODUCER_PINS"
[ "$n_pins" = "$N_PINS_WANT" ] || fail "producer pin inventory — $n_pins/$N_PINS_WANT pins verified"
# twin pins: judge-foh-trace sha must match check-foh-flows.sh's pinned
# line; sndpack/battlefield pins must match the sibling device checks.
# (the sibling's pin line sits inside its PRODUCER_PINS quoted block, so
# it may carry the block's closing quote — match the exact sha+path pair
# and require EXACTLY one occurrence)
c="$(grep -cF "2267f8b796b1881d6ef749b5931a5fb08ae9f914b7a67a0e2608d4cada99616e port/foh/judge-foh-trace.js" "$FOH/check-foh-flows.sh")" || true
[ "$c" = 1 ] || fail "twin pin — check-foh-flows.sh does not carry the same judge-foh-trace.js sha exactly once (count $c; paired change rule)"
rig_pin_assert_once "$GFX/check-device-music.sh" SNDPACK_SHA256 "$SNDPACK_SHA256" || exit 1
rig_pin_assert_once "$GFX/check-device-music.sh" MUSIC_BF_SHA256 "$MUSIC_BF_SHA256" || exit 1
echo "   producer pins OK (6) + twin pins (judge sha, sndpack, battlefield.pcm)"

# g01 params (strict extraction — check-device-render class)
unset name seed p1 p2 stage frames trace
gparams="$(node -e "
  const m=require('./oracle/goldens/manifest.json');
  const g=m.goldens.find(x=>x.id==='g01');
  if(!g) throw new Error('g01 missing from manifest');
  console.log('name='+g.name);
  console.log('seed='+g.seed);
  console.log('p1='+g.p1); console.log('p2='+g.p2);
  console.log('stage='+g.stage);
  console.log('frames='+g.frames);
  console.log('trace='+g.trace);
")" || fail "g01 manifest param extraction failed"
[ -n "$gparams" ] || fail "g01 manifest param extraction returned nothing"
while IFS='=' read -r gk gv; do
  case "$gk" in
    name)
      [[ "$gv" =~ ^[a-z0-9][a-z0-9-]*$ ]] || fail "manifest g01.name fails validation ('$gv')"
      name=$gv ;;
    trace)
      [[ "$gv" =~ ^[a-z0-9][a-z0-9-]*\.trace\.json$ ]] || fail "manifest g01.trace fails validation ('$gv')"
      trace=$gv ;;
    seed|p1|p2|stage|frames)
      [[ "$gv" =~ ^(0|[1-9][0-9]{0,11})$ ]] || fail "manifest g01.$gk not a canonical bounded decimal ('$gv')"
      printf -v "$gk" '%s' "$gv" ;;
    *) fail "unexpected manifest extraction line '$gk=$gv'" ;;
  esac
done <<< "$gparams"
: "$name" "$seed" "$p1" "$p2" "$stage" "$frames" "$trace"
[ "$frames" -eq "$FRAMES_PIN" ] || fail "manifest g01.frames ($frames) != pinned $FRAMES_PIN"
[ "$stage" -eq 0 ] || fail "g01 stage is not battlefield — the music leg's track selection pin breaks (reviewed change)"
FROZEN=oracle/goldens/$name.sha256.json
made "$FROZEN"
# m01 + g03 seeds (the state-bridge legs' launch seeds — the same
# manifest-derived values check-foh-flows.sh uses)
m01seed="$(node -e '
  const m = JSON.parse(require("fs").readFileSync("port/goldens-m4/manifest.json", "utf8"));
  const g = m.goldens.find((x) => x.id === "m01");
  if (!g || g.cpu !== true) { console.error("m01 shape"); process.exit(1); }
  console.log(String(g.seed));
')" || fail "cannot parse m01 seed"
[[ "$m01seed" =~ ^(0|[1-9][0-9]{0,11})$ ]] || fail "m01 seed grammar ('$m01seed')"
g03seed="$(node -e '
  const m = JSON.parse(require("fs").readFileSync("oracle/goldens/manifest.json", "utf8"));
  const g = m.goldens.find((x) => x.id === "g03");
  if (!g || g.cpu !== false) { console.error("g03 shape"); process.exit(1); }
  console.log(String(g.seed));
')" || fail "cannot parse g03 seed"
[[ "$g03seed" =~ ^(0|[1-9][0-9]{0,11})$ ]] || fail "g03 seed grammar ('$g03seed')"

# flow inventory pin (both directions; the check-foh-flows class).
# iter 99 (M4 task 12): the committed inventory grew f06/f07 (the
# target flows — DRIVEN by port/sim/target/check-device-target.sh, the
# paired device check); THIS check's driven legs stay the five below.
FLOW_INVENTORY=(f01-vs-g01 f02-cpu-m01 f03-options f04-nav f05-vs-g03                 f06-target-t01 f07-target-t02)
FLOW_IDS=(f01-vs-g01 f02-cpu-m01 f03-options f04-nav f05-vs-g03)
FLOW_BRIDGE=(verify state state none state) # DEVICE bridge mode per flow
FLOW_SEED=("$seed" "$m01seed" "$seed" "" "$g03seed")
FLOW_SHOTS=("startup title menu-top menu-battle css sss" \
            "css-cpu sss-ystory" \
            "options-gameplay options-edited" \
            "menu-controls" \
            "css-p2 sss-pstadium")
FLOW_LAUNCH=(1 1 1 0 1)
globbed="$(ls "$FLOWS"/*.flow | sed 's|.*/||; s|\.flow$||' | sort | tr '\n' ' ' | sed 's/ $//')"
pinned="$(printf '%s\n' "${FLOW_INVENTORY[@]}" | sort | tr '\n' ' ' | sed 's/ $//')"
[ "$globbed" = "$pinned" ] || fail "flow inventory pin — flows/*.flow {$globbed} != pinned {$pinned}"
for k in 0 1 2 3 4; do
  id="${FLOW_IDS[$k]}"
  made "$FLOWS/$id.flow" "$FLOWS/$id.expect"
  if [ "${FLOW_BRIDGE[$k]}" != "none" ]; then
    made "$FLOWS/$id.bstate.expect"
  fi
done

anim_file() {
  case "$1" in
    0) echo anim_0_marth.bin ;;
    1) echo anim_1_puff.bin ;;
    2) echo anim_2_fox.bin ;;
    3) echo anim_3_falco.bin ;;
    4) echo anim_4_falcon.bin ;;
    *) echo "DEVICE FOH FAIL: bad char id '$1'" >&2; return 1 ;;
  esac
}
ANIM_P1="$(anim_file "$p1")"
ANIM_P2="$(anim_file "$p2")"
GFXDATA_FROZEN=$GFX/gfxdata-frozen.txt
VFXDATA_FROZEN=$GFX/vfxdata-frozen.txt
VFXGLYPHS_FROZEN=$GFX/vfxglyphs-frozen.txt
made "$GFXDATA_FROZEN" "$VFXDATA_FROZEN" "$VFXGLYPHS_FROZEN"

bash pipeline/extractor/build-extractor.sh
rm -f "$TABLES/ml_tables.c" "$TABLES/ml_tables.h" \
  "$TABLES/ml_stages.c" "$TABLES/ml_stages.h" \
  "$TABLES/ml_targets.c" "$TABLES/ml_targets.h" \
  "$TABLES/$ANIM_P1" "$TABLES/$ANIM_P2"
# targets stage added iter 100: the host twin recipe links foh_dev.c,
# which consumes target_play/gfx_target since iter 99 (paired
# mechanical repair, registered — task 14 owns the cold rerun).
node pipeline/run.js --only animations,tables,stages,targets --out "$TABLES"
made "$TABLES/ml_tables.c" "$TABLES/ml_tables.h" \
  "$TABLES/ml_stages.c" "$TABLES/ml_stages.h" \
  "$TABLES/ml_targets.c" "$TABLES/ml_targets.h" \
  "$TABLES/$ANIM_P1" "$TABLES/$ANIM_P2"
rm -f "$BUILD/simdata.txt"
node "$CAL/dump-sim-data.js" --out "$BUILD/simdata.txt"
made "$BUILD/simdata.txt"
rm -f "$BUILD/g01.trace.txt"
node "$SIM/trace-to-txt.js" "oracle/goldens/$trace" "$BUILD/g01.trace.txt"
made "$BUILD/g01.trace.txt"
echo "   data planes OK (tables + 2 anim bins + simdata + g01 trace)"

# --- [2] audio build: sndpack + menu/battlefield PCM + music manifests ---------
echo "== [2/9] audio build (fresh pipeline audio stage; pinned three ways) =="
DIST="${MELEELIGHT_CLONE:-$HOME/.cache/meleelight-funkey-s/upstream}"
rm -rf "$AUDIO_OUT"
node pipeline/run.js --only audio --dist "$DIST" --out "$AUDIO_OUT"
made "$AUDIO_OUT/sounds.json" "$AUDIO_OUT/audio/music/menu.pcm" \
     "$AUDIO_OUT/audio/music/battlefield.pcm"
msum="$(rig_host_sha256 "$AUDIO_OUT/audio/music/menu.pcm")" || exit 1
[ "$msum" = "$MUSIC_MENU_SHA256" ] || fail "menu.pcm sha256 $msum != pinned $MUSIC_MENU_SHA256 (pipeline/ffmpeg drift — reviewed re-freeze)"
bsum="$(rig_host_sha256 "$AUDIO_OUT/audio/music/battlefield.pcm")" || exit 1
[ "$bsum" = "$MUSIC_BF_SHA256" ] || fail "battlefield.pcm sha256 $bsum != pinned $MUSIC_BF_SHA256"
pack_re="^pack-snd OK count=180 dataBytes=${NUM12} fileBytes=${NUM12}\$"
for side in a b; do
  rm -f "$BUILD/sndpack-$side.bin" "$BUILD/pack-out-$side.txt"
  node "$GFX/pack-snd.js" "$AUDIO_OUT" "$BUILD/sndpack-$side.bin" \
    > "$BUILD/pack-out-$side.txt" || fail "pack-snd.js failed (side $side)"
  made "$BUILD/sndpack-$side.bin" "$BUILD/pack-out-$side.txt"
  c="$(grep -cE "$pack_re" "$BUILD/pack-out-$side.txt")" || true
  [ "$c" = 1 ] || grammar_die "pack-snd output fails the anchored grammar (side $side)"
done
cmp "$BUILD/sndpack-a.bin" "$BUILD/sndpack-b.bin" || fail "sndpack not byte-stable x2"
rm -f "$BUILD/sndpack.bin"
cp "$BUILD/sndpack-a.bin" "$BUILD/sndpack.bin"
made "$BUILD/sndpack.bin"
psum="$(rig_host_sha256 "$BUILD/sndpack.bin")" || exit 1
[ "$psum" = "$SNDPACK_SHA256" ] || fail "sndpack sha256 $psum != pinned $SNDPACK_SHA256"
# menu + battlefield cfg extraction (strict; the check-device-music
# class) + the pinned-metadata asserts
mcfg="$(node -e '
  const die = (m) => { console.error(m); process.exit(1); };
  const s = JSON.parse(require("fs").readFileSync(process.argv[1], "utf8"));
  if (s.formatVersion !== 1) die("sounds.json formatVersion != 1");
  for (const key of ["menu", "battlefield"]) {
    const e = s.music && s.music[key];
    if (!e) die("music." + key + " missing");
    if (e.blob !== "audio/music/" + key + ".pcm") die("non-canonical blob path");
    const sp = e.sprite;
    if (!sp || !Array.isArray(sp.start) || sp.start.length !== 2 ||
        !Array.isArray(sp.loop) || sp.loop.length !== 2) die("sprite shape");
    for (const v of [...sp.start, ...sp.loop]) {
      if (!Number.isInteger(v) || v < 0) die("sprite window not a non-negative integer");
    }
    if (!/^[0-9a-f]{16}$/.test(e.volume.bits)) die("volume bits grammar");
    console.log(key + "_volbits=" + e.volume.bits);
    console.log(key + "_start=" + sp.start[0] + "," + sp.start[1]);
    console.log(key + "_loop=" + sp.loop[0] + "," + sp.loop[1]);
  }
' "$AUDIO_OUT/sounds.json")" || fail "music cfg extraction failed"
unset MENU_VB MENU_SO MENU_LO BF_VB BF_SO BF_LO
while IFS='=' read -r mk mv; do
  case "$mk" in
    menu_volbits) MENU_VB="$mv" ;;
    menu_start) MENU_SO="$mv" ;;
    menu_loop) MENU_LO="$mv" ;;
    battlefield_volbits) BF_VB="$mv" ;;
    battlefield_start) BF_SO="$mv" ;;
    battlefield_loop) BF_LO="$mv" ;;
    *) fail "unexpected music cfg line '$mk=$mv'" ;;
  esac
done <<< "$mcfg"
[ "${MENU_VB:-}" = "$MUSIC_MENU_VOLBITS" ] || fail "menu volbits ${MENU_VB:-} != pinned $MUSIC_MENU_VOLBITS"
[ "${MENU_SO:-}" = "$MUSIC_MENU_START" ] || fail "menu start window ${MENU_SO:-} != pinned $MUSIC_MENU_START"
[ "${MENU_LO:-}" = "$MUSIC_MENU_LOOP" ] || fail "menu loop window ${MENU_LO:-} != pinned $MUSIC_MENU_LOOP"
[ "${BF_VB:-}" = "$MUSIC_BF_VOLBITS" ] || fail "battlefield volbits ${BF_VB:-} != pinned $MUSIC_BF_VOLBITS"
[ "${BF_SO:-}" = "$MUSIC_BF_START" ] || fail "battlefield start window ${BF_SO:-} != pinned $MUSIC_BF_START"
[ "${BF_LO:-}" = "$MUSIC_BF_LOOP" ] || fail "battlefield loop window ${BF_LO:-} != pinned $MUSIC_BF_LOOP"
# music manifests (host twin + device paths; strict foh_dev grammar)
rm -f "$BUILD/foh-music-host.txt" "$BUILD/foh-music-dev.txt"
{
  echo "track menu $AUDIO_OUT/audio/music/menu.pcm $MENU_VB ${MENU_SO/,/ } ${MENU_LO/,/ }"
  echo "track battlefield $AUDIO_OUT/audio/music/battlefield.pcm $BF_VB ${BF_SO/,/ } ${BF_LO/,/ }"
} > "$BUILD/foh-music-host.txt"
{
  echo "track menu $DSD/menu.pcm $MENU_VB ${MENU_SO/,/ } ${MENU_LO/,/ }"
  echo "track battlefield $DSD/battlefield.pcm $BF_VB ${BF_SO/,/ } ${BF_LO/,/ }"
} > "$BUILD/foh-music-dev.txt"
made "$BUILD/foh-music-host.txt" "$BUILD/foh-music-dev.txt"
echo "   audio OK (sndpack + menu/battlefield PCM pinned; meta pins verified)"

# --- [3] host build + TWIN legs -------------------------------------------------
echo "== [3/9] host build (headless twin) + twin reference legs =="
CFLAGS_COMMON=(-ffp-contract=off -Wall -Wextra -Werror
  -I"$TABLES" -Iport/ryu -Iport/sim -Ioracle/qjs)
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
# host foh_dev build, factored (iter 97, review-95 M-b) so the T12
# keymap copy-build tooth compiles with EXACTLY the production recipe
# (only the foh_dev.c path + extra -I dirs differ).
build_foh_headless() { # <foh_dev_src> <out> [extra cc args...]
  local src="$1" out="$2"
  shift 2
  cc -O2 "${CFLAGS_COMMON[@]}" "$@" -o "$out" \
    "$BUILD/raster.o" "$src" "$FOH/foh.c" "$FOH/foh_font.c" \
    "$FOH/foh_render.c" "$FOH/foh_persist.c" "$GFX/platform_headless.c" \
    "$GFX/anim1.c" "$GFX/gfx_render.c" "$GFX/gfx_target.c" \
    "$GFX/gfx_vfx.c" "$GFX/gfx_overlay.c" "$GFX/gfx_bg.c" \
    port/sim/target/target_play.c "$TABLES/ml_targets.c" \
    "${SIM_TUS[@]}" \
    port/sim/characters/shared/moves/*.c \
    port/sim/characters/fox/moves/*.c \
    port/sim/characters/falco/moves/*.c \
    port/sim/characters/falcon/moves/*.c \
    port/sim/characters/marth/moves/*.c \
    port/sim/characters/puff/moves/*.c \
    -lm -lpthread
}
build_foh_headless "$FOH/foh_dev.c" "$BUILD/foh_dev_headless"
made "$BUILD/foh_dev_headless"
echo "   host twin built (raster -O3, all else -O2; -ffp-contract=off everywhere)"

# --- keymap SSOT asserts (iter 95 H2; compiled-table proof iter 97, M-b) --------
teeth=0
# (1) the COMPILED table == the frozen file, byte-exact. Since iter 97
# the dumped table IS the table: platform_keymap.h is the ONE source
# definition site, consumed by platform_sdl1.c's platform_poll
# translation arm AND by --dump-keymap (no second copy anywhere). The
# old global source-substring scan is DELETED — comments satisfied it,
# and it proved nothing about compiled behavior; the compiled proof is
# this cmp + the T12 perturbed-copy-build tooth + the T-devswap device
# tooth on the real chain.
rm -f "$BUILD/keymap-dump.txt"
"$BUILD/foh_dev_headless" --dump-keymap > "$BUILD/keymap-dump.txt" \
  || fail "foh_dev --dump-keymap failed"
made "$BUILD/keymap-dump.txt"
cmp "$BUILD/keymap-dump.txt" "$FOH/keymap-frozen.txt" \
  || fail "foh_dev's compiled keymap != the frozen keymap-frozen.txt (SSOT drift)"
# (2) the frozen file's own whitelist grammar (header + exactly 12
# canonical map rows; PROCESS §3)
n_kmap=0
kline=0
while IFS= read -r kln; do
  kline=$((kline + 1))
  if [ "$kline" = 1 ]; then
    [ "$kln" = "KEYMAP1" ] || fail "keymap-frozen.txt line 1 is not exactly 'KEYMAP1'"
    continue
  fi
  if ! [[ "$kln" =~ ^map\ ([a-z]+)\ [A-Z]\ ([a-z])$ ]]; then
    fail "keymap-frozen.txt line fails the KEYMAP1 grammar: '$kln'"
  fi
  n_kmap=$((n_kmap + 1))
done < "$FOH/keymap-frozen.txt"
[ "$n_kmap" = 12 ] || fail "keymap-frozen.txt carries $n_kmap map rows (want 12)"
# (3) T12 — the COMPILED-table tooth (iter 97, review-95 M-b): build a
# COPY foh_dev whose ONLY delta is a PERTURBED copy of
# platform_keymap.h (a<->b keysym cells swapped); quoted-include
# resolution makes the copied TU consume the perturbed header (its
# "../gfx/platform_keymap.h" resolves beside the copy) while every
# other header falls back to the real tree via -I. The copy's dump
# MUST diverge from the frozen file (rc exactly 1) AND equal the
# constructed perturbed expectation — proving --dump-keymap reads the
# COMPILED table, not any file, and that the cmp gate has teeth.
rm -rf "$BUILD/tooth-kmcopy"
mkdir -p "$BUILD/tooth-kmcopy/foh" "$BUILD/tooth-kmcopy/gfx"
cp "$FOH/foh_dev.c" "$BUILD/tooth-kmcopy/foh/foh_dev.c"
node -e '
  const fs = require("fs");
  const [src, dst] = process.argv.slice(1);
  const raw = fs.readFileSync(src, "utf8");
  const rowA = "    {\"a\", '\''A'\'', '\''a'\'', offsetof(PlatformInput, a)},";
  const rowB = "    {\"b\", '\''B'\'', '\''b'\'', offsetof(PlatformInput, b)},";
  if (!raw.includes(rowA) || !raw.includes(rowB)) {
    console.error("T12: exact a/b rows not found in platform_keymap.h");
    process.exit(1);
  }
  const out = raw
    .replace(rowA, "    {\"a\", '\''A'\'', '\''b'\'', offsetof(PlatformInput, a)},")
    .replace(rowB, "    {\"b\", '\''B'\'', '\''a'\'', offsetof(PlatformInput, b)},");
  if (out === raw) { console.error("T12: swap was a no-op"); process.exit(1); }
  fs.writeFileSync(dst, out);
' "$GFX/platform_keymap.h" "$BUILD/tooth-kmcopy/gfx/platform_keymap.h" \
  || fail "T12: could not derive the perturbed platform_keymap.h copy"
made "$BUILD/tooth-kmcopy/gfx/platform_keymap.h"
cmp -s "$BUILD/tooth-kmcopy/gfx/platform_keymap.h" "$GFX/platform_keymap.h" && \
  fail "T12: perturbed header copy is byte-identical to the real header (dead tooth)"
build_foh_headless "$BUILD/tooth-kmcopy/foh/foh_dev.c" \
  "$BUILD/tooth-kmcopy/foh_dev_headless" -Iport/foh -Iport/gfx \
  || fail "T12: perturbed copy build failed outright"
made "$BUILD/tooth-kmcopy/foh_dev_headless"
rm -f "$BUILD/tooth-kmcopy/keymap-dump.txt"
"$BUILD/tooth-kmcopy/foh_dev_headless" --dump-keymap \
  > "$BUILD/tooth-kmcopy/keymap-dump.txt" \
  || fail "T12: perturbed copy --dump-keymap failed"
made "$BUILD/tooth-kmcopy/keymap-dump.txt"
rc=0
cmp -s "$BUILD/tooth-kmcopy/keymap-dump.txt" "$FOH/keymap-frozen.txt" || rc=$?
[ "$rc" = 1 ] || fail "T12: perturbed-table dump vs frozen cmp rc $rc (want exactly 1 — rc 0 means --dump-keymap is BLIND to the compiled table)"
node -e '
  const fs = require("fs");
  const [src, dst] = process.argv.slice(1);
  const raw = fs.readFileSync(src, "utf8");
  const out = raw.split("\n").map((ln) => {
    if (ln === "map a A a") return "map a A b";
    if (ln === "map b B b") return "map b B a";
    return ln;
  }).join("\n");
  if (out === raw) { console.error("no-op"); process.exit(1); }
  fs.writeFileSync(dst, out);
' "$FOH/keymap-frozen.txt" "$BUILD/tooth-kmcopy/keymap-want.txt" \
  || fail "T12: could not construct the perturbed dump expectation"
cmp "$BUILD/tooth-kmcopy/keymap-dump.txt" "$BUILD/tooth-kmcopy/keymap-want.txt" \
  || fail "T12: perturbed dump != the constructed perturbed expectation (the dump does not reflect the compiled table)"
teeth=$((teeth + 1))
echo "   keymap SSOT OK (dump == frozen; ONE compiled definition site platform_keymap.h; source scan deleted)"
echo "    T12 OK: perturbed compiled-table COPY build dumps a diverging keymap and DIES at the frozen cmp"

# device fb-witness judge (strict FBWIT1 grammar; foh_dev.c dies in-app
# on any mismatch — this re-judges the pulled rows fail-closed).
# SINGLE STRICT READER (iter 97, review-95 M-a): the file must be
# newline-terminated (a torn trailer is skipped by `read` — the
# round-2 hole), EVERY line passes the exact-position whitelist, the
# END terminator must be the LAST line, and reader-iterations ==
# grep-count == pinned nw+2. Row grammar: canonical-decimal tick
# (M-e) + yoff=0 EXACTLY (L-b — the measured pan-reject policy; any
# other value names the H1 wrong-page hazard).
judge_fbwit() { # <file> <flow-id> <shot-names...>
  local wf="$1" fid="$2"
  shift 2
  local want=("$@") nw=${#want[@]} i=0 ln sawEnd=0
  made "$wf"
  [ -s "$wf" ] || grammar_die "fbwit $fid: empty witness file"
  [ -z "$(tail -c 1 "$wf")" ] \
    || grammar_die "fbwit $fid: missing trailing newline (torn write)"
  local nlines
  nlines="$(grep -c "" "$wf")" || fail "fbwit $fid: cannot count lines"
  [ "$nlines" = "$((nw + 2))" ] || grammar_die "fbwit $fid: $nlines lines (want $((nw + 2)))"
  while IFS= read -r ln; do
    [ "$sawEnd" = 0 ] || grammar_die "fbwit $fid: content after the END terminator"
    if [ "$i" = 0 ]; then
      [ "$ln" = "FBWIT1 flow=$fid xform=$FBWIT_XFORM_PIN ll=$FBWIT_LL_PIN vyres=$FBWIT_VYRES_PIN" ] \
        || grammar_die "fbwit $fid: header '$ln' != pinned envelope"
    elif [ "$i" -le "$nw" ]; then
      if ! [[ "$ln" =~ ^W\ (0|[1-9][0-9]{0,6})\ ([a-z0-9-]{1,32})\ yoff=0\ eq=1$ ]]; then
        grammar_die "fbwit $fid: row $i fails the FBWIT1 grammar (canonical tick, yoff=0 pinned): '$ln'"
      fi
      [ "${BASH_REMATCH[2]}" = "${want[$((i - 1))]}" ] \
        || grammar_die "fbwit $fid: row $i shot '${BASH_REMATCH[2]}' != expected '${want[$((i - 1))]}'"
    else
      [ "$ln" = "END shots=$nw" ] \
        || grammar_die "fbwit $fid: trailer '$ln' != 'END shots=$nw'"
      sawEnd=1
    fi
    i=$((i + 1))
  done < "$wf"
  [ "$i" = "$nlines" ] \
    || grammar_die "fbwit $fid: reader iterated $i of $nlines counted lines (torn trailer)"
  [ "$sawEnd" = 1 ] \
    || grammar_die "fbwit $fid: END terminator never seen at the final position"
}

# foh_dev summary parsers (whitelist grammars — foh_dev.c fprintf
# sites). Resemblance = corruption SUBSTRING-ANYWHERE (iter 97,
# review-95 M-c — the iter-86 needle-freedom form): the needle
# 'foh_dev <kind>:' appearing ANYWHERE on ANY line (grep -F,
# unanchored) must count exactly 1, so a damaged duplicate like
# 'Xfoh_dev audio: ...' is death, never silence. Numerals are
# CANONICAL decimals (iter 97, review-95 M-e: $NUM12/$NUM19 — '00' is
# corruption; the C producer emits %ld/%PRIu64).
parse_foh_summary() { # <log> <launched 0|1> <want-shots>
  local log="$1" launched="$2" wshots="$3" re cnt line pcnt
  unset foh_skips foh_fails foh_transitions
  pcnt="$(grep -cF 'foh_dev foh:' "$log")" || true
  [ "$pcnt" = 1 ] || grammar_die "app log $log has $pcnt lines containing 'foh_dev foh:' (want exactly 1 — resemblance ANYWHERE is corruption)"
  re="^foh_dev foh: ${NUM12} ticks, ${NUM12} transitions, ${wshots} shots, ${NUM12} render skips, ${NUM12} failed presents, launched=${launched}\$"
  cnt="$(grep -cE "$re" "$log")" || true
  [ "$cnt" = 1 ] || grammar_die "app log $log has $cnt lines matching the pinned foh-summary grammar (want 1: shots=$wshots launched=$launched)"
  line="$(grep -E "$re" "$log")"
  # groups: 1=ticks 2=transitions 3=skips 4=fails (canonical numerals
  # are capture groups — indices audited iter 97)
  if [[ "$line" =~ ^foh_dev\ foh:\ (0|[1-9][0-9]{0,11})\ ticks,\ (0|[1-9][0-9]{0,11})\ transitions,\ ${wshots}\ shots,\ (0|[1-9][0-9]{0,11})\ render\ skips,\ (0|[1-9][0-9]{0,11})\ failed\ presents,\ launched=${launched}$ ]]; then
    foh_transitions="${BASH_REMATCH[2]}"
    foh_skips="${BASH_REMATCH[3]}"
    foh_fails="${BASH_REMATCH[4]}"
  else
    grammar_die "foh summary line failed re-extraction ('$line')"
  fi
}
parse_match_summary() { # <log> <frames> <pace>
  local log="$1" fr="$2" pace="$3" re cnt line pcnt
  unset match_skips match_fails match_wall_ms
  pcnt="$(grep -cF 'foh_dev match:' "$log")" || true
  [ "$pcnt" = 1 ] || grammar_die "app log $log has $pcnt lines containing 'foh_dev match:' (want exactly 1 — resemblance ANYWHERE is corruption)"
  re="^foh_dev match: ${fr} frames, ${NUM12} render skips, ${NUM12} failed presents, wall ${NUM12} ms, pace=${pace} budget=${BUDGET_NS} ns\$"
  cnt="$(grep -cE "$re" "$log")" || true
  [ "$cnt" = 1 ] || grammar_die "app log $log has $cnt lines matching the pinned match-summary grammar (want 1)"
  line="$(grep -E "$re" "$log")"
  # groups: 1=skips 2=fails 3=wall
  if [[ "$line" =~ ^foh_dev\ match:\ ${fr}\ frames,\ (0|[1-9][0-9]{0,11})\ render\ skips,\ (0|[1-9][0-9]{0,11})\ failed\ presents,\ wall\ (0|[1-9][0-9]{0,11})\ ms,\ pace=${pace}\ budget=${BUDGET_NS}\ ns$ ]]; then
    match_skips="${BASH_REMATCH[1]}"
    match_fails="${BASH_REMATCH[2]}"
    match_wall_ms="${BASH_REMATCH[3]}"
  else
    grammar_die "match summary line failed re-extraction ('$line')"
  fi
}
parse_audio_summary() { # <log>
  local log="$1" re cnt line pcnt
  unset au_underruns au_badlen au_starts au_stops
  pcnt="$(grep -cF 'foh_dev audio:' "$log")" || true
  [ "$pcnt" = 1 ] || grammar_die "app log $log has $pcnt lines containing 'foh_dev audio:' (want exactly 1 — resemblance ANYWHERE is corruption)"
  re="^foh_dev audio: ${NUM12} callbacks, ${NUM12} underruns, ${NUM12} badlen, ${NUM12} voice starts, ${NUM12} voice stops, ${NUM12} steals, rate=(0|44100) samples=(0|512) channels=(0|2)\$"
  cnt="$(grep -cE "$re" "$log")" || true
  [ "$cnt" = 1 ] || grammar_die "app log $log has $cnt lines matching the pinned audio-summary grammar (want 1)"
  line="$(grep -E "$re" "$log")"
  # groups: 1=callbacks 2=underruns 3=badlen 4=starts 5=stops
  if [[ "$line" =~ ^foh_dev\ audio:\ (0|[1-9][0-9]{0,11})\ callbacks,\ (0|[1-9][0-9]{0,11})\ underruns,\ (0|[1-9][0-9]{0,11})\ badlen,\ (0|[1-9][0-9]{0,11})\ voice\ starts,\ (0|[1-9][0-9]{0,11})\ voice\ stops,\ (0|[1-9][0-9]{0,11})\ steals, ]]; then
    au_underruns="${BASH_REMATCH[2]}"
    au_badlen="${BASH_REMATCH[3]}"
    au_starts="${BASH_REMATCH[4]}"
    au_stops="${BASH_REMATCH[5]}"
  else
    grammar_die "audio summary line failed re-extraction ('$line')"
  fi
}
parse_music_summary() { # <log>
  local log="$1" re cnt line pcnt
  unset mu_out mu_starves mu_refills
  pcnt="$(grep -cF 'foh_dev music:' "$log")" || true
  [ "$pcnt" = 1 ] || grammar_die "app log $log has $pcnt lines containing 'foh_dev music:' (want exactly 1 — resemblance ANYWHERE is corruption)"
  re="^foh_dev music: ${NUM19} out frames, ${NUM12} starves, ${NUM12} refills, ring=32768 chunk=16384\$"
  cnt="$(grep -cE "$re" "$log")" || true
  [ "$cnt" = 1 ] || grammar_die "app log $log has $cnt lines matching the pinned music-summary grammar (want 1)"
  line="$(grep -E "$re" "$log")"
  # groups: 1=out 2=starves 3=refills
  if [[ "$line" =~ ^foh_dev\ music:\ (0|[1-9][0-9]{0,18})\ out\ frames,\ (0|[1-9][0-9]{0,11})\ starves,\ (0|[1-9][0-9]{0,11})\ refills, ]]; then
    mu_out="${BASH_REMATCH[1]}"
    mu_starves="${BASH_REMATCH[2]}"
    mu_refills="${BASH_REMATCH[3]}"
  else
    grammar_die "music summary line failed re-extraction ('$line')"
  fi
}

# device shot pair judge (byte-exact vs the twin reference; structural
# P6 validation first — the check-foh-flows judge_shot_pair class)
judge_dev_shot() { # <ctx> <device.ppm> <ref.ppm>
  local ctx="$1" df="$2" rf="$3"
  made "$df" "$rf"
  node -e '
    const fs = require("fs");
    const b = fs.readFileSync(process.argv[1]);
    const hdr = Buffer.from("P6\n240 240\n255\n", "latin1");
    if (b.length < hdr.length || !b.subarray(0, hdr.length).equals(hdr)) {
      console.error("shot header is not exactly P6/240 240/255"); process.exit(1);
    }
    if (b.length !== hdr.length + 240 * 240 * 3) {
      console.error("shot payload != 240*240*3"); process.exit(1);
    }
  ' "$df" || fail "shot $ctx: structural validation failed on the device shot"
  cmp "$df" "$rf" || fail "shot $ctx: device shot != host twin reference (byte-exact judgment, pre-registered iter 93)"
}

# TWIN legs: references + starts/stops expectations. f01 runs x2
# (byte-stability witness); the others once. Every twin leg runs with
# audio+music so the mixer bookkeeping (voice starts/stops) is the
# deterministic expected value for its device leg.
declare -a TWIN_STARTS TWIN_STOPS
run_twin() { # <k> <side>
  local k="$1" side="$2" id mode extra seedv pdir
  id="${FLOW_IDS[$k]}"
  mode="${FLOW_BRIDGE[$k]}"
  seedv="${FLOW_SEED[$k]}"
  rm -rf "$BUILD/twin-$id-$side"
  mkdir -p "$BUILD/twin-$id-$side/shots"
  # task 13 hermeticity (iter 100): fresh per-invocation persist dir —
  # flows start from the authored defaults and the f03 twin's B-exit
  # save cannot leak into later twins' LAUNCH settings.
  pdir="$PWD/$BUILD/twin-$id-$side/persist"
  case "$mode" in
    verify)
      MLFK_PERSIST_DIR="$pdir" \
      "$BUILD/foh_dev_headless" --flow "$FLOWS/$id.flow" --input flow \
        --flow-out "$BUILD/twin-$id-$side/trace.txt" \
        --shots-dir "$BUILD/twin-$id-$side/shots" --pace 0 \
        --bridge verify --simdata "$BUILD/simdata.txt" --seed "$seedv" \
        --trace "$BUILD/g01.trace.txt" --frames "$frames" \
        --out "$BUILD/twin-$id-$side/stream.txt" \
        --timing "$BUILD/twin-$id-$side/tim.txt" \
        --bstate-out "$BUILD/twin-$id-$side/bstate.txt" \
        --gfxdata "$GFXDATA_FROZEN" --vfxdata "$VFXDATA_FROZEN" \
        --glyphs "$VFXGLYPHS_FROZEN" --anim-dir "$TABLES" --legible \
        --sndpack "$BUILD/sndpack.bin" \
        --music-manifest "$BUILD/foh-music-host.txt" \
        2> "$BUILD/twin-$id-$side/log.txt"
      made "$BUILD/twin-$id-$side/stream.txt" "$BUILD/twin-$id-$side/bstate.txt"
      ;;
    state)
      MLFK_PERSIST_DIR="$pdir" \
      "$BUILD/foh_dev_headless" --flow "$FLOWS/$id.flow" --input flow \
        --flow-out "$BUILD/twin-$id-$side/trace.txt" \
        --shots-dir "$BUILD/twin-$id-$side/shots" --pace 0 \
        --bridge state --simdata "$BUILD/simdata.txt" --seed "$seedv" \
        --bstate-out "$BUILD/twin-$id-$side/bstate.txt" \
        --sndpack "$BUILD/sndpack.bin" \
        --music-manifest "$BUILD/foh-music-host.txt" \
        2> "$BUILD/twin-$id-$side/log.txt"
      made "$BUILD/twin-$id-$side/bstate.txt"
      ;;
    none)
      MLFK_PERSIST_DIR="$pdir" \
      "$BUILD/foh_dev_headless" --flow "$FLOWS/$id.flow" --input flow \
        --flow-out "$BUILD/twin-$id-$side/trace.txt" \
        --shots-dir "$BUILD/twin-$id-$side/shots" --pace 0 \
        --sndpack "$BUILD/sndpack.bin" \
        --music-manifest "$BUILD/foh-music-host.txt" \
        2> "$BUILD/twin-$id-$side/log.txt"
      ;;
  esac
  made "$BUILD/twin-$id-$side/trace.txt" "$BUILD/twin-$id-$side/log.txt"
}
for k in 0 1 2 3 4; do
  id="${FLOW_IDS[$k]}"
  mode="${FLOW_BRIDGE[$k]}"
  run_twin "$k" a
  # the twin trace is FRAME-EXACT: cmp against the frozen .expect
  # directly (proves foh_dev's flow arm == the reviewed foh_app machine)
  cmp "$BUILD/twin-$id-a/trace.txt" "$FLOWS/$id.expect" \
    || fail "twin $id: flow-mode trace differs from the frozen $FLOWS/$id.expect"
  if [ "$mode" != "none" ]; then
    cmp "$BUILD/twin-$id-a/bstate.txt" "$FLOWS/$id.bstate.expect" \
      || fail "twin $id: BRIDGE-STATE differs from the frozen witness"
  fi
  nshots_want="$(printf '%s\n' ${FLOW_SHOTS[$k]} | wc -l | tr -d ' ')"
  nshots_got="$(ls "$BUILD/twin-$id-a/shots" | wc -l | tr -d ' ')"
  [ "$nshots_got" = "$nshots_want" ] || fail "twin $id: $nshots_got shots != $nshots_want"
  parse_audio_summary "$BUILD/twin-$id-a/log.txt"
  TWIN_STARTS[$k]="$au_starts"
  TWIN_STOPS[$k]="$au_stops"
done
# f01 x2 byte-stability + full-stream verify vs frozen g01
run_twin 0 b
cmp "$BUILD/twin-f01-vs-g01-a/trace.txt" "$BUILD/twin-f01-vs-g01-b/trace.txt" \
  || fail "twin f01 x2: traces not byte-identical"
cmp "$BUILD/twin-f01-vs-g01-a/stream.txt" "$BUILD/twin-f01-vs-g01-b/stream.txt" \
  || fail "twin f01 x2: streams not byte-identical"
for sname in ${FLOW_SHOTS[0]}; do
  cmp "$BUILD/twin-f01-vs-g01-a/shots/$sname.ppm" \
      "$BUILD/twin-f01-vs-g01-b/shots/$sname.ppm" \
    || fail "twin f01 x2: shot $sname not byte-identical"
done
parse_audio_summary "$BUILD/twin-f01-vs-g01-b/log.txt"
[ "$au_starts" = "${TWIN_STARTS[0]}" ] || fail "twin f01 x2: voice starts differ ($au_starts vs ${TWIN_STARTS[0]}) — the SFX wiring is not deterministic"
[ "$au_stops" = "${TWIN_STOPS[0]}" ] || fail "twin f01 x2: voice stops differ"
rm -f "$BUILD/twin-f01.run.json"
node "$SIM/wrap-run.js" g01 "$BUILD/twin-f01-vs-g01-a/stream.txt" "$BUILD/twin-f01.run.json"
made "$BUILD/twin-f01.run.json"
node oracle/harness/verify-stream.js "$BUILD/twin-f01.run.json" "$FROZEN"
echo "   twin legs OK (5 traces == frozen; f01 x2 stable + STREAM MATCH; starts/stops recorded per flow)"

# --- [4] host teeth: the key-translation-swap kill chain -------------------------
echo "== [4/9] teeth (host side): swap variants + normalizer + generator =="
# (teeth counter initialized at the keymap block — T12 runs there)
mkswap() { # <src> <dst> <sed-y-spec>  (letter transliteration on I rows only)
  node -e '
    const fs = require("fs");
    const [src, dst, spec] = process.argv.slice(1);
    const [from, to] = spec.split(":");
    if (!from || !to || from.length !== to.length) { console.error("bad spec"); process.exit(1); }
    const map = {};
    for (let i = 0; i < from.length; i++) map[from[i]] = to[i];
    const out = fs.readFileSync(src, "utf8").split("\n").map((ln) => {
      const m = /^I ([0-9]+) ([-A-Z]+)$/.exec(ln);
      if (!m) return ln;
      const tok = m[2] === "-" ? "-" :
        [...m[2]].map((c) => map[c] || c).join("");
      return "I " + m[1] + " " + (tok.length ? tok : "-");
    }).join("\n");
    fs.writeFileSync(dst, out);
  ' "$1" "$2" "$3"
  made "$2"
}
norm() { # <in> <out>
  node "$FOH/normalize-foh-trace.js" "$1" "$2"
  made "$2"
}
rm -f "$BUILD/f01.norm.expect"
norm "$FLOWS/f01-vs-g01.expect" "$BUILD/f01.norm.expect"
# T1 A/B swap; T2 direction reverse (L<->R, U<->D); T3 START dropped
swap_tooth() { # <name> <spec>
  local nm="$1" spec="$2" dir="$BUILD/tooth-$1"
  rm -rf "$dir"
  mkdir -p "$dir/shots"
  mkswap "$FLOWS/f01-vs-g01.flow" "$dir/f01-vs-g01.flow" "$spec"
  cmp -s "$dir/f01-vs-g01.flow" "$FLOWS/f01-vs-g01.flow" && \
    fail "T-$nm: the swap variant is byte-identical to the committed flow (dead tooth)"
  MLFK_PERSIST_DIR="$PWD/$dir/persist" \
  "$BUILD/foh_dev_headless" --flow "$dir/f01-vs-g01.flow" --input flow \
    --flow-out "$dir/trace.txt" --shots-dir "$dir/shots" --pace 0 \
    2> "$dir/log.txt" || fail "T-$nm: variant twin run failed outright (want a clean run with a DIVERGENT trace)"
  norm "$dir/trace.txt" "$dir/trace.norm"
  local rc=0
  cmp -s "$dir/trace.norm" "$BUILD/f01.norm.expect" || rc=$?
  [ "$rc" = 1 ] || fail "T-$nm: normalized swap-variant trace cmp rc $rc, want exactly 1 (the frozen trace MUST kill this key-translation swap)"
}
swap_tooth abswap "AB:BA"
teeth=$((teeth + 1))
echo "    T1 OK: A/B swap dies against the frozen f01 trace (normalized judge)"
swap_tooth dirrev "LRUD:RLDU"
teeth=$((teeth + 1))
echo "    T2 OK: direction reversal dies against the frozen f01 trace"
rm -rf "$BUILD/tooth-sdrop"
mkdir -p "$BUILD/tooth-sdrop/shots"
node -e '
  const fs = require("fs");
  const out = fs.readFileSync(process.argv[1], "utf8").split("\n").map((ln) => {
    const m = /^I ([0-9]+) ([-A-Z]+)$/.exec(ln);
    if (!m || m[2] === "-") return ln;
    const tok = [...m[2]].filter((c) => c !== "S").join("");
    return "I " + m[1] + " " + (tok.length ? tok : "-");
  }).join("\n");
  fs.writeFileSync(process.argv[2], out);
' "$FLOWS/f01-vs-g01.flow" "$BUILD/tooth-sdrop/f01-vs-g01.flow"
made "$BUILD/tooth-sdrop/f01-vs-g01.flow"
MLFK_PERSIST_DIR="$PWD/$BUILD/tooth-sdrop/persist" \
"$BUILD/foh_dev_headless" --flow "$BUILD/tooth-sdrop/f01-vs-g01.flow" \
  --input flow --flow-out "$BUILD/tooth-sdrop/trace.txt" \
  --shots-dir "$BUILD/tooth-sdrop/shots" --pace 0 \
  2> "$BUILD/tooth-sdrop/log.txt" || fail "T-sdrop: variant run failed outright"
norm "$BUILD/tooth-sdrop/trace.txt" "$BUILD/tooth-sdrop/trace.norm"
rc=0
cmp -s "$BUILD/tooth-sdrop/trace.norm" "$BUILD/f01.norm.expect" || rc=$?
[ "$rc" = 1 ] || fail "T-sdrop: normalized START-drop trace cmp rc $rc, want exactly 1"
teeth=$((teeth + 1))
echo "    T3 OK: dropped START dies against the frozen f01 trace"
# T4 normalizer whitelist: malformed + resembling lines die rc 2
rm -f "$BUILD/tooth-norm.trace" "$BUILD/tooth-norm.out"
{ head -n 3 "$FLOWS/f01-vs-g01.expect"; echo "T 999 css sss warp"; \
  tail -n +4 "$FLOWS/f01-vs-g01.expect"; } > "$BUILD/tooth-norm.trace"
rc=0
node "$FOH/normalize-foh-trace.js" "$BUILD/tooth-norm.trace" "$BUILD/tooth-norm.out" 2>/dev/null || rc=$?
[ "$rc" = 2 ] || fail "T-norm: normalizer accepted a resembling-but-wrong line (rc $rc, want 2)"
teeth=$((teeth + 1))
echo "    T4 OK: normalizer dies rc 2 on a resembling-but-wrong line"
# T9 generator: non-monotone flow dies rc 2 before emitting
rm -f "$BUILD/tooth-gen.flow" "$BUILD/tooth-gen.fks"
printf 'FLOW1\nI 1 -\nI 400 A\nI 390 -\nEND 500\n' > "$BUILD/tooth-gen.flow"
rc=0
node "$FOH/flow-to-fkscript.js" "$BUILD/tooth-gen.flow" "$BUILD/tooth-gen.fks" 2>/dev/null || rc=$?
[ "$rc" = 2 ] || fail "T-gen: generator accepted a non-monotone flow (rc $rc, want 2)"
test ! -f "$BUILD/tooth-gen.fks" || fail "T-gen: generator emitted a script despite dying"
teeth=$((teeth + 1))
echo "    T5 OK: fk-script generator dies rc 2 on a non-monotone flow (no output)"

# --- [5] fk scripts (mechanical derivation, x2 byte-stable) ----------------------
echo "== [5/9] fk_input scripts (derived x2, byte-stable) =="
for k in 0 1 2 3 4; do
  id="${FLOW_IDS[$k]}"
  rm -f "$BUILD/$id.fks" "$BUILD/$id.fks.b"
  node "$FOH/flow-to-fkscript.js" "$FLOWS/$id.flow" "$BUILD/$id.fks" >/dev/null
  node "$FOH/flow-to-fkscript.js" "$FLOWS/$id.flow" "$BUILD/$id.fks.b" >/dev/null
  made "$BUILD/$id.fks" "$BUILD/$id.fks.b"
  cmp "$BUILD/$id.fks" "$BUILD/$id.fks.b" || fail "fk script $id not byte-stable x2"
done
echo "   5 fk scripts derived (LEAD 8200 ms, 50 ms/frame — AGENT-LOG iter 93)"

# --- [6] arm build + push --------------------------------------------------------
echo "== [6/9] armv7 build (shared rig stamp) + push + provenance =="
rig_arm_build
rig_stamp_rehash foh_device fk_input
STAMP_FOH_SHA="$(rig_stamp_bin_sha foh_device)"
dsh "rm -rf $DTMP $DSD && mkdir -p $DTMP $DSD"
adb -s "$DEV" push "$DEVB/foh_device" "$DEVB/fk_input" \
  "$BUILD/simdata.txt" "$BUILD/g01.trace.txt" \
  "$GFXDATA_FROZEN" "$VFXDATA_FROZEN" "$VFXGLYPHS_FROZEN" \
  "$TABLES/$ANIM_P1" "$TABLES/$ANIM_P2" \
  "$BUILD/foh-music-dev.txt" "$DTMP/" >/dev/null
for k in 0 1 2 3 4; do
  id="${FLOW_IDS[$k]}"
  adb -s "$DEV" push "$FLOWS/$id.flow" "$BUILD/$id.fks" "$DTMP/" >/dev/null
done
adb -s "$DEV" push "$BUILD/sndpack.bin" \
  "$AUDIO_OUT/audio/music/menu.pcm" \
  "$AUDIO_OUT/audio/music/battlefield.pcm" \
  "$BUILD/simdata.txt" "$DSD/" >/dev/null
rig_push_provenance "$DTMP" foh_device fk_input
dsh "chmod +x $DTMP/foh_device $DTMP/fk_input"
for hf in "$BUILD/simdata.txt" "$BUILD/g01.trace.txt" "$GFXDATA_FROZEN" \
          "$VFXDATA_FROZEN" "$VFXGLYPHS_FROZEN" \
          "$TABLES/$ANIM_P1" "$TABLES/$ANIM_P2" "$BUILD/foh-music-dev.txt"; do
  bn="$(basename "$hf")"
  hsum="$(rig_host_sha256 "$hf")" || exit 1
  dsum="$(rig_dev_sha256 "$DTMP/$bn")" || exit 1
  [ "$dsum" = "$hsum" ] || fail "pushed $bn device sha ($dsum) != host sha ($hsum)"
done
for k in 0 1 2 3 4; do
  id="${FLOW_IDS[$k]}"
  for hf in "$FLOWS/$id.flow" "$BUILD/$id.fks"; do
    bn="$(basename "$hf")"
    hsum="$(rig_host_sha256 "$hf")" || exit 1
    dsum="$(rig_dev_sha256 "$DTMP/$bn")" || exit 1
    [ "$dsum" = "$hsum" ] || fail "pushed $bn device sha ($dsum) != host sha ($hsum)"
  done
done
for hf in "$BUILD/sndpack.bin" "$AUDIO_OUT/audio/music/menu.pcm" \
          "$AUDIO_OUT/audio/music/battlefield.pcm"; do
  bn="$(basename "$hf")"
  hsum="$(rig_host_sha256 "$hf")" || exit 1
  dsum="$(rig_dev_sha256 "$DSD/$bn")" || exit 1
  [ "$dsum" = "$hsum" ] || fail "pushed $bn device sha ($dsum) != host sha ($hsum)"
done
dsh "sync" # writeback BEFORE the paced legs (the iter-73 mitigation)
echo "   pushed + sha-verified (binaries via stamp provenance; committed flow bytes == device bytes)"

# --- deadman + park (once, spanning all device legs) -----------------------------
DM_NONCE="$RANDOM$RANDOM$$"
rm -f "$BUILD/deadman.sh"
cat > "$BUILD/deadman.sh" << EOF
#!/bin/sh
# generated by check-device-foh.sh — frontend-park DEADMAN (the
# check-device-render.sh iter-52/55/76 design, foh_device scoped)
echo \$\$ > $DTMP/deadman.pid
i=0
while [ \$i -lt $DEADMAN_S ]; do
  sleep 2
  if [ -f $DTMP/deadman.cancel ]; then rm -f $DTMP/deadman.pid; exit 0; fi
  i=\$((i+2))
done
if [ "\$(cat $DTMP/deadman.nonce 2>/dev/null)" = "$DM_NONCE" ] && [ ! -f $DTMP/deadman.cancel ]; then
  echo fired > $DTMP/deadman.fired
  rm -f /mnt/disable_frontend
  gp="\$(cat $DTMP/foh.pid.$DM_NONCE 2>/dev/null)"
  case "\$gp" in
    ''|*[!0-9]*) : ;;
    *) if grep -q foh_device "/proc/\$gp/cmdline" 2>/dev/null; then kill "\$gp"; fi ;;
  esac
  if [ -f $DTMP/qd.low_bat_check.$DM_NONCE ]; then
    n=0
    for c in /proc/[0-9]*/comm; do
      if [ "x\$(cat "\$c" 2>/dev/null)" = "xlow_bat_check" ]; then n=\$((n+1)); fi
    done
    if [ "\$n" = 0 ]; then /etc/init.d/S12low-bat-check start; fi
    n2=0
    for c in /proc/[0-9]*/comm; do
      if [ "x\$(cat "\$c" 2>/dev/null)" = "xlow_bat_check" ]; then n2=\$((n2+1)); fi
    done
    if [ "\$n2" != 0 ]; then rm -f $DTMP/qd.low_bat_check.$DM_NONCE; fi
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
dsh "printf '%s' '$DM_NONCE' > $DTMP/deadman.nonce; rm -f $DTMP/deadman.cancel $DTMP/deadman.fired"
dsh "setsid sh $DTMP/deadman.sh </dev/null >/dev/null 2>&1 & sleep 1"
dsh "test -f $DTMP/deadman.pid" >/dev/null 2>&1 || fail "park deadman did not start (no pid file)"
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
echo "   deadman armed (${DEADMAN_S}s window) + frontend parked for the device phase"

# --- [7] device flow legs (the M3-binding legs) ----------------------------------
echo "== [7/9] device flow legs: fk_input -> uinput -> SDL keysyms -> platform_poll =="
DEV_STARTS=""
FBWIT_TOTAL=0
for k in 0 1 2 3 4; do
  id="${FLOW_IDS[$k]}"
  mode="${FLOW_BRIDGE[$k]}"
  seedv="${FLOW_SEED[$k]}"
  # foh-max: LEAD + (END-370)*STEP ms -> ticks (x3/50) + 600 margin
  endf="$(grep -E '^END (0|[1-9][0-9]*)$' "$FLOWS/$id.flow" | awk '{print $2}')"
  [[ "$endf" =~ ^(0|[1-9][0-9]{0,5})$ ]] || fail "leg $id: flow END frame grammar ('$endf')"
  fohmax=$(( (8200 + (endf - 370) * 50) * 3 / 50 + 600 ))
  # device argv (written to a region file so rig_argv_assert_once can
  # pin --input + poll and refuse duplicate overrides)
  args="--flow $DTMP/$id.flow --input poll --flow-out $DTMP/$id.trace.txt"
  args="$args --shots-dir $DTMP/$id-shots --ready-file $DTMP/$id.ready"
  args="$args --foh-max $fohmax --pace 1 --budget-ns $BUDGET_NS"
  args="$args --fb-witness $DTMP/$id.fbwit.txt"
  args="$args --sndpack $DSD/sndpack.bin --music-manifest $DTMP/foh-music-dev.txt"
  case "$mode" in
    verify)
      args="$args --bridge verify --simdata $DTMP/simdata.txt --seed $seedv"
      args="$args --trace $DTMP/g01.trace.txt --frames $frames"
      args="$args --out $DTMP/$id.out.txt --timing $DTMP/$id.tim.txt"
      args="$args --bstate-out $DTMP/$id.bstate.txt"
      args="$args --gfxdata $DTMP/gfxdata-frozen.txt --vfxdata $DTMP/vfxdata-frozen.txt"
      args="$args --glyphs $DTMP/vfxglyphs-frozen.txt --anim-dir $DTMP --legible"
      ;;
    state)
      args="$args --bridge state --simdata $DTMP/simdata.txt --seed $seedv"
      args="$args --bstate-out $DTMP/$id.bstate.txt"
      ;;
    none) : ;;
  esac
  rm -f "$BUILD/$id.argv"
  printf '%s\n' "$args" > "$BUILD/$id.argv"
  rig_argv_assert_once "$BUILD/$id.argv" "--input" || exit 1
  c="$(grep -c -- "--input poll" "$BUILD/$id.argv")" || true
  [ "$c" = 1 ] || fail "leg $id: device argv does not pin '--input poll' (the M3 binding)"
  rm -f "$BUILD/$id-launch.sh"
  cat > "$BUILD/$id-launch.sh" << EOF
#!/bin/sh
# generated by check-device-foh.sh — leg launcher for $id
cd $DTMP || exit 9
rm -rf $id.apprc $id.ready $id-shots $id-persist foh.pid.$DM_NONCE app.start.ts app.end.ts
mkdir -p $id-shots
# task 13 hermeticity (iter 100): fresh tmpfs persist dir per leg —
# flows start from defaults and the f03 save never sits on the SD
# inside the paced loop.
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

  echo "== leg $id (bridge=$mode, foh-max=$fohmax)"
  dsh "printf '' > $DTMP/qd.low_bat_check.$DM_NONCE"
  LBC_STOPPED=1
  lbc_pid="$(rig_daemon_stop low_bat_check)"
  dsh "date +%s > $DTMP/qstop.ts"
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
  echo "   app ready — playing $id.fks through fk_input (uinput -> SDL keysyms)"
  dsh "sh -lc 'cd $DTMP && ./fk_input $id.fks'" || fail "leg $id: fk_input injector failed"
  # bounded §7#1 waits, quiet where the paced surface is long
  case "$mode" in
    verify) sleep 55 ;;   # the 3600-frame match (quiet window)
    none)   sleep 12 ;;   # f04 runs to foh-max after the script ends
    state)  sleep 3 ;;    # state legs exit at LAUNCH (inside the script span)
  esac
  done_f=0
  for _ in $(seq 1 30); do
    if dsh "test -f $DTMP/$id.apprc" >/dev/null 2>&1; then done_f=1; break; fi
    sleep 2
  done
  if [ "$done_f" != 1 ]; then
    dsh "cat $DTMP/$id.applog.txt" >&2 || true
    fail "leg $id: app rc file never appeared"
  fi
  # restore-FIRST discipline (iter 78/80): the daemon comes back before
  # any pull/judge chores; the coupled stamp feeds the bracket assert
  if rig_daemon_restore low_bat_check /etc/init.d/S12low-bat-check "$DTMP/qrestore.ts"; then
    dsh "rm -f $DTMP/qd.low_bat_check.$DM_NONCE"
    dsh "test ! -f $DTMP/qd.low_bat_check.$DM_NONCE"
    LBC_STOPPED=0
  else
    fail "leg $id: low_bat_check did not verify as running after restart"
  fi
  qstop_ts="$(rig_dev_ts "$DTMP/qstop.ts")" || exit 1
  appstart_ts="$(rig_dev_ts "$DTMP/app.start.ts")" || exit 1
  append_ts="$(rig_dev_ts "$DTMP/app.end.ts")" || exit 1
  qrestore_ts="$(rig_dev_ts "$DTMP/qrestore.ts")" || exit 1
  rig_quiesce_bracket_assert "foh $id low_bat_check" \
    "$qstop_ts" "$appstart_ts" "$append_ts" "$qrestore_ts" \
    "$QW_PRE_SLACK_S" "$QW_POST_SLACK_S" || exit 1
  dsh "test -s $DTMP/foh.pid.$DM_NONCE" # the deadman's scoped-kill record
  pullv "$DTMP/$id.apprc" "$BUILD/$id.apprc"
  if ! cmp -s "$BUILD/$id.apprc" <(printf 'RC=0\n'); then
    dsh "cat $DTMP/$id.applog.txt" >&2 || true
    fail "leg $id: app rc file is not EXACTLY 'RC=0<newline>' (got: '$(cat "$BUILD/$id.apprc")')"
  fi
  pullv "$DTMP/$id.trace.txt" "$BUILD/$id.dev-trace.txt"
  pullv "$DTMP/$id.applog.txt" "$BUILD/$id.dev-applog.txt"
  # trace: grammar + NORMALIZED equality vs the FROZEN committed trace
  node "$FOH/judge-foh-trace.js" "$BUILD/$id.dev-trace.txt" "$id" "${FLOW_LAUNCH[$k]}"
  norm "$BUILD/$id.dev-trace.txt" "$BUILD/$id.dev-trace.norm"
  rm -f "$BUILD/$id.norm.expect"
  norm "$FLOWS/$id.expect" "$BUILD/$id.norm.expect"
  cmp "$BUILD/$id.dev-trace.norm" "$BUILD/$id.norm.expect" \
    || fail "leg $id: DEVICE trace (normalized) != frozen $FLOWS/$id.expect (normalized) — the real keysym path diverged from the committed flow"
  # BOUNDED-DELTA judgment (iter 95, review-93 M1): per-event device
  # ticks vs the flow's injection cadence, measured-then-frozen bounds
  # — a multi-second mid-run stall no longer normalizes away
  node "$FOH/normalize-foh-trace.js" --bounded "$FLOWS/$id.expect" \
    "$BUILD/$id.dev-trace.txt" "$FLOWS/$id.flow" "$fohmax" \
    || fail "leg $id: BOUNDED-DELTA judgment failed (mid-run stall or injection-cadence defect)"
  # shots: exact count + byte-exact vs the twin references
  nshots_want="$(printf '%s\n' ${FLOW_SHOTS[$k]} | wc -l | tr -d ' ')"
  ndev="$(dsh "ls $DTMP/$id-shots | wc -l")" || fail "leg $id: cannot enumerate device shots"
  ndev="${ndev%$'\n'}"
  ndev="$(printf '%s' "$ndev" | tr -d ' ')"
  [[ "$ndev" =~ ^(0|[1-9][0-9]{0,2})$ ]] || fail "leg $id: device shot count grammar ('$ndev')"
  [ "$ndev" = "$nshots_want" ] || fail "leg $id: device shot count $ndev != pinned $nshots_want"
  for sname in ${FLOW_SHOTS[$k]}; do
    pullv "$DTMP/$id-shots/$sname.ppm" "$BUILD/$id.dev-shot-$sname.ppm"
    judge_dev_shot "$id/$sname" "$BUILD/$id.dev-shot-$sname.ppm" \
      "$BUILD/twin-$id-a/shots/$sname.ppm"
  done
  # present witness (iter 95, review-93 H1): the app died in-app on any
  # displayed-page mismatch; re-judge the pulled rows fail-closed
  pullv "$DTMP/$id.fbwit.txt" "$BUILD/$id.fbwit.txt"
  # shellcheck disable=SC2086 — FLOW_SHOTS is a space-joined name list
  judge_fbwit "$BUILD/$id.fbwit.txt" "$id" ${FLOW_SHOTS[$k]}
  FBWIT_TOTAL=$((FBWIT_TOTAL + nshots_want))
  # summaries: skips==0, presentFails==0 on the paced FOH phase; audio
  # underruns/badlen==0; starts/stops == the twin's; music starves==0
  parse_foh_summary "$BUILD/$id.dev-applog.txt" "${FLOW_LAUNCH[$k]}" "$nshots_want"
  [ "$foh_skips" = 0 ] || fail "leg $id: $foh_skips FOH render skips (want 0; quiesced leg)"
  [ "$foh_fails" = 0 ] || fail "leg $id: $foh_fails failed presents in the FOH phase"
  parse_audio_summary "$BUILD/$id.dev-applog.txt"
  [ "$au_underruns" = 0 ] || fail "leg $id: $au_underruns audio underruns (want 0)"
  [ "$au_badlen" = 0 ] || fail "leg $id: $au_badlen audio badlen callbacks (want 0)"
  [ "$au_starts" = "${TWIN_STARTS[$k]}" ] || fail "leg $id: device voice starts $au_starts != twin ${TWIN_STARTS[$k]} (the menu-SFX/match wiring diverged)"
  [ "$au_stops" = "${TWIN_STOPS[$k]}" ] || fail "leg $id: device voice stops $au_stops != twin ${TWIN_STOPS[$k]}"
  parse_music_summary "$BUILD/$id.dev-applog.txt"
  [ "$mu_starves" = 0 ] || fail "leg $id: $mu_starves music starves (want 0)"
  [ "$mu_out" != 0 ] || fail "leg $id: music consumed 0 output frames (the menu track never played)"
  DEV_STARTS="$DEV_STARTS $id=$au_starts"
  if [ "$mode" != "none" ]; then
    pullv "$DTMP/$id.bstate.txt" "$BUILD/$id.dev-bstate.txt"
    cmp "$BUILD/$id.dev-bstate.txt" "$FLOWS/$id.bstate.expect" \
      || fail "leg $id: DEVICE BRIDGE-STATE != frozen witness (launch plumbing through the real keysym path)"
  fi
  if [ "$mode" = verify ]; then
    pullv "$DTMP/$id.out.txt" "$BUILD/$id.dev-out.txt"
    pullv "$DTMP/$id.tim.txt" "$BUILD/$id.dev-tim.txt"
    [ "$mu_refills" != 0 ] || fail "leg $id: music refills == 0 on the match leg (the SD streamer never ran)"
  fi
  echo "   -> leg $id OK (trace normalized-match, $nshots_want shots byte-exact, starts=$au_starts==twin)"
done

# f01 stream + perf judgment (host side; UNCHANGED judges)
echo "== f01 launch bridge judgment (stream + p99 + summaries)"
rm -f "$BUILD/f01.dev-run.json"
node "$SIM/wrap-run.js" g01 "$BUILD/f01-vs-g01.dev-out.txt" "$BUILD/f01.dev-run.json"
made "$BUILD/f01.dev-run.json"
node oracle/harness/verify-stream.js "$BUILD/f01.dev-run.json" "$FROZEN"
echo "   FOH-launched device stream == frozen g01 (UNCHANGED verify-stream.js, full $frames frames)"
# timing: the strict key=value judge (check-device-render parser class)
timing_judge_bytes_assert() {
  local jf="$1"
  if ! tail -c 17 "$jf" | cmp -s - <(printf 'judge_complete=1\n'); then
    fail "timing judge output does not END with exactly 'judge_complete=1<newline>'"
  fi
}
rm -f "$BUILD/timjudge-out.txt"
node "$GFX/judge-render-timing.js" "$BUILD/f01-vs-g01.dev-tim.txt" "$frames" \
  > "$BUILD/timjudge-out.txt" || fail "timing judgment failed"
made "$BUILD/timjudge-out.txt"
timing_judge_bytes_assert "$BUILD/timjudge-out.txt"
# ORDERED FULL WHITELIST (iter 95, review-93 M4 — the iter-86 class):
# exactly the pinned judge-render-timing.js emission — every key in
# order, each exactly once, every value grammar-validated; any extra,
# missing, reordered, or malformed line is death, never silence.
TIMING_KEYS=(full_p50_ns full_p50_ms full_p99_ns full_p99_ms
  full_max_ns full_max_ms sim_p50_ns sim_p50_ms sim_p99_ns sim_p99_ms
  render_p50_ns render_p50_ms render_p99_ns render_p99_ms
  render_max_ns render_max_ms present_p50_ns present_p50_ms
  present_p99_ns present_p99_ms skips rendered judge_complete)
unset full_p99_ns full_p99_ms skips rendered
ji=0
while IFS= read -r jline; do
  [ "$ji" -lt "${#TIMING_KEYS[@]}" ] \
    || fail "timing judge output has more than ${#TIMING_KEYS[@]} lines"
  jk="${TIMING_KEYS[$ji]}"
  case "$jline" in
    "$jk="*) : ;;
    *) fail "timing judge line $((ji + 1)) is '$jline' (want key '$jk' — ordered whitelist)" ;;
  esac
  jv="${jline#"$jk="}"
  case "$jk" in
    judge_complete)
      [ "$jv" = 1 ] || fail "judge_complete value ('$jv')" ;;
    *_ms)
      [[ "$jv" =~ ^(0|[1-9][0-9]{0,8})\.[0-9]{3}$ ]] || fail "timing judge $jk grammar ('$jv')" ;;
    *)
      [[ "$jv" =~ ^(0|[1-9][0-9]{0,11})$ ]] || fail "timing judge $jk grammar ('$jv')" ;;
  esac
  case "$jk" in
    full_p99_ns|full_p99_ms|skips|rendered) printf -v "$jk" '%s' "$jv" ;;
  esac
  ji=$((ji + 1))
done < "$BUILD/timjudge-out.txt"
[ "$ji" = "${#TIMING_KEYS[@]}" ] \
  || fail "timing judge output has $ji lines (want ${#TIMING_KEYS[@]})"
for jk in full_p99_ns full_p99_ms skips rendered; do
  [ -n "${!jk:-}" ] || fail "timing judge output missing '$jk'"
done
[ "$full_p99_ns" -lt "$P99_FULL_LIMIT_NS" ] || fail "match p99 ${full_p99_ms} ms >= 16.67 ms"
[ "$skips" = 0 ] || fail "match timing artifact reports $skips skips (want 0)"
[ "$rendered" = "$FRAMES_PIN" ] || fail "match rendered $rendered != $FRAMES_PIN"
parse_match_summary "$BUILD/f01-vs-g01.dev-applog.txt" "$frames" 1
[ "$match_skips" = 0 ] || fail "match summary reports $match_skips skips"
[ "$match_fails" = 0 ] || fail "match summary reports $match_fails failed presents"
[ "$match_wall_ms" -ge "$WALL_MIN_MS" ] && [ "$match_wall_ms" -le "$WALL_MAX_MS" ] \
  || fail "match wall ${match_wall_ms} ms outside [$WALL_MIN_MS,$WALL_MAX_MS]"
echo "   f01 perf OK (p99 ${full_p99_ms} ms < 16.67; skips 0/$FRAMES_PIN; wall ${match_wall_ms} ms)"

# --- [8] OPK: package (mksquashfs 4.4 ONLY) + mount + FOH-entry evidence ---------
echo "== [8/9] OPK: FOH launcher packaged, mounted on device, evidence run =="
STAGE=$BUILD/opk-stage
rm -rf "$STAGE" "$BUILD/$OPK_NAME" "$BUILD/opk-verify"
mkdir -p "$STAGE"
cp "$DEVB/foh_device" "$GFX/opk/mlfk-foh.sh" \
   "$GFX/opk/meleelight-foh.funkey-s.desktop" "$GFX/opk/icon32.png" \
   "$FLOWS/f01-vs-g01.flow" "$STAGE/"
chmod +x "$STAGE/foh_device" "$STAGE/mlfk-foh.sh"
ssum="$(rig_host_sha256 "$STAGE/foh_device")" || exit 1
[ "$ssum" = "$STAMP_FOH_SHA" ] || fail "staged foh_device sha ($ssum) != stamp record ($STAMP_FOH_SHA)"
rm -f "$BUILD/mksq-version.txt"
docker run --rm -v "$PWD":/work -w /work "$ARMIMGID" bash -lc '
  set -e
  export PATH=/opt/FunKey-sdk-2.3.0/bin:$PATH
  mksquashfs -version | head -1 > '"$BUILD"'/mksq-version.txt
  mksquashfs '"$STAGE"' '"$BUILD/$OPK_NAME"' \
    -all-root -noappend -no-exports -no-xattrs -comp gzip >/dev/null
'
made "$BUILD/mksq-version.txt" "$BUILD/$OPK_NAME"
printf '%s\n' "$MKSQ_VERSION_LINE" | cmp -s - "$BUILD/mksq-version.txt" \
  || fail "container mksquashfs version line is not exactly '$MKSQ_VERSION_LINE'"
docker run --rm -v "$PWD":/work -w /work "$ARMIMGID" bash -lc '
  set -e
  export PATH=/opt/FunKey-sdk-2.3.0/bin:$PATH
  unsquashfs -d '"$BUILD"'/opk-verify '"$BUILD/$OPK_NAME"' >/dev/null
'
for f in foh_device mlfk-foh.sh meleelight-foh.funkey-s.desktop icon32.png f01-vs-g01.flow; do
  made "$BUILD/opk-verify/$f"
  cmp "$STAGE/$f" "$BUILD/opk-verify/$f"
done
n_stage="$(find "$STAGE" -type f | wc -l | tr -d ' ')"
n_verify="$(find "$BUILD/opk-verify" -type f | wc -l | tr -d ' ')"
{ [ "$n_stage" = 5 ] && [ "$n_verify" = 5 ]; } || fail "OPK content count (stage $n_stage, unsquashed $n_verify, want 5)"
OPK_SHA="$(rig_host_sha256 "$BUILD/$OPK_NAME")" || exit 1
echo "   OPK packaged ($MKSQ_VERSION_LINE) + contents verified (5 files)"
OPK_INSTALLED=1
adb -s "$DEV" push "$BUILD/$OPK_NAME" "$DEVAPPS/" >/dev/null
dsum="$(rig_dev_sha256 "$DEVAPPS/$OPK_NAME")" || exit 1
[ "$dsum" = "$OPK_SHA" ] || fail "device-side OPK sha ($dsum) != host ($OPK_SHA)"
# pinned evidence args: a bounded NO-INPUT FOH run (startup -> title,
# 500 ticks, two tick shots) — proves the OPK launcher enters the FOH.
OPK_FOHMAX=500
rm -f "$BUILD/foh-args"
printf '%s' "--flow $DTMP/opkmnt/f01-vs-g01.flow --input poll --flow-out /tmp/mlfk/opkfoh/foh-trace.txt --shots-dir /tmp/mlfk/opkfoh/shots --foh-max $OPK_FOHMAX --pace 1 --budget-ns $BUDGET_NS --fb-witness /tmp/mlfk/opkfoh/fbwit.txt" > "$BUILD/foh-args"
made "$BUILD/foh-args"
adb -s "$DEV" push "$BUILD/foh-args" "$DSD/" >/dev/null
hsum="$(rig_host_sha256 "$BUILD/foh-args")" || exit 1
dsum="$(rig_dev_sha256 "$DSD/foh-args")" || exit 1
[ "$dsum" = "$hsum" ] || fail "pushed foh-args sha mismatch"
dsh "mkdir -p $DTMP/opkmnt /tmp/mlfk/opkfoh/shots && rm -rf /tmp/mlfk/opkfoh/foh-trace.txt /tmp/mlfk/opkfoh/opk.rc /tmp/mlfk/opkfoh/boot-marker"
OPK_MOUNTED=1
dsh "mount -t squashfs -o loop,ro $DEVAPPS/$OPK_NAME $DTMP/opkmnt"
dsh "test -x $DTMP/opkmnt/mlfk-foh.sh"
# low_bat_check quiesce bracket — LEG-IDENTICAL posture (iter 97,
# review-95 M-d): the OPK evidence run's skips==0 gate is judged under
# the same daemon-quiesced conditions as the flow legs (deadman qd
# marker + stop, restore-FIRST after the rc, device-clock bracket
# stamps around the dispatch/rc window).
dsh "printf '' > $DTMP/qd.low_bat_check.$DM_NONCE"
LBC_STOPPED=1
lbc_pid="$(rig_daemon_stop low_bat_check)"
dsh "date +%s > $DTMP/qstop.ts"
dsh "date +%s > $DTMP/opk.start.ts"
dsh "setsid sh -lc '$DTMP/opkmnt/mlfk-foh.sh' </dev/null >/dev/null 2>&1 & sleep 2"
sleep 10 # 500 paced ticks ~ 8.4 s
opk_done=0
for _ in $(seq 1 15); do
  if dsh "test -f /tmp/mlfk/opkfoh/opk.rc" >/dev/null 2>&1; then opk_done=1; break; fi
  sleep 2
done
[ "$opk_done" = 1 ] || fail "OPK evidence run never finished (opk.rc absent)"
dsh "date +%s > $DTMP/opk.end.ts"
# restore-FIRST (iter 78/80 discipline) before any pull/judge chores
if rig_daemon_restore low_bat_check /etc/init.d/S12low-bat-check "$DTMP/qrestore.ts"; then
  dsh "rm -f $DTMP/qd.low_bat_check.$DM_NONCE"
  dsh "test ! -f $DTMP/qd.low_bat_check.$DM_NONCE"
  LBC_STOPPED=0
else
  fail "OPK leg: low_bat_check did not verify as running after restart"
fi
qstop_ts="$(rig_dev_ts "$DTMP/qstop.ts")" || exit 1
opkstart_ts="$(rig_dev_ts "$DTMP/opk.start.ts")" || exit 1
opkend_ts="$(rig_dev_ts "$DTMP/opk.end.ts")" || exit 1
qrestore_ts="$(rig_dev_ts "$DTMP/qrestore.ts")" || exit 1
rig_quiesce_bracket_assert "foh opk-evidence low_bat_check" \
  "$qstop_ts" "$opkstart_ts" "$opkend_ts" "$qrestore_ts" \
  "$QW_PRE_SLACK_S" "$QW_POST_SLACK_S" || exit 1
pullv /tmp/mlfk/opkfoh/opk.rc "$BUILD/opk.rc"
cmp -s "$BUILD/opk.rc" <(printf 'RC=0\n') || {
  dsh "cat /tmp/mlfk/opkfoh/mlfk-foh.log" >&2 || true
  fail "OPK evidence rc file is not EXACTLY 'RC=0<newline>' (got '$(cat "$BUILD/opk.rc")')"
}
pullv /tmp/mlfk/opkfoh/boot-marker "$BUILD/opk-boot-marker"
rm -f "$BUILD/opk-boot-want"
printf 'MLFK FOH BOOT\nbin %s\nmode evidence\n' "$STAMP_FOH_SHA" > "$BUILD/opk-boot-want"
cmp "$BUILD/opk-boot-marker" "$BUILD/opk-boot-want" \
  || fail "OPK boot marker != expected (MOUNTED foh_device sha must equal the arm-build stamp record — the launcher must run the FOH binary from the OPK)"
pullv /tmp/mlfk/opkfoh/foh-trace.txt "$BUILD/opk.dev-trace.txt"
node "$FOH/judge-foh-trace.js" "$BUILD/opk.dev-trace.txt" f01-vs-g01 0
norm "$BUILD/opk.dev-trace.txt" "$BUILD/opk.dev-trace.norm"
rm -f "$BUILD/opk.norm-want"
printf 'FOHTRACE1 flow=f01-vs-g01\nSHOT F startup\nT F startup title timer\nSHOT F title\nEND F transitions=1\n' > "$BUILD/opk.norm-want"
cmp "$BUILD/opk.dev-trace.norm" "$BUILD/opk.norm-want" \
  || fail "OPK evidence trace (normalized) != the constructed startup->title expectation"
# BOUNDED-DELTA judgment on the OPK verdict path (iter 97, review-95
# M-d — the flow legs' judge, same frozen bounds): the constructed
# expectation is the FROZEN f01.expect's own pre-input prefix (header
# + SHOT 200 startup + T 370 timer transition + SHOT 373 title —
# frozen bytes, measured-identical to the archived green OPK trace:
# |T-F| = 0 everywhere, AGENT-LOG iter 97) + END == foh-max EXACT.
# `input-free` is the leg's EXPLICIT declaration (no fk_input runs in
# the evidence branch — a structural fact, never inferred); the
# normalizer dies rc 3 without it and rc 2 if it ever anchors.
rm -f "$BUILD/opk.bounded-want"
head -n 4 "$FLOWS/f01-vs-g01.expect" > "$BUILD/opk.bounded-want"
printf 'END %s transitions=1\n' "$OPK_FOHMAX" >> "$BUILD/opk.bounded-want"
made "$BUILD/opk.bounded-want"
node "$FOH/normalize-foh-trace.js" --bounded "$BUILD/opk.bounded-want" \
  "$BUILD/opk.dev-trace.txt" "$FLOWS/f01-vs-g01.flow" "$OPK_FOHMAX" \
  input-free \
  || fail "OPK leg: BOUNDED-DELTA judgment failed (startup-phase stall or END != foh-max)"
# strict skip-summary parse, flow-leg posture verbatim (iter 97, M-d):
# mlfk-foh.log carries foh_device's stderr (the frozen launcher
# appends it); launched=0, shots=2, skips==0, failed presents==0.
pullv /tmp/mlfk/opkfoh/mlfk-foh.log "$BUILD/opk.mlfk-foh.log"
parse_foh_summary "$BUILD/opk.mlfk-foh.log" 0 2
[ "$foh_skips" = 0 ] || fail "OPK leg: $foh_skips FOH render skips (want 0; quiesced leg)"
[ "$foh_fails" = 0 ] || fail "OPK leg: $foh_fails failed presents in the OPK FOH phase"
[ "$foh_transitions" = 1 ] || fail "OPK leg: summary transitions $foh_transitions != 1"
# T13 — T11's stall shape wired into the OPK leg at COPY level (iter
# 97, M-d): +200 ticks on the >=300 suffix of an opk trace COPY still
# PASSES elision (asserted — that is the hole) but DIES in the
# bounded judge (rc exactly 3).
node -e '
  const fs = require("fs");
  const [src, dst, cutS, shiftS] = process.argv.slice(1);
  const cut = Number(cutS), shift = Number(shiftS);
  const lines = fs.readFileSync(src, "utf8").slice(0, -1).split("\n");
  const out = lines.map((ln) => {
    const m = /^(T|S|SHOT|LAUNCH|END) ([0-9]+) (.*)$/.exec(ln);
    if (!m) return ln;
    const t = Number(m[2]);
    return t >= cut ? m[1] + " " + (t + shift) + " " + m[3] : ln;
  });
  fs.writeFileSync(dst, out.join("\n") + "\n");
' "$BUILD/opk.dev-trace.txt" "$BUILD/tooth-opkstall.trace" 300 200 \
  || fail "T13: could not derive the OPK stall variant"
made "$BUILD/tooth-opkstall.trace"
cmp -s "$BUILD/tooth-opkstall.trace" "$BUILD/opk.dev-trace.txt" && \
  fail "T13: OPK stall variant is byte-identical (dead tooth)"
norm "$BUILD/tooth-opkstall.trace" "$BUILD/tooth-opkstall.norm"
cmp -s "$BUILD/tooth-opkstall.norm" "$BUILD/opk.norm-want" \
  || fail "T13: the OPK stall variant should still PASS elision (the bounded judge must be the killer)"
rc=0
node "$FOH/normalize-foh-trace.js" --bounded "$BUILD/opk.bounded-want" \
  "$BUILD/tooth-opkstall.trace" "$FLOWS/f01-vs-g01.flow" "$OPK_FOHMAX" \
  input-free >/dev/null 2>&1 || rc=$?
[ "$rc" = 3 ] || fail "T13: OPK +200-tick stall bounded rc $rc (want exactly 3)"
teeth=$((teeth + 1))
echo "    T13 OK: OPK-leg stall copy passes elision but DIES in the bounded judge (rc 3)"
# T14 — anchor-null posture binds BOTH directions (iter 97, review-95
# L-a): (a) the OPK trace WITHOUT the input-free declaration dies rc 3
# (silent cadence-skip is dead); (b) an anchored flow trace WITH the
# declaration dies rc 2 (stale declaration).
rc=0
node "$FOH/normalize-foh-trace.js" --bounded "$BUILD/opk.bounded-want" \
  "$BUILD/opk.dev-trace.txt" "$FLOWS/f01-vs-g01.flow" "$OPK_FOHMAX" \
  >/dev/null 2>&1 || rc=$?
[ "$rc" = 3 ] || fail "T14a: anchor-null WITHOUT input-free rc $rc (want exactly 3 — bounded mode must never succeed silently without cadence judgment)"
t14_end="$(grep -E '^END (0|[1-9][0-9]*)$' "$FLOWS/f01-vs-g01.flow" | awk '{print $2}')"
[[ "$t14_end" =~ ^(0|[1-9][0-9]{0,5})$ ]] || fail "T14: f01 END grammar ('$t14_end')"
t14_max=$(( (8200 + (t14_end - 370) * 50) * 3 / 50 + 600 ))
rc=0
node "$FOH/normalize-foh-trace.js" --bounded "$FLOWS/f01-vs-g01.expect" \
  "$BUILD/f01-vs-g01.dev-trace.txt" "$FLOWS/f01-vs-g01.flow" "$t14_max" \
  input-free >/dev/null 2>&1 || rc=$?
[ "$rc" = 2 ] || fail "T14b: anchored trace WITH input-free rc $rc (want exactly 2 — stale declaration must die)"
teeth=$((teeth + 1))
echo "    T14 OK: anchor-null is fatal undeclared (rc 3) and a stale input-free declaration is fatal (rc 2)"
for sname in startup title; do
  pullv "/tmp/mlfk/opkfoh/shots/$sname.ppm" "$BUILD/opk.dev-shot-$sname.ppm"
  judge_dev_shot "opk/$sname" "$BUILD/opk.dev-shot-$sname.ppm" \
    "$BUILD/twin-f01-vs-g01-a/shots/$sname.ppm"
done
# present witness on the OPK evidence shots too (both tick-indexed)
pullv /tmp/mlfk/opkfoh/fbwit.txt "$BUILD/opk.fbwit.txt"
judge_fbwit "$BUILD/opk.fbwit.txt" f01-vs-g01 startup title
FBWIT_TOTAL=$((FBWIT_TOTAL + 2))
dsh "umount $DTMP/opkmnt"
OPK_MOUNTED=0
dsh "rm -f $DEVAPPS/$OPK_NAME"
dsh "test ! -f $DEVAPPS/$OPK_NAME"
OPK_INSTALLED=0
# evidence-mode sentinel removal + absence verify (iter 95, review-93
# M2): $DSD/foh-args flips mlfk-foh.sh into the evidence branch by mere
# presence — it must NOT outlive this check (a later launch off
# /mnt/mlfk-scratch would silently run the bounded evidence branch
# instead of live play). RC-verified BEFORE the verdict; trap-covered.
dsh "rm -f $DSD/foh-args"
dsh "test ! -e $DSD/foh-args" \
  || fail "evidence-mode sentinel $DSD/foh-args could not be verified gone"
echo "   OPK OK (mounted from $DEVAPPS, launcher entered the FOH, boot marker bin==stamp, in-app shots byte-exact + fb-witnessed; evidence OPK + foh-args sentinel removed)"

# --- [8b] DEVICE tooth: swapped injector mapping through the REAL chain ----------
# (iter 95, review-93 H2 part 2 — permanent, runs every check): a
# deliberately A<->B-swapped INJECTOR mapping (generated keymap COPY —
# committed bytes never edited) drives the f01 flow through the REAL
# fk_input -> uinput -> SDL keysyms -> platform_poll chain; the judge
# MUST DIE on the resulting device trace (normalized cmp rc exactly 1).
# A clean pass here would mean the comparator/chain is BLIND — fatal.
echo "== [8b] device tooth: A<->B-swapped injector mapping must die at the judge =="
rm -rf "$BUILD/tooth-devswap"
mkdir -p "$BUILD/tooth-devswap"
node -e '
  const fs = require("fs");
  const [src, dst] = process.argv.slice(1);
  const raw = fs.readFileSync(src, "utf8");
  const out = raw.split("\n").map((ln) => {
    if (ln === "map a A a") return "map a A b";
    if (ln === "map b B b") return "map b B a";
    return ln;
  }).join("\n");
  if (out === raw) { console.error("swap was a no-op"); process.exit(1); }
  fs.writeFileSync(dst, out);
' "$FOH/keymap-frozen.txt" "$BUILD/tooth-devswap/keymap.txt" \
  || fail "T-devswap: could not derive the swapped keymap copy"
made "$BUILD/tooth-devswap/keymap.txt"
cmp -s "$BUILD/tooth-devswap/keymap.txt" "$FOH/keymap-frozen.txt" && \
  fail "T-devswap: swapped keymap copy is byte-identical to the frozen file (dead tooth)"
node "$FOH/flow-to-fkscript.js" "$FLOWS/f01-vs-g01.flow" \
  "$BUILD/tooth-devswap/f01.fks" "$BUILD/tooth-devswap/keymap.txt" >/dev/null \
  || fail "T-devswap: tooth fk-script derivation failed"
made "$BUILD/tooth-devswap/f01.fks"
cmp -s "$BUILD/tooth-devswap/f01.fks" "$BUILD/f01-vs-g01.fks" && \
  fail "T-devswap: tooth fks is byte-identical to the real fks (dead tooth)"
adb -s "$DEV" push "$BUILD/tooth-devswap/f01.fks" "$DTMP/toothswap.fks" >/dev/null
hsum="$(rig_host_sha256 "$BUILD/tooth-devswap/f01.fks")" || exit 1
dsum="$(rig_dev_sha256 "$DTMP/toothswap.fks")" || exit 1
[ "$dsum" = "$hsum" ] || fail "T-devswap: pushed tooth fks sha mismatch"
tswap_end="$(grep -E '^END (0|[1-9][0-9]*)$' "$FLOWS/f01-vs-g01.flow" | awk '{print $2}')"
[[ "$tswap_end" =~ ^(0|[1-9][0-9]{0,5})$ ]] || fail "T-devswap: f01 END grammar"
tswap_max=$(( (8200 + (tswap_end - 370) * 50) * 3 / 50 + 600 ))
rm -f "$BUILD/toothswap-launch.sh"
cat > "$BUILD/toothswap-launch.sh" << EOF
#!/bin/sh
# generated by check-device-foh.sh — device-tooth leg launcher
cd $DTMP || exit 9
rm -rf toothswap.apprc toothswap.ready toothswap-shots foh.pid.$DM_NONCE
mkdir -p toothswap-shots
setsid sh -c './foh_device --flow $DTMP/f01-vs-g01.flow --input poll \\
  --flow-out $DTMP/toothswap.trace.txt --shots-dir $DTMP/toothswap-shots \\
  --ready-file $DTMP/toothswap.ready --foh-max $tswap_max --pace 1 \\
  --budget-ns $BUDGET_NS \\
  2> $DTMP/toothswap.applog.txt & \\
  echo \$! > $DTMP/foh.pid.$DM_NONCE; \\
  wait \$!; arc=\$?; \\
  echo "RC=\$arc" > $DTMP/toothswap.apprc' \\
  </dev/null >/dev/null 2>&1 &
sleep 2
EOF
made "$BUILD/toothswap-launch.sh"
adb -s "$DEV" push "$BUILD/toothswap-launch.sh" "$DTMP/" >/dev/null
hsum="$(rig_host_sha256 "$BUILD/toothswap-launch.sh")" || exit 1
dsum="$(rig_dev_sha256 "$DTMP/toothswap-launch.sh")" || exit 1
[ "$dsum" = "$hsum" ] || fail "T-devswap: pushed launcher sha mismatch"
dsh "chmod +x $DTMP/toothswap-launch.sh"
dsh "sh -lc $DTMP/toothswap-launch.sh"
ready=0
for _ in $(seq 1 "$READY_TRIES"); do
  if dsh "test -f $DTMP/toothswap.ready" >/dev/null 2>&1; then ready=1; break; fi
  sleep 1
done
if [ "$ready" != 1 ]; then
  dsh "cat $DTMP/toothswap.applog.txt" >&2 || true
  fail "T-devswap: app ready marker never appeared (${READY_TRIES}s)"
fi
dsh "sh -lc 'cd $DTMP && ./fk_input toothswap.fks'" \
  || fail "T-devswap: fk_input injector failed"
sleep 15 # the swapped run cannot LAUNCH and runs to foh-max (~21 s)
tswap_done=0
for _ in $(seq 1 30); do
  if dsh "test -f $DTMP/toothswap.apprc" >/dev/null 2>&1; then tswap_done=1; break; fi
  sleep 2
done
if [ "$tswap_done" != 1 ]; then
  dsh "cat $DTMP/toothswap.applog.txt" >&2 || true
  fail "T-devswap: app rc file never appeared"
fi
pullv "$DTMP/toothswap.apprc" "$BUILD/toothswap.apprc"
if ! cmp -s "$BUILD/toothswap.apprc" <(printf 'RC=0\n'); then
  dsh "cat $DTMP/toothswap.applog.txt" >&2 || true
  fail "T-devswap: tooth app rc is not EXACTLY 'RC=0<newline>' (want a clean run with a DIVERGENT trace)"
fi
pullv "$DTMP/toothswap.trace.txt" "$BUILD/toothswap.dev-trace.txt"
norm "$BUILD/toothswap.dev-trace.txt" "$BUILD/toothswap.dev-trace.norm"
rc=0
cmp -s "$BUILD/toothswap.dev-trace.norm" "$BUILD/f01.norm.expect" || rc=$?
[ "$rc" = 1 ] || fail "T-devswap: normalized swapped-chain DEVICE trace cmp rc $rc vs frozen f01 (want exactly 1 — rc 0 means the injector/backend chain is BLIND to an A/B swap)"
teeth=$((teeth + 1))
echo "    T-devswap OK: A<->B-swapped injector mapping DIES at the judge on the REAL uinput->SDL->platform_poll chain"

# --- park restore + deadman cancel ------------------------------------------------
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

# --- [9] device-artifact teeth + hygiene ------------------------------------------
echo "== [9/9] teeth (device artifacts, on COPIES) + hygiene =="
# T6: one byte appended to a COPY of a device shot -> the production
# judge (judge_dev_shot) dies
cp "$BUILD/f01-vs-g01.dev-shot-css.ppm" "$BUILD/tooth-shot.ppm"
printf 'x' >> "$BUILD/tooth-shot.ppm"
rc=0
( judge_dev_shot "tooth" "$BUILD/tooth-shot.ppm" \
    "$BUILD/twin-f01-vs-g01-a/shots/css.ppm" ) 2>/dev/null || rc=$?
[ "$rc" != 0 ] || fail "T-shot: the production shot judge accepted a corrupted device shot"
teeth=$((teeth + 1))
echo "    T6 OK: corrupted device-shot copy dies in the production judge"
# T7: nibble-flip in a COPY of the f01 device run JSON -> verify-stream rc 2
node -e '
  const fs = require("fs");
  const j = JSON.parse(fs.readFileSync(process.argv[1], "utf8"));
  const h = j.frames[41].h;
  j.frames[41].h = (h[0] === "0" ? "1" : "0") + h.slice(1);
  fs.writeFileSync(process.argv[2], JSON.stringify(j));
' "$BUILD/f01.dev-run.json" "$BUILD/tooth-run.json"
made "$BUILD/tooth-run.json"
rc=0
node oracle/harness/verify-stream.js "$BUILD/tooth-run.json" "$FROZEN" >/dev/null 2>&1 || rc=$?
[ "$rc" = 2 ] || fail "T-stream: verify-stream rc $rc on a nibble-flipped run copy (want 2)"
teeth=$((teeth + 1))
echo "    T7 OK: nibble-flipped device run copy dies in verify-stream (rc 2)"
# T8: lcancel substitution in a COPY of the f03 device bstate -> cmp dies
sed 's/lcancel=2/lcancel=1/' "$BUILD/f03-options.dev-bstate.txt" > "$BUILD/tooth-bstate.txt"
cmp -s "$BUILD/tooth-bstate.txt" "$BUILD/f03-options.dev-bstate.txt" && \
  fail "T-bstate: the substitution was a no-op (dead tooth)"
rc=0
cmp -s "$BUILD/tooth-bstate.txt" "$FLOWS/f03-options.bstate.expect" || rc=$?
[ "$rc" = 1 ] || fail "T-bstate: perturbed bstate copy cmp rc $rc vs frozen (want 1)"
teeth=$((teeth + 1))
echo "    T8 OK: perturbed bstate copy dies against the frozen witness"
# T9: boot-marker perturbation (COPY) -> the exact-bytes compare dies
sed 's/^mode evidence$/mode live/' "$BUILD/opk-boot-marker" > "$BUILD/tooth-marker"
rc=0
cmp -s "$BUILD/tooth-marker" "$BUILD/opk-boot-want" || rc=$?
[ "$rc" = 1 ] || fail "T-marker: perturbed boot-marker copy cmp rc $rc (want 1)"
teeth=$((teeth + 1))
echo "    T9 OK: perturbed boot-marker copy dies against the constructed expectation"
# T10: fb-witness rows (COPY): eq flipped + a resembling row -> the
# production witness judge dies (iter 95, review-93 H1)
sed 's/ eq=1$/ eq=0/' "$BUILD/f01-vs-g01.fbwit.txt" > "$BUILD/tooth-fbwit.txt"
cmp -s "$BUILD/tooth-fbwit.txt" "$BUILD/f01-vs-g01.fbwit.txt" && \
  fail "T-fbwit: eq substitution was a no-op (dead tooth)"
rc=0
# shellcheck disable=SC2086
( judge_fbwit "$BUILD/tooth-fbwit.txt" f01-vs-g01 ${FLOW_SHOTS[0]} ) 2>/dev/null || rc=$?
[ "$rc" != 0 ] || fail "T-fbwit: witness judge accepted eq=0 rows"
{ head -n 1 "$BUILD/f01-vs-g01.fbwit.txt"; echo "W 1 startup yoff=999 eq=1"; \
  tail -n +2 "$BUILD/f01-vs-g01.fbwit.txt"; } > "$BUILD/tooth-fbwit2.txt"
rc=0
# shellcheck disable=SC2086
( judge_fbwit "$BUILD/tooth-fbwit2.txt" f01-vs-g01 ${FLOW_SHOTS[0]} ) 2>/dev/null || rc=$?
[ "$rc" != 0 ] || fail "T-fbwit: witness judge accepted a resembling extra row"
# TORN TRAILER (iter 97, review-95 M-a — the reviewer's exact shape):
# header + all rows + an UNTERMINATED END fragment; grep counts the
# fragment as a line while `read` drops it, so the round-1 reader
# PASSED this file — the strict reader must die on the missing
# trailing newline / reader-vs-count mismatch.
n_f01_fbwit="$(grep -c "" "$BUILD/f01-vs-g01.fbwit.txt")" || fail "T-fbwit: count failed"
{ head -n "$((n_f01_fbwit - 1))" "$BUILD/f01-vs-g01.fbwit.txt"; \
  printf 'END shots=6'; } > "$BUILD/tooth-fbwit3.txt"
[ -n "$(tail -c 1 "$BUILD/tooth-fbwit3.txt")" ] || fail "T-fbwit: torn variant unexpectedly newline-terminated (dead tooth)"
rc=0
# shellcheck disable=SC2086
( judge_fbwit "$BUILD/tooth-fbwit3.txt" f01-vs-g01 ${FLOW_SHOTS[0]} ) 2>/dev/null || rc=$?
[ "$rc" != 0 ] || fail "T-fbwit: witness judge accepted a TORN unterminated trailer"
# yoff drift (iter 97, review-95 L-b): a back-page row (yoff=240) must
# die — the measured pan-reject policy pins yoff=0 exactly.
sed 's/^W \([0-9]*\) startup yoff=0/W \1 startup yoff=240/' \
  "$BUILD/f01-vs-g01.fbwit.txt" > "$BUILD/tooth-fbwit4.txt"
cmp -s "$BUILD/tooth-fbwit4.txt" "$BUILD/f01-vs-g01.fbwit.txt" && \
  fail "T-fbwit: yoff substitution was a no-op (dead tooth)"
rc=0
# shellcheck disable=SC2086
( judge_fbwit "$BUILD/tooth-fbwit4.txt" f01-vs-g01 ${FLOW_SHOTS[0]} ) 2>/dev/null || rc=$?
[ "$rc" != 0 ] || fail "T-fbwit: witness judge accepted yoff=240 (the pan-reject pin must kill any non-zero page)"
teeth=$((teeth + 1))
echo "    T10 OK: perturbed fb-witness copies (eq=0, resembling row, TORN trailer, yoff=240) die in the production witness judge"
# T11: bounded-delta teeth (COPIES of the fresh f01 device trace; the
# review-93 M1 scenario — a mid-run stall the elision normalizes away)
f01_end="$(grep -E '^END (0|[1-9][0-9]*)$' "$FLOWS/f01-vs-g01.flow" | awk '{print $2}')"
f01_max=$(( (8200 + (f01_end - 370) * 50) * 3 / 50 + 600 ))
node -e '
  const fs = require("fs");
  const [src, dst, cutS, shiftS] = process.argv.slice(1);
  const cut = Number(cutS), shift = Number(shiftS);
  const lines = fs.readFileSync(src, "utf8").slice(0, -1).split("\n");
  const out = lines.map((ln) => {
    const m = /^(T|S|SHOT|LAUNCH|END) ([0-9]+) (.*)$/.exec(ln);
    if (!m) return ln;
    const t = Number(m[2]);
    return t >= cut ? m[1] + " " + (t + shift) + " " + m[3] : ln;
  });
  fs.writeFileSync(dst, out.join("\n") + "\n");
' "$BUILD/f01-vs-g01.dev-trace.txt" "$BUILD/tooth-stall.trace" 600 200 \
  || fail "T-bounded: could not derive the stall variant"
made "$BUILD/tooth-stall.trace"
cmp -s "$BUILD/tooth-stall.trace" "$BUILD/f01-vs-g01.dev-trace.txt" && \
  fail "T-bounded: stall variant is byte-identical (dead tooth)"
norm "$BUILD/tooth-stall.trace" "$BUILD/tooth-stall.norm"
cmp -s "$BUILD/tooth-stall.norm" "$BUILD/f01.norm.expect" \
  || fail "T-bounded: the stall variant should still PASS elision (it must be the bounded judge that kills it)"
rc=0
node "$FOH/normalize-foh-trace.js" --bounded "$FLOWS/f01-vs-g01.expect" \
  "$BUILD/tooth-stall.trace" "$FLOWS/f01-vs-g01.flow" "$f01_max" \
  >/dev/null 2>&1 || rc=$?
[ "$rc" = 2 ] || [ "$rc" = 3 ] || fail "T-bounded: mid-run +200-tick stall rc $rc (want 3, or 2 if structure broke)"
[ "$rc" = 3 ] || fail "T-bounded: stall variant died rc 2 (structural), want the BOUND arm rc 3"
teeth=$((teeth + 1))
echo "    T11 OK: mid-run +200-tick stall passes elision but DIES in the bounded judge (rc 3)"

rig_no_commit_guard "$BUILD" "$DEVB" "$TABLES" "$AUDIO_OUT"

# opk=evidence (iter 95, review-93 M3): the OPK leg proves the
# mount-and-run EVIDENCE path only — frontend-nav launch + the live
# branch are task 14's gate leg, deliberately NOT claimed here.
echo "DEVICE FOH OK (flows=5 shots=13 bridge=1 states=3 opk=evidence fbwit=$FBWIT_TOTAL p99=${full_p99_ms}ms skips=0 underruns=0 starves=0 starts${DEV_STARTS} teeth=$teeth)"
