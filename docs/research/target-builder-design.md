# A45 — the target builder: what it is, and how to ship it in pieces

Research lane R, ticket **A45** (`docs/FEATURES-SPEC.md` §6/§8/§9). Owner:
*"we need target builder to be implemented (if it exists in the source code
we're basing our port on that is)."* It exists.

**VERDICT: the ticket's headline framing is wrong in a way that changes the
plan.** A45 is not "a 58 KB jQuery+DOM editor must be rewritten for a d-pad".
Upstream's target builder is **already a gamepad editor** — 9 of its 1606
lines touch the DOM, and all 9 are in the *save/share* overlay. What actually
blocks A45 is not the UI at all: it is that **the port's target plane is keyed
by an integer stage id end to end**, in both the sim and the renderer, so
there is nowhere to put a stage that does not come from the pipeline.

That relocates the risk, and it relocates the first shippable piece. The
smallest useful thing is not a screen. It is a **codec** — and the second
piece, still with no editor at all, already gives the owner custom target
stages he can play.

## Method and provenance

- Upstream read at the pinned clone `27af171`,
  `$MELEELIGHT_CLONE = /Users/chase/.cache/meleelight-funkey-s/upstream`,
  **read-only** — nothing in it was modified.
- `src/target/targetbuilder.js` was read **in full** (all 1606 lines, two
  passes). `src/stages/encode.js` (258), `src/stages/targetselect.js` (557),
  `src/target/targetplay.js` (289) and `src/target/util/getConnected.js` (89)
  likewise. Nothing was sampled.
- Where a claim is marked `MEASURED` I **executed** the upstream code. Method:
  `babel-core` from the clone's own `node_modules` transpiled `encode.js` and
  its five dependencies into a scratch directory (never the clone, never the
  repo) and the functions were run under node 22. This is the `fmt_diff`
  discipline applied early — *never transcribe the reference; execute the
  original's own bytes* — and it falsified two things I had written down from
  reading alone.
- `CITED` = read at a named `file:line` this session, not executed.
  `ASSUMED` = a judgement call, flagged as one.
- **No device runs.** No repo file outside this document was touched.
- Licensing: nothing was vendored or copied. Upstream's own code translated
  into C is meleelight, already covered by `LICENSE-meleelight`; it needs no
  `NOTICES` entry. No new third-party dependency is proposed anywhere below.
  Custom stages are player-authored geometry, not Nintendo-derived assets —
  but the project distributes nothing regardless.

---

## 0. Four premises to correct before scheduling anything

Recorded rather than silently fixed, per `gc-adapter-usb1-routing.md`'s
convention.

**P1 — "jQuery+DOM editor → 240×240 C, exactly as the FOH was" is wrong.**
`MEASURED` (grep over the whole file): `targetbuilder.js` contains exactly
**nine** DOM/jQuery lines — `:804-807`, `:814`, `:822`, `:826`, `:848`,
`:850`. Every one of them is inside the pause menu's *Save stage* arm or the
*showing code* dismissal. The editor itself reads `input[p][0].lsX/lsY`
(`:175-176`) and the face/shoulder/d-pad buttons, and draws to the same canvas
`ui` context every other screen uses. There is **no mouse anywhere in the
file.** The FOH's rewrite burden (mouse semantics → hand cursor) does not
recur here; what recurs is only the *scale* problem (1200×750 → 240×240).

**P2 — the function named `createTargetCode` is dead, and it is not the share
code.** `MEASURED` (grep over `src/`): `createTargetCode`
(`targetbuilder.js:83-115`) has **zero callers**. It also cannot work: it
reads `stageTemp.box` (`:86`) and `stageTemp.startingPoint.x` (`:85`), but
`stageTemp` has no `box` key and its `startingPoint` is an *array* of `Vec2D`
(`:53-72`, `:62`). It is a fossil of an older stage model. The live share code
is `createStageCode` / `parseStageCode` in **`src/stages/encode.js`**, called
at `targetbuilder.js:803` and `targetselect.js:157,546`. Anyone who greps for
"target code" and finds `createTargetCode` will port the wrong 33 lines.

