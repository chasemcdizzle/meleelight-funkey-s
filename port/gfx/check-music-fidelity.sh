#!/usr/bin/env bash
# check-music-fidelity.sh — M4 task 7 HOST leg: music-channel fidelity by
# OFFLINE DETERMINISTIC RENDER DIFFERENTIAL (fix_plan §M4 task 7;
# pre-registration AGENT-LOG iter 87). The mixer plane gained a dedicated
# MUSIC channel (snd_mixer.h; ring/refill = the device streaming policy
# run synchronously by snd_render.c); the INDEPENDENT reference
# (snd_reference.js --music-track) implements the same DOCUMENTED
# semantics separately (sprite program Start-once-then-Loop-repeating,
# floor(ms*441/20) quantization, ZOH 2x, Q8 gain per channel, sum before
# the single S16 clamp, window-past-EOF = silence — the fod quirk).
#
# Composes, in order:
#   [0] PRODUCER BYTE PINS (check-sim.sh / wrap-run.js / verify-stream.js
#       — the mixer check's table wholesale) + INVENTORY-EXECUTION
#       BINDING (both manifests' full id+name inventories == the pinned
#       arrays, both directions) + the STAGE->TRACK map pinned from
#       upstream main.js:1342-1360 (startGame's switch(stageSelect) —
#       DETERMINISTIC, zero seeded-RNG involvement; the iter-87 seam
#       survey verdict: music selection is RENDER-PLANE) + the MUSIC
#       track sha256 pin table (8 rows, measured-then-frozen iter 87;
#       ffmpeg is 3-way pinned so blob bytes are stable) + a strict
#       whitelist extractor for per-track volbits/sprite ms.
#   [1] SIM CONFORMS — bash port/sim/check-sim.sh (pinned; the forced-
#       cold rm-before-produce shape — provides sim_host, tables,
#       simdata, g07/g08 AIBRIDGE1 for the tapped replays).
#   [2] AUDIO — fresh pipeline audio stage into pipeline/build/
#       audio-musicfid, MUSIC pcm shas verified against the pin table,
#       pack x2 byte-identical + SNDPACK_SHA256 pin, cross-checked
#       against check-mixer-fidelity.sh's constant (one pack identity
#       across the audio surface).
#   [3] builds — sim_host_snd (event tap) + snd_render (the mixer
#       check's exact recipes; every TU -ffp-contract=off -Wall -Wextra
#       -Werror).
#   [4] THE DIFFERENTIAL — for EVERY golden (8 oracle + m01/m02 +
#       s01/s02): tapped replay STREAM-MATCH-judged by the PINNED-
#       UNCHANGED verify-stream.js (instrumentation cannot perturb the
#       sim), then C render x2 byte-stable WITH the stage's music track
#       vs the independent reference (--voices 8 --music-track) — cmp
#       BIT-IDENTICAL; musout == frames*735 on BOTH sides,
#       musstarves == 0, musrefills > 0 (the ring/refill path really
#       streamed), steals/maxvoices equality C-vs-ref. BASIS: capped-8
#       + music (the device contract). The unlimited-voice exposure and
#       the per-golden exposure pins are task 6's surface
#       (check-mixer-fidelity.sh) — NOT re-judged here; the no-music
#       byte-identity regression also lives there.
#   [5] SYNTHETIC LEGS (empty schedules — pure music; the tracks and
#       program arms no 60 s match reaches): menu@10900 frames (the
#       earliest loop WRAP: startDur+loopDur = 3,989,396 src frames =
#       sim frame ~10857), fod@20000 (EOF-silence overhang from ~304 s
#       — fileFrames 6,703,200 < loop end 7,322,849 — PLUS its wrap at
#       ~frame 19927), targettest@600 (short-intro chaining, startDur
#       22 src frames). C vs ref cmp BIT-IDENTICAL each.
#   [6] TEETH (pre-registered T1-T4 + grammar): T1 --tooth-music-gain
#       (g01) diverges; T2 --tooth-music-loop-beg (g01) diverges; T3
#       --tooth-music-loop-dur (menu WRAP leg — fires only past a wrap)
#       diverges; T4 --tooth-music-underfill -> verdict musstarves > 0
#       AND diverges (starve accounting is load-bearing); grammar teeth:
#       bad volbits / malformed ms pair (C) + unknown track (ref) die
#       nonzero. Positive controls throughout.
#   [7] no-commit guard (rc case-split).
#
# Prints `MUSIC FIDELITY OK (goldens=12 tracks=8 diff=bit-identical
# wraps=2 eofsilence=1)`, exit 0; ANY divergence, stream mismatch, pin
# mismatch, count disagreement, or missing artifact -> nonzero.
#
# HONEST EXPOSURE (PROCESS §8): the differential proves IMPLEMENTATION
# equality of two independent realizations; the documented device
# adaptations themselves (quantization, ZOH, Q8, frame-boundary starts)
# are shared BY DOCUMENTATION and a shared misreading is invisible here
# (the task-6 exposure class). Browser-audible authority = Chase (M4
# acceptance). Menu/targettest SELECTION is FOH's surface (tasks 9-12);
# this check covers their WINDOWS via the synthetic legs.
#
# PROVENANCE: music PCM + rendered output derive from Nintendo-derived
# blobs — gitignored build output only, never committed (guarded below;
# the sha PINS are hashes, not bytes).
set -euo pipefail
cd "$(dirname "$0")/../.."

GFX=port/gfx
B=$GFX/build
MFI=$B/musicfid
SIM=port/sim/sim
CAL=port/sim/calib
TABLES=pipeline/build/sim-tables
AUDIO_OUT=pipeline/build/audio-musicfid
M4G=port/goldens-m4
DIST="${MELEELIGHT_CLONE:-$HOME/.cache/meleelight-funkey-s/upstream}"

# Pack identity (measured-then-frozen, iter 57; must equal the
# check-mixer-fidelity.sh AND check-device-audio.sh pins — cross-checked
# below: one pack identity across the audio surface).
SNDPACK_SHA256=f69579082fe569249879faa5ceccb7a810d94d8092695ddc8bb543f3bda3ccb4

host_sha256() { shasum -a 256 "$1" | cut -d' ' -f1; }

fail() { echo "MUSIC FIDELITY FAIL: $1" >&2; exit 1; }
grammar_die() { echo "MUSIC FIDELITY FAIL: $1" >&2; exit 2; }

# made <file...> — rm-before-produce freshness guard (riglib pattern).
made() {
  local f
  for f in "$@"; do
    if ! [ -s "$f" ]; then
      fail "artifact $f missing or empty after its producer ran (rm-before-produce freshness guard)"
    fi
  done
}

# count_e — grep count with the rc CASE-SPLIT (rc>=2 = read error DIES).
count_e() {
  local c rc=0
  c="$(grep -cE -- "$2" "$1")" || rc=$?
  if [ "$rc" -ge 2 ]; then
    grammar_die "count helper — grep -cE rc $rc reading '$1' (a read error is CORRUPT evidence, never a 0 count)"
  fi
  printf '%s' "$c"
}

