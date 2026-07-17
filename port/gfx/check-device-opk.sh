#!/usr/bin/env bash
# M3 task 7 leg-3 engine: OPK packaging + FRONTEND launch evidence
# (fix_plan §M3 task 7; invoked by port/sim/device/verify_m3.sh).
# Prints the FIXED verdict literal
#   OPK LAUNCH OK (frontend-launched via gmenu2x, boot marker bin-sha == stamp, evidence rc=0, stream prefix 900/900 vs frozen g01, shot structural)
# exit 0, iff ALL of:
#
#   1. OPK ASSETS (committed, pinned): port/gfx/opk/mlfk.sh (launcher —
#      Exec= a script, never the binary; tmpfs evidence + SD data-dir
#      env chain; boot marker carries sha256 of the MOUNTED binary),
#      meleelight.funkey-s.desktop (anchored field grammar + the
#      MANDATORY trailing empty line), icon32.png (32x32 PNG, sha
#      pinned). ICON PROVENANCE: generated from OUR OWN C renderer's
#      host framebuffer dump of g01 frame 900 (gfx_app_headless
#      --shot-frame 900, the task-4 artifact) via
#      `ffmpeg -i g01.app-shot-a.ppm -vf "crop=120:96:60:72,
#      scale=32:32:flags=area,eq=brightness=0.12:saturation=1.6"` —
#      meleelight-derived vector art rendered by port/gfx/raster.c; no
#      Nintendo sprite/asset bytes (upstream meleelight draws stylized
#      vector art; this is a downscaled stage silhouette). Private
#      project — nothing is ever distributed (CLAUDE.md hard rule 4).
#   2. PACKAGING: staged {gfx_device (stamped bytes, rehash-adjacent),
#      mlfk.sh, .desktop, icon32.png}; packaged INSIDE the SDK
#      container with its mksquashfs 4.4 ONLY (version line asserted by
#      anchored grammar BEFORE packaging — a newer mksquashfs writes a
#      squashfs the FunKey kernel SILENTLY fails to mount; CLAUDE.md
#      §Commands OPK rule), `-all-root -noappend -no-exports -no-xattrs
#      -comp gzip`; contents verified by unsquashfs-in-container +
#      per-file cmp vs the stage (PROCESS §4 "OPK contents verified by
#      checksum after packaging").
#   3. FRONTEND LAUNCH (the real path, never adb-exec): OPK pushed to
#      /mnt/Applications (device sha == host sha), data plane
#      provisioned on /mnt/mlfk-scratch (each file sha-verified;
#      sndpack vs the frozen SNDPACK pin), gmenu2x RESTARTED (pkill ->
#      the /usr/local/sbin/frontend supervisor respawns it). MEASURED
#      DEVICE BEHAVIOUR (iter 58, empirical — gmenu2x IGNORES its conf
#      section/link for the start position; a pkill-respawn lands at
#      the STABLE persisted section, which pkill never advances — only
#      a CLEAN gmenu2x exit does, and launching MeleeLight, a
#      games-section app, keeps the section = games; verified stable
#      across many respawns): the persisted start section is `games`.
#      The FROZEN navigation is injected through port/tools/fk_input
#      (our own uinput device -> the real SDL keysym path):
#        altright `n` -> settings (section change resets link to 0),
#        altleft  `m` -> back to games at LINK 0 = "Mario 64 (U)" (the
#          n/m round trip NORMALIZES the within-games link regardless
#          of whatever link was inherited — section change always
#          resets to link 0, measured),
#        right    `r` -> link 1 = "MeleeLight" (alphabetical grid order
#          Mario64 < MeleeLight, measured),
#        accept   `a` -> launch.
#      SECTION-ANCHOR HONESTY (documented, fail-CLOSED): the only
#      inherited-state assumption is the persisted section == games. It
#      cannot be anchored by a fixed key count (sections wrap, period
#      4) and gmenu2x has no home-to-section key (`q` = volume,
#      measured). If a human left the menu in a different section AND
#      launched a non-games app between gate runs, the nav lands on the
#      wrong entry, no boot marker appears, and this leg FAILS LOUD —
#      never a false pass (the boot-marker bin-sha pin below also
#      rejects any other app's bytes). A menu framebuffer grab
#      (pre-accept) is pulled as EVIDENCE ONLY — the launch verdict is
#      the BOOT MARKER, never pixels (pre-registration iter 58).
#   4. LAUNCH EVIDENCE (all judged on the HOST): boot marker appears
#      (bounded poll) with the exact 3-line grammar and its `bin` sha
#      == the arm-build stamp's gfx_device record (PROCESS §4
#      installed-artifact identity at launch time — proves the
#      frontend MOUNTED and ran the packaged bytes); mode=evidence
#      (the pinned opk-args file selected the bounded paced g01
#      replay, 900 frames, audio on, --shot-frame 450); opk.rc is
#      exactly `RC=0\n`; the pulled stream is a STRICT full 900-frame
#      stream whose every frame hash equals the frozen g01 stream's
#      prefix (real conformance teeth on the OPK-installed binary);
#      the in-app screenshot passes the UNCHANGED judge-shot.js
#      (structural + non-blank); the app summary line parses under the
#      task-4 anchored grammar with 0 failed presents (render skips
#      are NOT gated here — perf is the audio leg's business; a
#      skip-flooded run cannot fake the shot: judge-shot rejects
#      blank/garbage frames).
#   5. HYGIENE: the OPK is REMOVED from /mnt/Applications at check end
#      (evidence lives in pulled host artifacts; nothing stays
#      installed — trap-guarded, also on failure) and gmenu2x is
#      respawn-cycled with the restoration VERIFIED (iter 60, review-58
#      L1: pkill rcs captured + busybox no-match case-split, then a
#      bounded rig_proc_respawn_poll must SEE gmenu2x running before
#      the verdict prints; iter 62, review-60 L1-residual: the poll is
#      the TRUE-RESPAWN form — pre-kill pid captured, its exit required,
#      then a live successor pid != old; the EXIT trap performs the same
#      verified cycle — WARN-loud — only when step 8 did not already
#      verify, never re-killing a verified frontend); device writes only /tmp/mlfk +
#      /mnt/mlfk-scratch (+ the OPK file itself, removed); own
#      processes killed on exit; the frontend is NEVER parked by this
#      check (it IS the surface under test).
#
# Rig plumbing INHERITED from port/sim/device/riglib.sh (Tier-A arc,
# VERDICT: GO): nonce-dsh, pullv, rm-before-produce+made(), shared
# stamp-cached arm build (this script's bytes are stamp input via
# RIG_SCRIPTS), rehash-adjacent-to-push, the shared no-reclaim
# device-keyed lock, the no-commit guard.
#
# Env: FUNKEY_ADB_ID (device id), MLFK_FORCE_ARM=1 (ignore build
# stamp), MLFK_OPK_SKIP_NAV=1 (NEGATIVE TESTING ONLY: skip the
# injection so the boot-marker poll must fail loud — the T3 tooth).
set -euo pipefail
cd "$(dirname "$0")/../.."

