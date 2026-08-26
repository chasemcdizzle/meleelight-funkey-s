# A45 T5–T8 — the remaining seven tools, specified before they are built

Companion to `docs/research/target-builder-design.md` (the 2026-08-24 spike).
That document scoped T1–T8 from a cold read; T1–T4 have since shipped
(D39/D42/D43/D50/D51/D52) and the shipped code moves three of the spike's
conclusions. This page re-specifies **T5 Platform+Wall · T6 Ledge+Damage ·
T7 Polygon · T8 Scale+Draw Mode** against what is now on disk.

Every claim is graded **Measured** (I ran it), **Cited** (I read the named
line) or **Assumed**. Upstream line numbers are `src/target/targetbuilder.js`
at pin `27af171` unless another file is named.

---

## 0. THE FINDING THAT RESHAPES THE TICKETS: `getConnected` is not T7's, and
##    it is a live defect the moment T5 or T7 ships

The spike put `getConnected` inside T7 as a helper the polygon tool needs.
**That is wrong on both halves.**

**Measured** — `src/stages/encode.js:237`: `parseStageCode` ends with

```js
stage.ledgePos  = (stage.ledge).map((l) => stage[l[0]][l[1]][l[2]]);
stage.connected = getConnected(stage);
```

So upstream computes `connected` **at parse time, for every custom stage**.
It is not a builder-only bookkeeping field. The port's
`custom_stage.c:82` instead writes

```c
out->hasConnected = false; // no `connected` field in the code grammar
```

whose comment is true about the *grammar* (14 fields, none of them
`connected` — Cited, `stage_code.h`) and false about the *behaviour*:
upstream does not read `connected` out of the code, it **derives** it.

**Why no check caught it.** `check-custom-stage.sh` compares the port's
custom path against the port's TTAB1 path. **Measured** — the ten authored
target stages carry no `connected` key (`pipeline/lib/targets-schema.js:143`
pins its absence), so `target_play.c:88` also writes `false`. Both sides of
the differential are `false`, and the frozen goldens were recorded through
the authored path, so the browser never went through `parseStageCode` either.
This is `/CONTEXT.md`'s **seam** entry exactly: each side's own check passes
while nothing asserts the crossing.

**Why it does not bite today, and why it will bite immediately.**
**Measured** (`scratchpad/conn.js`, upstream's own `getConnected` +
`extremePoint` bodies over all ten authored stages): **zero links on all
ten** — t01..t10 produce `[null,null]` for every ground and platform. And
**Cited** `physics.js:265-300`: an all-null `connected` takes the same arm as
an absent one (`maybeLeftGroundTypeAndIndex === null` → `fallOffGround`). So
on the shipped corpus the divergence is exactly nothing, and the green is
honest.

It stops being nothing the moment a player draws. **Cited**
`getConnected.js:36-47`: a ground whose left extreme coincides (< 0.001
Manhattan) with a **wallR**'s right extreme links as `["r", j]`, which
`physics.js:281-285` turns into `disableFall = true`. **Every polygon the
T7 tool closes emits its ground edges and its wall edges sharing vertices by
construction** (`:351-367` classifies each edge of one closed loop), so a
polygon-built floor upstream stops the player at its lip and the port would
walk them off it. T5's WALL tool reaches the same state by hand.

**Consequence for the tickets.** `getConnected` moves **out of T7 and into
the sim plane, as T5's prerequisite**, at upstream's own site — the
parse→`MlStageX` filler. It is `port/sim/util/get_connected.h` built from the
already-ported `extremePoint` (`port/sim/util/extreme_point.h`) and
`manhattanDist` (`port/sim/util/lin_alg.h`), called from
`tp_stage_from_custom`. **The builder does not need it at all**: `connected`
is not encoded, and upstream's only other consumer is the *Test stage* arm,
which D50 did not port. Upstream's five `stageTemp.connected = getConnected(...)`
assignments (`:410`, `:457`, `:510`, `:613`, `:735`) and its three
`connected[..].splice(...)` calls (`:651`, `:689`, `:714`) are therefore
**dead in this port** and are carried as commented dead arms, not as code —
the `SIDESPECIALGROUNDHIT reads phys.timer` precedent.

