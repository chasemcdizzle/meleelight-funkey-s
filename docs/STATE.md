# STATE.md — current truth (driver-updated every turn)

_Read CLAUDE.md first, then this page. History → docs/AGENT-LOG.md;
queue → fix_plan.md; standards → docs/PROCESS.md._

## Live right now (updated: 2026-07-16, post-iter-43 writer)

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
- **In flight**: nothing — task-2 Tier-A review arc (conform script +
  riglib + percentiles.js, non-checksummed surfaces) opens on landing
  (driver-owned), then task 3 (renderer core, host-side).
- **Latest AGENT-LOG entry**: iter 43 (M3 task 2); latest log id:
  .loop/m3-task2-donecheck.log.
- **Device**: FunKey-S on ADB, id 12c00003237f5528, healthy. adbd drops
  exit codes → RC-echo via port/sim/device/adbsh.sh. /tmp tmpfs 128 MB;
  big artifacts → /mnt/mlfk-scratch; ADB pulls ~4.4 MB/s (budget pull
  time). Arm build stamp-cached (`MLFK_FORCE_ARM=1` forces).
- **Branch**: agent/auto, clean between iterations (iter-39 hardening
  commit on top of af06bb7 + process-docs commit). Origin only.
- **Loop**: dynamic self-paced driver; ~20-30 min heartbeat; writer
  completion notifications are the primary wake signal.

## Next

1. Driver: ground-truth iter 43 (cold done-check re-run) + open the
   task-2 Tier-A arc (check-device-conform.sh, riglib.sh,
   percentiles.js) → launch task-3 writer (renderer core, host-side)
   on GO.
2. Ladder: fix_plan §M3 tasks 3-7 → M3 gate (`verify_m3.sh`) →
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
- Standing: never push to upstream/schmooblidon; origin only; no
  distribution of anything; writers never post to GitHub (driver owns
  tracker writes).
