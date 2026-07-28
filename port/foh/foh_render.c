// port/foh/foh_render.c — FOH screen rendering (fix_plan §M4 task 9).
// REWRITTEN look at 240x240 over the raster prims (never a DOM port);
// menus are NOT checksummed — visual authority is Chase's acceptance
// playthrough (task 10 owns the device look pass). Deterministic by
// construction: pure function of FohState, no RNG, no clock — the
// check's shot byte-stability x2 depends on it.
#include "foh.h"

#include <math.h> // sqrtf/fabsf/fabs only — see the device-libm note below

// DEVICE LIBM (M3 task 1 class finding, CLAUDE.md §Commands): the FunKey SDK's
// static musl math was built with unsafe-FP optimizations, so NO device libc
// math symbol is trustworthy without a differential sweep. port/gfx/raster.c:17
// already fixed this class for the renderer by routing circle/ring vertices
// through the vendored fd_cos/fd_sin; these prims follow the same rule. sqrtf/
// fabsf are the documented exception (exactly-rounded VFP instructions).
#include "../fdlibm/fdlibm.h"

static float fd_cosf(float x) { return (float)fd_cos((double)x); }
static float fd_sinf(float x) { return (float)fd_sin((double)x); }
#include <stdio.h>  // snprintf (the task-13 records line)
#include <string.h> // memcpy (the backdrop cache)

// Labels: faithful strings from the upstream tables (cited), uppercased
// for the 5x7 font. These are UI text of a rewritten non-checksummed
// surface, not engine values (HARD RULE 5 concerns data planes).
static const char *kMenuTitle[4] = {
    // menuTitle (menu.js:32)
    "MAIN MENU", "OPTIONS", "BATTLE MODE", "CONTROLS"};
static const char *kMenuText[4][4] = {
    // menuText (menu.js:19-24)
    {"VS. MELEE", "TARGET TEST", "TARGET BUILDER", "OPTIONS"},
    {"AUDIO", "GAMEPLAY", "KEYBOARD CONTROLS", "CREDITS"},
    {"LOCAL VS", "SPECTATE", "P2P", "SERVER"},
    {"CONTROLLER", "KEYBOARD"},
};
static const char *kCharNames[5] = {
    // characters.js:2-8 order == the oracle char ids
    "MARTH", "JIGGLYPUFF", "FOX", "FALCO", "CAPTAIN FALCON"};
static const char *kStageNames[7] = {
    // stageselect.js:13-29 order == the oracle stage ids; slot 6 = the
    // visible-but-refusing RANDOM slot (registered exclusion, foh.h)
    "BATTLEFIELD",  "YOSHI'S STORY",     "POKEMON STADIUM",
    "DREAMLAND",    "FINAL DESTINATION", "FOUNTAIN OF DREAMS",
    "RANDOM"};
static const char *kLCancelNames[3] = {
    // settings.js:46 comment: 0 normal | 1 auto | 2 smash64
    "NORMAL", "AUTO", "SMASH64"};

// Upstream menuExplanation (menu.js:25-30), uppercased for the bitmap
// faces. Same provenance class as kMenuText above: UI strings of a
// rewritten, non-checksummed surface.
static const char *kMenuExpl[4][4] = {
    {"MULTIPLAYER BATTLES!", "SMASH TEN TARGETS!", "BUILD TARGET TEST STAGES!",
     "GAME SETUP."},
    {"SELECT AUDIO LEVELS.", "CHANGE GAMEPLAY SETTINGS.",
     "CUSTOMIZE & CALIBRATE CONTROLS.", "WHO DID THIS?"},
    {"ONE BOX THIS SCREEN.", "RANKED MODE", "HOSTLESS MULIPLAYER",
     "HOSTED MULTIPLAYER"},
    {"CUSTOMIZE & CALIBRATE CONTROLLER.", "CUSTOMIZE KEYBOARD CONTROLS.", "",
     ""},
};

static const RastCol kBg = {12, 12, 28, 256};
static const RastCol kPanel = {30, 30, 60, 256};
static const RastCol kText = {220, 220, 230, 256};
static const RastCol kDim = {120, 120, 140, 256};
static const RastCol kAccent = {255, 200, 60, 256};
static const RastCol kCursor = {90, 160, 255, 256};

// --- 8-bit source-over, then an OPAQUE store ------------------------------
// MEASURED BUG (A1 restyle, iter A1-0): raster.c's blend565 (port/gfx/
// raster.c:69) packs r and b into one field and shifts the weighted sum by
// 8 — but the red term's low bits land inside the blue field's 5 bits, so
// ANY partial-alpha blend corrupts blue by up to 24/255. Numerically:
// rgb(147,14,42) at a=87 over rgb(39,0,91) returns b=25 where the correct
// answer is 9. On antialiased EDGES (raster.c's only pre-existing partial-
// alpha caller) that is a 1 px fringe nobody had reason to notice; on the
// full-screen area fills this restyle needs it is catastrophic banding.
//
// raster.c is the sim's -O3 hot TU and feeds the frozen banner ink pins, so
// it is NOT this task's to change (registered for the driver). Instead every
// area fill below composites in 8-bit against the unpacked framebuffer and
// stores through rast_blend_px OPAQUE — the one path that is exact — which
// also keeps the clip band and the ink plane honoured.
static void px8_over(Raster *rz, int x, int y, RastCol c, unsigned a) {
  if (a == 0) return;
  if (a > 256) a = 256;
  if (x < 0 || x >= RAST_W || y < 0 || y >= RAST_H) return;
  const uint16_t d = rz->fb[(size_t)y * RAST_W + (size_t)x];
  // Low-bit replication: a 5-bit channel's true 8-bit value is (v<<3)|(v>>2),
  // not v<<3 — the naive form reads every destination up to 7/255 dark, and
  // layered partial-alpha passes compound that bias.
  const unsigned r5 = (unsigned)((d >> 11) & 0x1F), g6 = (unsigned)((d >> 5) & 0x3F);
  const unsigned b5 = (unsigned)(d & 0x1F);
  const unsigned dr = (r5 << 3) | (r5 >> 2);
  const unsigned dg = (g6 << 2) | (g6 >> 4);
  const unsigned db = (b5 << 3) | (b5 >> 2);
  RastCol o;
  o.r = (uint8_t)(((unsigned)c.r * a + dr * (256 - a)) >> 8);
  o.g = (uint8_t)(((unsigned)c.g * a + dg * (256 - a)) >> 8);
  o.b = (uint8_t)(((unsigned)c.b * a + db * (256 - a)) >> 8);
  o.a256 = 256;
  rast_blend_px(rz, x, y, o, 256);
}

static void fill_rect(Raster *rz, int x, int y, int w, int h, RastCol c) {
  for (int yy = y; yy < y + h; yy++) {
    for (int xx = x; xx < x + w; xx++) {
      px8_over(rz, xx, yy, c, c.a256);
    }
  }
}

