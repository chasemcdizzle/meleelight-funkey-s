# Determinism / oracle spike — verdict (issue #7)

**Question**: is the project's verification spine — Slippi-style deterministic
input-replay + per-frame state checksums against browser meleelight as
oracle — real?

**Verdict: YES — meleelight is input-deterministic, conditional on one thing
only: a seeded gameplay RNG.** Two fresh headless-Chrome page loads fed the
same 3600-frame input trace produce **bit-identical SHA-256 checksum streams
for every frame**, through hits, KOs, respawns, fox lasers, shields, grabs,
throws, and a mashed-out grab (CAPTUREWAIT). This holds with a CPU player
too. The oracle harness works today at ~6,900 sim-frames/sec including
serialization + hashing.

Setup, harness code, patch, and repro commands: [`spikes/determinism/`](../../spikes/determinism/README.md).
Upstream: `schmooblidon/meleelight` @ `27af171`, built with webpack 1 under
docker `node:8` (amd64-under-emulation on an M-series Mac works; two dead
2018 devDeps — `deepstream.io`'s git-URL dependency and `electron*` — must be
dropped from package.json first). Browser: headless Chrome 150 via
Playwright, one fresh browser context per run.

## Experiments

Trace: 3800 frames, P1 Fox (human) vs P2 Marth on Battlefield; scripted
phases (lasers → approach → jab scrap → grab attempts → walk-off KO) then
seeded beat-based mashing. Verified coverage per run: `CAPTUREWAIT` +
`CAPTUREPULLED`/`CAPTURECUT`/`CATCHATTACK` (grab connected, victim mashed),
`DEADDOWN`/`REBIRTH` (KO + respawn), `DAMAGE*`, `GUARD*`, max 9 concurrent
articles (lasers), 40+ distinct action states, match live all 3600 frames.

| Exp | Config | Result |
|---|---|---|
| A | seeded `Math.random` (mulberry32), virtual clock, 2 humans, two fresh page loads | **IDENTICAL** — 3600/3600 checksums equal; RNG draw counts equal (134) |
| A′ | same but **real wall clock** (no `performance.now`/`Date.now` freeze) | **IDENTICAL**, and identical to A's stream — the sim reads no wall-clock at all |
| B | **native (unseeded)** `Math.random`, 2 humans | diverges at frame **2995**: P2 in `CAPTUREWAIT`, `phys.pos.x` differs by exactly 1.0 — the `characters/shared/moves/CAPTUREWAIT.js:26` mash-out wiggle (`0.5*Math.sign(Math.random()-0.5)`), precisely the site the anatomy doc predicted. Frames 1–2994 identical: **nothing else in 2-human gameplay touches RNG state** |
| C | seeded, P2 = **CPU** (difficulty 5) | **IDENTICAL** — 3600/3600; RNG draw counts equal (637). The AI's many `Math.random` reads are deterministic under a seeded PRNG **once the stream is protected** (see leak below) |
| D | transcendental exposure | see table below |

## The one leak found (and fixed in the harness patch)

`percentShake` (`src/main/main.js:361-367`, HUD-only) fires
`setTimeout(..., 20/40/60ms)` callbacks that call `Math.random` **outside
the sim step, at wall-clock-dependent times** — stealing draws from the
shared seeded stream and shifting every later gameplay draw. With 2 humans
this survived (gameplay draws are rare); with a CPU it desynced run-vs-run
at frame ~2690 with unequal draw counts (487 vs 525) despite seeding.
Routing `percentShake` to a separate RNG made experiment C bit-identical.

**Class lesson (zoom-out) for the port and the checksum spec:** gameplay
RNG must be a dedicated, seeded PRNG owned by the sim, advanced only inside
the tick. Cosmetic/async consumers (vfx, HUD shake, KO-shout selection,
menu sparkle — ~127 of the 134 `Math.random` sites) must NOT share that
stream. Slippi reaches the same design. Total gameplay-stream draws in a
60-second 2-human match: **134** (mostly screenShake seeds + KO shouts +
grab wiggle); with one CPU: **637**.

## Transcendental / libm exposure (experiment D)

Math call counts inside sim steps over 3600 frames (render suppressed):

| fn | 2 humans | human + CPU | per frame (2H) |
|---|---|---|---|
| `sqrt` | 33,744 | 66,762 | 9.4 |
| `atan2` | 6,499 | 6,779 | 1.8 |
| `sin` | 2,027 | 1,469 | 0.6 |
| `cos` | 2,018 | 1,453 | 0.6 |
| `atan` | 656 | 388 | 0.2 |
| `pow` | 288 | 13,926 | 0.1 (AI uses pow heavily) |
| `tan` | 0 | 4 | — |
| `exp` `log` `asin` `acos` `cbrt` `hypot` | 0 | 0 | — |

`sqrt` dominates but is **correctly rounded per IEEE-754** — bit-identical
on any conforming double implementation, no risk. The real cross-engine
exposure for the C port (issue #4's plan) is only **`atan2`, `sin`, `cos`,
`atan`, `pow`** (+`tan` if AI is ported) — a small, closed set. Ship V8's
fdlibm-derived implementations of those five on the C side (or shim both
sides to one implementation) and the checksum oracle can demand exact
equality; no epsilon comparison needed.

## What this means for the oracle harness design (fog item) and checksum spec

1. **The spine is real.** Input-only replay is sufficient; upstream's
   replay-system position-forcing was hedging against its own unseeded RNG,
   nothing deeper. No other hidden nondeterminism surfaced in 3600 frames:
   no iteration-order, no wall-clock, no float-mode issues.
2. **Conditions**: (a) seed/replace `Math.random` before page scripts run;
   (b) keep async/cosmetic consumers off the gameplay stream (harness
   patches `percentShake`; a port does this structurally). Virtual clock
   freezing is NOT required for determinism — keep it anyway as hygiene.
3. **Checksum surface that worked**: per player `phys` (whole object),
   `hit`, `timer`, `actionState`, `percent`, `stocks`, `hitboxes`, plus the
   `aArticles` queue. Stable serialization: sorted keys, floats via
   shortest-round-trip `String(x)` (injective on doubles), explicit `-0`,
   cycle-safe. ~10.6 KB/frame for 2 players. Exclude `percentShake` (HUD,
   wall-clock) — it is the only player-object field written off-tick.
4. **Performance**: ~6,900 frames/sec headless including SHA-256 — a
   60-second match verifies in ~0.5 s; the oracle can run per-commit.
5. **Trace format**: per-frame array of full 22-field `Input` objects per
   player, injected at the top of `pollInputs` — replay-accurate without
   touching the input-buffer plumbing. The port should adopt the same
   injection seam (poll boundary), not a lower one.
6. **AI is usable in fixtures**: with a sim-owned seeded PRNG, CPU players
   are fully deterministic, so single-player test scenarios are legitimate
   oracle traces.
7. **Cross-engine risk (not this spike's claim)**: same-browser determinism
   is what's proven. V8-vs-C equivalence is the port's job; the exposure is
   the five functions above. Decide the shared-math strategy before writing
   physics code (per anatomy doc + issue #4).

## Files

- Harness + patch + trace: `spikes/determinism/` (this repo, branch `spike/determinism`)
- Upstream patch: `spikes/determinism/meleelight-harness.patch`
  (4 hooks: `__harness` API + step-driven tick + `pollInputs` injection +
  `percentShake` RNG isolation; ~70 lines total)
