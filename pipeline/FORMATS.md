# pipeline/FORMATS.md — generated-data formats the C side implements against

Status: **PROVISIONAL** (auto-adopted, M1 REPLAN iter 9; §4 added iter 11).
Format changes after C consumers exist require a version bump in the magic
and a regeneration of every artifact + expected.json re-freeze in the same
change (same discipline as oracle/CHECKSUM.md §8).

## 0. Global rules (all formats)

- **Byte order: little-endian, always, explicitly.** Host (Apple arm64)
  and device (FunKey-S ARMv7 Cortex-A7) are both little-endian; every
  multi-byte field below is written `writeUInt16LE`/`writeUInt32LE`/
  `writeInt16LE` and MUST be read LE. No native-endian shortcuts in
  generators; the C side may read fields directly after an
  `assert(magic)` because its only target is LE.
- **Executed-JS provenance.** Every artifact is serialized from data
  EXECUTED out of the original built upstream (never hand-transcribed).
  The run manifest records the upstream git HEAD and the sha256 of each
  source artifact consumed.
- **Determinism.** Byte-identical output for identical inputs: state
  names sorted bytewise (ASCII), fixed character order, no timestamps,
  no absolute paths, manifest JSON with recursively sorted keys +
  2-space indent + trailing newline.
- **Integrity.** The manifest (`manifest.json`) lists every artifact
  with `sha256` and `bytes`. Pinned coverage counts live in
  `pipeline/expected.json` (measured-then-frozen from executed runs).

## 1. Manifest (`manifest.json`, schema v1)

One per pipeline run, at the output root:

```
{
  "schema": "meleelight-funkey-s data pipeline manifest v1",
  "upstreamHead": "<full git sha of the upstream clone>",
  "stages": {
    "<stage name>": {
      "format": "<artifact format id, e.g. ANIM1>",
      "sources": [ { "path": "<repo-relative>", "sha256": "..." } ],
      "coverage": { ...stage-specific integer counts... },
      "artifacts": [
        { "path": "<output-relative>", "sha256": "...", "bytes": N, ... }
      ]
    }
  }
}
```

Keys are emitted sorted at every level; artifact lists are sorted by
`path`. `coverage` values are exact integers (see expected.json).

## 2. ANIM1 — packed int16 bezier-path animation binaries

One file per character: `anim_<id>_<name>.bin` with ids/names
`0 marth · 1 puff · 2 fox · 3 falco · 4 falcon` (upstream
`window.animations` order == the oracle harness char indices).

Source model (upstream `dist/js/animations.js`, executed):
`animations[charId][stateName]` = array of frames (1-based frame *n*
rendered from index *n−1*); each frame = array of paths; each path =
`Int16Array` laid out `[startX, startY, then 6-tuples of cubic-bezier
control coords (c1x c1y c2x c2y x y)*]` — consumed by upstream
`render.js drawArrayPathCompress`. Coordinates are in model space;
facing is applied by mirroring x at draw time; y is screen-down (the
renderer applies its own sign/scale).

All offsets below are **absolute byte offsets from file start**. All
fields little-endian. Layout, in file order:

### 2.1 Header — 16 bytes

| off | type | field |
|---|---|---|
| 0 | 4 bytes | magic `"MLA1"` (0x4D 0x4C 0x41 0x31) |
| 4 | u16 | version = 1 |
| 6 | u16 | charId (0–4) |
| 8 | u32 | stateCount |
| 12 | u32 | fileSize (total bytes; integrity check) |

### 2.2 State directory — stateCount × 12 bytes, at offset 16

Entries sorted **bytewise ascending by state name** (plain ASCII, so
`strcmp` order — binary-searchable by name on the C side):

| type | field |
|---|---|
| u32 | nameOff — absolute offset of NUL-terminated ASCII state name |
| u32 | frameCount |
| u32 | framesOff — absolute offset of this state's frame-offset table |

### 2.3 String table

Concatenated NUL-terminated state names, directory order, zero-padded
to a 4-byte boundary.

