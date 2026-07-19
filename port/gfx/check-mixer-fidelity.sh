#!/usr/bin/env bash
# check-mixer-fidelity.sh — M4 task 6 done-check: mixer fidelity by
# OFFLINE DETERMINISTIC RENDER DIFFERENTIAL + real play-ids + stop-path
# live coverage (fix_plan §M4 task 6; pre-registration AGENT-LOG iter 82).
#
# Composes, in order:
#   [1] SIM CONFORMS — bash port/sim/check-sim.sh (the frozen M2 gate;
#       the id-plumbing edits touch sim TUs and the checksum surface
#       must not move). Also produces the sim tables, SIMDATA and the
#       g07/g08 AIBRIDGE1 artifacts this check reuses.
#   [2] SNDPACK — fresh pipeline audio stage, pack x2 byte-identical,
#       sha256 pinned AND cross-checked against check-device-audio.sh's
#       frozen pin (one pack identity across the audio surface).
#   [3] THE DIFFERENTIAL — for EVERY golden (8 oracle + 2 m4 + s01):
#       replay on sim_host_snd with the event tap (each run judged
#       STREAM MATCH by the UNCHANGED verify-stream.js against its
#       frozen stream — instrumentation cannot perturb the sim), then
#       render the schedule OFFLINE twice through the C mixer math
#       (snd_render.c, byte-stable x2) and once through the INDEPENDENT
#       reference (snd_reference.js), and cmp the PCM byte-for-byte.
#       Comparison basis = BIT-DIFF, decided by the pre-registered
#       measurement (AGENT-LOG iter 82): the reference renders TWICE —
#       UNLIMITED voices (browser howler truth; measures true
#       concurrency) and CAPPED at 8 with the documented
#       steal-oldest-by-start-sequence policy (independently
#       implemented from the documented contract). The C mixer must be
#       bit-identical to the CAPPED reference on EVERY golden, and to
#       the UNLIMITED reference wherever measured concurrency <= 8
#       (measured: 9/11 goldens; g06 and m02 each peak at 9 voices /
#       1 steal — the measured REGISTERED EXPOSURE: on device,
#       steal-oldest truncates that one voice tail vs the browser's
#       unlimited mixing). Steal counts must agree C vs capped-ref
#       exactly.
#   [4] STOP-PATH LIVENESS — the s01 scenario schedule must carry the
#       measured stop-path coverage: 2 shieldbreakercharge.stop + 2
#       furaloop.stop (all id-routed), 2 furaloop episodes, 2 shield
#       breaks, a KO (the four in-match stop ARMS: NSG release, NSG
#       charge==122 auto, hitDetection FURAFURA, FURAFURA wake —
#       AGENT-LOG iter 82 records the arm attribution).
#   [5] GFX_APP LEG — s01 replayed through the REAL app (headless
#       backend, mixer attached via the ml_snd_sink + ml_snd_stop_id_sink
#       chokepoints): STREAM MATCH + voice starts/stops == the schedule
#       counts (the live path consumes the same events).
#   [6] TEETH — standing negative tests (tooth flags are TEST-ONLY
#       seams): on the s01 render — gain nibble, dropped stop,
#       resample-step skew, stop-id skew (howler stale-id no-op -> the
#       loop rings on), schedule-grammar corruption (both renderers
#       fail closed); on the g06 render — steal-policy flip (g06 is a
#       schedule where a steal REALLY fires, so the flip must diverge).
#       Positive controls throughout.
#
# Prints `MIXER FIDELITY OK (...)`, exit 0; ANY divergence, stream
# mismatch, count disagreement, or missing artifact -> nonzero.
#
# PROVENANCE: the pack + rendered PCM derive from Nintendo-derived
# blobs — gitignored build output only, never committed (guarded below).
set -euo pipefail
cd "$(dirname "$0")/../.."

