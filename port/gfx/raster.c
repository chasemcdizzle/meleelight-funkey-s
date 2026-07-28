// port/gfx/raster.c — the rastbench measured variant as a module.
// See raster.h. THE hot raster TU: built -O3 -ffp-contract=off (the only
// -O3 TU in the tree, per CLAUDE.md/PLAN §5).
//
// Provenance: restructured from spikes/device-feasibility/rastbench.c
// (the exact algorithm the device perf envelope was measured with):
// edge table + active-edge list, nonzero winding, SUBS=4 vertical
// subsamples with fractional span-end coverage, blend565, adaptive
// cubic flattening with control-polygon deviation vs tol 0.25 px.
#include "raster.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../fdlibm/fdlibm.h" // fd_sin/fd_cos — see device-libm note below

// DEVICE-LIBM CLASS (M3 task 4; fix_plan §M3 task 1 rule "trust NO
// device-libc math symbol"): the FunKey SDK's static musl libm ships a
// BROKEN floor family (identity for non-integers — measured iter 38),
// and its float transcendentals are unswept. This TU therefore consumes
// NO device-libm floor/ceil/trig:
//   - the (int)floorf/(int)ceilf sites use the exact integer helpers
//     below (pure casts + compares, no libm);
//   - circle/ring vertices route through the vendored fd_cos/fd_sin
//     doubles (bit-exact on both host and device — and the more
//     faithful choice anyway: the browser renders under the fdlibm
//     Math shim).
// Remaining libm surface here: sqrtf/fabsf only — both exactly-rounded
// IEEE operations, swept device-vs-host by port/sim/device/mathsweep.c.
static inline int ifloorf(float x) {
  const int i = (int)x;
  return i - (x < (float)i);
}
static inline int iceilf(float x) {
  const int i = (int)x;
  return i + (x > (float)i);
}

#define RAST_TWO_PI 6.28318530717958647692f

#define SUBS 4
#define MAXEDGES 65536
#define MAXPTS 8192
#define MAXACT 4096
#define TOL 0.25f

typedef struct { float ymin, ymax, x, dxdy; int dir; } Edge;

static Edge g_edges[MAXEDGES];
static int g_nedges;
static int g_order[MAXEDGES];
static int g_active[MAXACT];
static float g_xs[MAXACT];
static int g_wind[MAXACT];
static uint16_t g_cov[RAST_W]; // one row of AA coverage (0..256)

static float g_px[MAXPTS], g_py[MAXPTS];
static int g_npts;
static int g_subStart; // first point of the open subpath

// --- 565 helpers -----------------------------------------------------------

static inline uint16_t pack565(uint8_t r, uint8_t g, uint8_t b) {
  return (uint16_t)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
}

static inline uint16_t blend565(uint16_t dst, uint16_t col, unsigned a) {
  unsigned dr = dst & 0xF81F, dg = dst & 0x07E0; // r+b packed, g
  unsigned cr = col & 0xF81F, cg = col & 0x07E0;
  unsigned rr = ((cr * a + dr * (256 - a)) >> 8) & 0xF81F;
  unsigned rg = ((cg * a + dg * (256 - a)) >> 8) & 0x07E0;
  return (uint16_t)(rr | rg);
}

static int g_ink_on = 1; // M4 task 2 (raster.h note): bg art suppresses ink

void rast_ink_enable(int on) { g_ink_on = on ? 1 : 0; }

void rast_blend_px(Raster *rz, int x, int y, RastCol col, unsigned a256) {
  if (x < 0 || x >= RAST_W || y < rz->clipY0 || y >= rz->clipY1) return;
  unsigned a = (a256 * col.a256) >> 8;
  if (!a) return;
  if (a > 256) a = 256;
  const uint16_t c565 = pack565(col.r, col.g, col.b);
  const size_t idx = (size_t)y * RAST_W + (size_t)x;
  rz->fb[idx] = (a >= 256) ? c565 : blend565(rz->fb[idx], c565, a);
  if (g_ink_on) rz->ink[idx] = 1;
}