### 2.4 Frame-offset tables

For each state (directory order): `frameCount` × u32, each the absolute
offset of that frame's record, or **0 = frame absent** (upstream data
CAN be sparse; renderer treats absent as "skip draw" — measured: 0
absent frames in the current pin, the case is still specified). 4-byte
aligned by construction.

### 2.5 Frame records

Concatenated, 2-byte aligned (every record is a whole number of 16-bit
words; the section starts 4-aligned):

```
u16 pathCount
repeat pathCount times:
  u16   coordCount            (measured max 650; assert ≤ 65535)
  int16 coords[coordCount]    (verbatim copy of the source Int16Array)
```

Path shape contract (renderer semantics, not enforced by the container):
`coordCount ≥ 2` and `(coordCount − 2) % 6 == 0`. The serializer counts
violations into `coverage.irregularPaths` (measured: 0) but copies data
verbatim either way — faithfulness over prettiness.

### 2.6 Reference implementations

- Encoder + decoder: `pipeline/lib/animbin.js` (`encodeAnim`,
  `decodeAnim`). Every pipeline run round-trips each binary through the
  decoder and compares every state name / frame count / absent-frame
  position / coord against the executed source data before the artifact
  is accepted.
- The C reader implements THIS document; on disagreement between code
  and this spec, the spec + a regenerated artifact set win.

### 2.7 Coverage reconciliation (why "744 states" == anatomy's "754 files")

Measured against pin `27af171`: `src/animations/` holds 754 files =
744 exported action-state data files + 5 `index.js` aggregators + 5
dead falco files never required by `falco/index.js` and referenced
nowhere (`ILLUSIONFX`, `THROWNDOC{BACK,DOWN,FORWARD,UP}`). The executed
global therefore exposes exactly 744 states (marth 155 · puff 154 ·
fox 142 · falco 148 · falcon 145), 27,820 frames, 27,808 paths,
7,747,148 int16 coords. `check-expected.js` re-derives this
reconciliation live against the upstream src tree on every check run.

## 3. CTAB1 — generated C engine tables (attributes / framesData / intangibility / ECB / hitboxes)

Artifacts per pipeline run: `ml_tables.h` + `ml_tables.c` (the C tables the
sim links against) and `tables.json` (the canonical executed-JS model the
tables were generated from; doubles carried as bit patterns). Generator:
`pipeline/stages/tables.js`. Source: the extractor bundle
`dist/js/extractor.js`, built from the REAL upstream data modules
(per-character `attributes`/`ecb` files registering into
`src/main/characters.js`) by `pipeline/extractor/build-extractor.sh` with
upstream's own docker node:8 webpack toolchain, then executed under a
`window` shim in node (pure data construction — Vec2D/createHitbox object
literals and constant arithmetic; no `Math.*`, no DOM — engine-neutral,
same PROVISIONAL basis as the animations stage).

### 3.1 Value encoding (the whole point)

- **Doubles are stored as IEEE-754 uint64 bit patterns**, emitted as
  `UINT64_C(0x…)` with a human-readable shortest-round-trip decimal in an
  adjacent comment (the port/fdlibm value↔bit-pattern pairing convention;
  here the BITS are the authoritative value). C code reconstructs with
  `ml_f64()` (memcpy; zero-cost). Values are therefore EXACT — never a
  decimal-literal round-trip (PLAN §2).
- **Ints as ints.** Fields whose executed values are mathematical integers
  AND semantically integral (frame counts/indices, ECB quads, hitbox
  damage/angle/knockback/type/flags) are typed `int32_t`/`int16_t`; the
  generator HARD-THROWS if an executed value violates the pinned typing
  (no silent coercion, ever). int→double conversion in C is exact for
  these ranges.
- Booleans are `uint8_t` 0/1. NaN/Inf/-0 are rejected by the generator
  (none exist at the pin; a bit-pattern field would carry -0 faithfully if
  upstream ever introduced one — the reject is for int-typed fields and
  finiteness).

### 3.2 Field typing (pinned; measured iter 10)