// ===========================================================================
// A1 RESTYLE — PHASE 0 SHARED PRIMITIVES
// ===========================================================================
// Upstream draws its menus with the 2D canvas API on a 1200x750 stage
// (menu.js / startscreen.js). None of that is carryable: no canvas, no
// browser text, a 240x240 RGB565 framebuffer and a 5x integer downscale
// that is NOT uniform (240/1200 = 0.20 in x, 240/750 = 0.32 in y). So the
// LOOK is re-authored at 240x240 from the upstream primitives, cited per
// call site; the geometry constants below are ours, the colours/rhythm are
// upstream's.
//
// All four prims are pure functions of their arguments (+ FohState.frame),
// so foh_render stays deterministic and the flows check's shot
// byte-stability x2 arm keeps holding.

// --- prim 1: two-stop gradients (linear + radial) --------------------------
// Upstream uses createLinearGradient (menu.js:259-263, the menu backdrop)
// and createRadialGradient (startscreen.js:20-27, the title backdrop; :85-89
// the centre bloom). Two stops covers every call site we re-author; the
// title's 5-stop ramp collapses to its endpoints (#27005b -> #38005b), a
// 3-bit-per-channel difference the RGB565 framebuffer cannot resolve anyway.

static RastCol lerp_col(RastCol a, RastCol b, float t) {
  RastCol o;
  o.r = (uint8_t)((float)a.r + ((float)b.r - (float)a.r) * t + 0.5f);
  o.g = (uint8_t)((float)a.g + ((float)b.g - (float)a.g) * t + 0.5f);
  o.b = (uint8_t)((float)a.b + ((float)b.b - (float)a.b) * t + 0.5f);
  o.a256 = (uint16_t)((float)a.a256 + ((float)b.a256 - (float)a.a256) * t +
                      0.5f);
  return o;
}

// Linear ramp from (x0,y0) to (x1,y1): t = clamped projection onto the axis.
static void grad_linear(Raster *rz, float x0, float y0, float x1, float y1,
                        RastCol c0, RastCol c1) {
  const float dx = x1 - x0, dy = y1 - y0;
  const float len2 = dx * dx + dy * dy;
  if (len2 <= 0.0f) gfx_fatal("foh_render: degenerate linear gradient");
  for (int y = 0; y < RAST_H; y++) {
    for (int x = 0; x < RAST_W; x++) {
      float t = (((float)x - x0) * dx + ((float)y - y0) * dy) / len2;
      if (t < 0.0f) t = 0.0f;
      if (t > 1.0f) t = 1.0f;
      const RastCol c = lerp_col(c0, c1, t);
      px8_over(rz, x, y, c, c.a256);
    }
  }
}

// Radial ramp centred (cx,cy): t = clamp(dist / rad). Carrying alpha in the
// stops is what makes the title's red/grey blooms possible without a
// shadowBlur (startscreen.js:30-33 uses one; we ramp alpha to 0 instead).
static void grad_radial(Raster *rz, float cx, float cy, float rad,
                        RastCol c0, RastCol c1) {
  if (rad <= 0.0f) gfx_fatal("foh_render: degenerate radial gradient");
  for (int y = 0; y < RAST_H; y++) {
    for (int x = 0; x < RAST_W; x++) {
      const float ddx = (float)x - cx, ddy = (float)y - cy;
      float t = sqrtf(ddx * ddx + ddy * ddy) / rad;
      if (t > 1.0f) t = 1.0f;
      const RastCol c = lerp_col(c0, c1, t);
      if (c.a256 == 0) continue;
      px8_over(rz, x, y, c, c.a256);
    }
  }
}

// Crisp 1 px ring / line plotted straight through px8_over. The raster's
// rast_ring/rast_stroke_seg antialias, which means partial coverage, which
// means the blend565 blue-spill above — visible as blue speckle on the faint
// decorative strokes. These are integer-stepped and exact.
// Round-to-nearest that is CORRECT FOR NEGATIVES. (int)(v + 0.5f) truncates
// toward zero, so -0.15 and -1.15 both land on 0 — which silently applies a
// partial-alpha composite twice. The +1024 bias makes the floor exact for the
// whole coordinate range these prims see, with no libc call (device floorf is
// one of the unsafe-FP musl symbols, M3 task 1). Exact for v > -1024.5; every
// live caller sits within a few hundred px of the origin.
// The bias is added in DOUBLE: in binary32, v + 1024.5f loses low bits and
// 200.499954f rounds UP before the cast (measured: 4 pixels dropped from a
// pulse annulus at menuTimer 24).
static int iround(float v) { return (int)((double)v + 1024.5) - 1024; }

// SPAN CONVENTION (shared by every fill below): pixel x covers [x, x+1) with
// its centre at x+0.5, and every span is HALF-OPEN [xa, xb). Half-open is what
// makes adjacent spans tile without sharing a pixel — and a shared pixel is a
// real defect here, because partial-alpha source-over is not idempotent.
static void span8(Raster *rz, int xa, int xb, int y, RastCol c, unsigned a) {
  for (int x = xa; x < xb; x++) px8_over(rz, x, y, c, a);
}

// Exact filled disc: ONE half-open span per row, so every destination pixel is
// composited exactly once.
static void disc8(Raster *rz, float cx, float cy, float r, RastCol c,
                  unsigned a) {
  if (r < 0.0f) return;
  const int y0 = iround(cy - r) - 1, y1 = iround(cy + r) + 1;
  for (int y = y0; y <= y1; y++) {
    const float dy = (float)y + 0.5f - cy;
    const float t = r * r - dy * dy;
    if (t < 0.0f) continue;
    const float dx = sqrtf(t);
    span8(rz, iround(cx - dx), iround(cx + dx), y, c, a);
  }
}

// Exact annulus of WIDTH w, rasterised in ONE pass. Stacking adjacent unit
// rings to fake a thick one double-composites their shared pixels (measured:
// r=10 and r=11 both emit (196,58)), so thickness is a parameter, not a loop
// over radii. At most two disjoint half-open spans per row.
static void ring8(Raster *rz, float cx, float cy, float r, float w, RastCol c,
                  unsigned a) {
  if (w <= 0.0f) return;
  const float ro = r + w * 0.5f;
  float ri = r - w * 0.5f;
  if (ri <= 0.0f) { disc8(rz, cx, cy, ro, c, a); return; }
  const int y0 = iround(cy - ro) - 1, y1 = iround(cy + ro) + 1;
  for (int y = y0; y <= y1; y++) {
    const float dy = (float)y + 0.5f - cy;
    const float to = ro * ro - dy * dy;
    if (to < 0.0f) continue;
    const float dxo = sqrtf(to);
    const float ti = ri * ri - dy * dy;
    if (ti < 0.0f) { // row passes through the hole's cap: one solid span
      span8(rz, iround(cx - dxo), iround(cx + dxo), y, c, a);
      continue;
    }
    // Two spans, disjoint by construction: [cx-dxo, cx-dxi) and [cx+dxi, cx+dxo).
    const float dxi = sqrtf(ti);
    span8(rz, iround(cx - dxo), iround(cx - dxi), y, c, a);
    span8(rz, iround(cx + dxi), iround(cx + dxo), y, c, a);
  }
}