// --- batch blend primitives (M4 task 3, measured-hotspot class fix) --------
// ATTRIBUTION (device, .loop/m4-task3-prof-device.log): per-pixel
// rast_blend_px calls issued from -O2 TUs dominated the device render
// time — bg gradient 240x150 calls/frame (~4.0 ms), glyph/sprite mask
// blits (~1.5 ms avg, multi-ms banner peaks). These three primitives
// move the PIXEL LOOP into this -O3 TU with the clip/ink checks hoisted
// per row, replicating rast_blend_px's arithmetic EXACTLY (same integer
// ops, same order, same skip conditions) — bit-identical output by
// construction, proven mechanically by the x2 byte-stable renders + the
// pre/post-optimization host-shot cmp (AGENT-LOG iter 73).

// One full row [0,RAST_W) at y, OPAQUE colour (col.a256 must be 256):
// exactly the loop `for x: rast_blend_px(rz, x, y, col, 256)`.
void rast_fill_row_opaque(Raster *rz, int y, RastCol col) {
  if (col.a256 != 256) gfx_fatal("rast_fill_row_opaque: non-opaque colour");
  if (y < rz->clipY0 || y >= rz->clipY1) return;
  const uint16_t c565 = pack565(col.r, col.g, col.b);
  uint16_t *fbrow = &rz->fb[(size_t)y * RAST_W];
  for (int x = 0; x < RAST_W; x++) fbrow[x] = c565;
  if (g_ink_on) memset(&rz->ink[(size_t)y * RAST_W], 1, RAST_W);
}

// --- B9: the px8_over run primitives ---------------------------------------
// SAME CLASS, SAME DISCIPLINE as the three above, one caller further out.
// foh_render.c's px8_over (foh_render.c:102) composites in 8-bit against the
// UNPACKED framebuffer and then stores through rast_blend_px OPAQUE — the one
// path that dodges blend565's blue spill. That is correct but it pays a
// cross-TU call PER PIXEL from an -O2 TU, and the FOH's two biggest per-frame
// loops are exactly that call in a row-shaped loop:
//
//   span8        (foh_render.c:273) — every poly8/disc8/fill span
//   grad_radial  (foh_render.c:245) — the LUT replay of the cached radial
//
// MEASURED on the FunKey-S (Cortex-A7, .loop/b9 layer bench, N=200, title
// screen): the cached radial's replay alone cost 5.59 ms of a 16.67 ms frame
// and the 30-wedge fan 3.59 ms, with the whole title render at 16.96 ms —
// i.e. ABOVE the frame budget on its own, which is what the standing
// "1 FOH render skip" was: a ~0.3 ms/frame drift that trips the valve once
// every ~50 title ticks and is repaid by the skipped frame.
//
// Both take the loop into this -O3 TU and hoist the invariants (clip test,
// row base, ink flag, the source colour's pre-multiplied terms) out of the
// per-pixel body, replicating px8_over's arithmetic EXACTLY — same integer
// ops, same order, same skip and clamp conditions, same uint8_t narrowing —
// so the composited bytes are unchanged by construction. Proven mechanically
// by pre/post byte-identical shot cmp across all five FOH flows.
//
// CLIP: px8_over tests y against [0,RAST_H) and rast_blend_px then tests it
// against [clipY0,clipY1); a pixel had to pass BOTH, so both are kept.