GFX=port/gfx
OPKDIR=$GFX/opk
BUILD=$GFX/build
CAL=port/sim/calib
DEVB=$CAL/build/device
SIM=port/sim/sim
TABLES=pipeline/build/sim-tables
AUDIO_OUT=pipeline/build/audio-dev
DIST="${MELEELIGHT_CLONE:-$HOME/.cache/meleelight-funkey-s/upstream}"
FDC=oracle/fdlibm-crosscheck
DTMP=/tmp/mlfk
DSD=/mnt/mlfk-scratch
DEVAPPS=/mnt/Applications
mkdir -p "$BUILD" "$DEVB"

# --- frozen pins (M3 task 7; changing any is a reviewed repo change) ---------
OPK_NAME=mlfk-evidence.opk
EVID_FRAMES=900             # bounded evidence replay (15 s paced)
EVID_SHOT_FRAME=450         # mid-run, in-match (post-`starting` window)
BUDGET_NS=16666667          # 60 fps pacing budget
GFXDATA_FROZEN=$GFX/gfxdata-frozen.txt
GFXDATA_SHA256=5499a3dd5fc374d6ed988faf0bef6fa2e189eb314e892bdd83c7534dc0865c94
# SNDPACK identity — the SAME frozen pin check-device-audio.sh owns
# (iter 57, measured-then-frozen; that script is the pin's origin and
# single point of re-freeze). Reuse-if-pinned, else rebuilt below.
SNDPACK_SHA256=f69579082fe569249879faa5ceccb7a810d94d8092695ddc8bb543f3bda3ccb4
SND_COUNT_PIN=180
# icon32.png identity (measured-then-frozen iter 58; provenance above)
ICON_SHA256=efea406086050d51b3894692c6030afef1b0cc7ed06de442b728ef2e6072fe99
# The SDK container's squashfs-tools version line, EXACT (anchored
# whole-line — a different mksquashfs writes an OPK the FunKey kernel
# silently cannot mount; measured iter 58):
MKSQ_VERSION_LINE='mksquashfs version 4.4 (2019/08/29)'
RESPAWN_TRIES=15            # x1 s: frontend supervisor respawn window
MARKER_TRIES=30             # x2 s: accept-press -> boot marker
RC_TRIES=30                 # x2 s: evidence run (~15 s paced) -> rc file
FROZEN_G01=oracle/goldens/g01-fox-marth-battlefield.sha256.json

source port/sim/device/adbsh.sh # (also defines $DEV — it keys the lock)
source port/sim/device/riglib.sh

rig_lock_acquire

# FRONTEND_VERIFIED — set to 1 ONLY after step 8's pkill+respawn-poll
# has VERIFIED gmenu2x running (iter 60, review-58 L1). The EXIT-trap
# frontend cycle is conditional on it: re-killing an already-verified
# frontend from the trap would end the run frontend-down UNVERIFIED —
# the exact accident the finding described.
FRONTEND_VERIFIED=0

opk_cleanup() {
  local prc pn cpid old_cpid
  # own processes: per-process pkill with the rc CAPTURED and busybox
  # case-split (0 = killed, 1 = no match — nothing of ours running;
  # anything else is a real failure, loud) — never masked with `; true`
  # (iter 60, review-58 L1).
  for pn in gfx_device fk_input; do
    prc=0
    rig_dsh_retry "pkill $pn" || prc=$?
    case "$prc" in
      0|1) : ;;
      *) echo "WARN: could not pkill $pn on the device (rc $prc)" >&2 ;;
    esac
  done
  # the OPK must never stay installed past the check (hygiene §5)
  rig_dsh_retry "rm -f $DEVAPPS/$OPK_NAME" \
    || echo "WARN: could not remove $DEVAPPS/$OPK_NAME — remove it by hand" >&2
  # leave the frontend RUNNING on a clean menu (we never park; a fresh
  # respawn rescans /mnt/Applications without our OPK). Skip when step 8
  # already VERIFIED the respawn; otherwise cycle + verify here, WARN
  # loud on failure (a trap must never mask the run's real exit code).
  if [ "$FRONTEND_VERIFIED" != 1 ]; then
    # TRUE-RESPAWN (iter 62, review-60 L1-residual): capture the
    # pre-kill pid so the poll can require its exit + a successor.
    old_cpid="$(rig_proc_pid gmenu2x)" || old_cpid=""
    prc=0
    rig_dsh_retry "pkill gmenu2x" || prc=$?
    case "$prc" in
      0|1) : ;;
      *) echo "WARN: cleanup pkill gmenu2x failed (rc $prc)" >&2 ;;
    esac
    if cpid="$(rig_proc_respawn_poll gmenu2x "$RESPAWN_TRIES" "$old_cpid")"; then
      echo "   cleanup: gmenu2x respawned (pid $cpid) — frontend RUNNING" >&2
    else
      echo "WARN: cleanup could NOT verify a gmenu2x respawn within ${RESPAWN_TRIES}s — the frontend may be DOWN; check the device" >&2
    fi
  fi
  rig_cleanup
}
trap opk_cleanup EXIT

