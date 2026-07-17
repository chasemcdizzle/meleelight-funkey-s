// port/gfx/anim1.h — C reader for the ANIM1 packed animation binaries
// (pipeline/FORMATS.md §2; reference implementation pipeline/lib/animbin.js).
//
// Implemented against the SPEC: 16-byte header (magic "MLA1", version 1,
// charId u16, stateCount u32, fileSize u32), state directory sorted
// bytewise by name (binary-searchable via strcmp), NUL-terminated string
// table, per-state frame-offset tables (u32 absolute; 0 = frame absent),
// frame records (u16 pathCount, then per path u16 coordCount + int16
// coords verbatim). All fields little-endian; host (arm64) and device
// (armv7) are both LE, and every read below goes through explicit
// LE byte assembly anyway so the reader is endian-correct everywhere.
//
// The loader validates magic/version/fileSize and bounds-checks every
// offset before dereferencing (bad artifact = loud abort via
// gfx_fatal, never a silent wild read). Coordinates are exposed as a
// pointer into the loaded buffer via per-coord accessor (2-byte
// alignment is guaranteed by the format; int16 loads are assembled
// from bytes to stay alignment/UB-clean).
#ifndef GFX_ANIM1_H
#define GFX_ANIM1_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
  const char *name;      // NUL-terminated, points into the file buffer
  uint32_t frameCount;
  uint32_t framesOff;    // absolute offset of this state's frame-offset table
} Anim1State;

typedef struct {
  uint8_t *buf;          // whole file
  uint32_t size;
  uint16_t charId;
  uint32_t stateCount;
  Anim1State *dir;       // stateCount entries, spec-sorted (strcmp order)
} Anim1;

// One decoded path view (no copy: coords live in the file buffer).
typedef struct {
  const uint8_t *coords; // coordCount int16 LE values
  uint32_t coordCount;
} Anim1Path;

// Load + validate an anim_<id>_<name>.bin. Aborts loudly on any
// violation. `expectCharId` < 0 skips the charId cross-check.
void anim1_load(Anim1 *a, const char *path, int expectCharId);

// Binary search the directory (spec §2.2 sort order). NULL = state not
// present (upstream renderPlayer's `animations[c][s] === undefined`
// early-return arm).
const Anim1State *anim1_find(const Anim1 *a, const char *stateName);

// Frame record access, frameIdx in [0, frameCount). Returns the number
// of paths and positions *cursor at the first path; 0 paths with
// *absent=1 when the frame offset is 0 (spec §2.4 absent sentinel).
uint32_t anim1_frame(const Anim1 *a, const Anim1State *st,
                     uint32_t frameIdx, uint32_t *cursor, int *absent);

// Read path k's header at *cursor (advancing it past the whole path).
Anim1Path anim1_next_path(const Anim1 *a, uint32_t *cursor);

// Coord accessor (LE int16 at index i of a path).
static inline int16_t anim1_coord(const Anim1Path *p, uint32_t i) {
  const uint8_t *b = p->coords + (size_t)i * 2u;
  return (int16_t)(uint16_t)((uint16_t)b[0] | ((uint16_t)b[1] << 8));
}

// Provided by the host (gfx_replay.c): fatal report, never returns.
void gfx_fatal(const char *what) __attribute__((noreturn));

#endif // GFX_ANIM1_H