// Integer Bresenham, HALF-OPEN (the final pixel belongs to the next segment).
// The previous float DDA floored its step count, so a 2.7 px segment took 2
// steps of 1.35 and left GAPS (measured on the title ray fan:
// (142,203) -> (142,205)); Bresenham emits exactly one pixel per step, so it
// can neither gap nor repeat.
static void line8(Raster *rz, float x0f, float y0f, float x1f, float y1f,
                  RastCol c, unsigned a) {
  int x = iround(x0f), y = iround(y0f);
  const int xe = iround(x1f), ye = iround(y1f);
  int dx = xe - x, dy = ye - y;
  const int sx = dx >= 0 ? 1 : -1, sy = dy >= 0 ? 1 : -1;
  if (dx < 0) dx = -dx;
  if (dy < 0) dy = -dy;
  int err = dx - dy;
  while (x != xe || y != ye) {
    px8_over(rz, x, y, c, a);
    const int e2 = 2 * err;
    if (e2 > -dy) { err -= dy; x += sx; }
    if (e2 < dx) { err += dx; y += sy; }
  }
}

// w-px stroke = w unit-offset copies along the segment normal.
// NOTE: unlike the span fills above, a multi-layer stroke does NOT guarantee
// one composite per pixel — parallel offsets can round together, and a closed
// outline revisits its vertices. That is invisible while every caller is
// opaque (source-over with a == 256 is idempotent), so the invariant is
// enforced rather than assumed: a partial-alpha stroke would silently
// reintroduce the exact defect class this renderer was built to avoid.
static void lineW8(Raster *rz, float x0, float y0, float x1, float y1,
                   float w, RastCol c, unsigned a) {
  if (a != 256 || c.a256 != 256) {
    gfx_fatal("foh_render: thick stroke requires opaque colour (see note)");
  }
  int layers = (int)w;
  if (layers < 1) layers = 1;
  const float dx = x1 - x0, dy = y1 - y0;
  const float len = sqrtf(dx * dx + dy * dy);
  const float nx = len > 0.0f ? -dy / len : 0.0f;
  const float ny = len > 0.0f ? dx / len : 0.0f;
  for (int o = 0; o < layers; o++) {
    const float d = (float)o - (float)(layers - 1) * 0.5f;
    line8(rz, x0 + nx * d, y0 + ny * d, x1 + nx * d, y1 + ny * d, c, a);
  }
}

// Crisp nonzero-winding scanline fill. rast_poly antialiases its edges, and
// partial coverage is exactly what blend565 gets wrong (see px8_over), so
// every filled shape in the FOH goes through here instead. Nonzero winding
// (not even-odd) because the menu-bar cap is three NESTED arcs.
#define FOH_SCAN_MAX 96
static void poly8(Raster *rz, const float *xy, int n, RastCol c, unsigned a) {
  if (n < 3) return;
  float ymin = xy[1], ymax = xy[1];
  for (int k = 1; k < n; k++) {
    if (xy[2 * k + 1] < ymin) ymin = xy[2 * k + 1];
    if (xy[2 * k + 1] > ymax) ymax = xy[2 * k + 1];
  }
  int y0 = (int)ymin, y1 = (int)ymax + 1;
  if (y0 < 0) y0 = 0;
  if (y1 > RAST_H) y1 = RAST_H;
  for (int y = y0; y < y1; y++) {
    const float sy = (float)y + 0.5f;
    float xs[FOH_SCAN_MAX];
    int wind[FOH_SCAN_MAX];
    int m = 0;
    for (int k = 0; k < n; k++) {
      const int j = (k + 1) % n;
      const float ay = xy[2 * k + 1], by = xy[2 * j + 1];
      if ((ay <= sy) == (by <= sy)) continue;
      if (m >= FOH_SCAN_MAX) gfx_fatal("foh_render: scanline crossings overflow");
      const float ax = xy[2 * k], bx = xy[2 * j];
      xs[m] = ax + (bx - ax) * (sy - ay) / (by - ay);
      wind[m] = by > ay ? 1 : -1;
      m++;
    }
    for (int i = 1; i < m; i++) { // insertion sort by x
      const float kx = xs[i];
      const int kw = wind[i];
      int j2 = i - 1;
      while (j2 >= 0 && xs[j2] > kx) {
        xs[j2 + 1] = xs[j2]; wind[j2 + 1] = wind[j2]; j2--;
      }
      xs[j2 + 1] = kx; wind[j2 + 1] = kw;
    }
    int w = 0;
    for (int i = 0; i + 1 < m; i++) {
      w += wind[i];
      if (w == 0) continue;
      int xa = iround(xs[i]), xb = iround(xs[i + 1]);
      if (xa < 0) xa = 0;
      if (xb > RAST_W) xb = RAST_W;
      for (int x = xa; x < xb; x++) px8_over(rz, x, y, c, a);
    }
  }
}

// --- prim 2: the grid + shine backdrop -------------------------------------
// Upstream: 60 vertical + 60 horizontal lines at a 30 px pitch, lineWidth 3,
// rgba(255,255,255,0.13) (menu.js:296-305). At 240x240 a 30 px pitch would
// leave 8 cells; re-authored to a 14 px pitch (17 cells) at 1 px, which is
// the same visual density as the reference at this size.
//
// The SHINE is ours: upstream's motion in this layer is the dot travelling
// the fg2 ellipse (menu.js:347-354) plus the rising bg2 boxes (:318-332).
// Both need per-frame state we do not carry; a slow diagonal brightening
// sweep over the grid reads the same and is a pure function of `frame`.
#define FOH_GRID_PITCH 14

static void grid_shine(Raster *rz, int frame) {
  const RastCol line = {255, 255, 255, 256};
  // The sweep runs over the x+y diagonal (0..478) and BOUNCES back, so the
  // cycle closes without the teleport a saw wave would give at its wrap. One
  // leg is ~530 frames, the same order as upstream's 600-frame
  // menuGlobalTimer wrap (menu.js:311).
  const int ph = frame % 1060;
  const float sweep = (float)(ph < 530 ? ph : 1060 - ph) * 0.905f;
  for (int y = 0; y < RAST_H; y++) {
    for (int x = 0; x < RAST_W; x++) {
      if (x % FOH_GRID_PITCH != 0 && y % FOH_GRID_PITCH != 0) continue;
      const float d = fabsf((float)(x + y) - sweep);
      unsigned a = 33; // 0.13 * 256
      if (d < 46.0f) a += (unsigned)(54.0f * (1.0f - d / 46.0f));
      px8_over(rz, x, y, line, a);
    }
  }
}

// --- prim 3: the menu-bar widget -------------------------------------------
// Upstream menu.js:445-527. One bar = a flat body, a LEFT point (the two
// short segments at :455-456) and a RIGHT circular cap built from three
// nested arcs of r 35 / 20 / 10 (:449-453) which read as a spiral. Selected
// rows swap the rgba(0,0,0,0.76) fill for rgb(254,238,27) with BLACK text
// and gain the pulsing concentric rings (:486-520).
//
// Re-authored at 240x240 (all four bars 134 px of flat body, 22 px tall,
// cap r 11, diagonal cascade 11 px per row against upstream's 65/1200):
#define FOH_BAR_H 22
#define FOH_BAR_R 11
#define FOH_BAR_STEP 11
#define FOH_BAR_TOP 58
#define FOH_BAR_PITCH 31
#define FOH_BAR_LEFT 62
#define FOH_BAR_CAP 196

