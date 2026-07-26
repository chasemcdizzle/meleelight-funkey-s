# PORTABILITY.md — the FunKey-S-specific surface (owner ruling 2026-07-19)

Chase's directive: FunKey-S-specific port machinery must be NOTED and
MANAGEABLE so a future port to another device is easy. This file is the
living inventory. Going forward, any writer that adds device-specific
code/behavior adds a row here in the same commit (the driver enforces
at review); the driver keeps the classification honest.

The architecture already isolates most device specifics behind seams —
the port cost for a new target is the DEVICE-BOUND column, not the sim.

## Layer 0 — device-agnostic by construction (zero port cost)

- The entire sim (`port/sim/` minus `device/`): doubles-only C,
  fdlibm-pinned, no OS calls in the frame path. Bit-exact anywhere with
  IEEE-754 + the vendored math (proven: host arm64 macOS == armv7 musl).
- The data pipeline (`pipeline/`), oracle (`oracle/`), all goldens.
- The renderer core (`port/gfx/gfx_render.c`, `raster.c`, vfx, glyphs
  consumption): draws into a 240x240 RGB565 buffer in memory. The SIZE
  and FORMAT are FunKey-tuned constants (see Layer 2).
- The AI (`port/sim/ai.c`), audio MIXER math, input INTERPRETATION.
- The FOH screen machine + font + flow rig (`port/foh/`): pure
  (state, PlatformInput) step over the platform seam + raster — the
  input FEEDER is the only per-target piece (committed flow scripts on
  host/CI, `platform_poll` on device; foh.h "INPUT SEAM" note).
- The FOH device app driver (`port/foh/foh_dev.c`): links whichever
  platform backend the target provides; its poll arm consumes ONLY the
  `PlatformInput` seam (iter 93). The menu/stage MUSIC TRACK MAP
  (menu at title->menu-top, main.js:388-390; stage->track at LAUNCH,
  main.js:1341-1360; targettest at the target LAUNCH seam, iter 99 —
  music.js:102-113) and the menu SFX mapping are upstream-faithful
  data — device-agnostic.
- The target-mode compositor (`port/gfx/gfx_target.c`) + the target
  sim plane (`port/sim/target/`): pure TTAB1-data-driven rendering/
  sim over the same raster + camera constants — device-agnostic
  (iter 99); the Layer-2 letterbox/legibility constants apply as for
  the VS renderer.
- The persistence chokepoint (`port/foh/foh_persist.{h,c}`, iter
  100): the MLFKPERSIST1 format (checksummed, hex16 bit-pattern
  doubles — strtod-free by design, so device-libc parse quirks are
  structurally out), the loud reset-to-defaults semantics, and the
  atomic tmp+fsync+rename save are device-agnostic (proven: host
  arm64 macOS file bytes == armv7 musl device bytes). Only the
  DEFAULT DIRECTORY (`/mnt/mlfk-data` — the FunKey SD data dir) and
  the dir-fsync EINVAL/ENOTSUP tolerance (FAT class) are
  target-flavored; a new target re-points the default dir
  (`MLFK_PERSIST_DIR` is the same override either way).

## Layer 1 — the platform seam (per-target TU, by design)

`port/gfx/platform.h`: `platform_init/present/poll/quit` + input struct.
Exactly ONE backend TU linked per target — this is the intended port
point (CLAUDE.md §SDL policy, ssb64 lineage):
- `platform_headless.c` — CI/loop (portable)
- `platform_sdl2.c` — host dev (portable)
- `platform_sdl1.c` — **FunKey-S**: SDL1.2, 240x240x16 SetVideoMode
  fallback chain, BitsPerPixel==16 bail, SDL_Flip, the measured
  FBIOPAN_DISPLAY error whitelist (kernel-specific!), audio open
  44100/S16LSB/2ch/512.
A new device = write one new backend TU (+ audio open params).

## Layer 2 — FunKey-tuned constants (port = re-measure + re-pin)

- Screen: 240x240, RGB565, letterbox transform k=0.2/dy=45
  (`Gfx` constants; the browser-parity IoU pins bake these).
- `GFX_LEGIBLE_MIN_DEV_PX = 2.0` (gfx.h) — stroke floor for the 1.54"
  panel; re-derive per panel size/DPI.
- Render perf budget: p99 < 16.67ms, render ≤ 8ms — PLAN §5 numbers
  measured on the V3s Cortex-A7; re-measure per SoC.
