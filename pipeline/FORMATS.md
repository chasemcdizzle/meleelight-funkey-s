# pipeline/FORMATS.md — generated-data formats the C side implements against

Status: **PROVISIONAL** (auto-adopted, M1 REPLAN iter 9). Format changes
after C consumers exist require a version bump in the magic and a
regeneration of every artifact + expected.json re-freeze in the same
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