// One row run [xa,xb) at y, CONSTANT colour, alpha `a` (0..256, clamped):
// exactly the loop `for x in [xa,xb): px8_over(rz, x, y, col, a)`.
// NOTE col.a256 is deliberately unread — px8_over ignores it too (it stores
// through an o.a256 == 256 colour), so `a` is the only alpha in play.
void rast_fill_run(Raster *rz, int y, int xa, int xb, RastCol col,
                   unsigned a) {
  if (a == 0) return;
  if (a > 256) a = 256;
  if (y < 0 || y >= RAST_H || y < rz->clipY0 || y >= rz->clipY1) return;
  if (xa < 0) xa = 0;
  if (xb > RAST_W) xb = RAST_W;
  if (xa >= xb) return;
  const size_t base = (size_t)y * RAST_W;
  uint16_t *const fb = &rz->fb[base];
  uint8_t *const ink = &rz->ink[base];
  if (a == 256) {
    // px8_over's opaque fast path: the destination cannot influence the
    // result, so this is the same store, not an approximation.
    const uint16_t c565 = pack565(col.r, col.g, col.b);
    for (int x = xa; x < xb; x++) fb[x] = c565;
    if (g_ink_on) memset(&ink[xa], 1, (size_t)(xb - xa));
    return;
  }
  // Hoisted: col.X * a is loop-invariant. Integer +, so regrouping the sum
  // `(c.X * a + dX * (256 - a)) >> 8` around a precomputed `c.X * a` is
  // value-identical, not an approximation.
  const unsigned na = 256 - a;
  const unsigned cr = (unsigned)col.r * a;
  const unsigned cg = (unsigned)col.g * a;
  const unsigned cb = (unsigned)col.b * a;
  for (int x = xa; x < xb; x++) {
    const uint16_t d = fb[x];
    // Low-bit replication, exactly px8_over's (foh_render.c:119-126).
    const unsigned r5 = (unsigned)((d >> 11) & 0x1F);
    const unsigned g6 = (unsigned)((d >> 5) & 0x3F);
    const unsigned b5 = (unsigned)(d & 0x1F);
    const unsigned dr = (r5 << 3) | (r5 >> 2);
    const unsigned dg = (g6 << 2) | (g6 >> 4);
    const unsigned db = (b5 << 3) | (b5 >> 2);
    fb[x] = pack565((uint8_t)((cr + dr * na) >> 8),
                    (uint8_t)((cg + dg * na) >> 8),
                    (uint8_t)((cb + db * na) >> 8));
    if (g_ink_on) ink[x] = 1;
  }
}

// One row run [xa,xb) at y from a PER-PIXEL colour row (each entry carries
// its own a256, 0 = skip): exactly the loop
//   `for x in [xa,xb): c = row[x]; if (!c.a256) continue;
//                      px8_over(rz, x, y, c, c.a256)`.
// `row` is indexed by ABSOLUTE x, like rz->fb — callers pass the row base of
// a RAST_W-strided source (grad_radial's LUT is exactly that shape).
void rast_blend_run(Raster *rz, int y, int xa, int xb, const RastCol *row) {
  if (!row) return;
  if (y < 0 || y >= RAST_H || y < rz->clipY0 || y >= rz->clipY1) return;
  if (xa < 0) xa = 0;
  if (xb > RAST_W) xb = RAST_W;
  const size_t base = (size_t)y * RAST_W;
  uint16_t *const fb = &rz->fb[base];
  uint8_t *const ink = &rz->ink[base];
  for (int x = xa; x < xb; x++) {
    const RastCol c = row[x];
    unsigned a = (unsigned)c.a256;
    if (a == 0) continue;
    if (a > 256) a = 256;
    if (a == 256) {
      fb[x] = pack565(c.r, c.g, c.b);
    } else {
      const unsigned na = 256 - a;
      const uint16_t d = fb[x];
      const unsigned r5 = (unsigned)((d >> 11) & 0x1F);
      const unsigned g6 = (unsigned)((d >> 5) & 0x3F);
      const unsigned b5 = (unsigned)(d & 0x1F);
      const unsigned dr = (r5 << 3) | (r5 >> 2);
      const unsigned dg = (g6 << 2) | (g6 >> 4);
      const unsigned db = (b5 << 3) | (b5 >> 2);
      fb[x] = pack565((uint8_t)(((unsigned)c.r * a + dr * na) >> 8),
                      (uint8_t)(((unsigned)c.g * a + dg * na) >> 8),
                      (uint8_t)(((unsigned)c.b * a + db * na) >> 8));
    }
    if (g_ink_on) ink[x] = 1;
  }
}