# judge_verdict_file <file> <full-line ERE> <resemblance ERE> — exactly
# one newline-terminated line matching the full grammar (file bytes,
# never $(cat); the iter-84 posture).
judge_verdict_file() {
  local f="$1" full="$2" resem="$3" nl c r
  test -s "$f" || grammar_die "verdict file $f missing or empty"
  nl="$(grep -c '' "$f")" || grammar_die "cannot count lines of $f"
  [ "$nl" = 1 ] || grammar_die "verdict file $f has $nl lines, want exactly 1 (extra or missing lines are CORRUPTION)"
  if [ -n "$(tail -c 1 "$f")" ]; then
    grammar_die "verdict file $f lacks the trailing newline (torn write)"
  fi
  c="$(count_e "$f" "$full")"
  [ "$c" = 1 ] || grammar_die "verdict file $f: $c lines match the full grammar '$full', want exactly 1"
  r="$(count_e "$f" "$resem")"
  [ "$r" = 1 ] || grammar_die "verdict file $f: $r lines RESEMBLE the verdict ('$resem') but 1 matches the full grammar — a resembling non-matching line is CORRUPTION, never ignorable"
}

# --- [0] PRODUCER BYTE PINS (the mixer check's table wholesale) --------------
# UPDATE DISCIPLINE (binding): a reviewed change to a pinned producer
# updates its line below IN THE SAME COMMIT (shasum -a 256 <path>).
PRODUCER_PINS="\
ce0882bee2a0bb0ad11ac51366ef467c3811d832f9dc932c4eb10dd3ccc4c8cb port/sim/check-sim.sh
b835b5f886225e0015dae152576eea5a42fa69d7ba0699f4de0e31438d05c5b9 port/sim/sim/wrap-run.js
f420723433b19166b53a80aedf54931ffdfbc6d2505c773fd73b7a13bbcdf60e oracle/harness/verify-stream.js"
N_PINS_WANT=3
n_pins=0
while IFS= read -r pline; do
  [ -n "$pline" ] || continue
  if ! [[ "$pline" =~ ^[0-9a-f]{64}\ [A-Za-z0-9._/-]+$ ]]; then
    fail "producer pin table — line fails the anchored grammar: '$pline'"
  fi
  psha="${pline%% *}"
  ppath="${pline#* }"
  test -f "$ppath" || fail "pinned producer $ppath is missing from the tree"
  pout="$(shasum -a 256 "$ppath")" || fail "cannot hash producer $ppath"
  have="${pout%% *}"
  if ! [[ "$have" =~ ^[0-9a-f]{64}$ ]] || [ "$pout" != "$have  $ppath" ]; then
    fail "producer hash read malformed for $ppath: '$pout'"
  fi
  if [ "$have" != "$psha" ]; then
    fail "producer byte pin — $ppath sha256 $have != pinned $psha (a producer change invalidates this check's evidence; a legitimate, reviewed change must update the pin in the same commit)"
  fi
  n_pins=$((n_pins + 1))
done <<< "$PRODUCER_PINS"
if [ "$n_pins" != "$N_PINS_WANT" ]; then
  fail "producer pin inventory — $n_pins pins verified, want exactly $N_PINS_WANT (a dropped pin line is corruption, never a partial pass)"
fi

# --- [0b] INVENTORY-EXECUTION BINDING (the mixer check's arrays) -------------
ORACLE_IDS=(g01 g02 g03 g04 g05 g06 g07 g08)
ORACLE_NAMES=(
  "g01-fox-marth-battlefield"
  "g02-falco-puff-ystory"
  "g03-falcon-fox-pstadium"
  "g04-puff-falcon-dreamland"
  "g05-marth-falco-fdest"
  "g06-falcon-marth-fountain"
  "g07-falco-falcon-battlefield"
  "g08-fox-puff-fdest"
)
M4_IDS=(m01 m02 s01 s02)
M4_NAMES=(
  "m01-falcon-marth-d1-ystory"
  "m02-falcon-fox-d9-dreamland"
  "s01-marth-fox-stops-battlefield"
  "s02-marth-fox-guardon-break-battlefield"
)
BRIDGE_CPU_IDS=(g07 g08)
if [ "${#ORACLE_IDS[@]}" != 8 ] || [ "${#ORACLE_NAMES[@]}" != 8 ] || \
   [ "${#M4_IDS[@]}" != 4 ] || [ "${#M4_NAMES[@]}" != 4 ] || \
   [ "${#BRIDGE_CPU_IDS[@]}" != 2 ]; then
  fail "inventory pin — pinned array lengths off (oracle ${#ORACLE_IDS[@]}/8+${#ORACLE_NAMES[@]}/8, m4 ${#M4_IDS[@]}/4+${#M4_NAMES[@]}/4, bridge ${#BRIDGE_CPU_IDS[@]}/2)"
fi
for arr in ORACLE_IDS ORACLE_NAMES M4_IDS M4_NAMES BRIDGE_CPU_IDS; do
  dupes="$(eval 'printf "%s\n" "${'"$arr"'[@]}"' | sort | uniq -d)"
  if [ -n "$dupes" ]; then
    fail "inventory pin — duplicate entries in $arr: $dupes"
  fi
done
inv="$(node -e '
  const fs = require("fs");
  const m = JSON.parse(fs.readFileSync(process.argv[1], "utf8"));
  process.stdout.write(m.goldens.map((g) => g.id).join(" ") + "|" +
    m.goldens.map((g) => g.name).join(" "));
' oracle/goldens/manifest.json)" || fail "cannot read the oracle manifest"
if [ "$inv" != "${ORACLE_IDS[*]}|${ORACLE_NAMES[*]}" ]; then
  fail "oracle inventory pin — manifest {$inv} != pinned {${ORACLE_IDS[*]}|${ORACLE_NAMES[*]}} (both directions; a grown/shrunk/renamed golden set is a reviewed pin update)"
fi
inv="$(node -e '
  const fs = require("fs");
  const m = JSON.parse(fs.readFileSync(process.argv[1], "utf8"));
  process.stdout.write(m.goldens.map((g) => g.id).join(" ") + "|" +
    m.goldens.map((g) => g.name).join(" "));
' "$M4G/manifest.json")" || fail "cannot read the m4 manifest"
if [ "$inv" != "${M4_IDS[*]}|${M4_NAMES[*]}" ]; then
  fail "m4 inventory pin — manifest {$inv} != pinned {${M4_IDS[*]}|${M4_NAMES[*]}} (both directions; a grown/shrunk/renamed golden set is a reviewed pin update)"
fi