static float bar_left(int i) { return (float)(FOH_BAR_LEFT - FOH_BAR_STEP * i); }
static float bar_cap(int i) { return (float)(FOH_BAR_CAP - FOH_BAR_STEP * i); }
static float bar_top(int i) {
  return (float)(FOH_BAR_TOP + FOH_BAR_PITCH * i);
}

#define FOH_BAR_MAXPTS 80

// Append one point, capacity-checked BEFORE the store.
static int put_pt(float *xy, int n, float x, float y) {
  if (n >= FOH_BAR_MAXPTS) gfx_fatal("foh_render: outline point overflow");
  xy[2 * n] = x;
  xy[2 * n + 1] = y;
  return n + 1;
}

// Append a circular arc to an outline point list (a0 -> a1, signed).
static int arc_pts(float *xy, int n, float cx, float cy, float r, float a0,
                   float a1, int steps) {
  for (int k = 0; k <= steps; k++) {
    const float t = a0 + (a1 - a0) * (float)k / (float)steps;
    n = put_pt(xy, n, cx + r * fd_cosf(t), cy + r * fd_sinf(t));
  }
  return n;
}

// Build the bar outline for row i. Returns the point count.
static int bar_outline(float *xy, int i) {
  const float L = bar_left(i), C = bar_cap(i), T = bar_top(i);
  const float R = (float)FOH_BAR_R, cy = T + R;
  const float halfPi = 1.5707963f;
  int n = 0;
  n = put_pt(xy, n, L, T);
  n = put_pt(xy, n, C, T);
  n = arc_pts(xy, n, C, cy, R, -halfPi, halfPi, 12);          // outer cap
  n = put_pt(xy, n, C, cy + 0.771f * R);                      // :450 (262/235)
  n = arc_pts(xy, n, C, cy, 0.571f * R, halfPi, -halfPi, 10); // r20 CCW
  n = put_pt(xy, n, C, cy - 0.286f * R);                      // :452 (225/235)
  n = arc_pts(xy, n, C, cy, 0.286f * R, -halfPi, halfPi, 8);  // r10
  n = put_pt(xy, n, C, T + FOH_BAR_H);
  n = put_pt(xy, n, L - 1, T + FOH_BAR_H);
  n = put_pt(xy, n, L - 3, cy);                               // the left point
  return n;
}

static void stroke_closed(Raster *rz, const float *xy, int n, float w,
                          RastCol c, unsigned a) {
  for (int k = 0; k < n; k++) {
    const int j = (k + 1) % n;
    lineW8(rz, xy[2 * k], xy[2 * k + 1], xy[2 * j], xy[2 * j + 1], w, c, a);
  }
}

// --- prim 4 lives in foh_font.c (the 6x9 heavy face + its italic shear) ----

// hsl -> rgb for the per-row animated hue. Upstream states the menu chrome
// as hsl(menuCurColour, 60%, 41%) (menu.js:364-365) and its fill as the same
// at 0.75 alpha (:364); menuColours = [238,358,117,55] (:34).
static RastCol hsl_col(double h, double sPct, double lPct, uint16_t a256) {
  const double s = sPct / 100.0, l = lPct / 100.0;
  double hh = h;
  while (hh < 0.0) hh += 360.0;
  while (hh >= 360.0) hh -= 360.0;
  const double c = (1.0 - fabs(2.0 * l - 1.0)) * s;
  const double hp = hh / 60.0;
  double hm = hp; // fmod() is device-libc; hp is already reduced to [0,6)
  while (hm >= 2.0) hm -= 2.0;
  const double x = c * (1.0 - fabs(hm - 1.0));
  const double m = l - c / 2.0;
  double r = 0, g = 0, b = 0;
  if (hp < 1.0)      { r = c; g = x; }
  else if (hp < 2.0) { r = x; g = c; }
  else if (hp < 3.0) { g = c; b = x; }
  else if (hp < 4.0) { g = x; b = c; }
  else if (hp < 5.0) { r = x; b = c; }
  else               { r = c; b = x; }
  RastCol o;
  o.r = (uint8_t)((r + m) * 255.0 + 0.5);
  o.g = (uint8_t)((g + m) * 255.0 + 0.5);
  o.b = (uint8_t)((b + m) * 255.0 + 0.5);
  o.a256 = a256;
  return o;
}

// Text with a 1 px outline in `edge` — upstream strokeText+fillText
// (startscreen.js:130-131 the wordmark, :180-181 PRESS START).
static void text2_outlined(Raster *rz, int x, int y, int scale, int italic,
                           const char *s, RastCol fill, RastCol edge) {
  static const int ox[8] = {-1, 0, 1, -1, 1, -1, 0, 1};
  static const int oy[8] = {-1, -1, -1, 0, 0, 1, 1, 1};
  for (int k = 0; k < 8; k++) {
    foh_text2(rz, x + ox[k], y + oy[k], scale, italic, s, edge);
  }
  foh_text2(rz, x, y, scale, italic, s, fill);
}

// --- backdrop cache -------------------------------------------------------
// The full-screen gradient layers are pure constants, so recomputing them
// every frame is provably redundant work — and each is a 240x240 pass with a
// sqrt AND a divide per pixel, on a device whose FOH loop has a 16.67 ms
// budget and fails its device legs on a single skipped frame. Built on first
// use, then memcpy'd. Determinism is unaffected: the cache is a pure function
// of compile-time constants, so frame 1 and frame N render identically.
//
// The background pass is bracketed with rast_ink_enable(0) (raster.h's
// documented use), so the cached and uncached paths agree on the ink plane
// too — background art is not part of the silhouette mask either way.
typedef enum { BG_TITLE = 0, BG_MENU, BG_COUNT } FohBgId;
static uint16_t g_bg[BG_COUNT][RAST_W * RAST_H];
static int g_bg_ready[BG_COUNT];

// Returns 1 if the cached backdrop was restored (caller skips the build).
static int bg_begin(Raster *rz, FohBgId id) {
  // The memcpy path bypasses rast_blend_px and therefore the clip band, so a
  // cached frame would ignore a band that the BUILD frame honoured. foh_render
  // always clears full-screen today; if that ever changes, fail loudly instead
  // of silently diverging build-frame from cached-frame.
  if (rz->clipY0 != 0 || rz->clipY1 != RAST_H) {
    gfx_fatal("foh_render: backdrop cache requires the full clip band");
  }
  rast_ink_enable(0);
  if (!g_bg_ready[id]) return 0;
  memcpy(rz->fb, g_bg[id], sizeof g_bg[id]);
  rast_ink_enable(1);
  return 1;
}

static void bg_end(Raster *rz, FohBgId id) {
  memcpy(g_bg[id], rz->fb, sizeof g_bg[id]);
  g_bg_ready[id] = 1;
  rast_ink_enable(1);
}

// ===========================================================================

static void text_center(Raster *rz, int y, int scale, const char *s,
                        RastCol c) {
  const int w = foh_text_width(s, scale);
  foh_text(rz, (RAST_W - w) / 2, y, scale, s, c);
}