**The proof this owes**: a golden whose custom stage HAS a link, replayed
through the browser's own `parseStageCode`. See §5.

---

## 1. What is already on disk (so nothing gets built twice)

**Measured** — `cc -fsyntax-only -Wall -Wextra -Werror` on a TU that includes
only `sim/util/detect_intersections.h` + `sim/util/extreme_point.h` compiles
clean and does **not** drag the generated `ml_stages.h`. So the builder TU
can use the M2-replay-verified geometry directly:

| upstream | already in the port | cap |
|---|---|---|
| `intersectsAny` | `detect_intersections.h:40` | `ML_MAX_LINES` 128 |
| `distanceToLine` | `:82` | — |
| `distanceToPolygon` | `:133` | `PolygonPts` 128 pts |
| `lineDistanceToLines` | `:154` | 128 |
| `manhattanDist` | `lin_alg.h:54` | — |
| `extremePoint` | `extreme_point.h:9` | — |

**Measured** — `di_minimum` over an empty list returns `INFINITY`
(`detect_intersections.h:70-81`), where upstream's fold returns `undefined`.
The one consumer that can see it is WALL's proximity guard
(`:484`, `distanceToOtherWalls !== undefined && < 2`): `INFINITY < 2` and
`undefined < 2` are both false, so the arm is not taken either way. Pinned
here so a later reader does not "fix" it.

The **value model is already complete** (`stage_code.h`): `polygon`,
`bgPolygon`, `bgLine`, `polygonMapIsNull[]`, `ledge`, `scale`, and the
`DamageType` tagged value (`stage_types.h:20-44`) all exist and round-trip
through `mlk_encode`/`mlk_parse`. **T5–T8 add no fields to `MlkStage`.**

**CORRECTED WHILE BUILDING (2026-08-26).** `polygonMapIsNull[]` records the
SHAPE of upstream BUG 2 but not the map's CONTENTS — because a parsed stage
never has any. The builder does, for a polygon drawn this session. That map
therefore lives in `foh_tbuild.c` module state beside `g_doc`, not in
`MlkStage`: the codec could never write it, adding it would move the pinned
`sizeof(MlkStage)` for nothing, and keeping it out reproduces upstream's
observable behaviour exactly — a polygon you drew moves with its surfaces
and one you loaded does not. `map_all_null()` is called at every document
boundary, which is what makes BUG 2 deliberate rather than accidental.

Caps that bind (all Cited, all already `_Static_assert`-cross-checked by
`check-tbuild.sh` leg [1]): `MLK_MAX_POLYGONS` 16 (upstream 120),
`MLK_MAX_POLY_POINTS` 32, `ML_MAX_SURFACES` 64 per list,
`ML_MAX_LABELLED_SURFACES` 96 total, `ML_MAX_LEDGES` 16 (upstream unbounded),
`ML_MAX_TARGETS` 10 (upstream 20).

---

## 2. The control map — three new deviations, pre-registered

