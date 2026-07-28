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
#      the /usr/local/sbin/frontend supervisor respawns it).
#      SECTION ANCHOR (iter 115 — MEASURED, replacing the iter-58
#      inherited-state assumption): gmenu2x DOES read its start section
#      from the persisted conf /mnt/FunKey/.gmenu2x/gmenu2x.conf. Iter
#      58 concluded the opposite after editing the SHIPPED conf under
#      /usr/share (not the live per-user one) and iter 73 then failed
#      loud when the persisted section had drifted to `settings` (the
#      frontend opened gmenu2x's Wallpaper dialog instead — witnessed
#      again here, .loop/m4-t115-opkfoh-run2.log). So the start state
#      is now ANCHORED, not assumed: the live conf is backed up (host
#      copy, sha-verified), rewritten to `section=<games>`, and
#      RESTORED at the end and from the EXIT trap.
#      The FROZEN navigation is injected through port/tools/fk_input
#      (our own uinput device -> the real SDL keysym path):
#        altright `n` -> the next section, altleft `m` -> back to games
#          (the round trip NORMALIZES the within-games link: a section
#          change always resets link to 0, measured iter 58),
#        the GRID walk to NAV_LINK: gmenu2x's link grid is 2 COLUMNS
#          (measured iter 115 by framebuffer witness) — `d` steps a ROW
#          (+2) and `r` steps WITHIN the row and WRAPS at the row end
#          (two `r`s from link 0 return to link 0 — the iter-73 nav
#          could not have reached link 2 at all), so the walk is
#          NAV_LINK/2 downs then NAV_LINK%2 rights,
#        accept   `a` -> launch.
#      FAIL-CLOSED REGARDLESS: a nav that lands anywhere else produces
#      no boot marker and this leg dies loud — never a false pass (the
#      boot-marker bin-sha pin also rejects any other app's bytes). A
#      menu framebuffer grab (pre-accept) is pulled as EVIDENCE ONLY —
#      the launch verdict is the BOOT MARKER, never pixels
#      (pre-registration iter 58).
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
#      the in-app screenshot passes judge-shot.js (structural +
#      non-blank; criterion-5 retired for the M4 bg-art surface —
#      reviewed pin change, judge-shot.js header + AGENT-LOG iter 73);
#      the app summary line parses under the
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
# ============================================================================
# MLFK_OPK_FOH=1 — THE FOH MODE (M4 EXIT GATE leg [3]; iter 115)
# ============================================================================
# Same script, same eight steps, same discipline; what changes is WHICH
# OPK is packaged and WHICH evidence proves the frontend launched it.
# Prints the FIXED verdict literal
#   OPK FOH LAUNCH OK (frontend-launched via gmenu2x into the FOH, boot
#   marker bin-sha == stamp, evidence rc=0, 1 transitions vs frozen,
#   shot structural)
# (parsed by port/sim/device/verify_m4.sh:451 under an anchored
# full-line grammar with a proper-prefix tear guard — no progress line
# in this script may start with `OPK FOH LAUNCH`).
#
#   A. ASSETS: port/gfx/opk/mlfk-foh.sh (the FOH launcher — it ENTERS
#      THE FRONT-OF-HOUSE, never the direct-match path) +
#      meleelight-foh.funkey-s.desktop (own anchored whole-file grammar
#      + the mandatory trailing empty line) + the same pinned
#      icon32.png + the committed flow port/foh/flows/f01-vs-g01.flow
#      (the PLAY-shape OPK carries it: mlfk-foh.sh's live branch runs
#      `--flow $DIR/f01-vs-g01.flow` from the mount).
#   B. TITLE (the iter-73 stale-nav class, fixed at the cause): the FOH
#      OPK's .desktop Name is "MeleeLight FOH" — UNIQUE on the device.
#      The persistent play install /mnt/Applications/meleelight.opk
#      (title "MeleeLight", pushed by the driver after the M3 human
#      gate) is NEVER touched, and no longer makes the navigation
#      ambiguous: iter 73 measured the failure of the M3 arm's
#      single-`r` nav against TWO same-title entries. MEASURED games
#      order (iter 115 — device OPK inventory probe + framebuffer
#      witness of the live grid): link 0 "Mario 64 (U)" (/mnt/Native
#      games/sm64_us_v1.3), link 1 "MeleeLight" (the play install),
#      link 2 "MeleeLight FOH" (ours), link 3 "PicoArch", link 4
#      "Super Smash Bros." — hence NAV_LINK=2 here vs the M3 arm's 1.
#      The
#      inherited-state assumption is asserted, not assumed: the
#      games-section OPK inventory is pinned below (FOH_APPS_PIN) and a
#      drift dies LOUD with the re-measure instruction, before any
#      launch. A wrong landing can still never false-pass — the FOH
#      boot marker lives at a different path AND carries its own
#      3-line grammar AND the foh_device stamp sha.
#   C. EVIDENCE MODE: mlfk-foh.sh selects `mode evidence` exactly when
#      $DSD/foh-args exists (mlfk-foh.sh:67) and passes that single
#      line to foh_device verbatim. Every path in it is ABSOLUTE and
#      lives on the data plane — under a FRONTEND launch the OPK mount
#      point belongs to the frontend, so the flow file is pushed to
#      $DSD (sha-verified) as well as packaged.
#   D. LAUNCH EVIDENCE (all judged on the HOST): boot marker
#      /tmp/mlfk/opkfoh/boot-marker byte-equal to
#      `MLFK FOH BOOT\nbin <arm-stamp foh_device sha>\nmode evidence\n`;
#      opk.rc exactly `RC=0\n`; the recorded FOH trace judged by the
#      UNCHANGED port/foh/judge-foh-trace.js (that judge and
#      port/foh/normalize-foh-trace.js are BYTE-PINNED gate producers —
#      verify_m4.sh's freeze manifest lists both, so this consumer needs
#      no second pin of its own) + the elision normalizer
#      (byte-equal to the constructed startup->title expectation) +
#      the SAME BOUNDED-DELTA judgment check-device-foh.sh's OPK leg
#      uses (frozen f01-vs-g01.expect's own 4-line pre-input prefix +
#      `END <foh-max> transitions=1`, `input-free` declared — a
#      startup-phase stall dies rc 3); the app's own FOH summary line
#      under an anchored grammar (transitions=1, shots=2, launched=0,
#      failed presents == 0); and both tick shots pulled and judged
#      STRUCTURALLY.
#      SHOT JUDGE (registered deviation): judge-shot.js is NOT
#      applicable to menu frames — it requires a PPM+PGM pair and
#      asserts the MATCH compositor's letterbox band, while the FOH
#      renderer emits PPM only (foh_dev.c:1772) and draws full-screen.
#      The structural judge here is the smallest thing that fails on a
#      blank/garbage/frozen surface: exact `P6 240 240 255` header,
#      exact byte length, a NON-UNIFORM frame whose foreground (non-
#      modal-colour) coverage sits inside [0.5%, 60%] — measured iter
#      115: splash 4.69%, title 2.63% — and two shots that DIFFER. EXPOSURE (PROCESS §8): byte-exactness of FOH
#      shots against the host twin renderer is check-device-foh.sh's
#      job (a separate reviewed producer, per-shot cmp); THIS leg's
#      claim is narrower and exact — the FRONTEND-LAUNCHED instance
#      composited real, advancing frames.
#      Render skips are REPORTED, not gated (the M3 arm's registered
#      posture verbatim: perf is the audio/render legs' business; this
#      leg carries no low_bat_check quiesce bracket and therefore
#      makes no skip claim). A skip-flooded run cannot fake the
#      evidence: shots force a render through the valve and the
#      structural judge rejects blank/identical frames.
#
# WHAT THE M3 (default) ARM INHERITS FROM THIS CHANGE — declared, not
# claimed away (review-115-1 M5). Its staged files, verdict literal,
# device evidence paths, host artifact names and generated NAV_LINK=1
# nav-script bytes are IDENTICAL to before. Three shared behaviours are
# deliberately STRICTER for both arms:
#   (a) the frontend OPK-inventory pin (step [5]) — the M3 nav is a
#       measured link index too, and a drifted inventory should die with
#       a diagnosis instead of a mute 60 s marker timeout;
#   (b) the gmenu2x section anchor (step [6]) — the start section is now
#       forced and restored instead of assumed;
#   (c) the boot marker is compared WHOLE-FILE byte-exact instead of
#       line-wise (same accept set, no trailing-byte hole).
# The M3 arm additionally remains blocked on a DEVICE-STATE issue this
# change does not touch: the persistent play install
# /mnt/Applications/meleelight.opk carries the SAME title as the M3
# evidence OPK ("MeleeLight"), so its NAV_LINK=1 is ambiguous while both
# are installed (measured iter 73, unchanged here — the FOH arm's unique
# title is the sanctioned way around it).
#
# Rig plumbing INHERITED from port/sim/device/riglib.sh (Tier-A arc,
# VERDICT: GO): nonce-dsh, pullv, rm-before-produce+made(), shared
# stamp-cached arm build (this script's bytes are stamp input via
# RIG_SCRIPTS), rehash-adjacent-to-push, the shared no-reclaim
# device-keyed lock, the no-commit guard.
#
# Env: FUNKEY_ADB_ID (device id), MLFK_FORCE_ARM=1 (ignore build
# stamp), MLFK_OPK_FOH=1 (the M4 FOH mode above; default 0 = the M3
# gfx_device arm), MLFK_OPK_SKIP_NAV=1 (NEGATIVE TESTING ONLY: skip the
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
# M4 task 3: the committed vfx render-plane artifacts (gfx_app now
# REQUIRES --vfxdata/--glyphs; device path is browser-free — iter-72
# rule: pin the FROZEN files' shas, never a fresh capture's). The
# evidence args + the mlfk.sh live arm both consume these from the data
# dir; --legible rides the same args (the documented device-scale
# stage-legibility adaptation, gfx.h GFX_LEGIBLE_MIN_DEV_PX).
VFXDATA_FROZEN=$GFX/vfxdata-frozen.txt
VFXDATA_SHA256=545015a3d7e3bc138059fcb9711040758e729a7d21aac650b009ed7fdb5bd662
VFXGLYPHS_FROZEN=$GFX/vfxglyphs-frozen.txt
VFXGLYPHS_SHA256=8926cab4d648579d099053994bf309943b5a6bc3c5abf733af9ac6b71f3cbbeb
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