static void header(Raster *rz, const char *title) {
  fill_rect(rz, 0, 0, RAST_W, 24, kPanel);
  text_center(rz, 6, 2, title, kAccent);
}

static void render_startup(const FohState *s, Raster *rz) {
  text_center(rz, 90, 3, "MELEELIGHT", kText);
  text_center(rz, 130, 1, "FUNKEY-S PORT", kDim);
  // deterministic progress bar over the 370-frame startup window
  const int w = (RAST_W - 40) * s->startupTimer / 370;
  fill_rect(rz, 20, 170, RAST_W - 40, 6, kPanel);
  fill_rect(rz, 20, 170, w, 6, kAccent);
}

// --- title screen (upstream menus/startscreen.js, re-authored) -------------
// Layer order is upstream's: BG1 radial + red glow (drawStartScreenInit,
// :18-45), BG2 ray pinwheel + centre bloom + green ring set + ray fan
// (drawStartScreen, :71-118), then the UI wordmark / PRESS START / the
// letterbox arcs (:119-190). Two upstream layers are NOT carried and are
// registered here rather than silently dropped: the 20 rising light-dust
// particles (:13-16, :152-163) are seeded with Math.random at module load —
// the FOH consumes no RNG by contract (foh.h) — and the six drifting
// stroked circles (:49-70) carry per-frame velocity state that would buy a
// handful of near-black pixels at this size.
static void render_title(const FohState *s, Raster *rz) {
  const int f = s->frame;

  // BG1: the radial backdrop (:20-27, 5 stops -> endpoints) plus the red glow
  // the shadowBlur pinwheel produces (:30-43). Both are frame-invariant.
  if (!bg_begin(rz, BG_TITLE)) {
    const RastCol v0 = {0x27, 0x00, 0x5b, 256}, v1 = {0x38, 0x00, 0x5b, 256};
    grad_radial(rz, 120.0f, 120.0f, 170.0f, v0, v1);
    const RastCol r0 = {147, 14, 42, 150}, r1 = {147, 14, 42, 0};
    grad_radial(rz, 62.0f, 118.0f, 132.0f, r0, r1);
    bg_end(rz, BG_TITLE);
  }

  // BG2: 30 grey wedges of PI/30 every PI/15, rotating angB += 0.001
  // (:72-84). One wedge = a triangle fan; 3 points is plenty at r 340.
  {
    const RastCol wedge = {0x33, 0x32, 0x36, 256}; // opaque: see px8_over
    const float ang0 = (float)(f % 6360) * 0.00098793f; // exact 2pi
    for (int k = 0; k < 30; k++) {
      const float a = ang0 + (float)k * 0.2094395f; // PI/15
      const float b = a + 0.1047198f;               // PI/30
      const float tri[6] = {120.0f, 120.0f,
                            120.0f + 340.0f * fd_cosf(a), 120.0f + 340.0f * fd_sinf(a),
                            120.0f + 340.0f * fd_cosf(b), 120.0f + 340.0f * fd_sinf(b)};
      poly8(rz, tri, 3, wedge, 256);
    }
  }
  // the centre bloom (:85-89)
  {
    const RastCol g0 = {51, 51, 51, 210}, g1 = {51, 51, 51, 0};
    grad_radial(rz, 120.0f, 120.0f, 96.0f, g0, g1);
  }
  // the green ring set (:90-101) and the rotating ray fan (:103-118)
  {
    const RastCol grn = {149, 255, 131, 31}; // 0.12 alpha
    float rad = 5.0f;
    for (int k = 0; k < 10; k++) {
      ring8(rz, 134.0f, 187.0f, rad, 1.0f, grn, 31);
      rad += 8.0f + (float)k * 2.0f;
    }
    const float angR = (float)(f % 6360) * 0.00098793f;
    for (int k = 0; k < 12; k++) {
      const float a = angR + (float)k * 0.2617994f; // PI/12
      const float ca = fd_cosf(a), sa = fd_sinf(a);
      line8(rz, 134.0f + 6.0f * ca, 186.0f + 6.0f * sa,
            134.0f + 300.0f * ca, 186.0f + 300.0f * sa, grn, 31);
    }
  }

  // UI: the two-weight wordmark. "Melee" is a dark fill under a light
  // stroke (:124-131); "LIGHT" is the glow weight and BOBS (:133-150 —
  // mlVel integrates +/-0.05 and flips past 0.8, a ~64-frame triangle;
  // reproduced here as an exact integer triangle so it needs no state).
  {
    const RastCol dark = {10, 8, 24, 256}, edge = {186, 186, 196, 256};
    const int w = foh_text2_width("MELEE", 5);
    text2_outlined(rz, (RAST_W - w) / 2, 42, 5, 0, "MELEE", dark, edge);
    const int bob = ((f / 2) % 24 < 12 ? (f / 2) % 24 : 24 - (f / 2) % 24) / 3
                    - 2;
    const RastCol glow = {172, 172, 190, 256};
    const int w2 = foh_text2_width("LIGHT", 3);
    foh_text2(rz, (RAST_W - w2) / 2, 94 + bob, 3, 0, "LIGHT", glow);
  }
  // the two grey discs behind the prompt (:164-174)
  disc8(rz, 120.0f, 187.0f, 6.0f, (RastCol){0x98, 0x98, 0x98, 256}, 256);
  disc8(rz, 120.0f, 187.0f, 3.0f, (RastCol){0x6c, 0x6b, 0x6b, 256}, 256);
  // PRESS START: #f0c900 over a black stroke (:175-181)
  {
    const RastCol gold = {0xf0, 0xc9, 0x00, 256}, blk = {0, 0, 0, 256};
    const int w = foh_text2_width("PRESS START", 1);
    text2_outlined(rz, (RAST_W - w) / 2, 183, 1, 0, "PRESS START", gold, blk);
  }
  // the letterbox arcs (:182-190): two huge circles of r 3000 centred far
  // off-canvas, so each edge is a shallow downward/upward bulge.
  {
    const RastCol blk = {0, 0, 0, 154}; // rgba(0,0,0,0.6)
    for (int x = 0; x < RAST_W; x++) {
      const float dx = (float)x - 120.0f;
      const float h = sqrtf(960.0f * 960.0f - dx * dx);
      const int yTop = (int)(-928.0f + h);
      const int yBot = (int)(1168.0f - h);
      for (int y = 0; y < yTop && y < RAST_H; y++) px8_over(rz, x, y, blk, blk.a256);
      for (int y = yBot > 0 ? yBot : 0; y < RAST_H; y++) px8_over(rz, x, y, blk, blk.a256);
    }
  }
}