# --- [0c] STAGE -> MUSIC TRACK map (pinned from upstream) --------------------
# main.js:1342-1360 (startGame's switch(stageSelect)): 0 battlefield ·
# 1 yStory · 2 pStadium · 3 dreamland · 4 finald · 5 fod. The iter-87
# seam survey (AGENT-LOG pre-registration): DETERMINISTIC, no seeded
# draws (main.js:1322's Math.random is setBackgroundType — the pinned
# off-step draw); menu/targettest have no match-plane caller (FOH tasks
# 9-12 own their selection — synthetic-leg coverage below).
STAGE_TRACKS=(battlefield yStory pStadium dreamland finald fod)
[ "${#STAGE_TRACKS[@]}" = 6 ] || fail "STAGE_TRACKS pin — ${#STAGE_TRACKS[@]} rows, want 6"

# MUSIC TRACK PINS (measured-then-frozen iter 87 from the pinned
# pipeline audio stage; ffmpeg is 3-way pinned in expected.json so blob
# bytes cannot drift silently — a pin mismatch is pipeline/ffmpeg drift,
# a reviewed re-freeze):  <sha256> <track>
MUSIC_PINS="\
c7c1fa2262389496beaba8854d9ffa254861a14f89741cde0432e61197649f44 battlefield
29c1f859f6c4063e59f157ae9168b48ead7f5e45a7180e9b67b4711362110f2c dreamland
8b0702b52e7ddb95ea3135212a49df63415f0deb5f8602b2cbbe1e8129645df7 finald
d5d305db589c4e8412fa0a5b2709692917ff8cbc16dece8764ab1d2087cea254 fod
bbf52720a559ca7b0cf21837a1425a42fd612719442a006b041c913d5f8c4856 menu
89b8522cba053509eabef3fb9f516207759ec13fd19c0272c362eb6887bdd2ef pStadium
0c922c6f7111e6d90888c3fe692acc2d03306c858135c1501261b36f216a951d targettest
b503d90bc50b79e6ed2f4e62c4c3fc1a4a1d337730debf69ea7f5bad0a436929 yStory"
N_TRACKS_WANT=8

# --- [0d] strict music-config extractor (whitelist parse; PROCESS §3) --------
# read_music <track> — sets M_BLOB M_VOLBITS M_SO M_SD M_LO M_LD from
# $AUDIO_OUT/sounds.json via a hard-throwing node emitter + per-key
# anchored bash whitelists (duplicates refused, exactly 6 lines).
read_music() {
  local track="$1" out line v n=0
  M_BLOB= M_VOLBITS= M_SO= M_SD= M_LO= M_LD=
  [[ "$track" =~ ^[A-Za-z]+$ ]] || grammar_die "read_music — track name '$track' fails the whitelist"
  out="$(node -e '
    const fs = require("fs");
    const s = JSON.parse(fs.readFileSync(process.argv[1], "utf8"));
    const t = process.argv[2];
    function die(m) { console.error("music-extract FAIL: " + m); process.exit(1); }
    if (s.formatVersion !== 1) die("sounds.json formatVersion != 1");
    if (!s.music || typeof s.music !== "object") die("no music map");
    const e = s.music[t];
    if (!e) die("track not in the SND1 map: " + t);
    const bits = e.volume && e.volume.bits;
    if (typeof bits !== "string" || !/^[0-9a-f]{16}$/.test(bits)) die("bad volume bits");
    if (typeof e.blob !== "string" || e.blob !== "audio/music/" + t + ".pcm") {
      die("blob path is not the canonical audio/music/<track>.pcm: " + JSON.stringify(e.blob));
    }
    const sp = e.sprite;
    if (!sp || !Array.isArray(sp.start) || sp.start.length !== 2 ||
        !Array.isArray(sp.loop) || sp.loop.length !== 2) die("bad sprite windows");
    for (const x of [...sp.start, ...sp.loop]) {
      if (!Number.isInteger(x) || x < 0 || x > 1000000000) die("sprite ms outside the sane domain");
    }
    const emit = (k, v) => process.stdout.write(k + "=" + String(v) + "\n");
    emit("blob", e.blob); emit("volbits", bits);
    emit("so", sp.start[0]); emit("sd", sp.start[1]);
    emit("lo", sp.loop[0]); emit("ld", sp.loop[1]);
  ' "$AUDIO_OUT/sounds.json" "$track")" || fail "music config extraction failed for track $track"
  while IFS= read -r line; do
    n=$((n + 1))
    v="${line#*=}"
    case "$line" in
      (blob=*)
        [[ "$v" =~ ^audio/music/[A-Za-z]+\.pcm$ ]] || grammar_die "music grammar — $track blob '$v' fails the whitelist"
        [ -z "$M_BLOB" ] || grammar_die "music grammar — duplicate blob line for $track"
        M_BLOB="$v" ;;
      (volbits=*)
        [[ "$v" =~ ^[0-9a-f]{16}$ ]] || grammar_die "music grammar — $track volbits '$v' not 16 lowercase hex"
        [ -z "$M_VOLBITS" ] || grammar_die "music grammar — duplicate volbits line for $track"
        M_VOLBITS="$v" ;;
      (so=*)
        [[ "$v" =~ ^(0|[1-9][0-9]{0,9})$ ]] || grammar_die "music grammar — $track so '$v' not an exact-token integer"
        [ -z "$M_SO" ] || grammar_die "music grammar — duplicate so line for $track"
        M_SO="$v" ;;
      (sd=*)
        [[ "$v" =~ ^[1-9][0-9]{0,9}$ ]] || grammar_die "music grammar — $track sd '$v' not a positive exact-token integer"
        [ -z "$M_SD" ] || grammar_die "music grammar — duplicate sd line for $track"
        M_SD="$v" ;;
      (lo=*)
        [[ "$v" =~ ^(0|[1-9][0-9]{0,9})$ ]] || grammar_die "music grammar — $track lo '$v' not an exact-token integer"
        [ -z "$M_LO" ] || grammar_die "music grammar — duplicate lo line for $track"
        M_LO="$v" ;;
      (ld=*)
        [[ "$v" =~ ^[1-9][0-9]{0,9}$ ]] || grammar_die "music grammar — $track ld '$v' not a positive exact-token integer"
        [ -z "$M_LD" ] || grammar_die "music grammar — duplicate ld line for $track"
        M_LD="$v" ;;
      (*)
        grammar_die "music grammar — unrecognized line '$line' for $track (whitelist parse)" ;;
    esac
  done <<< "$out"
  [ "$n" = 6 ] || grammar_die "music grammar — $track emitted $n lines, want exactly 6"
}