// One row run [xa,xb) at y through rast_blend_px's OWN arithmetic (i.e.
// through blend565, NOT px8_over's 8-bit composite): exactly the loop
//   `for x in [xa,xb): rast_blend_px(rz, x, y, col, a256)`.
// The font blitters (foh_font.c) call rast_blend_px directly rather than
// px8_over, so they need this variant and NOT rast_fill_run — the two differ
// whenever col.a256 < 256, and silently swapping one for the other would
// change every partial-alpha glyph. `a` is loop-invariant here, so the alpha
// product, the 565 pack and the ink fill all hoist out of the pixel body.
// MEASURED on device: "MELEE" at scale 5 (the title wordmark) 2.75 -> 0.55 ms.
void rast_blend_px_run(Raster *rz, int y, int xa, int xb, RastCol col,
                       unsigned a256) {
  if (y < rz->clipY0 || y >= rz->clipY1) return;
  unsigned a = (a256 * col.a256) >> 8;
  if (!a) return;
  if (a > 256) a = 256;
  if (xa < 0) xa = 0;
  if (xb > RAST_W) xb = RAST_W;
  if (xa >= xb) return;
  const uint16_t c565 = pack565(col.r, col.g, col.b);
  const size_t base = (size_t)y * RAST_W;
  uint16_t *const fb = &rz->fb[base];
  if (a >= 256) {
    for (int x = xa; x < xb; x++) fb[x] = c565;
  } else {
    for (int x = xa; x < xb; x++) fb[x] = blend565(fb[x], c565, a);
  }
  // rast_blend_px sets ink for every pixel it does NOT early-return on, and
  // `a` is constant across the run, so the whole run inks or none of it does.
  if (g_ink_on) memset(&rz->ink[base + (size_t)xa], 1, (size_t)(xb - xa));
}

// A8 mask blit: exactly gfx_overlay.c's blit_mask loop —
//   for y,x: a8 = mask[y*w+x]; if (!a8) continue;
//            rast_blend_px(rz, x0+x, y0+y, col, (a8*256)/255)
void rast_blit_a8mask(Raster *rz, const uint8_t *mask, int w, int h,
                      int x0, int y0, RastCol col) {
  if (!mask) return;
  const uint16_t c565 = pack565(col.r, col.g, col.b);
  for (int y = 0; y < h; y++) {
    const int py = y0 + y;
    if (py < rz->clipY0 || py >= rz->clipY1) continue;
    const uint8_t *mrow = &mask[(size_t)y * (size_t)w];
    for (int x = 0; x < w; x++) {
      const unsigned a8 = mrow[x];
      if (!a8) continue;
      const int px = x0 + x;
      if (px < 0 || px >= RAST_W) continue;
      unsigned a = (((a8 * 256u) / 255u) * col.a256) >> 8;
      if (!a) continue;
      if (a > 256) a = 256;
      const size_t idx = (size_t)py * RAST_W + (size_t)px;
      rz->fb[idx] = (a >= 256) ? c565 : blend565(rz->fb[idx], c565, a);
      if (g_ink_on) rz->ink[idx] = 1;
    }
  }
}

// RGBA sprite blit: exactly gfx_overlay.c's sprite loop —
//   for y,x: px = rgba[4*(y*w+x)]; if (!px[3]) continue;
//            rast_blend_px(rz, x0+x, y0+y, {px[0..2],256}, (px[3]*256)/255)
void rast_blit_rgba(Raster *rz, const uint8_t *rgba, int w, int h,
                    int x0, int y0) {
  if (!rgba) return;
  for (int y = 0; y < h; y++) {
    const int py = y0 + y;
    if (py < rz->clipY0 || py >= rz->clipY1) continue;
    const uint8_t *rrow = &rgba[4 * (size_t)y * (size_t)w];
    for (int x = 0; x < w; x++) {
      const uint8_t *p = &rrow[4 * x];
      if (!p[3]) continue;
      const int px = x0 + x;
      if (px < 0 || px >= RAST_W) continue;
      unsigned a = ((p[3] * 256u) / 255u); // col.a256 == 256: (a*256)>>8 == a
      if (!a) continue;
      if (a > 256) a = 256;
      const uint16_t c565 = pack565(p[0], p[1], p[2]);
      const size_t idx = (size_t)py * RAST_W + (size_t)px;
      rz->fb[idx] = (a >= 256) ? c565 : blend565(rz->fb[idx], c565, a);
      if (g_ink_on) rz->ink[idx] = 1;
    }
  }
}