// The hue-tinted chrome behind the bars: the two rotated/scaled ellipses
// (menu.js:277-294), the travelling dot on them (:344-355) and the bracket
// frame (:366-417, re-authored as a stroked polyline with the same top
// notch + the three diagonal slashes).
static void menu_chrome(const FohState *s, Raster *rz, RastCol hueLine) {
  // the ellipses: arc r 400 under scale(0.4,1) rotate(0.7|0.8) about
  // (800,400) -> semi-axes (32,128) about (160,128) at 240x240.
  const RastCol ell = {3, 31, 219, 128}; // rgba(3,31,219,0.5)
  for (int e = 0; e < 2; e++) {
    const float rot = e == 0 ? 0.7f : 0.8f;
    const float cr = fd_cosf(rot), sr = fd_sinf(rot);
    float px = 0.0f, py = 0.0f;
    for (int k = 0; k <= 40; k++) {
      const float t = (float)k * 0.15708f; // 2pi/40
      const float ex = 32.0f * fd_cosf(t), ey = 128.0f * fd_sinf(t);
      const float x = 160.0f + ex * cr - ey * sr;
      const float y = 128.0f + ex * sr + ey * cr;
      if (k > 0) line8(rz, px, py, x, y, ell, 128);
      px = x; py = y;
    }
  }
  // the travelling dot (menuAngle += 0.015, menu.js:347)
  {
    const float t = (float)(s->frame % 424) * 0.0148186f; // exact 2pi
    const float cr = fd_cosf(0.7f), sr = fd_sinf(0.7f);
    const float ex = 32.0f * fd_cosf(t), ey = 128.0f * fd_sinf(t);
    disc8(rz, 160.0f + ex * cr - ey * sr, 128.0f + ex * sr + ey * cr, 3.0f,
          hueLine, 256);
  }
  // the bracket frame: closed band, notched up-right across the top.
  {
    static const float pts[] = {
        30.0f, 220.0f,  24.0f, 214.0f,  24.0f,  54.0f,  30.0f,  48.0f,
        112.0f, 48.0f,  124.0f, 30.0f,  208.0f, 30.0f,  214.0f, 36.0f,
        214.0f, 214.0f, 208.0f, 220.0f};
    const int n = (int)(sizeof pts / sizeof pts[0]) / 2;
    stroke_closed(rz, pts, n, 5.0f, hueLine, 256);
    // The three slashes riding the notch. Upstream FILLS these as quads
    // (menu.js:394-417), and a 2 px lineW8 collapses to a 1 px hairline on a
    // diagonal (its +/-0.5 normal offsets round to the same pixels), so they
    // go through poly8 like every other filled shape here.
    for (int k = 0; k < 3; k++) {
      const float x = 106.0f + (float)k * 7.0f;
      const float quad[8] = {x + 12.0f, 30.0f, x + 15.0f, 30.0f,
                             x + 3.0f,  48.0f, x,         48.0f};
      poly8(rz, quad, 4, hueLine, 256);
    }
  }
}

static void render_menu(const FohState *s, Raster *rz) {
  // screen -> upstream menuMode table index (menu.js:44-47)
  int mm;
  switch (s->screen) {
    case FOH_MENU_TOP: mm = 0; break;
    case FOH_MENU_OPTIONS: mm = 1; break;
    case FOH_MENU_BATTLE: mm = 2; break;
    case FOH_MENU_CONTROLS: mm = 3; break;
    default: gfx_fatal("foh_render: menu render on a non-menu screen");
  }
  const int count = mm == 3 ? 2 : 4;
  // PASS 2 below indexes the label tables by menuSelected directly (the old
  // code only ever indexed inside k < count), and kMenuText[3] has just two
  // initialisers. foh.c keeps the cursor in range; make that loud, not lucky.
  if (s->menuSelected < 0 || s->menuSelected >= count) {
    gfx_fatal("foh_render: menu cursor out of range for this menu mode");
  }

  // BG1: the diagonal backdrop ramp (menu.js:259-263) + the grid/shine.
  if (!bg_begin(rz, BG_MENU)) {
    const RastCol b0 = {12, 11, 54, 256}, b1 = {1, 2, 15, 256};
    grad_linear(rz, 0.0f, 0.0f, 240.0f, 240.0f, b0, b1);
    bg_end(rz, BG_MENU);
  }
  grid_shine(rz, s->frame);

  const RastCol hueLine = hsl_col(s->menuHue, 60.0, 41.0, 256);
  menu_chrome(s, rz, hueLine);

  // the mode title: "italic 900 48px" white at 0.5 alpha (menu.js:438-440)
  {
    const RastCol t = {132, 132, 140, 256};
    foh_text2(rz, 18, 22, 2, 1, kMenuTitle[mm], t);
  }

  // the bars. Pass 1 = every row's black body + gold outline (menu.js:
  // 442-460); pass 2 = the selected row in yellow with black text and the
  // ring pulse (:461-527). Upstream draws the unselected yellow copies at
  // x+1000, i.e. off-canvas — so only the selected one is ever visible.
  {
    const RastCol body = {4, 4, 12, 256};         // rgba(0,0,0,0.76) flattened
    const RastCol gold = {243, 204, 0, 256};      // rgba(255,214,0,0.95)
    const RastCol sel = {254, 238, 27, 256};      // rgb(254,238,27)
    const RastCol selTx = {0, 0, 0, 256};
    const RastCol ring = {255, 247, 144, 256};
    float xy[FOH_BAR_MAXPTS * 2];
    // PASS 1 (menu.js:442-460): every row's black body + gold outline, plus
    // the unselected labels. Upstream draws all bodies before the selected
    // overlay, so a later row can never occlude the selected row's pulse.
    for (int k = 0; k < count; k++) {
      const int n = bar_outline(xy, k);
      poly8(rz, xy, n, body, 256);
      stroke_closed(rz, xy, n, 1.0f, gold, 256);
      if (k == s->menuSelected) continue;
      const char *label = kMenuText[mm][k];
      const int tw = foh_text2_width(label, 1);
      foh_text2(rz, (int)(123.0f - (float)(FOH_BAR_STEP * k)) - tw / 2,
                (int)bar_top(k) + 6, 1, 1, label, sel);
    }
    // PASS 2 (menu.js:461-527): the selected row in yellow with black text,
    // then its ring pulse. Upstream's unselected yellow copies are drawn at
    // x+1000 — off-canvas — so only the selected one is ever visible.
    {
      const int k = s->menuSelected;
      const int n = bar_outline(xy, k);
      poly8(rz, xy, n, sel, 256);
      stroke_closed(rz, xy, n, 1.0f, gold, 256);
      const char *label = kMenuText[mm][k];
      const int tw = foh_text2_width(label, 1);
      foh_text2(rz, (int)(123.0f - (float)(FOH_BAR_STEP * k)) - tw / 2,
                (int)bar_top(k) + 6, 1, 1, label, selTx);
      // the pulse: two static rings, a fading disc, and one or two
      // collapsing rings driven by menuTimer/menuCycle (menu.js:486-520).
      const float cx = bar_cap(k), cy = bar_top(k) + (float)FOH_BAR_R;
      const float t = (float)s->menuTimer;
      ring8(rz, cx, cy, 11.0f, 3.0f, ring, 179); // globalAlpha 0.7
      ring8(rz, cx, cy, 4.0f, 5.0f, ring, 179);
      float fa = 1.0f - t * 0.033f;
      if (fa < 0.0f) fa = -fa;
      unsigned da = (unsigned)(fa * 256.0f);
      if (da > 256) da = 256;
      disc8(rz, cx, cy, 8.0f, ring, da);
      float rr = (100.0f - t * 2.0f) * 0.32f;
      if (rr < 4.1f) rr = 4.1f;
      ring8(rz, cx, cy, rr, 1.0f, ring, 128);   // globalAlpha 0.5
      if (s->menuCycle == 1 && s->menuTimer > 10) {
        float rr2 = (130.0f - t * 2.0f) * 0.32f;
        if (rr2 < 4.1f) rr2 = 4.1f;
        ring8(rz, cx, cy, rr2, 1.0f, ring, 128);
      }
    }
  }

  // the explanation bar (menu.js:418-437): black at 0.7 behind a white
  // 1 px stroke, with the entry's blurb in the small face.
  {
    const RastCol back = {5, 5, 13, 256}, edge = {255, 255, 255, 256};
    const RastCol txt = {214, 214, 214, 256};
    // Wider than upstream's 580/1200 because our face is proportionally
    // bigger: the longest blurb ("CUSTOMIZE & CALIBRATE CONTROLLER.", 33
    // glyphs) is 230 px at face-2 scale 1, so the bar runs 4..236.
    fill_rect(rz, 4, 196, 232, 20, back);
    static const float box[] = {4.0f, 196.0f, 236.0f, 196.0f,
                                236.0f, 216.0f, 4.0f, 216.0f};
    stroke_closed(rz, box, 4, 1.0f, edge, 256);
    const char *ex = kMenuExpl[mm][s->menuSelected];
    const int w = foh_text2_width(ex, 1);
    foh_text2(rz, (RAST_W - w) / 2, 201, 1, 0, ex, txt);
  }
}