# --- mode selector (M4 gate leg 3; header section "THE FOH MODE") ------------
# Every mode-dependent name lives HERE, so the eight steps below stay
# single-copy. Default 0 = the M3 gfx_device arm, byte-for-byte the
# same commands it has always run.
OPK_FOH="${MLFK_OPK_FOH:-0}"
case "$OPK_FOH" in
  0|1) : ;;
  *) echo "DEVICE FAIL: MLFK_OPK_FOH must be 0 or 1 (got '$OPK_FOH')" >&2; exit 1 ;;
esac
FOH=port/foh
FLOWS=$FOH/flows
if [ "$OPK_FOH" = 1 ]; then
  OPK_NAME=mlfk-foh-launch.opk
  OPK_BIN=foh_device
  OPK_LAUNCHER=$OPKDIR/mlfk-foh.sh
  OPK_DESKTOP=$OPKDIR/meleelight-foh.funkey-s.desktop
  OPK_ARGS_NAME=foh-args     # mlfk-foh.sh's evidence-mode selector
  OPK_EV=$DTMP/opkfoh        # mlfk-foh.sh's FIXED evidence dir
  OPK_APPLOG=mlfk-foh.log
  OPK_APPLOG_HOST=$DEVB/opkfoh-applog.txt
  MARKER_LINE1='MLFK FOH BOOT'
  NAV_LINK=2                 # measured grid link of "MeleeLight FOH"
  FOH_FLOW_ID=f01-vs-g01
  FOH_FOHMAX=500             # bounded no-input evidence run (~8.4 s paced)
else
  OPK_NAME=mlfk-evidence.opk
  OPK_BIN=gfx_device
  OPK_LAUNCHER=$OPKDIR/mlfk.sh
  OPK_DESKTOP=$OPKDIR/meleelight.funkey-s.desktop
  OPK_ARGS_NAME=opk-args
  OPK_EV=$DTMP/opk
  OPK_APPLOG=mlfk.log
  OPK_APPLOG_HOST=$DEVB/opk-mlfk.log
  MARKER_LINE1='MLFK OPK BOOT'
  NAV_LINK=1                 # measured grid link of "MeleeLight"