require_device

echo "== [0/8] stale-state startup normalization (cross-run chokepoint) =="
# This check needs the frontend RUNNING (it is the surface under test):
# a stale park marker from a prior crashed render/audio run would keep
# gmenu2x down — remove it (RC-verified); a stale ARMED deadman from
# those runs is disarmed via its designed cancel channel (task-4
# chokepoint, same sequencing: marker first, then deadman).
stale_marker=0
stale_deadman=0
nrc=0
dsh "test -f /mnt/disable_frontend" >/dev/null || nrc=$?
case "$nrc" in
  0) stale_marker=1 ;;
  1) : ;;
  *) echo "DEVICE FAIL: startup normalization could not probe the frontend marker (rc $nrc)" >&2; exit 1 ;;
esac
nrc=0
dsh "test -e $DTMP/deadman.nonce -o -e $DTMP/deadman.pid" >/dev/null || nrc=$?
case "$nrc" in
  0) stale_deadman=1 ;;
  1) : ;;
  *) echo "DEVICE FAIL: startup normalization could not probe for stale deadman state (rc $nrc)" >&2; exit 1 ;;
esac
if [ "$stale_marker" = 1 ] || [ "$stale_deadman" = 1 ]; then
  echo "WARN: stale prior-run state on the device (marker=$stale_marker deadman-state=$stale_deadman) — normalizing" >&2
  if [ "$stale_marker" = 1 ]; then
    dsh "rm -f /mnt/disable_frontend"
    dsh "test ! -f /mnt/disable_frontend"
    echo "   stale /mnt/disable_frontend removed (RC-verified gone)"
  fi
  if [ "$stale_deadman" = 1 ]; then
    dsh "mkdir -p $DTMP && touch $DTMP/deadman.cancel"
    sdm_gone=0
    for _ in $(seq 1 6); do # §7#1 bounded foreground poll
      if dsh "test ! -f $DTMP/deadman.pid" >/dev/null 2>&1; then sdm_gone=1; break; fi
      sleep 2
    done
    if [ "$sdm_gone" = 1 ]; then
      echo "   stale deadman disarmed via its cancel channel (exit verified: pid file gone)"
    else
      sdm_pid="$(dsh "cat $DTMP/deadman.pid")" || {
        echo "DEVICE FAIL: startup normalization could not read the stale deadman pid file" >&2
        exit 1
      }
      sdm_pid="${sdm_pid%$'\n'}"
      if ! [[ "$sdm_pid" =~ ^[0-9]{1,7}$ ]]; then
        echo "DEVICE FAIL: stale deadman.pid is not a bounded decimal pid ('$sdm_pid')" >&2
        exit 1
      fi
      nrc=0
      dsh "test -d /proc/$sdm_pid" >/dev/null || nrc=$?
      case "$nrc" in
        0)
          echo "DEVICE FAIL: stale deadman (pid $sdm_pid) is STILL RUNNING and ignored its cancel for 12 s — a defect, not stale state; inspect the device" >&2
          exit 1
          ;;
        1)
          echo "   stale deadman.pid was reboot/kill-orphaned (pid $sdm_pid dead) — stale state"
          ;;
        *)
          echo "DEVICE FAIL: startup normalization could not probe pid $sdm_pid liveness (rc $nrc)" >&2
          exit 1
          ;;
      esac
    fi
    dsh "rm -rf $DTMP"
    echo "   stale deadman state wiped ($DTMP)"
  fi
else
  echo "   no stale prior-run state (marker absent, no deadman state)"
fi
rig_devsha_selftest

echo "== [1/8] committed OPK assets (desktop grammar + trailing empty line, icon pin, launcher) =="
DESKTOP=$OPKDIR/meleelight.funkey-s.desktop
LAUNCHER=$OPKDIR/mlfk.sh
ICON=$OPKDIR/icon32.png
made "$DESKTOP" "$LAUNCHER" "$ICON"
# .desktop: anchored whole-file grammar — the 9 exact lines this repo
# ships (a field drift is a reviewed change) + the MANDATORY trailing
# empty line (file ends in exactly one empty line: last two bytes \n\n,
# not more — measured envelope requirement).
desktop_want=$'[Desktop Entry]\nName=MeleeLight\nComment=MeleeLight port (private build)\nExec=mlfk.sh\nTerminal=false\nType=Application\nStartupNotify=true\nIcon=icon32\nCategories=games;\n\n'
if ! printf '%s' "$desktop_want" | cmp -s - "$DESKTOP"; then
  echo "DEVICE FAIL: $DESKTOP does not match the pinned desktop-entry bytes (incl. the mandatory trailing empty line)" >&2
  exit 1
fi
echo "   .desktop byte-exact vs the pinned entry (trailing empty line present)"
# icon: PNG magic + 32x32 IHDR + frozen content pin
node -e '
  const b = require("fs").readFileSync(process.argv[1]);
  if (b.subarray(0, 8).toString("hex") !== "89504e470d0a1a0a") { console.error("not a PNG"); process.exit(3); }
  if (b.subarray(12, 16).toString("ascii") !== "IHDR") { console.error("no IHDR"); process.exit(3); }
  const w = b.readUInt32BE(16), h = b.readUInt32BE(20);
  if (w !== 32 || h !== 32) { console.error("icon is " + w + "x" + h + ", want 32x32"); process.exit(3); }
  console.log("   icon32.png: PNG 32x32");