**P3 — `+ ADD CODE` is the *last* piece worth shipping, not the first.**
The slot's whole job upstream is to accept a ~1 KB string **pasted into an
HTML textarea** (`targetselect.js:132-136`, `:156`). The FunKey-S has no
clipboard, no keyboard, no network, and its on-screen letter grid (D8) is
blocked behind A14. Entering a 1 KB code on a d-pad grid is on the order of a
thousand button presses. §3 proposes the transport that actually fits the
device.

**P4 — the blocker is the sim/renderer stage plane, not `foh.c`.**
`CITED`: `tp_stage_from_ttab1(int tstageId, MlStageX *out)` and
`tp_setup_target(GameState *g, int charId, int tstageId)`
(`port/sim/target/target_play.h`), and `gfx_target_init(Gfx *g, int tstageId,
int backgroundType)` which does `g_tt = &ml_tstages[tstageId]`
(`port/gfx/gfx_target.c:81-92`). Both planes accept **an id into a
compile-time TTAB1 table** and nothing else. A stage the player authored has
no id. Good news in §5: the structs behind those functions are already plain
runtime structs, so the fix is a new filler, not a redesign.

---

## 1. What the editor actually does

`targetBuilderControls(p, input)` — `targetbuilder.js:159-855` — is the whole
machine. It runs in gameMode 4, driven by one player slot (`targetBuilder`,
`:25`, set by `targetselect.js:118`).

### 1.1 The document under edit

`stageTemp` (`:53-72`) `CITED`. Fifteen keys:

| key | what | edited by |
|---|---|---|
| `polygon`, `polygonMap` | closed shapes + each shape's owned surface indices | POLYGON, DELETE, MOVE |
| `platform` | one-way platforms | PLATFORM |
| `ground`, `ceiling`, `wallL`, `wallR` | collision surfaces | WALL, POLYGON (derived), DAMAGE (props), DELETE, MOVE |
| `target` | breakable target centres, **cap 20** (`:563`) | TARGET, DELETE, MOVE |
| `startingPoint` | 4 spawn points, defaults `±10, ±30` at y=0 (`:62`) | MOVE only |
| `ledge` | `[type, index, side]` triples | LEDGE |
| `blastzone` | `Box2D([-250,-250],[250,250])` (`:64`) | **never edited — no tool** |
| `scale` | 3, clamp `[2,6]` (`:65`, `:747`, `:757`) | SCALE |
| `offset` | `[600,375]` (`:66`) | **never edited** |
| `connected` | recomputed by `getConnected(stageTemp)` after every structural edit (`:410`, `:457`, `:510`, `:613`, `:735`) | derived |
| `background.polygon`, `background.line` | the no-collision decorative plane | any tool while `drawMode` (`:44`, `:765-770`) |

### 1.2 The ten tools

`toolInfo` (`:36`) `CITED`, cycled by **L/R shoulders or d-pad left/right**
(`:213-230`) with wrap, each change playing `menuSelect` and arming a 120-frame
name toast:

| # | tool | lines | essential? |
|---|---|---|---|
| 0 | **Polygon** | `:277-411` | **Essential.** Click-per-vertex; closes within 2 canvas px of the origin (`:292`); rejects self-intersection via `intersectsAny` (`:298`) and plays `deny`; needs ≥3 points; computes winding from signed area (`:313-318`) and classifies **every edge** into ground / ceiling / wallL / wallR by its angle (`:351-367`), recording the mapping in `polygonMap`. B pops the last vertex (`:396-408`). Cap 120 polygons (`:280`). |
| 1 | **Platform** / **Line** | `:412-458` | **Essential.** Hold-A drag. Needs ≥10 canvas px of *width* (`:429`); rejects `|angle| > π/6` with a "Bad angle" toast (`:441-446`). In `drawMode` becomes a background line, with a Manhattan-length test instead. |
| 2 | **Wall** | `:459-512` | **Essential.** Hold-A drag; `wallType` cycled with d-pad up/down (`:231-248`). Manhattan length ≥10 (`:476`); an L-wall within 2 world units of any R-wall (or vice versa) is refused with "Walls too close" (`:484-492`); ground/ceiling need `|angle| ≤ π/6`, walls need a non-axis angle (`:493`). |
| 3 | **Ledge** | `:513-541` | **Essential.** Hovers the nearest platform/ground, picks the nearer *end*, A toggles a ledge there. |
| 4 | **Damage** | `:542-559` | **Essential but risky** — see §5.3. Hovers a surface, A toggles `surface[2] = {damageType}`; type cycled with d-pad up/down (`:249-267`), one of `fire/electric/slash/darkness` (`:32`). |
| 5 | **Target** | `:560-571` | **Essential — and the entire point of a *target* builder.** A drops a target at the crosshair; `deny` at 20. |
| 6 | **Move** | `:572-618` | **Essential.** Hover priority `startingPoint → target → polygon → line` (`:582-590`), then hold-A drag. `centerItem` (`:1542-1590`) snaps points/targets to the crosshair and *offsets* line endpoints; moving a polygon also offsets every surface `polygonMap` says it owns. |
| 7 | **Delete** | `:619-738` | **Essential**, and the most intricate arm in the file: deleting a surface must renumber every `polygonMap` entry and every `ledge` index above it, and drop ledges that pointed at it (`:641-690`, `:697-728`). |
| 8 | **Scale** | `:739-764` | **Incidental.** Zooms the world→canvas factor in 0.1 steps every 6 frames, clamped `[2,6]`. Freezes the crosshair while active (`:172-174`). |
| 9 | **Draw Mode** | `:765-770` | **Incidental.** Toggles the background (no-collision) plane; while on, tools 2–4 are forced to 1 (`:269-273`). |

