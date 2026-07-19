# STATE.md — current truth (driver-updated every turn)

_Read CLAUDE.md first, then this page. History → docs/AGENT-LOG.md;
queue → fix_plan.md; standards → docs/PROCESS.md._

## Live right now (updated: 2026-07-19, iter 79 — ai-rig arc round-2 residuals CLOSED; ai-rig round 3 = closure-or-cap)

- **Iter 79 (M4 hardening — ai-rig arc round-2 residuals, 2 Mediums)
  DONE** (full entry: AGENT-LOG iter 79 pre-registration + result;
  driver triage .loop/review-77-triage.md; RESPAWN iteration — the
  first writer died on a usage limit mid-teeth, this session reviewed
  + adopted its in-tree edits then ran all evidence fresh): M1 ERANGE
  guard (errno reset + `errno == ERANGE` in the fail condition) on the
  replay_ai_port.c frame strtol AND its class siblings (parse_expect
  counts, --cover-gate, --ncov-pin) — an overflowing decimal is now
  corruption death, never LONG_MAX saturation; M2 the coverage-table
  UNIVERSE is pinned: expected-capture-aiport.json `coverage`
  {ncov 64, liveArms 61, deadArms by NAME} + check-ai-replay.sh's
  frozen NCOV_FROZEN=64 twin (asserted before the lock) feeding
  --cover-gate/--ncov-pin/--dead-pin, replay asserts ncov-pin ==
  compiled ML_AI_NCOV + dead-pin names == compiled g_dead_arms
  (bijection); a grown named-but-unhit arm can no longer pass in
  lockstep. Teeth 6/6 with asserted death classes
  (.loop/m4-airig79-teeth.log): T5 positive control 0 divergences;
  T1 the reviewer's exact overflowing-frame probe → `malformed frame
  field` exit 3; T2 overflow --expect → EXPECT PARSE FAIL; T3a the
  reviewer's exact accident DEMONSTRATED (NCOV-65+PROBE_NEW_ARM probe
  binary PASSES under old flags) → T3b same probe dies under
  --ncov-pin 64 (COVERAGE PIN FAIL exit 2); T4 wrong dead-pin name →
  DEAD-ARM PIN FAIL exit 2. Cold check GREEN first attempt: `AI MATCH`
  exit 0 (.loop/m4-airig79-donecheck.log; run cap held, 1 composed
  run). No device surfaces touched.
  **Driver next: ai-rig arc ROUND 3 = closure-or-cap (grammar-variant
  re-raises → cap naming the class, per the triage ruling); device-rig
  round 3 runs concurrently (iter 78).**

## [superseded by iter 79] (updated: 2026-07-19, iter 78 — device-rig arc round-2 residuals CLOSED; device-rig round 3 = closure-or-cap)

- **Iter 78 (M4 hardening — device-rig arc round-2 residuals, 4
  Mediums) DONE** (full entry: AGENT-LOG iter 78 pre-registration +
  result; driver triage .loop/review-76-triage.md): M1 the quiesce
  window is now EXACT in both scripts — daemon stop is the last
  pre-launch action, restore is the FIRST device action after
  app-exit detection (ahead of rc pull/hash/cmp chores), with a
  STANDING device-clock bracket tooth (riglib
  rig_quiesce_bracket_assert over qstop/app.start/app.end/qrestore
  stamps, 10 s slacks; fired live on the cold render gate:
  stop->start 0 s, app 60 s, end->restore 2 s); M2 parseStat
  requires ALL NINE /proc/stat fixed-table line classes (clipped
  snapshot = corruption); M3 the verdict needle is suffix-free
  (`^SKIP ATTRIB VERDICT: \((a|b|c)\)$`, detail on a separate
  non-gating line; canonical AGENT-LOG line rewritten through the
  designed replacement channel); M4 twin-pin + argv EXACTNESS (new
  riglib rig_pin_assert_once / rig_argv_assert_once: exactly-one
  assignment per pinned var in BOTH scripts' bytes, all 20 gfx_device
  options exactly once in the extracted launcher region). Cold checks
  GREEN first attempt: `DEVICE RENDER OK ... skips 0/3600`
  (.loop/m4-rig78-donecheck.log, the expected one arm rebuild) +
  `SKIP ATTRIB OK (arm=sampler, skips=1/3600, events=50, stream
  MATCH)` (.loop/m4-rig78-donecheck2.log, stamp HIT). Teeth 21/21
  with asserted death classes (.loop/m4-rig78-teeth.log) incl. the
  reviewer's exact ' — superseded' and duplicate-SHOT_FRAME probes;
  manifest re-pins (riglib + check-device-render, cite iter78) +
  anchor same commit, self-check 23/23 + ANCHOR GREEN
  (.loop/m4-rig78-manifest-selfcheck.log). Run cap held (2 paced
  runs); device left clean (lbc ==1, no marker, scratch wiped,
  gmenu2x live).
  **Driver next: device-rig arc ROUND 3 = closure-or-cap
  (grammar-variant re-raises → cap naming the class, per the triage
  ruling); ai-rig round 2 runs concurrently (iter 77).**

## [superseded by iter 78] (updated: 2026-07-19, iter 77 — ai-rig arc round-1 findings CLOSED; ai-rig round 2 = closure pending)

- **Iter 77 (M4 hardening — task-4 ai-rig arc round-1 closure) DONE**
  (full entry: AGENT-LOG iter 77 pre-registration + result): ALL
  triage items closed (.loop/review-75-triage.md; review
  .loop/review-75-1.log r1 NO-GO) + the two round-1 findings outside
  the M-list (stale-capture High, run lock) closed with the standard
  classes. Highlights: spec-aiport.js recon bookkeeping allowlist is
  now PER-SLOT and the post `bk` is the FOUR-SLOT array (foreign-slot
  bookkeeping writes = wsViol in-page AND divergence in C; captures
  re-recorded by the check, pins counts invariant); replay gained
  --expect strict-grammar record inventory (REQUIRED under --strict;
  ferror-checked — truncation/read error = corruption death),
  representability guards before every captured-data cast (+ a
  SIBLING found by the audit: the frame field's NULL-endptr strtol,
  now full-token validated), ledgePos-empty rule-7 death, and
  --cover-gate 61 (live arms pinned per golden; the 3 documented-dead
  arms pinned ZERO); check-ai-replay.sh gained corpus inventory pin,
  no-reclaim run lock, freshness contract, rc-case-split hygiene
  guard; ai.c H_LEDGE_CTA comment now cites T4a/T4b (T4 refuted).
  Cold check GREEN: `AI MATCH` (.loop/m4-airig77-donecheck.log, 4×
  STREAM MATCH, 0 divergences, 61 live arms per golden); 12 teeth
  fired with asserted death CLASSES incl. the reviewer's three exact
  probes now dying (.loop/m4-airig77-teeth.log). check-sim skip
  justified mechanically (ai.o byte-identical HEAD vs worktree);
  bridge surface untouched (git status 0 lines). Composed runs 2/2,
  probe captures 1/2.
  **Driver next: ai-rig arc ROUND 2 (closure re-review of the iter-77
  surfaces); device-rig arc round 2 runs concurrently (iter 76).**

## [superseded by iter 77] (updated: 2026-07-19, iter 76 — device-rig arc round-1 findings CLOSED; rig arc round 2 = closure pending)

- **Iter 76 (M4 hardening — iters-73/74 device-rig arc round-1
  closure) DONE** (full entry: AGENT-LOG iter 76 pre-registration +
  canonical-needle section + result): ALL triage items closed
  (.loop/review-73-triage.md; review .loop/review-73-1.log r1 NO-GO).
  Highlights: **H** — the on-device deadman now backstops the daemon
  quiesce too (nonce-scoped qd markers, comm-scan-guarded hard-coded
  restore arms, marker cleared only on live rescan) and the quiesce
  window narrowed to exactly the paced run; TOOTH fired with real
  transport-death evidence (host SIGKILL + adb kill-server mid-paced
  -run → deadman restored BOTH frontend AND low_bat_check,
  .loop/m4-rig76-probe-h.log). NEW riglib `rig_qd_normalize` step-0
  chokepoint (stale markers restored before any disarm/wipe);
  rig_daemon_restore idempotent + exact-cardinality (device-probed
  6/6 incl. n=2 refusals); skip-attrib degraded-mode lockout
  (readonly SKA_AUTHORITATIVE, DEV banner + exit 3, OK sentinel
  structurally unreachable); full-line verdict-needle grammar +
  resemblance counter (canonical line appended to AGENT-LOG); NEW
  validate-ev.js (full EV whitelist grammar, 108-line corpus 0 false
  rejections, skip/event reconciliation, sampler arms require kernel
  windows on EVERY event — win=none fails closed); correlator
  validates ALL sampler payloads + full-line /proc/stat + vmstat
  dup/required-key grammars; rc files judged by exact bytes RC=0\n;
  skip-attrib runs the exact task-3 workload (sha twin-pins + full
  argv + judged shot); render check gained the RENDER_OK fail-closed
  exit guard. Cold checks GREEN: `DEVICE RENDER OK ... skips 0/3600`
  (.loop/m4-rig76-donecheck.log) + `SKIP ATTRIB OK (arm=sampler,
  skips=1/3600, events=42, stream MATCH)`
  (.loop/m4-rig76-donecheck2.log); 18/18 host teeth
  (.loop/m4-rig76-teeth.log); manifest re-pins (riglib +
  check-device-render, arc-pending cite iter76) + anchor + 23/23
  self-check (.loop/m4-rig76-manifest-selfcheck.log). Device left
  clean (lbc ==1, no marker, scratch wiped, gmenu2x live). Paced
  runs: 2 cold + 1 partial H-probe; one arm rebuild (stamp).
  **Driver next: rig arc ROUND 2 (closure re-review of the iter-76
  surfaces); task-4's ai.c arc still pending separately.** Low
  disposition on record: fdlibm lround boundary sweep registered for
  the next fdlibm-touching iteration.

## [superseded by iter 76] (updated: 2026-07-19, iter 75 — M4 task 4 DONE: ai.js C port, AI MATCH)

- **Iter 75 (M4 task 4 — ai.js structure-parallel C port) DONE** (full
  entry: AGENT-LOG iter 75 pre-registration + result): cold done-check
  `bash port/sim/calib/check-ai-replay.sh` → `AI MATCH` exit 0
  (.loop/m4-task4-donecheck.log). port/sim/ai.{h,c} (22 fns over
  MlAiSim; tagged bank writes, curentAction typo as slice state, fdlibm
  math, quirks q1-q8 verbatim) verified by the NEW aiport capture spec
  (runAI-only, pre read-set projection + M2-parallel post, wsViol==0,
  153-preset sweep) — 0 divergences over 7571 records on g07+g08,
  ZERO divergence-driven fix rounds. Honest coverage measured via the
  --cover arm instrument: 61/64 arms per golden; 3 zero-hit + 5 more
  surfaces measured-DEAD upstream (FORMAT.md "The aiport spec" lists
  them — incl. the ai.js:1254 curentAction write, dead by the
  :228/:1253 contradiction; T4 tooth amended accordingly, recorded).
  Teeth all fired (.loop/m4-task4-teeth.log): nibble→1, draw-drop→1104
  cascade, bk-drop→3/1, cta-serializer→3663, q2-typo→1. Regressions
  green: SIM CONFORMS + AI BRIDGE OK (AIBRIDGE1/check-sim.sh
  byte-untouched — task 5 retires the bridge from the live path).
  Task-5 handoff notes in the AGENT-LOG entry (MlAiSim population,
  bank-row alias/slot-0 re-copy stays caller's job, same seeded stream,
  rule-16 re-survey on new goldens). Device untouched this iteration
  (host-only task; iters 73-74 device-rig Codex arc unaffected).

## [superseded by iter 75] (updated: 2026-07-19, tasks 3+8 done): stall class ATTRIBUTED + mitigated; task 3 DONE)

- **Iter 74 (M4 task 8 — skip-stall attribution instrument,
  driver-re-ordered forward) DONE; M4 task 3 UNBLOCKED → DONE** (full
  entries: AGENT-LOG iter 74 pre-registration + result + addendum):
  **SKIP ATTRIB VERDICT: (a)** — the external stall class =
  `low_bat_check` (FunKey OS battery poller: 2 s shell loop, ~8
  busybox forks + blocking AXP20x i2c sysfs reads per wake; event
  comb every ~123 frames ≈ 2.05 s, phase-random per run — the
  "1100-1500 zone" was phase+load illusion). Matrix: live arms 2-3
  skips/33-34 events per 3600 paced frames; quiesce arms ×2 = 0
  skips, comb gone. Instrument committed (Tier-B diagnostic):
  gfx_app --attrib (per-frame mono/raw + rusage rows), sk_sampler.c
  (fork-free 250 ms /proc counter snapshots), correlate-skips.js
  (whitelist grammars + attrib_complete terminator), pre/post kernel
  + per-pid snapshots; `bash port/sim/device/check-skip-attrib.sh` →
  `SKIP ATTRIB OK` exit 0 cold (.loop/m4-task8-donecheck.log; arms
  nosampler/sampler/quiesce via MLFK_SKATTRIB_ARM). MITIGATION in
  check-device-render.sh: low_bat_check quiesced for the paced window
  (riglib rig_comm_pids/rig_daemon_stop/rig_daemon_restore —
  comm-scan kill-by-pid; busybox start-stop-daemon -K -x is a
  measured NO-OP for script daemons and pidof is comm-blind), restore
  hard-gated + trap-covered; skips==0 gate UNWEAKENED. The unblocked
  task-3 rerun then hit the FIRST-ever-reached host<->device shot
  bit-compare and exposed the SECOND iter-38-class instance: device
  musl lround shifted HUD glyph anchors 1 px — fixed with fdlibm.c
  exact round/lround STRONG overrides + mathsweep rr/lr columns + nm
  assertion (floor ceil fmod round lround). **Task 3 cold done-check
  GREEN: `DEVICE RENDER OK (full p99 12.777 ms, render-only p99
  5.598 ms, sim p99 7.429 ms, present p99 1.400 ms, skips 0/3600)`
  exit 0 (.loop/m4-task8-task3-donecheck.log; shot BIT-IDENTICAL
  PPM+PGM; p99 recovered ~2.7 ms vs the blocked attempts).**
  Regressions ALL green: CROSSCHECK OK, SIM CONFORMS 8/8, RENDER OK
  (IoU 0.9059, no pin moved), DEVICE CONFORMS g01, DEVICE CONFORMS
  8/8 + SIM P99 OK (.loop/m4-task8-{host,device}-regressions.log).
  Manifest: riglib.sh + check-device-render.sh re-pinned arc-pending
  (cite iter74) + anchor; SELF-CHECK 23/23 + ANCHOR GREEN. Driver
  notes: (1) OPK PLAY path keeps low_bat_check live by design (valve
  absorbs it; Chase's ratified playtest ran with it); task-14
  verify_m4 assembly decides whether with-audio legs adopt the
  quiesce (machinery is shared riglib); (2) check-device-audio.sh /
  check-device-opk.sh paced legs don't yet carry the mitigation;
  (3) Tier-B review round for the new instrument surfaces + the
  fdlibm round/lround addition is driver-queueable; (4) 9/12 paced
  runs consumed, device left clean (daemons verified restored, play
  install untouched).

## [superseded by iter 74] (updated: 2026-07-18, iter 73 — M4 task 3 landed+verified, done-check BLOCKED on the external stall class)

- **Iter 73 (M4 task 3 — stage legibility + device render rung)
  LANDED + VERIFIED, done-check BLOCKED (honest report; full entry =
  AGENT-LOG iter 73)**: legibility = device-only `--legible`
  (GFX_LEGIBLE_MIN_DEV_PX 2.0 device px, gfx.h rationale, twin-pinned,
  standing in-check no-legible-differs witness; host IoU path
  byte-untouched); --vfxdata/--glyphs threaded through
  check-device-render.sh / check-device-opk.sh / mlfk.sh with the
  COMMITTED frozen files' sha pins (iter-72 rule); arm build + host
  backends gain the vfx render TUs; judge-shot criterion-5 retired
  (reviewed pin change — ink-suppressed bg art); arm-gcc-10.2 -Werror
  class fixes (hit_detection.h noreturn decl — SIM CONFORMS 8/8 rerun
  green; overlay buffer); dv_start countdown clamped to the atlas
  domain (valve-tooth-found C-only skip-desync state); bit-identical
  render optimization round measured with the NEW -DMLFK_RENDER_PROF
  per-pass profiler (batch blend prims + rast_fill cov-window +
  unit-circle table): device render p99 13.20 -> 7.06 ms, full p99
  20.79 -> 15.51 (all budgets MET; trail in device-perf.md iter-73).
  GREEN: host RENDER OK (IoU min 0.9041, no pin moved), STREAM MATCH
  every device attempt, device shot BIT-IDENTICAL to host, teeth all
  fired, manifest re-pin 5 producers arc-pending + anchor SELF-CHECK
  23/23 GREEN. **BLOCKED: skips==0 unreachable today — 8/8 paced
  attempts carried 1-3 skips from isolated ~7-15 ms EXTERNAL kernel
  stalls (frames ~1118-1290 zone, the iters-54/59 zone); adbd-poll /
  writeback / rig-machinery / swap / DVFS / fresh-boot all REFUTED by
  isolation probes. skips==0 unweakened; task-8 attribution instrument
  = closure path; do NOT retry blind (driver: one cold retry on fresh
  device state is legitimate new evidence).** ALSO REGISTERED:
  check-device-opk.sh's frozen nav is stale (post-M3 inventory: the
  persistent meleelight.opk + 4 other OPKs; two same-title entries) —
  failed loud as designed, needs re-measured nav + unique evidence-OPK
  title; check-device-audio.sh still unthreaded (task 6/14). Device
  left clean, play install untouched.

## [superseded by iter 73] (updated: 2026-07-18, iter 72 — glyph-jitter class fix: measured glyph comparison)

- **Iter 72 (M4 task 2 micro-iteration — glyph-jitter class fix) DONE**:
  the driver's cold-r71 finding (committed vfxglyphs-frozen.txt vs
  fresh capture: ONE hex char of 43,013 — sprite "ready" RGBA px 2576,
  r 234<->235, delta 1; ~1-in-8 cold runs) root-classed: bit-freezing
  browser-rasterized TEXT assumes a determinism canvas font rendering
  does not provide. Pre-registered characterization (new standing
  instrument port/gfx/glyph-jitter-probe.js, 5 fresh sessions + frozen
  + preserved-failed = 21 pairs, .loop/m4-task2r72-probe.log):
  structural drift 0/21, max channel delta 1, max diff-pixel count 1.
  Refutation shape (c) fired -> measured-then-frozen comparator
  port/gfx/glyph-compare.js replaces the vfxglyphs cmp in
  check-render.sh: structure EXACT, channels within 4, <=16 of 19,764
  pixels may differ; twin-pinned (script + expected-render.json
  glyphComparePins). vfxglyphs-frozen.txt NOT re-frozen (5/5 fresh
  sessions byte-matched it). Teeth T1-T7 all bite incl. negative
  control: the preserved failing pair now PASSES
  (.loop/m4-task2r72-teeth.log). Cold RENDER OK x2, fresh sessions
  (donecheck{,2}.log): IOU MIN 0.9010 / 0.9030, streams MATCH. This
  was a measurement-honesty correction, not a weakening — documented
  with exposure figures in the AGENT-LOG entry. Task-3 note: device
  path consumes the COMMITTED frozen file (browser-free) — device
  comparisons stay bit-exact; pin the frozen sha, never a fresh
  capture's.

- **Iter 71 (M4 task 2 hardening round 3 — loo trajectory continuity)
  DONE**: the single review-70 round-3 Medium (.loop/review-70-1.log,
  NO-GO; capture-canvas.js:487) closed on
  port/gfx/{capture-canvas.js,iou.js,expected-render.json(comment)}.
  The injection-frame canonical render joined the deterministic
  render-plane RNG; det = strict replay ASSERTED bitwise-equal to the
  canonical mask (capture-side throw + iou.js judge twin, negative
  tooth proven); post-canonical vfxQueue restored BY REFERENCE, the
  finally native re-render DELETED — zero native draws at the frame,
  frames 151+ continue exactly the saved-mask trajectory. Accident
  measured pre-fix (probe OLD arm: randomTail 0/4 equal, star scatter
  moved), continuity measured post-fix (NEW arm: 4/4 identical,
  det==canonical) — .loop/m4-task2r71-probe-{old,new}.log; declared
  probe deviation (184-mask form is lifetime-blind, template FRAMES
  measured) pre-registered. Cold done-check RENDER OK DONECHECK_RC=0
  (.loop/m4-task2r71-donecheck.log): IOU MIN 0.8982 -> 0.9026, f0150
  0.9339 -> 0.9319, bdiff values identical, NO pinned value moved;
  check-sim.sh skipped (zero port/sim bytes). **Task-2 renderer arc
  round 4 = CLOSURE-OR-CAP (driver)**: rerun the reviewer on the new
  commit; per PROCESS §3 bounded convergence, if round 4 raises only
  variants of closed classes the arc CAPS with the recurring objection
  class named.

- **Iter 70 (M4 task 2 hardening round 2 — INJECT1 grammar, cast
  domain, region fix, browser attribution, inkNames pin, closure
  TOCTOU, iou parsers) DONE**: all 7 review-65 round-2 Mediums
  (.loop/review-65-triage-r2.md / .loop/review-65-3.log) closed on
  port/gfx/{gfx_vfx.c,iou.js,check-render.sh,capture-canvas.js,
  expected-render.json}. M1 whitelist-exact INJECT1 C parser (+
  emit-side grammar guard); M2 vfx_cfg_int cast-domain guard at 8
  sites (2 cited + 6 class siblings; isfinite no-op reordered before
  the cast); M3 firefoxcharge judge region cfg.face->cfg.f (measured
  delta: frames 1/5 box -> 3/7 box, 26.6 px wider x; NO pinned value
  moved, 0.88 aggregate untouched, run-1 min 0.8982); M4 browser
  leave-one-out attribution (deterministic det/loo masks at the
  injection frame, evaluate-only — gfx-pagelib.js untouched; iou.js
  bdiff>0 per effect); M5 exact ordered 5-name inkNames pin in 4
  validator sites; M6 final closure-identity re-check before
  RENDER OK; M7 full-corpus VFXDATA1 validation + exact 16-hex
  scale.bits grammar. Cold done-check RENDER OK DONECHECK_RC=0
  (.loop/m4-task2r70-donecheck.log); 7/7 teeth fired with predicted
  classes incl. a REAL end-to-end TOCTOU run and a pre-fix UBSan
  diagnostic at the cited line (.loop/m4-task2r70-teeth.log);
  check-sim.sh skipped (zero port/sim bytes — justified in the log).
  **Task-2 renderer arc round 3 = CLOSURE (driver)**: rerun the
  reviewer on the new commit; per PROCESS §3 bounded convergence the
  arc is at round 3 of ~8.

- **Iter 69 (M4 hardening — carrier uniqueness + banner affine counts)
  DONE**: both review-68 Mediums (.loop/review-68-1.log) closed on
  `port/sim/calib/check-vfx-seam.sh` ONLY. R1: each CARRIERS entry's 3
  tokens asserted DISTINCT at the inventory pin (cross-component
  repetition stays legal — g01/g04/g06 serves four components by
  design); R2: the check-sim `== gNN (name)` banner literal gets
  `count_aff` like every other decision literal (banner exact ×1 +
  banner-affine ×1 per golden — torn `=`/`==` fragments now die).
  Grammars re-validated 55/55 against the same archived corpus, zero
  false rejections (.loop/m4-rig69-corpusval.log); teeth 2/2 fired
  with the predicted message classes (.loop/m4-rig69-teeth.log); cold
  done-check `VFX SEAM MATCH` DONECHECK_RC=0
  (.loop/m4-rig69-donecheck.log). **vfx-rig arc round 4 =
  CLOSURE-OR-CAP (driver)**: per PROCESS §3 bounded convergence, if
  round 4 raises only grammar-variant re-raises of the closed classes,
  the arc CAPS with the recurring objection class named.

- **Iter 68 (M4 hardening — vfx-seam identity binding + truncated
  resemblance + rc case-split) DONE**: all three review-66 Mediums
  (.loop/review-66-triage.md) closed on
  `port/sim/calib/check-vfx-seam.sh` ONLY. M1: grammar counts now bind
  IDENTITY — frozen CARRIERS array (3 carrier golden names per
  component; runA/runB banners exact ×1 each, per-name STREAM MATCH
  ×2, byte-stable bound positionally `3 0 1 1 1`) + 8-name SIM_GOLDENS
  for check-sim. M2: the `count_aff` affinity counter (extension OR
  torn PREFIX of the exact literal) on the verdict + every evidence
  literal + STREAM stems. M3: grep rc case-split in all count helpers
  (rc 0/1 = count, rc ≥ 2 / any awk rc = loud corruption death; no
  blanket suppression). Grammars validated 55/55 against the full
  archived corpus (11 raw iter-66 logs + 44 reconstructed sections
  from the 4 archived composed runs) with ZERO false rejections
  (.loop/m4-rig68-corpusval.log); teeth 5/5 fired
  (.loop/m4-rig68-teeth.log); cold done-check `VFX SEAM MATCH`
  DONECHECK_RC=0 (.loop/m4-rig68-donecheck.log). Round-3 review
  returned 2 Mediums (carrier uniqueness + check-sim banner affinity,
  .loop/review-68-1.log) — closed by iter 69. Judgment lives in `vfx_judge_log`
  — the registered template (with the round-2 refinements) for task-14
  verify_m4.sh.

- **Iter 67 (M4 task 2 hardening — injection-set pin + per-effect ink
  assertions) DONE**: both review-65 Mediums (.loop/review-65-triage.md)
  closed on port/gfx/{check-render.sh, capture-canvas.js, iou.js,
  expected-render.json}. M1: frozen `injectPin` (ordered 7-name reviewed
  set + inkNames 5 + frame 150) asserted independently by
  check-render.sh (early + INJECT1 emitter), capture-canvas.js
  (pre-browser) and iou.js — a dropped/renamed effect dies on every
  side. M2: no-inject + five LEAVE-ONE-OUT C baselines (all streams
  cmp'd == run-a: injection is render-plane-only), per-effect
  browser-ink + C-ink + leave-one-out differential-ink assertions at
  f150 + a region-soundness guard (all differential ink inside the
  derived-region union); regions derived from the executed stages.json
  transform + frozen VFXDATA1 bounds x dVfx code-literal scales
  (documented in iou.js injectRegions()). Teeth: T-M1 both sides fired;
  T-M2 round 1 REFUTED the shared-baseline design (region overlap —
  diff=123 despite the stub), reworked to leave-one-out, rerun fired
  exactly the finding's scenario (f0150 aggregate 0.9122 PASS + INJ
  dashDust diff=0 death). Cold done-check RENDER OK exit 0, fresh
  capture, IOU MIN 0.9049 (.loop/m4-task2r67-donecheck.log; capture cap
  2/2 — run 1 died on the registered node -p ANSI-colour class,
  String() fix). check-sim.sh skipped, justified: zero port/sim bytes.
  **Task-2 arc ROUND 2 PENDING (driver): rerun the reviewer with the
  COMPLETE commit diff** (the round-1 High was a truncated artifact —
  include gfx-pagelib.js + the C TUs for the Tier-B union/bounds note
  + this commit's fix bytes).

- **Iter 66 (M4 hardening — check-vfx-seam aggregator classes) DONE**:
  all 4 review-64 Mediums (.loop/review-64-triage.md) closed on
  `port/sim/calib/check-vfx-seam.sh` ONLY — the verify_m3.sh aggregator
  classes adapted host-side: freshness-evidence grammar (runA/runB/
  byte-stable/STREAM MATCH counts measured from the 11-log iter-64
  corpus), expect_verdict (exact ×1 + final-line + measured
  verdict-prefix resemblance), mkdir-atomic no-reclaim host lock
  (`build/vfx-seam.lock`, iter-41 posture), 10-literal inventory pin
  over CHECKS/VERDICTS/SPECS, `  | ` relay prefix (one column-0
  `VFX SEAM MATCH` possible). Teeth 4/4 fired
  (.loop/m4-rig66-teeth.log); cold done-check `VFX SEAM MATCH` rc 0,
  zero grammar false-rejections on fresh logs
  (.loop/m4-rig66-donecheck.log). Round-2 review returned 3 Mediums
  (M1 identity/M2 truncation/M3 rc split) — closed by iter 68; the host aggregator
  classes are the registered pattern for task-14's verify_m4.sh
  assembly (AGENT-LOG iter 66 zoom-out). Tier-B sim-TU surface: clean
  per reviewer (no action).

## [superseded by iter 66] (updated: 2026-07-18, iter 65 — M4 task 2 DONE)

- **Iter 65 (M4 task 2 — renderer vfx + overlay/banner/background + IoU
  re-freeze) DONE** (respawn writer after the credit-death; dead
  writer's pre-reg adopted with 2 recorded amendments, its ml_events
  dust-glue diff reviewed + adopted verbatim): cold
  `bash port/gfx/check-render.sh` → `RENDER OK`, exit 0
  (.loop/m4-task2-donecheck.log). Full render sequence both sides
  (renderVfx + renderOverlay(true); mask fg1|fg2|UI): all 45 dVfx draw
  arms in gfx_vfx.c (canvas-2d emulation, NaN no-ops load-bearing,
  render-LOCAL RNG), HUD overlay + browser-rasterized VFXGLYPHS1 glyph
  atlas, ink-suppressed background art, executed VFXDATA1 template
  plane — both artifacts ×2 byte-stable, committed + cmp-tripwired.
  Corpus 16→24 + synthetic f150 injection (firefox*/shine* measured
  zero-live in EVERY golden). Old 0.91 pin retired with its exposure;
  NEW pin **0.88** frozen after the pre-registered refutation-(a)
  round: percentShake capture-to-capture variance (f1297 0.8835)
  closed by a render-guard-class fix (capture zeroes shake under
  snapshot/restore), threshold = floor over both honest minima; final
  cold run min 0.9032. Teeth T1/T1b/T2a/T2b/T3/T4/T5 logged
  (.loop/m4-task2-teeth.log) incl. the honest T1 sub-threshold
  sensitivity note. Regressions: SIM CONFORMS 8/8 + VFX SEAM MATCH.
  Task-3 handoff: check-device-render.sh/check-device-opk.sh must
  thread --vfxdata/--glyphs into gfx_app (it now requires them);
  ready/go banner sounds await the task-6 mixer.

## [superseded by iter 65] (updated: 2026-07-17, iter 64 — M4 task 1 DONE)

- **Iter 64 (M4 task 1 — vfx seam widening, sim + capture side) DONE**:
  cold `bash port/sim/calib/check-vfx-seam.sh` → `VFX SEAM MATCH`,
  exit 0 (.loop/m4-task1-donecheck.log; ~7 min — captures are ~15-25 s
  each, not minutes). ml_events vfx plane widened name-only → FULL
  drawVfx config (MlVfx + ml_drawVfx* emitters + ml_vfx_sink renderer
  chokepoint; cb_vfx canon); 193 sites translated (112 move TUs +
  article 9 + hitdet 17 + physics 4 + asshort 1 + sim_boot
  entrance/start); affected-cluster list MEASURED to include hitdet
  (brief's guess refuted by grep — 18 upstream sites); 10 specs
  re-recorded ×2 byte-stable, every run STREAM-MATCH guarded, ALL
  cluster replays 0-divergence (2592 live full-config events);
  SIM CONFORMS 8/8 unchanged (frozen goldens untouched). Read-set
  widenings forced by the configs (rule-7 corollary, 2×):
  shieldDepletion +pos/face (7-key pre), puff stage projection
  +wallL/wallR. Teeth: nibble→11 exact, name-only→34 (= non-empty-vfx
  records), face-drop→11, hitdet-field-drop→14400; restores proven by
  0-divergence re-replay. Honest coverage: asshort breakShield +
  physics shocked/burning zero-live; sim_boot boot vfx capture-less
  (task 2's render checks exercise them). Task-2 handoff notes in
  AGENT-LOG iter 64 (sink semantics, drawVfx defaults, circleDust
  draws already burned, render-plane spawn sites out of seam).
  Tier-B review round for the sim TUs: PENDING (driver queues it —
  mechanical arg-threading, done-check is the bit-exact oracle).

## [superseded by iter 64] (updated: 2026-07-17, iter 63 — PHASE M4, REPLAN done)

- **Phase: M4 — Full-game parity (REPLAN complete, iter 63)**: fix_plan
  `Current phase: M4`; §M4 concretized as a conventions block + 14
  dependency-ordered tasks with runnable done-checks; M4 EXIT GATE
  concretized into CLAUDE.md §Commands (`bash
  port/sim/device/verify_m4.sh` — full-game trace suite on device at
  60 fps with audio+music + menu flows + OPK-into-FOH, verify_m3.sh
  freeze-manifest/authoritative discipline inherited; on mechanical
  pass the DRIVER emits `LOOP STOP: m4-complete — awaiting Chase
  acceptance playthrough`). Ladder: (1) vfx seam widening
  sim+captures; (2) renderer vfx + overlay/banner/bg + IoU re-freeze;
  (3) stage legibility at device scale; (4) ai.js C port
  (capture-replay verified); (5) live CPU integration (bridge retired
  from live path, d1/d9 coverage); (6) mixer fidelity + play-ids +
  stop-path; (7) music streaming from SD; (8) skip-burst attribution
  instrument; (9) FOH core + flows host; (10) FOH device; (11) target
  test data+sim; (12) target test FOH+device; (13) SD persistence;
  (14) verify_m4.sh assembly. Key PROVISIONAL calls (AGENT-LOG iter
  63): target-plane = a SEPARATE parallel stream (CHECKSUM.md stays
  v1, no re-freeze); M4 goldens at port/goldens-m4/ (oracle/ is
  M0-only); menus verified by structural flow scripts + the
  checksummed match-launch bridge (no browser IoU); outOfCameraTimer
  stays render-excluded everywhere; scope exclusions (target builder,
  replay UI, multiplayer, credits). Measured: fd_tan already
  vendored+swept — "adds tan" was pre-satisfied at M0.

- **Iter 62 (M3 hardening — gate relay prefix, true-respawn poll,
  probe-order attribution) DONE**: all three .loop/review-60-triage.md
  + attribution items landed. (H2-residual) verify_m3.sh relay_lines
  chokepoint — every relayed sub-content line (tail'd leg logs,
  canned-rc bytes, status lists) prints `  | `-prefixed; the genuine
  `M3 GATE OK` echo is the ONLY possible unprefixed line-anchored
  occurrence (contract documented at definition + emission site).
  (L1-residual) riglib rig_proc_respawn_poll TRUE-RESPAWN form
  (+ rig_proc_pid): pre-kill pid captured at all 3 opk sites; verified
  respawn = old pid GONE (/proc RC-checked) AND live single pid != old.
  (class completion) judge-render-timing.js emits judge_complete=1;
  parse_timing_judge in render+audio checks reads judge stdout from
  FILE BYTES with the 17-byte terminator assert (iter-61 pattern;
  corpus 5/5 zero false rejections). (attribution) T5 probe moved
  AFTER the paced attempts; gate counters saved before the probe.
  **ATTRIBUTION VERDICT: probe after-effect — class-fixed by
  ordering.** Cold run attempt1 skips=1 / attempt2 skips=0 → `DEVICE
  AUDIO OK (full p99 12.393 ms, underruns=0, attempts=2; cbs=5166
  starts=274 stops=0 skips=0/3600)`, exit 0
  (.loop/m3-task7r62-audio-donecheck.log; 1/3 paced cap; T5 through
  the reordered path: 19 underruns counted + rejected). The
  probe-before elevated signature (3/4 attempts; driver cold both
  attempts 3,1) did NOT recur; residual single-skip transient = the
  pre-probe registered class (M4 instrument seed stands); thermal
  confound honestly recorded; skips==0 gate unweakened, cooldown arm
  not triggered. Gate mechanics 5/5 green incl. NEW relay teeth
  (.loop/m3-task7r62-gate-mechanics.log — status refusal now names 11
  producers: the two edited reviewed-go surfaces
  check-device-render.sh + judge-render-timing.js truthfully flipped
  to arc-pending); respawn tooth 8/8
  (.loop/m3-task7r62-tooth-respawn.log); timing-blank tooth both
  shipped variants (.loop/m3-task6r62-tooth-timingblank.log).
  Manifest: 6 producers re-pinned + MANIFEST_SHA256 → 578bfbd5… same
  commit; `SELF-CHECK 23/23 + ANCHOR GREEN`
  (.loop/m3-task7r62-manifest-selfcheck.log). **BOTH ARCS ROUND-3
  SCOPED CONFIRM PENDING (driver)**: audio round-3 confirm surface now
  incl. iter-62 probe-order + timing-judge bytes; gate-arc round-3
  confirm surface = iter-62 relay/respawn/timing bytes. Sequencing
  unchanged: both closures → driver flips ALL statuses to reviewed-go
  in the phase-advance commit → cold authoritative verify_m3.sh →
  sentinel + Chase S1 ratification.

- **Iter 61 (M3 task 6 hardening ROUND 2 — audio round-2 triage
  closure) DONE**: both .loop/review-59-triage.md items landed. (M)
  platform_audio_sdl.h platform_audio_stop: SDL_PauseAudio(1) BEFORE
  the terminal gap sample (quiesce the callback source, then sample
  under SDL_LockAudio — no callback can start after the sample;
  accounting semantics otherwise identical). (L) check-device-audio.sh
  reads judge + pack producer outputs from FILE BYTES (judge stdout →
  file + byte-exact 'judge_complete=1\n' tail assert; pack verdict →
  file + wc -c == one-verdict-line + one final newline), so trailing
  blank lines violate the grammar as written. NEW teeth T9/T10
  (trailing-blank → death, positive controls pass) + T8 file-based;
  standing T5 probe through the reordered path: 24 underruns counted +
  rejected. Cold `bash port/gfx/check-device-audio.sh` → `DEVICE AUDIO
  OK (full p99 12.952 ms, underruns=0, attempts=1; cbs=5166 starts=274
  stops=0 skips=0/3600)`, exit 0 (.loop/m3-task6r61-donecheck.log; run
  cap 1/1, no retry consumed, one pre-registered arm rebuild).
  Manifest: check-device-audio.sh re-pinned (c5471dd5…, status
  truthfully arc-in-flight) + verify_m3.sh MANIFEST_SHA256 →
  7e148d8c… in the SAME commit per the documented discipline (the
  anchor line is verify_m3.sh's ONLY change; its NORMALIZED manifest
  row ca21b4a5… unchanged — see AGENT-LOG iter 61 for the driver's
  gate-closure review); `SELF-CHECK 23/23 + ANCHOR GREEN`
  (.loop/m3-task6r61-manifest-selfcheck.log). Residual class instance
  flagged (not fixed, out of triaged scope): parse_timing_judge's
  $()-captured timing-judge stdout still normalizes a TRAILING blank
  line (inherited task-4 apparatus — driver may queue with the render
  check). **AUDIO ARC ROUND 3 = SCOPED CONFIRM PENDING (driver)**:
  round-2 fix bytes (platform_audio_sdl.h + check-device-audio.sh)
  are the confirm surface; sequencing unchanged — audio round-3
  closure + gate-arc round-2 closure → driver flips statuses to
  reviewed-go in the phase-advance commit → cold authoritative
  verify_m3.sh → sentinel + Chase S1 ratification.

- **Iter 60 (M3 task 7 hardening — gate-assembly round-1 triage
  closure) DONE**: all 6 triage items (.loop/review-58-triage.md
  H1/H2/M1/M2/L1/L2) landed on port/sim/device/{verify_m3.sh,
  m3-freeze-manifest.txt} + port/gfx/check-device-opk.sh +
  port/gfx/opk/mlfk.sh + riglib.sh (shared respawn-poll body). The
  gate now: [0] verifies the manifest's own bytes vs the in-script
  MANIFEST_SHA256 anchor (update discipline: any manifest edit changes
  the literal in the SAME commit; verify_m3.sh's manifest row is the
  NORMALIZED digest excluding that line — circularity, see both
  headers); [0b] HARD-REFUSES in AUTHORITATIVE mode while any producer
  status is arc-in-flight/arc-pending (currently 9 — the refusal IS
  the expected default-run outcome until closure); `M3 GATE OK` prints
  ONLY on a fully-authoritative all-real run — MLFK_M3_DEV=1 or
  MLFK_M3_FAKE_LEG_DIR force `M3 GATE (DEV — NON-AUTHORITATIVE)` +
  exit 3 (readonly flag, sentinel structurally locked out);
  verdict-RESEMBLING malformed lines at every leg parse = corruption
  death (discriminators measured from the real corpus);
  check-device-opk.sh restores the frontend VERIFIED (pkill rcs
  case-split + bounded rig_proc_respawn_poll before the verdict; trap
  never re-kills a verified frontend); mlfk.sh refuses loud (RC=7)
  when MLFK_DATA_DIR lacks simdata.txt. All teeth fired
  (.loop/m3-task7r60-*.log); 23/23 + anchor self-check green; ZERO
  real-leg gate runs consumed (driver owns the phase-advance cold run;
  it will trigger one arm-stamp rebuild — RIG_SCRIPTS bytes changed).
  Manifest re-pinned for the 4 touched producers, statuses kept
  arc-pending/arc-in-flight per truth. **GATE ARC ROUND 2 = CLOSURE
  PENDING (driver). Sequencing unchanged: audio round-2 closure +
  gate-arc round-2 closure → driver flips ALL statuses to reviewed-go
  (cites = closure logs) in the phase-advance commit → cold
  authoritative verify_m3.sh → sentinel + Chase S1 ratification.**

- **Iter 59 (M3 task 6 hardening — audio round-1 triage closure) DONE**:
  all 5 triage items (.loop/review-57-triage.md H/M1/M2/M3/L) landed;
  cold `bash port/gfx/check-device-audio.sh` → `DEVICE AUDIO OK (full
  p99 12.267 ms, underruns=0, attempts=2; cbs=5166 starts=274 stops=0
  skips=0/3600)`, exit 0 (.loop/m3-task6r59-donecheck2.log; run 1 =
  .loop/m3-task6r59-donecheck.log, honestly REFUSED on the registered
  transient skip class — audio legs green both attempts). NEW: judge
  `judge_complete=1` integrity terminator + counter-bound retry
  classification (fail_* now reporting-only); STANDING T5 device
  starvation probe every run (19 underruns counted + rejected);
  boundary-interval gap accounting in platform_audio_sdl.h (open→first
  + last→stop; healthy runs still 0); app-summary resemblance rule;
  exactly-one-line pack verdict; teeth T6/T7/T8 fired. MEASURED
  EXPOSURE recorded (PROCESS §8): DMA-xrun blindness + no SDL1.2
  priority API — M4 mixer-fidelity seed is the closure path.
  m3-freeze-manifest.txt re-pinned for the 2 touched audio producers
  (same commit, documented path; 23/23 direct-shasum self-check
  green). **AUDIO ARC ROUND 2 = CLOSURE PENDING (driver): the closure
  review covers the new bytes; producer edits INVALIDATE prior gate
  evidence — sequence per triage: audio round-2 closure → task-7 arc →
  THEN the phase-advance cold verify_m3.sh.** Skip-class measurement
  for the M4 instrument candidate: 3/4 attempts today (frames
  466/467/1116/1170/1192), probe after-effect not excluded.

- **Iter 58 (M3 task 7, the M3 EXIT GATE) DONE**: cold
  `bash port/sim/device/verify_m3.sh` → `M3 GATE OK`, exit 0
  (.loop/m3-task7-donecheck.log). Four legs all passed host-judged:
  [1] `DEVICE CONFORMS 8/8 + SIM P99 OK`; [2] `DEVICE AUDIO OK (full
  p99 12.573 ms, underruns=0, attempts=2)`; [3] `OPK LAUNCH OK`
  (packaged with the SDK container's mksquashfs 4.4 ONLY, launched via
  the REAL gmenu2x frontend driven by fk_input, boot-marker bin-sha ==
  arm stamp, evidence g01 900/900 stream prefix == frozen, in-app
  screenshot judge-shot structural); [4] `S1 INPUT OK`. NEW:
  port/gfx/opk/{mlfk.sh, meleelight.funkey-s.desktop, icon32.png (from
  OUR renderer's g01 f900 shot — no Nintendo bytes)},
  port/gfx/check-device-opk.sh, port/sim/device/verify_m3.sh (REUSES
  the arc-hardened sub-checks; verdicts parsed by exact anchored
  grammar), port/sim/device/m3-freeze-manifest.txt (PROCESS §4
  reviewed-pin freeze — 23 producers; HARD-REFUSES before any leg on
  drift). Measured gmenu2x nav (empirical): conf ignored for start;
  pkill-respawn = stable persisted section (games); `n m r a`
  normalizes link + selects MeleeLight; wrong section → no boot marker
  → leg FAILS LOUD (fail-closed). Teeth T1-T4 fired. Regression
  DEVICE RENDER OK skips 0/3600 (attempt 2; attempt 1's 1-skip = the
  registered transient class, not a regression). **PHASE-ADVANCE IS
  THE DRIVER'S NEXT TURN**: ground-truth the gate cold, then the
  human-gate sentinel `LOOP STOP: m3-device — needed: Chase S1
  ratification playtest` (the GATE does NOT print it — driver duty).

- **[superseded by iter 58] updated 2026-07-17, iter-57 writer completion**

- **Phase: M3 — GATE PASSED (MILESTONE PASS: M3, 2026-07-17)**: the
  authoritative verify_m3.sh printed M3 GATE OK exit 0 (driver-cold, all
  23 producers reviewed-go). LOOP STOPPED at the §H human gate:
  **LOOP STOP: m3-device — needed: Chase S1 ratification playtest**.
  RATIFIED 2026-07-17 (Chase playtest: controls perfect, sound perfect;
  visual amendments → M4 seeds: stage-surface legibility at device
  scale + vfx render seam). #18 closing; next: M4 REPLAN (PLAN §4/M4).
  Play install lives at /mnt/mlfk-data + /mnt/Applications/meleelight.opk
  (persistent, survives rig cleanup).
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
- **Iter 55 (M3 task 4 hardening ROUND 3) DONE**: both review-54
  round-3 residuals closed — H cross-run deadman sequencing via
  STALE-STATE STARTUP NORMALIZATION (new step 0 chokepoint in
  check-device-render.sh: stale marker restored FIRST + RC-verified,
  stale deadman disarmed via its designed cancel channel with exit
  verified, state wiped only after; RIG_PRESERVE_DTMP held while an
  old backstop may still be needed) + NONCE-SCOPED deadman kill (argv
  tag infeasible — gfx_app rejects unknown args, gfx_app.c out of
  surface; equivalent: launcher records gfx_device's pid under
  gfx.pid.<nonce>, deadman kills only that pid and only while its
  /proc cmdline is still gfx_device); M rig_stamp_ok srchash value
  grammar restored (^srchash=[0-9a-f]{64}$ AND equality) +
  rig_srchash produce-time 64-hex assert (empty/short never stamped).
  Cold done-check DEVICE RENDER OK exit 0 FIRST attempt, skips 0/3600
  (.loop/m3-task4r55-donecheck.log; the round's one rebuild). Teeth:
  T1 plant+probe normalization, T2 the review's exact A/B stranding
  sequence (no stranding), T3 scoped-kill both directions, T4a-d +
  T5a2/b2/c stamp grammar (zero false rejections). Regression DEVICE
  CONFORMS g01, stamp HIT (.loop/m3-task4r55-reg-g01.log);
  conform/input skipped with pre-registered justification (stamp-
  machinery-only riglib edits). Zoom-out: cross-run state machines
  get ONE startup chokepoint, never per-sequence patches.
- **Iter 56 (M3 task 4 hardening ROUND 4, FINAL) DONE**: both
  review-55 Highs closed by INHERITED-STATE PESSIMISM
  (RIG_PRESERVE_DTMP=1 set GLOBALLY the instant the rig lock is held —
  before require_device/selftest/any probe can fail into the trap;
  trap preserves $DTMP and never touches a marker/cancel it does not
  own until step-0 normalization POSITIVELY verifies inherited-state
  ownership and clears the flag; step 0 moved ahead of the devsha
  selftest); the Medium closed by riglib rig_host_sha256() — FULL-LINE
  host shasum grammar (`<64hex>  <actual path>` reconstruction, mirror
  of rig_dev_sha256) at ALL surface extraction sites (pullv, stamp
  read/write/rehash, srchash stdin form, render gsum/hsum) — zero
  `cut -f1` scrapes remain. Cold done-check DEVICE RENDER OK exit 0
  FIRST attempt, skips 0/3600, full p99 11.400 ms
  (.loop/m3-task4r56-donecheck.log; the round's one rebuild). Teeth
  8/8: T1/T2 planted-foreign-state preservation through forced
  require_device + mid-normalization probe failures (live transport —
  preservation proven as a decision), T3a-f shasum grammar incl. the
  review's exact 64-hex-field-wrong-tail case, zero false rejections.
  Regression DEVICE CONFORMS g01, stamp HIT via the new parser
  (.loop/m3-task4r56-reg-g01.log). Zoom-out: protection flags default
  to PRESERVE from resource-acquisition time; only positive
  verification flips them.
- **In flight**: task-4 arc CAPPED-CLOSED (driver-recorded; class + Low disposition in AGENT-LOG) (iter
  56 was the pre-announced FINAL round; recurring class named:
  "cross-run frontend-park/deadman sequencing before startup
  normalization owns inherited state"; residual = operator-error
  concurrency, dispositioned Low with reviewer concurrence in
  .loop/review-55-1.log; review-50's untriaged Low — gfx_app
  getline/ferror — left for the driver's cap record). task-5 arc
  CLOSED at GO (round 2). Then: task-6 review arc → task 7 (OPK +
  verify_m3.sh gate).
- **Iter 57 (M3 task 6) DONE**: `bash port/gfx/check-device-audio.sh`
  → DEVICE AUDIO OK (full p99 12.614 ms, underruns=0, attempts=2;
  cbs=5166 starts=274 stops=0 skips=0/3600), exit 0
  (.loop/m3-task6-donecheck.log) — AUDIO IS LIVE ON THE FUNKEY: g01
  full match with render + the 44100/S16LSB/2ch/512 callback + the
  8-voice SFX mixer (spike math verbatim; steal-oldest-by-start-seq —
  spike-default claim refuted, audiotest has NO allocation), fed via
  the ONE ml_snd_sink chokepoint (sim_tick untouched; SIM CONFORMS +
  every audio-on stream verifies — the mixer only reads). SNDPACK1
  from the REUSED pipeline audio stage, count=180 + sha frozen; pushed
  to /mnt/mlfk-scratch with provenance (never committed). Audio-on
  p99 cost vs task-4 baseline ≈ +1.1 ms (sim 8.481 / render 3.422 /
  present 1.849). NEW transient measurement: cold attempt 1 skips=8
  (BURST form of the registered class) — retry policy absorbed it,
  attempt 2 clean; bursts recurring → the iter-56 M4 attribution
  instrument, never a wider retry budget. Teeth: standing T1-T4
  (pack truncation, dropped-blob death at play, underrun-perturbation
  gate fail, grammar deaths) + device T5 (64-sample starvation → 17
  underruns counted + rejected; .loop/m3-task6-tooth-t5.log).
  Honest coverage: audible FIDELITY unverified by construction (M3 =
  structural liveness only); stop-path has zero live g01 coverage;
  M4 seeds registered (mixer fidelity + music, stop-path coverage,
  skip-burst instrument). Regressions green: DEVICE RENDER OK (11.065
  ms full p99) + SIM CONFORMS (.loop/m3-task6-reg-{render,sim}.log).
- **Latest AGENT-LOG entry**: iter 73 (M4 task 3 RESULT — landed +
  verified, done-check BLOCKED on the external stall class; honest
  report); latest log id: .loop/m4-task3-donecheck.log (attempt
  family + m4-task3-teeth.log, m4-task3-prof*-device.log,
  m4-task3-manifest-selfcheck.log).
- **Device**: FunKey-S on ADB, id 12c00003237f5528, healthy. adbd drops
  exit codes → RC-echo via port/sim/device/adbsh.sh. /tmp tmpfs 128 MB;
  big artifacts → /mnt/mlfk-scratch; ADB pulls ~4.4 MB/s (budget pull
  time). Arm build stamp-cached (`MLFK_FORCE_ARM=1` forces).
- **Branch**: agent/auto, clean between iterations (iter-39 hardening
  commit on top of af06bb7 + process-docs commit). Origin only.
- **Loop**: dynamic self-paced driver; ~20-30 min heartbeat; writer
  completion notifications are the primary wake signal.

## Next

1. Driver: Tier-B review round for the iter-64 sim-TU surface
   (PROCESS §3 — mechanical vfx arg-threading across 116 TUs +
   ml_events/canon; done-check is a bit-exact oracle replay, so one
   structural round; escalate on any Medium+).
2. Writer: M4 task 2 — renderer vfx + overlay/banner/background + IoU
   re-freeze (fix_plan §M4; done-check `bash port/gfx/check-render.sh`
   → `RENDER OK`; consume ml_vfx_sink — handoff notes in AGENT-LOG
   iter 64).
3. Then the ladder in order (fix_plan §M4 tasks 3-14), Tier-A arcs on
   every non-checksummed shipping surface, Tier B ≥1 round on sim TUs.
3. Phase end: driver cold AUTHORITATIVE `bash
   port/sim/device/verify_m4.sh` → `M4 GATE OK` → sentinel
   `LOOP STOP: m4-complete — awaiting Chase acceptance playthrough`.

[superseded by iter 63 — M3 driver sequencing retained in AGENT-LOG]

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