GFX=port/gfx
B=$GFX/build
MF=$B/mixerfid
SIM=port/sim/sim
CAL=port/sim/calib
TABLES=pipeline/build/sim-tables
AUDIO_OUT=pipeline/build/audio-mixerfid
DIST="${MELEELIGHT_CLONE:-$HOME/.cache/meleelight-funkey-s/upstream}"
mkdir -p "$MF"

# Pack identity (measured-then-frozen, iter 57; must equal the
# check-device-audio.sh pin — cross-checked below).
SNDPACK_SHA256=f69579082fe569249879faa5ceccb7a810d94d8092695ddc8bb543f3bda3ccb4

host_sha256() { shasum -a 256 "$1" | cut -d' ' -f1; }

fail() { echo "MIXER FIDELITY FAIL: $1" >&2; exit 1; }

# --- [1] SIM CONFORMS (the frozen M2 gate; also builds tables/bridges) -----
echo "== [1/6] SIM CONFORMS regression (check-sim.sh) =="
bash port/sim/check-sim.sh
test -f "$CAL/build/g07.ai-bridge.txt" || fail "check-sim.sh left no g07 bridge"
test -f "$CAL/build/g08.ai-bridge.txt" || fail "check-sim.sh left no g08 bridge"

# --- [2] SNDPACK (fresh stage, x2 pack determinism, pinned identity) -------
echo "== [2/6] SNDPACK1 (fresh audio stage, x2, pinned) =="
node pipeline/run.js --only audio --dist "$DIST" --out "$AUDIO_OUT"
pack_re='^pack-snd OK count=180 dataBytes=[0-9]{1,12} fileBytes=[0-9]{1,12}$'
for side in a b; do
  node "$GFX/pack-snd.js" "$AUDIO_OUT" "$MF/sndpack-$side.bin" \
    > "$MF/pack-out-$side.txt" || fail "pack-snd.js failed (side $side)"
  pline="$(cat "$MF/pack-out-$side.txt")"
  pcnt="$(grep -Ec "$pack_re" "$MF/pack-out-$side.txt")" || true
  [ "$pcnt" = 1 ] || fail "pack-snd verdict grammar violation: '$pline'"
done
cmp "$MF/sndpack-a.bin" "$MF/sndpack-b.bin" || fail "pack not byte-stable x2"
mv "$MF/sndpack-a.bin" "$MF/sndpack.bin"; rm -f "$MF/sndpack-b.bin"
psum="$(host_sha256 "$MF/sndpack.bin")"
[ "$psum" = "$SNDPACK_SHA256" ] || \
  fail "sndpack sha256 $psum != pinned $SNDPACK_SHA256 (pipeline/ffmpeg drift — reviewed re-freeze required)"
# one pack identity across the audio surface: the device check's pin
# must be the same constant (anchored full-line extraction; exactly one)
devpins="$(grep -E '^SNDPACK_SHA256=[0-9a-f]{64}$' "$GFX/check-device-audio.sh")" || \
  fail "cannot extract the check-device-audio.sh pack pin"
[ "$(printf '%s\n' "$devpins" | wc -l | tr -d ' ')" = 1 ] || \
  fail "check-device-audio.sh pack pin not unique"
[ "$devpins" = "SNDPACK_SHA256=$SNDPACK_SHA256" ] || \
  fail "pack pin disagrees with check-device-audio.sh ($devpins)"
echo "   pack x2 byte-identical, sha pinned + device-pin cross-checked"

# --- [3] builds ------------------------------------------------------------
echo "== [3/6] builds (sim_host_snd + snd_render + gfx_app headless) =="
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
rm -f "$MF/sim_host_snd" "$MF/snd_render" "$MF/gfx_app_headless" "$MF/raster.o"
cc -O2 "${CFLAGS[@]}" -o "$MF/sim_host_snd" \
  "$SIM/sim_main.c" "$GFX/snd_events_tap.c" \
  "${SIM_CORE[@]}" "${MOVES[@]}" -lm
cc -O2 -ffp-contract=off -Wall -Wextra -Werror \
  -o "$MF/snd_render" "$GFX/snd_render.c"