void rast_clear(Raster *rz, uint8_t r, uint8_t g, uint8_t b,
                int clipY0, int clipY1) {
  const uint16_t c = pack565(r, g, b);
  for (int i = 0; i < RAST_W * RAST_H; i++) rz->fb[i] = c;
  memset(rz->ink, 0, sizeof(rz->ink));
  rz->clipY0 = clipY0 < 0 ? 0 : clipY0;
  rz->clipY1 = clipY1 > RAST_H ? RAST_H : clipY1;
}

// --- path building -----------------------------------------------------------

void rast_path_reset(void) { g_nedges = 0; g_npts = 0; g_subStart = 0; }

static void edge_add(float x0, float y0, float x1, float y1) {
  if (y0 == y1) return;
  if (g_nedges >= MAXEDGES - 1) gfx_fatal("raster: edge overflow");
  Edge *e = &g_edges[g_nedges];
  if (y0 < y1) { e->ymin = y0; e->ymax = y1; e->x = x0; e->dir = 1; }
  else         { e->ymin = y1; e->ymax = y0; e->x = x1; e->dir = -1; }
  e->dxdy = (x1 - x0) / (y1 - y0);
  g_nedges++;
}

static void pt_add(float x, float y) {
  if (g_npts >= MAXPTS) gfx_fatal("raster: point overflow");
  g_px[g_npts] = x; g_py[g_npts] = y; g_npts++;
}

void rast_sub_begin(float x, float y) { g_subStart = g_npts; pt_add(x, y); }

void rast_sub_line(float x, float y) { pt_add(x, y); }

// fixed-count subdivision derived from control-polygon deviation vs
// tolerance (rastbench.c flatten_cubic, verbatim)
void rast_sub_cubic(float x1, float y1, float x2, float y2,
                    float x3, float y3) {
  if (g_npts == 0) gfx_fatal("raster: cubic with no current point");
  const float x0 = g_px[g_npts - 1], y0 = g_py[g_npts - 1];
  const float dx = x3 - x0, dy = y3 - y0;
  const float d1 = fabsf((x1 - x0) * dy - (y1 - y0) * dx);
  const float d2 = fabsf((x2 - x0) * dy - (y2 - y0) * dx);
  const float len = sqrtf(dx * dx + dy * dy) + 1e-6f;
  const float dev = (d1 > d2 ? d1 : d2) / len; // max ctrl-pt deviation, px
  int n = 1 + (int)sqrtf(3.0f * dev / TOL);
  if (n > 24) n = 24;
  const float dt = 1.0f / (float)n;
  for (int i = 1; i <= n; i++) {
    const float t = (float)i * dt, mt = 1.0f - t;
    const float a = mt * mt * mt, b = 3 * mt * mt * t,
                c = 3 * mt * t * t, d = t * t * t;
    pt_add(a * x0 + b * x1 + c * x2 + d * x3,
           a * y0 + b * y1 + c * y2 + d * y3);
  }
}

void rast_sub_close(void) {
  const int n = g_npts - g_subStart;
  if (n < 2) { g_npts = g_subStart; return; } // degenerate subpath
  for (int i = g_subStart; i + 1 < g_npts; i++) {
    edge_add(g_px[i], g_py[i], g_px[i + 1], g_py[i + 1]);
  }
  edge_add(g_px[g_npts - 1], g_py[g_npts - 1],
           g_px[g_subStart], g_py[g_subStart]);
  g_npts = g_subStart; // points recycled; edges keep the geometry
  g_subStart = g_npts;
}

// --- fill ----------------------------------------------------------------------

static int cmp_edge_ymin(const void *a, const void *b) {
  const float ya = g_edges[*(const int *)a].ymin;
  const float yb = g_edges[*(const int *)b].ymin;
  return (ya > yb) - (ya < yb);
}