# --- [0e] STRICT GOLDEN PARAM PARSE (the mixer check's read_golden) ----------
read_golden() {
  local mf="$1" id="$2" out line v n=0
  G_NAME= G_TRACE= G_FRAMES= G_SEED= G_P1= G_P2= G_STAGE= G_CPU= G_DIFF=
  out="$(node -e '
    const fs = require("fs");
    const m = JSON.parse(fs.readFileSync(process.argv[1], "utf8"));
    const g = m.goldens.find((x) => x.id === process.argv[2]);
    if (!g) { console.error("no golden " + process.argv[2] + " in " + process.argv[1]); process.exit(1); }
    const emit = (k, v) => process.stdout.write(k + "=" + String(v) + "\n");
    emit("name", g.name); emit("trace", g.trace); emit("frames", g.frames);
    emit("seed", g.seed); emit("p1", g.p1); emit("p2", g.p2);
    emit("stage", g.stage); emit("cpu", g.cpu); emit("difficulty", g.difficulty);
  ' "$mf" "$id")" || fail "manifest read for $id from $mf failed"
  while IFS= read -r line; do
    n=$((n + 1))
    v="${line#*=}"
    case "$line" in
      (name=*)
        [[ "$v" =~ ^[a-z0-9][a-z0-9-]*$ ]] || grammar_die "manifest grammar — $id name '$v' fails the whitelist [a-z0-9-]"
        [ -z "$G_NAME" ] || grammar_die "manifest grammar — duplicate name line for $id"
        G_NAME="$v" ;;
      (trace=*)
        [[ "$v" =~ ^[a-z0-9][a-z0-9.-]*$ ]] || grammar_die "manifest grammar — $id trace '$v' fails the whitelist (basename characters only)"
        [ -z "$G_TRACE" ] || grammar_die "manifest grammar — duplicate trace line for $id"
        G_TRACE="$v" ;;
      (frames=*)
        [[ "$v" =~ ^[1-9][0-9]{0,5}$ ]] || grammar_die "manifest grammar — $id frames '$v' is not a positive integer"
        [ -z "$G_FRAMES" ] || grammar_die "manifest grammar — duplicate frames line for $id"
        G_FRAMES="$v" ;;
      (seed=*)
        [[ "$v" =~ ^[0-9]{1,10}$ ]] || grammar_die "manifest grammar — $id seed '$v' is not a plain integer"
        [ -z "$G_SEED" ] || grammar_die "manifest grammar — duplicate seed line for $id"
        G_SEED="$v" ;;
      (p1=*)
        [[ "$v" =~ ^[0-4]$ ]] || grammar_die "manifest grammar — $id p1 '$v' outside the char domain 0-4"
        [ -z "$G_P1" ] || grammar_die "manifest grammar — duplicate p1 line for $id"
        G_P1="$v" ;;
      (p2=*)
        [[ "$v" =~ ^[0-4]$ ]] || grammar_die "manifest grammar — $id p2 '$v' outside the char domain 0-4"
        [ -z "$G_P2" ] || grammar_die "manifest grammar — duplicate p2 line for $id"
        G_P2="$v" ;;
      (stage=*)
        [[ "$v" =~ ^[0-5]$ ]] || grammar_die "manifest grammar — $id stage '$v' outside the stage domain 0-5"
        [ -z "$G_STAGE" ] || grammar_die "manifest grammar — duplicate stage line for $id"
        G_STAGE="$v" ;;
      (cpu=*)
        [[ "$v" =~ ^(true|false)$ ]] || grammar_die "manifest grammar — $id cpu '$v' is not a boolean"
        [ -z "$G_CPU" ] || grammar_die "manifest grammar — duplicate cpu line for $id"
        G_CPU="$v" ;;
      (difficulty=*)
        [[ "$v" =~ ^([1-9]|null)$ ]] || grammar_die "manifest grammar — $id difficulty '$v' outside 1-9/null"
        [ -z "$G_DIFF" ] || grammar_die "manifest grammar — duplicate difficulty line for $id"
        G_DIFF="$v" ;;
      (*)
        grammar_die "manifest grammar — unrecognized param line '$line' for $id (whitelist parse; resembles-but-doesn't-match is corruption)" ;;
    esac
  done <<< "$out"
  [ "$n" = 9 ] || grammar_die "manifest grammar — $id emitted $n param lines, want exactly 9"
  if [ "$G_TRACE" != "$G_NAME.trace.json" ]; then
    grammar_die "manifest grammar — $id trace '$G_TRACE' != name-derived '$G_NAME.trace.json' (basename-only by construction)"
  fi
  if [ "$G_CPU" = "true" ]; then
    [ "$G_DIFF" != "null" ] || grammar_die "manifest grammar — $id cpu golden without a difficulty"
  else
    [ "$G_DIFF" = "null" ] || grammar_die "manifest grammar — $id non-cpu golden with a difficulty"
  fi
}

# --- RUN LOCK + SHARED-SCRATCH LOCK (own first, then shared) -----------------
mkdir -p "$B"
LOCK="$B/musicfid.lock"
if ! mkdir "$LOCK" 2>/dev/null; then
  lockage="unknown"
  if lockmtime="$(stat -f %m "$LOCK" 2>/dev/null || stat -c %Y "$LOCK" 2>/dev/null)"; then
    lockage="$(( $(date +%s) - lockmtime )) s"
  fi
  echo "MUSIC FIDELITY REFUSED: run lock $LOCK already exists (age: $lockage)." >&2
  echo "  Another check-music-fidelity.sh run may be rewriting the shared" >&2
  echo "  scratch in $MFI / $AUDIO_OUT right now. NO auto-reclaim (iter-41" >&2
  echo "  posture). If you are sure no run is live, remove it manually:" >&2
  echo "  rm -rf '$LOCK'" >&2
  exit 1
fi
trap 'rm -rf "$LOCK"' EXIT
mkdir -p "$CAL/build"
SLOCK="$CAL/build/shared-scratch.lock"
if ! mkdir "$SLOCK" 2>/dev/null; then
  slockage="unknown"
  if slockmtime="$(stat -f %m "$SLOCK" 2>/dev/null || stat -c %Y "$SLOCK" 2>/dev/null)"; then
    slockage="$(( $(date +%s) - slockmtime )) s"
  fi
  echo "MUSIC FIDELITY REFUSED: shared scratch lock $SLOCK already exists (age: $slockage)." >&2
  echo "  A sibling consumer (check-ai-live.sh / check-vfx-seam.sh /" >&2
  echo "  check-mixer-fidelity.sh) may be rewriting the shared calib build/ +" >&2
  echo "  sim-tables scratch right now. NO auto-reclaim (iter-41 posture). If" >&2
  echo "  you are sure no run is live, remove it manually: rm -rf '$SLOCK'" >&2
  exit 1
fi
trap 'rm -rf "$LOCK" "$SLOCK"' EXIT
mkdir -p "$MFI"

# --- [1] SIM CONFORMS (the pinned M2 gate; forced-cold shared artifacts) -----
echo "== [1/7] SIM CONFORMS regression (check-sim.sh) =="
rm -f "$CAL/build/sim_host" "$CAL/build/simdata.txt" \
      "$TABLES/ml_tables.c" "$TABLES/ml_stages.c"
for id in "${BRIDGE_CPU_IDS[@]}"; do
  rm -f "$CAL/build/$id.trace.txt" "$CAL/build/$id.ai-bridge.txt"
done
bash port/sim/check-sim.sh
made "$CAL/build/sim_host" "$CAL/build/simdata.txt" \
     "$TABLES/ml_tables.c" "$TABLES/ml_stages.c"