- Audio: 22050 SFX / 44100 out, 8 voices, 512 buffer — measured fit for
  this SoC (issue #12); re-measure.
- Music streaming ring: `SND_MUSIC_RING_FRAMES 32768` /
  `SND_MUSIC_CHUNK_FRAMES 16384` (snd_mixer.h — PLAN §7's 2x64 KB;
  the ring/chunk literals are also pinned into
  check-device-music.sh's summary grammar). Measured fit iter 87 on
  THIS device's SD: refill read p50/p99 0.413/1.594 ms per 64 KiB
  chunk vs a 743 ms half-ring tolerance (~466x margin); sequential SD
  read ~21.2 MiB/s. Re-measure per target/storage via the standing
  instruments: the `--music-lat` sidecar + the check's dd probe.
- Music reader-thread poll cadence: 25 ms (gfx_app.c mus_reader_main
  — >= 29 refill opportunities per half-ring drain at 22050 Hz).
  Re-derive if the ring, source rate, or storage latency class
  changes on a new target. foh_dev.c carries the same reader (iter 93).
- FOH device-flow injection timing (`port/foh/flow-to-fkscript.js`,
  iter 93): LEAD 8200 ms + 50 ms per flow frame (3x the 60 fps tick) —
  tuned to THIS device's fk_input/uinput launch latency and SDL poll
  cadence; re-measure per target input path. The BOUNDED-DELTA trace
  judgment (`normalize-foh-trace.js --bounded`, iter 95) freezes
  measured bounds over the SAME model (anchor offset 40..240 ticks,
  in-run deviation -90..+30) — re-measure + re-freeze per target.
- Input: the S1 chord table (PLAN §6) maps the FunKey's EXACT control
  set (d-pad + 8 buttons, letter keysyms u/d/l/r/a/b/x/y/s/k/n/q).
  A device with real analog or more buttons gets a NEW mapping table
  (data-driven by design — `port/gfx/s1_input.h`); the S1 semantics
  (SOCD, tap-jump-off, digital shield) are Chase-ratified for THIS
  hardware. The logical-button → letter-keysym mapping itself is the
  frozen SSOT `port/foh/keymap-frozen.txt` (iter 95): ONE file
  consumed by the flow-script generator, compiled into foh_dev
  (`--dump-keymap`), and asserted against the platform backend's poll
  table — a new target re-freezes THIS file (+ its check pins), never
  scattered per-tool tables.

## Layer 3 — device-bound machinery (rewrite per target)

- **Device libc/kernel quirk fixes** (the recurring class — 3 instances):
  FunKey SDK's static musl libm is FP-unsafe → exact floor/ceil/fmod/
  round/lround strong overrides in `port/fdlibm/fdlibm.c` (harmless
  elsewhere — KEEP for any target; the mathsweep differential instrument
  re-validates any new toolchain in one run); musl-1.2 time_t vs old
  kernel `input_event` ABI (fk_input.c 16-byte layout); FBIOPAN_DISPLAY
  whitelist (platform_sdl1.c). NEW-TARGET RULE: run mathsweep + the
  ABI-size asserts against the new toolchain FIRST (the instruments are
  the port tool).
- **The device rig** (`port/sim/device/`: adbsh nonce-RC over a broken
  adbd, riglib stamp/pullv/lock/deadman/park, the gmenu2x frontend
  nav, `low_bat_check` quiesce): ~all FunKey-OS-specific. The PATTERNS
  (identity pins, freshness, fail-closed) transfer; the bytes don't.
  `port/foh/check-device-foh.sh` (iter 93/95) is the same class: its
  leg/hygiene plumbing, fk_input handshake, and OPK evidence leg are
  FunKey-OS layout; the JUDGES it composes (frozen-trace + bounded
  cadence + byte-exact shots + fb witness + summary grammars) are the
  portable pattern. `port/sim/target/check-device-target.sh` (iter
  99) is the same class again — its target-leg plumbing is FunKey-OS
  layout; its both-verifier stream judgment + TBRIDGE-STATE witness
  are portable patterns.
  `port/sim/device/check-device-fullgame.sh` (iter 109, the M4 gate's
  leg-1 engine: 12 paced match/scenario goldens on device with live
  render + SFX mixer + SD music streaming) is the same class a third
  time. FunKey-BOUND: the per-leg `low_bat_check` quiesce window
  (stop -> launch -> restore-first -> `rig_quiesce_bracket_assert`) and
  its `/etc/init.d/S12low-bat-check` START channel; the `/mnt` SD
  scratch + `/tmp` tmpfs split the pushes and the `sync`-before-pacing
  step assume; the `/mnt/disable_frontend` park + gmenu2x kill; the
  on-device recovery deadman (a busybox-`sh` `/proc` comm scan against
  THIS init system). DEVICE-MEASURED PINS that a new target must
  RE-MEASURE and RE-FREEZE, in-script, as reviewed edits — none of
  them is a portable constant: the sustained-playback windows
  `CB_MIN/CB_MAX` (44100 Hz / 512-sample callback cadence),
  `MUSOUT_MIN/MUSOUT_MAX` and `REFILL_MIN/REFILL_MAX` (the SD music
  streamer's 32768/16384 ring+chunk geometry), `P99_FULL_LIMIT_NS` and
  `WALL_MIN_MS/WALL_MAX_MS` (this device's 60 fps pacing budget),
  `READY_TRIES`/`DEADMAN_S`, and `QW_PRE_SLACK_S/QW_POST_SLACK_S` (the
  measured ADB round-trip + daemon-restart latency). PORTABLE: the
  frozen `SFX_PINS` table and `TEETH_PIN` (sim-plane facts, not device
  facts — they change only when the sim/mixer changes), the twin-vs-
  device judgment structure, the whitelist-grammar parsers, and the
  teeth. The audio-window derivations are recorded beside the pins.
- **Present witness fb pins** (`foh_dev.c` FBWIT_*, iter 95 —
  kernel-specific, MEASURED on this kernel): fb 240x720 declared (3
  pages) but FBIOPAN_DISPLAY rejected, yoffset always 0, read() exposes
  ONLY the visible 240x240 page, content == submitted RGB565 under the
  IDENTITY transform. A new target re-runs the `--fb-witness-raw`
  probe and re-pins FBWIT_VYRES/FBWIT_LL/FBWIT_XFORM (+ the check's
  FBWIT_*_PIN copies); the instrument is the port tool.
- **OPK packaging** (mksquashfs 4.4 pin, .desktop, /mnt layout,
  launcher data-dir chain): OPK is the FunKey/gmenu2x format; another
  device has its own packaging. The FOH-generation launcher
  (`port/gfx/opk/mlfk-foh.sh` + `meleelight-foh.funkey-s.desktop`,
  iter 93) is the same class — its data-dir chain, boot-marker, and
  tmpfs conventions are FunKey OS layout.
- **PMIC stall mitigation**: `low_bat_check` is FunKey OS's daemon;
  the skip-attribution INSTRUMENT (`skip-attrib/`) is the portable
  part — run it on any new device to find ITS stall sources.
- **Power-cycle rig** (`port/foh/check-device-persist.sh`, iter 100):
  the two-session persistence proof reboots THIS device over ADB —
  the dispatch is the FunKey-measured detach recipe (`setsid sh -c
  'sleep 2; /sbin/reboot'` through nonce-dsh; a raw `adb shell "… &"`
  is killed by this old adbd's teardown before the detach takes —
  measured iter 100) and relies on the SD `adb` marker restarting
  adbd at boot (~40 s to healthy; bounded 120 s wait + an offline
  witness so a non-cycle can never pass as a cycle). IDENTITY-GRADE
  reboot witness (iter 102, review-100 H1): the offline check alone
  can be faked by a silently-failed reboot + an adbd blip, so the
  cycle is JUDGED host-side on the boot identity — PRE vs POST
  `/proc/sys/kernel/random/boot_id` (canonical UUID) with a `btime`
  from `/proc/stat` fallback (source MEASURED at runtime; BOTH exist
  on THIS kernel — boot_id is the primary), asserting POST != PRE and
  POST `/proc/uptime` < the host-measured dispatch->read gap (a fresh
  boot, not a stale one). A new target re-measures which identity
  source exists (if NEITHER, no identity-grade witness — STOP); the
  session/byte-identity judgment structure ports as-is. Directory
  durability degradation is now LOUD: a save whose `open(dir)` for the
  fsync fails emits `foh_persist: saved-nodirsync` (never a silent
  plain `saved`) — device legs assert the plain form only. The product
  save fires on the options B-exit inside the render loop (upstream's
  own cookie-write moment) — an SD-latency-sensitive target may need
  the save deferred to a loop boundary (registered class note).
- **Deadman-orphan reap** (`port/sim/device/riglib.sh`
  `rig_orphan_reap`, iter 102 — the orphaned-deadman leak class fix):
  every rig-sourcing device check now scans `/proc` for processes
  whose cmdline references the rig's device dirs (`/tmp/mlfk`,
  `/mnt/mlfk-scratch`) at BOTH the shared entry (`rig_lock_acquire`
  step-0: loud clean-then-proceed + duty transfer) and ALL exit paths
  (`rig_cleanup` teardown, before the `$DTMP` wipe that would
  otherwise lose a graceful `deadman.cancel` inside the comb's 2 s
  poll window). FunKey-OS `/proc` + `cat cmdline | tr` shapes; the
  scan-then-kill-by-pid PATTERN and the "teardown on every exit path,
  never trust a cancel racing a wipe" rule are the portable parts.

## Porting recipe (when a new target appears)

1. Toolchain audit: mathsweep + fmt corpus + ABI asserts vs the new
   libc/kernel (Layer 3 instruments). Fix by strong-override, never
   by weakening.
2. New platform backend TU (Layer 1) + audio-open params.
3. Re-measure Layer 2 constants (perf budgets, legibility, audio fit);
   new input mapping table for the control set (human ratification).
4. Rebuild the device rig for the new transport/OS (Layer 3 patterns).
5. The gate structure (conformance/perf/packaging/live-session) ports
   as-is; its evidence producers get re-pinned per PROCESS §4.