### 1.3 The rest of the control surface

- **Crosshair** (`:171-201`): `unGriddedCrossHairPos += ls * multi * 3 / scale`,
  where `multi` is 1 when **X or Y is held** and 5 otherwise (`:171`) — the
  precision modifier. Clamped so the *rendered* position stays inside
  1200×750.
- **Grid** (`:80`, `:134-152`, `:207-212`): `Z` cycles `gridSizes =
  [80,40,20,10,0]`; index 4 is free movement. Snapping is done in world units
  with a `600 % gridSize` / `375 % gridSize` phase term.
- **Pause menu** (`:774-777`, `:780-844`): START opens it (`pause` sound);
  three rows — **Test stage** → `startTargetGame(targetBuilder, true)`,
  **Save stage** → §3, **Quit** → `changeGamemode(1)`.
- **Sounds used** `MEASURED` (grep): `menuSelect` ×11, `menuBack` ×8,
  `blunthit` ×7, `menuForward` ×4, `deny` ×3, `pause` ×1. `blunthit` and
  `pause` are **not yet referenced anywhere in `port/`** `MEASURED`, but both
  exist upstream (`sfx.js:202`, `:216`) and therefore in the pipeline's 180
  mapped sounds — a name lookup, not new work.
- **`undo()` (`:116-129`) is dead** `MEASURED` (grep over `src/`): zero
  callers, and the only thing ever pushed to `undoList` is `"target"`
  (`:565`) — the platform/ledge pushes are commented out (`:448`, `:500`,
  `:536`). **Do not port it.** If the owner wants undo, it is new design work,
  not a translation.
- **`renderTargetBuilder` (`:974-1451`) at line `:1297`** reads a bare global
  `animations[...]` to draw a character silhouette while the SCALE tool is
  active. See §7 risk R5.

### 1.4 Essential vs incidental, stated as a cut list

**Essential** — tools 0–7, the crosshair, the grid, the pause menu, and the
save. **Incidental** — SCALE (8), DRAW MODE (9) and the whole `background`
plane, the `hoverToolbar` fade (`:202-206`), the toolbar's 10 rounded-rect
icons with their per-tool preview stacks (`:1143-1291`, ~150 lines of canvas
path work that a 240-wide screen cannot show anyway), and the three
positional toasts. **Dead** — `createTargetCode`, `undo`.

---

## 2. What each interaction becomes on this device

Upstream's canvas is 1200×750. The port's camera is fixed:
`GFX_K = 0.2`, `GFX_DY = 45` (`port/gfx/gfx.h:42-43`) `CITED` — so the entire
upstream canvas lands as a **240×150 letterboxed band**, and *nothing needs
panning*. One device pixel = 5 canvas px = `5/scale` world units (1.67 at the
default scale 3).