for id in "${BRIDGE_CPU_IDS[@]}"; do
  made "$CAL/build/$id.trace.txt" "$CAL/build/$id.ai-bridge.txt"
done

# --- [2] AUDIO (fresh stage, MUSIC pins, pack x2, pinned identity) -----------
echo "== [2/7] audio stage (fresh) + MUSIC track pins + SNDPACK1 =="
rm -rf "$AUDIO_OUT"
node pipeline/run.js --only audio --dist "$DIST" --out "$AUDIO_OUT"
made "$AUDIO_OUT/sounds.json"
# MUSIC pcm sha table (8 rows, both-directions: every pinned track's
# blob exists and hashes to its pin; the sounds.json music map must
# carry EXACTLY the pinned track set)
n_tracks=0
while IFS= read -r mline; do
  [ -n "$mline" ] || continue
  if ! [[ "$mline" =~ ^[0-9a-f]{64}\ [A-Za-z]+$ ]]; then
    fail "music pin table — line fails the anchored grammar: '$mline'"
  fi
  msha="${mline%% *}"
  mtrack="${mline#* }"
  read_music "$mtrack" # also validates blob path + windows
  made "$AUDIO_OUT/$M_BLOB"
  have="$(host_sha256 "$AUDIO_OUT/$M_BLOB")"
  if [ "$have" != "$msha" ]; then
    fail "music pin — $mtrack pcm sha256 $have != pinned $msha (pipeline/ffmpeg drift — reviewed re-freeze required)"
  fi
  n_tracks=$((n_tracks + 1))
done <<< "$MUSIC_PINS"
[ "$n_tracks" = "$N_TRACKS_WANT" ] || fail "music pin inventory — $n_tracks pins verified, want $N_TRACKS_WANT"
mus_inv="$(node -e '
  const s = JSON.parse(require("fs").readFileSync(process.argv[1], "utf8"));
  process.stdout.write(Object.keys(s.music).sort().join(" "));
' "$AUDIO_OUT/sounds.json")" || fail "cannot read the sounds.json music inventory"
[ "$mus_inv" = "battlefield dreamland finald fod menu pStadium targettest yStory" ] || \
  fail "music inventory — sounds.json tracks {$mus_inv} != the pinned 8-track set"
echo "   music pins OK (8 tracks, shas frozen iter 87)"
pack_re='^pack-snd OK count=180 dataBytes=[0-9]{1,12} fileBytes=[0-9]{1,12}$'
pack_resem='pack-snd '
rm -f "$MFI/sndpack.bin" "$MFI/sndpack-a.bin" "$MFI/sndpack-b.bin"
for side in a b; do
  rm -f "$MFI/pack-out-$side.txt"
  node "$GFX/pack-snd.js" "$AUDIO_OUT" "$MFI/sndpack-$side.bin" \
    > "$MFI/pack-out-$side.txt" || fail "pack-snd.js failed (side $side)"
  made "$MFI/sndpack-$side.bin" "$MFI/pack-out-$side.txt"
  judge_verdict_file "$MFI/pack-out-$side.txt" "$pack_re" "$pack_resem"
done
cmp "$MFI/sndpack-a.bin" "$MFI/sndpack-b.bin" || fail "pack not byte-stable x2"
mv "$MFI/sndpack-a.bin" "$MFI/sndpack.bin"; rm -f "$MFI/sndpack-b.bin"
psum="$(host_sha256 "$MFI/sndpack.bin")"
[ "$psum" = "$SNDPACK_SHA256" ] || \
  fail "sndpack sha256 $psum != pinned $SNDPACK_SHA256 (pipeline/ffmpeg drift — reviewed re-freeze required)"
# one pack identity across the audio surface: the mixer check's pin must
# be the same constant (anchored full-line extraction; exactly one)
mixpins="$(grep -E '^SNDPACK_SHA256=[0-9a-f]{64}$' "$GFX/check-mixer-fidelity.sh")" || \
  fail "cannot extract the check-mixer-fidelity.sh pack pin"
[ "$(printf '%s\n' "$mixpins" | wc -l | tr -d ' ')" = 1 ] || \
  fail "check-mixer-fidelity.sh pack pin not unique"
[ "$mixpins" = "SNDPACK_SHA256=$SNDPACK_SHA256" ] || \
  fail "pack pin disagrees with check-mixer-fidelity.sh ($mixpins)"
echo "   pack x2 byte-identical, sha pinned + mixer-check pin cross-checked"

# --- [3] builds --------------------------------------------------------------
echo "== [3/7] builds (sim_host_snd + snd_render) =="
CFLAGS=(-ffp-contract=off -Wall -Wextra -Werror
  -I"$TABLES" -Iport/ryu -Iport/sim -Ioracle/qjs)
SIM_CORE=("$SIM/sim_boot.c" "$SIM/sim_tick.c" "$SIM/sim_ser.c"
  "$SIM/sim_data.c" "$SIM/sim_ai_live.c" port/sim/ai.c
  "$CAL/canon.c" "$CAL/player_canon.c"
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
  oracle/qjs/sha256.c port/fdlibm/fdlibm.c)
