# On-device feasibility spike — measured numbers (ticket #8)

Resolves wayfinder ticket #8: replace the two undemonstrated assumptions from
the strategy survey (`porting-strategies.md` §9) with numbers **measured on a
real FunKey-S** (adb `12c00003237f5528`, single Cortex-A7 @1.2GHz, VFPv4, no
GPU, 240x240 16bpp RGB565 panel, HWSURFACE|DOUBLEBUF honored).

Date: 2026-07-13. Branch: `spike/device-feasibility`.
Code + raw logs + evidence frames: `spikes/device-feasibility/`.

Budget frame (funkey-envelope doc): 16.67 ms/frame total at 60fps
(~20.0M cycles); render target **<= ~8 ms** (half budget) to leave room for
the sim.

---

## Experiment 1 — software vector-path rasterization at 240x240 (S1's open risk)

### What ran

`rastbench.c` — a C/SDL-1.2 software renderer exercising exactly the
meleelight rendering model (anatomy doc §6) on REAL data:

- **Real animation data**: fox + marth WAIT/DASH/ATTACKAIRN extracted from
  `src/animations/` by `extract_anim.js` (356 paths, 124,576 int16 coords —
  the verbatim `Int16Array` bezier-path format of `render.js`
  `drawArrayPathCompress`, cycled frame-by-frame like `player.timer` does).
- **Worst-ish scene, every frame**: full 240x240 clear -> battlefield-ish
  stage (main platform + 3 platforms, polygon fills) -> **2 characters**
  (single-colour silhouettes: flatten cubics to polylines at tolerance
  `tol`, nonzero-winding scanline fill via active-edge table, x-mirrored
  facing, animated positions; ~58 cubics/char/frame) -> **4 projectiles**
  (rotated 4-bezier diamonds through the same pipeline) -> `SDL_Flip`.
- **Variants**: AA off (hard spans) / AA on (4x vertical subsamples +
  fractional span-end coverage, 565 blend); flattening tolerance 0.25px /
  1.0px; character scale `zoom=1` (browser-equivalent: charScale x
  (240/1200), chars ~13-20 px tall) and `zoom=2` (camera-zoomed bound,
  ~26-40 px).
- 1200 frames per variant, first 30 dropped; per-frame `clock_gettime`.

Fidelity evidence: `results/device-frame-aa-zoom2.png` (dumped **by the
device** from its own surface post-flip) is pixel-identical to the host
headless render; fox/marth silhouettes are clearly recognizable.

### Numbers (ms/frame, n=1170)

| Variant | raster avg | raster p99 | flip avg | **total avg** | **total p99** | total max |
|---|---:|---:|---:|---:|---:|---:|
| **baseline** (clear+flip only — the floor) | 0.111 | 0.342 | 0.762 | **0.873** | **1.261** | 9.37 |
| scene AA-off tol 0.25 zoom 1 | 0.538 | 0.843 | 0.741 | **1.279** | **1.683** | 3.96 |
| scene AA-off tol 1.0 zoom 1 | 0.447 | 0.791 | 0.757 | **1.203** | **1.710** | 8.32 |
| scene AA-on tol 0.25 zoom 1 | 1.440 | 2.002 | 0.748 | **2.188** | **3.444** | 10.71 |
| scene AA-on tol 1.0 zoom 1 | 1.340 | 1.814 | 0.745 | **2.085** | **2.688** | 8.24 |
| scene AA-off tol 0.25 zoom 2 | 0.674 | 1.031 | 0.740 | **1.414** | **1.870** | 4.93 |
| scene AA-on tol 0.25 zoom 2 (worst variant) | 1.802 | 2.388 | 0.737 | **2.539** | **3.212** | 11.88 |

(max column: ~1-in-1000 OS-jitter outliers on the single core — adbd/kernel
housekeeping, not render cost; p99 is the honest tail.)

### Verdict — S1's renderer question is CLOSED, with room to spare

- The **worst** variant (anti-aliased, finest tolerance, 2x-zoomed
  characters) renders + presents in **2.5 ms avg / 3.2 ms p99** — a **2.5x
  margin under the 8 ms half-budget**, leaving **~14 ms/frame** for sim +
  audio + input + OS.
- AA costs ~2.7x the raster time of non-AA (1.8 vs 0.67 ms) and is
  **affordable** — the "faithful-looking anti-aliased vector look" survives
  on-device; no need to drop to hard-edged fills (0.55 ms) except as free
  headroom.
- Flattening tolerance barely matters (tol 1.0 saves ~7%); ship quality
  (0.25 px) is essentially free.
- `SDL_Flip` (~0.74 ms avg) is already ~30-60% of a scene frame — present
  cost, not raster cost, is the fixed floor (0.87 ms).
- Cycles check: 2.5 ms @1.2GHz ≈ 3.0M cycles ≈ 52 cycles/px full-repaint —
  ~30x cheaper than ssb64's textured-3D 1,667 cyc/px, confirming the
  envelope doc's "2D vector fighter lives in a different, feasible regime".