' "$ICON"
isum="$(rig_host_sha256 "$ICON")" || exit 1
if [ "$isum" != "$ICON_SHA256" ]; then
  echo "DEVICE FAIL: icon32.png sha256 $isum != pinned $ICON_SHA256" >&2
  exit 1
fi
if [ "$(head -1 "$LAUNCHER")" != "#!/bin/sh" ]; then
  echo "DEVICE FAIL: mlfk.sh does not start with #!/bin/sh" >&2
  exit 1
fi
echo "   asset pins OK (icon $ICON_SHA256)"

echo "== [2/8] host data plane (g01 params + M1 tables + SIMDATA1 + trace + GFXDATA + SNDPACK) =="
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
")" || { echo "DEVICE FAIL: g01 manifest param extraction failed" >&2; exit 1; }
if [ -z "$gparams" ]; then
  echo "DEVICE FAIL: g01 manifest param extraction returned nothing" >&2
  exit 1
fi
while IFS='=' read -r gk gv; do
  case "$gk" in
    name)
      if ! [[ "$gv" =~ ^[a-z0-9][a-z0-9-]*$ ]]; then
        echo "DEVICE FAIL: manifest g01.name fails validation ('$gv')" >&2
        exit 1
      fi
      name=$gv
      ;;
    trace)
      if ! [[ "$gv" =~ ^[a-z0-9][a-z0-9-]*\.trace\.json$ ]]; then
        echo "DEVICE FAIL: manifest g01.trace fails validation ('$gv')" >&2
        exit 1
      fi
      trace=$gv
      ;;
    seed|p1|p2|stage|frames)
      if ! [[ "$gv" =~ ^[0-9]{1,12}$ ]]; then
        echo "DEVICE FAIL: manifest g01.$gk not a bounded decimal integer ('$gv')" >&2
        exit 1
      fi
      printf -v "$gk" '%s' "$gv"
      ;;
    *)
      echo "DEVICE FAIL: unexpected manifest extraction line '$gk=$gv'" >&2
      exit 1
      ;;
  esac
done <<< "$gparams"
: "$name" "$seed" "$p1" "$p2" "$stage" "$frames" "$trace"
[ "$frames" -le 5000 ] || { echo "DEVICE FAIL: g01 frames $frames > 5000" >&2; exit 1; }
[ "$EVID_FRAMES" -le "$frames" ] || { echo "DEVICE FAIL: EVID_FRAMES $EVID_FRAMES > golden frames $frames" >&2; exit 1; }
[ "$stage" -le 5 ] || { echo "DEVICE FAIL: g01 stage $stage > 5" >&2; exit 1; }
[ "$p1" -le 4 ] || { echo "DEVICE FAIL: g01 p1 $p1 > 4" >&2; exit 1; }
[ "$p2" -le 4 ] || { echo "DEVICE FAIL: g01 p2 $p2 > 4" >&2; exit 1; }
made "$FROZEN_G01"
if [ "oracle/goldens/$name.sha256.json" != "$FROZEN_G01" ]; then
  echo "DEVICE FAIL: manifest g01 name ($name) does not resolve to the pinned frozen stream $FROZEN_G01" >&2
  exit 1
fi
anim_file() {
  case "$1" in
    0) echo anim_0_marth.bin ;;
    1) echo anim_1_puff.bin ;;
    2) echo anim_2_fox.bin ;;
    3) echo anim_3_falco.bin ;;
    4) echo anim_4_falcon.bin ;;
    *) echo "DEVICE FAIL: bad char id '$1'" >&2; return 1 ;;
  esac
}
ANIM_P1="$(anim_file "$p1")"
ANIM_P2="$(anim_file "$p2")"
made "$GFXDATA_FROZEN"
gsum="$(rig_host_sha256 "$GFXDATA_FROZEN")" || exit 1
if [ "$gsum" != "$GFXDATA_SHA256" ]; then
  echo "DEVICE FAIL: $GFXDATA_FROZEN sha256 $gsum != pinned $GFXDATA_SHA256" >&2
  exit 1
fi
bash pipeline/extractor/build-extractor.sh
rm -f "$TABLES/ml_tables.c" "$TABLES/ml_tables.h" \
  "$TABLES/ml_stages.c" "$TABLES/ml_stages.h" \
  "$TABLES/$ANIM_P1" "$TABLES/$ANIM_P2"
node pipeline/run.js --only animations,tables,stages --out "$TABLES"
made "$TABLES/ml_tables.c" "$TABLES/ml_tables.h" \
  "$TABLES/ml_stages.c" "$TABLES/ml_stages.h" \
  "$TABLES/$ANIM_P1" "$TABLES/$ANIM_P2"
rm -f "$DEVB/simdata.txt"
node "$CAL/dump-sim-data.js" --out "$DEVB/simdata.txt"
made "$DEVB/simdata.txt"
if [ ! -f "oracle/goldens/$trace" ]; then
  echo "DEVICE FAIL: g01 trace file oracle/goldens/$trace missing" >&2
  exit 1
fi
rm -f "$DEVB/g01.trace.txt"
node "$SIM/trace-to-txt.js" "oracle/goldens/$trace" "$DEVB/g01.trace.txt"
made "$DEVB/g01.trace.txt"
# SNDPACK: reuse iff the bytes equal the FROZEN pin (identity is the
# requirement here — the pin's origin/re-freeze point is
# check-device-audio.sh); otherwise rebuild from the reused pipeline
# audio stage exactly like that script and re-assert the pin.
sp_ok=0
if [ -f "$BUILD/sndpack.bin" ]; then
  psum="$(rig_host_sha256 "$BUILD/sndpack.bin")" || exit 1
  if [ "$psum" = "$SNDPACK_SHA256" ]; then sp_ok=1; fi