Single source of truth: `pipeline/lib/tables-schema.js` (JS side) ==
the `ML_*_FIELDS` X-macros generated into `ml_tables.h` (C side).

- `ml_attributes_t` (exactly 46 fields per char, key set asserted):
  35 double-bits (`aerialHmaxV` … `wallJumpVelY`, incl. `waitAnimSpeed`,
  pinned f64 with its `run`/`walkAnimSpeed` siblings though integral at
  the pin), 6 `int32_t` scalars (`airdodgeIntangible, dashFrameMax,
  dashFrameMin, jumpSquat, runTurnBreakPoint, weight`), 3 `int32_t`
  arrays (`hurtboxOffset[2], ledgeSnapBoxOffset[3], shieldOffset[2]`),
  2 booleans (`multiJump, walljump`). Struct order = F64 sorted, I32
  sorted, I32V sorted, BOOL sorted (bytewise).
- `framesData` → `{name, int32 frames}`; `intangibility` →
  `{name, int32 start, int32 length}` (upstream comment "start, length").
- ECB → per state `int16_t v[4]` per frame, upstream order
  `[bottomOffsetY, halfWidthX, midY, topY]` (consumed by
  `src/physics/physics.js:1097-1102`). Empty state arrays exist upstream
  (puff `DEAD*`, 4 of them) and are kept verbatim: `frameCount 0`,
  `frames == NULL`.