cc -O3 "${CFLAGS[@]}" -c "$GFX/raster.c" -o "$MF/raster.o"
cc -O2 "${CFLAGS[@]}" -o "$MF/gfx_app_headless" \
  "$MF/raster.o" "$GFX/gfx_app.c" "$GFX/platform_headless.c" \
  "$GFX/anim1.c" "$GFX/gfx_render.c" "$GFX/gfx_vfx.c" \
  "$GFX/gfx_overlay.c" "$GFX/gfx_bg.c" \
  "${SIM_CORE[@]}" "${MOVES[@]}" -lm
echo "   builds OK (every TU -ffp-contract=off -Wall -Wextra -Werror)"

# --- schedule + differential helpers ---------------------------------------
rend_re='^snd-render OK frames=([0-9]{1,7}) plays=([0-9]{1,9}) stops=([0-9]{1,9}) steals=([0-9]{1,9}) maxvoices=([0-9]{1,3}) bytes=([0-9]{1,12})$'
ref_re='^snd-ref OK frames=([0-9]{1,7}) plays=([0-9]{1,9}) stops=([0-9]{1,9}) maxvoices=([0-9]{1,3}) steals=([0-9]{1,9}) bytes=([0-9]{1,12})$'
term_re='^SNDEV OK plays=([0-9]{1,9}) stops=([0-9]{1,9}) lastFrame=([0-9]{1,7})$'

MAXV_ALL=0
DIFF_COUNT=0
STEALS_ALL=0

