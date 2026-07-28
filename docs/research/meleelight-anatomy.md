# meleelight anatomy — what we are actually porting

Research for issue #2. Investigated against a fresh clone of
`schmooblidon/meleelight` (HEAD `27af171`, 2018-05-30 "falcon vfx"). All file
pointers below are paths into that clone (`src/...` relative to the repo
root). The clone lives in scratch only — not vendored here.

## TL;DR verdicts

| Question | Verdict |
|---|---|
| Sim/render separable? | **Yes, ~90%.** Fixed-tick sim (`setTimeout` 16ms) and rAF render are already separate loops; `src/physics/` has zero DOM/jQuery. Entanglements: sim calls Howler `sounds.*.play()` and `drawVfx()` inline, and `main.js` is a god module everything circularly imports. |
| Main loop | Fixed ~60Hz logic tick via `setTimeout(gameTick, 16)` (`src/main/main.js:1138`), render on `requestAnimationFrame` (`main.js:1153-1154`) with an optional 30fps half-rate mode. No accumulator — tick is wall-clock-paced but frame-counted. |
| Determinism | Core is per-frame and integer-timer driven, but **~7 `Math.random` sites touch gameplay/AI state** (list below), and replays hedge by force-writing positions each frame. Transcendental float math (`Math.sin/cos/atan2/pow`) is the cross-platform checksum hazard. |
| Rendering | Canvas 2D only (5 stacked 1200×750 canvases). Characters are **single-colour filled bezier-path silhouettes** per animation frame; stages are hand-authored `Vec2D` line-segment/polygon data. No images for gameplay entities. |
| CPU opponent | **Yes.** `src/main/ai.js` (1575 lines), difficulty slider 1–9 on the character select screen. Heuristic, works for all 5 characters via `generalAI`. |
| Input analog-native? | **Yes** — sim consumes GC-style analog axes (`lsX/lsY/csX/csY/lA/rA`). Better: the keyboard path already **synthesizes analog from digital keys** with range/modifier scaling — exactly the model our button→analog synthesis needs. |
| Framework lock-in | None. No engine, no React, no minified blobs. jQuery + Howler are the only runtime deps; jQuery is confined to menus/UI chrome. |

## 1. Repo and build system

- Build: webpack 1 + Babel (`es2015` + `stage-0`) with partial Flow type
  annotations (`.flowconfig`, `// @flow` headers). Configs in
  `bin/webpack/{build,dev,animations}.config.js`.
- Two bundles: the game (`src/index.js` → imports everything) and a separate
  **animations bundle** (`src/animations.js`) that assigns
  `window.animations = [marth, puff, fox, falco, falcon]` — split out because
  it is 30MB of source and Babel is skipped for it (`package.json` babel
  `ignore`). The game reads animation data through this global
  (`src/main/render.js:83-90`).
- Runtime deps: `howler` (audio), `jquery` (menus/DOM). Dev deps include
  Electron packaging, an Express server (`app.js`), and
  deepstream.io/peerjs for the (half-built) online modes.
- Host page: `dist/meleelight.html` — 5 gameplay canvases + ~60 DOM ids of
  menu/HUD chrome (buttons, gamepad SVGs, debug counters).

## 2. Size / structure map (where the 2.2MB+ of JS lives)

1,418 JS files, ~34MB of `src/`:

| Dir | Size | Lines | Role |
|---|---|---|---|
| `src/animations/` | **30MB**, 754 files | data | Per-character, per-action-state vector frames: `module.exports = [frame, ...]`, each frame an array of `Int16Array` bezier paths (`src/animations/fox/APPEAL.js`). ~27.9k path arrays total; fox alone ≈1.47M int16 coords (≈3MB binary), all 5 chars ≈15MB as raw int16, far less RLE'd (see §8). **Data, trivially transformable — not code.** |
| `src/characters/` | 2.3MB | 33,656 | Per-character move/action-state logic + hitbox data. One file per action state (e.g. `src/characters/fox/moves/UPTILT.js`) exporting `{name, init, main, interrupt}` driven by integer `player[p].timer`. `attributes.js`, `ecb.js` per char. |
| `src/main/` | 872K | 11,990 | `main.js` (1,788 ln god module: game modes, loops, global state), `render.js` (555), `ai.js` (1,575), `player.js` (state object), `sfx.js`/`music.js`, `replay.js`, `multiplayer/`, `util/` (Vec2D, Box2D, deepCopy...), `vfx/`. |
| `src/physics/` | 212K | 5,183 | The sim core: `physics.js` (1,233), `environmentalCollision.js` (1,343, ECB vs surfaces), `hitDetection.js` (1,120), `actionStateShortcuts.js` (621), `article.js` (639, projectiles), `interpolatedCollision.js`. Zero DOM/jQuery in the whole dir. |
| `src/menus/` | 172K | ~4.4k | Start screen, main menu, CSS (char select, 1,263 ln), stage select, options, credits — canvas-drawn but jQuery-glued. |
| `src/input/` | 164K | 2,591 | `input.js` (Input type + polling), `meleeInputs.js` (GC-axis scaling/deadzone), `gamepad/` (per-pad calibration DB). |
| `src/stages/` | 152K | — | Hand-authored stage geometry + `stagerender.js`. |
| `src/target/` | 76K | — | Target test play + builder. |