| upstream | on the FunKey-S | note |
|---|---|---|
| crosshair driven by `lsX/lsY` | **d-pad, via `foh_hand_step`** (`port/foh/foh_hand.h`) | Already the exact shape: doubles, integrated per frame, clamped to the canvas. `FOH_CURSOR_VX/VY` is the calibration knob. **But `foh_hand_step` clamps to a rect; the builder's clamp (`:182-201`) also *rewrites* `unGriddedCrossHairPos` so the ungridded and gridded positions stay consistent.** That is a second position, not a second clamp — `foh_hand.h` supplies the motion, the builder owns the grid projection. |
| `X`/`Y` held = fine movement (`:171`) | same two buttons | D33 moved X to grab and Y to special *in match*; the builder is a menu plane and can bind freely. Owner ratification, not a technical constraint. |
| `Z` cycles the grid (`:207`) | needs a button | `z` is the alternate smash button in-engine (`/CONTEXT.md`) and is free on a menu screen. |
| L/R shoulders cycle the tool (`:213-230`) | **unchanged** | The tool selector is *already* shoulder-driven. This is why the 10-icon toolbar is incidental: replace it with one centred tool name plus the existing 120-frame toast. |
| d-pad up/down cycles wall/damage type (`:232-266`) | conflicts with the cursor | The cursor takes the d-pad. **Rewrite:** put type cycling on the shoulders *while a modifier is held*, or on a second press of the tool button. Registered deviation; owner-visible. |
| hold-A drag (tools 1, 2, 6) | unchanged | A held is A held. |
| the 10-icon toolbar (`:1143-1291`) | **cut** | 10 icons across 116 device px = 11.6 px each. Below legibility. Replaced by the tool-name toast, which upstream already draws (`:1150-1159`). |
| hand / red-X / crosshair sprites (`:1416-1432`) | keep | `foh_hand`'s CSS hand sprite already exists; the red X and the plus-crosshair are two rect fills. |
| pause menu, 3 rows of 400×100 (`:1434-1450`) | 3 rows in the FOH text style | `foh_pause.c` is the finished template. |
| "Too small" / "Bad angle" / "Walls too close" toasts drawn **at the cursor** (`:1395-1415`) | keep, but bottom-anchored | An 80×25 canvas box is 16×5 device px — it cannot hold text at the cursor. Move to a fixed status line. |
| the `showingCode` overlay (`:845-853`) | **does not exist** | §3. |

`foh_hand.h` applies to: the crosshair (motion + clamp). It does **not** apply
to hit-testing — `foh_hand_hit` answers "which of these N rects", and the
builder hovers *geometry*, using the already-ported `distanceToLine` /
`distanceToPolygon` (§5.4).

---

## 3. The share code — the highest-value answer

### 3.1 What it is

`src/stages/encode.js`. `createStageCode(stage) → string` (`:11-108`) and
`parseStageCode(code) → Stage | null` (`:183-258`). **It is a serialisation of
the entire stage, not of the target layout** — targets are field 11 of 14.

Grammar: fields separated by `&`, records within a field by `~`, numbers
within a record by `,`. Every number is `Number.prototype.toFixed(2)`
(`:14`, `:38`, `:77`, `:99`, `:105`, `:106`), so the emitted alphabet is
exactly `-?\d+\.\d\d`, plus the small integers of fields 1 and 10.

| # | field | record shape |
|---|---|---|
| 0 | `startingPoint` | `x,y` |
| 1 | `startingFace` | `1` or `-1`, comma-separated; **`1,1,1,1` when the stage has no `startingFace` key** (`:20-22`) |
| 2–6 | `ground`, `ceiling`, `wallL`, `wallR`, `platform` | `x1,y1,x2,y2,d` — `d` ∈ 0..4 = none/fire/electric/slash/darkness |
| 7 | `background.line` | same |
| 8 | `polygon` | `x,y,x,y,…` (flat) |
| 9 | `background.polygon` | same |
| 10 | `ledge` | `<g\|p>,index,side` — only the **first character** of the type name is written (`:90`) and read (`:150-155`) |
| 11 | `target` | `x,y` |
| 12 | `blastzone` | `minx,miny,maxx,maxy` |
| 13 | `scale` | one number |

`parseStageCode` returns `null` on `code.length < 14`, on any thrown error, or
on a missing starting point (`:208-210`, `:247-255`) — that null is what draws
"Invalid code" (`targetselect.js:158-162`).

### 3.2 Is it round-trippable? Yes — measured, with two caveats