# run_one <id> <manifest> <frozen> — replay+tap (STREAM MATCH), render
# C x2 + reference, bit-diff. Sets sched_plays/sched_stops.
run_one() {
  id=$1; manifest=$2; frozen=$3
  eval "$(node -e "
    const m=require('./$manifest');
    const g=m.goldens.find(x=>x.id==='$id');
    if(!g){console.error('no golden $id in $manifest');process.exit(1);}
    console.log('name='+g.name);
    console.log('seed='+g.seed);
    console.log('p1='+g.p1); console.log('p2='+g.p2);
    console.log('stage='+g.stage);
    console.log('frames='+g.frames);
    console.log('cpu='+(g.cpu?1:0));
    console.log('difficulty='+(g.difficulty||5));
    console.log('trace='+g.trace);
  ")"
  tdir=$(dirname "$manifest")
  echo "== $id ($name)"
  node "$SIM/trace-to-txt.js" "$tdir/$trace" "$MF/$id.trace.txt" >/dev/null
  cpu_args=()
  if [ "$cpu" = "1" ]; then
    case "$id" in
      g07|g08) cpu_args=(--cpu --difficulty "$difficulty" --ai-bridge "$CAL/build/$id.ai-bridge.txt");;
      *)       cpu_args=(--cpu --difficulty "$difficulty");;  # live C AI
    esac
  fi
  rm -f "$MF/$id.sndev.txt" "$MF/$id.sim-out.txt"
  ML_SND_EVENTS_OUT="$MF/$id.sndev.txt" "$MF/sim_host_snd" \
    --trace "$MF/$id.trace.txt" --simdata "$CAL/build/simdata.txt" \
    --seed "$seed" --p1 "$p1" --p2 "$p2" --stage "$stage" \
    --frames "$frames" ${cpu_args[@]+"${cpu_args[@]}"} \
    > "$MF/$id.sim-out.txt" || fail "$id: sim_host_snd replay failed"
  node "$SIM/wrap-run.js" "$id" "$MF/$id.sim-out.txt" \
    "$MF/$id.sim-run.json" "$manifest" || fail "$id: wrap-run failed"
  node oracle/harness/verify-stream.js "$MF/$id.sim-run.json" "$frozen" \
    || fail "$id: tapped replay does NOT stream-match the frozen golden"
  # terminator (strict): counts + lastFrame == the golden's frames
  tline="$(tail -1 "$MF/$id.sndev.txt")"
  if [[ "$tline" =~ $term_re ]]; then
    sched_plays="${BASH_REMATCH[1]}"; sched_stops="${BASH_REMATCH[2]}"
    [ "${BASH_REMATCH[3]}" = "$frames" ] || fail "$id: schedule lastFrame != frames"
  else
    fail "$id: schedule terminator grammar violation: '$tline'"
  fi
  # C render x2 (byte-stable) + the independent reference + bit-diff
  "$MF/snd_render" --pack "$MF/sndpack.bin" --events "$MF/$id.sndev.txt" \
    --frames "$frames" --out "$MF/$id.c.pcm" > "$MF/$id.rend.txt" \
    || fail "$id: snd_render failed"
  "$MF/snd_render" --pack "$MF/sndpack.bin" --events "$MF/$id.sndev.txt" \
    --frames "$frames" --out "$MF/$id.c2.pcm" > /dev/null \
    || fail "$id: snd_render (2nd) failed"
  cmp "$MF/$id.c.pcm" "$MF/$id.c2.pcm" || fail "$id: C render not byte-stable x2"
  rm -f "$MF/$id.c2.pcm"
  # the reference, BOTH ways: unlimited (browser truth / concurrency
  # measurement) and capped-8 (the device contract; the binding diff)
  node "$GFX/snd_reference.js" --audio "$AUDIO_OUT" \
    --events "$MF/$id.sndev.txt" --frames "$frames" \
    --out "$MF/$id.refu.pcm" > "$MF/$id.refu.txt" \
    || fail "$id: snd_reference (unlimited) failed"
  node "$GFX/snd_reference.js" --audio "$AUDIO_OUT" \
    --events "$MF/$id.sndev.txt" --frames "$frames" --voices 8 \
    --out "$MF/$id.ref8.pcm" > "$MF/$id.ref8.txt" \
    || fail "$id: snd_reference (capped) failed"
  rline="$(cat "$MF/$id.rend.txt")"
  [[ "$rline" =~ $rend_re ]] || fail "$id: snd-render verdict grammar: '$rline'"
  c_plays="${BASH_REMATCH[2]}"; c_stops="${BASH_REMATCH[3]}"
  c_steals="${BASH_REMATCH[4]}"
  uline="$(cat "$MF/$id.refu.txt")"
  [[ "$uline" =~ $ref_re ]] || fail "$id: snd-ref (unlimited) verdict grammar: '$uline'"
  u_plays="${BASH_REMATCH[2]}"; u_stops="${BASH_REMATCH[3]}"
  u_maxv="${BASH_REMATCH[4]}"; u_steals="${BASH_REMATCH[5]}"
  cline="$(cat "$MF/$id.ref8.txt")"
  [[ "$cline" =~ $ref_re ]] || fail "$id: snd-ref (capped) verdict grammar: '$cline'"
  r8_steals="${BASH_REMATCH[5]}"
  [ "$u_steals" = 0 ] || fail "$id: unlimited reference reported steals"
  [ "$c_plays" = "$sched_plays" ] && [ "$u_plays" = "$sched_plays" ] \
    || fail "$id: play counts disagree (sched=$sched_plays C=$c_plays ref=$u_plays)"
  [ "$c_stops" = "$sched_stops" ] && [ "$u_stops" = "$sched_stops" ] \
    || fail "$id: stop counts disagree (sched=$sched_stops C=$c_stops ref=$u_stops)"
  # THE BINDING DIFFERENTIAL: C == capped reference, every golden
  cmp "$MF/$id.c.pcm" "$MF/$id.ref8.pcm" \
    || fail "$id: RENDER DIFFERENTIAL DIVERGED (C mixer vs capped independent reference)"
  [ "$c_steals" = "$r8_steals" ] \
    || fail "$id: steal counts disagree (C=$c_steals capped-ref=$r8_steals)"
  if [ "$u_maxv" -le 8 ]; then
    # <=8 domain: the unlimited (cap-free) reference must ALSO match —
    # the stronger independence check — and nothing may have stolen
    [ "$c_steals" = 0 ] || fail "$id: steals in a <=8-voice schedule"
    cmp "$MF/$id.c.pcm" "$MF/$id.refu.pcm" \
      || fail "$id: C != unlimited reference despite <=8 concurrency"
    echo "   $id: STREAM MATCH + C x2 stable + C==ref (capped AND unlimited) BIT-IDENTICAL (plays=$sched_plays stops=$sched_stops maxvoices=$u_maxv)"
  else
    # >8 domain (the measured g06 class): the capped diff is binding;
    # the unlimited delta is the REGISTERED device-vs-browser exposure
    [ "$c_steals" -ge 1 ] || fail "$id: concurrency $u_maxv > 8 but no steal fired"
    echo "   $id: STREAM MATCH + C x2 stable + C==capped-ref BIT-IDENTICAL; MEASURED EXPOSURE: peak $u_maxv voices, $c_steals steal(s) vs browser-unlimited (registered, AGENT-LOG iter 82)"
  fi
  [ "$u_maxv" -gt "$MAXV_ALL" ] && MAXV_ALL=$u_maxv
  STEALS_ALL=$((STEALS_ALL + c_steals))
  DIFF_COUNT=$((DIFF_COUNT + 1))
  rm -f "$MF/$id.c.pcm" "$MF/$id.refu.pcm" "$MF/$id.ref8.pcm" # Nintendo-derived; keep the dir lean
}