Engine you'd actually port (physics + characters + player/util + input
interpretation + stage data): roughly **50k lines of readable, unminified
ES6**, plus the animation data as a pure asset pipeline problem.

## 3. Architecture and the main loop

- **Logic tick**: `gameTick(oldInputBuffers)` (`src/main/main.js:909`)
  dispatches on a global integer `gameMode` (0 start screen, 1 menu, 2 char
  select, 3 versus, 4 target builder, 5 target play, 6/7 stage selects,
  10-14 option menus, 20 startup). In-game (mode 3), per tick:
  `resetHitQueue → movingPlatforms → destroy/executeArticles →
  [interpretInputs + update(i)] ×4 → checkPhantoms → hitDetect ×4 →
  executeHits → article hits → matchTimerTick` (`main.js:1039-1080`), then
  `saveGameState(input)` (replay capture) and re-arms
  `setTimeout(gameTick, 16, input)` (`main.js:1136-1138`).
- **Render tick**: `renderTick()` (`main.js:1153`) self-schedules with
  `requestAnimationFrame`, clears layers and draws
  background/stage/players/articles/vfx/overlay from the *live* global state
  (no snapshot/interpolation). An `fps30` flag renders every other rAF.
- **Consequence for us**: the sim is already a frame-counted step function
  over an input buffer; nothing in the tick reads wall-clock for gameplay
  decisions (all `performance.now()` in `gameTick` feeds debug counters).
  Replacing `setTimeout` pacing with a hard 60Hz loop is safe.
- **Downside**: `main.js` is a god module — ~350 files import from it and it
  imports back (circular). State is module-level mutable globals
  (`player[4]`, `gameMode`, `playerType`, `characterSelections`...). A port
  should flatten this into one game-state struct; the *logic* transliterates
  cleanly, the *module graph* should not be copied.

## 4. Simulation model (what state exists)

- `playerObject` (`src/main/player.js`): `phys` sub-object with positions,
  velocities (`cVel`/`kVel` knockback), ECB diamonds (`ECB1/ECBp`), shield
  HP/analog, ledge boxes, ~80 scalar flags/timers; plus `hit` (hitlag,
  knockback), `timer` (action-state frame counter, drives both logic and
  animation frame selection), `actionState` string, hitboxes.
- Action states: table `actionStates[charId][stateName]` (built in
  `src/physics/actionStateShortcuts.js`); each state's `main(p, input)` runs
  once per tick and reads the 8-deep input buffer (`interpretInputs`,
  `main.js:662` — buffered analog history for dash/smash detection, with
  pause-aware offset logic).
- Hit resolution is queue-based (`hitQueue` in `physics/hitDetection.js`,
  executed after all players update) — good, order-deterministic.
- Projectiles ("articles": fox/falco LASER, ILLUSION, etc.) in
  `src/physics/article.js` with their own queues.
- Melee-accurate mechanics knobs live in `src/settings.js`
  (`gameSettings`: turbo, l-cancel type, phantom threshold...).

## 5. Determinism audit

134 `Math.random` hits in 32 files; almost all cosmetic (vfx particle
positions, screen shake, menu sparkles). The ones that matter:

**Gameplay-state randomness (must seed/replace with PRNG in port):**
- `src/characters/shared/moves/CAPTUREWAIT.js:26` —
  `player[p].phys.pos.x += 0.5*Math.sign(Math.random()-0.5)` (grab-struggle
  wiggle mutates real position).
- `src/main/ai.js` — AI decisions: e.g. `ai.js:1258`
  (`randomSeed = Math.floor(Math.random()*30)+1`), miss-tech rolls
  (`ai.js:1004`), plus several branch rolls. Any CPU player is nondeterministic.
- `src/physics/actionStateShortcuts.js:18,43,65,87,109` — random KO
  shout selection (audio-only, but an event-stream divergence).
- `src/main/main.js:1309` — `setBackgroundType(Math.round(Math.random()))`
  (visual only).

**Wall-clock leakage:**
- `percentShake` (`main.js:361-367`) writes player-visible HUD state via
  `setTimeout(...,20/40/60)` — HUD-only (`render.js:422-425`) but is
  wall-clock, as is `screenShake` (`main.js:352-359`, canvas transform).