fi
if [ "$sp_ok" != 1 ]; then
  echo "   sndpack absent or off-pin — rebuilding from the pipeline audio stage"
  rm -rf "$AUDIO_OUT"
  node pipeline/run.js --only audio --dist "$DIST" --out "$AUDIO_OUT"
  made "$AUDIO_OUT/sounds.json"
  pack_line_re="^pack-snd OK count=${SND_COUNT_PIN} dataBytes=[0-9]{1,12} fileBytes=[0-9]{1,12}\$"
  for side in a b; do
    rm -f "$BUILD/sndpack-$side.bin"
    pout="$(node "$GFX/pack-snd.js" "$AUDIO_OUT" "$BUILD/sndpack-$side.bin")" || {
      echo "DEVICE FAIL: pack-snd.js failed (side $side)" >&2
      exit 1
    }
    if ! printf '%s\n' "$pout" | grep -Eq "$pack_line_re"; then
      echo "DEVICE FAIL: pack-snd output failed the pinned grammar: '$pout'" >&2
      exit 1
    fi
    made "$BUILD/sndpack-$side.bin"
  done
  cmp "$BUILD/sndpack-a.bin" "$BUILD/sndpack-b.bin"
  rm -f "$BUILD/sndpack.bin"
  cp "$BUILD/sndpack-a.bin" "$BUILD/sndpack.bin"
  made "$BUILD/sndpack.bin"
  psum="$(rig_host_sha256 "$BUILD/sndpack.bin")" || exit 1
  if [ "$psum" != "$SNDPACK_SHA256" ]; then
    echo "DEVICE FAIL: rebuilt sndpack sha256 $psum != pinned $SNDPACK_SHA256" >&2
    exit 1
  fi
fi
echo "   data plane OK (tables, simdata, g01 trace, gfxdata pin, sndpack == frozen pin)"

echo "== [3/8] armv7 build (shared rig stamp) =="
rig_arm_build
rig_stamp_rehash gfx_device fk_input
STAMP_GFX_SHA="$(rig_stamp_bin_sha gfx_device)"

echo "== [4/8] stage + mksquashfs 4.4 (SDK container ONLY) + content verification =="
STAGE=$BUILD/opk-stage
rm -rf "$STAGE" "$BUILD/$OPK_NAME" "$BUILD/opk-verify"
mkdir -p "$STAGE"
cp "$DEVB/gfx_device" "$LAUNCHER" "$DESKTOP" "$ICON" "$STAGE/"
chmod +x "$STAGE/gfx_device" "$STAGE/mlfk.sh"
# the staged binary must BE the stamped bytes (rehash was adjacent)
ssum="$(rig_host_sha256 "$STAGE/gfx_device")" || exit 1
if [ "$ssum" != "$STAMP_GFX_SHA" ]; then
  echo "DEVICE FAIL: staged gfx_device sha ($ssum) != stamp record ($STAMP_GFX_SHA)" >&2
  exit 1
fi
# version-assert THEN package, in one serial container run; the version
# line is captured to a file and judged on the host by exact match.
rm -f "$BUILD/mksq-version.txt"
docker run --rm -v "$PWD":/work -w /work "$ARMIMGID" bash -lc '
  set -e
  export PATH=/opt/FunKey-sdk-2.3.0/bin:$PATH
  mksquashfs -version | head -1 > port/gfx/build/mksq-version.txt
  mksquashfs port/gfx/build/opk-stage port/gfx/build/'"$OPK_NAME"' \
    -all-root -noappend -no-exports -no-xattrs -comp gzip >/dev/null
'
made "$BUILD/mksq-version.txt" "$BUILD/$OPK_NAME"
if ! printf '%s\n' "$MKSQ_VERSION_LINE" | cmp -s - "$BUILD/mksq-version.txt"; then
  echo "DEVICE FAIL: container mksquashfs version line is not exactly '$MKSQ_VERSION_LINE':" >&2
  cat "$BUILD/mksq-version.txt" >&2
  echo "  (a non-4.4 mksquashfs writes an OPK the FunKey kernel silently cannot mount)" >&2
  exit 1
fi
echo "   packaged with $MKSQ_VERSION_LINE"
# contents verification: unsquash in the SAME container, cmp every file
docker run --rm -v "$PWD":/work -w /work "$ARMIMGID" bash -lc '
  set -e
  export PATH=/opt/FunKey-sdk-2.3.0/bin:$PATH
  unsquashfs -d port/gfx/build/opk-verify port/gfx/build/'"$OPK_NAME"' >/dev/null
'
for f in gfx_device mlfk.sh meleelight.funkey-s.desktop icon32.png; do
  made "$BUILD/opk-verify/$f"
  cmp "$STAGE/$f" "$BUILD/opk-verify/$f"
done
n_stage="$(find "$STAGE" -type f | wc -l | tr -d ' ')"
n_verify="$(find "$BUILD/opk-verify" -type f | wc -l | tr -d ' ')"
if [ "$n_stage" != 4 ] || [ "$n_verify" != 4 ]; then
  echo "DEVICE FAIL: OPK content count mismatch (stage $n_stage, unsquashed $n_verify, want 4)" >&2
  exit 1
fi
OPK_SHA="$(rig_host_sha256 "$BUILD/$OPK_NAME")" || exit 1
echo "   OPK contents verified (4 files byte-identical through unsquashfs; opk sha $OPK_SHA)"

echo "== [5/8] push: OPK -> $DEVAPPS, data plane -> $DSD, injector -> $DTMP =="
dsh "rm -rf $DTMP $DSD && mkdir -p $DTMP $DSD"
adb -s "$DEV" push "$BUILD/$OPK_NAME" "$DEVAPPS/" >/dev/null
dsum="$(rig_dev_sha256 "$DEVAPPS/$OPK_NAME")" || exit 1
if [ "$dsum" != "$OPK_SHA" ]; then
  echo "DEVICE FAIL: device-side OPK sha ($dsum) != host ($OPK_SHA)" >&2
  exit 1