fi
# The grid walk (header §3): 2 columns, `d` = +2, `r` = +1 within the
# row. Integer arithmetic, no floats — link = 2*downs + rights.
NAV_COLS=2
NAV_DOWNS=$((NAV_LINK / NAV_COLS))
NAV_RIGHTS=$((NAV_LINK % NAV_COLS))
# gmenu2x's live per-user state (NOT the shipped /usr/share copy — that
# was iter 58's mistake). The start section is anchored through it.
GM_CONF=/mnt/FunKey/.gmenu2x/gmenu2x.conf
GM_CONF_TMP=$GM_CONF.mlfk-staging   # same-directory staging (atomic rename)
GM_SECTIONS_DIR=/mnt/FunKey/.gmenu2x/sections
GM_SECTIONS_PIN='applications
emulators
games
settings'
GM_GAMES_SECTION=2           # index of `games` in the pinned listing
# Every OPK the frontend scans, EXCLUDING the one this check installs
# (measured 2026-07-26; AppleDouble `._*` companions and the SD trash
# are not frontend-visible OPKs). The frozen NAV_LINK is a function of
# this inventory — a drift here must force a re-measure, not a guess.
OPK_INVENTORY_PIN='/mnt/Applications/commander_funkey-s.opk
/mnt/Applications/meleelight.opk
/mnt/Applications/ssb64.opk
/mnt/Applications/st-sdl_funkey-s.opk
/mnt/Applications/unarchiver_funkey-s.opk
/mnt/Emulators/gamegear_mednafen_funkey-s.opk
/mnt/Emulators/gb_gnuboy_funkey-s.opk
/mnt/Emulators/gbc_gnuboy_funkey-s.opk
/mnt/Emulators/sms_picodrive_funkey-s.opk
/mnt/Libretro/gb_gbc_picoarch_funkey-s.opk
/mnt/Libretro/gba_picoarch_funkey-s.opk
/mnt/Libretro/lynx_picoarch_funkey-s.opk
/mnt/Libretro/megadrive_picoarch_funkey-s.opk
/mnt/Libretro/nes_picoarch_funkey-s.opk
/mnt/Libretro/ngp_picoarch_funkey-s.opk
/mnt/Libretro/pce_picoarch_funkey-s.opk
/mnt/Libretro/picoarch_funkey-s.opk
/mnt/Libretro/ps1_picoarch_funkey-s.opk
/mnt/Libretro/snes_picoarch_funkey-s.opk
/mnt/Libretro/wonderswan_picoarch_funkey-s.opk
/mnt/Native games/sm64_us_v1.3_funkey-s.opk
/mnt/Settings/clock_funkey-s.opk
/mnt/Settings/poweroff_funkey-s.opk
/mnt/Settings/reboot_funkey-s.opk'

source port/sim/device/adbsh.sh # (also defines $DEV — it keys the lock)
source port/sim/device/riglib.sh

rig_lock_acquire

# FRONTEND_VERIFIED — set to 1 ONLY after step 8's pkill+respawn-poll
# has VERIFIED gmenu2x running (iter 60, review-58 L1). The EXIT-trap
# frontend cycle is conditional on it: re-killing an already-verified
# frontend from the trap would end the run frontend-down UNVERIFIED —
# the exact accident the finding described.
FRONTEND_VERIFIED=0
# CONF_SAVED — 1 once the live gmenu2x conf has been backed up to the
# host and rewritten; CONF_RESTORED — 1 once step [8] put the original
# bytes back. The trap restores only when the run did not get there.
CONF_SAVED=0
CONF_RESTORED=0

# gm_conf_push <hostfile> — install <hostfile> AS the live gmenu2x conf
# TRANSACTIONALLY (review-115-1 M2): push to a sibling staging path,
# verify the STAGED bytes, and only then rename over the live conf (same
# filesystem => the live file is never a partially-written file), then
# verify the live bytes. A transport loss can therefore only ever leave
# a stray staging file, never a truncated menu conf.
gm_conf_push() {
  local src="$1" hs ds
  hs="$(rig_host_sha256 "$src")" || return 1
  adb -s "$DEV" push "$src" "$GM_CONF_TMP" >/dev/null || return 1
  ds="$(rig_dev_sha256 "$GM_CONF_TMP")" || return 1
  [ "$ds" = "$hs" ] || return 1
  dsh "mv $GM_CONF_TMP $GM_CONF" || return 1
  ds="$(rig_dev_sha256 "$GM_CONF")" || return 1
  [ "$ds" = "$hs" ] || return 1
  dsh "sync" || return 1
  return 0
}

# gm_conf_verify — the LIVE conf must equal the backup (post-respawn
# re-verification, review-115-1 M2).
gm_conf_verify() {
  local hs ds
  hs="$(rig_host_sha256 "$DEVB/gmenu2x.conf.bak")" || return 1
  ds="$(rig_dev_sha256 "$GM_CONF")" || return 1
  [ "$ds" = "$hs" ] || return 1
  return 0
}

# gm_conf_restore — put the backed-up conf back, transactionally.
# Used by step [8] (fail-loud) and by the trap (WARN-loud).
gm_conf_restore() {
  gm_conf_push "$DEVB/gmenu2x.conf.bak" || return 1
  CONF_RESTORED=1
  return 0
}