echo "== [4/6] the offline render differential (11 goldens) =="
for id in g01 g02 g03 g04 g05 g06 g07 g08; do
  eval "$(node -e "
    const m=require('./oracle/goldens/manifest.json');
    const g=m.goldens.find(x=>x.id==='$id');
    console.log('gname='+g.name);
  ")"
  run_one "$id" oracle/goldens/manifest.json "oracle/goldens/$gname.sha256.json"
done
for id in m01 m02; do
  eval "$(node -e "
    const m=require('./port/goldens-m4/manifest.json');
    const g=m.goldens.find(x=>x.id==='$id');
    console.log('gname='+g.name);
  ")"
  run_one "$id" port/goldens-m4/manifest.json "port/goldens-m4/$gname.sha256.json"
done
run_one s01 port/goldens-snd/manifest.json \
  port/goldens-snd/s01-marth-fox-stops-battlefield.sha256.json
[ "$DIFF_COUNT" = 11 ] || fail "differential covered $DIFF_COUNT goldens, want 11"

# --- [4b] stop-path liveness (the s01 coverage contract) -------------------
echo "== [4b] s01 stop-path liveness =="
SD=$MF/s01.sndev.txt
sbc_stops=$(grep -Ec '^S [0-9]+ shieldbreakercharge\.stop 1 [0-9a-f]{16}$' "$SD") || true
fl_stops=$(grep -Ec '^S [0-9]+ furaloop\.stop 1 [0-9a-f]{16}$' "$SD") || true
fl_plays=$(grep -Ec '^P [0-9]+ furaloop$' "$SD") || true
breaks=$(grep -Ec '^P [0-9]+ shieldbreak$' "$SD") || true
kills=$(grep -Ec '^P [0-9]+ kill$' "$SD") || true
noid_stops=$(grep -Ec '^S [0-9]+ [0-9A-Za-z]+\.stop 0 ' "$SD") || true
[ "$sbc_stops" = 2 ] || fail "s01 shieldbreakercharge.stop count $sbc_stops != 2"
[ "$fl_stops" = 2 ] || fail "s01 furaloop.stop count $fl_stops != 2"
[ "$fl_plays" = 2 ] || fail "s01 furaloop play count $fl_plays != 2"
[ "$breaks" = 2 ] || fail "s01 shieldbreak count $breaks != 2"
[ "$kills" = 1 ] || fail "s01 kill count $kills != 1"
[ "$noid_stops" = 0 ] || fail "s01 carries $noid_stops id-less stops (all four arms pass ids)"
echo "   s01: 2x shieldbreakercharge.stop + 2x furaloop.stop (all id-routed), 2 breaks, 2 furaloop episodes, 1 KO"