fi
adb -s "$DEV" push "$DEVB/simdata.txt" "$DEVB/g01.trace.txt" \
  "$GFXDATA_FROZEN" "$TABLES/$ANIM_P1" "$TABLES/$ANIM_P2" \
  "$BUILD/sndpack.bin" "$DSD/" >/dev/null
for hf in "$DEVB/simdata.txt" "$DEVB/g01.trace.txt" "$GFXDATA_FROZEN" \
          "$TABLES/$ANIM_P1" "$TABLES/$ANIM_P2" "$BUILD/sndpack.bin"; do
  bn="$(basename "$hf")"
  hsum="$(rig_host_sha256 "$hf")" || exit 1
  dsum="$(rig_dev_sha256 "$DSD/$bn")" || exit 1
  if [ "$dsum" != "$hsum" ]; then
    echo "DEVICE FAIL: pushed $bn device sha ($dsum) != host sha ($hsum)" >&2
    exit 1
  fi
done
# the pinned evidence args (mode selector): generated fresh, pushed,
# sha-verified — the launcher passes this line verbatim to gfx_device
rm -f "$BUILD/opk-args"
printf '%s' "--trace $DSD/g01.trace.txt --simdata $DSD/simdata.txt --gfxdata $DSD/gfxdata-frozen.txt --anim-dir $DSD --seed $seed --p1 $p1 --p2 $p2 --stage $stage --frames $EVID_FRAMES --pace 1 --budget-ns $BUDGET_NS --sndpack $DSD/sndpack.bin --out $DTMP/opk/opk-out.txt --timing $DTMP/opk/opk-tim.txt --shot-frame $EVID_SHOT_FRAME --shot-ppm $DTMP/opk/opk-shot.ppm --shot-pgm $DTMP/opk/opk-shot.pgm" > "$BUILD/opk-args"
made "$BUILD/opk-args"
adb -s "$DEV" push "$BUILD/opk-args" "$DSD/" >/dev/null
hsum="$(rig_host_sha256 "$BUILD/opk-args")" || exit 1
dsum="$(rig_dev_sha256 "$DSD/opk-args")" || exit 1
if [ "$dsum" != "$hsum" ]; then
  echo "DEVICE FAIL: pushed opk-args device sha ($dsum) != host sha ($hsum)" >&2
  exit 1
fi
adb -s "$DEV" push "$DEVB/fk_input" "$DTMP/" >/dev/null
rig_push_provenance "$DTMP" fk_input
dsh "chmod +x $DTMP/fk_input"
# the FROZEN measured navigation script (iter 58 empirical; header
# "MEASURED DEVICE BEHAVIOUR": persisted section = games; n then m
# round-trips to games at LINK 0 (Mario 64), normalizing the inherited
# within-games link; r -> link 1 "MeleeLight"; a -> launch)
rm -f "$BUILD/opk-nav.script"
cat > "$BUILD/opk-nav.script" << 'EOF'
# generated by check-device-opk.sh — frozen measured gmenu2x navigation
s 1500
d n
s 250
u n
s 400
d m
s 250
u m
s 400
d r
s 250
u r
s 400
d a
s 250
u a
EOF
made "$BUILD/opk-nav.script"
adb -s "$DEV" push "$BUILD/opk-nav.script" "$DTMP/" >/dev/null
hsum="$(rig_host_sha256 "$BUILD/opk-nav.script")" || exit 1
dsum="$(rig_dev_sha256 "$DTMP/opk-nav.script")" || exit 1
if [ "$dsum" != "$hsum" ]; then
  echo "DEVICE FAIL: pushed nav script device sha ($dsum) != host sha ($hsum)" >&2
  exit 1
fi
echo "   pushed + sha-verified (OPK, 7 data files, fk_input, nav script)"

echo "== [6/8] frontend cycle: respawn gmenu2x (deterministic start state) + inject nav =="
# marker must be ABSENT (frontend alive is the precondition under test)
if ! dsh "test ! -f /mnt/disable_frontend" >/dev/null 2>&1; then
  echo "DEVICE FAIL: /mnt/disable_frontend present after normalization — frontend cannot run" >&2
  exit 1
fi
dsh "rm -rf $DTMP/opk" # a stale evidence dir must never satisfy a poll
# TRUE-RESPAWN (iter 62, review-60 L1-residual): capture the pre-kill
# pid; the poll then requires its EXIT plus a live successor != it — a
# lingering old gmenu2x under a slow SIGTERM can never fake a respawn.
old_gpid="$(rig_proc_pid gmenu2x)" || old_gpid=""
prc=0
dsh "pkill gmenu2x" >/dev/null 2>&1 || prc=$?
case "$prc" in
  0) : ;;
  1) echo "WARN: gmenu2x was not running before the cycle (supervisor should respawn it)" >&2 ;;
  *) echo "DEVICE FAIL: pkill gmenu2x failed (rc $prc)" >&2; exit 1 ;;
esac
# §7#1 bounded foreground poll (shared riglib body, iter 60; iter-62
# true-respawn form)
gpid="$(rig_proc_respawn_poll gmenu2x "$RESPAWN_TRIES" "$old_gpid")" || {
  echo "DEVICE FAIL: gmenu2x did not TRUE-respawn within ${RESPAWN_TRIES}s of pkill (old pid must exit AND a successor must appear — frontend supervisor defect / lingering old process?)" >&2
  exit 1
}
echo "   gmenu2x respawned (pid $gpid) — menu at the deterministic start state"
if [ "${MLFK_OPK_SKIP_NAV:-0}" = 1 ]; then
  echo "WARN: MLFK_OPK_SKIP_NAV=1 — SKIPPING injection (negative testing only)" >&2