opk_cleanup() {
  local prc pn cpid old_cpid
  # own processes: per-process pkill with the rc CAPTURED and busybox
  # case-split (0 = killed, 1 = no match — nothing of ours running;
  # anything else is a real failure, loud) — never masked with `; true`
  # (iter 60, review-58 L1).
  for pn in "$OPK_BIN" fk_input; do
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
  # the evidence-mode sentinel must never outlive the check, on ANY exit
  # path (review-115-1 M1): a stray $DSD/foh-args flips a later human
  # launch of the FOH launcher out of live mode (mlfk-foh.sh:67).
  if [ "$OPK_FOH" = 1 ]; then
    if rig_dsh_retry "rm -f $DSD/$OPK_ARGS_NAME" \
       && rig_dsh_retry "test ! -f $DSD/$OPK_ARGS_NAME"; then
      : # verified gone
    else
      echo "WARN: could NOT verify removal of the evidence sentinel $DSD/$OPK_ARGS_NAME — remove it by hand (a later launcher run would start in evidence mode)" >&2
    fi
  fi
  # the human's gmenu2x start section must never stay rewritten
  if [ "$CONF_SAVED" = 1 ] && [ "$CONF_RESTORED" != 1 ]; then
    if gm_conf_restore; then
      echo "   cleanup: original gmenu2x conf restored (bytes verified)" >&2
    else
      echo "WARN: could NOT restore the original gmenu2x conf — push $DEVB/gmenu2x.conf.bak to $GM_CONF by hand" >&2
    fi
  fi
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
DESKTOP=$OPK_DESKTOP
LAUNCHER=$OPK_LAUNCHER
ICON=$OPKDIR/icon32.png
made "$DESKTOP" "$LAUNCHER" "$ICON"
# .desktop: anchored whole-file grammar — the 9 exact lines this repo
# ships (a field drift is a reviewed change) + the MANDATORY trailing
# empty line (file ends in exactly one empty line: last two bytes \n\n,
# not more — measured envelope requirement). The FOH entry differs in
# exactly three fields (Name/Comment/Exec) — and its UNIQUE Name is
# what makes the frontend navigation unambiguous (header section B).
if [ "$OPK_FOH" = 1 ]; then
  desktop_want=$'[Desktop Entry]\nName=MeleeLight FOH\nComment=MeleeLight FOH port (private build)\nExec=mlfk-foh.sh\nTerminal=false\nType=Application\nStartupNotify=true\nIcon=icon32\nCategories=games;\n\n'
else
  desktop_want=$'[Desktop Entry]\nName=MeleeLight\nComment=MeleeLight port (private build)\nExec=mlfk.sh\nTerminal=false\nType=Application\nStartupNotify=true\nIcon=icon32\nCategories=games;\n\n'
fi
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
  echo "DEVICE FAIL: $LAUNCHER does not start with #!/bin/sh" >&2
  exit 1
fi
if [ "$OPK_FOH" = 1 ]; then
  # committed FOH inputs: the packaged flow AND the frozen expectation
  # the evidence trace is judged against (both are check-device-foh.sh's
  # committed corpus — consumed here by path, never copied)
  made "$FLOWS/$FOH_FLOW_ID.flow" "$FLOWS/$FOH_FLOW_ID.expect"
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
# M4 task 3: vfxdata/vfxglyphs frozen pins (same committed-input class)
made "$VFXDATA_FROZEN" "$VFXGLYPHS_FROZEN"
vsum="$(rig_host_sha256 "$VFXDATA_FROZEN")" || exit 1
if [ "$vsum" != "$VFXDATA_SHA256" ]; then
  echo "DEVICE FAIL: $VFXDATA_FROZEN sha256 $vsum != pinned $VFXDATA_SHA256" >&2
  exit 1
fi
gsum2="$(rig_host_sha256 "$VFXGLYPHS_FROZEN")" || exit 1
if [ "$gsum2" != "$VFXGLYPHS_SHA256" ]; then
  echo "DEVICE FAIL: $VFXGLYPHS_FROZEN sha256 $gsum2 != pinned $VFXGLYPHS_SHA256" >&2
  exit 1
fi
bash pipeline/extractor/build-extractor.sh
rm -f "$TABLES/ml_tables.c" "$TABLES/ml_tables.h" \
  "$TABLES/ml_stages.c" "$TABLES/ml_stages.h" \
  "$TABLES/$ANIM_P1" "$TABLES/$ANIM_P2" \
  "$TABLES/assets/menu.img1"
node pipeline/run.js --only animations,tables,stages,assets --out "$TABLES"
made "$TABLES/ml_tables.c" "$TABLES/ml_tables.h" \
  "$TABLES/ml_stages.c" "$TABLES/ml_stages.h" \
  "$TABLES/$ANIM_P1" "$TABLES/$ANIM_P2"
# A1 restyle Phase 1 (review-b9-1-codex [H]): mlfk-foh.sh's data dir must
# QUALIFY, and qualifying now means simdata.txt AND assets/menu.img1 — the
# FOH's CSS/SSS render real artwork and foh_render's art_load is fatal
# without it. This leg wipes $DSD and provisions it from scratch, so
# without the pack here the FOH launcher would refuse its own data dir and
# exit 8 (or, worse, silently fall through to a STALE /mnt/mlfk-data).
# PROVENANCE: Nintendo-derived, private use only, gitignored build output.
made "$TABLES/assets/menu.img1"
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
rig_stamp_rehash "$OPK_BIN" fk_input
STAMP_BIN_SHA="$(rig_stamp_bin_sha "$OPK_BIN")"

echo "== [4/8] stage + mksquashfs 4.4 (SDK container ONLY) + content verification =="
STAGE=$BUILD/opk-stage
rm -rf "$STAGE" "$BUILD/$OPK_NAME" "$BUILD/opk-verify"
mkdir -p "$STAGE"
cp "$DEVB/$OPK_BIN" "$LAUNCHER" "$DESKTOP" "$ICON" "$STAGE/"
# the FOH OPK is packaged in its PLAY shape: mlfk-foh.sh's live branch
# runs `--flow $DIR/f01-vs-g01.flow` from the mount, so the flow is a
# packaged member (the evidence run reads the data-plane copy instead —
# the frontend owns the mount point, we do not).
STAGE_FILES="$OPK_BIN $(basename "$LAUNCHER") $(basename "$DESKTOP") icon32.png"
if [ "$OPK_FOH" = 1 ]; then
  cp "$FLOWS/$FOH_FLOW_ID.flow" "$STAGE/"
  STAGE_FILES="$STAGE_FILES $FOH_FLOW_ID.flow"
fi
N_STAGE_WANT="$(printf '%s\n' $STAGE_FILES | wc -l | tr -d ' ')"
chmod +x "$STAGE/$OPK_BIN" "$STAGE/$(basename "$LAUNCHER")"
# the staged binary must BE the stamped bytes (rehash was adjacent)
ssum="$(rig_host_sha256 "$STAGE/$OPK_BIN")" || exit 1
if [ "$ssum" != "$STAMP_BIN_SHA" ]; then
  echo "DEVICE FAIL: staged $OPK_BIN sha ($ssum) != stamp record ($STAMP_BIN_SHA)" >&2
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
# shellcheck disable=SC2086 — STAGE_FILES is our own space-joined list
for f in $STAGE_FILES; do
  made "$BUILD/opk-verify/$f"
  cmp "$STAGE/$f" "$BUILD/opk-verify/$f"
done
n_stage="$(find "$STAGE" -type f | wc -l | tr -d ' ')"
n_verify="$(find "$BUILD/opk-verify" -type f | wc -l | tr -d ' ')"
if [ "$n_stage" != "$N_STAGE_WANT" ] || [ "$n_verify" != "$N_STAGE_WANT" ]; then
  echo "DEVICE FAIL: OPK content count mismatch (stage $n_stage, unsquashed $n_verify, want $N_STAGE_WANT)" >&2
  exit 1
fi
OPK_SHA="$(rig_host_sha256 "$BUILD/$OPK_NAME")" || exit 1
echo "   OPK contents verified ($N_STAGE_WANT files byte-identical through unsquashfs; opk sha $OPK_SHA)"

echo "== [5/8] push: OPK -> $DEVAPPS, data plane -> $DSD, injector -> $DTMP =="
dsh "rm -rf $DTMP $DSD && mkdir -p $DTMP $DSD"
adb -s "$DEV" push "$BUILD/$OPK_NAME" "$DEVAPPS/" >/dev/null
dsum="$(rig_dev_sha256 "$DEVAPPS/$OPK_NAME")" || exit 1
if [ "$dsum" != "$OPK_SHA" ]; then
  echo "DEVICE FAIL: device-side OPK sha ($dsum) != host ($OPK_SHA)" >&2
  exit 1
fi
# NAV PRECONDITION (header §3), asserted BEFORE the launch: NAV_LINK is
# a MEASURED grid index and a grid index is a function of the frontend's
# OPK inventory. Pin it, so a human adding/removing an app produces a
# precise diagnosis here instead of a mute 60 s marker timeout.
# The DEVICE runs ONE command whose rc dsh observes (review-115-1 M4: a
# device-side `find | grep | sort` reports only sort's status, so a
# partial listing could equal the pin); all filtering and ordering is
# host-side. The exclusion is the EXACT installed path, never a
# basename suffix (review-115-1 H1: `/mnt/anywhere/$OPK_NAME` must stay
# VISIBLE — a stale same-named OPK elsewhere is exactly what this pin
# exists to surface).
apps_raw="$(dsh "find /mnt -name '*.opk'")" \
  || { echo "DEVICE FAIL: could not enumerate the frontend OPK inventory" >&2; exit 1; }
apps_now="$(printf '%s\n' "$apps_raw" | tr -d '\r' \
  | grep -v '/\._' | grep -v '^/mnt/\.Trashes/' \
  | grep -vxF "$DEVAPPS/$OPK_NAME" | grep -v '^$' | LC_ALL=C sort)"
if [ "$apps_now" != "$OPK_INVENTORY_PIN" ]; then
  echo "DEVICE FAIL: the frontend OPK inventory differs from the pin the frozen navigation was measured against." >&2
  echo "  pinned:" >&2
  printf '%s\n' "$OPK_INVENTORY_PIN" | sed 's/^/    /' >&2
  echo "  on device now (this check's own $OPK_NAME excluded):" >&2
  printf '%s\n' "$apps_now" | sed 's/^/    /' >&2
  echo "  NAV_LINK=$NAV_LINK is a MEASURED grid index (the games grid is ordered" >&2
  echo "  alphabetically by .desktop Name). Re-measure the menu and update NAV_LINK +" >&2
  echo "  OPK_INVENTORY_PIN as a reviewed change — never let this leg guess." >&2
  exit 1
fi
echo "   frontend OPK inventory == the pin the frozen nav was measured against"
adb -s "$DEV" push "$DEVB/simdata.txt" "$DEVB/g01.trace.txt" \
  "$GFXDATA_FROZEN" "$VFXDATA_FROZEN" "$VFXGLYPHS_FROZEN" \
  "$TABLES/$ANIM_P1" "$TABLES/$ANIM_P2" \
  "$BUILD/sndpack.bin" "$DSD/" >/dev/null
# artwork lives in a SUBDIR of the data dir (art_load looks for
# "$DATA/assets/menu.img1"), so it is pushed separately from the flat set.
dsh "mkdir -p $DSD/assets"
adb -s "$DEV" push "$TABLES/assets/menu.img1" "$DSD/assets/" >/dev/null
ahsum="$(rig_host_sha256 "$TABLES/assets/menu.img1")" || exit 1
adsum="$(rig_dev_sha256 "$DSD/assets/menu.img1")" || exit 1
if [ "$adsum" != "$ahsum" ]; then
  echo "DEVICE FAIL: pushed menu.img1 device sha ($adsum) != host sha ($ahsum)" >&2
  exit 1
fi
for hf in "$DEVB/simdata.txt" "$DEVB/g01.trace.txt" "$GFXDATA_FROZEN" \
          "$VFXDATA_FROZEN" "$VFXGLYPHS_FROZEN" \
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
# sha-verified — the launcher passes this line verbatim to the app.
# FOH mode: the mere EXISTENCE of $DSD/foh-args is what selects
# `mode evidence` in mlfk-foh.sh:67; every path in it is absolute
# (the frontend, not this script, owns the OPK mount point).
rm -f "$BUILD/$OPK_ARGS_NAME"
if [ "$OPK_FOH" = 1 ]; then
  adb -s "$DEV" push "$FLOWS/$FOH_FLOW_ID.flow" "$DSD/" >/dev/null
  hsum="$(rig_host_sha256 "$FLOWS/$FOH_FLOW_ID.flow")" || exit 1
  dsum="$(rig_dev_sha256 "$DSD/$FOH_FLOW_ID.flow")" || exit 1
  if [ "$dsum" != "$hsum" ]; then
    echo "DEVICE FAIL: pushed flow device sha ($dsum) != host sha ($hsum)" >&2
    exit 1
  fi
  printf '%s' "--flow $DSD/$FOH_FLOW_ID.flow --input poll --flow-out $OPK_EV/foh-trace.txt --shots-dir $OPK_EV/shots --foh-max $FOH_FOHMAX --pace 1 --budget-ns $BUDGET_NS" > "$BUILD/$OPK_ARGS_NAME"
else
  printf '%s' "--trace $DSD/g01.trace.txt --simdata $DSD/simdata.txt --gfxdata $DSD/gfxdata-frozen.txt --vfxdata $DSD/vfxdata-frozen.txt --glyphs $DSD/vfxglyphs-frozen.txt --legible --anim-dir $DSD --seed $seed --p1 $p1 --p2 $p2 --stage $stage --frames $EVID_FRAMES --pace 1 --budget-ns $BUDGET_NS --sndpack $DSD/sndpack.bin --out $DTMP/opk/opk-out.txt --timing $DTMP/opk/opk-tim.txt --shot-frame $EVID_SHOT_FRAME --shot-ppm $DTMP/opk/opk-shot.ppm --shot-pgm $DTMP/opk/opk-shot.pgm" > "$BUILD/$OPK_ARGS_NAME"
fi
made "$BUILD/$OPK_ARGS_NAME"
adb -s "$DEV" push "$BUILD/$OPK_ARGS_NAME" "$DSD/" >/dev/null
hsum="$(rig_host_sha256 "$BUILD/$OPK_ARGS_NAME")" || exit 1
dsum="$(rig_dev_sha256 "$DSD/$OPK_ARGS_NAME")" || exit 1
if [ "$dsum" != "$hsum" ]; then
  echo "DEVICE FAIL: pushed $OPK_ARGS_NAME device sha ($dsum) != host sha ($hsum)" >&2
  exit 1
fi
adb -s "$DEV" push "$DEVB/fk_input" "$DTMP/" >/dev/null
rig_push_provenance "$DTMP" fk_input
dsh "chmod +x $DTMP/fk_input"
# the FROZEN measured navigation script (iter 58 empirical; header
# "MEASURED DEVICE BEHAVIOUR": persisted section = games; n then m
# round-trips to games at LINK 0 (Mario 64), normalizing the inherited
# within-games link; r -> link 1 "MeleeLight"; a -> launch)
# The grid walk to NAV_LINK (1 = "MeleeLight" on the M3 arm, 2 =
# "MeleeLight FOH" in FOH mode): n/m normalizes the link to 0, then
# NAV_DOWNS `d` presses (a row = 2 links) and NAV_RIGHTS `r` presses
# (within the row). Guarded by the inventory pin asserted in step [5].
rm -f "$BUILD/opk-nav.script"
{
  echo "# generated by check-device-opk.sh — frozen measured gmenu2x navigation"
  printf 's 1500\n'
  for navk in n m; do
    printf 'd %s\ns 250\nu %s\ns 400\n' "$navk" "$navk"
  done
  navi=0
  while [ "$navi" -lt "$NAV_DOWNS" ]; do
    printf 'd d\ns 250\nu d\ns 400\n'
    navi=$((navi + 1))
  done
  navi=0
  while [ "$navi" -lt "$NAV_RIGHTS" ]; do
    printf 'd r\ns 250\nu r\ns 400\n'
    navi=$((navi + 1))
  done
  printf 'd a\ns 250\nu a\n'
} > "$BUILD/opk-nav.script"
made "$BUILD/opk-nav.script"
adb -s "$DEV" push "$BUILD/opk-nav.script" "$DTMP/" >/dev/null
hsum="$(rig_host_sha256 "$BUILD/opk-nav.script")" || exit 1
dsum="$(rig_dev_sha256 "$DTMP/opk-nav.script")" || exit 1
if [ "$dsum" != "$hsum" ]; then
  echo "DEVICE FAIL: pushed nav script device sha ($dsum) != host sha ($hsum)" >&2
  exit 1
fi
echo "   pushed + sha-verified (OPK, 9 data files, fk_input, nav script)"

echo "== [6/8] frontend cycle: respawn gmenu2x (deterministic start state) + inject nav =="
# PRE-RUN SYNC (M4 task 3 — the measured dirty-writeback stall class,
# see check-device-render.sh's note): flush the pushed OPK + data-plane
# bytes to SD BEFORE the frontend launch, so expiry writeback never
# lands inside the paced evidence run.
dsh "sync"
# marker must be ABSENT (frontend alive is the precondition under test)
if ! dsh "test ! -f /mnt/disable_frontend" >/dev/null 2>&1; then
  echo "DEVICE FAIL: /mnt/disable_frontend present after normalization — frontend cannot run" >&2
  exit 1
fi
dsh "rm -rf $OPK_EV" # a stale evidence dir must never satisfy a poll
if [ "$OPK_FOH" = 1 ]; then
  dsh "mkdir -p $OPK_EV/shots" # foh_device writes shots into an existing dir
fi
# --- SECTION ANCHOR (header §3; measured iter 115) ---------------------------
# gmenu2x reads its start section from the LIVE per-user conf. Back that
# conf up to the HOST (a device-tmpfs backup would not survive a reboot),
# then force `section=<games>`. Restored in step [8] and from the trap.
# `find`, NEVER `ls`, for machine-parsed enumeration (MEASURED iter 115:
# this busybox `ls` COLOURISES and multi-columns when its stdout is the
# adbd terminal rather than a pipe — moving the sort host-side changed
# exactly that context, so `ls` output stopped being one-name-per-line.
# `find` is format-stable in both). One device command, rc observed by
# dsh; ordering + basename extraction host-side (review-115-1 M4).
sects_raw="$(dsh "find $GM_SECTIONS_DIR -mindepth 1 -maxdepth 1")" \
  || { echo "DEVICE FAIL: could not list the gmenu2x sections dir $GM_SECTIONS_DIR" >&2; exit 1; }
sects_now="$(printf '%s\n' "$sects_raw" | tr -d '\r' | grep -v '^$' | sed 's|.*/||' | LC_ALL=C sort)"
if [ "$sects_now" != "$GM_SECTIONS_PIN" ]; then
  echo "DEVICE FAIL: the gmenu2x section set differs from the pin the anchor index was measured against" >&2
  echo "  pinned:     $(printf '%s' "$GM_SECTIONS_PIN" | tr '\n' ' ')" >&2
  echo "  on device:  $(printf '%s' "$sects_now" | tr '\n' ' ')" >&2
  echo "  GM_GAMES_SECTION=$GM_GAMES_SECTION is an INDEX into that ordered set — re-measure it." >&2
  exit 1
fi
rm -f "$DEVB/gmenu2x.conf.bak" "$DEVB/gmenu2x.conf.anchored"
pullv "$GM_CONF" "$DEVB/gmenu2x.conf.bak"
CONF_SAVED=1
nsec="$(grep -c '^section=' "$DEVB/gmenu2x.conf.bak")" || true
if ! [[ "$nsec" =~ ^(0|[1-9][0-9]{0,6})$ ]]; then
  echo "DEVICE FAIL: could not count '^section=' lines in the backed-up conf (got '$nsec')" >&2
  exit 1
fi
if [ "$nsec" != 1 ]; then
  echo "DEVICE FAIL: $GM_CONF carries $nsec '^section=' lines (want exactly 1 — refusing to rewrite an unrecognised conf)" >&2
  exit 1
fi
sed "s/^section=.*/section=$GM_GAMES_SECTION/" "$DEVB/gmenu2x.conf.bak" > "$DEVB/gmenu2x.conf.anchored"
made "$DEVB/gmenu2x.conf.anchored"
if ! grep -q "^section=$GM_GAMES_SECTION\$" "$DEVB/gmenu2x.conf.anchored"; then
  echo "DEVICE FAIL: the anchored conf does not carry section=$GM_GAMES_SECTION" >&2
  exit 1
fi
gm_conf_push "$DEVB/gmenu2x.conf.anchored" || {
  echo "DEVICE FAIL: could not install the anchored gmenu2x conf transactionally (staged bytes / rename / verify)" >&2
  exit 1
}
echo "   gmenu2x start section ANCHORED to games (index $GM_GAMES_SECTION); original conf backed up to $DEVB/gmenu2x.conf.bak"
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
  if dsh "test -f $OPK_EV/boot-marker" >/dev/null 2>&1; then marker_seen=1; break; fi
  sleep 2
done
if [ "$marker_seen" != 1 ]; then
  echo "DEVICE FAIL: boot marker never appeared ($((MARKER_TRIES * 2))s) — the frontend did not launch the OPK (mount failure / navigation landed elsewhere)" >&2
  exit 1
fi
pullv "$OPK_EV/boot-marker" "$DEVB/opk-boot-marker.txt"
# marker: WHOLE-FILE byte equality against the only marker this launch
# may produce (review-115-1 M3: a line-wise parse plus a `wc -l` count
# accepted arbitrary UNTERMINATED trailing bytes — wc counts newlines,
# so a 4th torn line was invisible). Every field is determined here:
# the mode's header line, the arm-build stamp sha, `mode evidence`.
rm -f "$DEVB/opk-boot-want.txt"
printf '%s\nbin %s\nmode evidence\n' "$MARKER_LINE1" "$STAMP_BIN_SHA" > "$DEVB/opk-boot-want.txt"
made "$DEVB/opk-boot-want.txt"
if ! cmp "$DEVB/opk-boot-marker.txt" "$DEVB/opk-boot-want.txt"; then
  echo "DEVICE FAIL: boot marker is not byte-identical to the expected marker" >&2
  echo "  want: $MARKER_LINE1 / bin $STAMP_BIN_SHA / mode evidence" >&2
  echo "  got:" >&2
  sed 's/^/    /' "$DEVB/opk-boot-marker.txt" >&2
  echo "  (a bin-sha difference means the frontend ran DIFFERENT bytes)" >&2
  exit 1
fi
echo "   boot marker OK (mounted-binary sha == stamp record $STAMP_BIN_SHA; mode evidence)"
rc_seen=0
for _ in $(seq 1 "$RC_TRIES"); do # §7#1 bounded foreground poll
  if dsh "test -f $OPK_EV/opk.rc" >/dev/null 2>&1; then rc_seen=1; break; fi
  sleep 2
done
if [ "$rc_seen" != 1 ]; then
  dsh "cat $OPK_EV/$OPK_APPLOG" >&2 || true
  echo "DEVICE FAIL: evidence run never finished (rc file absent after $((RC_TRIES * 2))s)" >&2
  exit 1
fi
pullv "$OPK_EV/opk.rc" "$DEVB/opk.rc"
if ! printf 'RC=0\n' | cmp -s - "$DEVB/opk.rc"; then
  dsh "cat $OPK_EV/$OPK_APPLOG" >&2 || true
  echo "DEVICE FAIL: evidence rc file is not exactly 'RC=0\\n' ($(cat "$DEVB/opk.rc"))" >&2
  exit 1
fi
pullv "$OPK_EV/$OPK_APPLOG" "$OPK_APPLOG_HOST"
if [ "$OPK_FOH" = 1 ]; then
  # ---- FOH evidence: trace + summary + tick shots (all host-judged) ----
  pullv "$OPK_EV/foh-trace.txt" "$DEVB/opkfoh-trace.txt"
  # 1. the UNCHANGED structural trace judge (FOHTRACE1 grammar, flow id,
  #    launched=0 — this evidence run must NOT have launched a match)
  node "$FOH/judge-foh-trace.js" "$DEVB/opkfoh-trace.txt" "$FOH_FLOW_ID" 0 | sed 's/^/   /'
  # 2. elision-normalized == the constructed startup->title expectation
  rm -f "$DEVB/opkfoh-trace.norm" "$DEVB/opkfoh-norm-want.txt"
  node "$FOH/normalize-foh-trace.js" "$DEVB/opkfoh-trace.txt" "$DEVB/opkfoh-trace.norm"
  made "$DEVB/opkfoh-trace.norm"
  printf 'FOHTRACE1 flow=%s\nSHOT F startup\nT F startup title timer\nSHOT F title\nEND F transitions=1\n' \
    "$FOH_FLOW_ID" > "$DEVB/opkfoh-norm-want.txt"
  if ! cmp "$DEVB/opkfoh-trace.norm" "$DEVB/opkfoh-norm-want.txt"; then
    echo "DEVICE FAIL: FOH evidence trace (normalized) != the constructed startup->title expectation" >&2
    exit 1
  fi
  # 3. BOUNDED-DELTA judgment (check-device-foh.sh's OPK-leg posture
  #    verbatim): the constructed expectation is the FROZEN
  #    f01-vs-g01.expect's own pre-input prefix + END == foh-max EXACT,
  #    with `input-free` DECLARED (no injector runs against the app in
  #    this leg — the injection drove the MENU, not the app). A
  #    startup-phase stall dies rc 3 even though elision would pass it.
  rm -f "$DEVB/opkfoh-bounded-want.txt"
  head -n 4 "$FLOWS/$FOH_FLOW_ID.expect" > "$DEVB/opkfoh-bounded-want.txt"
  printf 'END %s transitions=1\n' "$FOH_FOHMAX" >> "$DEVB/opkfoh-bounded-want.txt"
  made "$DEVB/opkfoh-bounded-want.txt"
  node "$FOH/normalize-foh-trace.js" --bounded "$DEVB/opkfoh-bounded-want.txt" \
    "$DEVB/opkfoh-trace.txt" "$FLOWS/$FOH_FLOW_ID.flow" "$FOH_FOHMAX" input-free \
    || { echo "DEVICE FAIL: FOH evidence BOUNDED-DELTA judgment failed (startup-phase stall or END != foh-max)" >&2; exit 1; }
  # 4. the app's own FOH summary, anchored. Whitelist-grammar posture
  #    (review-115-1 M3, the check-device-foh.sh parse_foh_summary rule
  #    verbatim): ANY line RESEMBLING a summary is corruption, so the
  #    `foh_dev foh:` resemblance count must be exactly 1 as well as the
  #    full-grammar count. Every counter this leg's claims rest on is a
  #    LITERAL: the measured tick length (== the pinned foh-max), one
  #    transition, two shots, zero failed presents, launched=0. Only the
  #    ungated skip counter is a capture group.
  fres="$(grep -cF 'foh_dev foh:' "$OPK_APPLOG_HOST")" || true
  if [ "$fres" != 1 ]; then
    echo "DEVICE FAIL: app log carries $fres lines containing 'foh_dev foh:' (want exactly 1 — resemblance ANYWHERE is corruption)" >&2
    exit 1
  fi
  fsum_re="^foh_dev foh: ${FOH_FOHMAX} ticks, 1 transitions, 2 shots, (0|[1-9][0-9]{0,11}) render skips, 0 failed presents, launched=0\$"
  fcnt="$(grep -Ec "$fsum_re" "$OPK_APPLOG_HOST")" || true
  if [ "$fcnt" != 1 ]; then
    echo "DEVICE FAIL: app log has $fcnt lines matching the pinned FOH summary grammar (want exactly 1: $FOH_FOHMAX ticks, 1 transition, 2 shots, 0 failed presents, launched=0)" >&2
    grep -F 'foh_dev foh:' "$OPK_APPLOG_HOST" >&2 || true
    exit 1
  fi
  fsline="$(grep -E "$fsum_re" "$OPK_APPLOG_HOST")"
  if [[ "$fsline" =~ ^foh_dev\ foh:\ ${FOH_FOHMAX}\ ticks,\ 1\ transitions,\ 2\ shots,\ (0|[1-9][0-9]{0,11})\ render\ skips, ]]; then
    ev_skips="${BASH_REMATCH[1]}"
  else
    echo "DEVICE FAIL: FOH summary line failed re-extraction ('$fsline')" >&2
    exit 1
  fi
  # 5. the in-app screenshots: exact device count, pulled, judged
  #    structurally (header section D — menu frames, so judge-shot.js's
  #    match-letterbox criteria do not apply)
  # one device command whose rc dsh observes, host-side ordering, and an
  # IDENTITY pin rather than a count (review-115-1 M4): a partial
  # listing that happens to be 2 lines long cannot pass.
  shots_raw="$(dsh "find $OPK_EV/shots -mindepth 1 -maxdepth 1")" \
    || { echo "DEVICE FAIL: cannot enumerate the device shots dir" >&2; exit 1; }
  shots_now="$(printf '%s\n' "$shots_raw" | tr -d '\r' | grep -v '^$' | sed 's|.*/||' | LC_ALL=C sort | tr '\n' ' ')"
  if [ "$shots_now" != "startup.ppm title.ppm " ]; then
    echo "DEVICE FAIL: device shots dir holds {${shots_now% }} (want exactly {startup.ppm title.ppm})" >&2
    exit 1
  fi
  for sname in startup title; do
    pullv "$OPK_EV/shots/$sname.ppm" "$DEVB/opkfoh-shot-$sname.ppm"
  done
  # STRUCTURAL FLOORS for a FLAT-PALETTE TEXT UI (measured iter 115 —
  # the FOH menu surface is not judge-shot.js's match surface: the
  # startup splash is 5 colours and the title screen 3, so a
  # distinct-colour floor of the match kind is meaningless here). What
  # IS structural: the exact PNM envelope, a frame that is not UNIFORM,
  # a foreground (non-modal-colour) coverage that is neither ~nothing
  # nor a flood, and two shots that DIFFER. A blank/cleared/garbage or
  # frozen surface fails at least one.
  node -e '
    const fs = require("fs");
    function die(m) { console.error("opkfoh-shot: " + m); process.exit(3); }
    const HDR = "P6\n240 240\n255\n";
    const NPX = 240 * 240;
    const FG_MIN = 0.005, FG_MAX = 0.60;
    const paths = process.argv.slice(1);
    const rep = paths.map((p) => {
      const b = fs.readFileSync(p);
      if (b.length !== HDR.length + NPX * 3) die(p + ": byte length " + b.length + " != header + 240*240*3");
      if (b.subarray(0, HDR.length).toString("latin1") !== HDR) die(p + ": header is not exactly P6 240 240 255");
      const m = new Map();
      for (let o = HDR.length; o < b.length; o += 3) {
        const k = (b[o] << 16) | (b[o + 1] << 8) | b[o + 2];
        m.set(k, (m.get(k) || 0) + 1);
      }
      if (m.size < 2) die(p + ": UNIFORM surface (1 colour) — nothing was composited");
      let bg = 0;
      for (const n of m.values()) if (n > bg) bg = n;
      const fg = (NPX - bg) / NPX;
      if (fg < FG_MIN) die(p + ": foreground coverage " + (fg * 100).toFixed(3) + "% < " + (FG_MIN * 100) + "% — an effectively blank frame");
      if (fg > FG_MAX) die(p + ": foreground coverage " + (fg * 100).toFixed(3) + "% > " + (FG_MAX * 100) + "% — a flooded/garbage frame");
      return m.size + " colours/" + (fg * 100).toFixed(2) + "% fg";
    });
    if (fs.readFileSync(paths[0]).equals(fs.readFileSync(paths[1]))) {
      die("the two tick shots are byte-identical — the FOH never advanced past startup");
    }
    console.log("   shots structural OK (startup " + rep[0] + ", title " + rep[1] + ", frames differ)");
  ' "$DEVB/opkfoh-shot-startup.ppm" "$DEVB/opkfoh-shot-title.ppm"
  # (tick length is a PINNED LITERAL in the summary grammar above, so it
  # is reported from the pin — review-115-2 NEW-H: the round-2 parser
  # narrowing dropped the ev_ticks capture while this line still read
  # it, which under `set -u` aborted the run before the verdict.)
  echo "   FOH evidence run: $FOH_FOHMAX ticks, 1 transition, 2 shots, skips=$ev_skips (unGated here — no quiesce bracket in this leg), presentFails=0"
else
pullv "$OPK_EV/opk-out.txt" "$DEVB/opk-out.txt"
pullv "$OPK_EV/opk-shot.ppm" "$DEVB/opk-shot.ppm"
pullv "$OPK_EV/opk-shot.pgm" "$DEVB/opk-shot.pgm"
pullv "$OPK_EV/opk-tim.txt" "$DEVB/opk-tim.txt"
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
cnt="$(grep -Ec "$sum_re" "$OPK_APPLOG_HOST")" || true
if [ "$cnt" != 1 ]; then
  echo "DEVICE FAIL: app log has $cnt lines matching the pinned evidence summary grammar (want exactly 1)" >&2
  exit 1
fi
sline="$(grep -E "$sum_re" "$OPK_APPLOG_HOST")"
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
au_cnt="$(grep -Ec '^gfx_app audio: [0-9]{1,12} callbacks, [0-9]{1,12} underruns, [0-9]{1,12} badlen, [0-9]{1,12} voice starts, [0-9]{1,12} voice stops, [0-9]{1,12} steals, rate=44100 samples=512 channels=2$' "$OPK_APPLOG_HOST")" || true
if [ "$au_cnt" != 1 ]; then
  echo "DEVICE FAIL: app log has $au_cnt device audio summary lines (want exactly 1 — audio callback liveness under the frontend launch)" >&2
  exit 1
fi
echo "   evidence run: skips=$ev_skips (unGated here) presentFails=0 wall=${ev_wall}ms; audio summary line present"
fi

echo "== [8/8] hygiene: remove the OPK, respawn a clean menu VERIFIED, no-commit guard =="
dsh "rm -f $DEVAPPS/$OPK_NAME"
dsh "test ! -f $DEVAPPS/$OPK_NAME"
if [ "$OPK_FOH" = 1 ]; then
  # the evidence-mode sentinel must never outlive the check
  # (check-device-foh.sh's review-93 M2 rule): a stray foh-args on the
  # scratch plane would flip a later launcher run out of live mode.
  dsh "rm -f $DSD/$OPK_ARGS_NAME"
  dsh "test ! -f $DSD/$OPK_ARGS_NAME"
fi
# restore the human's gmenu2x start section BEFORE the final respawn, so
# the menu we leave running is the menu they left (bytes verified).
if [ "$CONF_SAVED" = 1 ] && [ "$CONF_RESTORED" != 1 ]; then
  gm_conf_restore || {
    echo "DEVICE FAIL: could not restore the original gmenu2x conf ($GM_CONF) — refusing to print the verdict with the frontend state still rewritten" >&2
    exit 1
  }
  echo "   original gmenu2x conf restored (bytes verified)"
fi
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
# POST-RESPAWN re-verification (review-115-1 M2): the conf was restored
# BEFORE the final frontend cycle, so re-assert the live bytes AFTER it —
# a menu that rewrote its conf on the way through must not leave the
# human's start state altered by this check.
if [ "$CONF_SAVED" = 1 ]; then
  gm_conf_verify || {
    # HAND THE RESTORE BACK TO THE TRAP (review-115-2 NEW-M): the live
    # conf diverged AFTER our verified restore, so the run is no longer
    # a restored run — clear the flag or the trap's guard would skip the
    # very restoration this failure calls for.
    CONF_RESTORED=0
    echo "DEVICE FAIL: after the final frontend cycle the live gmenu2x conf no longer equals the backup — the human's menu state was altered by this run (the EXIT trap will re-restore it)" >&2
    exit 1
  }
  echo "   gmenu2x conf re-verified AFTER the final respawn (== the backed-up original)"
fi
echo "   OPK removed from $DEVAPPS (verified gone); frontend respawn-cycled and VERIFIED RUNNING (pid $fpid)"
# $OPKDIR (mlfk.sh, .desktop, icon32.png) is COMMITTED source — the OPK
# assets, tracked and freeze-pinned; only build output is guarded.
rig_no_commit_guard "$BUILD" "$DEVB" "$TABLES"

# FIXED verdict literals — every clause is asserted above (marker sha ==
# stamp, rc bytes, the mode's evidence judgment, structural shot). Each
# mode prints exactly ONE, and no progress line above shares either
# line's prefix (verify_m4.sh:451/464 applies an anchored full-line
# grammar plus a proper-prefix tear guard to the FOH one).
if [ "$OPK_FOH" = 1 ]; then
  echo "OPK FOH LAUNCH OK (frontend-launched via gmenu2x into the FOH, boot marker bin-sha == stamp, evidence rc=0, 1 transitions vs frozen, shot structural)"
else
  echo "OPK LAUNCH OK (frontend-launched via gmenu2x, boot marker bin-sha == stamp, evidence rc=0, stream prefix 900/900 vs frozen g01, shot structural)"
fi
