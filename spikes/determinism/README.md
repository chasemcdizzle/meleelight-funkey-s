# Determinism / oracle spike (issue #7)

Answers: is Slippi-style **input-replay + per-frame state checksum** against
browser meleelight a real verification spine? Verdict and analysis in
[`docs/research/determinism-spike.md`](../../docs/research/determinism-spike.md).

## Contents

- `meleelight-harness.patch` — tiny patch on upstream meleelight
  (`schmooblidon/meleelight` @ `27af171`): exposes `window.__harness`
  (programmatic match setup + manual `gameTick` stepping), a synthetic-input
  injection point at the top of `pollInputs`, a `__harnessMode` guard that
  replaces the self-arming `setTimeout(gameTick, 16)` re-arm, a
  `__harnessNoRender` guard in `renderTick`, and RNG isolation for
  `percentShake` (whose wall-clock setTimeout callbacks otherwise steal
  draws from the seeded gameplay PRNG stream — the one leak the spike
  found; see the verdict doc).
- `harness/init.js` — page init (before any game script): mulberry32-seeded
  `Math.random` (+ call counter), virtual `performance.now`/`Date.now` clock
  advancing 16.667 ms/tick, call counters on `Math.sin/cos/atan2/pow/...`.
- `harness/pagelib.js` — page-side stable serializer (sorted keys, exact
  float round-trip via `String(x)`, `-0`/`NaN` explicit, cycle-safe) +
  SHA-256 via `crypto.subtle` + the frame step loop.
- `harness/gen-trace.js` — deterministic input-trace generator (P1 Fox vs
  P2 Marth on Battlefield: lasers, hits, grab+mash (CAPTUREWAIT), scripted
  walk-off KO, then seeded mash chaos).
- `harness/trace-p1p2.json` — the generated 3800-frame trace used for the
  verdict (seed 1337).
- `harness/run.js` — Playwright runner (headless Chrome): serves the clone,
  injects init scripts, starts a match via `__harness.setupMatch`, steps N
  frames feeding the trace, emits `{frame, sha256}` stream + coverage +
  math/rng call counts.
- `harness/compare.js` — first-divergence finder + field-level state diff.
- `run-experiments.sh` — experiments A–D end to end.

## Repro

```sh
# 1. clone + patch upstream (MIT; private use only)
git clone https://github.com/schmooblidon/meleelight /tmp/meleelight
cd /tmp/meleelight && git apply /path/to/meleelight-harness.patch
# also remove dead devDeps that break npm install in 2026:
#   deepstream.io (dead git URL: uws-dependency), electron*
# and drop the postinstall script (see determinism-spike.md §build)

# 2. build with node 8 (docker, amd64-under-emulation is fine)
docker run --rm --platform linux/amd64 -v "$PWD":/app -w /app node:8 \
  bash -c "npm install --ignore-scripts && npm run animations && npm run build"

# 3. harness deps (any modern node; uses installed Chrome via channel:chrome)
cd spikes/determinism/harness && npm i playwright

# 4. run everything
../run-experiments.sh /tmp/meleelight
```

Single run:

```sh
node harness/run.js --dist /tmp/meleelight --frames 3600 --seed 42 \
  --out out/run.json \
  [--native-rng]           # experiment B: leave Math.random unseeded \
  [--cpu]                  # experiment C: P2 = CPU difficulty 5 \
  [--real-clock]           # skip performance.now/Date.now freezing \
  [--capture-frames 512]   # store full serialized state at these frames \
  [--capture-all]          # store every frame (divergence field diffs) \
  [--render]               # keep the rAF renderer drawing (debug)
node harness/compare.js out/run1.json out/run2.json
```

## Trace format

`trace-p1p2.json` = JSON array indexed by frame; each element is
`[inputP0, inputP1, null, null]`, each input a full 22-field meleelight
`Input` object (`src/input/input.js` shape: 12 bools `a b x y z l r s du dl
dr dd`, analog `lsX lsY csX csY lA rA rawX rawY rawcsX rawcsY`). The runner
holds the last trace frame once the trace is exhausted. Frames before the
match's `starting` window ends (~frame 91) are ignored by the sim.