- `performance.now()` (~20 sites) — debug timing, replay frame-delay
  bookkeeping, controller menu; none feed sim state. `Date.now`/`getTime`
  only for cookies and replay filenames.

**Float semantics:** all math is JS doubles. Cortex-A7 (VFPv4) does IEEE-754
doubles in hardware, so +,−,×,÷,sqrt are bit-reproducible if we keep double
precision (do NOT drop to float). The hazard is transcendentals —
`Math.sin/cos/atan2/pow/exp` appear throughout the sim (12 in
`hitDetection.js` — launch angles; 8 in `article.js`; 7 in `physics.js`;
`render.js` `rotateVector`) and are implementation-defined. For the
browser-oracle checksum plan we must ship the same implementations on both
sides (e.g. port V8's fdlibm-derived routines to C, and shim the browser to
use those exact JS routines — or checksum with an epsilon-tolerant
comparison).

**Telling detail:** the replay system doesn't trust input-only determinism —
`saveGameState` records inputs *and* per-frame positions, and playback
force-overwrites `player[i].phys.pos` every frame
(`src/main/replay.js:62-91,188`). Consistent with the random sites above.
Our per-frame checksum harness is therefore also an upstream-bug-finder:
after seeding the RNG sites, input-only replay should converge.

## 6. Rendering

- Canvas 2D everywhere; contexts created at `main.js:1552-1560` over 5
  stacked 1200×750 canvases (BG1 static bg, BG2 stage, FG1 shake-transform
  layer, FG2 players/vfx, UI HUD) — `dist/meleelight.html`.
- Characters: `renderPlayer` (`src/main/render.js:72`) picks
  `animations[charId][actionState][frame-1]` (frame = `floor(player.timer)`,
  clamped by `framesData`), chooses **one fill colour** (palette, flash
  states, shield/intangible blink — `render.js:121-164`) and calls
  `drawArrayPathCompress` (`render.js:37-68`): each `Int16Array` is
  `[startX, startY, then 6-tuples of cubic bezier control points]`, filled as
  one path with x-mirroring for facing. So a character frame = flat
  single-colour silhouette. Sword trails, shields, vfx drawn as separate
  paths/shapes (`src/main/vfx/`).
- Stages: pure vector data (`src/stages/vs-stages/battlefield.js` —
  `polygon/platform/ground/ceiling/wallL/wallR/ledge/blastzone` as `Vec2D`
  lists, plus per-stage `scale` and pixel `offset`), drawn by
  `stagerender.js`. Collision consumes the same data — geometry and art are
  one source of truth. Stage-space→screen transform is just
  `pos*scale + offset` (`render.js:73-74`), trivially retargetable to 240×240.

## 7. Input, audio, content

**Input** (`src/input/input.js`): `Input` record = 12 buttons + 6 analog
axes + 4 raw axes (`input.js:16-37`). Polling paths: gamepad (Web Gamepad
API with per-controller calibration DB in `src/input/gamepad/`, GC-axis
rescale in `meleeInputs.js`), **keyboard** (`pollKeyboardInputs`,
`input.js:148-237`: digital keys → ±1 axes, per-direction range settings,
5 modifier keys scaling X/Y — e.g. walk/tilt modifiers — then
`tasRescale`+`deaden`), replay, network, and AI (AI writes into
`aiInputBank`, consumed identically to human input — `input.js:128-129`).
Z→(A + lightpress lA=0.35) synthesis at `input.js:221-227`. **Port note:**
the keyboard path is a complete, tuned precedent for synthesizing Melee
analog values from FunKey buttons.

**Audio**: Howler. ~180 SFX `Howl`s from wav files (`src/main/sfx.js`),
8 music tracks (`src/main/music.js`); `dist/sfx` 7.1MB, `dist/music` 29MB.
Sim code calls `sounds.X.play()` directly inside action-state logic
(e.g. `characters/fox/moves/UPTILT.js:39`) — the port should intercept this
as an emitted sound-event queue (also gives us an extra checksum stream).

**Content inventory**:
- Characters (5): Marth, Jigglypuff, Fox, Falco, Captain Falcon
  (`src/main/characters.js:2-8`).
- VS stages (6): Battlefield, Dreamland, FD, Fountain of Dreams, Pokémon
  Stadium, Yoshi's Story (`src/stages/vs-stages/`); 10 target-test stages +
  user-built ones.