`MEASURED` — a builder-shaped `stageTemp` (1 polygon, 1 platform, 7 damage-
carrying wallR surfaces, 2 targets, 4 starting points, 2 ledges, a background
polygon and line) was encoded, parsed, and re-encoded:

```
ROUNDTRIP c1 === c2 : true
```

**The code is the canonical form**: the first encode is lossy (`toFixed(2)`
quantises to hundredths), every encode after that is exact. Two caveats, both
measured, both upstream bugs to be carried or registered:

**Caveat A — the 6th surface of every type silently loses its damage type.**
`encode.js:39` reads `if (i !== 5)`, where `i` is the **surface index within
the type**. It was plainly meant to be `n !== 5` (skip the damage digit for
`background.line`, which is type index 5). Measured, seven fire wallR
surfaces in, seven out:

```
in : fire|fire|fire|fire|fire|fire|fire
out: fire|fire|fire|fire|fire|    |fire
                                ^ index 5
```

**Caveat B — a decoded stage has `polygonMap = [null, …]`** (`encode.js:244`)
`MEASURED`. Every polygon↔surface link is gone. The consequence, `CITED` at
`targetbuilder.js:1566` and `:698`: re-editing an imported stage lets you drag
a polygon's *outline* away from its *collision surfaces*, and deleting a
polygon orphans them. Upstream guards against crashing on this and nothing
more.

### 3.3 Two more measured facts that change how it should be built

**It parses without `strtod` — and it must.** The project has banned decimal
parsing on device since iter 38: the SDK's static musl `strtod` mis-rounds
subnormals by one ulp and drops `-0`, which is exactly why `foh_persist`
carries doubles as hex16 bit patterns (`foh_persist.h:96-97`) `CITED`. But the
code's alphabet is `-?\d+\.\d\d` only. Parse the digits into an **integer
number of hundredths** and divide by `100.0`: for any magnitude this format
can hold, the integer is exact and IEEE division is correctly rounded, so the
result is bit-identical to a correct `strtod`. **~20 lines, no libc, no risk.**
A stricter grammar than `parseFloat` also *is* the "Invalid code" path
upstream already displays.

**Emitting needs `toFixed(2)`, which the port does not have.** `ml_fmt.c`
implements `String(x)` (ECMA-262 §6.1.6.1.20) over vendored Ryu; `toFixed` is
a different algorithm (§21.1.3.3). Two options: (a) implement it — ~60 lines
on top of Ryu, with a ready-made oracle (the `fmt_diff` differential rig
already compares C against V8); or (b) emit `String(x)` instead — `parseFloat`
accepts it and it is *more* precise, but our codes would then differ byte-wise
from upstream's for the same stage. **Recommend (a)**, because byte-identity
against upstream's own executed `createStageCode` is the only mechanical judge
this plane can have.

### 3.4 Where codes should actually come from on this device

`ASSUMED` (a design proposal, and the one place this document is asking for an
owner ruling):

A share code is for moving a stage *between machines*. The FunKey-S has an SD
card, and the owner mounts it. So: **custom stages live as one text file per
slot in the persist directory** — `custom0.mlstage` … `custom9.mlstage`, one
code line plus a `SUM` line, published through `foh_persist.c`'s existing
atomic `tmp → fsync → rename` path. Dropping a code in from a browser session
becomes "copy a file onto the SD card", which is *better* than upstream's
paste box, costs nothing, and needs neither a letter grid nor A14.