void rast_fill(Raster *rz, RastCol col) {
  if (g_nedges == 0) { rast_path_reset(); return; }
  const uint16_t c565 = pack565(col.r, col.g, col.b);
  float ymin = 1e9f, ymax = -1e9f;
  for (int i = 0; i < g_nedges; i++) {
    g_order[i] = i;
    if (g_edges[i].ymin < ymin) ymin = g_edges[i].ymin;
    if (g_edges[i].ymax > ymax) ymax = g_edges[i].ymax;
  }
  qsort(g_order, (size_t)g_nedges, sizeof(int), cmp_edge_ymin);
  int y0 = ifloorf(ymin); if (y0 < rz->clipY0) y0 = rz->clipY0;
  int y1 = iceilf(ymax);  if (y1 > rz->clipY1) y1 = rz->clipY1;
  int next = 0, nact = 0;
  const float substep = 1.0f / SUBS;
  const unsigned cov_inc = 256 / SUBS;

  for (int y = y0; y < y1; y++) {
    // Touched-column window (M4 task 3, measured-hotspot class fix —
    // many-small-fill frames paid a full 240-column memset + scan per
    // row per fill call): g_cov is all-zero OUTSIDE the window by
    // invariant (static zero init + the end-of-row re-zero below), the
    // accumulation tracks [covLo, covHi], and the blend loop visits
    // only that window. BIT-IDENTICAL: every skipped column holds
    // cv == 0, which the old loop skipped via `if (!cv) continue`.
    int covLo = RAST_W, covHi = -1;
    for (int s = 0; s < SUBS; s++) {
      const float sy = (float)y + ((float)s + 0.5f) * substep;
      while (next < g_nedges && g_edges[g_order[next]].ymin <= sy) {
        if (nact >= MAXACT) gfx_fatal("raster: active-edge overflow");
        g_active[nact++] = g_order[next++];
      }
      int n = 0;
      for (int i = 0; i < nact; i++) {
        Edge *e = &g_edges[g_active[i]];
        if (e->ymax <= sy) { g_active[i--] = g_active[--nact]; continue; }
        if (e->ymin <= sy) {
          g_xs[n] = e->x + (sy - e->ymin) * e->dxdy;
          g_wind[n] = e->dir;
          n++;
        }
      }
      for (int i = 1; i < n; i++) { // insertion sort by x
        const float x = g_xs[i]; const int w = g_wind[i]; int j = i - 1;
        while (j >= 0 && g_xs[j] > x) {
          g_xs[j + 1] = g_xs[j]; g_wind[j + 1] = g_wind[j]; j--;
        }
        g_xs[j + 1] = x; g_wind[j + 1] = w;
      }
      int wind = 0; float spanx = 0;
      for (int i = 0; i < n; i++) {
        const int prev = wind;
        wind += g_wind[i];
        if (prev == 0 && wind != 0) spanx = g_xs[i];
        else if (prev != 0 && wind == 0) {
          float xa = spanx, xb = g_xs[i];
          if (xa < 0) xa = 0;
          if (xb > RAST_W) xb = RAST_W;
          if (xb <= xa) continue;
          const int ia = ifloorf(xa), ib = ifloorf(xb);
          if (ia < covLo) covLo = ia;
          if (ia == ib) {
            g_cov[ia] += (uint16_t)((xb - xa) * (float)cov_inc);
            if (ia > covHi) covHi = ia;
          } else {
            g_cov[ia] += (uint16_t)(((float)(ia + 1) - xa) * (float)cov_inc);
            for (int x = ia + 1; x < ib; x++) g_cov[x] += (uint16_t)cov_inc;
            if (ib < RAST_W) {
              g_cov[ib] += (uint16_t)((xb - (float)ib) * (float)cov_inc);
              if (ib > covHi) covHi = ib;
            } else if (ib - 1 > covHi) {
              covHi = ib - 1; // interior loop wrote up to ib-1
            }
          }
        }
      }
    }
    if (covHi < covLo) continue; // no coverage on this row (invariant holds)
    uint16_t *row = rz->fb + (size_t)y * RAST_W;
    uint8_t *inkrow = rz->ink + (size_t)y * RAST_W;
    for (int x = covLo; x <= covHi; x++) {
      unsigned cv = g_cov[x];
      if (!cv) continue;
      if (cv > 256) cv = 256;
      const unsigned a = (cv * col.a256) >> 8; // coverage x colour alpha
      if (!a) continue;
      row[x] = (a >= 256) ? c565 : blend565(row[x], c565, a);
      if (g_ink_on) inkrow[x] = 1;
    }
    // restore the all-zero invariant for the touched window only
    memset(&g_cov[covLo], 0, (size_t)(covHi - covLo + 1) * sizeof(g_cov[0]));
  }
  rast_path_reset();
}