- Hitboxes → per move `id0..id3` pointers (NULL = absent upstream); each
  hitbox: `offset` = array of double-bits Vec2D + `offsetCount` +
  `offsetIsArray` (0 = single Vec2D upstream — the engine branches on
  this; 21 exist, throws), `size` double-bits, 9 `int32_t` fields
  (`dmg, angle, kg, bk, sk, type, clank, hitGrounded, hitAirborne`),
  `throwextra` boolean. Aliased upstream offset arrays (e.g. fox nair
  reusing id1's array) are serialized per-hitbox verbatim (copies).

### 3.3 Ordering / determinism

Characters in charId order (0 marth · 1 puff · 2 fox · 3 falco ·
4 falcon == `CHARIDS`, asserted; same ids as ANIM1 and the oracle
harness). All state/move name keys bytewise-sorted ASCII (`strcmp`
order). Names asserted `[A-Za-z0-9_]+`. No timestamps, no absolute
paths; provenance (upstream HEAD + extractor bundle sha256) embedded in
the generated headers and recorded in the manifest sources together with
`pipeline/extractor/extractor.{entry,config}.js` hashes.

### 3.4 The registries NOT in CTAB1 (scope note)

`actionSounds` (task 4 territory, audio map) and the raw `offsets`
registry (consumed by the engine ONLY via the hitbox objects; embedded
there) are exposed by the extractor bundle but not emitted as C tables.
`setVelocities`/`posOffset` constants attached to actionState objects in
`characters/<char>/index.js` are move-logic constants entangled with the
actionState function table; they are ported with the sim code in M2 (the
extractor deliberately does not import the index.js aggregators — that is
the god-module boundary, anatomy §3).

### 3.5 ANIM1 cross-check (measured-then-frozen reconciliation)

`framesData`/ECB per-state frame counts vs the SAME run's decoded ANIM1
binaries is NOT uniform equality upstream (26 framesData states differ,
e.g. marth DOWNWAIT 60 vs 70 anim frames; 4 puff framesData states have
no animation; 27 ECB states differ; 13 ECB states have no animation —
incl. falco's 5 dead-file states and the TECHWALLJUMP class).
`pipeline/lib/tables-anim-xref.js` re-derives the FULL reconciliation
live from run artifacts and asserts the exact sorted lists pinned in
`pipeline/expected.json` `tables.animXref`.

### 3.6 Canonical leaf dump (round-trip contract)

One line per leaf value, `path=value`, doubles as 16 lowercase hex digits
of their bit pattern, ints as decimal; emission order == §3.2/§3.3 order:

```
attr/<char>/<field>[=|[i]=]…
frames/<char>/<STATE>=<int>
intang/<char>/<STATE>=<start>,<length>
ecb/<char>/<STATE>/<frameIdx>=<a>,<b>,<c>,<d>
hb/<char>/<move>/id<N>/offsetIsArray=<0|1>
hb/<char>/<move>/id<N>/offset[<k>]=<xbits>,<ybits>
hb/<char>/<move>/id<N>/<field>=<val>   (size bits, then i32 fields, then throwextra)
```

Reference implementations: `pipeline/lib/tables_check.c` (walks the
COMPILED tables via the X-macros; an actual store/load round trip through
a C double) and `pipeline/lib/tables-dump.js` (fresh executed-JS walk).
`check-tables.sh` compiles the former with `-ffp-contract=off` and
`cmp`(1)s the two dumps — every emitted value bit-equal to a fresh
execution of the real upstream modules. The C reader/consumer implements
THIS document; on disagreement between code and spec, the spec + a
regenerated artifact set win.

## 4. STAB1 — generated C VS-stage geometry tables

Artifacts per pipeline run: `ml_stages.h` + `ml_stages.c` (the stage tables
the sim and renderer link against) and `stages.json` (the canonical
executed-JS model; doubles carried as bit patterns). Generator:
`pipeline/stages/stages.js`. Source: the SAME extractor bundle as CTAB1
(`dist/js/extractor.js`, task-3-extended entry imports upstream's own
aggregator `src/stages/vs-stages/vs-stages.js` → `window.__stages`),
executed under the shared `window` shim in node
(`tables-schema.js loadExtractor` — one loader for all extractor globals).

### 4.1 One source of truth (PLAN §4 M1)

Upstream renders stages from the SAME structures the collision engine
consumes: `stagerender.js` draws `polygon`/`ground`/`wallL`/`wallR`/
`ceiling`/`platform`/`blastzone`/`ledge`; `physics.js` collides against
`ground`/`platform`/`ceiling`/`wallL`/`wallR`/`connected`/`ledge`/`scale`/
`offset`. There is NO separate draw-only stage vector data for VS stages
(`background`/`box`/`target` are target-stage machinery — `box` is
pinned-empty for all 6 VS stages, the others pinned-absent; the schema
hard-throws on drift). These tables therefore serve both the M2 sim and
the M3 renderer.

### 4.2 God-module boundary (how ystory/fountain got in)

`ystory.js`/`fountain.js` top-level-import `main/main`,
`stages/activeStage` and `physics/environmentalCollision`, but reference
them ONLY inside their `movingPlatforms`/`updatePlatform` function bodies
— sim LOGIC ported with M2 (like `setVelocities`, §3.4), never called at
extraction. `extractor.config.js` externals-stubs those EXACT request
strings (`"var {}"`), so the god-module never enters the bundle;
`build-extractor.sh` hard-fails on any `document.`-access leak. The stage
DATA literals are self-contained (verified: only `Vec2D`/`Box2D` and
local consts) — a stub can therefore never change an extracted value, and
the exact-key-set assert catches wrong-module substitution.

### 4.3 Value encoding + typing (pinned; measured iter 11)

Same discipline as CTAB1 §3.1: geometry coordinates, `blastzone` and
`scale` are IEEE-754 uint64 bit patterns (`UINT64_C(0x…)` + shortest
round-trip decimal comment; decode `ml_stage_f64()` — named apart from
`ml_tables.h`'s `ml_f64` so both generated headers can share a TU);
integral-and-semantically-integral fields are `int32_t` with generator
hard-throws: `offset[2]` (screen pixels), ledge/connected indices, faces
(asserted `1|-1`), `movingPlats` entries (asserted `< platformCount`).
Booleans are `uint8_t` 0/1.

Single source of truth: `pipeline/lib/stages-schema.js`. Pinned there:

- Stage order = oracle harness `--stage` ids == upstream
  `activeStage.js stageMapping`: 0 battlefield · 1 ystory · 2 pstadium ·
  3 dreamland · 4 fdest · 5 fountain (`STAGE_NAMES`).
- Surface kind order (struct + dump + coverage): `ground, platform,
  ceiling, wallL, wallR` (`SURF_KINDS`). A `Surface` is exactly
  `[Vec2D, Vec2D]` at the pin; the type-level optional
  `SurfaceProperties` third element exists on NO VS stage and its
  appearance HARD-THROWS (never silently dropped — format bump).
- Empty surface lists exist upstream and are kept verbatim (`fdest`
  platforms, `ystory` ceilings): count 0 / `NULL` pointer.
- `ledge` = `{type 0 ground | 1 platform, index, side 0 left | 1 right}`,
  parallel to `ledgePos[i]` (lengths asserted equal); indices
  bounds-checked against the typed surface list.
- `startingPoint`/`startingFace`/`respawnPoints`/`respawnFace` are fixed
  `[ML_STAGE_PLAYERS = 4]` arrays (engine player slots).
- `blastzone` = Box2D → `{minX, minY, maxX, maxY}` bits.
- `connected` (ystory + fountain only): `hasConnected` flag +
  `connectedGround[groundCount]` / `connectedPlatform[platformCount]`
  (lengths pinned to how `physics.js` indexes them); each entry is a
  left/right pair of `{present, type, index}` labels with the
  `getSurfaceFromStage` enum `0 g · 1 p · 2 c · 3 l(wallL) · 4 r(wallR)`
  (only `g` occurs at the pin; membership asserted).
- `movingPlats` carries the platform indices; the movement functions are
  M2 sim code (§3.4 discipline). Fountain's platform rest heights are the
  extracted `platform[1]/[2]` y values; ystory's mover extracted at its
  authored start position.

### 4.4 Ordering / determinism

Stages in stageId order; within a stage: polygon rings (authored order —
ring/vertex order is render/collision-meaningful, NEVER sorted), then
surfaces in `SURF_KINDS` order (authored order within each list — physics
indexes these lists positionally), ledges, ledgePos, spawn/respawn,
blastzone, scale, offset, connected (grounds then platforms), movingPlats.
No timestamps, no absolute paths; provenance (upstream HEAD + extractor
bundle sha256 + extractor entry/config hashes) embedded in the generated
headers and recorded in the manifest sources.

### 4.5 Canonical leaf dump (round-trip contract)

One line per leaf value, `path=value`, doubles as 16 lowercase hex digits
of their bit pattern, ints as decimal; emission order == §4.4:

```
stage/<name>/polygon/<r>/<v>=<xbits>,<ybits>
stage/<name>/<kind>[<i>]=<x1>,<y1>,<x2>,<y2>        (kind in SURF_KINDS order)
stage/<name>/ledge[<i>]=<type>,<index>,<side>
stage/<name>/ledgePos[<i>]=<xbits>,<ybits>
stage/<name>/startingPoint[<i>]=<xbits>,<ybits>
stage/<name>/startingFace[<i>]=<1|-1>
stage/<name>/respawnPoints[<i>]=<xbits>,<ybits>
stage/<name>/respawnFace[<i>]=<1|-1>
stage/<name>/blastzone=<minx>,<miny>,<maxx>,<maxy>
stage/<name>/scale=<bits>
stage/<name>/offset=<x>,<y>
stage/<name>/hasConnected=<0|1>
stage/<name>/connected/<g|p>[<i>][<0|1>]=<-|type,index>   (when hasConnected)
stage/<name>/movingPlats[<i>]=<platformIndex>
```

Reference implementations: `pipeline/lib/stages_check.c` (walks the
COMPILED tables; actual store/load round trip through a C double) and
`pipeline/lib/stages-dump.js` (fresh executed-JS walk).
`check-stages.sh` compiles the former with `-ffp-contract=off` and
`cmp`(1)s the two dumps — every emitted value bit-equal to a fresh
execution of the real upstream stage modules (412 leaf lines at the pin).
The C reader/consumer implements THIS document; on disagreement between
code and spec, the spec + a regenerated artifact set win.
