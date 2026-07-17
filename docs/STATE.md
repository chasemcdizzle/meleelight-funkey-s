# STATE.md — current truth (driver-updated every turn)

_Read CLAUDE.md first, then this page. History → docs/AGENT-LOG.md;
queue → fix_plan.md; standards → docs/PROCESS.md._

## Live right now (updated: 2026-07-17, iter-54 writer completion)

- **Phase: M3** (issue #18) — on-device. M0/M1/M2-CAL/M2 all PASSED
  (`bash port/sim/check-sim.sh` → SIM CONFORMS, all 8 goldens bit-exact;
  re-verified iter 39).
- **Iter 38 (M3 task 1) DONE**: commit af06bb7 — `DEVICE CONFORMS g01`
  (armv7 static sim, 3600/3600 STREAM MATCH on the FunKey, 21 s wall ≈
  5.8 ms/frame sim-only avg). Class finding: SDK static musl libm is
  FP-unsafe (floor/ceil/round identity, fmod(0,0), strtod misrounding) →
  exact floor/ceil/fmod strong overrides in fdlibm.c + strtod-free
  fmt_diff --gen + standing mathsweep instrument. round/trunc also broken
  on device, zero sim call sites — extend overrides+sweep before any use.
- **Iter 39 (M3 task 1 review-hardening) DONE**: all 9 verified Codex
  round-1 findings fixed with teeth proven (stamp authenticates
  script+image+binary bytes; digest-proven pulls via pullv; strict
  mathsweep corpus parse + count pins; fail-loud manifest eval/srchash/
  git guard; RC-marker leading-newline + 0-255 validation; mkdir rig
  lock; visible-WARN cleanup; nm override assertion). H1's
  "overrides absent" claim REFUTED on record (fdlibm.c:156/174/200).
  Cold done-check `DEVICE CONFORMS g01` exit 0; logs `.loop/m3-task1r39-*`.
- **Iter 40 (M3 task 1 review-hardening ROUND 2) DONE**: all 9 triaged
  round-2 findings (`.loop/review-39-1.log`, NO-GO) class-fixed with
  teeth proven — nonce RC markers (EXIT-trap bypass now fails), no-eval
  manifest parse with strict validation, frozen CORPUS_LINES=257287 pin
  + strict mathsweep grammar, symlink-aware NUL-framed srchash,
  fail-loud nm/git guards, rehash-adjacent-to-push, docker run by Id,
  fail-closed lock (never auto-delete a pid-less lock). TOCTOU-with-
  concurrent-mutator + hostile-repo-content dispositioned in writing
  (AGENT-LOG iter 40); rig threat model ("Review bar for rig/check
  scripts") added to PROCESS.md §3 + failure mode 8 to §7. Cold
  done-check DEVICE CONFORMS g01 through BOTH rebuild and cache-HIT
  paths; logs `.loop/m3-task1r40-*`.
- **PROCESS.md amended (reservations arc resolved, post-iter-40 driver
  turn)**: Tier B never wholly skippable (+ escalation conditions);
  concrete parallel-lane trigger (5 iters / ≥20% serialized host wait /
  registered consumer); checker-succession EVIDENCE PACKAGES
  (anti-laundering); STATE recovery pointer (monotonic log ids);
  reviewed-pin FREEZE MANIFEST for gate evidence — verify_m3.sh must
  hard-refuse unreviewed evidence producers (task-7 requirement).
- **Iter 41 (M3 task 1 review-hardening ROUND 3) DONE**: round-3 triage
  (`.loop/review-40-1.log`, NO-GO) — 5 surgical fixes with teeth proven:
  shared no-reclaim rig lock at `${TMPDIR:-/tmp}/mlfk-rig-<dev>.lock`
  (mkdir-atomic, keyed by DEVICE id, zero reclamation code — existing
  lock = loud death + manual rm); corpus IDENTITY pin CORPUS_SHA256=
  b164802a…b3d05 frozen next to CORPUS_LINES (one file feeds BOTH
  sweeps); post-push device-side digest of all 4 binaries vs stamp
  ("push provenance") before anything runs; srchash find -L (symlinked
  dirs descended, broken links = loud death); count-pipeline explicit
  status + non-numeric guard. TOCTOU-with-concurrent-mutator re-raise
  RE-dispositioned pointing at the iter-40 record (fix 3 covers its
  only observable edge). Cold done-check DEVICE CONFORMS g01 exit 0,
  rebuild path; logs `.loop/m3-task1r41-*`.
- **Iter 42 (M3 task 1 review-hardening ROUND 4) DONE**: round-4 triage
  (`.loop/review-41-1.log`, NO-GO — 2 findings, ONE class: host-side
  artifact not freshness-proven) closed by a CLASS SWEEP of
  check-device-g01.sh — rm-before-produce + `made()` exists-non-empty
  assert on all 13 fixed sites (incl. the two named: wrap-run JSON
  High :534, fdlibm corpus Medium :204; plus tables/.h headers,
  simdata, trace text, host sweep binaries+outputs, fmt corpus+output,
  the 4 armv7 binaries pre-docker); pullv gains a non-empty assert
  (closes both-sides-empty cmp). Both no-op-producer teeth fired
  (stale frozen-sha-identical artifacts on disk → loud made() death;
  T-wrap after the full device run, pre-verify-stream). Class rule:
  content pins prove CONTENT, never FRESHNESS. Cold done-check DEVICE
  CONFORMS g01 exit 0 (stamp HIT; the one forced rebuild landed in the
  T-wrap probe); 3/4 run cap; logs `.loop/m3-task1r42-*`. PROCESS
  honesty note on record: writer dead-parked on a background monitor
  (failure mode #1), driver-nudged, resumed foreground.
- **Tier-A device-rig arc CLOSED**: round 5 VERDICT: GO, NO findings
  (.loop/review-42-1.log; arc summary in the driver AGENT-LOG entry).
  The rig plumbing (nonce-dsh, pullv + non-empty assert, stamp + push
  provenance, shared device-keyed lock, rm-before-produce + made()) is
  the inheritance package for every M3 device script.
- **Iter 43 (M3 task 2) DONE**: `bash port/sim/device/
  check-device-conform.sh` → DEVICE CONFORMS 8/8 + SIM P99 OK, exit 0
  (.loop/m3-task2-donecheck.log). All 8 goldens replayed ON the FunKey,
  streams judged host-side by the unchanged verify-stream.js — zero
  armv7 divergences (empty ledger; the iter-38 libc class fix held).
  MEASURED sim-only (docs/research/device-perf.md): p50 4.27-5.81 ms,
  p99 7.95-10.68 ms — worst p99 (g08) leaves ~6 ms for
  render+present+audio. sim_main gained --timing (CLOCK_MONOTONIC ns,
  RAM-buffered, post-run write; host+device); percentiles.js is the
  host-side timing judge. Rig plumbing EXTRACTED into
  port/sim/device/riglib.sh (both device scripts source it; RIG_SCRIPTS
  = every rig script's bytes are stamp input → ONE shared arm build).
  Teeth: no-timing probe (pullv death), run-side stream perturbation
  (MISMATCH at exact frame; frozen-side perturbation trips the seal
  first — run side proves the judge), 1 ms threshold probe (SIM P99
  FAIL after STREAM MATCH). Regressions green: check-sim.sh SIM
  CONFORMS + check-device-g01.sh DEVICE CONFORMS g01 via the lib.
- **Iter 44 (M3 task 3) DONE**: `bash port/gfx/check-render.sh` →
  RENDER OK, exit 0 (.loop/m3-task3-donecheck.log). Renderer core
  host-side in NEW `port/gfx/`: ANIM1 C reader (FORMATS.md §2), the
  rastbench measured raster as a module (-O3 only on that TU; explicit
  ink plane), structure-parallel stage/players/articles compositor to
  ONE 240x240 RGB565 buffer, camera = STAB1 pos*scale+offset verbatim
  (upstream has NO dynamic zoom — measured) + k=0.2/dy=45 letterbox,
  GFXDATA1 executed colour/flag dump. Silhouette IoU vs the browser
  canvas (capture-canvas.js, run-capture served-bytes class, STREAM-
  MATCH-guarded, oracle/ untouched): measured 0.9149-0.9302 over 16
  sampled frames, threshold frozen 0.91 (seed 0.90; never loosened);
  x2 byte-stable renders; render-on C replay STREAM MATCHes g01.
  Teeth: dy-perturb → all-frames FAIL; PPM instability → cmp fail;
  1e-9 sim-write → MISMATCH frame 2. Host render ~45 µs/frame avg.
  VFX excluded BOTH sides — registered deferral (ml_events seam is
  name-only; M4 seed "vfx render seam widening"). Regression green:
  check-sim.sh SIM CONFORMS 8/8.
- **Iter 45 (M3 task 2 review-hardening) DONE**: task-2 arc round-1
  findings closed — (1) MATRIX PIN: conform.sh asserts the manifest ==
  frozen {g01..g08}, unique, CPU role on exactly {g07,g08}, BEFORE any
  build/device work; 8/8 derives from the pinned set (MLFK_MANIFEST
  override = negative-testing seam, default unchanged); (2) sim_main.c
  --timing malloc guarded (frames > 10^7 or SIZE_MAX/8 wrap → sim_fatal
  before malloc); (3) perf-history PRESENCE: read-only assert that
  docs/research/device-perf.md carries a measured row per pinned golden
  (content stays writer duty; split documented in the script header).
  All three teeth fired (.loop/m3-task2r45-tooth-*.log); cold
  done-check DEVICE CONFORMS 8/8 + SIM P99 OK
  (.loop/m3-task2r45-donecheck.log); check-sim.sh SIM CONFORMS.
  PROCESS honesty: writer dead-parked on background watchers again
  (nudged 3x) + one verify-then-destroy-in-one-call incident that
  sabotaged its own live run — class lessons in the AGENT-LOG entry.
- **Iter 46 (M3 task 3 review-hardening) DONE**: task-3 arc round-1
  findings (.loop/review-44-1.log NO-GO, triage
  .loop/review-44-triage.md) closed — (1) CORPUS PIN: sampledFrameCount
  16 frozen; check-render.sh + iou.js twin pins (16 unique frames,
  asserted pre-build), capture-canvas.js rejects duplicate frames;
  (2) ORACLE BUILD DIGEST: served-bytes sha256 (43 files incl. hooked
  main.js as served) recorded in run-JSON meta, pinned as
  servedDistSha256 (measured-then-frozen; reproduced exactly by the
  done-check's independent capture), asserted before judging;
  (3) REUSE BINDING: capture.digests.json sidecar (driver bytes +
  GFXDATA) written at capture time, MLFK_GFX_REUSE_CANVAS refuses loud
  on any mismatch; (4) CONSOLE FAIL-CLOSED: frozen consoleErrorAllowlist
  (favicon 404, localforage, our own sfx/music route aborts), anything
  else kills the capture; (5) miniView pAx==0 VERDICT: upstream
  render.js:173 divides unguarded → ±Inf (NaN unreachable), C already
  mirrors it bit-exactly under IEEE no-trap (grep-verified) — comment
  documenting the assumption at the division site, NO guard (hard rule
  5). All 4 teeth fired (.loop/m3-task3r46-tooth-*.log); cold
  done-check RENDER OK exit 0 (.loop/m3-task3r46-donecheck.log),
  IOU MIN 0.9149 ≥ 0.91, both STREAM MATCH 3600/3600.
- **Iter 47 (M3 task 2 review-hardening ROUND 2) DONE**: the round-2
  Medium (.loop/review-45-triage.md) closed — the matrix pin's
  exact-set comparison no longer word-splits manifest data (unquoted
  $gids could drop a stub id:"" entry): validation now runs INSIDE
  node on the parsed JSON (count == 8, ids nonempty + ^g0[1-8]$ +
  unique, CPU role on exactly {g07 g08} via env from the shell pin),
  emitting ONE validated line the shell consumes QUOTED.
  MLFK_MANIFEST override kept for teeth. Tooth fired: 9-entry stub
  id:"" manifest copy → loud pin death before any build/device work
  (.loop/m3-task2r47-tooth-blankid.log). Cold done-check DEVICE
  CONFORMS 8/8 + SIM P99 OK (.loop/m3-task2r47-donecheck.log; one
  expected arm rebuild — script bytes are stamp input). PROCESS
  honesty: monitor-park again (nudged once; fix = foreground bounded
  until-loop) + nohup launched without rc-echo (exit 0 evidenced by
  final markers + trap-clean lock removal; wrapper pattern noted).
- **Iter 48 (M3 task 3 review-hardening ROUND 2) DONE**: both round-2
  Mediums (.loop/review-46-triage.md) closed — (1) ALLOWLIST FLOOR:
  capture-canvas.js rejects at load (pre-browser) any
  consoleErrorAllowlist textIncludes/urlIncludes with trimmed length
  < 8; sfx/music url patterns lengthened to the MEASURED "/dist/sfx/"
  + "/dist/music/" forms (368/368 measured lines, zero match-set
  change); (2) REUSE INPUT-CLOSURE BINDING: NEW
  port/gfx/capture-closure.js = the ONE mechanical enumeration (9
  members: capture-closure.js, capture-canvas.js, fdlibm.js,
  harness init.js + pagelib.js, gfx-pagelib.js, expected-render.json,
  goldens manifest.json, the g01 trace) — capture-canvas.js LOADS from
  the map, the sidecar hashes every member, reuse refuses on
  member-set or digest drift either direction (old-format sidecars
  refuse → recapture). Both teeth fired
  (.loop/m3-task3r48-tooth-{allowlist,reuse}.log, cmp-verified
  restores); cold done-check RENDER OK exit 0
  (.loop/m3-task3r48-donecheck.log), IOU MIN 0.9149 ≥ 0.91, both
  STREAM MATCH 3600/3600. Class: bind the input CLOSURE — read-side
  twin of iter-42's write-site enumeration.
- **Iter 49 (M3 task 3 review-hardening ROUND 3) DONE**: the round-3
  single finding (.loop/review-48-1.log — closure hashes computed only
  AFTER replay; an editor save mid-capture binds NEW bytes to masks
  from OLD bytes) closed: capture-canvas.js snapshots sha256 of all 9
  closure members BEFORE any member is consumed (CLOSURE_SNAP), and at
  sidecar-write time re-hashes + verifies each equals its snapshot —
  drift → loud death naming the member, NO sidecar; the sidecar records
  the snapshot (consumed-bytes) hashes. Dispositioned-class variant
  taken under PROCESS §3's trivial-whole-class exception. Tooth fired
  (.loop/m3-task3r49-tooth-midrun.log: mid-capture whitespace append
  to expected-render.json → "closure member changed MID-CAPTURE", rc 1,
  sidecar absent; restore cmp-verified). Cold done-check RENDER OK
  exit 0 (.loop/m3-task3r49-donecheck.log), IOU MIN 0.9149 ≥ 0.91,
  both STREAM MATCH 3600/3600. Task-3 Tier-A arc → CAPPED-CLOSED by
  the driver.
- **Task-2 AND task-3 Tier-A arcs CLOSED** (driver, post-iter-49):
  task-2 GO at round 3; task-3 CAPPED-CLOSED at round 3 (named class:
  TOCTOU re-raises; trivial fix taken iter 49). Rig inheritance packages:
  riglib.sh + capture-closure.js.
- **Iter 50 (M3 task 4) DONE**: `bash port/gfx/check-device-render.sh`
  → DEVICE RENDER OK (full p99 10.743 ms, render-only p99 2.568 ms, sim
  p99 7.527 ms, present p99 1.479 ms, skips 0/3600), exit 0
  (.loop/m3-task4-donecheck.log) — FIRST PIXELS ON THE FUNKEY'S SCREEN:
  g01 replayed live, paced 60 fps, SDL1.2 240x240x16 (chain step 0,
  RGB565 verified), STREAM MATCH 3600/3600 with render+present live,
  screenshot (own fb, f900) structurally judged + BIT-IDENTICAL to the
  host render. Three-backend platform seam shipped (port/gfx/platform.h
  + headless/sdl2/sdl1 TUs, ONE per binary; PlatformInput carries the
  FunKey letter-keysym map for task 5). gfx_device joined the shared
  rig build (ARMBINS + port/gfx srchash + RIG_SCRIPTS — stamp now
  335a0f1a…). GFXDATA staged as committed gfxdata-frozen.txt
  (sha-pinned; check-render.sh cmp tripwire). Class fixes: device-libm
  FLOAT plane pre-empted (integer floor/ceil helpers + fdlibm trig →
  render cross-platform deterministic; mathsweep +sqrtf/fabsf columns,
  healthy); per-script push provenance (G01_BINS); pkill -f self-match.
  All 4 teeth fired (.loop/m3-task4-tooth-*.log + the standing in-check
  valve tooth). Regressions green: check-sim SIM CONFORMS, check-render
  RENDER OK (IoU min unchanged 0.9149 with fdlibm-routed trig),
  check-device-g01 DEVICE CONFORMS g01. Perf: ~5.9 ms p99 headroom left
  for task-6 audio (docs/research/device-perf.md iter-50 table).
- **Iter 51 (M3 task 5) DONE**: `bash port/gfx/check-device-input.sh` →
  S1 INPUT OK, exit 0 (.loop/m3-task5-donecheck.log) — the S1 input
  layer is live at the pollInputs seam. port/gfx/s1_input.h =
  data-driven PLAN §6 chord table (header-only; task-4 TU lists
  untouched); s1_sweep asserts the 15 pinned chord checks bit-exact +
  2048-combo dump ×2 + 1/80-grid closure. gfx_app --live records the
  golden-trace JSON via ml_sb_num String(x) (round-trip bit-exact,
  proven host-side before the device leg); --tapjump-off-p1 on gfx_app
  + sim_main (S1 contract; default paths unchanged, flag proven live).
  port/tools/fk_input (own uinput device, ssb64 pattern) played the
  committed 1080-frame s1-session.script through the REAL SDL keysym
  path on the FunKey: coverage 24/24 signatures (chords held 15-18
  frames as designed), FOUR-way byte-identical checksum streams (host
  ×2 + device replay + the live stream), non-vacuity leg green. CLASS
  FINDING (fixed + logged): musl-1.2 64-bit time_t vs the old kernel's
  16-byte input_event ABI — short write errno 0; fk_input emits the
  kernel's 32-bit layout explicitly (iter-38 class extended to
  kernel-struct timestamp ABIs). Instrument exposure: the resolver
  quantizer absorbs sub-half-grid-step table perturbations (tooth
  round 1 measured it; round 2 fired at a full step). riglib.sh grew
  per its own contract (RIG_SCRIPTS/ARMBINS/srchash roots).
- **Iter 52 (M3 task 4 hardening + rig-wide whitelist-grammar parser
  audit) DONE**: all 8 triaged review-50 findings closed (deadman-
  guarded park with nonce disarm + cancel-on-success proven every run;
  detached setsid/rc-file launch; pessimistic PARKED; skip gate
  0/3600; PPM+PGM bit-compare GATING; pkill rcs captured;
  platform_present success channel; wall window [58,66] s; strict
  valve grammar) — cold done-check DEVICE RENDER OK exit 0
  (.loop/m3-task4r52-donecheck.log). REAL FINDING (the new M2 gate's
  first run caught it): the FunKey kernel rejects FBIOPAN_DISPLAY, so
  the patched libSDL's SDL_Flip returns -1 with
  `ioctl(FBIOPAN_DISPLAY) failed` on EVERY frame while presents run —
  platform_sdl1 pins that exact measured-benign signature (whitelist
  posture on a C API; probe .loop/m3-task4r52-probe-sdlflip.log).
  PARSER AUDIT: 42 sites enumerated / 24 conformant / 13 converted
  (NEW shared rig_dev_sha256 + rig_stamp_bin_sha + parse_app_summary;
  require_device full-line; conform perf-row end anchor; check-render
  strict gparams + render-only grammar; input.sh apprc/sweep/sha
  conversions) / 2 tightened / 2 not-converted with reasons — full
  list + corpus citations in AGENT-LOG iter 52. 39 teeth fired
  (32 host + 7 device-probe incl. deadman FIRE under real transport
  death). Regressions ALL green: check-render RENDER OK, check-sim
  SIM CONFORMS, device g01 + conform 8/8 + input S1 INPUT OK.
- **Iter 53 (M3 task 5 hardening) DONE**: all 8 triaged review-51
  round-1 findings closed — cold done-check `S1 INPUT OK` exit 0,
  FIRST run (.loop/m3-task5r53-donecheck.log). H1 strict full-stream
  validator on all 5 streams (F 1..1080 contiguous + RNG + SIM OK,
  gate-fatal); M1 fk_input anchored full-line script grammar + ferror
  (the `d l s 250` joined line dies rc 2 pre-injection, device-probed);
  M2 raw-line whitelist grammar before JSON.parse in the coverage
  judge (duplicate-key class dead; discriminating pair vs the old
  judge proven); M3 sweep exactly-one-verdict-line + byte-exact apprc
  cmp; M4 SOCD LIVE WITNESS — gfx_device --live records a raw-keysym
  bitmask sidecar (mandatory --record-keys), judge asserts pairing
  fidelity + universal SOCD invariants + 2 new signatures: the real
  session carried 18/18 SOCD H/V frames through the actual uinput→SDL
  path (iter-51's registered exposure closed); M5 standing tapjump
  behavioral oracle (host differential, measured-then-frozen
  divergence at frame 121); M6 pessimistic PARKED + rig_dsh_retry
  cleanup; L1 clayer-diag requires ls neutral. 11 host teeth + 4
  device-probe teeth fired (.loop/m3-task5r53-teeth-{host,fkinput}.log);
  zero false rejections on every genuine corpus. Regressions recorded:
  check-sim/check-device-render NOT run (no sim TU; riglib untouched)
  — gfx_app --trace guarded by host smoke (F1 == the g01 anchor;
  .loop/m3-task5r53-smoke-host.log).
- **Iter 54 (M3 task 4 hardening ROUND 2) DONE**: all 5 triaged
  review-52 round-2 findings closed (.loop/review-52-triage.md) — H
  deadman disarm ordering (RC-checked marker-gone VERIFICATION gates
  both disarm channels; rig_cleanup RIG_PRESERVE_DTMP=1 keeps the nonce
  alive on unverified restore — deadman probe: nonce survived cleanup,
  fired in-window, marker removed, gmenu2x respawned); M1 FRAMES_PIN=
  3600 literal (manifest cross-asserted at the pin; gate asserts the
  LITERAL); M2 timing-judge duplicate-key = corruption death + 8-key
  presence; M3 digit bounds on every numeric grammar in both files
  BEFORE bash arithmetic (status-2-as-false hole closed); L
  rig_stamp_ok strict WHOLE-FILE grammar (any extra/malformed line =
  MISS). Cold done-check DEVICE RENDER OK exit 0, skips 0/3600
  (.loop/m3-task4r54-donecheck.log; 2/2 paced cap — attempt 1's 1-skip
  fail was a GENUINE transient the H3 gate correctly caught, frame-1190
  sim spike 13.02 ms). Teeth: 9 stamp-grammar + M1/M2/M3a-c host teeth
  + the device deadman probe, zero false rejections on genuine corpus.
  Regressions ALL green: DEVICE CONFORMS g01 · DEVICE CONFORMS 8/8 +
  SIM P99 OK · S1 INPUT OK (.loop/m3-task4r54-reg-*.log).
- **In flight**: task-4 arc ROUND 3 = closure review pending (iter 54
  closed round 2; review-50's untriaged Low — gfx_app getline/ferror —
  still open for its disposition); task-5 arc ROUND 2 pending (closure
  review of iter 53). Then: both arcs to GO → task 6 (audio-on) →
  task 7 (OPK + verify_m3.sh gate).
- **Latest AGENT-LOG entry**: iter 54 (M3 task 4 hardening round 2);
  latest log id: .loop/m3-task4r54-donecheck.log.
- **Device**: FunKey-S on ADB, id 12c00003237f5528, healthy. adbd drops
  exit codes → RC-echo via port/sim/device/adbsh.sh. /tmp tmpfs 128 MB;
  big artifacts → /mnt/mlfk-scratch; ADB pulls ~4.4 MB/s (budget pull
  time). Arm build stamp-cached (`MLFK_FORCE_ARM=1` forces).
- **Branch**: agent/auto, clean between iterations (iter-39 hardening
  commit on top of af06bb7 + process-docs commit). Origin only.
- **Loop**: dynamic self-paced driver; ~20-30 min heartbeat; writer
  completion notifications are the primary wake signal.

## Next

1. Driver: ground-truth iter 51 (cold
   `bash port/gfx/check-device-input.sh`) + fold the task-5 surface
   (check-device-input.sh, s1_input.h, s1_sweep.c, fk_input.c,
   judge-s1-coverage.js, s1-session.script, the gfx_app.c/sim_main.c
   flags, the riglib.sh roster/roots additions) into the running
   task-4/rig Tier-A review arc → launch task-6 writer (audio-on) on
   GO.
2. Ladder: fix_plan §M3 tasks 6-7 → M3 gate (`verify_m3.sh`) →
   LOOP STOP: m3-device + push-notify Chase for S1 ratification →
   close #18 → M4 REPLAN (PLAN §4/M4) → Chase acceptance →
   LOOP STOP: m4-complete.

## Rulings (standing owner directives)

- 2026-07-14 — Full autonomy: all judgment calls delegated; run until M4
  done; push-notify at milestone gates; stop for M4-complete, physical
  blockers, or usage-limit deaths (notify /login and stop).
- 2026-07-14 — ZOOM OUT is HARD RULE 8: class fix > one-off; zoom-out
  note ends every root-cause session.
- 2026-07-16 — Adopt brawlback PROCESS-EXPORT standards per
  docs/PROCESS.md (tiered Codex review arcs, pre-registration, STATE.md,
  artifact identity pins, ground-truth ritual); the four explicit
  non-adoptions + reopen conditions live in PROCESS.md's final section.
- 2026-07-16 — Whitelist-grammar rule (brawlback provenance): decision-
  bearing parsers = anchored empirical grammars, fail closed, corpus-
  validated; binding form PROCESS.md §3.
- Standing: never push to upstream/schmooblidon; origin only; no
  distribution of anything; writers never post to GitHub (driver owns
  tracker writes).