# --- [5] the real-app leg (gfx_app headless, mixer attached) --------------
echo "== [5/6] gfx_app leg (s01 through the live sink path) =="
rm -f "$MF/s01.app-out.txt" "$MF/s01.app-tim.txt" "$MF/s01.app-log.txt"
"$MF/gfx_app_headless" \
  --trace "$MF/s01.trace.txt" --simdata "$CAL/build/simdata.txt" \
  --gfxdata "$GFX/gfxdata-frozen.txt" --vfxdata "$GFX/vfxdata-frozen.txt" \
  --glyphs "$GFX/vfxglyphs-frozen.txt" --anim-dir "$TABLES" \
  --seed 9001 --p1 0 --p2 2 --stage 0 --frames 3600 --pace 0 \
  --out "$MF/s01.app-out.txt" --timing "$MF/s01.app-tim.txt" \
  --sndpack "$MF/sndpack.bin" 2> "$MF/s01.app-log.txt" \
  || fail "gfx_app s01 replay failed"
node "$SIM/wrap-run.js" s01 "$MF/s01.app-out.txt" "$MF/s01.app-run.json" \
  port/goldens-snd/manifest.json || fail "gfx_app wrap failed"
node oracle/harness/verify-stream.js "$MF/s01.app-run.json" \
  port/goldens-snd/s01-marth-fox-stops-battlefield.sha256.json \
  || fail "gfx_app s01 stream mismatch (mixer attach perturbed the sim)"
au_re='^gfx_app audio: ([0-9]{1,12}) callbacks, ([0-9]{1,12}) underruns, ([0-9]{1,12}) badlen, ([0-9]{1,12}) voice starts, ([0-9]{1,12}) voice stops, ([0-9]{1,12}) steals, rate=0 samples=0 channels=0$'
aline="$(grep -E '^gfx_app audio: ' "$MF/s01.app-log.txt")" || fail "no gfx_app audio summary"
[ "$(grep -Ec '^gfx_app audio: ' "$MF/s01.app-log.txt")" = 1 ] || fail "duplicate audio summaries"
[[ "$aline" =~ $au_re ]] || fail "gfx_app audio summary grammar: '$aline'"
app_starts="${BASH_REMATCH[4]}"; app_stops="${BASH_REMATCH[5]}"
# s01 schedule truth (from [4]): 60 plays, 4 stops
s01_term="$(tail -1 "$SD")"
[[ "$s01_term" =~ $term_re ]] || fail "s01 terminator re-read failed"
[ "$app_starts" = "${BASH_REMATCH[1]}" ] || fail "gfx_app starts $app_starts != schedule ${BASH_REMATCH[1]}"
[ "$app_stops" = "${BASH_REMATCH[2]}" ] || fail "gfx_app stops $app_stops != schedule ${BASH_REMATCH[2]}"
echo "   gfx_app: STREAM MATCH; starts/stops == schedule ($app_starts/$app_stops)"

# --- [6] teeth (s01 render; every tooth must fire) -------------------------
echo "== [6/6] teeth =="
tooth_diff() { # <label> <extra renderer args...> — MUST diverge from ref
  lbl=$1; shift
  "$MF/snd_render" --pack "$MF/sndpack.bin" --events "$SD" --frames 3600 \
    --out "$MF/tooth.pcm" "$@" > /dev/null || fail "tooth $lbl: renderer died"
  if cmp -s "$MF/tooth.pcm" "$MF/s01.ref.keep.pcm"; then
    fail "TOOTH $lbl DID NOT FIRE (output identical to the reference)"
  fi
  echo "   tooth $lbl fired (divergence detected)"
  rm -f "$MF/tooth.pcm"
}
# reference copy for tooth comparisons (regenerate once)
node "$GFX/snd_reference.js" --audio "$AUDIO_OUT" --events "$SD" \
  --frames 3600 --out "$MF/s01.ref.keep.pcm" > /dev/null
# positive control: the clean render still matches
"$MF/snd_render" --pack "$MF/sndpack.bin" --events "$SD" --frames 3600 \
  --out "$MF/clean.pcm" > /dev/null