That leaves the `+ ADD CODE` slot with a real job — **listing the code files
it found** — and leaves on-screen code *display* (upstream's `showingCode`)
as an optional late ticket that nobody has to wait for.

---

## 4. Persistence

### 4.1 What upstream persists

`CITED`: cookies only, 36500-day expiry.
- `custom0` … `custom9` — the **code strings**, written at
  `targetbuilder.js:811` and `:818` (save) and `targetselect.js:164` (add
  code); re-read at boot by `getTargetStageCookies` (`targetselect.js:542-556`).
  Hard limit 10 (`:817`, `:102`).
- `<char>target<slot>` — target-test records, slots **0..19**, i.e. custom
  slots have personal bests too (`targetplay.js:40` is `[5][20]`).
- Delete/dupe **shift the cookies** (`targetselect.js:83-97`).

### 4.2 An upstream defect the port must decide about

`MEASURED` — the in-memory accumulation at `targetselect.js:164-166` (and
identically at `:551-552`) is

```js
customTargetStages[customTargetStages.length - 1] = stage;
setCustomTargetStages(customTargetStages.length, customTargetStages[...]);
```

Executed with three successive adds:

```
after A : ["A"]
after B : ["B","B"]     <- A is gone
after C : ["B","C","C"] <- B survives only in slot 0
```

**Every added code clobbers the previously added stage in memory**, on both
the add path and the boot-reload path. The *cookies* are written correctly, so
the data survives on disk and is destroyed again on the next load. There is no
oracle for this plane (custom stages appear in no golden; `CHECKSUM.md` covers
players and articles only), so HARD RULE 5's tiebreak does not apply
mechanically. **Owner ruling needed: carry it verbatim, or register a
deviation and fix it.** Recommend fixing — it makes the feature unusable — and
registering it as such.

### 4.3 What `foh_persist` needs

`CITED` — `foh_persist.h`: `MLFKPERSIST5`, exactly 68 LF lines, strict
anchored grammar, `SUM` over all preceding bytes, doubles as hex16, migration
from v1–v4 by prefix.

- **Do not put stage codes in `mlfk-persist.dat`.** Ten codes can be tens of
  KB against a file that is currently ~2 KB and is rewritten on every settings
  change, and the load path's failure mode is a **loud reset to defaults** —
  putting player-authored content behind that blast radius risks the owner's
  50 personal bests. Separate files (§3.4), same publish discipline.
- **One bump is genuinely owed: `rec` rows 50 → 100.** `targetRecords` is
  `[5][10]` in `FohState` because custom slots were scope-excluded
  (`foh_persist.h`); upstream's is `[5][20]`. That is a pure append after the
  existing rec block, so v1–v5 stay strict prefixes and the single parser
  still serves them; migration fills the new rows with `-1.0`
  (`bff0000000000000`). **v6, and nothing else.**

---

## 5. What it needs from the sim — and this *is* a separate plane

### 5.1 The blocker, precisely

`CITED`: `tp_setup_target(g, charId, tstageId)` /
`tp_stage_from_ttab1(tstageId, out)` (`port/sim/target/target_play.h`) and
`gfx_target_init(g, tstageId, bg)` (`port/gfx/gfx_target.c:81`). Both index a
generated table. The builder's *Test stage* arm is `startTargetGame(p, true)`
→ `setActiveStageBuilderTestStage(stageTemp)` (`targetplay.js:180-182`), which
the port explicitly excluded: *":181 test arm: BUILDER plane (stageTemp) —
scope-excluded; test is hardwired false on this path"*
(`port/sim/target/target_play.c:284-285`) `CITED`.

### 5.2 The good news

`MlStageX` (`port/sim/physics.h:74-89`) `CITED` is a plain runtime struct —
five `SurfaceList`s, `connected` pairs, ledges, blastzone, respawn points. It
is not generated and not const. So the sim change is **one new filler
function**, `tp_stage_from_custom(const MlkStage *, MlStageX *)`, plus a
`tstageId < 0` custom arm in `tp_setup_target` and `gfx_target_init`. It is
small — but it is squarely in `port/sim/` and `port/gfx/`, **not** lane M, and
it must be called out as its own plane exactly as the ticket says.

### 5.3 Four hard limits that a player can exceed

| limit | value | where | builder can produce |
|---|---|---|---|
| surfaces per type | **64** | `ML_MAX_SURFACES`, `port/sim/stage_types.h` | 120 polygons × ≥3 edges each (`targetbuilder.js:280`) |
| concatenated labelled surfaces | **96** | `ML_MAX_LABELLED_SURFACES`, same file | same |
| ledges | **16** | `ML_MAX_LEDGES`, `port/sim/physics.h:66` | unbounded (`:535`) |
| targets | **10** | `ML_MAX_TARGETS`, `target_play.h` — `_Static_assert`-tied to upstream's 10-element `targetDestroyed` literal (`targetplay.js:37`) | **20** (`targetbuilder.js:563`) |

The target row is a genuine upstream/port asymmetry: JS arrays grow, so a
20-target custom stage plays fine upstream and would overflow
`bool targetDestroyed[10]` here. `tp_setup_target` already "dies LOUDLY
outside 1..cap", so the port fails safe — but the *builder* must refuse the
11th target, or the caps must rise. **Owner-visible either way.** Raising the
caps costs RAM inside `MlSim`; that is host-measurable with one `sizeof`
print and was not measured here.

### 5.4 The one plane that has never run

`MEASURED` (`pipeline/lib/targets-schema.js:25`, `:101`): **`damageType`
exists on zero authored stages**, VS or target. `CITED`
(`port/sim/physics.h:41-43`): `dealWithDamagingStageCollision`'s hitQueue
pushes are *"zero-live on VS stages … M4 target stages are the reachable
future domain"*. The C is translated (`physics.c:202-203`, five call sites)
and **has never been exercised by any golden.** The builder's DAMAGE tool is
what makes it live. That ticket owes its own recorded golden.

### 5.5 What is already ported (and cuts the estimate)

`MEASURED` (`port/sim/util/`): `intersectsAny`, `distanceToLine`,
`distanceToPolygon`, `lineDistanceToLines` all exist in
`detect_intersections.h`; `manhattanDist` / `euclideanDist` in `lin_alg.h`;
`extremePoint` in `extreme_point.h`. All four `detectIntersections` exports
are M2-task-1 replay-verified (`port/sim/calib/expected-capture-util.json`,
`distanceToPolygon` 2739 live records on g01). **Every geometry primitive the
builder needs already exists and is proven.** The only missing helper is
`getConnected` (89 lines, built from `extremePoint` + `manhattanDist`).

---

## 6. Ticket breakdown

Ordered. Each row is independently shippable and independently provable. Sizes
are `ASSUMED` estimates calibrated against A7 (≈422 lines ≈ 1 iteration).

**T1 — `ml_stagecode`: the codec and the custom-stage value model.**
`port/sim/stagecode/{ml_stagecode.c,h}` — `MlkStage` (the runtime stage the
builder edits and the sim plays), `mlk_encode()`, `mlk_parse()`, the
strtod-free hundredths parser (§3.3), and `ml_to_fixed(x,2)`.
*Proves:* `port/sim/stagecode/check-stagecode.sh` → `STAGECODE MATCH` —
differential against **upstream's own `encode.js` executed from the read-only
clone** (transpiled by the check, never transcribed; the `fmt_diff`
discipline) over (a) all 10 authored TTAB1 stages re-encoded, (b) a generated
corpus covering every field including the `i !== 5` and `polygonMap = null`
quirks, (c) `encode(parse(c)) == c` idempotence. Zero UI. **Touches no lane-M
file.** ~1 iteration.

**T2 — custom stages PLAY, with no editor at all.**
`tp_stage_from_custom` + `gfx_target_init` custom arm + cap enforcement (§5.3)
+ boot-time load of `custom*.mlstage` from the persist dir. `foh_persist` v6
(rec rows → 100).
*Proves:* a recorded target-stream golden — `port/goldens-m4/record-target.sh`
and `verify-target-stream.js` already exist — replaying a **custom** stage
whose code the browser oracle loaded through the same `parseStageCode`.
**This is the first player-visible ship, and it is already useful:** drop a
code file on the SD card, play the stage. ~1 iteration.

**T3 — TSS custom slots and their verbs.**
Slots 10..19 with upstream's "Custom N" labels (`targetselect.js:288-294`),
Play / Delete / Dupe (`:80-113`, with the cookie shifting), and `+ ADD CODE`
re-pointed at the file listing. Retires `ev_refused(s,"addcode")`.
*Proves:* a committed flow script + frozen structural trace + shot judge
(`check-foh-flows.sh` pattern). **Lane M — serial behind A7/A14.**
~1 iteration.

**T4 — the editor core, targets only.**
gameMode 4: crosshair + grid + tool cycling + pause menu (Test / Save / Quit),
with tools **5 (Target), 6 (Move), 7 (Delete)** live and restricted to targets
and starting points. Saving writes a `.mlstage` through T1's encoder.
*Proves:* a flow script that places targets, saves, and asserts the emitted
code re-parses to the same `MlkStage`; plus Test stage handing off to T2's
custom play path. **This is a complete, useful target builder** — the "target
test" use case is placing targets, and it is ~15% of the upstream file.
~1–2 iterations.

**T5 — Platform and Wall tools** (`:412-512`): hold-A drag, the length/angle/
proximity guards, the status-line toasts. ~1 iteration.

**T6 — Ledge and Damage tools** (`:513-559`). Carries the §5.4 obligation: a
recorded target golden containing a damage surface, judged against the browser
oracle. ~1–2 iterations, and the second one is the risk.

**T7 — Polygon tool** (`:277-411`) + `getConnected` + the DELETE arm's full
renumbering (`:619-738`). Largest and most intricate; the geometry primitives
are already there (§5.5). ~1–2 iterations.

**T8 — Scale and Draw Mode** (`:739-770` and every `drawMode` branch). Purely
cosmetic; last. ~1 iteration.

**T9 — on-screen code display / entry.** Only if the owner still wants it
after T1–T3. Blocked by D8's letter grid, which is blocked by A14. Unscoped.

**Smallest useful first piece: T1.** It is the only one with zero UI, zero
lane-M contention, and a fully mechanical proof, and every later ticket
depends on the value model it defines. **Smallest player-visible piece: T2.**

---

## 7. Estimate and risks

**8 to 11 iterations** for T1–T8, plus device legs which are owed and not
runnable now (the device is disconnected). That is roughly **8–10× A7**. Any
plan that treats A45 as one ticket is wrong by that factor.

Ordering note: A45 is *not* purely lane M. **T1, T2, T5–T8 touch
`port/sim/` and `port/gfx/` only** and can run beside the lane-M chain; only
**T3** and the FOH-facing half of **T4** need `foh.c` / `foh.h` /
`foh_render.c` and must queue behind A7 and A14.

### Risks, most to least dangerous

- **R1 — the damage plane has never executed** (§5.4). Five translated call
  sites with zero golden coverage, made live by one tool. Mitigation: T6 owns
  a damage golden; if that proves expensive, ship T1–T5 and defer the DAMAGE
  tool.
- **R2 — the caps** (§5.3). A player can author a stage the sim cannot hold.
  Decide *before* T2 whether to refuse loudly at load (cheap, safe, visible)
  or raise the caps (RAM cost inside `MlSim`, host-measurable with one
  `sizeof` print — **not measured here**).
- **R3 — legibility at 240×150.** Grid lines at 2 device px, targets at 5,
  toasts that cannot hold text at the cursor. This cannot be judged from
  source — the project's standing lesson — and needs an owner playtest with a
  `FOH_CURSOR_SPEED`-shaped knob.
- **R4 — the `customTargetStages` clobbering bug** (§4.2). Needs an owner
  ruling before T2, because it decides what "ten custom stages" even means.
- **R5 — `targetbuilder.js:1297` reads a global `animations[...]`** to draw a
  character silhouette under the SCALE tool. **I could not determine** whether
  the port can render an ANIM1 path outside a match. *How to find out:* read
  `gfx_render_player_pass` in `port/gfx/gfx_render.c` and check whether the
  ANIM1 frame data is bound at `gfx_init` or only at match setup; if only at
  match setup, T8 either binds it or drops the silhouette as a registered
  render delta. It is cosmetic either way.
- **R6 — `toFixed(2)`** (§3.3). Real but bounded, with an existing
  differential oracle. If it slips, option (b) ships and byte-identity with
  upstream is registered as lost.

### Things I could not determine, and how to settle each

1. **Does the ANIM1 plane bind outside a match?** — R5 above.
2. **`sizeof(MlSim)` headroom for larger caps.** Host-measurable:
   `cc -Iport/sim -o /tmp/sz` a one-liner printing `sizeof(MlSim)`, then
   compare against the device's measured MemAvailable (a device leg, owed).
3. **Whether the owner wants share codes at all**, given §3.4 makes file
   transport strictly better on this hardware. This is the single question
   whose answer moves the most work (T9, and the D8/A14 dependency, disappear
   if the answer is "files are fine").
4. **Whether upstream's browser build actually reloads more than one custom
   stage correctly.** §4.2 measures the *logic* as broken; I did not run the
   built page. *How to find out:* `oracle/harness/run.js` against the built
   dist with two `custom*` cookies pre-seeded, and read `customTargetStages`.