else
  dsh "sh -lc 'cd $DTMP && ./fk_input opk-nav.script'" || {
    echo "DEVICE FAIL: fk_input nav injection failed" >&2
    exit 1
  }
fi
# menu framebuffer grab: EVIDENCE ONLY (launch verdict = boot marker)
rm -f "$DEVB/opk-menu-fb.raw"
dsh "dd if=/dev/fb0 of=$DTMP/opk-menu-fb.raw bs=65536 2>/dev/null"
pullv "$DTMP/opk-menu-fb.raw" "$DEVB/opk-menu-fb.raw" \
  || echo "WARN: menu framebuffer grab failed (evidence-only artifact)" >&2

echo "== [7/8] launch evidence: boot marker, rc, stream prefix, screenshot (host-judged) =="
marker_seen=0
for _ in $(seq 1 "$MARKER_TRIES"); do # §7#1 bounded foreground poll
  if dsh "test -f $DTMP/opk/boot-marker" >/dev/null 2>&1; then marker_seen=1; break; fi
  sleep 2
done
if [ "$marker_seen" != 1 ]; then
  echo "DEVICE FAIL: boot marker never appeared ($((MARKER_TRIES * 2))s) — the frontend did not launch the OPK (mount failure / navigation landed elsewhere)" >&2
  exit 1
fi
pullv "$DTMP/opk/boot-marker" "$DEVB/opk-boot-marker.txt"
# marker grammar: EXACTLY three lines, anchored
if [ "$(wc -l < "$DEVB/opk-boot-marker.txt" | tr -d ' ')" != 3 ]; then
  echo "DEVICE FAIL: boot marker is not exactly 3 lines:" >&2
  cat "$DEVB/opk-boot-marker.txt" >&2
  exit 1
fi
if [ "$(sed -n 1p "$DEVB/opk-boot-marker.txt")" != "MLFK OPK BOOT" ]; then
  echo "DEVICE FAIL: boot marker line 1 is not 'MLFK OPK BOOT'" >&2
  exit 1
fi
mline="$(sed -n 2p "$DEVB/opk-boot-marker.txt")"
if ! [[ "$mline" =~ ^bin\ ([0-9a-f]{64})$ ]]; then
  echo "DEVICE FAIL: boot marker line 2 fails the 'bin <64hex>' grammar ('$mline')" >&2
  exit 1
fi
marker_sha="${BASH_REMATCH[1]}"
if [ "$marker_sha" != "$STAMP_GFX_SHA" ]; then
  echo "DEVICE FAIL: boot marker bin sha ($marker_sha) != arm-build stamp gfx_device record ($STAMP_GFX_SHA) — the frontend ran DIFFERENT bytes" >&2
  exit 1
fi
if [ "$(sed -n 3p "$DEVB/opk-boot-marker.txt")" != "mode evidence" ]; then
  echo "DEVICE FAIL: boot marker line 3 is not 'mode evidence' ('$(sed -n 3p "$DEVB/opk-boot-marker.txt")')" >&2
  exit 1
fi
echo "   boot marker OK (mounted-binary sha == stamp record $STAMP_GFX_SHA; mode evidence)"
rc_seen=0
for _ in $(seq 1 "$RC_TRIES"); do # §7#1 bounded foreground poll
  if dsh "test -f $DTMP/opk/opk.rc" >/dev/null 2>&1; then rc_seen=1; break; fi
  sleep 2
done
if [ "$rc_seen" != 1 ]; then
  dsh "cat $DTMP/opk/mlfk.log" >&2 || true
  echo "DEVICE FAIL: evidence run never finished (rc file absent after $((RC_TRIES * 2))s)" >&2
  exit 1
fi
pullv "$DTMP/opk/opk.rc" "$DEVB/opk.rc"
if ! printf 'RC=0\n' | cmp -s - "$DEVB/opk.rc"; then
  dsh "cat $DTMP/opk/mlfk.log" >&2 || true
  echo "DEVICE FAIL: evidence rc file is not exactly 'RC=0\\n' ($(cat "$DEVB/opk.rc"))" >&2
  exit 1
fi
pullv "$DTMP/opk/opk-out.txt" "$DEVB/opk-out.txt"
pullv "$DTMP/opk/opk-shot.ppm" "$DEVB/opk-shot.ppm"
pullv "$DTMP/opk/opk-shot.pgm" "$DEVB/opk-shot.pgm"
pullv "$DTMP/opk/mlfk.log" "$DEVB/opk-mlfk.log"
pullv "$DTMP/opk/opk-tim.txt" "$DEVB/opk-tim.txt"
# stream: STRICT whole-file grammar (F 1..EVID_FRAMES contiguous + RNG
# + SIM OK, nothing else) AND every frame hash == the frozen g01
# stream's prefix — the OPK-installed binary conforms over the
# evidence window, judged by the frozen stream, on the host.
node -e '
  const fs = require("fs");
  function die(m) { console.error("opk-stream: " + m); process.exit(3); }
  const [p, frozenPath, nStr] = process.argv.slice(1);
  const N = parseInt(nStr, 10);
  const frozen = JSON.parse(fs.readFileSync(frozenPath, "utf8"));
  if (!Array.isArray(frozen.frames) || frozen.frames.length < N) die("frozen stream shorter than " + N);
  const raw = fs.readFileSync(p, "utf8");
  if (raw.length === 0 || raw[raw.length - 1] !== "\n") die("stream does not end with a newline (truncated write?)");
  const lines = raw.split("\n"); lines.pop();
  if (lines.length !== N + 2) die("stream has " + lines.length + " lines, want " + (N + 2));
  const F_RE = /^F ([0-9]+) ([0-9a-f]{64})$/;
  for (let i = 0; i < N; i++) {
    const m = F_RE.exec(lines[i]);
    if (!m || m[1] !== String(i + 1)) die("line " + (i + 1) + " is not frame " + (i + 1) + ": " + JSON.stringify(lines[i].slice(0, 80)));
    const fr = frozen.frames[i];
    if (!fr || fr.f !== i + 1 || typeof fr.h !== "string") die("frozen frame " + (i + 1) + " malformed");
    if (m[2] !== fr.h) die("HASH MISMATCH at frame " + (i + 1) + " (device " + m[2] + " != frozen " + fr.h + ")");
  }
  if (!/^RNG (0|[1-9][0-9]*) (0|[1-9][0-9]*)$/.test(lines[N])) die("bad RNG trailer: " + JSON.stringify(lines[N]));
  if (lines[N + 1] !== "SIM OK") die("last line is not exactly SIM OK");
  console.log("   stream prefix OK: " + N + "/" + N + " frame hashes == frozen g01");
