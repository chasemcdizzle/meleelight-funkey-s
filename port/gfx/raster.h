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

// INK SUPPRESSION (M4 task 2): the background art plane (upstream bg1/bg2
// layers) is NOT part of the silhouette mask on either side (the browser
// reference masks fg1|fg2|UI only), but the device framebuffer still wants
// the art. While disabled (0), fills/blits composite into the 565 fb but
// never set the ink plane. Default enabled (1); gfx_render_frame brackets
// the background pass only.
void rast_ink_enable(int on);

// Single-pixel source-over blend (a256 0..256) honouring the clip band and
// the ink-enable flag; used by the glyph/sprite blitter (gfx_overlay.c).
void rast_blend_px(Raster *rz, int x, int y, RastCol col, unsigned a256);

// Canvas `globalCompositeOperation = "screen"` for the four upstream vfx that
// set it (shineloop, laser, illusion, phantasm). OFF by default; switch it on
// around the draw and off again, the way upstream saves and restores the
// canvas field. See the note beside screen565 in raster.c for why shineloop
// could not keep drawing source-over.
void rast_screen_enable(int on);

// Batch blend primitives (M4 task 3, measured-hotspot class fix — see
// raster.c): pixel loops moved into this -O3 TU, arithmetic EXACTLY
// rast_blend_px's (bit-identical by construction).
//   rast_fill_row_opaque — one full row at y, opaque colour (a256==256).
//   rast_blit_a8mask     — the glyph-mask loop (a8 alpha, 0 skipped).
//   rast_blit_rgba       — the sprite loop (RGBA rows, alpha 0 skipped).
void rast_fill_row_opaque(Raster *rz, int y, RastCol col);
// B9 run primitives — the same class one caller further out, for
// foh_render.c's px8_over (which composites in 8-bit and stores OPAQUE, so
// it never touches blend565). Both replicate px8_over's arithmetic EXACTLY
// and clip against BOTH [0,RAST_H) and [clipY0,clipY1), as the px8_over ->
// rast_blend_px pair did. See the block comment in raster.c.
//   rast_fill_run  == for x in [xa,xb): px8_over(rz, x, y, col, a)
//   rast_blend_run == for x in [xa,xb): c = row[x]; if (!c.a256) continue;
//                                       px8_over(rz, x, y, c, c.a256)
// `row` is indexed by ABSOLUTE x (RAST_W-strided source row base).
void rast_fill_run(Raster *rz, int y, int xa, int xb, RastCol col,
                   unsigned a);
void rast_blend_run(Raster *rz, int y, int xa, int xb, const RastCol *row);
// Run form of rast_blend_px ITSELF (blend565 path) — for callers that use
// rast_blend_px directly, i.e. the font blitters. NOT interchangeable with
// rast_fill_run: they differ whenever col.a256 < 256.
//   rast_blend_px_run == for x in [xa,xb): rast_blend_px(rz, x, y, col, a256)
void rast_blend_px_run(Raster *rz, int y, int xa, int xb, RastCol col,
                       unsigned a256);
void rast_blit_a8mask(Raster *rz, const uint8_t *mask, int w, int h,
                      int x0, int y0, RastCol col);
void rast_blit_rgba(Raster *rz, const uint8_t *rgba, int w, int h,
                    int x0, int y0);

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