- Modes: local VS (1–4 players, any mix of humans/CPUs; CPU difficulty
  slider on CSS — `src/menus/css.js:72,316-329`), Target Test (timers,
  records, medals), Target Builder (with shareable stage codes,
  `src/stages/encode.js`), options menus (audio/gameplay/keyboard/controller
  calibration), credits, replays (record/save via pako+localforage,
  `src/main/replay.js`), and half-built online play (deepstream/peerjs
  spectate/P2P/server, `src/main/multiplayer/` — ignore for the port).
- No training mode per se, but frame-by-frame stepping and debug/hitbox
  display exist (`frameByFrame` in `main.js`).

## 8. Harder / easier than expected

**Easier:**
- No engine/framework lock-in; no minified or generated-opaque blobs; the
  30MB animations dir is machine-formatted but transparent data.
- Physics/sim core is DOM-free, frame-counted, queue-ordered — near-ideal
  for a straight C transliteration + fixed-timestep loop.
- Sim and render already run on separate clocks; render never writes sim
  state (only reads), so a 240×240 SDL1.2 renderer swaps in cleanly.
- Input already analog-in-shape with a keyboard→analog synthesis precedent.
- Stage art == collision data; one vector format for everything.
- Single-colour silhouette characters: on FunKey we can pre-rasterize every
  animation frame to 1-bit masks (palette-tinted at blit time) offline —
  bezier tessellation never needs to run on device. Raw anim data ≈15MB as
  int16; RLE 1-bit sprites at 240p scale will be a fraction of that, and only
  the 1–2 selected characters need residency in 64MB.
- CPU opponent exists, so single-player on a handheld is viable day one.

**Harder / gotchas:**
- `main.js` god-module with ~350 files importing its mutable globals;
  circular imports everywhere. Port the logic, not the structure.
- Sim entanglements to sever: inline `sounds.*.play()` (180 sites-ish),
  inline `drawVfx()` from physics (`src/physics/physics.js:31` import), and
  wall-clock `setTimeout` shake effects.
- Gameplay `Math.random` sites (§5) must be replaced with a seeded PRNG
  before the checksum oracle can work — including patching the *browser*
  side to match, since the oracle needs the same seed.
- Transcendental libm divergence vs V8 — decide the shared-math strategy
  early (fdlibm port both sides is the safe call).
- Replays force-sync positions — upstream never proved input-only
  determinism; expect to find and fix latent divergences ourselves.
- Menus/HUD are jQuery+DOM+canvas hybrids positioned in absolute 1200×750
  pixels — rewrite, don't port, the front-of-house.
- Old toolchain (webpack 1, Babel stage-0, Flow, node 8) — running the
  browser oracle needs either the prebuilt `dist/` page or a pinned-ancient
  node; plan for a docker/nvm pin.
- Per-frame silhouettes are unique (no skeletal reuse): asset pipeline must
  handle ~27.9k paths; budget flash, not RAM.

> **CORRECTION (A8 audit, 2026-07-27):** this bullet is FALSE as measured
> — 8 of 13 upstream menu files contain ZERO `document.` and ZERO jQuery
> (the 9th has one `console.log(document.cookie)`); the entire live DOM
> surface is 3 out-of-scope features (controller SVG calibration, css tag
> input, targetselect paste box) plus one DEAD file (startScreenPrompt.js,
> 0 importers); and the HUD half is disproven inside our own tree by
> gfx_overlay.c, which WAS transliterated using browser-captured glyphs.
> This bullet must NOT be cited as a rewrite justification (it was, at
> fix_plan.md §M4 conventions and port/foh/foh.h:3-5). Evidence:
> .loop/reuse-audit/REPORT.md.


## 9. Pointers index (quick reference)

- Loop: `src/main/main.js:909` (`gameTick`), `:1138` (`setTimeout 16`),
  `:1153` (`renderTick`/rAF)
- Sim step order: `src/main/main.js:1039-1080`; per-player `update`:
  `main.js:895-902` → `src/physics/physics.js`
- Input type/polling: `src/input/input.js:16-37,122-138`; keyboard analog
  synthesis `:148-237`; buffering `src/main/main.js:662`
- Character move template: `src/characters/fox/moves/UPTILT.js`
- Animation data format: `src/animations/fox/APPEAL.js`; consumed at
  `src/main/render.js:37-68,72-90`; global via `src/animations.js`
- Stage format: `src/stages/vs-stages/battlefield.js`
- AI: `src/main/ai.js:874` (`runAI`), difficulty use `:1004,1215,1258`
- Replay: `src/main/replay.js:62-123` (record), `:180-194` (playback
  position forcing)
- Determinism sites: `CAPTUREWAIT.js:26`, `ai.js:1258`,
  `actionStateShortcuts.js:18-109`, `main.js:352-367,1309`
- Audio: `src/main/sfx.js`, `src/main/music.js`
- Settings/mechanics knobs: `src/settings.js`