' "$DEVB/opk-out.txt" "$FROZEN_G01" "$EVID_FRAMES"
# screenshot: the UNCHANGED task-4 structural judge (blank/garbage dies)
node "$GFX/judge-shot.js" "$DEVB/opk-shot.ppm" "$DEVB/opk-shot.pgm" | sed 's/^/   /'
# app summary (task-4 anchored grammar, evidence params pinned): the
# run must be the pinned evidence shape; presents must all succeed.
# Render skips are NOT gated here (perf = the audio leg; registered).
sum_re="^gfx_app: ${EVID_FRAMES} frames, [0-9]{1,12} render skips, [0-9]{1,12} failed presents, wall [0-9]{1,12} ms, pace=1 budget=${BUDGET_NS} ns\$"
cnt="$(grep -Ec "$sum_re" "$DEVB/opk-mlfk.log")" || true
if [ "$cnt" != 1 ]; then
  echo "DEVICE FAIL: app log has $cnt lines matching the pinned evidence summary grammar (want exactly 1)" >&2
  exit 1
fi
sline="$(grep -E "$sum_re" "$DEVB/opk-mlfk.log")"
if [[ "$sline" =~ ^gfx_app:\ ${EVID_FRAMES}\ frames,\ ([0-9]{1,12})\ render\ skips,\ ([0-9]{1,12})\ failed\ presents,\ wall\ ([0-9]{1,12})\ ms, ]]; then
  ev_skips="${BASH_REMATCH[1]}"
  ev_pfails="${BASH_REMATCH[2]}"
  ev_wall="${BASH_REMATCH[3]}"
else
  echo "DEVICE FAIL: summary line failed re-extraction ('$sline')" >&2
  exit 1
fi
if [ "$ev_pfails" -ne 0 ]; then
  echo "DEVICE FAIL: $ev_pfails failed presents on the evidence run" >&2
  exit 1
fi
au_cnt="$(grep -Ec '^gfx_app audio: [0-9]{1,12} callbacks, [0-9]{1,12} underruns, [0-9]{1,12} badlen, [0-9]{1,12} voice starts, [0-9]{1,12} voice stops, [0-9]{1,12} steals, rate=44100 samples=512 channels=2$' "$DEVB/opk-mlfk.log")" || true
if [ "$au_cnt" != 1 ]; then
  echo "DEVICE FAIL: app log has $au_cnt device audio summary lines (want exactly 1 — audio callback liveness under the frontend launch)" >&2
  exit 1
fi
echo "   evidence run: skips=$ev_skips (unGated here) presentFails=0 wall=${ev_wall}ms; audio summary line present"

echo "== [8/8] hygiene: remove the OPK, respawn a clean menu VERIFIED, no-commit guard =="
dsh "rm -f $DEVAPPS/$OPK_NAME"
dsh "test ! -f $DEVAPPS/$OPK_NAME"
# final frontend cycle is a VERIFIED restoration (iter 60, review-58
# L1): pkill rc captured + busybox case-split (1 = no match is a WARN —
# the supervisor should have had it running; any other nonzero = loud
# fail), then the bounded respawn poll must SEE gmenu2x running BEFORE
# the verdict can print — OPK LAUNCH OK never coexists with a dead menu.
# TRUE-RESPAWN (iter 62, review-60 L1-residual): the pre-kill pid is
# captured and the poll requires its exit + a live successor != it.
old_fpid="$(rig_proc_pid gmenu2x)" || old_fpid=""
prc=0
dsh "pkill gmenu2x" >/dev/null 2>&1 || prc=$?
case "$prc" in
  0) : ;;
  1) echo "WARN: gmenu2x was not running at the final cycle (supervisor gap?) — poll below must still verify a respawn" >&2 ;;
  *) echo "DEVICE FAIL: final pkill gmenu2x failed (rc $prc)" >&2; exit 1 ;;
esac
fpid="$(rig_proc_respawn_poll gmenu2x "$RESPAWN_TRIES" "$old_fpid")" || {
  echo "DEVICE FAIL: gmenu2x did not TRUE-respawn within ${RESPAWN_TRIES}s after the final cycle (old pid must exit AND a successor must appear) — the frontend is DOWN; refusing to print the verdict" >&2
  exit 1
}
FRONTEND_VERIFIED=1
echo "   OPK removed from $DEVAPPS (verified gone); frontend respawn-cycled and VERIFIED RUNNING (pid $fpid)"
# $OPKDIR (mlfk.sh, .desktop, icon32.png) is COMMITTED source — the OPK
# assets, tracked and freeze-pinned; only build output is guarded.
rig_no_commit_guard "$BUILD" "$DEVB" "$TABLES"

# FIXED verdict literal — every clause is asserted above (marker sha ==
# stamp, rc bytes, 900/900 prefix vs frozen, judge-shot structural).
echo "OPK LAUNCH OK (frontend-launched via gmenu2x, boot marker bin-sha == stamp, evidence rc=0, stream prefix 900/900 vs frozen g01, shot structural)"