MOVES=(port/sim/characters/shared/moves/*.c
  port/sim/characters/fox/moves/*.c
  port/sim/characters/falco/moves/*.c
  port/sim/characters/falcon/moves/*.c
  port/sim/characters/marth/moves/*.c
  port/sim/characters/puff/moves/*.c)
rm -f "$MFI/sim_host_snd" "$MFI/snd_render"
cc -O2 "${CFLAGS[@]}" -o "$MFI/sim_host_snd" \
  "$SIM/sim_main.c" "$GFX/snd_events_tap.c" \
  "${SIM_CORE[@]}" "${MOVES[@]}" -lm
cc -O2 -ffp-contract=off -Wall -Wextra -Werror \
  -o "$MFI/snd_render" "$GFX/snd_render.c"
made "$MFI/sim_host_snd" "$MFI/snd_render"
echo "   builds OK (every TU -ffp-contract=off -Wall -Wextra -Werror)"

# --- differential helpers ----------------------------------------------------
# WITH-music verdict grammars (snd_render.c / snd_reference.js comments:
# the music fields appear ONLY with --music/--music-track)
rend_mus_re='^snd-render OK frames=([0-9]{1,7}) plays=([0-9]{1,9}) stops=([0-9]{1,9}) stopsm=([0-9]{1,9}) stopsu=([0-9]{1,9}) steals=([0-9]{1,9}) maxvoices=([0-9]{1,3}) bytes=([0-9]{1,12}) musout=([0-9]{1,12}) musstarves=([0-9]{1,12}) musrefills=([0-9]{1,12})$'
ref_mus_re='^snd-ref OK frames=([0-9]{1,7}) plays=([0-9]{1,9}) stops=([0-9]{1,9}) stopsm=([0-9]{1,9}) stopsu=([0-9]{1,9}) maxvoices=([0-9]{1,3}) steals=([0-9]{1,9}) bytes=([0-9]{1,12}) musout=([0-9]{1,12})$'
rend_resem='snd-render '
ref_resem='snd-ref '
term_re='^SNDEV OK plays=([0-9]{1,9}) stops=([0-9]{1,9}) lastFrame=([0-9]{1,7})$'

read_terminator() {
  local sd="$1" tline c
  test -s "$sd" || grammar_die "schedule $sd missing or empty"
  if [ -n "$(tail -c 1 "$sd")" ]; then
    grammar_die "schedule $sd lacks the trailing newline (torn write)"
  fi
  c="$(count_e "$sd" "$term_re")"
  [ "$c" = 1 ] || grammar_die "schedule $sd carries $c terminator-grammar lines, want exactly 1"
  c="$(count_e "$sd" '^SNDEV ')"
  [ "$c" = 1 ] || grammar_die "schedule $sd carries $c SNDEV-resembling lines, want exactly 1"
  tline="$(tail -1 "$sd")"
  [[ "$tline" =~ $term_re ]] || grammar_die "schedule $sd: final line is not the terminator: '$tline'"
  sched_plays="${BASH_REMATCH[1]}"; sched_stops="${BASH_REMATCH[2]}"
  sched_last="${BASH_REMATCH[3]}"
}

DIFF_COUNT=0
DIFF_COUNT_PIN=12
STAGE_COVER="" # accumulates covered stage ids (coverage bind below)

# render_pair <tag> <schedule> <frames> <track> — C x2 byte-stable WITH
# music vs the capped-8 reference with --music-track; cmp BIT-IDENTICAL;
# music-plane asserts (musout == frames*735 both sides, musstarves == 0,
# musrefills > 0) + steals/maxvoices C-vs-ref equality. Leaves
# $MFI/<tag>.c.pcm + $MFI/<tag>.ref.pcm for callers that need teeth
# references (callers rm them — Nintendo-derived, keep the dir lean).
render_pair() {
  local tag="$1" sched="$2" fr="$3" track="$4"
  local rline cline want_musout
  read_music "$track"
  rm -f "$MFI/$tag.c.pcm" "$MFI/$tag.c2.pcm" "$MFI/$tag.rend.txt"
  "$MFI/snd_render" --pack "$MFI/sndpack.bin" --events "$sched" \
    --frames "$fr" --out "$MFI/$tag.c.pcm" \
    --music "$AUDIO_OUT/$M_BLOB" --music-volbits "$M_VOLBITS" \
    --music-start "$M_SO,$M_SD" --music-loop "$M_LO,$M_LD" \
    > "$MFI/$tag.rend.txt" || fail "$tag: snd_render (music) failed"
  "$MFI/snd_render" --pack "$MFI/sndpack.bin" --events "$sched" \
    --frames "$fr" --out "$MFI/$tag.c2.pcm" \
    --music "$AUDIO_OUT/$M_BLOB" --music-volbits "$M_VOLBITS" \
    --music-start "$M_SO,$M_SD" --music-loop "$M_LO,$M_LD" \
    > /dev/null || fail "$tag: snd_render (2nd) failed"
  made "$MFI/$tag.c.pcm" "$MFI/$tag.c2.pcm" "$MFI/$tag.rend.txt"
  cmp "$MFI/$tag.c.pcm" "$MFI/$tag.c2.pcm" || fail "$tag: C render not byte-stable x2"
  rm -f "$MFI/$tag.c2.pcm"
  rm -f "$MFI/$tag.ref.pcm" "$MFI/$tag.ref.txt"
  node "$GFX/snd_reference.js" --audio "$AUDIO_OUT" --events "$sched" \
    --frames "$fr" --voices 8 --music-track "$track" \
    --out "$MFI/$tag.ref.pcm" > "$MFI/$tag.ref.txt" \
    || fail "$tag: snd_reference (music) failed"
  made "$MFI/$tag.ref.pcm" "$MFI/$tag.ref.txt"
  judge_verdict_file "$MFI/$tag.rend.txt" "$rend_mus_re" "$rend_resem"
  judge_verdict_file "$MFI/$tag.ref.txt" "$ref_mus_re" "$ref_resem"
  rline="$(cat "$MFI/$tag.rend.txt")"
  [[ "$rline" =~ $rend_mus_re ]] || grammar_die "$tag: snd-render verdict re-read failed: '$rline'"
  c_plays="${BASH_REMATCH[2]}"; c_stops="${BASH_REMATCH[3]}"
  c_steals="${BASH_REMATCH[6]}"; c_maxv="${BASH_REMATCH[7]}"
  c_musout="${BASH_REMATCH[9]}"; c_musstarves="${BASH_REMATCH[10]}"
  c_musrefills="${BASH_REMATCH[11]}"
  cline="$(cat "$MFI/$tag.ref.txt")"
  [[ "$cline" =~ $ref_mus_re ]] || grammar_die "$tag: snd-ref verdict re-read failed: '$cline'"
  r_plays="${BASH_REMATCH[2]}"; r_stops="${BASH_REMATCH[3]}"
  r_maxv="${BASH_REMATCH[6]}"; r_steals="${BASH_REMATCH[7]}"
  r_musout="${BASH_REMATCH[9]}"
  # THE BINDING DIFFERENTIAL: C == capped reference WITH music
  cmp "$MFI/$tag.c.pcm" "$MFI/$tag.ref.pcm" \
    || fail "$tag: MUSIC DIFFERENTIAL DIVERGED (C mixer+music vs independent reference; localize by first differing byte: out-frame = offset/4)"
  want_musout=$((fr * 735))
  [ "$c_musout" = "$want_musout" ] && [ "$r_musout" = "$want_musout" ] \
    || fail "$tag: musout disagrees (C=$c_musout ref=$r_musout want=$want_musout)"
  [ "$c_musstarves" = 0 ] || fail "$tag: offline render reported $c_musstarves music starves (want 0)"
  [ "$c_musrefills" -gt 0 ] || fail "$tag: musrefills=0 — the ring/refill path did not stream"
  [ "$c_plays" = "$r_plays" ] && [ "$c_stops" = "$r_stops" ] \
    || fail "$tag: play/stop counts disagree (C=$c_plays/$c_stops ref=$r_plays/$r_stops)"
  [ "$c_steals" = "$r_steals" ] || fail "$tag: steal counts disagree (C=$c_steals ref=$r_steals)"
  [ "$c_maxv" = "$r_maxv" ] || fail "$tag: maxvoices disagrees (C=$c_maxv ref=$r_maxv)"
}

# run_one <id> <manifest-dir(oracle|m4)> — tapped replay (STREAM MATCH)
# + the music differential on the golden's stage track.
run_one() {
  local id=$1 src=$2 manifest tdir frozen track
  if [ "$src" = "oracle" ]; then
    manifest=oracle/goldens/manifest.json; tdir=oracle/goldens
  else
    manifest=$M4G/manifest.json; tdir=$M4G
  fi
  read_golden "$manifest" "$id"
  frozen="$tdir/$G_NAME.sha256.json"
  test -f "$frozen" || fail "$id: frozen stream missing: $frozen"
  track="${STAGE_TRACKS[$G_STAGE]}"
  echo "== $id ($G_NAME; stage $G_STAGE -> track $track)"
  rm -f "$MFI/$id.trace.txt"
  node "$SIM/trace-to-txt.js" "$tdir/$G_TRACE" "$MFI/$id.trace.txt" >/dev/null
  made "$MFI/$id.trace.txt"
  cpu_args=()
  if [ "$G_CPU" = "true" ]; then
    case " ${BRIDGE_CPU_IDS[*]} " in
      (*" $id "*) cpu_args=(--cpu --difficulty "$G_DIFF" --ai-bridge "$CAL/build/$id.ai-bridge.txt");;
      (*)         cpu_args=(--cpu --difficulty "$G_DIFF");;  # live C AI
    esac
  fi
  rm -f "$MFI/$id.sndev.txt" "$MFI/$id.sim-out.txt" "$MFI/$id.sim-run.json"
  ML_SND_EVENTS_OUT="$MFI/$id.sndev.txt" "$MFI/sim_host_snd" \
    --trace "$MFI/$id.trace.txt" --simdata "$CAL/build/simdata.txt" \
    --seed "$G_SEED" --p1 "$G_P1" --p2 "$G_P2" --stage "$G_STAGE" \
    --frames "$G_FRAMES" ${cpu_args[@]+"${cpu_args[@]}"} \
    > "$MFI/$id.sim-out.txt" || fail "$id: sim_host_snd replay failed"
  made "$MFI/$id.sndev.txt" "$MFI/$id.sim-out.txt"
  node "$SIM/wrap-run.js" "$id" "$MFI/$id.sim-out.txt" \
    "$MFI/$id.sim-run.json" "$manifest" || fail "$id: wrap-run failed"
  made "$MFI/$id.sim-run.json"
  node oracle/harness/verify-stream.js "$MFI/$id.sim-run.json" "$frozen" \
    || fail "$id: tapped replay does NOT stream-match the frozen golden"
  read_terminator "$MFI/$id.sndev.txt"
  [ "$sched_last" = "$G_FRAMES" ] || fail "$id: schedule lastFrame != frames"
  render_pair "$id" "$MFI/$id.sndev.txt" "$G_FRAMES" "$track"
  echo "   $id: STREAM MATCH + C x2 stable + C==ref (capped-8 + $track) BIT-IDENTICAL (musout=$c_musout refills=$c_musrefills plays=$sched_plays stops=$sched_stops)"
  case " $STAGE_COVER " in
    (*" $G_STAGE "*) : ;;
    (*) STAGE_COVER="$STAGE_COVER $G_STAGE" ;;
  esac
  DIFF_COUNT=$((DIFF_COUNT + 1))
  rm -f "$MFI/$id.c.pcm" "$MFI/$id.ref.pcm" # Nintendo-derived; keep lean
}

echo "== [4/7] the music render differential ($DIFF_COUNT_PIN goldens) =="
for id in "${ORACLE_IDS[@]}"; do
  run_one "$id" oracle
done
for id in "${M4_IDS[@]}"; do
  run_one "$id" m4
done
[ "$DIFF_COUNT" = "$DIFF_COUNT_PIN" ] || fail "differential covered $DIFF_COUNT goldens, want $DIFF_COUNT_PIN"
# COVERAGE BIND: the 12 goldens must have exercised ALL SIX stage tracks
# (a golden-set change that drops a stage is a loud coverage failure)
for st in 0 1 2 3 4 5; do
  case " $STAGE_COVER " in
    (*" $st "*) : ;;
    (*) fail "stage-track coverage — no golden fields stage $st (track ${STAGE_TRACKS[$st]})" ;;
  esac
done
echo "   all 6 stage tracks covered by the golden set"

# --- [5] synthetic legs (pure music: wrap / EOF-silence / short intro) -------
echo "== [5/7] synthetic legs (menu wrap, fod EOF+wrap, targettest intro) =="
# empty schedule generator: `SNDEV OK plays=0 stops=0 lastFrame=<N>`
mk_empty_sched() { # <file> <frames>
  rm -f "$1"
  printf 'SNDEV OK plays=0 stops=0 lastFrame=%d\n' "$2" > "$1"
  made "$1"
}
# menu@10900: earliest loop WRAP of any track — startDur+loopDur =
# floor(7425*441/20) + (floor(180925*441/20)-floor(7425*441/20)) =
# 3,989,396 source frames = output frame 7,978,792 = sim frame ~10856.9.
mk_empty_sched "$MFI/syn-menu.sndev.txt" 10900
render_pair syn-menu "$MFI/syn-menu.sndev.txt" 10900 menu
echo "   menu@10900 (loop WRAP) BIT-IDENTICAL (musout=$c_musout refills=$c_musrefills)"
# fod@20000: EOF-silence (fileFrames 6,703,200 = 304.0 s < loop-window
# end 7,322,849) AND the fod wrap at startDur+loopDur = 7,322,871 src
# frames = sim frame ~19926.9 — the quirk carried verbatim.
mk_empty_sched "$MFI/syn-fod.sndev.txt" 20000
render_pair syn-fod "$MFI/syn-fod.sndev.txt" 20000 fod
echo "   fod@20000 (EOF-silence + wrap) BIT-IDENTICAL (musout=$c_musout refills=$c_musrefills)"
# targettest@600: short-intro chaining (startDur = floor(1*441/20) = 22
# source frames — the intro->loop boundary lands in output frame 44).
mk_empty_sched "$MFI/syn-tt.sndev.txt" 600
render_pair syn-tt "$MFI/syn-tt.sndev.txt" 600 targettest
echo "   targettest@600 (short-intro chain) BIT-IDENTICAL (musout=$c_musout refills=$c_musrefills)"
# keep syn-menu C/ref pcm? render_pair left them; teeth need the menu
# ref + g01 ref — regenerate below instead; drop these now.
rm -f "$MFI/syn-menu.c.pcm" "$MFI/syn-menu.ref.pcm" \
      "$MFI/syn-fod.c.pcm" "$MFI/syn-fod.ref.pcm" \
      "$MFI/syn-tt.c.pcm" "$MFI/syn-tt.ref.pcm"

# --- [6] teeth (pre-registered T1-T4 + grammar; positive controls) -----------
echo "== [6/7] teeth =="
SD=$MFI/g01.sndev.txt
read_music battlefield
BF_BLOB="$AUDIO_OUT/$M_BLOB"; BF_VB="$M_VOLBITS"
BF_START="$M_SO,$M_SD"; BF_LOOP="$M_LO,$M_LD"
# reference copies for tooth comparisons (regenerate once)
rm -f "$MFI/g01.ref.keep.pcm" "$MFI/menu.ref.keep.pcm"
node "$GFX/snd_reference.js" --audio "$AUDIO_OUT" --events "$SD" \
  --frames 3600 --voices 8 --music-track battlefield \
  --out "$MFI/g01.ref.keep.pcm" > /dev/null
node "$GFX/snd_reference.js" --audio "$AUDIO_OUT" \
  --events "$MFI/syn-menu.sndev.txt" --frames 10900 --voices 8 \
  --music-track menu --out "$MFI/menu.ref.keep.pcm" > /dev/null
made "$MFI/g01.ref.keep.pcm" "$MFI/menu.ref.keep.pcm"
# positive control: the clean C render still matches
rm -f "$MFI/clean.pcm"
"$MFI/snd_render" --pack "$MFI/sndpack.bin" --events "$SD" --frames 3600 \
  --out "$MFI/clean.pcm" --music "$BF_BLOB" --music-volbits "$BF_VB" \
  --music-start "$BF_START" --music-loop "$BF_LOOP" > /dev/null
made "$MFI/clean.pcm"
cmp "$MFI/clean.pcm" "$MFI/g01.ref.keep.pcm" || fail "positive control broke (clean g01+battlefield render != reference)"
rm -f "$MFI/clean.pcm"
tooth_mus() { # <label> <ref.pcm> <events> <frames> <music args...> — MUST diverge
  local lbl="$1" ref="$2" ev="$3" fr="$4"; shift 4
  rm -f "$MFI/tooth.pcm"
  "$MFI/snd_render" --pack "$MFI/sndpack.bin" --events "$ev" --frames "$fr" \
    --out "$MFI/tooth.pcm" "$@" > /dev/null || fail "tooth $lbl: renderer died"
  made "$MFI/tooth.pcm"
  if cmp -s "$MFI/tooth.pcm" "$ref"; then
    fail "TOOTH $lbl DID NOT FIRE (output identical to the reference)"
  fi
  echo "   tooth $lbl fired (divergence detected)"
  rm -f "$MFI/tooth.pcm"
}
tooth_mus "T1-music-gain" "$MFI/g01.ref.keep.pcm" "$SD" 3600 \
  --music "$BF_BLOB" --music-volbits "$BF_VB" --music-start "$BF_START" \
  --music-loop "$BF_LOOP" --tooth-music-gain
tooth_mus "T2-loop-beg" "$MFI/g01.ref.keep.pcm" "$SD" 3600 \
  --music "$BF_BLOB" --music-volbits "$BF_VB" --music-start "$BF_START" \
  --music-loop "$BF_LOOP" --tooth-music-loop-beg
read_music menu
tooth_mus "T3-loop-dur-wrap" "$MFI/menu.ref.keep.pcm" \
  "$MFI/syn-menu.sndev.txt" 10900 \
  --music "$AUDIO_OUT/$M_BLOB" --music-volbits "$M_VOLBITS" \
  --music-start "$M_SO,$M_SD" --music-loop "$M_LO,$M_LD" \
  --tooth-music-loop-dur
# T4 underfill: starves REPORTED (>0) AND output diverges — the starve
# accounting is load-bearing, not decorative.
rm -f "$MFI/tooth4.pcm" "$MFI/tooth4.txt"
"$MFI/snd_render" --pack "$MFI/sndpack.bin" --events "$SD" --frames 3600 \
  --out "$MFI/tooth4.pcm" --music "$BF_BLOB" --music-volbits "$BF_VB" \
  --music-start "$BF_START" --music-loop "$BF_LOOP" \
  --tooth-music-underfill > "$MFI/tooth4.txt" || fail "T4 renderer died"
made "$MFI/tooth4.pcm" "$MFI/tooth4.txt"
t4_c="$(count_e "$MFI/tooth4.txt" '^snd-render OK frames=3600 .* musstarves=[1-9][0-9]* musrefills=0$')"
[ "$t4_c" = 1 ] || fail "T4: underfill render did not report musstarves>0 musrefills=0 (starve accounting must SEE the withheld refills)"
if cmp -s "$MFI/tooth4.pcm" "$MFI/g01.ref.keep.pcm"; then
  fail "T4: underfill output identical to the reference (a starved ring must be audible)"
fi
echo "   tooth T4-underfill fired (musstarves>0 reported AND output diverges)"
rm -f "$MFI/tooth4.pcm" "$MFI/tooth4.txt"
# grammar teeth: malformed music args die NONZERO in both renderers
if "$MFI/snd_render" --pack "$MFI/sndpack.bin" --events "$SD" --frames 3600 \
  --out "$MFI/tg.pcm" --music "$BF_BLOB" --music-volbits "ZZd3333333333333" \
  --music-start "$BF_START" --music-loop "$BF_LOOP" > /dev/null 2>&1; then
  fail "T-grammar: C renderer accepted a non-hex --music-volbits"
fi
if "$MFI/snd_render" --pack "$MFI/sndpack.bin" --events "$SD" --frames 3600 \
  --out "$MFI/tg.pcm" --music "$BF_BLOB" --music-volbits "$BF_VB" \
  --music-start "0, 12366" --music-loop "$BF_LOOP" > /dev/null 2>&1; then
  fail "T-grammar: C renderer accepted an elastic-whitespace ms pair"
fi
if "$MFI/snd_render" --pack "$MFI/sndpack.bin" --events "$SD" --frames 3600 \
  --out "$MFI/tg.pcm" --music "$BF_BLOB" --music-volbits "$BF_VB" \
  --music-start "0,012366" --music-loop "$BF_LOOP" > /dev/null 2>&1; then
  fail "T-grammar: C renderer accepted a leading-zero ms pair"
fi
if node "$GFX/snd_reference.js" --audio "$AUDIO_OUT" --events "$SD" \
  --frames 3600 --voices 8 --music-track nosuchtrack \
  --out "$MFI/tg.pcm" > /dev/null 2>&1; then
  fail "T-grammar: reference accepted an unknown --music-track"
fi
echo "   grammar teeth fired (bad volbits / elastic-space pair / leading-zero pair / unknown track all die)"
rm -f "$MFI/tg.pcm" "$MFI/g01.ref.keep.pcm" "$MFI/menu.ref.keep.pcm"

# --- [7] no-commit guard (rc case-split) --------------------------------------
rc=0
dirty="$(git status --porcelain -- "$B" "$AUDIO_OUT" \
  "$M4G/manifest.json" "$M4G/*.trace.json" "$M4G/*.sha256.json")" || rc=$?
if [ "$rc" -ne 0 ]; then
  fail "no-commit guard — git status rc $rc (a status read error is CORRUPT evidence, never a clean pass)"
fi
if [ -n "$dirty" ]; then
  echo "MUSIC FIDELITY FAIL: build output not gitignored (or the golden contract dirtied):" >&2
  printf '%s\n' "$dirty" >&2
  exit 1
fi

echo "MUSIC FIDELITY OK (goldens=$DIFF_COUNT tracks=8 diff=bit-identical wraps=2 eofsilence=1)"
