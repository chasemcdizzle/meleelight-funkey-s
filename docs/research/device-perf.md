# device-perf.md — measured FunKey-S performance (append-only)

Measured tables only — every row comes from a logged run of a check
script on the real device (id in the entry); nothing here is estimated.
Append new entries at the bottom; never edit or delete a recorded row.

## 2026-07-16 — iter 43 (M3 task 2): sim-only per-frame timing, all 8 goldens

- Source: `.loop/m3-task2-donecheck.log` — cold
  `bash port/sim/device/check-device-conform.sh` → `DEVICE CONFORMS 8/8`
  + `SIM P99 OK`, exit 0. Device 12c00003237f5528 (FunKey-S, Cortex-A7),
  static armv7 sim_device (SDK gcc 10.2, every TU
  `-O2 -ffp-contract=off -Wall -Wextra -Werror -static`).
- Methodology: `sim_device --timing` records per-frame CLOCK_MONOTONIC ns
  around `sim_game_tick` + `sim_frame_hash` ONLY (stream print excluded;
  buffered in RAM, written post-run — zero file I/O in the frame loop).
  p50/p99 computed HOST-side by `port/sim/device/percentiles.js`
  (nearest-rank on the ascending sort, idx = ceil(q*n)-1). "wall s" is
  the host-timed whole device dispatch for that golden (includes trace +
  simdata parse and tmpfs stream write; informational). Gate: p99_ns <
  16,670,000 (16.67 ms) per golden.

| golden | frames | p50 ms | p99 ms | wall s |
|---|---|---|---|---|
| g01 | 3600 | 5.691 | 10.334 | 21 |
| g02 | 3600 | 5.407 | 9.962 | 20 |
| g03 | 3600 | 5.068 | 9.624 | 19 |
| g04 | 3600 | 5.045 | 9.436 | 19 |
| g05 | 3600 | 5.564 | 9.952 | 21 |
| g06 | 3600 | 5.814 | 10.445 | 22 |
| g07 | 3600 | 4.266 | 7.954 | 16 |
| g08 | 3600 | 5.444 | 10.684 | 21 |

- Worst sim-only p99: 10.684 ms (g08) — 6.0 ms of the 16.67 ms frame
  budget remains for render + present + audio at p99 (PLAN §5 allows
  p99 render-only ≤ 8 ms; the M3 task 4/6 full-frame gates will measure
  the real sum). Run-to-run stability: a second full matrix in the same
  session (`.loop/m3-task2-run1.log`) agreed within 0.02 ms at p50 and
  0.35 ms at p99 per golden.
- Cross-check: the g07 CPU golden is the cheapest (4.27/7.95 ms) — its
  AI-bridge slot replaces a human input column and its match fields the
  fewest live hitboxes; g07/g08 bridge parse cost lands outside the
  timed region (boot), visible only in wall.

## 2026-07-16 — iter 45 (M3 task 2 hardening): post-hardening cold matrix re-measure

- Source: `.loop/m3-task2r45-donecheck.log` — cold
  `bash port/sim/device/check-device-conform.sh` → `DEVICE CONFORMS 8/8`
  + `SIM P99 OK`, exit 0, after the review-43 round-1 fixes (matrix pin,
  timing-buffer overflow guard, perf-history presence assertion). Same
  device/toolchain/methodology as the iter-43 entry; binaries rebuilt
  this iteration (script bytes are stamp input — stamp
  96f711a71501a867192bb1cae33bf9a60e987b5799c1b2db2ca7964dfd80c5c6).

| golden | frames | p50 ms | p99 ms | wall s |
|---|---|---|---|---|
| g01 | 3600 | 5.672 | 9.850 | 21 |
| g02 | 3600 | 5.420 | 10.049 | 20 |
| g03 | 3600 | 5.072 | 9.166 | 19 |
| g04 | 3600 | 5.031 | 9.250 | 19 |
| g05 | 3600 | 5.564 | 9.765 | 21 |
| g06 | 3600 | 5.805 | 11.004 | 22 |
| g07 | 3600 | 4.257 | 8.137 | 16 |
| g08 | 3600 | 5.444 | 10.893 | 21 |

- Consistent with iter 43 (p50 within 0.02 ms per golden; p99 within
  0.6 ms — worst now g06 11.004 ms, still > 5.6 ms headroom under the
  16.67 ms budget). The perf-history presence assertion in the check
  script asserts these tables EXIST per pinned golden; appending them
  stays this writer duty.

## 2026-07-16 — iter 50 (M3 task 4): first LIVE device render — g01 full match, SDL1.2, paced 60 fps

- Source: `.loop/m3-task4-donecheck.log` — cold
  `bash port/gfx/check-device-render.sh` → `DEVICE RENDER OK`, exit 0
  (stamp 335a0f1a…). Same device/toolchain; the measured binary is
  `gfx_device` (SDL1.2 backend, DYNAMIC libSDL-1.2, raster TU -O3), the
  full headless sim + port/gfx compositor, paced at 16,666,667 ns/frame
  with the frameskip valve armed. Buckets are WORK time around
  tick+hash / render / lock+blit+SDL_Flip; pacing sleep excluded.
  SetVideoMode succeeded at chain step 0 (HWSURFACE|DOUBLEBUF,
  16bpp RGB565). Stream STREAM MATCH 3600/3600 with render+present live.

| golden | frames | bucket | p50 ms | p99 ms | max ms |
|---|---|---|---|---|---|
| g01 | 3600 | full (sim+render+present) | 7.399 | 10.743 | 18.907 |
| g01 | 3600 | sim | 5.051 | 7.527 | — |
| g01 | 3600 | render | 1.316 | 2.568 | 10.264 |
| g01 | 3600 | present | 0.958 | 1.479 | — |

- Skips: **0/3600** (the single worst frame, full 18.907 ms, overran the
  budget in its RENDER phase — the valve only skips when the SIM
  finishes past the deadline, so that frame rendered late and the next
  frame absorbed the ~2 ms without a skip; the standing
  in-check tooth proves it fires at a 1000 ns budget). Render p99
  2.568 ms vs the 8 ms PLAN §5 allowance (3.1x margin — right on the
  rastbench 2.54/3.21 prediction); full-frame p99 10.743 ms leaves
  ~5.9 ms for the M3 task-6 audio callback. Device screenshot (own
  framebuffer, frame 900) BIT-IDENTICAL to the host headless render —
  the render float plane is cross-platform deterministic after the
  iter-50 device-libm class fix (integer floor/ceil helpers + fdlibm
  trig routing; sqrtf/fabsf swept healthy in mathsweep's new float
  columns).