Caveats (honest scope): single-colour fills only (no gradients/vfx blend
stacks — meleelight's vfx are extra fills of the same kind, covered by the
headroom); 5-layer canvas compositing collapsed to one buffer (correct for a
port — the layers exist for browser dirty-tracking, not visual necessity);
no sprite caching — this is the *pessimistic* every-frame-retessellate cost.

## Experiment 2 — QuickJS instrument (S2)

### What ran

- **Bellard QuickJS 2026-06-04** (the "+42%" release; quickjs-ng is
  cmake-only and the SDK image has no cmake — ticket allowed the fallback),
  cross-compiled **static** for armv7/musl in ONE gcc invocation via a
  40-line embedder (`qjsmin.c`: no qjsc/repl bootstrap needed; adds µs
  `hrtime()` + RSS report). It cross-compiled first try — the "trivially
  cross-compiles" claim from the survey holds.
- `simbench.js` — a meleelight-shaped sim workload: REAL leaf functions
  copied verbatim from the clone (Vec2D, linAlg dot/scalar/norm/add/
  subtract/euclideanDist/orthogonalProjection, solveQuadraticEquation) driven
  in the anatomy-§3 tick shape over playerObject-shaped state: 4x
  (interpretInputs over an 8-deep buffer + physics update with Vec2D
  allocation churn + ECB sweeps vs 12 surfaces x 4 ECB points x 3 passes) +
  hitDetect x4 (swept circle-circle per hitbox pair) + executeHits (real
  Melee KB formula) + per-tick state deepCopy (replay.js does this every
  frame). *A calibrated proxy, not the actual bundle* (running the real
  bundle headless is ticket #7's oracle harness).

### Numbers

| Metric | Value |
|---|---|
| Device (QuickJS, 600 frames) | **min 25.0 / avg 29.3 / p50 29.1 / p90 30.3 / p99 47.9 / max 55.4 ms per sim frame** |
| Host Node v22 JIT, same file | avg **0.041** ms/frame (Apple Silicon) |
| Slowdown, device-interpreted vs desktop-JIT | **~714x** |
| Static binary size | **890 KB** (stripped) |
| Workload RSS on device | **VmRSS 1.16 MB / VmHWM 1.17 MB** |

Calibration note: the proxy measures ~0.04 ms under desktop V8 — if the real
sim frame costs the survey's assumed 0.3-1 ms under V8, the interpreted
device cost scales to **hundreds of ms/frame**; even this LIGHT proxy already
costs 29 ms — **1.8x the entire 16.7 ms frame budget** before a pixel is
drawn.

**Bonus determinism data point:** after 600 frames the state checksum
diverges between Node and device QuickJS (818.283 vs 637.256). The workload
calls `Math.sin/cos/atan2/pow` every frame; V8's fdlibm vs musl's libm differ
in last-ULP bits and the dynamics amplify them. This is on-device
confirmation of the survey's §1: **any oracle-grade runtime (and the C port)
must vendor V8's fdlibm-derived transcendentals** — host-libm QuickJS is
already checksum-incompatible with V8.

### Verdict

- **Strategy B (ship interpreted JS) is dead, now by measurement**: >= 29
  ms/frame sim-only (real bundle: worse), i.e. <= ~2 fps-equivalent
  sim+render against a 60fps kill-criterion. Matches the survey's 2-9 fps
  order-of-magnitude estimate (375-560x predicted; ~714x vs a faster modern
  host core measured).
- **The instrument itself is a keeper**: a 1-command-buildable, 890 KB
  static, 1.2 MB-RSS QuickJS runs real ES6 on-device — perfectly serviceable
  as the future on-device/host **oracle runtime** (determinism spike,
  ticket #7) once its Math table is patched to fdlibm.

## Strategy recommendation for the lock grilling (#10)

**Lock S1 (Strategy A/E: agent-driven structure-parallel C rewrite +
executed-data pipeline).** Its one undemonstrated assumption — the software
vector renderer — is now measured at 2.5 ms/frame worst-variant (3.2 ms p99)
against an 8 ms allowance, with the full anti-aliased look. The competing
strategy class (B/D, interpretation) is measured dead: 29 ms/frame for a
light sim proxy, 714x off desktop JIT. Numbers this spike adds to S1's plan:
render budget confirmed with ~5.5 ms spare; present floor 0.87 ms;
transcendental divergence confirmed live (vendor fdlibm on both sides, per
survey §1); QuickJS retained as oracle instrument only. Remaining S1 risks
are the ones the survey already scoped (divergence burn-down rate, audio
spike) — none are renderer- or strategy-threatening.

---

*Method: all device numbers from synchronous `adb shell sh -lc` runs with the
gmenu2x frontend parked, binaries + data in /tmp (tmpfs), 1200-frame runs
(rasterizer) / 600-frame runs (QuickJS), warmup skipped; device left clean
(files removed, frontend restored). Exact repro commands:
`spikes/device-feasibility/README.md`.*