// --- prims ----------------------------------------------------------------------

void rast_poly(Raster *rz, const float *xy, int n, RastCol col) {
  rast_path_reset();
  rast_sub_begin(xy[0], xy[1]);
  for (int i = 1; i < n; i++) rast_sub_line(xy[i * 2], xy[i * 2 + 1]);
  rast_sub_close();
  rast_fill(rz, col);
}

void rast_stroke_seg(Raster *rz, float x0, float y0, float x1, float y1,
                     float w, RastCol col) {
  float dx = x1 - x0, dy = y1 - y0;
  const float len = sqrtf(dx * dx + dy * dy);
  if (len < 1e-9f) return;
  const float nx = -dy / len * (w * 0.5f), ny = dx / len * (w * 0.5f);
  const float quad[8] = { x0 + nx, y0 + ny, x1 + nx, y1 + ny,
                          x1 - nx, y1 - ny, x0 - nx, y0 - ny };
  rast_poly(rz, quad, 4, col);
}

#define CIRCLE_SEGS 32

// Unit-circle table (M4 task 3, measured-hotspot class fix): the 32
// segment angles are FIXED compile-time constants, yet every
// rast_circle/rast_ring call burned 64/124 fdlibm double-precision trig
// calls (stock icons alone ~1,500/frame on device). The table entries
// are computed ONCE by the IDENTICAL expressions the loops used inline
// — same float bit patterns, so every derived vertex is bit-identical.
static float g_circ_cos[CIRCLE_SEGS], g_circ_sin[CIRCLE_SEGS];
static int g_circ_init = 0;
static void circ_init(void) {
  if (g_circ_init) return;
  g_circ_cos[0] = 1.0f; // unused (i starts at 1; begin point is literal)
  g_circ_sin[0] = 0.0f;
  for (int i = 1; i < CIRCLE_SEGS; i++) {
    const float t = (float)i * (RAST_TWO_PI / CIRCLE_SEGS);
    g_circ_cos[i] = (float)fd_cos((double)t);
    g_circ_sin[i] = (float)fd_sin((double)t);
  }
  g_circ_init = 1;
}

void rast_circle(Raster *rz, float cx, float cy, float rad, RastCol col) {
  circ_init();
  rast_path_reset();
  rast_sub_begin(cx + rad, cy);
  for (int i = 1; i < CIRCLE_SEGS; i++) {
    rast_sub_line(cx + rad * g_circ_cos[i], cy + rad * g_circ_sin[i]);
  }
  rast_sub_close();
  rast_fill(rz, col);
}

void rast_ring(Raster *rz, float cx, float cy, float rad, float w,
               RastCol col) {
  const float ro = rad + w * 0.5f;
  float ri = rad - w * 0.5f;
  if (ri < 0) ri = 0;
  circ_init(); // bit-identical unit-circle table (note above)
  rast_path_reset();
  rast_sub_begin(cx + ro, cy);
  for (int i = 1; i < CIRCLE_SEGS; i++) {
    rast_sub_line(cx + ro * g_circ_cos[i], cy + ro * g_circ_sin[i]);
  }
  rast_sub_close();
  // inner contour wound the other way -> nonzero-winding annulus
  rast_sub_begin(cx + ri, cy);
  for (int i = CIRCLE_SEGS - 1; i >= 1; i--) {
    rast_sub_line(cx + ri * g_circ_cos[i], cy + ri * g_circ_sin[i]);
  }
  rast_sub_close();
  rast_fill(rz, col);
}
