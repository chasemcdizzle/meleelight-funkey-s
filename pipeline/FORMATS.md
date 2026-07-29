# pipeline/FORMATS.md — generated-data formats the C side implements against

Status: **PROVISIONAL** (auto-adopted, M1 REPLAN iter 9; §4 added iter 11;
§5 added iter 12).
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

## 5. SND1 — converted PCM audio + the sound map

Artifacts per pipeline run: `audio/sfx/<name>.pcm` (one per
`dist/sfx/*.wav`), `audio/music/<name>.pcm` (one per `dist/music/*.ogg`),
`sounds.json` (the canonical executed-JS sound map) and `audio/README.md`
(provenance notice). Generator: `pipeline/stages/audio.js`; schema/walk:
`pipeline/lib/sounds-schema.js`. Source: the SAME extractor bundle as
CTAB1/STAB1 — task 4 extends the entry with upstream's own `main/sfx`
(the 180 named Howls plus its load-time `changeVolume` pass) and
`main/music` (the 8 `MusicManager` track Howls) → `window.__sounds`,
executed under the shared loader's browser-parity shim (`window ===
global` — sfx.js reads back names it assigned to `window` as bare
globals; a detached window object breaks that path) with a `Howl`
capture shim (records each constructor cfg verbatim; loads no audio;
upstream's own post-construction `_volume` writes land on the captured
instances exactly as on real Howler objects).

**PROVENANCE / DISTRIBUTION:** all §5 audio content is Nintendo-derived
(ripped Melee SFX/music). PRIVATE USE ONLY, never distributed; blobs
exist only in gitignored build output (`build*/`) — the repo commits
hashes, never bytes. The manifest stage entry carries an explicit
`provenance` field and `check-audio.sh` enforces the no-commit guard.

### 5.1 PCM blobs (the device runtime format)

Headerless raw PCM, S16LE (little-endian per §0), per the measured
device verdict (PLAN §7, docs/research/audio-spike.md):

| dir | source | rate | channels | frame size | runtime use |
|---|---|---|---|---|---|
| `audio/sfx/` | `dist/sfx/*.wav` | 22050 Hz | 1 (mono) | 2 bytes | pre-decoded into RAM, 8-voice mixer |
| `audio/music/` | `dist/music/*.ogg` | 22050 Hz | 2 (stereo, interleaved L R) | 4 bytes | streamed from SD, 2×64 KB double-buffer |

Every blob's byte length is asserted ≡ 0 mod frame size (generator
hard-throw + check-expected re-check); the manifest artifact entry
records `samples` (sample FRAMES, i.e. bytes / frame size), `channels`
and `rate` alongside the standard `sha256`/`bytes`. wav/ogg never ship
to the device (ogg is the offline source format only).

### 5.2 Determinism / the ffmpeg pin

Raw s16le output has no container metadata or timestamps, but the
resampler (16000→22050 etc.) and the vorbis decoder are properties of
the ffmpeg BUILD. Byte-stability ×2 within a run pair cannot see a
version change, so the pin is explicit and three-layered, frozen in
`pipeline/expected.json` `audio`:

- `tool.ffmpeg` — exact version string; `stages/audio.js` HARD-FAILS
  before converting anything when the installed ffmpeg differs
  (fail loudly, never drift).
- `tool.sfxArgs` / `tool.musicArgs` — the exact conversion argv
  (incl. `-fflags +bitexact`, `-map_metadata -1`); recorded verbatim in
  the manifest and compared by check-expected, so a loosened flag is a
  failure, not a silent re-measure.
- `artifactsSha256` — aggregate sha256 over `"<path> <sha256>\n"` of
  every audio artifact (sorted by path): the frozen output bytes.

Upgrading ffmpeg is a DELIBERATE re-freeze: update `tool.ffmpeg`,
`artifactsSha256` and the byte/sample counts in expected.json in the
same change, with the diff recorded in docs/AGENT-LOG.md (same
discipline as oracle/CHECKSUM.md §8).

### 5.3 `sounds.json` — the sound map (canonical model)

Deterministic JSON (§0 rules; sorted keys, no provenance inside the
artifact — provenance lives in the manifest so unrelated pipeline edits
don't churn the frozen aggregate). Doubles are `{bits, dec}` pairs
(16 lowercase hex digits of the IEEE-754 pattern + shortest round-trip
decimal; bits authoritative, CTAB1 §3.1 discipline). Shape:

```
formatVersion: 1
sfx.<name>:   file (sfx/<x>.wav) · blob (audio/sfx/<x>.pcm) ·
              volume {bits,dec} · cfgVolume {bits,dec} · loop 0|1
music.<name>: file (music/<x>.ogg) · blob (audio/music/<x>.pcm) ·
              volume · cfgVolume · sprite { start: [offMs, durMs],
              loop: [offMs, durMs] }
actionSounds.<char>.<STATE>: [ [frame, sfxName], ... ]
```

Pinned semantics (measured iter 12; hard-throws on drift):

- **`volume` is the post-load effective value** — sfx.js's own
  module-scope `changeVolume(sounds, 0.5, 0)` /
  `changeVolume(MusicManager, 0.3, 1)` overwrites every Howl's
  `_volume` with `masterDefault × (volumeOverwrites[name] || 1)`,
  making the authored constructor volume dead at runtime. `volume` is
  what Howler actually plays at default settings and is the C mixer's
  per-sound gain source; `cfgVolume` preserves the authored
  constructor value (provenance). Runtime master-volume changes
  (audiomenu, main.js pause ducking) are M4 logic, not data.
- sfx cfg keys ⊆ {src, volume, loop}; exactly one looping sound at the
  pin (`furaloop`). src is a single-element array. 180 Howl names over
  179 distinct wavs (`electricfizz.wav` is shared by `electricfizz` +
  `loudelectricfizz`); 25 of the 204 wavs are unreferenced by any Howl
  (converted anyway — upstream content, and the map records
  reachability).
- music: exactly 8 tracks (TRACK_NAMES pinned); cfg keys ⊆ {src,
  volume, html5, sprite, onend}; `html5: true` asserted (upstream's
  own streaming hint — matches the SD-streaming design); sprite keys
  exactly `<name>Start`/`<name>Loop`, each `[offsetMs, durationMs]`
  int32 (the Start→Loop chaining `onend` handlers are M4 logic; the
  windows are the data). Sprite windows may end before the decoded
  track does (e.g. menu loop ends at 180.9 s of a 200.4 s ogg) —
  carried verbatim, Howler sprite semantics.
- `actionSounds` (per-char `STATE → [[frame, sfxName], …]`, registered
  by the attributes modules, parked here from CTAB1 §3.4): frames
  int32 ≥ 0, every sfxName must resolve in the sfx map (referential
  integrity hard-throw), event order verbatim. The
  `player.timer == frame → play` caller (actionStateShortcuts.js) is
  M2/M4 sim logic.

### 5.4 C emission judgment (why no ml_sounds.c yet)

CTAB1/STAB1 got generated C tables because the M2 sim links them.
The sound map's only consumer is the M4 audio mixer (PLAN §7), which
doesn't exist yet — emitting a C surface now would freeze a
mixer-facing API before the mixer design does, so `sounds.json` is the
canonical artifact and the M4 mixer task generates its C table FROM
this model, implementing against THIS section with the same round-trip
discipline (dual dump, hard-throw typing). PROVISIONAL (auto-adopted,
iter 12). On disagreement between code and spec, the spec + a
regenerated artifact set win.

## 6. TTAB1 — generated C target-test stage tables

Artifacts per pipeline run: `ml_targets.h` + `ml_targets.c` (the authored
target-stage tables the M4 target sim and renderer link against) and
`targets.json` (the canonical executed-JS model; doubles carried as bit
patterns). Generator: `pipeline/stages/targets.js`. Source: the SAME
extractor bundle as CTAB1/STAB1 (`dist/js/extractor.js`, the M4-task-11
entry imports upstream's own aggregator
`src/stages/targetstages/tstages.js` → `window.__targetStages`), executed
under the shared `window` shim (`tables-schema.js loadExtractor`).

### 6.1 Shape (measured-then-pinned, iter 94 — the STAB1 inverse)

Ten authored stages, id order 0..9 == upstream
`activeStage.js targetStageMapping` (`targetstage1`..`targetstage10`) ==
the `setActiveStageTarget` selection domain. Per stage the EXACT key set
(schema `pipeline/lib/targets-schema.js`, hard-throw on drift):
`startingPoint` (pinned length 1 — `startTargetGame` reads only `[0]`),
`box` (Box2D[] — the authored AABBs, PAYLOAD here, the inverse of
STAB1's pinned-empty), `ground`/`ceiling`/`wallL`/`wallR`/`platform`
(`[Vec2D, Vec2D]` surfaces; NO authored stage carries the optional
SurfaceProperties third element — damageType is measured-absent across
ALL authored stages, its appearance hard-throws, format-bump territory),
`ledge` (`[type∈{ground,platform}, index (bounds-checked int32), side
∈{0,1}]`), `target` (Vec2D[] — breakable centers; the collision radius
is the CODE literal 7 in `targetplay.js`, not data), `scale` (f64),
`blastzone` (Box2D), `offset` (int32[2]). OPTIONAL: `ledgePos` — EXACTLY
one authored stage carries it (`targetstage9`, parallel to its 1-entry
`ledge`; AI-only upstream, ai.js:890, and target mode fields no CPU —
carried VERBATIM, the fdest-quirk faithfulness precedent; found by the
schema hard-throw itself: the file's `ledgePos :` space-before-colon
defeated the static key grep). `name`/`polygon`/`polygonMap`/
`respawnPoints`/`respawnFace`/`startingFace`/`movingPlats`/
`movingPlatforms`/`connected` are pinned ABSENT (polygonMap exists only
on the builder/custom plane, scope-excluded per fix_plan §M4).

### 6.2 Value encoding + round trip

Same discipline as CTAB1 §3.1/STAB1 §4.3: doubles as `UINT64_C(0x…)` bit
patterns + shortest-round-trip decimal comments (decode `ml_target_f64()`
— named apart from `ml_f64`/`ml_stage_f64` so all generated headers can
share a TU); ints int32 with generator hard-throws; empty lists emit
count 0 / NULL. Canonical leaf dump grammar
(`tstage/<name>/<field>[i]=<bits|ints>` + the `hasLedgePos` presence
line): JS walker `pipeline/lib/targets-dump.js`, C twin
`pipeline/lib/targets_check.c`; `pipeline/check-targets.sh` cmp(1)s the
two byte-for-byte — 718 leaf values (measured-then-frozen, iter 94).
Coverage pins live in `pipeline/expected.json` `targets` (10 stages, 90
targets — targetstage6 has 9, targetstage9 has 1; 85 boxes; per-stage
counts). On disagreement between code and spec, the spec + regenerated
artifacts win.

## 7. IMG1 — pre-scaled RGB565 menu artwork

Artifacts per pipeline run: `assets/menu.img1` (ONE packed file holding
every menu image) and `assets/README.md` (provenance notice). Generator:
`pipeline/stages/assets.js`; codecs `pipeline/lib/png.js` (decode) and
`pipeline/lib/img1.js` (resample / encode / read / dump). C loader:
`port/gfx/img1.{c,h}`.

Source: upstream's own menu artwork under `dist/assets/` — 5 character
portraits (`css/{marth,puff,fox,falco,falcon}.png`), 6 VS-stage previews
(`stage-icons/{bf,ys,ps,dl,fd,fod}.png`, in oracle `--stage` id order)
plus the RANDOM icon (`stage-icons/Icon_Transparent_Question.png`), and
3 hand cursors (`hand/{handpoint,handopen,handgrab}.png`). Unlike
CTAB1/STAB1/TTAB1 no JS is executed: these are content files, converted
like §5's audio.

**PROVENANCE / DISTRIBUTION:** all §7 artwork is Nintendo-derived
(ripped Melee character/stage art). PRIVATE USE ONLY, never distributed;
`menu.img1` exists only in gitignored build output (`build*/`) — the repo
commits hashes, never bytes. The manifest stage entry carries an explicit
`provenance` field and `check-assets.sh` enforces the no-commit guard.

### 7.1 Container layout

Little-endian throughout (§0). All offsets are absolute file offsets.

```
0   4   magic "IMG1"
4   4   u32 imageCount
8   4   u32 fileBytes            (== the file's actual length)
12  ..  directory: imageCount x 24 bytes, in the pinned order below
          0   char name[16]      NUL-terminated, [a-z0-9_], NUL-padded
          16  u16 w
          18  u16 h
          20  u32 dataOff        multiple of 4
then, per image, at dataOff:
          w*h x u16   RGB565 pixels, row-major, top-left origin
          w*h x u8    alpha, same order
        each image's block padded to the next multiple of 4
```

`dataOff` is 4-aligned so the 565 plane can be read as `uint16_t*`
directly out of the loaded file (§0 grammar: LE targets only —
`img1_open` refuses on a big-endian host rather than swapping colours
silently). The loader enforces the WHOLE grammar above, not merely the
parts that would crash it — name charset, NUL termination, zero padding
after the terminator, name uniqueness, `dataOff` 4-aligned AND after the
directory AND non-overlapping with the previous image, plus every length
against `fileBytes`. `check-assets.sh` proves rejection with one
corrupted copy per arm — 15, and the count itself is asserted: file
shorter than the header, bad magic, truncated, zero count, count whose
directory overruns the file, unterminated name, empty name, illegal name
byte, non-zero name padding, duplicate name, zero dimension, misaligned
`dataOff`, `dataOff` 0 aliasing the header, overlapping blocks,
out-of-file `dataOff`. Each tooth asserts the arm's COMPLETE diagnostic
by string equality: rejection alone is not enough (a tooth that corrupts
the wrong bytes is rejected by a different arm and leaves the intended
one untested — measured, two teeth did exactly that), and a substring
match is not enough either (a fragment can be satisfied by another arm's
longer message).

The directory ORDER is pinned in `expected-assets.json`
(`assets.directory`) and `lib/img1-dump-order.js` compares the order the
C loader actually observes against it, because nothing else binds them —
the C and JS dumps agree with each other whatever the order is, and
`perImage` is keyed by name. A file with two names swapped against their
pixels is structurally legal, loads fine, and is caught only here; the
check proves that both ways. That parser validates EVERY dump line
against an anchored grammar (PROCESS §3's whitelist rule) rather than
filtering on a line prefix, so malformed records and unexpected trailing
data fail instead of being skipped.

PROVENANCE ENFORCEMENT (`lib/assets-nocommit-guard.js`) is by CONTENT and
covers BOTH git planes: the forbidden set is the 15 source PNG sha256s
plus the emitted `menu.img1` sha256, taken from the run manifest, and
every stage-0 INDEX blob (via `git cat-file --batch`) as well as every
tracked working-tree file is hashed against it. Name-based scanning was
insufficient (a renamed copy passed), and hashing working-tree bytes for
index paths was insufficient too (a blob staged and then removed from
disk stayed commit-ready and invisible). A tracked path that cannot be
read fails unless git itself reports it deleted.

Directory order is pinned and is the consumer's index space:

| idx | name | idx | name | idx | name |
|---|---|---|---|---|---|
| 0 | `marth` | 5 | `stage_bf` | 11 | `stage_random` |
| 1 | `puff` | 6 | `stage_ys` | 12 | `hand_point` |
| 2 | `fox` | 7 | `stage_ps` | 13 | `hand_open` |
| 3 | `falco` | 8 | `stage_dl` | 14 | `hand_grab` |
| 4 | `falcon` | 9 | `stage_fd` | | |
| | | 10 | `stage_fod` | | |

0..4 is the oracle `--p1/--p2` character id order and 5..10 the
`--stage` id order (CLAUDE.md §Commands), so a caller may index directly;
`img1_find()` by name is the alternative.

### 7.2 Pixels: 565 + an 8-bit alpha plane (the MEASURED decision)

Pixel format is the device framebuffer's: RGB565. `img1_blit` expands 565
back to 888 by BIT REPLICATION, so the values a stored pixel can actually
take are `rep(k) = (k<<3)|(k>>2)` (5-bit) and `(k<<2)|(k>>4)` (6-bit).
The encoder stores the code whose `rep(k)` is NEAREST to the source byte,
ties to the brighter code (`lib/img1.js quant565`, exhaustive integer
search over the lattice). Averaging happens in LINEAR LIGHT, not in
gamma-encoded sRGB codes — see the transfer tables below.

CORRECTION (2026-07-28, measured — this section previously said the
opposite). Until now the encoder quantized by TRUNCATION, `raster.c
pack565()` byte-for-byte, and this paragraph justified it: "rounding
would be marginally more accurate (truncation biases the mean by about
-3/255 on the 5-bit channels, measured), but an image pixel and a vector
fill of the same RGB888 must land on the SAME 565 value or they seam by
one step where art meets UI; truncation guarantees that, and at 16 bpp
the bias is invisible." The last clause is FALSE on this artwork, and
the -3/255 was the right number read the wrong way:

- Over all 256 codes, bit-replicated truncation is unbiased — the
  replicated low bits pay the debt back at the top of each 4-code group
  (mean signed error 0.000, measured). That average is what "invisible"
  was reading. Restricted to the DARK end the debt is never paid: over
  codes 0..15 the mean signed error is exactly **-3.5/255 per 5-bit
  channel** (nearest: +0.5), and the stage previews live there.
- Battlefield's source art has mean Y **9.18/255**. A near-constant
  -1.8/255 of luminance is therefore **20% of the whole image**.
  Measured share of source mean Y retained through quantization alone:
  bf 79.7% · ps 87.9% · fd 86.8% · fod 89.5% · dl 91.6% · ys 93.5%.
  Nothing about that is invisible; it is the single largest attributable
  loss in the asset path (`.loop/c4-dim/REPORT.md` hop 3).

THE COST, stated plainly: the seam invariant above is now WEAKER, though
not void. A representable colour ALWAYS agrees (truncating and rounding
both return its own code). A non-representable one usually agrees too —
truncation and nearest pick the same code for 196 of 256 five-bit values
and 194 of 256 six-bit values (measured); for the remaining 60 / 62 they
differ by exactly ONE code (~8/255 on a 5-bit channel, ~4/255 on the
6-bit) — 48 / 32 of those because nearest is strictly closer, the other
12 / 30 because an exact tie is resolved to the brighter code. They
differ because the fill still goes through `pack565`'s truncation while
the stored pixel is rounded. Second, smaller cost: because ties go to the
brighter code, nearest carries a small UPWARD bias averaged over all 256
codes (+0.375/255 on 5-bit, +0.469/255 on 6-bit) where truncation
averages 0.000 — a deliberate trade against truncation's -3.5/255 where
the art actually lives. `pack565` itself is unchanged and every stored code is
one of its fixed points (asserted in `lib/assets-selftest.js`), so the
two quantizers still share one lattice — they can only disagree about
which rung of it a given RGB888 belongs on. MEASURED rather than argued:
the SSS panel is where art meets fill most directly (`foh_render.c`
render_sss — the big preview sits on `inner` {10,8,20}, each thumbnail on
the `strip` {6,6,10} that carries its label), and ZERO pixels of the
seven emitted stage previews carry either of those exact RGB888 values,
so no pixel we ship can seam. The truncation loss, by contrast, hit every
dark pixel of every image.

LINEAR-LIGHT AVERAGING (same change, second half). The box filter of §7.4
used to average 8-bit sRGB CODES. Codes are gamma-encoded, so their
average is not the average of the light: it is systematically darker, and
the error grows with the contrast inside the box — which is exactly the
thin bright platform rims that make a 65x24 thumbnail readable. Measured
share of the source's mean LINEAR light retained by the old path: bf
66.8% · fd 72.7% · ps 76.4% · fod 84.8% · ys 85.9% · dl 87.0%; bf's p95
fell from a source 16.11 to 15.76. The resampler now converts to linear
light, averages premultiplied, and converts back (`SRGB_TO_LIN` /
`LIN_TO_SRGB` in `lib/img1.js`): light retained becomes 102.3% / 105.5% /
108.2% / 102.3% / 101.5% / 104.4% and bf's p95 rises to 30.06.

The >100% is quantization, not a brightness lift: rounding is very nearly
unbiased in sRGB code space (its own small upward tie bias is +0.375/255
on 5-bit and +0.469/255 on 6-bit, disclosed above — far too small to
explain a 2-8% light gain), and the code lattice is CONVEX in light, so at
the dark end the two nearest codes average brighter in light than the
value between them. That convexity is the term that matters here.
It is a property of 565 on near-black art, not a gamma knob: NEITHER of
the two fixes in this section departs from upstream's pixels on purpose —
both only stop the encoder from losing what is there. (The stagePreview
lift in §7.2.1 below IS such a departure; it is ruled, scoped and labelled
there, and it is the only one in §7.)

Both tables are INTEGER, built at load with BigInt exact-rational
arithmetic — never `Math.pow`. `LIN[c] = round(4095*EOTF(c/255))` for the
IEC 61966-2-1 EOTF, solved as an exact integer inequality (the exponent
2.4 = 12/5, so a fifth power clears it); the inverse is the nearest code
in linear distance, ties bright, derived from the forward table so that
`LIN_TO_SRGB[SRGB_TO_LIN[c]] === c` for every code. That identity is what
keeps an image emitted at its SOURCE size (the 58 px portraits, one
source pixel per destination pixel) bit-identical through the round trip
— measured, not assumed. Determinism therefore remains a property of the
arithmetic, exactly as §7.4 claims, with no libm anywhere on the path.
`lib/assets-selftest.js` re-derives both tables independently and
compares them entry for entry.

### 7.2.1 DELIBERATE DEVIATION — the stagePreview brightness lift

Everything else in §7 is a faithfulness argument: emit upstream's pixels,
lose nothing to the encoding. This is not. **Owner ruling 2026-07-28
(relayed by the driver): lift the stage previews with gamma 0.75.** It is
recorded here as a DEVIATION under HARD RULE 5 — upstream ships this art
raw and we are departing from it on purpose. Nobody should later read
this as a bug fix, and nobody should extend it without another ruling.

What was measured before the ruling (`.loop/c4-dim/REPORT.md`):

- The source art is genuinely near-black — `bf.png` mean Y **9.18/255**
  (3.6%), whole-image p95 16/255. That is not our loss: upstream's own
  browser render of that PNG is **byte-identical** to the source bytes
  (verified pixel-for-pixel against a captured upstream stage-select
  frame), so there is no brightness "supposed to be there" that the
  pipeline ate. Fixes A and B above recover what the pipeline DID eat;
  after them the previews carry 101-108% of the source's light.
- It is legible upstream anyway because upstream draws it **800x300 on a
  1200x750 canvas**. The FOH draws it **130x48 on a 240x240 panel** — 3%
  of the linear size. Detail loss at that scale is not a brightness
  problem and no lift fixes it, but the CONTRAST problem is real: on
  device the SSS wallpaper measures mean Y 15.19 and the left gutter
  18.15, so an un-lifted bf preview (10.47) reads as a black hole inside
  a brighter frame — which is what "can barely see them" meant.

Scope and shape of the deviation, all deliberate:

- **stagePreview CLASS ONLY**, structurally enforced rather than by
  convention: `stages/assets.js` hangs `gamma` on the CLASS (so no image
  is special-cased by name) and HARD-THROWS if any other class carries it,
  so portraits and cursors take the identity path by construction. (The
  stronger byte-level claim — that they are bit-identical to an un-lifted
  build — is measured in the change's evidence, not by a committed check:
  no committed check builds an un-lifted artifact to compare against.)
- **Named constant** `STAGE_PREVIEW_GAMMA = [3, 4]` (= 0.75). The
  alternative the owner asked to see is `[13, 20]` (= 0.65); switching is
  a one-line edit plus an `expected-assets.json` re-freeze.
- **Integer table, no `Math.pow`** — `lib/img1.js gammaTable` is the same
  exact-rational BigInt discipline as the transfer tables, so §7.4's
  determinism-by-construction survives intact.
- Applied AFTER the light-correct resample and BEFORE quantization, to
  RGB only: alpha, and therefore every pinned alpha class, is untouched.

Measured effect on mean Y (emitted bytes, not simulation) — source ·
current · A+B · A+B+0.75 (shipped) · A+B+0.65:

| stage | src | current | A+B | **+γ0.75** | +γ0.65 |
|---|---|---|---|---|---|
| bf  | 9.18  | 7.31  | 10.47 | **19.52** | 25.90 |
| ys  | 24.82 | 23.20 | 26.18 | **42.53** | 52.46 |
| ps  | 13.03 | 11.45 | 14.61 | **24.34** | 30.84 |
| dl  | 24.19 | 22.16 | 25.64 | **41.98** | 51.32 |
| fd  | 11.11 | 9.64  | 12.53 | **22.97** | 29.93 |
| fod | 17.54 | 15.70 | 18.76 | **31.11** | 39.73 |

Against the device-measured wallpaper (15.19) the shipped lift puts every
preview ABOVE its surround: bf 1.29x, fd 1.51x, ps 1.60x, fod 2.05x, dl
2.76x, ys 2.80x — versus 0.48x for bf today. Final judgement is the
owner's on hardware.

Alpha is a full 8-bit plane for EVERY image. The alternatives were
measured on the sources, not guessed:

| class | measured | images |
|---|---|---|
| opaque (a==255 everywhere) | colour type 2, no alpha channel at all | bf, ys, ps, dl, fd |
| binary (only 0 and 255) | a 1-bit colour key would suffice | all 5 portraits |
| aa (partial alpha present) | 24.53 / 33.92 / 28.35% of pixels partial (handgrab / handopen / handpoint), 4.45% (RANDOM icon), 0.91% (fod) | hands, fod, stage_random |

A 1-bit colour key therefore does **not** cover the domain: the three
hand cursors alone are a quarter to a third partial-alpha pixels, and no
threshold recovers an anti-aliased edge. Per-image before/after classes
are pinned in `pipeline/expected-assets.json` and re-derived from the
emitted artifact, so this stays measured rather than remembered.

BEWARE the self-inflicted version of this argument: an earlier revision
emitted portraits at 56 px (from 58 px sources) and observed that all
five came out `aa`, citing that as further proof. It was circular — the
3.4% downscale created the partial alpha (0% → 6.04% on marth, measured;
no destination column was a pure source copy). Portraits are now emitted
at their native 58 px, keep their `binary` class, and the encoding
decision rests where it always genuinely rested: on the cursors and the
RANDOM icon.

One encoding with one loader path was chosen over three variants. The
honest cost: all-255 planes for the 5 opaque previews (7,800 B) plus
8-bit planes where 1 bit would do for the 5 binary portraits (11,890 B
vs ~1,640 B) — about 18 KB of the 74 KB file. Three decode paths to
recover 18 KB on a device that streams music off SD is not a trade worth
making; if it ever is, the format gains a per-image class byte.

Transparent-pixel colour is not incidental: the portraits store WHITE
under their transparent pixels, so the resampler averages PREMULTIPLIED
RGB and divides the summed alpha back out. Straight averaging paints a
white halo around every character.

### 7.3 Sizes: what is derived and what is chosen

Upstream's canvas is 1200x750 (`dist/meleelight.html:218-222`), so a
literal scale to 240x240 maps its 81x58 portrait
(`src/menus/css.js:635`) to 16x12 device pixels — useless. The FOH is a
native 240x240 UI whose elements are proportionally much larger. So the
ONLY hand-chosen number per class is a target WIDTH; each height is
DERIVED from the measured source dimensions at the source's own aspect
ratio, and `lib/img1.js` hard-throws if that ever implies an upscale.

Which numbers are DERIVED and which are CHOSEN, stated plainly:

| class | width | derived or chosen | emitted |
|---|---|---|---|
| portrait | 58 | **derived**: the sources' own width, so there is NO GEOMETRIC SCALING — each destination pixel covers exactly one source pixel (4 across = 232 px still fits 240). Not "untouched": alpha-0 pixels are normalized to (0,0,0,0) and colour is 565-quantized, as for every image | 58x40/37/40/45/43 |
| stagePreview | 65 | **derived** from the target element: the FOH SSS thumbnail's inner area is 65 px wide with its border drawn OUTSIDE it (`render_sss`, `port/foh/foh_render.c`), so 65 fills the cell exactly; the 800x300 source aspect then leaves 24 px, and the A1 Phase 1 restyle spends the remaining 10 px of the cell on the black name strip | 65x24 |
| cursor | 24 | **chosen**: upstream's 101x133 on a 1200x750 canvas is 8.4% of width / 17.7% of height, which at 240 would be 20x27 — too small to read as a hand. 24 is ~10% of screen width; the height follows the source aspect | 24x32 |

Both slots are now CONSUMED (A1 restyle Phase 1, `render_css` /
`render_sss` in `port/foh/foh_render.c`): portraits appear twice — cropped
into each 44 px character-row cell (centred in x, TOP-aligned in y so the
head fills the cell) and full-width in the port panels, which are one
portrait wide by construction — and `hand_point`
is the CSS cursor. The 58 px "native size is the conservative choice"
bet paid off exactly as stated: the panel use needs the full width and
the cell use crops it, and a consumer that could only letterbox or crop
never had to un-blur. Stage previews are used at 1x in the thumbnails
and integer-2x in the big preview.

Re-sizing is a one-line edit in `stages/assets.js` (`CLASSES[].width`)
plus an `expected-assets.json` re-freeze.

### 7.4 Determinism, contract and round trip

No external tool participates: decode is stdlib zlib plus the PNG spec's
filter reconstruction, and resampling is an exact-coverage box filter in
INTEGER arithmetic (destination pixel i covers source interval
`[i*src/dst, (i+1)*src/dst)`; multiplying by `dst` makes every endpoint
an integer, so per-axis weights are exact and sum to `src`) over
LINEAR-LIGHT samples taken from the integer transfer tables of §7.2 —
BigInt exact-rational at build, no `Math.pow`, no float. Byte
stability is therefore a property of the code, not of a pinned tool
version — contrast §5.2, where the pin exists because resampler bytes
are an ffmpeg-build property. `check-assets.sh` still runs the decoder
against ffmpeg's independent decoder on all 15 sources (PNG decoding is
lossless: disagreement would be a bug, not a version difference).

The resampler has no external partner, so `lib/assets-selftest.js`
supplies its own — and note WHY its primary check is a second
implementation rather than a list of invariants. The first version of
this section asserted weight sums, constant-colour reproduction and an
axis-aligned halo boundary; review then showed a VERTICALLY FLIPPED
resampler and a STRAIGHT-alpha resampler both passed all of it.
Symmetries and sums are not correctness. So:

- **Primary:** `oracleResize`, an independent exact-rational (BigInt)
  area-average sharing no code with `lib/img1.js` — not even
  `axisWeights`, and not the transfer tables either: it derives its own
  pair by a different route (a monotone two-pointer scan over the exact
  integer characterization) and asserts the production tables match
  entry for entry, all 256 + 4096 of them — must agree BYTE-FOR-BYTE
  with the production resampler on six real source images at their real
  target sizes.
- Supporting: axis weights swept for every used (src,dst) pair and all
  `src <= 120, dst <= src` (each destination cell sums to exactly `src`,
  each source index contributes exactly `dst`); an IMPULSE must land in
  the one covering destination cell and a monotone GRADIENT must map
  exactly (both detect flip/transpose/offset); the halo boundary now
  falls INSIDE a destination cell, where straight-alpha averaging
  visibly fails; round-half-up is pinned at exact ties for colour and
  alpha (real artwork never produces a .5 quotient, so nothing else
  constrains the rounding mode); constant-colour and fully-transparent
  fixtures; and one tooth per decoder rejection branch plus the upscale
  hard-throw.

Without this, `artifactsSha256` would only freeze whatever the first run
produced — stability, not correctness.

`resizeRgba` has NO identity fast path, deliberately: `dw===sw` is exact
through the general path, and copying is NOT equivalent to it — copying
preserves whatever RGB hides under fully transparent pixels (the
portraits' white), while the general path emits `(0,0,0,0)` there. The
shortcut therefore made portraits carry hidden white while every other
image carried black; the oracle differential found it. One path, one
behaviour.

Coverage pins live in `pipeline/expected-assets.json` (15 images: 5
portraits / 7 stage previews / 3 cursors; per image the measured source
size, PNG colour type and before/after alpha class next to the emitted
size; the directory order; the IMG1 layout arithmetic; and an aggregate
sha256 over path+sha256 of every artifact). The checker also pins the
manifest stage's exact FIELD SET and re-hashes every recorded source
from disk, so provenance cannot be dropped or falsified — an earlier
version validated neither, and a stage returning `sources: []` printed
`ASSETS OK`. It sits BESIDE `pipeline/expected.json`
rather than inside it because that file and `lib/check-expected.js` are
sha256-pinned `reviewed-go` in `port/sim/device/m4-freeze-manifest.txt`;
`lib/check-assets-expected.js` carries the same two closed-schema arms
(contract WIDTH and full leaf-shape DEPTH pinned in code, not in the
data), so nothing is weakened. Promotion into `expected.json` is a
driver-owned re-pin.

Round trip: `port/gfx/img1_check --dump` (the C loader) and
`pipeline/lib/img1-dump.js` (an independent JS reader) emit the same
canonical text — one `row <img> <y> <565 hex> <alpha hex>` line per image
row — and `check-assets.sh` cmp(1)s them byte-for-byte. `img1_blit`
claims to be bit-identical to the raster's own `rast_blit_rgba` rather
than new blend math (it unpacks 565 and calls `rast_blend_px`, the same
entry point, with the same `(a*256)/255` conversion); `img1_check
--blit` proves it by memcmp of both the framebuffer AND the ink plane
over every image at 6 offsets (origin, interior, negative origin,
off-right/bottom, and two straddling the clip band's own edges) x 2 clip
bands x ink enabled and suppressed = 360 cases, each additionally
required to be NON-VACUOUS at the origin (the framebuffer and the ink
plane must both differ from a fresh clear, so a no-op blit cannot pass
by matching a no-op comparison). The same binary exercises the lookup
API in both directions (`img1_find` <-> `img1_at` for every entry) and
its out-of-domain answers (absent name, name PREFIX, negative index,
index == count, NULL set) — all of which a deliberately broken
`img1_find` used to survive. On disagreement between code and spec, the
spec + regenerated artifacts win.
