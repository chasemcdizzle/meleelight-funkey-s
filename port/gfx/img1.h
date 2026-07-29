// port/gfx/img1.h — IMG1 menu-image loader + blitter (M4 task A9).
//
// IMG1 is the pipeline `assets` stage's output (pipeline/FORMATS.md §7):
// upstream's own menu artwork — 5 character portraits, 6 VS-stage previews
// + the RANDOM icon, 3 hand cursors — decoded, pre-scaled for 240x240 and
// packed into ONE file as RGB565 (little-endian) plus an 8-bit alpha plane
// per image. The ENCODER stores the NEAREST bit-replicable code, not
// raster.c pack565's truncation (FORMATS.md §7.2, changed 2026-07-28): every
// stored code is still a pack565 fixed point, so this loader is unaffected,
// but an image pixel and a vector fill of the same RGB888 may now land one
// code apart when that colour is not exactly representable.
//
// The blitter is deliberately NOT new blend math: img1_blit unpacks 565 to
// RGB888 (bit-replication, which round-trips through pack565 exactly) and
// calls the raster's own rast_blend_px, so its output is bit-identical to
// rast_blit_rgba on the equivalent RGBA sprite by construction. img1_check
// proves that mechanically on every emitted image.
//
// PROVENANCE: the artwork is Nintendo-derived. PRIVATE USE ONLY, never
// distributed; the .img1 file is gitignored build output (never committed).
#ifndef GFX_IMG1_H
#define GFX_IMG1_H

#include <stddef.h>
#include <stdint.h>

#include "raster.h"

#define IMG1_NAME_MAX 15 // 16-byte NUL-terminated name field

typedef struct {
  char name[IMG1_NAME_MAX + 1];
  int w, h;
  const uint16_t *rgb565; // w*h pixels, LE 565 (the framebuffer's format)
  const uint8_t *a8;      // w*h alpha, 0 = fully transparent (skipped)
} Img1Image;

typedef struct {
  uint8_t *blob;  // whole file, owned
  size_t bytes;
  int count;
  Img1Image *img; // count entries, in the file's pinned directory order
} Img1Set;

// Load a .img1 file. Returns 0 on success, -1 on any failure (see
// img1_error()); the set is left zeroed on failure. Every offset, size and
// name in the file is validated against the §7.1 grammar and the file's own
// length before use.
//
// PRECONDITION: `set` must be zeroed (e.g. `Img1Set s = {0};`) or the
// result of a previous img1_open/img1_close. Given that, reopening is safe
// — img1_open closes the previous contents first rather than leaking them.
// Passing uninitialized memory is undefined, as it would be for free().
int img1_open(Img1Set *set, const char *path);
void img1_close(Img1Set *set);

// Lookup by name ("marth", "stage_bf", "hand_point", ...) or by directory
// index. NULL when absent — callers decide whether that is fatal.
const Img1Image *img1_find(const Img1Set *set, const char *name);
const Img1Image *img1_at(const Img1Set *set, int index);

// Source-over blit of the whole image with its top-left at (x0, y0), in
// raster coordinates, honouring the raster's clip band and ink plane.
void img1_blit(Raster *rz, const Img1Image *im, int x0, int y0);

// Last failure reason (static string, never NULL).
const char *img1_error(void);

#endif // GFX_IMG1_H