cmp "$MF/clean.pcm" "$MF/s01.ref.keep.pcm" || fail "positive control broke"
rm -f "$MF/clean.pcm"
tooth_diff "T1-gain-nibble" --tooth-gain furaloop
tooth_diff "T2-dropped-stop" --tooth-drop-first-stop
tooth_diff "T3-step-skew" --tooth-step-skew
tooth_diff "T4-stop-id-skew" --tooth-stop-id-skew
# T5 steal-policy flip, on g06 — the ONE schedule where a steal really
# fires (measured, 9-voice peak): flipping to steal-newest must diverge
# from the capped reference (which implements the documented
# steal-oldest policy).
node "$GFX/snd_reference.js" --audio "$AUDIO_OUT" --events "$MF/g06.sndev.txt" \
  --frames 3600 --voices 8 --out "$MF/g06.ref8.keep.pcm" > /dev/null
"$MF/snd_render" --pack "$MF/sndpack.bin" --events "$MF/g06.sndev.txt" \
  --frames 3600 --out "$MF/tooth5.pcm" --tooth-steal-newest > /dev/null
if cmp -s "$MF/tooth5.pcm" "$MF/g06.ref8.keep.pcm"; then
  fail "T5: steal-policy flip did NOT diverge on g06 (the schedule with a live steal)"
fi
# positive control: the unflipped render still matches
"$MF/snd_render" --pack "$MF/sndpack.bin" --events "$MF/g06.sndev.txt" \
  --frames 3600 --out "$MF/tooth5c.pcm" > /dev/null
cmp "$MF/tooth5c.pcm" "$MF/g06.ref8.keep.pcm" || fail "T5 positive control broke"
echo "   tooth T5-steal-flip fired on g06 (divergence detected; positive control clean)"
rm -f "$MF/tooth5.pcm" "$MF/tooth5c.pcm" "$MF/g06.ref8.keep.pcm"
# T6 grammar corruption: dropped terminator + malformed line — BOTH
# renderers must die nonzero (fail closed)
sed '$d' "$SD" > "$MF/tooth6a.txt"
if "$MF/snd_render" --pack "$MF/sndpack.bin" --events "$MF/tooth6a.txt" \
  --frames 3600 --out "$MF/t6.pcm" > /dev/null 2>&1; then
  fail "T6a: C renderer accepted a terminator-less schedule"
fi
if node "$GFX/snd_reference.js" --audio "$AUDIO_OUT" \
  --events "$MF/tooth6a.txt" --frames 3600 --out "$MF/t6.pcm" \
  > /dev/null 2>&1; then
  fail "T6a: reference accepted a terminator-less schedule"
fi
{ head -1 "$SD"; echo "P xx bogus!"; tail -n +2 "$SD"; } > "$MF/tooth6b.txt"
if "$MF/snd_render" --pack "$MF/sndpack.bin" --events "$MF/tooth6b.txt" \
  --frames 3600 --out "$MF/t6.pcm" > /dev/null 2>&1; then
  fail "T6b: C renderer accepted a malformed line"
fi
if node "$GFX/snd_reference.js" --audio "$AUDIO_OUT" \
  --events "$MF/tooth6b.txt" --frames 3600 --out "$MF/t6.pcm" \
  > /dev/null 2>&1; then
  fail "T6b: reference accepted a malformed line"
fi
echo "   tooth T6 fired (both renderers fail closed on grammar corruption)"
rm -f "$MF/tooth6a.txt" "$MF/tooth6b.txt" "$MF/t6.pcm" "$MF/s01.ref.keep.pcm"

# --- no-commit guard: build output (incl. Nintendo-derived PCM) ------------
if git status --porcelain -- "$B" "$AUDIO_OUT" | grep -q .; then
  fail "build output not gitignored"
fi

echo "MIXER FIDELITY OK (goldens=$DIFF_COUNT diff=bit-identical maxvoices=$MAXV_ALL steals=$STEALS_ALL s01stops=4)"
