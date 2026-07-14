# meleelight → FunKey-S — PLAN (spec locked 2026-07-14)

The locked, research-backed spec for a faithful port of
[meleelight](https://github.com/schmooblidon/meleelight) (browser JS Melee
remake, upstream pin `27af171`) to the FunKey-S, ready for the autonomous
build loop ([`docs/LOOP.md`](./docs/LOOP.md)) to execute. Every decision
below is backed by a closed wayfinder ticket (#2–#12 on this repo) and the
merged evidence in `docs/research/`, `spikes/`, `prototypes/`. Items tagged
**PROVISIONAL (auto-adopted)** were decided during spec assembly under
delegated judgment; Chase may amend them.

Rules the loop must obey every iteration: [`CLAUDE.md`](./CLAUDE.md).
Licensing: [`docs/LICENSING.md`](./docs/LICENSING.md).

## 1. Goals (locked at charting, 2026-07-13)

1. **Faithful port** — meleelight's gameplay verbatim; deviations are bugs.
   The browser original is the oracle.
2. **Input** — B0XX-style digital→analog layer for the FunKey's buttons;
   core techniques guaranteed; engine untouched.
3. **Hard 60 fps** — simulation AND render at 240×240, every frame.
4. **Private, personal use** — no distribution, ever.
5. **Solo parity is the playable bar** — and meleelight HAS a CPU opponent
   (`src/main/ai.js`, difficulty 1–9), so solo play means menus → VS match
   vs CPU → target test, all faithful (anatomy, #2).
6. Out of scope: netplay, other devices (the SDL2/headless builds are
   tooling, not targets), upstream contributions.

## 2. Locked strategy — S1 (issue #10, measured basis #8)

**Agent-driven, structure-parallel rewrite in C (C99/C11) with an
executed-data pipeline.**

- **Structure-parallel**: the C file/function layout mirrors the JS source
  (`src/physics/environmentalCollision.js` → `sim/environmental_collision.c`
  etc.) so per-frame checksums localize divergences to files/functions.
  Trusted translation surface: ~15–20k LOC of sim code. Port the logic,
  NOT the module graph — `main.js`'s mutable-global god-module flattens
  into one game-state struct (anatomy §3).
- **Executed-data pipeline**: the 31.5 MB data plane (animations, frame
  data, stage geometry) is converted by EXECUTING the original JS and
  serializing the live objects — equivalent by construction, never
  hand-transcribed.
- **Bit-exactness kit** (all mandatory, all measured-necessary):
  - doubles only — never float (Cortex-A7 VFPv4 does IEEE-754 doubles in
    hardware; +,−,×,÷,sqrt are then bit-reproducible);
  - vendored fdlibm for `atan2, sin, cos, atan, pow` (+`tan`: the AI uses
    it and M4 ports the AI) on BOTH sides of the oracle — V8-vs-musl-libm
    divergence was live-confirmed on device (#8). Source: V8's
    fdlibm-derived `ieee754.cc`, vendored at `port/fdlibm/`
    (**PROVISIONAL** — exact source file set fixed at M0);
  - exact-equality checksums — no epsilon, ever;
  - tick-owned seeded gameplay PRNG (**mulberry32** on both sides,
    **PROVISIONAL** — it's what the proven harness uses), advanced ONLY
    inside the sim tick; cosmetic/async consumers get a separate stream
    (the percentShake lesson, #7);
  - `-ffp-contract=off` (no fused multiply-add divergence) on every TU.
- **Oracle runtimes**: browser harness (`spikes/determinism/`) is the
  golden source; an fdlibm-patched QuickJS static build (890 KB, recipe in
  `spikes/device-feasibility/README.md`) is the on-device/host oracle
  runtime for running the ORIGINAL JS where a browser can't go.
- **Rejected on measurement/survey** (`docs/research/porting-strategies.md`):
  shipping interpreted JS (QuickJS sim proxy = 29.3 ms/frame on device,
  ~714× off JIT — dead), AOT routes (Static Hermes, porffor,
  AssemblyScript), wasm carriers (engine-in-wasm, wasm2c). C over C++ for
  ssb64-recipe parity, fdlibm compatibility, and CHECKER analyzability.

## 3. The verification spine (issue #7 — proven, not aspirational)

**Input-only replay is bit-identical** (3600/3600 frames across fresh
browser loads, incl. a CPU player) given the seeded tick-owned PRNG. This
is the project's entire safety story; the loop lives and dies by it.

- **Checksum surface** (frozen formally at M0; the spike-proven set): per
  player `phys` + `hit` + `timer` + `actionState` + `percent` + `stocks` +
  `hitboxes`, plus the `aArticles` queue. Stable serialization: sorted
  keys, shortest-round-trip float strings (injective on doubles), explicit
  `-0`, cycle-safe; SHA-256 per frame (~10.6 KB/frame serialized).
  `percentShake` is excluded (HUD-only, wall-clock — the one off-tick
  field).
- **Injection seam**: top of `pollInputs` — full 22-field `Input` objects
  per player per frame. The C port keeps the same seam.
- **Trace format**: `spikes/determinism/README.md` §Trace format.
- **Transcendental exposure**: only atan2/sin/cos/atan/pow (+tan for AI);
  sqrt is IEEE-exact everywhere. Shared fdlibm ⇒ exact equality.
- **Cost**: ~6,900 sim-frames/sec headless including hashing — a 60 s match
  verifies in ~0.5 s. Oracle runs per-commit; there is no excuse to skip it.
- **Sound events**: the port intercepts inline `sounds.*.play()` calls as
  an emitted sound-event queue — an additional checksummable stream
  (anatomy §7) and the audio seam (§7 below).

## 4. Milestone ladder (issue #11; gate table lives in CLAUDE.md §Gates)

Rungs: **M0 → M1 → M2-CAL → M2 → M3 → M4**. Loop phases use exactly these
ids (`fix_plan.md` `Current phase:`). Gate discipline per ssb64: each
milestone's EXIT GATE is an exact runnable command in `CLAUDE.md §Gates` —
concrete now where the tooling exists, else a precise definition that
REPLAN concretizes when the milestone becomes current. Per-task
`done-check:`s are separate and never substitute for the exit gate.
CHECKER owns all verdicts (writer ≠ checker).

### M0 — Oracle hardened

Productionize `spikes/determinism/` into `oracle/`:

- Seeded-PRNG + harness patch maintained against the upstream pin; pinned
  clone recipe (docker `node:8` build — the proven one).
- **fdlibm landed both sides**: JS implementations shim `Math.*` in the
  browser harness (pins the oracle against browser drift) AND `port/fdlibm/`
  C sources with a cross-check test (identical bit patterns JS↔C over a
  sweep + the golden traces' full call streams).
- fdlibm-patched QuickJS oracle runtime built (recipe from
  `spikes/device-feasibility/README.md`, Math table repointed at fdlibm)
  and checksum-verified against the browser on a golden trace.
- Trace recorder + **golden trace set covering all 5 characters and all 6
  VS stages** (human + CPU traces; the existing 3800-frame Fox/Marth/
  Battlefield trace is golden #1), committed with frozen checksum streams.
- Checksum spec frozen as a documented struct-field list (`oracle/CHECKSUM.md`).

EXIT (definition): every golden trace replays bit-identically across two
fresh browser runs, matches its committed checksum stream, and the QuickJS
runtime reproduces it. Seed command exists today (see CLAUDE.md §Gates).

### M1 — Data pipeline

Executed-JS serialization of the data plane, run offline on the host:

- Animations: all 754 files / ~27.9k paths / 5 chars serialized from the
  live `window.animations` global into **packed int16 bezier-path binaries**
  — the exact format the measured device renderer consumed
  (**PROVISIONAL** concretization of #11's "pre-rasterized/packed": ship
  packed paths, because on-device tessellation is measured-affordable at
  2.54 ms worst-case; offline pre-rasterization to 1-bit RLE masks stays a
  registered fallback if vfx stacking ever blows the render budget).
- Frame data / attributes / ECB / hitbox constants / stage geometry →
  generated C tables (stage art == collision data, one source of truth).
- SFX → 22050 Hz mono S16 PCM blobs; music → 22050 Hz stereo S16 raw PCM
  files for SD streaming (per #12); ogg/wav stay repo-side source formats.
- Every generated artifact carries a manifest with content hashes.

EXIT (definition): two fresh pipeline runs are hash-identical
(byte-stable), and coverage counts match the anatomy inventory (754
animation files, ~27.9k paths, 5 characters, 6 VS stages, ~180 SFX + 8
tracks). REPLAN concretizes the command on entry.

### M2-CAL — Calibration slice (ENTRY RUNG of M2, hard go/no-go)

Translate exactly ONE pure-math sim module —
`src/physics/environmentalCollision.js` (1,343 lines, no god-module
tentacles) — to C. Drive both sides with the recorded per-frame
inputs/outputs of that module over the 3800-frame golden trace; measure the
**divergence burn-down** to bit-identical.

Deliverables: divergences-per-KLOC, fix-rate (divergences resolved per
iteration), projected effort for the remaining ~14–19k LOC — a real
schedule estimate before committing the loop to the grind.

**Go/no-go (PROVISIONAL definition)**: GO = the slice reaches bit-identical
over the full trace AND the burn-down converged (strictly decreasing
divergence count, no oscillation) — the loop records the metrics in
`docs/AGENT-LOG.md` and proceeds. NO-GO = it cannot converge without
epsilon-cheating or the projection is absurd (>10× the M2 budget) — the
loop STOPS with sentinel `LOOP STOP: m2-entry-no-go` for Chase.

### M2 — Sim core checksum-locked, headless

The full structure-parallel C rewrite of the sim (physics, characters/moves,
articles, input interpretation, AI stays JS-side for now — ported in M4),
running on host (SDL2 + headless CI backends), fed by M1 data, replaying
golden traces.

EXIT (definition): every golden trace replays through the C sim
**bit-identical to the browser oracle's committed checksum stream, full
match length**. Divergence localization procedure documented (structure
parallelism = binary-search by module/frame).

### M3 — On-device at 60 fps  ⚠ includes HUMAN ESCALATION

SDL 1.2 device backend, S1 input table (§6), OPK packaging, device conformance:

- Platform seam per CLAUDE.md §SDL policy (three backends, one TU each);
  render 240×240 16bpp native, AA scanline fill (§5).
- Input: FunKey letter-keysym events (`u/d/l/r/a/b/x/y/s/k/n`, MENU=`q`)
  → S1 mapping table → final Melee-unit coordinates at the poll seam.
- OPK via the SDK container's **mksquashfs 4.4 only** + launcher-script
  pattern; ADB deploy/test rig (envelope doc §2).
- Device checksum conformance: golden traces replayed ON DEVICE (uinput
  injector or trace-fed build) match the oracle streams.

EXIT (definition): device checksum conformance + **p99 frame time
< 16.67 ms** over a full replayed match with audio on + OPK launches from
the frontend + a live played session. **HUMAN ESCALATION** (sentinel
`LOOP STOP: m3-device`): physical device work needs the FunKey plugged in
(loop proceeds autonomously over ADB when present, halts when not), and the
gate INCLUDES Chase's hands-on **S1 input-scheme ratification playtest**
(deferred from #9) — ratify or amend the mapping before M4.

### M4 — Full-game parity

- Menus/CSS/stage-select/options as a **thin C front-of-house**: faithful
  flows and canvas look at 240×240, rewritten not transliterated (they are
  jQuery+DOM hybrids — anatomy §8); menus are NOT checksummed, matches are.
- **Audio per #12's measured verdict**: SDL 44100 Hz / S16LSB / stereo /
  512-sample buffer; 8-voice mixer + music; SFX pre-decoded in RAM
  (22050 mono); music pre-decoded PCM streamed from SD (88.2 KB/s, 2×64 KB
  double-buffer); driven by the sim's sound-event queue.
- **CPU opponent**: port `ai.js` (adds `tan` to the fdlibm surface; AI
  inputs flow through the same input bank, so CPU matches are checksummable
  golden traces — proven deterministic in #7 exp. C).
- **Target test** mode (solo staple; target-stage data via M1 pipeline).
- Persistence: settings/records to SD (tmpfs is wiped at power-off).

EXIT (definition): full-game golden-trace conformance (menu flows scripted,
match + target-test traces checksum-conformant on device at 60 fps with
audio) + **Chase acceptance playthrough** (the solo-parity bar, sentinel
`LOOP STOP: m4-complete`). Passing this closes the project's build phase.

### Sequencing rationale (from #11)

Device bring-up before the sim port was already retired as a risk by the
feasibility spike (#8) — so the cheap, unblocking work (oracle, data) goes
first and the device rung consumes a de-risked renderer. Folding M4 into M3
was rejected: muddier gate.

## 5. Renderer + asset pipeline (measured basis: #8, anatomy §6/§8)

- **Model**: meleelight draws single-colour filled bezier-path silhouettes
  + vector stages + vfx fills — no textures, no images. The port renders
  the same content natively at 240×240 into the device's 16bpp surface
  (**PROVISIONAL**: render directly in RGB565/surface format — kills
  ssb64's per-pixel LUT conversion; verify actual format at init, bail if
  not 16bpp).
- **Measured budget** (real device, real animation data): worst variant
  (AA on, tolerance 0.25 px, zoom 2) = **2.54 ms avg / 3.21 ms p99** for
  clear + stage + 2 chars + 4 projectiles + flip, vs an 8 ms render
  allowance — 2.5× margin, ~14 ms/frame left for sim+audio+OS. Present
  floor (clear+flip) is 0.87 ms. AA is affordable (~2.7× non-AA raster
  cost); ship quality tolerance 0.25 px is essentially free.
- **Technique** (what `spikes/device-feasibility/rastbench.c` measured, to
  be re-used structurally): flatten cubics to polylines (adaptive, tol
  0.25 px), nonzero-winding scanline fill via active-edge table, 4× vertical
  subsample AA with fractional span-end coverage, 565 blend; x-mirroring
  for facing; stage-space→screen = `pos*scale + offset` retargeted to
  240×240. Zero allocations in the frame loop.
- **Camera/zoom**: browser camera logic ports verbatim (sim-side); the
  measured zoom-2 bound covers the worst realistic character size.
- The 5 stacked browser canvases collapse to ONE buffer (they exist for
  browser dirty-tracking, not visuals). Screen-shake as a blit offset;
  `percentShake` HUD wobble stays cosmetic and off the sim PRNG.
- **Design rules from the envelope data** (60 fps engineered in, not
  optimized in): span fills; fixed-point/integer interpolants where
  bit-exactness permits (rasterization is NOT checksummed — only sim state
  is — so the renderer may use floats/fixed-point freely); no per-pixel
  calls/branches; `-O2` everywhere, `-O3` only on the hot raster TU;
  keep the wall-clock-sim/skippable-render frameskip loop as a safety
  valve (~30 lines) even though we expect never to skip.
- **Asset residency**: only the selected 1–2 characters' animation data
  needs RAM residency; all-5 packed data ≈ 15 MB worst case anyway, and the
  RAM envelope (~26 MB available above a 22 MB RSS precedent) absorbs it.

## 6. Input — S1 "One-Mod + C-layer" (issues #6, #9; prototype verified)

Locked scheme (PROVISIONAL per #9 — Chase ratifies hands-on at the M3
gate; browser rig available anytime: `prototypes/control-mapping/serve.sh`).

**Roles**: d-pad = stick · A = attack · B = special · X = jump ·
**L = Mod** · **R = shield** (R+A = grab) · **Y (hold) = C-stick layer** ·
Start = pause · Fn reserved by FunKey OS.

**Contract**: a data-driven coordinate/role table (schemes S2/S3 = config
swaps) injecting FINAL Melee-unit values, quantized to 1/80, at the
`pollInputs` seam — bypassing `tasRescale` (the documented saturation
trap). SOCD = neutral (moot on the cross d-pad). `tapJumpOffp1 = true`
(digital up = 1.0 would tap-jump every upward DI). Left stick goes neutral
while the C-layer is held (drift freezes). Digital shield emits `r=true,
rA=1.0`; no light shield (single-stage triggers).

**S1 coordinate table** (Melee units; all values checked against
meleelight's engine thresholds — deadzone 0.28, walk floor 0.3, dash/f-smash
0.79, u/d-smash + tap-jump 0.66, crouch −0.69, fastfall −0.65, shield-drop
band (−0.70, −0.65]):

| Chord | Emitted coordinate | Purpose |
|---|---|---|
| d-pad cardinal | ±1.0 | dash, smash (flick auto-passes), DI, spotdodge/roll in shield |
| d-pad diagonal | (±0.7000, ±0.7000) | diagonal DI, 45° firefox, 45° wavedash (with R) |
| L + horizontal | ±0.6625 | walk / f-tilt band |
| L + vertical | ±0.5375 | u-tilt / d-tilt (clear of crouch −0.69, fastfall −0.65) |
| L + diagonal | (±0.7375, ±0.3125) | shallow firefox / drift ≈23° |
| L + R + down-diagonal | (±0.6375, −0.3750) | shallow wavedash ≈30° |
| R + down-diagonal | (±0.7000, −0.6875) | **shield drop** (and still a legal wavedash angle) |
| R + straight down | (0, −1.0) | spotdodge; wavedash-in-place |
| Y-layer + d-pad | csX/csY: cardinals ±1.0, diagonals (0.7, 0.7) | C-stick smashes/aerials/ASDI |

Techniques guaranteed (all 15 chord checks pass headless; dash 1.0 and
walk 0.6625 observed in a live match): dash/dash-dance, walk, all
tilts-vs-smashes, jab, short/full hop (authentic KNEEBEND timing), wavedash
45°/30°/in-place, shield drop, spotdodge/rolls, grab + JC grab, cardinal +
diagonal DI, C-stick aerials, charged smashes. Known sacrifices (accepted):
steep Mod-Y angle family, light shield, drift-during-C-stick, dedicated Z.
Quirk registry: `prototypes/control-mapping/README.md` §Design notes
(L+R+cardinal emits plain Mod values; C-layer diagonal csY 0.7 ≥ 0.66
resolves as u/d-smash grounded).

## 7. Audio (issue #12 — measured on device)

- SDL 1.2/ALSA config: **44100 Hz, AUDIO_S16LSB, 2 ch, 512 samples**
  (11.6 ms callback, ~23 ms latency ≈ 1.4 frames; the smallest buffer clean
  under 80 % CPU load — 256 is NOT safe; fall back to 1024 only if real
  hitches appear).
- Mixer: 8 mono 22050 Hz SFX voices (16.16 fixed-point resample, Q8 gain)
  + 1 pre-decoded stereo music stream; int32 accumulate + clamp. Measured:
  **1.25 % of the frame budget**, zero underruns under load.
- SFX pre-decoded to RAM (low single-digit MB); music pre-decoded OFFLINE
  to 22050 Hz stereo S16 raw PCM, **streamed from SD** (88.2 KB/s, 2×64 KB
  double-buffer, ~120–130 MB SD for all 8 tracks; SD reads measured
  non-disruptive at 21.3 MB/s O_DIRECT). Ogg never ships to the device.
- Sim → audio via the sound-event queue (§3); the callback owns mixing.

## 8. Device envelope + toolchain facts the loop needs

Full detail: `docs/research/funkey-envelope.md`. Headlines: single
Cortex-A7 @1.2 GHz (~20 M cycles/frame at 60 fps), 64 MB DDR2 (RAM is NOT
the constraint: ≤32 MB footprint target vs ~26 MB measured-available
precedent), no GPU, 240×240 16bpp, SDL 1.2, buttons arrive as letter
keysyms. Toolchain: docker `jondbell/funkey-s-sdk` →
`arm-funkey-linux-musleabihf-gcc` (gcc 10.2, musl, hard-float; targets A7
by default); exact commands in `CLAUDE.md §Commands`. Top gotchas
(mirrored in CLAUDE.md): mksquashfs 4.4 only; `sh -lc` login shell or
SDL_Init dies; log to tmpfs not SD; screenshot from inside the app, never
the fb device; serial docker builds.

## 9. Risk register (post-research; what killed what)

| Risk | Status |
|---|---|
| Software vector rendering at 60 fps | **DEAD** — measured 2.54 ms worst vs 8 ms allowance (#8) |
| Interpretation as a shipping strategy | **DEAD** — 29.3 ms/sim-frame measured (#8) |
| Input-replay determinism | **PROVEN** conditional on seeded tick-owned PRNG (#7) |
| Cross-engine float divergence | **CONTAINED** — five-function fdlibm surface, live-confirmed necessary (#7, #8) |
| Audio path | **DEAD** — 1.25 % of budget measured (#12) |
| RAM | **DEAD** — envelope measured (#5) |
| **V8-vs-C divergence burn-down rate** | **THE remaining unknown** — gated by M2-CAL before the loop commits to the 15–20k LOC grind |
| Input scheme feel | Deferred to Chase's M3 ratification playtest (#9) |
| Upstream toolchain rot (webpack 1 / node 8) | Contained — docker `node:8` recipe proven twice; upstream pinned at `27af171` |

## 10. Evidence appendix (all merged onto main)

| Ticket | Doc | Artifacts |
|---|---|---|
| #2 anatomy | `docs/research/meleelight-anatomy.md` | — |
| #3 license | `docs/research/meleelight-license.md` | `LICENSE-meleelight`, `NOTICES` |
| #4 strategies | `docs/research/porting-strategies.md` | — |
| #5 envelope | `docs/research/funkey-envelope.md` | — |
| #6 b0xx | `docs/research/b0xx-mapping.md` | — |
| #7 determinism | `docs/research/determinism-spike.md` | `spikes/determinism/` (harness, patch, trace) |
| #8 feasibility | `docs/research/device-feasibility-spike.md` | `spikes/device-feasibility/` (rastbench, qjsmin, logs, frames) |
| #9 control proto | `prototypes/control-mapping/README.md` | `prototypes/control-mapping/` (rig, patch, boot-check) |
| #12 audio | `docs/research/audio-spike.md` | `spikes/device-audio/` (audiotest, logs) |
| #10 strategy lock, #11 ladder | resolutions on the issues; folded into §2/§4 | — |