D50 rebound the builder for a device with no `z`. T5–T8 add inputs upstream
puts on the d-pad, which **is the crosshair here**, so three more rebinds are
owed. Deviation numbers **D54, D55, D56** (D53 is A26's — Measured, taken).

### D54 — type cycling moves to **X + shoulder**
Upstream cycles `wallType` (`:231-248`) and `damageType` (`:249-267`) with
d-pad **up/down**, *without* freezing the crosshair. Unavailable here.
**X is already the precision modifier** (`:171`, narrowed to X-only by D50),
so: **L/R alone cycles the TOOL (unchanged); X held + L/R cycles the active
tool's TYPE.** Nothing new is bound, X keeps its d-pad role, and the status
line names the current type. Rejected alternative: freezing the crosshair the
way SCALE does — it would cost precision movement on exactly the two tools
(WALL, DAMAGE) that need it.

### D55 — SCALE keeps upstream's d-pad, because upstream already froze the
### crosshair for it
*(Registered. And the freeze turned out to be the thing that was missing from
the first implementation — see §9.)*
**Cited** `:172-174`: `if (targetTool === 8) { multi = 0; }`. The crosshair
does not move while SCALE is active, so d-pad up/down is free **by upstream's
own construction**. T8 is therefore verbatim — this row exists to record that
the conflict was checked and found already solved, not to change anything.

### D56 — B pops a polygon vertex **while one is being drawn**, and leaves
### the builder otherwise
D50 made B the back edge (upstream's builder has no B arm). Upstream's
POLYGON tool binds B to "pop the last vertex" (`:396-408`), guarded by
`if (amDrawingPolygon)`. Those are compatible: **B pops while
`amDrawingPolygon`, and leaves when not.** This is upstream's own guard doing
the disambiguation, and it keeps the back button meaningful — a half-drawn
polygon is unfinished work, so backing out of it before backing out of the
screen is the behaviour a player expects. Registered because it narrows D50.

---

## 3. T5 — Platform and Wall (`:412-512`)

Two hold-A drag tools sharing one shape. **Cited**, arm by arm:

- **initiate** `:415`/`:461` — A edge while not holding; both endpoints set
  to `realCrossHair` (upstream-CANVAS units, not world).
- **stretch** `:421`/`:467` — endpoint 1 tracks the crosshair.
- **release** — endpoint 1 set, then the guards:
  - PLATFORM `:429` — `|x0 - x1| >= 10` in CANVAS px, **or** (drawMode and
    `manhattanDist >= 10`). Note the `||`: in drawMode the width test still
    runs first and can pass alone. Carried verbatim.
  - WALL `:476` — `manhattanDist >= 10`, canvas px, no drawMode alternative.
  - left/right ordering `:431-432`/`:478-479`, then the canvas→world
    conversion `(x - 600)/scale`, `(y - 375)/-scale`.
  - PLATFORM angle `:441` — `|angle| <= π/6` **and** `|angle| >= -π/6`; the
    second conjunct is vacuous (an absolute value is never < -π/6) and is
    carried as written.
  - WALL proximity `:481-492` — an `wallL` within 2 **world** units of any
    `wallR` (or the reverse) refuses; `ground`/`ceiling` never compute it
    and read `undefined` (§1).
  - WALL angle `:493` — ground/ceiling need `|angle| <= π/6`; wallL/wallR
    need `|angle| != 0 && |angle| != π`. **Exact float comparisons**, carried.
  - the `blunthit` on every release, refusal or not (`:453`, `:506`).
- **failure toasts** `badAngleTimer` / `tooSmallTimer` / `wallsTooCloseTimer`,
  all 120 frames — routed to this port's **status line** (`say()`), because
  an 80×25 canvas box is 16×5 device px (design spike §2). Every refusal in
  this screen is a string on screen; that rule is why the ticket exists.
- **`stageTemp.connected = getConnected(...)`** at `:457` and `:510` — dead
  here (§0), commented.

**Caps.** A refused push is a loud status line, never a truncation:
`ML_MAX_SURFACES` per list and `ML_MAX_LABELLED_SURFACES` in total are
checked **before** the push, so the document can never reach a state
`mlk_stage_playable` would refuse at load. This is R2's ruling ("refuse where
the message is useful") applied at the second site.

**State added to `FohState`:** `tbWallType` (0..3, upstream's
`wallTypeList` order ground/ceiling/wallL/wallR), `tbDragX0/Y0/X1/Y1`
(canvas doubles), and the existing `tbHoldA`.

## 4. T7 — Polygon (`:277-411`)

The largest arm, and the one whose faithfulness is hardest to eyeball.

- **start** `:279-287` — cap 120 upstream, `MLK_MAX_POLYGONS` 16 here;
  `drawingPolygon` opens with the origin **twice**.
- **the running edge** `:295` `canClosePolygon` = both `|Δ| < 2` in CANVAS
  units against `drawingPolygon[0]`.
- **self-intersection** `:296-302` — `currentPolygonLines` gains
  `[lg-4, lg-3]` only when `lg > 3 && !denied`; the candidate edge is
  `[lg-2, crosshair]`; when closing, the first line is **sliced off**
  (`:297`). A degenerate zero-length edge also refuses. `denied` is sticky
  and is cleared on the next accepted point — carried, it is observable.
- **closing** `:305-311` — pop the duplicated origin, need `>= 3` points.
- **winding** `:313-318` — signed area by `Σ (x_{i+1}-x_i)(y_{i+1}+y_i)`,
  `direction = sign(area) * -1`; `direction != 0 && direction != -0` is
  written with both spellings and both are the same test in C (`-0 == 0`);
  carried as one test with the note.
- **edge classification** `:351-367` — angle normalised into `[0, 2π)`, then
  ground `<= π/6 || >= 11π/6`, ceiling `[5π/6, 7π/6]`, wallR `> π`, else
  wallL. `polygonMap` records `(list, index)` per edge in the same pass.
- **drawMode** `:322-323`, `:339` — the background polygon plane gets the
  points and **no** surfaces and **no** polygonMap entry.
- **B** `:396-408` — pop, or abandon at `<= 2` points (D56).
- **the hover indicator** `:385-394` — `drawConnectIndicator`, rendered.

**The `polygonMap` null plane is load-bearing.** `stage_code.h` already
models BUG 2 (`encode.js:244`: a parsed stage's map is all-null). A polygon
the player draws *this session* has a real map; one loaded from a slot does
not. MOVE and DELETE must read `polygonMapIsNull[i]` and take upstream's
absent arms (`:1580` guards the MOVE offset with
`polygonMap[item[1]] !== null`; `:696` guards DELETE the same way). Getting
this wrong is silent: the outline drags away from its collision surfaces,
which is exactly the bug upstream has and we are carrying.

**DELETE's renumbering** (`:641-728`) lands here too, because it is only
reachable once surfaces and polygons exist. Four obligations per removed
surface: shift `polygonMap` indices above it, drop ledges that pointed at it,
decrement ledges above it, and (ground/platform only) splice `connected` —
the last dead here. The `polygon` case loops the map and does all of the
above per owned surface.

## 5. T6 — Ledge and Damage (`:513-559`), and the golden it owes

**LEDGE** `:513-541` is small and has no new risk: hover the nearest
`platform`/`ground` via `findLine(..., ignorePolygon = true)`, pick the
nearer **end** by `manhattanDist`, A toggles the `[type, index, side]` triple
(splice if present, push if not), `blunthit` either way. `ML_MAX_LEDGES` 16
is the new refusal.

**DAMAGE** `:542-559` is four lines of logic and one large obligation.
Logic: hover a `wallL/wallR/ceiling/ground`, A sets
`surface[2] = {damageType: <type>}` when absent or different, else
`{damageType: null}`. **Cited** — it writes `{damageType: null}`, it does not
delete the key; the port's `DT_NULL` vs `DT_ABSENT` tags already distinguish
those and `mlk_encode` already emits the digit accordingly.

The obligation is **R1**. `mlk_stage_playable` currently refuses any stage
carrying a real damage type (`custom_stage.c:54-60`), naming T6 as owing the
golden — so **T6 is not done until that refusal is retired against evidence**,
and retiring it without the golden would be exactly the "weakening a check to
make a run pass" that `/CONTEXT.md` names.

**The golden is reachable without touching `oracle/`** (HARD RULE 3 intact).
**Cited** `activeStage.js:82-88`: `setCustomTargetStages(index, val)` and
`setActiveStageCustomTarget(val)` are exported, and `run-target.js:236`
already reaches modules through `window.__wpCache`. So a new recorder arm
does, in the page:

```js
setCustomTargetStages(0, parseStageCode(code));  // upstream's OWN parse
setActiveStageCustomTarget(0);
setTargetStagePlaying(10);                       // MLK_PLAYING_BASE + 0
startTargetGame(0, false);
```

`t03` (Assumed name) is then a hand-authored code carrying (a) at least one
`fire` ground the player is driven into — making
`dealWithDamagingStageCollision`'s five translated-but-never-executed call
sites live for the first time — and (b) **two grounds that connect**, so the
same golden is §0's proof. It is judged by the UNCHANGED
`oracle/harness/verify-stream.js` plus the frozen
`port/goldens-m4/verify-target-stream.js`, like t01/t02.

**If that golden proves expensive, T6 ships LEDGE and stops.** The DAMAGE
tool is absent — not a tool that beeps — and `mlk_stage_playable` keeps its
refusal with its reason. That is the spike's own R1 mitigation and it is the
honest half.

## 6. T8 — Scale and Draw Mode (`:739-770`)

- SCALE `:741-763` — `scaleScroll` increments while the d-pad is held and
  fires every **6th** frame (`> 5`), ±0.1, clamped `[2,6]`, `menuSelect` per
  step. The clamp is applied **after** the add, so 6.0 is reachable and
  6.1 is not stored. Verbatim, with D55's note.
- DRAW MODE `:765-770` — `drawMode = 1 - drawMode` on an A edge, plus the two
  coercions: `:226-227` (cycling forward from 1 skips to 6 while drawMode)
  and `:269-273` (tools 2–4 are forced to 1 while drawMode). With three tools
  in T4 and ten after T7, that mapping must be re-derived against **our** tool
  list, not copied by index — the T4 header already renumbered the tools.
- **R5, the character silhouette** (`:1297`, a bare global `animations[...]`
  drawn while SCALE is active). **Not resolved here.** T8 either binds the
  ANIM1 plane outside a match or drops the silhouette as a registered render
  delta; it is cosmetic and it is the last thing in the ticket, so it does not
  gate anything.

---

## 7. Lanes, split file-disjoint

**Lane G — the sim plane.** `port/sim/util/get_connected.h` (new),
`port/sim/target/custom_stage.{c,h}`, `port/sim/target/check-custom-stage.sh`,
`port/goldens-m4/*` (the t03 recorder arm + golden). Touches **no**
`port/foh` file. Owns §0 and §5's golden.

**Lane E — the editor plane.** `port/foh/foh_tbuild.{c,h}`,
`port/foh/foh.h` (the `tb*` fields), `port/foh/foh_render.c`,
`port/foh/foh_tbuild_witness.c`, `port/foh/check-tbuild.sh`. Touches **no**
`port/sim` file. Owns T5, T7, T8 and T6's LEDGE half.

They meet at exactly one place: T6's DAMAGE arm in lane E is only shippable
once lane G has retired the load-time refusal. Order is therefore
**G then E's damage arm**; everything else in E is independent of G.

Within lane E the order is **T5 → T7 → T6 → T8**: T7's DELETE renumbering
needs T5's surfaces to renumber, T6's LEDGE hover needs T5's platforms to
hover, and T8's drawMode coercion needs the final tool list.

## 8. What this spec does not settle

1. **R5** (§6) — unmeasured, cosmetic, deferred inside T8.
2. **Legibility at 240×150** of grid lines, polygon outlines and the wall-type
   indicator. Not knowable from source; owner playtest, per the standing
   lesson.
3. **Whether `MLK_MAX_POLYGONS` 16 is enough** for a stage a player wants to
   build. Refusing loudly is shipped either way; raising it is an owner
   ruling with a `sizeof(MlSim)` cost, still unmeasured.

---

## 9. What building it changed about this spec

Recorded rather than quietly edited, because a spec that silently agrees with
whatever got built is not evidence of anything.

1. **`polygonMap`'s contents are builder state, not value-model state** —
   §1's correction above.
2. **`getConnected` landed before any tool did**, as its own commit, because
   §0 turned out to be a live defect rather than a T7 dependency.
3. **The tool NAMES had to travel in the view, not as exported symbols.**
   The first version called `foh_tb_tool_name()` from `foh_render.c`, which
   gave that TU an unconditional LINK dependency on `foh_tbuild.c` — the
   exact coupling the `foh_tbuild_ops` pointer exists to prevent. It broke
   eleven witnesses that deliberately do not link the builder, and it broke
   them at the linker rather than at a check, which is the only reason it was
   cheap to find. Anything the renderer needs now comes through `view()`.
4. **D55's freeze (`multi = 0` under SCALE) was missing** from the first T8
   implementation. Nothing about SCALE looks wrong without it — the tool
   zooms correctly — and the crosshair simply drifts while it does. Leg [9]
   caught it; it now has its own tooth.
5. **Three unbounded `while` loops in the witness hung the check** under
   tooth T5, where the builder is unreachable and the state they wait on
   never changes. A loop waiting on a state a tooth can freeze must be
   bounded, always: an unbounded one is not a failing assertion, it is a
   check that never returns.
