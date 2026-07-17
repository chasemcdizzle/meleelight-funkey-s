// port/gfx/raster.h — the AA scanline vector rasterizer (M3 task 3).
//
// The MEASURED rastbench variant (spikes/device-feasibility/rastbench.c,
// PLAN §5: worst variant 2.54 ms avg / 3.21 ms p99 on device) restructured
// as a real module:
//   - adaptive cubic-bezier flattening, tolerance 0.25 px (ship quality)
//   - nonzero-winding scanline fill via a ymin-sorted edge table +
//     active-edge list
//   - 4x vertical subsample AA with fractional span-end coverage
//   - RGB565 blend (source-over: out = col*a + dst*(256-a) on the packed
//     r+b / g fields)
//   - x-mirror facing + rotation about a rotation point (the
//     drawArrayPathCompress transform)
//   - zero allocations in the frame loop (all scratch is static, caps
//     asserted loudly)
//
// This TU is the ONE -O3 TU (CLAUDE.md hard rule: -O2 everywhere, -O3
// only on the hot raster TU); -ffp-contract=off like every TU.
// Rasterization is NOT checksummed (PLAN §5) — floats are fine here;
// camera/world->canvas math stays in doubles on the caller's side.
//
// INK MASK: alongside the 565 framebuffer the rasterizer keeps a per-pixel
// ink plane (1 = any coverage was ever composited). The silhouette IoU
// check needs "was ink drawn", which the 565 bytes alone cannot answer
// (a 10%-alpha fill over black is 565-invisible).
#ifndef GFX_RASTER_H
#define GFX_RASTER_H

#include <stdint.h>

#define RAST_W 240
#define RAST_H 240

typedef struct {
  uint16_t fb[RAST_W * RAST_H];  // RGB565
  uint8_t ink[RAST_W * RAST_H];  // 1 = ink composited at this pixel
  int clipY0, clipY1;            // rows [clipY0, clipY1) are drawable
} Raster;

// Colour with alpha 0..256 (256 = opaque). 565-packed at draw time.
typedef struct { uint8_t r, g, b; uint16_t a256; } RastCol;

void rast_clear(Raster *rz, uint8_t r, uint8_t g, uint8_t b,
                int clipY0, int clipY1);

// --- path building (screen space, floats) --------------------------------
// Paths accumulate into a static edge table; rast_fill consumes it.
void rast_path_reset(void);
// Begin a subpath at (x,y); subsequent verts extend it.
void rast_sub_begin(float x, float y);
void rast_sub_line(float x, float y);
// Adaptive cubic flatten (tol 0.25 px) from the current point.
void rast_sub_cubic(float x1, float y1, float x2, float y2,
                    float x3, float y3);
// Close the current subpath (emits its edges into the edge table).
void rast_sub_close(void);

// Fill everything accumulated since rast_path_reset (nonzero winding,
// 4x vertical subsample AA, fractional span ends), then reset.
void rast_fill(Raster *rz, RastCol col);

// --- convenience prims (each = reset+build+fill) --------------------------
void rast_poly(Raster *rz, const float *xy, int n, RastCol col);
// Stroke segment as a width-w quad (canvas butt caps).
void rast_stroke_seg(Raster *rz, float x0, float y0, float x1, float y1,
                     float w, RastCol col);
// Filled circle / stroked ring (annulus), polygonal approximation.
void rast_circle(Raster *rz, float cx, float cy, float rad, RastCol col);
void rast_ring(Raster *rz, float cx, float cy, float rad, float w,
               RastCol col);

void gfx_fatal(const char *what) __attribute__((noreturn)); // host-provided

#endif // GFX_RASTER_H