// --- the LOOK plane tick (foh.h) -------------------------------------------
// menu.js splits this across menuMove (:233-254, the reset + the offset) and
// drawMainMenu (:310-312 the global timer, :357-362 the hue lerp, :498-502
// the pulse phase). The FOH keeps rendering pure, so all of it advances
// here, off the tick count.
void foh_anim_tick(FohState *s) {
  // Bounded so the counter can never reach signed overflow and stays exactly
  // representable as a float. 25440 = 1060 (grid sweep bounce) * 24
  //                                 = 48 (wordmark bob) * 530,
  // so both wrap seamlessly. The rotating layers reduce the counter modulo
  // their own 2pi period at the call site (6360 and 424), and 25440 is an
  // exact multiple of BOTH, so they close at the counter wrap too — no pop.
  // Their rates sit within 1.2% of upstream's authored 0.001 / 0.015.
  s->frame = (s->frame + 1) % 25440;
  const int sc = s->screen;
  const int isMenu = sc == FOH_MENU_TOP || sc == FOH_MENU_OPTIONS ||
                     sc == FOH_MENU_BATTLE || sc == FOH_MENU_CONTROLS;
  if (!isMenu) {
    s->menuPrevSel = s->menuSelected;
    s->menuPrevScreen = sc;
    return;
  }
  if (s->menuSelected != s->menuPrevSel || sc != s->menuPrevScreen) {
    // menu.js:233-254 verbatim, including the TARGETTEST/TARGETBUILDER
    // special-case that WRITES menuColours (:243-252 — an upstream quirk:
    // the 0-hue arms make those two rows cross through red).
    const int prev = s->menuPrevSel, cur = s->menuSelected;
    s->menuCycle = 0;
    s->menuTimer = 0;
    if ((prev == 1 && cur == 2) || (prev == 2 && cur == 1)) {
      if (cur == 1) s->menuColours[cur] = 0.0;
      else s->menuHue = 0.0;
    } else if (prev == 1) {
      s->menuHue = 358.0;
      s->menuColours[1] = 358.0;
    }
    s->menuHueOff = s->menuColours[cur] - s->menuHue;
    s->menuPrevSel = cur;
    s->menuPrevScreen = sc;
  }
  if (s->menuHue != s->menuColours[s->menuSelected]) {
    s->menuHue += s->menuHueOff * 0.05; // menu.js:358
    if (s->menuTimer == 19) {
      // menu.js:360 lands the lerp exactly on step 20.
      s->menuHue = (double)(long)(s->menuHue + (s->menuHue < 0 ? -0.5 : 0.5));
    }
  }
  s->menuTimer++;             // menu.js:498
  if (s->menuTimer > 60) {    // menu.js:499-502
    s->menuTimer = 0;
    s->menuCycle = 1 - s->menuCycle;
  }
}

static void row_label(Raster *rz, int y, int row, int curRow,
                      const char *label) {
  if (row == curRow) {
    fill_rect(rz, 8, y - 4, RAST_W - 16, 18, kPanel);
    foh_text(rz, 12, y, 1, ">", kCursor);
  }
  foh_text(rz, 24, y, 1, label, row == curRow ? kText : kDim);
}

static void render_css(const FohState *s, Raster *rz) {
  header(rz, "CHARACTER SELECT");
  const int ys[4] = {50, 90, 130, 160};
  row_label(rz, ys[0], 0, s->cssRow, "P1");
  foh_text(rz, 60, ys[0], 1, kCharNames[s->p1Char], kAccent);
  row_label(rz, ys[1], 1, s->cssRow, "P2");
  foh_text(rz, 60, ys[1], 1, kCharNames[s->p2Char], kAccent);
  row_label(rz, ys[2], 2, s->cssRow, "P2 TYPE");
  foh_text(rz, 100, ys[2], 1, s->p2Type == 0 ? "HMN" : "CPU", kAccent);
  row_label(rz, ys[3], 3, s->cssRow, "CPU LEVEL");
  // slider look: 4 ticks (the upstream slider's 1..4 domain), current
  // level filled; dimmed entirely while P2 is human.
  for (int k = 0; k < 4; k++) {
    const RastCol c = (s->p2Type == 1 && k < s->difficulty) ? kAccent : kPanel;
    fill_rect(rz, 100 + k * 18, ys[3], 12, 8, c);
  }
  {
    const char lvl[2] = {(char)('0' + s->difficulty), 0};
    foh_text(rz, 180, ys[3], 1, lvl, s->p2Type == 1 ? kText : kDim);
  }
  text_center(rz, 200, 1, "START: STAGE SELECT", kDim);
  text_center(rz, 215, 1, "HOLD B: BACK", kDim);
}

static void render_sss(const FohState *s, Raster *rz) {
  header(rz, "STAGE SELECT");
  // 3x2 grid of stage tiles, ids 0..5
  for (int k = 0; k < 6; k++) {
    const int col = k % 3, row = k / 3;
    const int x = 10 + col * 75, y = 50 + row * 60;
    fill_rect(rz, x, y, 65, 44, kPanel);
    if (k == s->sssCursor) {
      // cursor frame (4 edges)
      fill_rect(rz, x - 2, y - 2, 69, 2, kCursor);
      fill_rect(rz, x - 2, y + 44, 69, 2, kCursor);
      fill_rect(rz, x - 2, y, 2, 44, kCursor);
      fill_rect(rz, x + 65, y, 2, 44, kCursor);
    }
    const char num[2] = {(char)('0' + k), 0};
    foh_text(rz, x + 4, y + 4, 1, num, kDim);
  }
  // The RANDOM slot (cursor 6): visible but REFUSING (registered
  // exclusion — upstream's arm draws from the seeded stream,
  // stageselect.js:80-84; foh.h header note). Rendered as a wide
  // dimmed tile below the grid.
  {
    const int x = 60, y = 172, w = 120, h = 16;
    fill_rect(rz, x, y, w, h, kPanel);
    if (s->sssCursor == 6) {
      fill_rect(rz, x - 2, y - 2, w + 4, 2, kCursor);
      fill_rect(rz, x - 2, y + h, w + 4, 2, kCursor);
      fill_rect(rz, x - 2, y, 2, h, kCursor);
      fill_rect(rz, x + w, y, 2, h, kCursor);
    }
    foh_text(rz, x + 6, y + 4, 1, "RANDOM", kDim);
  }
  text_center(rz, 194, 1, kStageNames[s->sssCursor], kAccent);
  text_center(rz, 208, 1, "A: FIGHT   B: BACK", kDim);
}

