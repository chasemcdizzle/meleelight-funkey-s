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
