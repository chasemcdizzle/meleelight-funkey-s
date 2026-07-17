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
  int y0 = (int)floorf(ymin); if (y0 < rz->clipY0) y0 = rz->clipY0;
  int y1 = (int)ceilf(ymax);  if (y1 > rz->clipY1) y1 = rz->clipY1;
  int next = 0, nact = 0;
  const float substep = 1.0f / SUBS;
  const unsigned cov_inc = 256 / SUBS;

  for (int y = y0; y < y1; y++) {
    memset(g_cov, 0, sizeof(g_cov));
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
          const int ia = (int)floorf(xa), ib = (int)floorf(xb);
          if (ia == ib) {
            g_cov[ia] += (uint16_t)((xb - xa) * (float)cov_inc);
          } else {
            g_cov[ia] += (uint16_t)(((float)(ia + 1) - xa) * (float)cov_inc);
            for (int x = ia + 1; x < ib; x++) g_cov[x] += (uint16_t)cov_inc;
            if (ib < RAST_W) {
              g_cov[ib] += (uint16_t)((xb - (float)ib) * (float)cov_inc);
            }
          }
        }
      }
    }
    uint16_t *row = rz->fb + (size_t)y * RAST_W;
    uint8_t *inkrow = rz->ink + (size_t)y * RAST_W;
    for (int x = 0; x < RAST_W; x++) {
      unsigned cv = g_cov[x];
      if (!cv) continue;
      if (cv > 256) cv = 256;
      const unsigned a = (cv * col.a256) >> 8; // coverage x colour alpha
      if (!a) continue;
      row[x] = (a >= 256) ? c565 : blend565(row[x], c565, a);
      inkrow[x] = 1;
    }
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

void rast_circle(Raster *rz, float cx, float cy, float rad, RastCol col) {
  rast_path_reset();
  rast_sub_begin(cx + rad, cy);
  for (int i = 1; i < CIRCLE_SEGS; i++) {
    const float t = (float)i * (RAST_TWO_PI / CIRCLE_SEGS);
    rast_sub_line(cx + rad * cosf(t), cy + rad * sinf(t));
  }
  rast_sub_close();
  rast_fill(rz, col);
}

void rast_ring(Raster *rz, float cx, float cy, float rad, float w,
               RastCol col) {
  const float ro = rad + w * 0.5f;
  float ri = rad - w * 0.5f;
  if (ri < 0) ri = 0;
  rast_path_reset();
  rast_sub_begin(cx + ro, cy);
  for (int i = 1; i < CIRCLE_SEGS; i++) {
    const float t = (float)i * (RAST_TWO_PI / CIRCLE_SEGS);
    rast_sub_line(cx + ro * cosf(t), cy + ro * sinf(t));
  }
  rast_sub_close();
  // inner contour wound the other way -> nonzero-winding annulus
  rast_sub_begin(cx + ri, cy);
  for (int i = CIRCLE_SEGS - 1; i >= 1; i--) {
    const float t = (float)i * (RAST_TWO_PI / CIRCLE_SEGS);
    rast_sub_line(cx + ri * cosf(t), cy + ri * sinf(t));
  }
  rast_sub_close();
  rast_fill(rz, col);
}