static void render_opt_gameplay(const FohState *s, Raster *rz) {
  header(rz, "GAMEPLAY");
  const int ys[3] = {60, 95, 130};
  row_label(rz, ys[0], 0, s->optRow, "TURBO");
  foh_text(rz, 120, ys[0], 1, s->turbo ? "ON" : "OFF", kAccent);
  row_label(rz, ys[1], 1, s->optRow, "L-CANCEL");
  foh_text(rz, 120, ys[1], 1, kLCancelNames[s->lCancelType], kAccent);
  row_label(rz, ys[2], 2, s->optRow, "TAP JUMP OFF");
  for (int k = 0; k < 4; k++) {
    const int x = 40 + k * 42;
    const int y = ys[2] + 18;
    if (s->optRow == 2 && s->optCol == k) {
      fill_rect(rz, x - 3, y - 3, 38, 16, kPanel);
    }
    const char pn[3] = {'P', (char)('1' + k), 0};
    foh_text(rz, x, y, 1, pn, kDim);
    foh_text(rz, x + 14, y, 1, s->tapJumpOff[k] ? "X" : "-",
             s->tapJumpOff[k] ? kAccent : kDim);
  }
  text_center(rz, 205, 1, "A: CHANGE   B: BACK", kDim);
}

static void render_match(Raster *rz) {
  // The FOH machine is terminal here; the driver owns the sim/renderer.
  text_center(rz, 110, 2, "LAUNCHING", kText);
}

// target-select (upstream drawTSS/drawTSSInit, stages/targetselect.js:
// 231-420, rewritten at 240x240 — foh.h rewrite deltas). Slots are the
// upstream 2-col x 5-row authored layout (col = floor(j/5), row = j%5)
// plus the refusing "+ ADD CODE" slot; the records line is the honest
// fresh-boot value (targetRecords ≡ -1 -> "--:--:--", targetplay.js:40 +
// targetselect.js:411-412; READ/persistence = task 13, medal/dev times
// deferred — foh.h note).
static void render_tss(const FohState *s, Raster *rz) {
  header(rz, "TARGET TEST");
  // char row (shoulder-driven; targetselect.js:60-74)
  foh_text(rz, 12, 32, 1, "L/R:", kDim);
  foh_text(rz, 44, 32, 1, kCharNames[s->p1Char], kAccent);
  // 2x5 grid of authored target stages (ids 0..9 == tstage ids)
  for (int k = 0; k < 10; k++) {
    const int col = k / 5, row = k % 5; // upstream floor(j/5) / j%5
    const int x = 16 + col * 108, y = 48 + row * 24;
    fill_rect(rz, x, y, 96, 18, kPanel);
    if (k == s->tssCursor) {
      fill_rect(rz, x - 2, y - 2, 100, 2, kCursor);
      fill_rect(rz, x - 2, y + 18, 100, 2, kCursor);
      fill_rect(rz, x - 2, y, 2, 18, kCursor);
      fill_rect(rz, x + 96, y, 2, 18, kCursor);
    }
    // "Target "+(i+1) (targetselect.js:93 label class)
    char label[10] = "TARGET ";
    if (k == 9) { label[7] = '1'; label[8] = '0'; label[9] = 0; }
    else { label[7] = (char)('1' + k); label[8] = 0; }
    foh_text(rz, x + 6, y + 5, 1, label,
             k == s->tssCursor ? kText : kDim);
  }
  // the refusing "+ Add Code" slot (builder plane; foh.h note)
  {
    const int x = 60, y = 172, w = 120, h = 14;
    fill_rect(rz, x, y, w, h, kPanel);
    if (s->tssCursor == 10) {
      fill_rect(rz, x - 2, y - 2, w + 4, 2, kCursor);
      fill_rect(rz, x - 2, y + h, w + 4, 2, kCursor);
      fill_rect(rz, x - 2, y, 2, h, kCursor);
      fill_rect(rz, x + w, y, 2, h, kCursor);
    }
    foh_text(rz, x + 6, y + 4, 1, "+ ADD CODE", kDim);
  }
  // records line (task 13 — the READ path through the persist plane):
  // upstream format targetselect.js:411-419 — -1 -> "--:--:--", else
  // "0"+floor(rec/60)+":"+((rec%60).toFixed(2), 5-char left-padded).
  // C form: integer centiseconds cs = (long)(rec*100 + 0.5) — no libc
  // float formatting on the device path (the iter-38/74 musl rounding
  // class; registered formatting delta, AGENT-LOG iter 100). The
  // addcode slot (cursor 10) keeps the dashes (foh.h note).
  {
    char line[40] = "PERSONAL BEST --:--:--";
    if (s->tssCursor <= 9) {
      const double rec = s->targetRecords[s->p1Char][s->tssCursor];
      if (rec != -1.0) {
        const long cs = (long)(rec * 100.0 + 0.5);
        snprintf(line, sizeof line, "PERSONAL BEST 0%ld:%02ld.%02ld",
                 cs / 6000, (cs % 6000) / 100, cs % 100);
      }
    }
    text_center(rz, 194, 1, line, kAccent);
  }
  text_center(rz, 208, 1, "A: GO   B: BACK", kDim);
}

static void render_tmatch(Raster *rz) {
  // Terminal like `match`; the driver owns the target sim/renderer.
  text_center(rz, 110, 2, "LAUNCHING", kText);
}

void foh_render(const FohState *s, Raster *rz) {
  rast_clear(rz, kBg.r, kBg.g, kBg.b, 0, RAST_H);
  switch (s->screen) {
    case FOH_STARTUP: render_startup(s, rz); break;
    case FOH_TITLE: render_title(s, rz); break;
    case FOH_MENU_TOP:
    case FOH_MENU_OPTIONS:
    case FOH_MENU_BATTLE:
    case FOH_MENU_CONTROLS: render_menu(s, rz); break;
    case FOH_CSS: render_css(s, rz); break;
    case FOH_SSS: render_sss(s, rz); break;
    case FOH_OPT_GAMEPLAY: render_opt_gameplay(s, rz); break;
    case FOH_MATCH: render_match(rz); break;
    case FOH_TSS: render_tss(s, rz); break;
    case FOH_TMATCH: render_tmatch(rz); break;
    default: gfx_fatal("foh_render: invalid screen");
  }
}
