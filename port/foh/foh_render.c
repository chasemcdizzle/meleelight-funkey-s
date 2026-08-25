// port/foh/foh_render.c — FOH screen rendering (fix_plan §M4 task 9).
// REWRITTEN look at 240x240 over the raster prims (never a DOM port);
// menus are NOT checksummed — visual authority is Chase's acceptance
// playthrough (task 10 owns the device look pass). Deterministic by
// construction: pure function of FohState, no RNG, no clock — the
// check's shot byte-stability x2 depends on it.
#include "foh.h"

#include "../gfx/ctl_style.h" // C30(c): the Controls screen's two cells
#include "foh_ctl_labels.h" // the Controls screen's action-label table

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
#include <stdlib.h> // getenv (the menu.img1 path resolution below)
#include <string.h> // memcpy (the backdrop cache)

#include "../gfx/img1.h" // A9 menu artwork (portraits / stage previews / hands)

// Labels: faithful strings from the upstream tables (cited), uppercased
// for the 5x7 font. These are UI text of a rewritten non-checksummed
// surface, not engine values (HARD RULE 5 concerns data planes).
static const char *kMenuTitle[4] = {
    // menuTitle (menu.js:32)
    "MAIN MENU", "OPTIONS", "BATTLE MODE", "CONTROLS"};
static const char *kMenuText[4][4] = {
    // menuText (menu.js:19-24)
    {"VS. MELEE", "TARGET TEST", "TARGET BUILDER", "OPTIONS"},
    // DEVIATION D25 (MENU-SPEC §12.1, owner-requested 2026-08-23): upstream's
    // row 2 is "Keyboard Controls" (menu.js:21), but it opens a CHOOSER — the
    // row was named for one of its own two destinations. It reads "CONTROLS".
    {"AUDIO", "GAMEPLAY", "CONTROLS", "CREDITS"},
    {"LOCAL VS", "SPECTATE", "P2P", "SERVER"},
    // DEVIATION D25, second half: upstream is ["Controller","Keyboard"]
    // (menu.js:22-23). What that "Keyboard" row opens on this device is the
    // FunKey-S's OWN buttons — there is no keyboard — so it is HANDHELD, a
    // category name parallel to CONTROLLER; and it comes FIRST, because no
    // controller path exists on this device today.
    // THE ORDER IS ROUTING, NOT PAINT: foh.c's step_menu maps row 0 to
    // FOH_CTRL_KEY and row 1 to FOH_CTRL_PAD to match. The upstream gameMode
    // identities (12 / 14) and the screen tokens are untouched.
    {"HANDHELD", "CONTROLLER"},
};
static const char *kCharNames[5] = {
    // characters.js:2-8 order == the oracle char ids
    "MARTH", "JIGGLYPUFF", "FOX", "FALCO", "CAPTAIN FALCON"};
// The CSS cells and port name-plates are 44 / 58 px wide, so they carry
// upstream's OWN abbreviated CSS labels (css.js draws "JIGGLY-PUFF" /
// "C.FALCON" in the cell strips) shortened again to fit the 5x7 face.
static const char *kCharShort[5] = {"MARTH", "PUFF", "FOX", "FALCO",
                                    "FALCON"};
// IMG1 directory names (pipeline `assets` stage), indexed by the SAME ids.
static const char *kCharArt[5] = {"marth", "puff", "fox", "falco", "falcon"};
static const char *kStageArt[6] = {"stage_bf", "stage_ys", "stage_ps",
                                   "stage_dl", "stage_fd", "stage_fod"};
// The SSS thumb strips are 65 px wide == 11 glyphs; these are upstream's own
// thumbnail labels (stageselect.js name row), not new prose.
static const char *kStageShort[6] = {"BATTLEFIELD", "Y-STORY", "P-STADIUM",
                                     "DREAMLAND", "F-DEST", "FOUNTAIN"};
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
    // D25 again: the two blurbs follow their rows. The HANDHELD blurb is 32
    // glyphs, one under the 33-glyph pin below (the bar width comment at the
    // explanation bar) — which "CUSTOMIZE & CALIBRATE CONTROLLER." still sets,
    // so the panel geometry does not move.
    {"CUSTOMIZE THE HANDHELD CONTROLS.", "CUSTOMIZE & CALIBRATE CONTROLLER.",
     "", ""},
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
  // OPAQUE FAST PATH — algebraically the same store, not an approximation:
  // at a == 256 the blend below is (c.X * 256 + dX * 0) >> 8 == c.X for every
  // channel, so the destination unpack cannot influence the result. Skipping
  // it removes a framebuffer READ and six shift/or ops from every pixel of
  // every opaque fill (the wedge fan, the bars, the strokes, fill_rect) —
  // which is most of what the FOH draws. Byte-identical by construction.
  if (a == 256) {
    RastCol o = c;
    o.a256 = 256;
    rast_blend_px(rz, x, y, o, 256);
    return;
  }
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
// PER-FRAME MEMO (B3, iter 123). The title's centre bloom redraws this same
// constant-argument ramp EVERY frame, and each pixel costs a blocking VSQRT
// plus a VDIV — on a Cortex-A7 those are ~14 cycles apiece and do not
// pipeline, so 57600 of each is the single most expensive thing the FOH does:
// MEASURED at 0.236 ms of the title frame's 0.469 ms on the host bench, i.e.
// half the screen's cost, on the screen that was overrunning the device's
// 16.67 ms budget (66 FOH render skips, iter 123). The ramp is a pure
// function of the arguments, so it is computed once and replayed; the LUT
// stores lerp_col's own output, so the composited bytes are unchanged.
// ponytail: ONE slot, because exactly one radial redraws per frame today. A
// second per-frame radial would just thrash it back to the slow path —
// correctness is unaffected — at which point give it a slot id the way the
// backdrop cache uses FohBgId.
typedef struct {
  float cx, cy, rad;
  RastCol c0, c1;
} GradKey;
static GradKey g_grKey;
static int g_grReady;
static RastCol g_grLut[RAST_W * RAST_H];
static int g_grLo[RAST_H], g_grHi[RAST_H];

static int gr_col_eq(RastCol a, RastCol b) {
  return a.r == b.r && a.g == b.g && a.b == b.b && a.a256 == b.a256;
}

static void grad_radial(Raster *rz, float cx, float cy, float rad,
                        RastCol c0, RastCol c1) {
  if (rad <= 0.0f) gfx_fatal("foh_render: degenerate radial gradient");
  if (!g_grReady || g_grKey.cx != cx || g_grKey.cy != cy ||
      g_grKey.rad != rad || !gr_col_eq(g_grKey.c0, c0) ||
      !gr_col_eq(g_grKey.c1, c1)) {
    for (int y = 0; y < RAST_H; y++) {
      int lo = RAST_W, hi = 0;
      for (int x = 0; x < RAST_W; x++) {
        const float ddx = (float)x - cx, ddy = (float)y - cy;
        float t = sqrtf(ddx * ddx + ddy * ddy) / rad;
        if (t > 1.0f) t = 1.0f;
        const RastCol c = lerp_col(c0, c1, t);
        g_grLut[(size_t)y * RAST_W + (size_t)x] = c;
        // the row's nonzero-alpha extent: the replay below skips the rest,
        // which is exactly the `if (c.a256 == 0) continue` it replaces.
        if (c.a256 != 0) { if (x < lo) lo = x; if (x + 1 > hi) hi = x + 1; }
      }
      g_grLo[y] = lo; g_grHi[y] = hi;
    }
    g_grKey.cx = cx; g_grKey.cy = cy; g_grKey.rad = rad;
    g_grKey.c0 = c0; g_grKey.c1 = c1;
    g_grReady = 1;
  }
  // REPLAY (B9): one call per row into the -O3 raster TU instead of one
  // cross-TU rast_blend_px per pixel. rast_blend_run IS this loop — same
  // arithmetic, same skip/clamp conditions, same clip pair (raster.c) — so
  // the composited bytes are unchanged. MEASURED on device: 5.59 -> 1.09 ms
  // for the title's centre bloom, the single biggest FOH frame cost.
  for (int y = 0; y < RAST_H; y++) {
    rast_blend_run(rz, y, g_grLo[y], g_grHi[y],
                   &g_grLut[(size_t)y * RAST_W]);
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
  // FULL-ROW OPAQUE FAST PATH (review-p1 fallback round, [M]). A backdrop
  // ramp is 240 of these spans and nothing else, i.e. 57,600 cross-TU
  // rast_blend_px calls per frame from an -O2 TU — the same per-pixel cost
  // class as the title screen, which took five optimisations to reach
  // skips == 0 against the 16.67 ms deadline (AGENT-LOG iter 123). The
  // raster's own batch primitive already exists for exactly this
  // (raster.h:58-64, M4 task 3): the loop moves into the -O3 raster TU with
  // "arithmetic EXACTLY rast_blend_px's (bit-identical by construction)".
  // Guarded on a FULL row and a == 256 because those are the only
  // conditions under which rast_fill_row_opaque is the same operation;
  // every other span keeps the general path. Byte-identity of the CSS/SSS
  // and Phase-0 shots across this change is the standing proof.
  if (a == 256 && c.a256 == 256 && xa <= 0 && xb >= RAST_W && y >= 0 &&
      y < RAST_H) {
    rast_fill_row_opaque(rz, y, c);
    return;
  }
  // B9: the general path is now ONE call per span, not one per pixel.
  // rast_fill_run IS `for x in [xa,xb): px8_over(rz, x, y, c, a)` with the
  // clip test, row base and ink flag hoisted (raster.c) — bit-identical, and
  // it subsumes the fast path above for every PARTIAL span too, which is what
  // the 30-wedge title fan and every rounded rect actually draw. MEASURED on
  // device: the wedge fan 3.59 -> 1.31 ms.
  rast_fill_run(rz, y, xa, xb, c, a);
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
      // B9 (review-b9-4-codex [M]): poly8 keeps its OWN span emit rather than
      // calling span8, so the span8 rewire did not reach it — which is why
      // the title's 30-wedge fan stayed at 3.56 ms while everything else
      // fell. This run is a CONSTANT colour and alpha over [xa,xb), i.e.
      // exactly rast_fill_run's contract; the clamps above are the same
      // clamps rast_fill_run applies, so passing the already-clamped range
      // through is bit-identical (and idempotent).
      rast_fill_run(rz, y, xa, xb, c, a);
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

// `line`/`base`/`sweep` are parameters because upstream uses this layer twice
// in two palettes and only ONE of them moves: white at 0.13 with the travelling
// highlight behind the main menu (menu.js:296-305), and a STATIC magenta
// lattice behind the SSS (stageselect.js backdrop — measured on the reference
// captures: no moving highlight). sweep == 0 skips the highlight arm whole,
// which also removes its per-pixel divide from the SSS frame (review-p1 round
// 1, [M]) — VDIV is a ~14-cycle blocking op on the Cortex-A7. The menu path
// (sweep == 1) is arithmetically untouched.
static void grid_shine(Raster *rz, int frame, RastCol line, unsigned base,
                       int sweepOn) {
  // The sweep runs over the x+y diagonal (0..478) and BOUNCES back, so the
  // cycle closes without the teleport a saw wave would give at its wrap. One
  // leg is ~530 frames, the same order as upstream's 600-frame
  // menuGlobalTimer wrap (menu.js:311).
  // Computed ONLY when it is used: a reader auditing "which screens read
  // the look plane" (the canonical-shot invariant) sees `frame` flow into
  // the SSS backdrop call and should not have to read this body to learn it
  // is inert there (review-p1 fallback round, [L]).
  float sweep = 0.0f;
  if (sweepOn) {
    const int ph = frame % 1060;
    sweep = (float)(ph < 530 ? ph : 1060 - ph) * 0.905f;
  }
  // The pixel SET and its order are exactly the old `if (x % P && y % P)
  // continue` sweep over all 57600 pixels — a grid ROW is every x, any other
  // row is every P-th x — but only the ~8160 pixels that survive are visited.
  // Each is still composited exactly once (the span convention above).
  for (int y = 0; y < RAST_H; y++) {
    const int step = (y % FOH_GRID_PITCH == 0) ? 1 : FOH_GRID_PITCH;
    for (int x = 0; x < RAST_W; x += step) {
      unsigned a = base; // menu: 33 == 0.13 * 256
      if (sweepOn) {
        const float d = fabsf((float)(x + y) - sweep);
        if (d < 46.0f) a += (unsigned)(54.0f * (1.0f - d / 46.0f));
      }
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

// --- prim 5: the A9 menu artwork (IMG1) ------------------------------------
// The pipeline `assets` stage packs upstream's OWN menu art — 5 character
// portraits (58 px wide), 6 VS-stage previews (65x24) and 3 hand cursors
// (24x32) — into one deterministic file (port/gfx/img1.h, FORMATS.md §7).
// PROVENANCE: Nintendo-derived, PRIVATE USE ONLY, gitignored build output;
// never committed, never distributed.
//
// Loaded ONCE, at the top of foh_render, so the read can never land inside a
// paced frame: the first FOH frame is the startup screen (no frame budget)
// and foh_render_warm hits it earlier still on both app paths.
//
// Determinism is unaffected. The set is a pure function of the pipeline's
// byte-stable output (its sha256 is pinned by pipeline/check-assets.sh), so
// it behaves exactly like the compile-time constants the backdrop cache
// already relies on: frame 1 and frame N — and host and device — render the
// same bytes from the same machine state.
//
// A MISSING file is FATAL, not a fallback (HARD RULE 2). A target that
// quietly rendered the portrait-less CSS would diverge from its twin on
// every shot — precisely the judge-defeating class B3 closed.
static Img1Set g_art;
static int g_art_ready;

// gfx_fatal takes one string, so the loud paths build their message here.
// Static because gfx_fatal is noreturn — nothing outlives the call.
static char g_art_msg[640];

// `reason` is img1_error() ONLY where an img1_open actually ran: on the
// truncation arm no open has happened, so img1_error() would report either
// the empty initial string or, worse, a STALE reason from an earlier failed
// open (review-p1 fallback round, [L]).
static void art_die(const char *why, const char *path, const char *reason) {
  snprintf(g_art_msg, sizeof g_art_msg,
           "foh_render: %s (%s): %s — menu.img1 is produced by `node "
           "pipeline/run.js --only assets` and must be provisioned next to "
           "the run's data dir",
           why, path, reason);
  gfx_fatal(g_art_msg);
}

// EXPLICIT OVERRIDES ARE AUTHORITATIVE (review-p1 round 1, [H]).
// If MLFK_MENU_IMG1 — or, failing that, MLFK_DATA_DIR — names a file, that
// file is the ONLY candidate: falling through to a mount point or the cwd
// after a failed open could load DIFFERENT artwork than the operator asked
// for, and two targets rendering different artwork is precisely the twin
// divergence the fail-loud rule exists to prevent. This is the same
// discipline the OPK launchers already state for the data dir ("explicit
// data dir must qualify; fallback chain not consulted",
// port/gfx/opk/mlfk-foh.sh:48). The unguided chain below is only reached
// when NEITHER variable is set.
static void art_load(void) {
  if (g_art_ready) return;
  const char *env = getenv("MLFK_MENU_IMG1");
  if (env != NULL && *env != '\0') {
    if (img1_open(&g_art, env) != 0)
      art_die("MLFK_MENU_IMG1 unusable", env, img1_error());
    g_art_ready = 1;
    return;
  }
  const char *dd = getenv("MLFK_DATA_DIR");
  if (dd != NULL && *dd != '\0') {
    char buf[512];
    const int n = snprintf(buf, sizeof buf, "%s/assets/menu.img1", dd);
    if (n < 0 || n >= (int)sizeof buf) {
      // A truncated path names a DIFFERENT file; never open it.
      art_die("MLFK_DATA_DIR path too long", dd,
              "snprintf truncated the path (no open attempted)");
    }
    if (img1_open(&g_art, buf) != 0)
      art_die("MLFK_DATA_DIR artwork unusable", buf, img1_error());
    g_art_ready = 1;
    return;
  }
  // Unguided: the two device mount points the launchers search, then the
  // cwd. First one that OPENS wins; none opening is fatal.
  static const char *const cand[3] = {"/mnt/mlfk-scratch/assets/menu.img1",
                                      "/mnt/mlfk-data/assets/menu.img1",
                                      "assets/menu.img1"};
  for (int k = 0; k < 3; k++) {
    if (img1_open(&g_art, cand[k]) == 0) {
      g_art_ready = 1;
      return;
    }
  }
  art_die("menu.img1 not found (set MLFK_MENU_IMG1 or MLFK_DATA_DIR)", cand[2],
          img1_error());
}

static const Img1Image *art(const char *name) {
  const Img1Image *im = img1_find(&g_art, name);
  if (im == NULL) {
    // Name the missing image: a renamed or dropped asset is otherwise
    // unlocalisable from the message alone (review-p1 fallback round, [L]).
    snprintf(g_art_msg, sizeof g_art_msg,
             "foh_render: menu.img1 has no image named '%s' — the `assets` "
             "stage's directory and this renderer have drifted",
             name);
    gfx_fatal(g_art_msg);
  }
  return im;
}

// Integer-scale source-over blit.
//
// NOT img1_blit: that composites through rast_blend_px, i.e. through the
// blend565 blue-spill bug this whole file routes around (see px8_over). The
// hand cursors carry ANTIALIASED alpha, so the defect would sit on every
// cursor edge of every frame; the portraits' binary alpha would escape it,
// but there is no reason to keep two blit paths. a8 == 255 is promoted to
// 256 so opaque source pixels take px8_over's store-only fast path.
// Sub-rect variant: the CSS row cells are 44 px wide and the portraits are
// 58, so the cells carry a centred CROP of the same art (upstream's cells
// crop too — its 85x85 cell is squarer than the source portrait).
static void blit_img_rect(Raster *rz, const Img1Image *im, int x0, int y0,
                          int sx, int sy, int sw, int sh, int sc) {
  if (sc < 1) gfx_fatal("foh_render: image scale must be >= 1");
  if (sx < 0 || sy < 0 || sw < 0 || sh < 0 || sx + sw > im->w ||
      sy + sh > im->h) {
    gfx_fatal("foh_render: image sub-rect outside the source image");
  }
  for (int y = sy; y < sy + sh; y++) {
    for (int x = sx; x < sx + sw; x++) {
      const size_t idx = (size_t)y * (size_t)im->w + (size_t)x;
      const unsigned a = im->a8[idx];
      if (a == 0) continue;
      const uint16_t p = im->rgb565[idx];
      const unsigned r5 = (unsigned)((p >> 11) & 0x1F);
      const unsigned g6 = (unsigned)((p >> 5) & 0x3F);
      const unsigned b5 = (unsigned)(p & 0x1F);
      RastCol c;
      c.r = (uint8_t)((r5 << 3) | (r5 >> 2));
      c.g = (uint8_t)((g6 << 2) | (g6 >> 4));
      c.b = (uint8_t)((b5 << 3) | (b5 >> 2));
      c.a256 = 256;
      // ALPHA CONVERSION, identical to img1.c:171's `(a8 * 256) / 255` (and
      // therefore to rast_blit_rgba's) — proved, not approximated: for every
      // a in 0..254, (a * 256) / 255 == a + a / 255 == a in integer division,
      // and a == 255 is the 256 case spelled out here. Written this way to
      // keep a DIVIDE off the per-pixel blit path (review-p1 fallback round,
      // [L]: parity was the concern, and it already holds exactly).
      const unsigned a256 = a >= 255u ? 256u : a;
      for (int py = 0; py < sc; py++) {
        for (int px = 0; px < sc; px++) {
          px8_over(rz, x0 + (x - sx) * sc + px, y0 + (y - sy) * sc + py, c,
                   a256);
        }
      }
    }
  }
}

static void blit_img(Raster *rz, const Img1Image *im, int x0, int y0,
                     int sc) {
  blit_img_rect(rz, im, x0, y0, 0, 0, im->w, im->h, sc);
}

// --- prim 6: rounded rects -------------------------------------------------
// Upstream's CSS cells, port panels and SSS boxes are all roundRect fills
// with a vertical two-stop gradient (css.js / stageselect.js). ONE half-open
// span per row keeps the one-composite-per-pixel invariant the fills above
// establish; the only sqrt is per ROW of the corner arc, never per pixel.
#define FOH_RR_MAX 10

static void rrect_v(Raster *rz, int x, int y, int w, int h, int r,
                    RastCol c0, RastCol c1) {
  if (w <= 0 || h <= 0) return;
  if (r < 0) r = 0;
  if (r > FOH_RR_MAX) gfx_fatal("foh_render: rounded-rect radius overflow");
  if (r * 2 > w) r = w / 2;
  if (r * 2 > h) r = h / 2;
  int ins[FOH_RR_MAX];
  for (int k = 0; k < r; k++) {
    const float dy = (float)r - 0.5f - (float)k;
    float t = (float)r * (float)r - dy * dy;
    if (t < 0.0f) t = 0.0f;
    ins[k] = r - iround(sqrtf(t));
    if (ins[k] < 0) ins[k] = 0;
  }
  for (int k = 0; k < h; k++) {
    int in = 0;
    if (k < r) in = ins[k];
    else if (k >= h - r) in = ins[h - 1 - k];
    const float t = h > 1 ? (float)k / (float)(h - 1) : 0.0f;
    const RastCol c = lerp_col(c0, c1, t);
    span8(rz, x + in, x + w - in, y + k, c, c.a256);
  }
}

// Flat rounded rect (the gradient's degenerate case, spelled out at the
// call sites that want a border colour rather than a ramp).
static void rrect(Raster *rz, int x, int y, int w, int h, int r, RastCol c) {
  rrect_v(rz, x, y, w, h, r, c, c);
}

// Text centred inside [x, x+w) using the 5x7 face.
static void text_in(Raster *rz, int x, int w, int y, int scale, const char *s,
                    RastCol c) {
  foh_text(rz, x + (w - foh_text_width(s, scale)) / 2, y, scale, s, c);
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
  {
    const RastCol white = {255, 255, 255, 256};
    grid_shine(rz, s->frame, white, 33, 1); // menu.js:296-305 (0.13 alpha)
  }

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
  // targetSelectTimer, advanced ONLY while the tick ends on target-select
  // (targetselect.js:268 lives inside drawTSS). Same LOOK-plane rules as
  // everything below: no flow edge, event or launch record can read it.
  if (s->screen == FOH_TSS) s->tssTimer++;
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

// --- the CANONICAL SHOT PHASE (B3, iter 123) -------------------------------
// MEASURED DEFECT this exists to kill: the device shot judge
// (check-device-foh.sh / check-device-target.sh) cmp's a DEVICE shot against
// the HOST TWIN's shot of the same state, byte for byte. Its pre-registered
// contract (foh_dev.c header, iter 93) is "captured on the q EDGE with the
// machine state settled (byte-identical to the twin's shot of the SAME
// state)" — i.e. the FOH render is a pure function of the MACHINE state.
//
// The look plane above broke that contract, because it is a pure function of
// the TICK COUNT, and the two targets do not share one:
//   - the host twin (--input flow) applies inputs at the flow's LOGICAL frame
//     numbers, so f01 shoots menu-top at tick 378;
//   - the device (--input poll) receives the same inputs as real uinput
//     keysyms on a WALL-CLOCK fk schedule, so it shoots menu-top at tick 593
//     (measured, iter 123: dev-trace.txt vs twin-a/trace.txt).
// 215 ticks apart => grid_shine's sweep sits 78 px further along the x+y
// diagonal (measured peak: host 344, device 423 — exactly (1060-593)*0.905
// vs 378*0.905) and menuTimer is 10 vs 3, so the ring pulse differs too.
// That is 2177 differing pixels no rasteriser fix can remove: the device's
// tick number at a q-marker shot is a real-time artefact and is not even
// reproducible run to run (which is precisely why the device TRACE is judged
// with the frame fields elided).
//
// CLASS FIX (HARD RULE 8): a shot is rendered from a COPY of the state whose
// look plane sits at its CANONICAL PHASE — the ORIGIN of every counter and,
// for the hue lerp alone, its exact fixed point. (frame/menuTimer/menuCycle
// converge to nothing: the grid sweep and the wedge fan run forever. What
// carries the argument is not convergence, it is TARGET-INDEPENDENCE.) A
// shot is then a
// pure function of the machine state on every target BY CONSTRUCTION, which
// is what the judge always claimed to compare. Nothing is relaxed: the cmp
// stays byte-exact, the fb witness still proves the captured bytes are the
// PRESENTED kernel page (the shot tick presents this same canonical frame),
// and live play never calls this. Applies only to shots at/after the flow's
// first non-neutral input; the earlier ones are tick-INDEXED on both targets
// (foh_dev.c's split), so they keep proving the animated look renders
// identically on device — the coverage that falsified the libm suspicion.
// WHAT THIS STOPS COMPARING cross-target, stated plainly for the next reader
// (UPDATED by the A1 Phase 1 restyle — CSS and SSS now read the look plane
// too, so the elided set grew; review-p1 round 1, [L]):
//   - the four MENU shots: the PHASE of grid_shine's sweep, the menuTimer
//     ring pulse, the menu_chrome travelling dot, and a mid-flight hue lerp;
//   - the CSS shots: the phase of the READY TO FIGHT ribbon's hsl pulse
//     (render_css, `s->frame % 60`);
//   - the SSS shots: the phase of the hovered thumb's 8-frame border flash
//     and the RANDOM box's (render_sss, `(s->frame / 8) % 2`);
//   - the TARGET-SELECT shots: the same 8-frame flash on the hovered slot
//     border (render_tss, `(s->tssTimer % 8) > 4` — upstream's own
//     targetselect.js:271), and the audio screen's lattice sweep
//     (render_opt_audio's grid_shine).
// Every one of those is a pure function of the LOOK PLANE — `frame` for
// most of them and the screen-local `tssTimer` for the target-select
// flash — and this function pins BOTH to 0 (with menuTimer/menuCycle and
// the hue's fixed point). That is the whole reason the animation went on
// the LOOK plane rather than onto any machine field: a shot stays a pure
// function of MACHINE state on every target, whatever counters the look
// acquires later. Any new phase counter MUST be added here too. The remaining screens
// (startup / match / tmatch / options-gameplay / the two controls
// destinations) read no look-plane field at all. The layers still draw, at phase 0, so their arithmetic is still
// judged; it is the phase VALUES that are not. Recording the device's own look plane and injecting it
// into the host twin would restore even those (the project's oracle-fed-seam
// idiom) — that is the preferred end state, and it is registered for the
// driver because it changes two GATE scripts and revisits the iter-93 design.
void foh_look_canonical(FohState *s) {
  // startupTimer is a per-tick counter too, and render_startup reads it — but
  // it is deliberately NOT reset here, because a startup shot must be
  // tick-indexed (identical tick on both targets) to be judgeable at all.
  // Unreachable today; loud rather than silently divergent if a flow ever
  // puts a non-neutral input before its startup shot.
  if (s->screen == FOH_STARTUP) {
    gfx_fatal("foh_look_canonical: a startup shot must be tick-indexed");
  }
  s->frame = 0;
  s->menuTimer = 0;
  s->menuCycle = 0;
  s->tssTimer = 0;
  // the hue lerp's fixed point (foh_anim_tick above lands menuHue exactly on
  // menuColours[menuSelected] at step 20); menuColours itself is navigation-
  // driven, not tick-driven, so it is left alone.
  if (s->menuSelected >= 0 && s->menuSelected < 4) {
    s->menuHue = s->menuColours[s->menuSelected];
  }
  s->menuHueOff = 0.0;
}

static void row_label(Raster *rz, int y, int row, int curRow,
                      const char *label) {
  if (row == curRow) {
    fill_rect(rz, 8, y - 4, RAST_W - 16, 18, kPanel);
    foh_text(rz, 12, y, 1, ">", kCursor);
  }
  foh_text(rz, 24, y, 1, label, row == curRow ? kText : kDim);
}

// --- CHARACTER SELECT (upstream menus/css.js, re-authored) -----------------
// A1 restyle Phase 1. Upstream's CSS is its densest screen: a silver header
// carrying the MELEE plate + VS badge + mode ribbon + BACK wedge, a row of
// five 85x85 rounded portrait cells at a 95 px pitch (rgb(41,47,68) ->
// rgb(85,95,128) gradient, black name strip), four 225 px-pitch port panels
// with HMN/CPU/NET/N-A type tabs, per-port tints, name plates, a CPU-level
// slider on the gradient track with its r17 knob, the port tokens, the hand
// cursor and the READY TO FIGHT ribbon.
//
// RE-AUTHORED, never scaled: 1200x750 -> 240x240 is 0.20 in x and 0.32 in y,
// so a uniform scale would put 31 px labels at 6 px. Every rhythm below is
// upstream's (five cells, four ports, tab-then-portrait-then-plate stack, the
// ribbon across the middle); every NUMBER is ours, chosen so the smallest
// legible unit — a 5x7 glyph — still fits. The one geometric coincidence
// worth naming: the A9 portraits are 58 px wide, which is exactly a quarter
// of the screen less the gaps, so a port panel is one portrait wide.
//
// SCOPE (foh.h): the machine gives ONE hand to port 0 (one input device) and
// — since A44 — FOUR toggleable ports. A port sitting at N/A renders as
// upstream's N-A panel, which is what upstream shows for an unjoined port;
// DEVIATION D6 used to say ports 3/4 could only ever be that, and it is
// retired. NET is not drawn because the network arm is scope-excluded
// (DEVIATION D5), so a NET tab would name an unreachable state; CPU is not
// drawn above port 1 for the same kind of reason (DEVIATION D40(b)) — the
// type simply never gets there.
//
// EVERY rectangle on this screen is a FOH_CSS_* constant in foh.h, shared
// verbatim with foh.c's hit tests: DEVIATION D4 requires the hit region and
// the drawn extent to be the same rectangle, and two files with two copies of
// the numbers is exactly how that stops being true.
#define CSS_CELL_W FOH_CSS_CELL_W
#define CSS_CELL_H FOH_CSS_CELL_H
#define CSS_CELL_Y FOH_CSS_CELL_Y
#define CSS_PANEL_W FOH_CSS_PANEL_W
#define CSS_PANEL_Y FOH_CSS_PANEL_Y
#define CSS_TAB_H FOH_CSS_TAB_H

static int css_cell_x(int k) { return foh_css_cell_x(k); }
static int css_panel_x(int k) { return foh_css_panel_x(k); }

// The hand sprite at a free cursor's position (css.js:1135-1143 picks it from
// handType). Upstream draws a 101x133 sprite at (x-40, y-30), i.e. the logical
// hot spot sits 39.6% across and 22.6% down the sprite, at the pointed
// fingertip — on this 24x32 asset that is (10, 7).
//
// Shared by the CSS and, since A25(c), by target-select, which passes type 0:
// there is no grab gesture on that screen, so the pointing hand is the only
// sprite it can be in (and `hand_point` is what the owner asked for by name).
static void draw_hand(Raster *rz, double x, double y, int type) {
  static const char *const kHand[3] = {"hand_point", "hand_open", "hand_grab"};
  const int px = (int)(x + 0.5) - 10, py = (int)(y + 0.5) - 7;
  blit_img(rz, art(kHand[type]), px, py, 1);
}

// The per-port tints (spec §3.3; upstream's own port colours).
static const RastCol kPortTint[4] = {{218, 51, 51, 256},
                                     {51, 53, 218, 256},
                                     {226, 218, 34, 256},
                                     {44, 217, 29, 256}};

// The silver header: MELEE plate, VS badge, mode ribbon, BACK wedge, and the
// BACK wedge's hold bar.
//
// `bHold` is the CSS back counter (FohState.bHold, foh.c's step_css). It is
// MACHINE state, not look plane, so foh_look_canonical does NOT pin it — and
// it does not have to: upstream guards the whole bar on `bestHold > 0`
// (css.js:741) and so does this, which means a cold shot draws exactly the
// bytes it drew before the bar existed. That guard is the reason A23 costs no
// re-freeze; do not turn it into a zero-width draw.
//
// `versusMode` is the same kind of state (FohState.versusMode, foh.c's mode
// ribbon arm; A27) and gets the same treatment for the same reason: the STOCK
// mode — 0, the state every judged CSS shot is taken in — draws the ribbon it
// always drew, byte for byte, and only the ENDLESS mode paints anything new.
static void css_header(Raster *rz, int bHold, int versusMode) {
  const RastCol h0 = {150, 156, 172, 256}, h1 = {70, 76, 98, 256};
  rrect_v(rz, 0, 0, RAST_W, 26, 0, h0, h1);
  // the black slab the BACK wedge sits on (css.js draws it as a skewed quad)
  {
    const RastCol blk = {10, 10, 16, 256};
    const float q[8] = {184.0f, 0.0f, 240.0f, 0.0f, 240.0f, 26.0f, 194.0f,
                        26.0f};
    poly8(rz, q, 4, blk, 256);
  }
  {
    const RastCol ink = {16, 16, 24, 256};
    foh_text2(rz, 3, 4, 2, 1, "MELEE", ink); // 68 px + the italic lean
  }
  // the VS badge (a filled disc under a lighter ring)
  {
    const RastCol disc = {58, 62, 78, 256}, ring = {166, 171, 186, 256};
    const RastCol tx = {228, 230, 238, 256};
    disc8(rz, 91.0f, 13.0f, 9.0f, disc, 256);
    ring8(rz, 91.0f, 13.0f, 9.0f, 2.0f, ring, 256);
    foh_text(rz, 86, 10, 1, "VS", tx);
  }
  // the mode ribbon: a chevron-capped plate (css.js flanks its blurb with
  // two arrow caps), carrying upstream's own label for this mode. The plate
  // is built from the FOH_CSS_MODE_* constants foh.c hit-tests it with —
  // one source for the draw and the click box (D4, foh.h).
  {
    const RastCol pl = {26, 28, 42, 256}, ed = {150, 155, 172, 256};
    const RastCol tx = {236, 238, 246, 256};
    const float x0 = (float)FOH_CSS_MODE_X0, x1 = (float)FOH_CSS_MODE_X1;
    const float y0 = (float)FOH_CSS_MODE_Y0, y1 = (float)FOH_CSS_MODE_Y1;
    const float cap = (float)FOH_CSS_MODE_CAP, ym = (y0 + y1) * 0.5f;
    const float p[12] = {x0 + cap, y0, x1 - cap, y0, x1,       ym,
                         x1 - cap, y1, x0 + cap, y1, x0,       ym};
    const int w = FOH_CSS_MODE_X1 - FOH_CSS_MODE_X0;
    poly8(rz, p, 6, pl, 256);
    stroke_closed(rz, p, 6, 1.0f, ed, 256);
    // THE LABEL SAYS WHICH MODE IS ARMED (A27 / D28). Upstream prints its
    // own blurb beside this widget — `versusMode ? "An endless KO fest!" :
    // "4-man survival test!"` (css.js:717-721) — so the words are its, not
    // ours; what does not survive the rewrite is their WIDTH. This plate is
    // 74 px, i.e. 12 glyphs of the 5x7 face, against 19 and 20 characters.
    //   * ENDLESS (1) takes upstream's string across two rows, dropping only
    //     the article: ENDLESS / KO FEST!.
    //   * STOCK (0) keeps "VS. MELEE" — the gamemode's own name, which is
    //     what the header has always read here. "4-MAN" was a lie on the
    //     two-port build A27 shipped against, and would cost a re-freeze of
    //     every judged CSS shot for the privilege even now that A44 makes it
    //     reachable; keeping it means the DEFAULT state renders exactly
    //     the bytes it rendered before A27, which check-css-mode.sh proves
    //     by cmp rather than by assertion.
    if (versusMode) {
      text_in(rz, FOH_CSS_MODE_X0, w, 6, 1, "ENDLESS", tx);
      text_in(rz, FOH_CSS_MODE_X0, w, 14, 1, "KO FEST!", tx);
    } else {
      text_in(rz, FOH_CSS_MODE_X0, w, 10, 1, "VS. MELEE", tx);
    }
  }
  // BACK: gold text behind a red arrow head (css.js:back button)
  {
    const RastCol gold = {247, 208, 32, 256}, red = {214, 26, 26, 256};
    const float a[6] = {198.0f, 13.0f, 206.0f, 6.0f, 206.0f, 20.0f};
    poly8(rz, a, 3, red, 256);
    foh_text(rz, 210, 10, 1, "BACK", gold);
    // The hold bar (css.js:735-746), the owner's "little red bar that fills
    // below the back button". Upstream takes `bestHold` = max over its four
    // ports; this device has ONE hand, so however many ports exist there is
    // one bHold counter and the max over a one-element set is the element
    // (A44 widened the ports, not the hands). Upstream's quad is
    // (1020,125) (abb,125) (abb,119) (1015,119) with abb = 1020 + 6*bestHold,
    // i.e. a left-to-right fill along the bottom lip of the wedge, leaning 5
    // px left on its top edge; FOH_CSS_BACK_BAR_* carry that shape at
    // upstream's own ratios (foh.h). The red is the ARROWHEAD's red, which is
    // upstream's own identity — css.js paints both the arrowhead (:580) and
    // the bar (:737) rgb(194,24,8), one colour for one widget — carried here
    // as this palette's {214,26,26} rather than as two different reds.
    if (bHold > 0) {
      const float w = (float)bHold * FOH_CSS_BACK_BAR_PER_FRAME;
      const float x1 = FOH_CSS_BACK_BAR_X0 + w;
      const float q[8] = {FOH_CSS_BACK_BAR_X0, 26.0f,
                          x1,                  26.0f,
                          x1,                  FOH_CSS_BACK_BAR_TOP,
                          FOH_CSS_BACK_BAR_LEAN, FOH_CSS_BACK_BAR_TOP};
      poly8(rz, q, 4, red, 256);
    }
  }
}

// One 44x30 character cell: the upstream gradient body with its black name
// strip. The strip is what makes the row read as upstream's at this size.
static void css_cell(Raster *rz, int k, int hot) {
  const RastCol c0 = {41, 47, 68, 256}, c1 = {85, 95, 128, 256};
  const RastCol strip = {6, 6, 10, 256}, name = {214, 216, 226, 256};
  const RastCol edge = {120, 128, 156, 256}, hotEdge = {254, 238, 27, 256};
  const int x = css_cell_x(k), y = CSS_CELL_Y;
  rrect_v(rz, x, y, CSS_CELL_W, CSS_CELL_H, 4, c0, c1);
  // the portrait, centre-cropped to the cell: 58 px of art into a 44 px
  // cell, top-aligned so the head fills it (upstream crops its cells too).
  {
    const Img1Image *im = art(kCharArt[k]);
    const int sw = CSS_CELL_W - 4, sh = CSS_CELL_H - 12;
    blit_img_rect(rz, im, x + 2, y + 1, (im->w - sw) / 2, 0, sw, sh, 1);
  }
  rrect(rz, x + 1, y + CSS_CELL_H - 10, CSS_CELL_W - 2, 9, 2, strip);
  text_in(rz, x, CSS_CELL_W, y + CSS_CELL_H - 9, 1, kCharShort[k], name);
  // 1 px outline, brightened while the d-pad row owns this cell.
  {
    const float b[8] = {(float)x,
                        (float)y,
                        (float)(x + CSS_CELL_W - 1),
                        (float)y,
                        (float)(x + CSS_CELL_W - 1),
                        (float)(y + CSS_CELL_H - 1),
                        (float)x,
                        (float)(y + CSS_CELL_H - 1)};
    stroke_closed(rz, b, 4, 1.0f, hot ? hotEdge : edge, 256);
  }
}

// One port token: the disc upstream drops on the cell a port has picked —
// drawn wherever the machine says it is (foh_css_token_pos), which is the
// hand itself while that token is being carried.
static void css_token(Raster *rz, double x, double y, int port) {
  const float cx = (float)x;
  const float cy = (float)y;
  const RastCol rim = {250, 250, 252, 256}, tx = {255, 255, 255, 256};
  const char lbl[3] = {'P', (char)('1' + port), 0};
  disc8(rz, cx, cy, (float)FOH_CSS_TOKEN_R, kPortTint[port], 256);
  ring8(rz, cx, cy, (float)FOH_CSS_TOKEN_R, 1.0f, rim, 256);
  foh_text(rz, (int)cx - 5, (int)cy - 3, 1, lbl, tx);
}

// One port panel. `type`: 0 human, 1 cpu, -1 unoccupied (upstream N-A).
// `diff` is that port's own CPU level (1..4) — P1 can be CPU too now — and
// `knobX` is the machine's continuous slider position for this port.
static void css_panel(Raster *rz, int port, int type, int chr, int nameChr,
                      int diff,
                      double knobX, int hotTab, int hotCpu) {
  const int x = css_panel_x(port), y = CSS_PANEL_Y;
  const int h = 120;
  const RastCol tint = kPortTint[port];
  const RastCol dim = {56, 58, 72, 256};
  const RastCol empty0 = {34, 36, 46, 256}, empty1 = {16, 17, 24, 256};
  RastCol b0, b1, edge;
  if (type < 0) {
    b0 = empty0;
    b1 = empty1;
    edge = dim;
  } else {
    // the port tint at ~30%, ramped to near-black (css.js panel gradient)
    b0.r = (uint8_t)(tint.r / 3 + 14);
    b0.g = (uint8_t)(tint.g / 3 + 14);
    b0.b = (uint8_t)(tint.b / 3 + 14);
    b0.a256 = 256;
    b1 = (RastCol){10, 10, 16, 256};
    edge = tint;
  }
  rrect_v(rz, x, y + CSS_TAB_H - 1, CSS_PANEL_W, h - CSS_TAB_H + 1, 4, b0, b1);

  // the type tab: a chevron-capped tag at the panel's top-left (css.js).
  {
    const char *lbl = type < 0 ? "N-A" : (type == 1 ? "CPU" : "HMN");
    const RastCol na = {82, 81, 81, 256};   // spec §3.3 N-A grey
    const RastCol cpu = {91, 91, 91, 256};  // spec §3.3 CPU grey
    const RastCol hmn = {44, 42, 14, 256};
    const RastCol fill = type < 0 ? na : (type == 1 ? cpu : hmn);
    const RastCol tx = type == 0 ? (RastCol){254, 238, 27, 256}
                                 : (RastCol){222, 222, 228, 256};
    // The chevron's tip is FOH_CSS_TAB_W: the hit rect in foh.c is the tab's
    // full drawn width, so the constant must be the SAME one (D4).
    const float t[10] = {(float)x,
                         (float)y,
                         (float)(x + FOH_CSS_TAB_W - 6),
                         (float)y,
                         (float)(x + FOH_CSS_TAB_W),
                         (float)(y + 5),
                         (float)(x + FOH_CSS_TAB_W - 6),
                         (float)(y + CSS_TAB_H),
                         (float)x,
                         (float)(y + CSS_TAB_H)};
    poly8(rz, t, 5, fill, 256);
    stroke_closed(rz, t, 5, 1.0f, hotTab ? (RastCol){254, 238, 27, 256} : edge,
                  256);
    foh_text(rz, x + 5, y + 2, 1, lbl, tx);
  }

  if (type < 0) {
    // upstream's empty-port watermark: the big slashed circle plus the
    // dot row along the bottom.
    const RastCol wm = {48, 50, 62, 256};
    const float cx = (float)(x + CSS_PANEL_W / 2), cy = (float)(y + 58);
    ring8(rz, cx, cy, 17.0f, 2.0f, wm, 256);
    line8(rz, cx - 12.0f, cy + 12.0f, cx + 12.0f, cy - 12.0f, wm, 256);
    for (int k = 0; k < 6; k++) {
      disc8(rz, (float)(x + 8 + k * 9), (float)(y + h - 8), 2.0f, wm, 256);
    }
  } else {
    // The portrait (58 px wide == the panel width, so it bleeds edge to edge
    // exactly as upstream's silhouettes do), then the name plate. These read
    // DIFFERENT planes upstream and so do we: the preview model comes from
    // `characterSelections[i]` (css.js:889) while the name plate switches on
    // `chosenChar[i]` (css.js:986). They only diverge once something writes
    // the shared plane without the CSS's own — which target-select's shoulder
    // arms do (targetselect.js:60-69 calls setCS alone).
    const Img1Image *im = art(kCharArt[chr]);
    blit_img(rz, im, x, y + CSS_TAB_H + 1, 1);
    {
      const RastCol pl = {4, 4, 8, 256}, tx = {240, 242, 250, 256};
      rrect(rz, x + 2, y + 62, CSS_PANEL_W - 4, 12, 2, pl);
      text_in(rz, x + 2, CSS_PANEL_W - 4, y + 64, 1, kCharShort[nameChr], tx);
    }
    // the ghost port letters upstream watermarks under the plate. Suppressed
    // on a CPU port: the slider box owns that space (upstream has 285 px of
    // panel to stack both; this one has 120).
    if (type != 1) {
      const RastCol gh = {70, 72, 88, 256};
      const char g[3] = {'P', (char)('1' + port), 0};
      foh_text2(rz, x + 16, y + h - 22, 2, 1, g, gh);
    }
  }

  // the CPU-level slider: upstream's gradient track with the round knob
  // carrying the level (css.js:316-329 — the 1..4 domain, foh.h delta).
  if (type == 1) {
    const RastCol box = {22, 24, 32, 256}, lab = {206, 208, 216, 256};
    const RastCol hotE = {254, 238, 27, 256};
    rrect(rz, x + 2, y + 78, CSS_PANEL_W - 4, 26, 3, box);
    if (hotCpu) {
      const float b[8] = {(float)(x + 2),
                          (float)(y + 78),
                          (float)(x + CSS_PANEL_W - 3),
                          (float)(y + 78),
                          (float)(x + CSS_PANEL_W - 3),
                          (float)(y + 103),
                          (float)(x + 2),
                          (float)(y + 103)};
      stroke_closed(rz, b, 4, 1.0f, hotE, 256);
    }
    text_in(rz, x + 2, CSS_PANEL_W - 4, y + 81, 1, "CPU LV", lab);
    // the track: blue -> red across 38 px, one lerp per column.
    {
      const RastCol t0 = {40, 60, 220, 256}, t1 = {220, 40, 40, 256};
      for (int k = 0; k < 38; k++) {
        const RastCol c = lerp_col(t0, t1, (float)k / 37.0f);
        span8(rz, x + 10 + k, x + 11 + k, y + 92, c, 256);
        span8(rz, x + 10 + k, x + 11 + k, y + 93, c, 256);
        span8(rz, x + 10 + k, x + 11 + k, y + 94, c, 256);
      }
    }
    {
      // The knob's centre is the machine's continuous slider position — the
      // same value foh.c hit-tests and drags (D4).
      const float kx = (float)knobX;
      const float ky = (float)(y + FOH_CSS_RAIL_Y);
      const RastCol knob = {228, 54, 54, 256}, rim = {252, 252, 254, 256};
      const RastCol tx = {255, 255, 255, 256};
      const char d[2] = {(char)('0' + diff), 0};
      disc8(rz, kx, ky, (float)FOH_CSS_KNOB_R, knob, 256);
      ring8(rz, kx, ky, (float)FOH_CSS_KNOB_R, 1.0f, rim, 256);
      foh_text(rz, (int)kx - 2, (int)ky - 3, 1, d, tx);
    }
  }
}

static void render_css(const FohState *s, Raster *rz) {
  // BG: upstream's dark blue body under the silver header. A VERTICAL ramp
  // (one lerp per row, then a full-row span) instead of grad_linear's
  // per-pixel diagonal projection — same look at this size, and it reduces
  // to 240 rast_fill_row_opaque calls via span8's fast path. NOTE the
  // absence of a per-pixel divide is NOT by itself why this needs no
  // backdrop-cache slot (it is a constant image redrawn every frame, which
  // a cache would still skip) — what makes redrawing it acceptable is the
  // batched row fill above (review-p1 fallback round, [L]). If this screen
  // ever misses the device budget, a BG_CSS slot is the next lever.
  {
    const RastCol b0 = {14, 16, 38, 256}, b1 = {4, 4, 12, 256};
    rrect_v(rz, 0, 0, RAST_W, RAST_H, 0, b0, b1);
  }
  css_header(rz, s->bHold, s->versusMode);

  // Which widget the hand is over. HOVER, not a row index — every "hot"
  // below is the same point-in-rect test foh.c acts on (D4).
  const double hx = s->cssHandX, hy = s->cssHandY;
  const int inBand =
      hy < (double)FOH_CSS_BAND_BOT && hy > (double)FOH_CSS_BAND_TOP;

  // the five character cells + the two port tokens. A25(c): the cell hover was
  // a hand-kept second copy of foh.c's drop test; both now call the SAME table
  // and the SAME predicate, which is what D4 has always asked for.
  {
    FohHandRect cells[5];
    foh_css_cells(cells);
    const int hotCell = foh_hand_hit(cells, 5, hx, hy);
    for (int k = 0; k < 5; k++) css_cell(rz, k, k == hotCell);
  }
  // Tokens, carried-on-top: the carried one rides the hand, so it must draw
  // over the resting one when they overlap. A port with no type has no token
  // — upstream guards both of its token passes on `playerType[i] > -1`
  // (css.js:1018 and css.js:1077). NOTE this does NOT make D4 true in both
  // directions: your OWN token stays grabbable at N/A while undrawn, because
  // upstream's grab guard is `playerType[j] == 1 || i == j` (css.js:300),
  // widened to every port by DEVIATION D40 without widening this draw.
  // That asymmetry is upstream's and is carried — foh.h D4 exception (b).
  // A44: all four ports, in D41's 2x2 (foh_css_token_pos owns the geometry).
  for (int pass = 0; pass < 2; pass++) {
    for (int k = 0; k < FOH_CSS_PORTS; k++) {
      if (foh_css_port_type(s, k) < 0) continue;
      if ((s->cssCarry == k) != pass) continue;
      double tx, ty;
      foh_css_token_pos(s, k, &tx, &ty);
      css_token(rz, tx, ty, k);
    }
  }

  // READY TO FIGHT (css.js:1167-1181): drawn IFF at least two ports are not
  // N/A and no participating port's token is held. Picking up a token
  // un-readies the screen — this ribbon is the feedback channel for the whole
  // CSS, so it is gated, never decorative. The text pulses
  // hsl(52,85%,25-50%) exactly as upstream does, off the LOOK plane's frame
  // counter, so foh_look_canonical pins it at phase 0 for every shot.
  if (s->cssReady) {
    const RastCol out = {196, 22, 30, 256}, in = {14, 6, 14, 256};
    const float o[8] = {0.0f, 66.0f, 240.0f, 62.0f, 240.0f, 88.0f, 0.0f, 92.0f};
    const float i[8] = {0.0f, 69.0f, 240.0f, 65.0f, 240.0f, 85.0f, 0.0f, 89.0f};
    poly8(rz, o, 4, out, 256);
    poly8(rz, i, 4, in, 256);
    const int ph = s->frame % 60;
    const int tri = ph < 30 ? ph : 60 - ph; // 0..30, no transcendental
    // Upstream pulses 25%..50% lightness; this is 35%..60% — the WHOLE
    // band is shifted +10 points, not just the floor, for the same reason
    // every other number on this screen is re-authored: at 240x240 the
    // judged shots render at the look plane's canonical phase
    // (frame == 0 — foh_look_canonical), which is the trough, and a 25%
    // yellow on black is barely a shape at 2 px stroke weight. Amplitude
    // (25 points) is upstream's; only the offset moved.
    const RastCol lit = hsl_col(52.0, 85.0, 35.0 + (double)tri * (25.0 / 30.0),
                                256);
    const int w = foh_text2_width("READY TO FIGHT", 2);
    foh_text2(rz, (RAST_W - w) / 2, 68, 2, 1, "READY TO FIGHT", lit);
  }

  // the four port panels — every one of them live since A44/D40 (an N/A
  // port still draws upstream's N-A panel; that is its type, not a pin)
  for (int k = 0; k < FOH_CSS_PORTS; k++) {
    const int type = foh_css_port_type(s, k);
    const int px = css_panel_x(k);
    const int hotTab = !inBand && hy > (double)CSS_PANEL_Y &&
                       hy < (double)(CSS_PANEL_Y + CSS_TAB_H) &&
                       hx > (double)px && hx < (double)(px + FOH_CSS_TAB_W);
    const int diff = foh_css_port_diff(s, k);
    // The knob is drawn at the CONTINUOUS slider position the machine holds,
    // which is also what foh.c hit-tests (D4) — not re-derived from the level.
    // A49: no `k < 2` guard any more. cssSliderX is FOUR wide (upstream's
    // own cpuSlider, css.js:72) now that D40(b) is retired and every port
    // can be CPU, so every port has a real knob position to draw.
    const double kx = foh_css_knob_x(s, k);
    const double ky = foh_css_knob_y();
    const int hotCpu = type == 1 && (s->cssCpuCarry == k ||
                                     (hy >= ky - FOH_CSS_KNOB_R &&
                                      hy <= ky + FOH_CSS_KNOB_R &&
                                      hx >= kx - FOH_CSS_KNOB_R &&
                                      hx <= kx + FOH_CSS_KNOB_R));
    // Two DIFFERENT planes, per port and never per index (CONTEXT.md): the
    // portrait reads the SELECTION (selChar, css.js:889) and the name plate
    // reads the CSS's own chosenChar (cssChar, css.js:986).
    css_panel(rz, k, type, s->selChar[k], s->cssChar[k], diff, kx, hotTab,
              hotCpu);
  }

  draw_hand(rz, hx, hy, foh_css_hand_type(s));

  {
    // The hint names the gestures that exist. There is no value stepper on
    // this screen any more, so the old "L/R: CHANGE" line would be a lie.
    const RastCol hint = {150, 152, 168, 256};
    text_center(rz, 228, 1, "B TOKEN  A GRAB/DROP  START FIGHT", hint);
  }
}

// --- STAGE SELECT (upstream stages/stageselect.js, re-authored) ------------
// A1 restyle Phase 1. Upstream: a magenta lattice over a dark violet field,
// one 800x300 preview boxed in light grey with the stage name set large
// inside it, six 150x90 thumbnails on a 175 px pitch (black name strip each,
// the hovered one's border FLASHING rgb(251,116,155)/rgb(255,182,204) on an
// 8-frame cycle) and the orange RANDOM box below.
//
// RE-AUTHORED at 240x240: the A9 stage previews are 65x24, so the "big"
// preview is the SAME artwork blitted 2x (130x48) — an integer scale, no
// resampler, no new blend math. The thumbs keep upstream's six-up shape but
// as 3x2, because the machine's cursor IS a 3x2 grid (foh.h rewrite delta);
// a single row of six would leave the d-pad's up/down arms unexplained.
// RANDOM stays TEXT: upstream only ever shows stage_random.png as an onerror
// fallback, and the slot REFUSES here anyway (foh.h — its upstream arm draws
// from the seeded RNG stream).
#define SSS_THUMB_W 69
#define SSS_THUMB_H 38
#define SSS_THUMB_X0 11
#define SSS_THUMB_Y0 92
#define SSS_THUMB_PX 74
#define SSS_THUMB_PY 46

static void render_sss(const FohState *s, Raster *rz) {
  const int cur = s->sssCursor;
  // The 8-frame border flash, off the LOOK plane (foh_look_canonical pins
  // frame == 0 for every judged shot, so this is target-independent).
  const int flash = (s->frame / 8) % 2;
  const RastCol pink = flash ? (RastCol){251, 116, 155, 256}
                             : (RastCol){255, 182, 204, 256};
  const RastCol orange = flash ? (RastCol){245, 144, 61, 256}
                               : (RastCol){251, 195, 149, 256};
  const RastCol idle = {96, 100, 118, 256};
  const RastCol strip = {6, 6, 10, 256}, name = {228, 230, 238, 256};

  // BG: the violet field + the magenta lattice (one lerp per row + the
  // grid's ~8k surviving pixels; no per-pixel transcendental).
  {
    const RastCol v0 = {30, 10, 44, 256}, v1 = {62, 16, 62, 256};
    rrect_v(rz, 0, 0, RAST_W, RAST_H, 0, v0, v1);
    const RastCol lat = {236, 120, 220, 256};
    grid_shine(rz, s->frame, lat, 20, 0); // static lattice: no sweep, no VDIV
  }

  // the big preview box
  {
    const RastCol edge = {190, 192, 202, 256}, inner = {10, 8, 20, 256};
    rrect(rz, 20, 12, 200, 72, 4, edge);
    rrect(rz, 22, 14, 196, 68, 3, inner);
    if (cur <= 5) {
      blit_img(rz, art(kStageArt[cur]), 55, 16, 2); // 130x48
      foh_text2(rz, 26, 68, 1, 0, kStageNames[cur], name);
    } else {
      // the refusing RANDOM slot's preview: upstream's own "?" panel.
      // The name comes from kStageNames[6] like every other slot's, so the
      // table stays the single source of screen names (review-p1 [L]).
      foh_text2(rz, 108, 26, 3, 0, "?", orange);
      foh_text2(rz, 26, 68, 1, 0, kStageNames[6], orange);
    }
  }

  // the six thumbnails (3x2 == the machine's cursor grid)
  for (int k = 0; k < 6; k++) {
    const int x = SSS_THUMB_X0 + (k % 3) * SSS_THUMB_PX;
    const int y = SSS_THUMB_Y0 + (k / 3) * SSS_THUMB_PY;
    rrect(rz, x, y, SSS_THUMB_W, SSS_THUMB_H, 3, k == cur ? pink : idle);
    rrect(rz, x + 2, y + 2, 65, 34, 2, strip);
    blit_img(rz, art(kStageArt[k]), x + 2, y + 2, 1);
    text_in(rz, x + 2, 65, y + 27, 1, kStageShort[k], name);
  }

  // the RANDOM box (cursor 6): VISIBLE but refusing (foh.h note) — drawn in
  // upstream's orange, flashing on the same 8-frame cycle when hovered.
  {
    const RastCol box = {18, 10, 22, 256};
    const RastCol ed = cur == 6 ? orange : idle;
    const RastCol tx = cur == 6 ? orange : (RastCol){170, 172, 186, 256};
    rrect(rz, 74, 182, 92, 24, 3, ed);
    rrect(rz, 76, 184, 88, 20, 2, box);
    ring8(rz, 100.0f, 194.0f, 8.0f, 2.0f, ed, 256);
    foh_text2(rz, 97, 189, 1, 0, "?", tx);
    foh_text(rz, 120, 191, 1, "RANDOM", tx);
  }

  {
    const RastCol hint = {198, 170, 210, 256};
    text_center(rz, 214, 1, "A: FIGHT   B: BACK", hint);
  }
}

// --- GAMEPLAY OPTIONS (upstream menus/gameplaymenu.js; MENU-SPEC §3) --------
// The COMPLETE upstream row list, in upstream's order (:178-182), with
// upstream's value strings (:230/:233/:236/:239; :242 is inverted on display
// per DEVIATION D23, spelled out at the row-4 call site below) and its one
// multi-cell row. Labels are the upstream literals uppercased for the 5x7
// face, and the value column is right-aligned because "EVERYONE WALLJUMPS"
// is 18 glyphs on a 240 px screen where upstream had 1200 (D4: our rects,
// upstream's semantics).
static void render_opt_gameplay(const FohState *s, Raster *rz) {
  header(rz, "GAMEPLAY");
  static const char *const kRows[5] = {"TURBO MODE", "L-CANCEL",
                                       "FLASH ON L-CANCEL",
                                       "EVERYONE WALLJUMPS", "TAP JUMP"};
  const int ys[5] = {40, 66, 92, 118, 144};
  const char *vals[4];
  vals[0] = s->turbo ? "ON" : "OFF";              // :230
  vals[1] = kLCancelNames[s->lCancelType];        // :233
  vals[2] = s->flashOnLCancel ? "ON" : "OFF";     // :236
  vals[3] = s->everyCharWallJump ? "ON" : "OFF";  // :239
  for (int r = 0; r < 5; r++) {
    row_label(rz, ys[r], r, s->optRow, kRows[r]);
    if (r == 4) continue;
    const int w = foh_text_width(vals[r], 1);
    foh_text(rz, RAST_W - 12 - w, ys[r], 1, vals[r], kAccent);
  }
  // Row 4 is menuHOptions[4] == 3, i.e. FOUR cells, one per port
  // (:242 renders "On"/"Off" per column). Upstream splits its single value
  // box into equal columns; ours sits under the label for width.
  for (int k = 0; k < 4; k++) {
    const int x = 22 + k * 52;
    const int y = ys[4] + 16;
    if (s->optRow == 4 && s->optCol == k) {
      fill_rect(rz, x - 4, y - 3, 48, 15, kPanel);
    }
    const char pn[3] = {'P', (char)('1' + k), 0};
    foh_text(rz, x, y, 1, pn, kDim);
    // DEVIATION D23 (A32, owner-reported): upstream's row is the double
    // negative "Tapjump off" whose value reads "On" when tap jump is
    // DISABLED (:242). The owner read that row as a defaults bug and filed
    // one. The STATE plane keeps upstream's polarity — `tapJumpOff` still
    // means "off", and foh_persist/foh_app/foh_dev hand that bit to the sim
    // unchanged — so this inverts on DISPLAY ONLY: the label above is
    // "TAP JUMP" and the value is the ENABLED state, lit when the feature
    // is on exactly like every other row on this screen.
    const int tapJumpOn = !s->tapJumpOff[k];
    foh_text(rz, x + 16, y, 1, tapJumpOn ? "ON" : "OFF",
             tapJumpOn ? kAccent : kDim);
  }
  text_center(rz, 205, 1, "A: CHANGE   B: BACK", kDim);
}

// --- AUDIO OPTIONS (upstream menus/audiomenu.js; MENU-SPEC §4) -------------
// Upstream's screen IS two wedges: a grey triangle (200,350+250i) ->
// (1000,200+250i) -> (1000,350+250i) with the volume-filled part drawn over
// it in a horizontal gradient, and a knob disc whose RADIUS GROWS WITH THE
// VALUE (15 + vol*65 — audiomenu.js:204). That is the whole visual language,
// so it is kept; only the coordinates are D4-scaled (x/5, y*0.32) and the
// row labels move above their wedge, because at 240 px the upstream label
// position (centred on x=225) sits on top of the wedge's thin end.
#define AUD_X0 40
#define AUD_X1 200
#define AUD_Y0 112
#define AUD_PITCH 80
#define AUD_RISE 48

static void render_opt_audio(const FohState *s, Raster *rz) {
  header(rz, "AUDIO");
  // BG: upstream's own linear ramp rgb(11,65,39) -> rgb(8,20,61)
  // (audiomenu.js:124-127) plus the white 30 px lattice (:160-166).
  {
    const RastCol g0 = {11, 65, 39, 256}, g1 = {8, 20, 61, 256};
    rrect_v(rz, 0, 24, RAST_W, RAST_H - 24, 0, g0, g1);
    const RastCol white = {255, 255, 255, 256};
    grid_shine(rz, s->frame, white, 26, 0);
  }
  // the panel (:130-134: black 0.5 fill, white 0.3 stroke, 10 px line)
  {
    const RastCol face = {0, 0, 0, 128}, edge = {255, 255, 255, 77};
    fill_rect(rz, 10, 30, RAST_W - 20, 172, face);
    rrect(rz, 10, 30, RAST_W - 20, 2, 0, edge);
    rrect(rz, 10, 200, RAST_W - 20, 2, 0, edge);
  }
  static const char *const kRow[2] = {"SOUNDS", "MUSIC"}; // :140-141
  for (int i = 0; i < 2; i++) {
    const double v = s->masterVolume[i];
    const int yb = AUD_Y0 + AUD_PITCH * i; // the wedge's baseline
    const bool sel = (s->audioRow == i);
    // the unfilled wedge (:175-180) — white at 0.3 selected, 0.1 idle
    {
      const RastCol w = sel ? (RastCol){255, 255, 255, 77}
                            : (RastCol){255, 255, 255, 26};
      const float tri[6] = {(float)AUD_X0, (float)yb, (float)AUD_X1,
                            (float)(yb - AUD_RISE), (float)AUD_X1, (float)yb};
      poly8(rz, tri, 3, w, w.a256);
    }
    // the filled part (:192-197). Upstream's horizontal gradient is
    // green->blue for sounds and navy->red for music (:182-191); a 3-point
    // fan cannot carry a gradient, so the fill takes the ramp's far stop
    // scaled by how far along it the wedge actually reaches — the same
    // colour the gradient would show at that x.
    if (v > 0.0) {
      const RastCol c0 = i == 0 ? (RastCol){12, 75, 13, 256}
                                : (RastCol){11, 13, 65, 256};
      const RastCol c1 = i == 0 ? (RastCol){15, 75, 255, 256}
                                : (RastCol){255, 15, 73, 256};
      const RastCol fc = lerp_col(c0, c1, (float)v);
      const float xr = (float)AUD_X0 + (float)v * (float)(AUD_X1 - AUD_X0);
      const float tri[6] = {(float)AUD_X0, (float)yb, xr,
                            (float)yb - (float)v * (float)AUD_RISE, xr,
                            (float)yb};
      poly8(rz, tri, 3, fc, 256);
    }
    // the knob (:203-207): centre rides the wedge's mid-height, radius
    // 15 + vol*65 on 1200 px == 3 + vol*13 here.
    {
      const RastCol k = sel ? (RastCol){255, 255, 255, 256}
                            : (RastCol){136, 136, 136, 256};
      const float cx = (float)AUD_X0 + (float)v * (float)(AUD_X1 - AUD_X0);
      const float cy = (float)yb - (float)v * (float)(AUD_RISE / 2);
      disc8(rz, cx, cy, 3.0f + (float)v * 13.0f, k, 256);
    }
    // label + the value as a percentage of the raw double (the machine
    // keeps upstream's unrounded dust; this is the readout, not the value)
    foh_text(rz, 12, yb - AUD_RISE - 12, 1, kRow[i], sel ? kText : kDim);
    {
      // DEVIATION D14 (MENU-SPEC §4 / §12.1) — this readout is an ADDED
      // 240x240 legibility adaptation, registered AND OWNER-RATIFIED
      // 2026-07-29 (presented as keep-or-strip with the removal site named;
      // the owner chose keep, rationale recorded in MENU-SPEC §4).
      // Tenths, as "0.7" — upstream prints no number at all (the wedge and
      // the knob's radius ARE the readout), but 240 px of wedge is a
      // coarse gauge, so the value is spelled out in the glyphs face 1
      // already carries. No '%': face 1's coverage is deliberately narrow
      // (foh_font.c) and widening it for one label is not worth defusing
      // any part of the missing-glyph guard.
      const int tenths = (int)(v * 10.0 + 0.5);
      // Buffer sized for the WHOLE int range the compiler can see, not for
      // the [0,10] tenths actually reachable (the audio screen clamps v to
      // [0,1], audiomenu.js:104-112). The SDK's arm gcc 10.2 cannot prove
      // that range and rejects the narrower buffer under
      // -Werror=format-truncation= (R5, 2026-07-31: host clang never
      // diagnosed it, so this TU had only ever built on the host). Widening
      // is byte-identical for every reachable value — a clamp here would
      // instead invent behaviour at inputs that cannot occur.
      char lvl[16];
      snprintf(lvl, sizeof lvl, "%d.%d", tenths / 10, tenths % 10);
      foh_text(rz, 206, yb - 6, 1, lvl, sel ? kAccent : kDim);
    }
    if (sel) foh_text(rz, 4, yb - 6, 1, ">", kCursor);
  }
  // There is no A handler upstream (:16-121) — the hint says so.
  text_center(rz, 212, 1, "L/R: LEVEL   B: SAVE + BACK", kDim);
}

// --- CONTROLS DESTINATIONS (MENU-SPEC §9; the arms are documented in
// foh.c's step_ctrl) ---------------------------------------------------------
static void render_ctrl_pad(Raster *rz) {
  header(rz, "CONTROLLER");
  // gamepadCalibration.js:71's own literal, in its own condition: no
  // navigator.getGamepads and no pad on a FunKey-S, so this is the state
  // upstream would be in. Uppercased for the 5x7 face like every other
  // label in this file.
  {
    const RastCol warn = {236, 96, 96, 256};
    ring8(rz, 120.0f, 76.0f, 16.0f, 2.0f, warn, 256);
    foh_text2(rz, 117, 68, 2, 0, "!", warn);
    text_center(rz, 110, 1, "ERROR: NO CONTROLLER DETECTED", warn);
  }
  // A33, CORRECTED 2026-08-23. This line used to read "THE FUNKEY-S HAS NO
  // GAMEPAD PORT", which is FALSE and was measured false in the same spike
  // that killed the feature: the USB port is physically there
  // (docs/research/gc-adapter.md §1.4/§2 — the earlier power-budget kill is
  // retracted in place). What is missing is HOST MODE in the shipped OS:
  // CONFIG_USB_MUSB_GADGET=y with no HOST/DUAL_ROLE (mutually exclusive
  // Kconfig in 4.14, so host code is not compiled), `dr_mode = "peripheral"`
  // in the DTS and a floating ID pin. Enabling it means rebuilding and
  // reflashing FunKey-OS; this project ships an OPK. Same width as the line
  // it replaces (32 chars), so no geometry moved, and it still reads as one
  // sentence with the line below it. D27 makes this screen unreachable at
  // FOH_CTL_CHOOSER 0 — the line is corrected anyway, because a flag-on
  // build must not paint a claim we have measured to be untrue.
  text_center(rz, 134, 1, "FUNKEY-OS SHIPS NO USB HOST MODE", kDim);
  text_center(rz, 148, 1, "AND NO CALIBRATION TO RUN.", kDim);
  text_center(rz, 205, 1, "B: BACK", kDim);
}

// The control scheme, rendered from the ACTIVE style rather than from the one
// ratified table. The BOX row is still PLAN §6's ("L=Mod, R=shield,
// Y(hold)=C-stick layer, A=attack, B=special, X=jump, Start=pause"), but it is
// now one of three (ctl_style.h) and the screen follows whichever is live;
// MENU opens the pause menu in every style (foh_dev.c). The BUTTON column is
// hardware and never varies. Since A31 (DEVIATION D26) this is no longer a
// READ-ONLY view: every button row is a cursor row and L/R rebinds it. What
// D13 sketched as "listening mode, hold-A clear, protected primaries" is
// deliberately NOT what shipped — see foh.c's step_ctrl note for why a
// permutation on an L/R row needs none of those three parts.
// foh_font.c's face 1 is UPPERCASE-ONLY (49 glyphs) and an unknown glyph is
// a hard gfx_fatal, so any string that comes from outside this file — e.g.
// ctl_style.h's display names — is folded before it reaches foh_text.
static void foh_upper(char *p) {
  for (; *p; p++) {
    if (*p >= 'a' && *p <= 'z') *p = (char)(*p - 'a' + 'A');
  }
}

static void render_ctrl_key(const FohState *s, Raster *rz) {
  // D25: this screen IS the FunKey-S's own buttons — never a keyboard. The
  // SCREEN TOKEN stays "controls-keyboard" (foh.c) and the upstream gameMode
  // stays 12: the rename is the paint, the identity is the wire format that
  // the judge grammar and every frozen flow expect key on.
  header(rz, "HANDHELD");
  static const char *const kBtn[FOH_CTL_ACTION_ROWS] = {
      "D-PAD", "A", "B", "X", "Y", "L", "R", "START", "MENU"};
  // review-r14 MAJOR: these labels used to be the BOX table, hard-coded,
  // while the FRESH-INSTALL style is NATURAL — so the screen described a
  // mapping the buttons did not have. They are now DERIVED from the same two
  // cells the input path reads (ctl_style_get / ctl_mod_on_r_get), following
  // ctl_roles() + s1_input_row_style() in port/gfx/s1_input.h (re-ratified
  // 2026-08-24, DEVIATIONS D31/D32/D33):
  //   A/B/X/Y : STYLE-INDEPENDENT — jump / attack / grab (Z) / special, in
  //             every style, BOX included.
  //   L / R   : L shields except in the BOX arrangement that puts Mod there;
  //             R is Mod in BOX, and the C-layer hold in NORMAL/NATURAL.
  // There is no second copy of the truth table here — every arm below cites
  // the predicate that decides it, so a style change cannot drift the screen.
  const CtlStyle style = ctl_style_get();
  const bool modOnR = ctl_mod_on_r_get();
  // The label table is a PURE function of (style, modOnR) and lives in
  // foh_ctl_labels.h so that check-foh-flows.sh leg [0m] can COMPILE it and
  // pin all three styles x Mod-on-L/R (review-r15 MAJOR: inline here, the
  // BOX and NORMAL rows had no coverage at all — the frozen keyboard
  // screenshot only ever exercises the fresh-install NATURAL). There is no
  // restated C-layer predicate in this TU any more.
  const char *kAct[FOH_CTL_LABEL_ROWS];
  foh_ctl_labels(style, modOnR, kAct);
  foh_text(rz, 12, 30, 1, "ACTIVE MAPPING", kAccent);
  // A31 (DEVIATION D26): the two settable rows became ELEVEN. The nine
  // action rows are now cursor rows too — that was the owner's whole
  // complaint — and the Mod-shoulder row is gone from the screen (its cell
  // survives in ctl_style.c; see foh.c's step_ctrl note).
  const int yStyle = 176, yReset = 190;
  for (int i = 0; i < FOH_CTL_ACTION_ROWS; i++) {
    const int y = 44 + i * 14;
    const bool cur = s->ctlRow == i;
    foh_text(rz, 16, y, 1, kBtn[i], cur ? kAccent : kText);
    // THE REBOUND LABEL. Row i is a PHYSICAL button; what it does is the
    // label of the LOGICAL button it is bound to (ctl_style.h). Row 0 is
    // the d-pad, which drives the control stick and is not one of the eight
    // bindable buttons, so it always reads its own label. Under the
    // identity binding this loop is exactly what it was before A31 —
    // kAct[i] — which is why the frozen label table (check-foh-flows.sh
    // leg [0m]) is untouched and still pins all three styles.
    const int act = (i == 0) ? 0 : ctl_bind_get(0, i - 1) + 1;
    foh_text(rz, 96, y, 1, kAct[act], cur ? kAccent : kDim);
  }
  // C30(c): the style row. Drawn from the same cell foh.c's step_ctrl
  // writes and the input path reads (ctl_style.h), so there is no second
  // copy that can drift out of sync with what the buttons actually do.
  {
    char buf[40];
    // UPPERCASE at the RENDER site, not in ctl_style.c: foh_font.c's face 1
    // carries no lowercase glyphs at all (49 glyphs: A-Z 0-9 and a short
    // punctuation set), and a missing glyph is a FATAL, not a blank — the
    // f04-nav flow proved it. The shared API keeps its natural-case strings
    // for any consumer with a real font; only this screen folds them.
    snprintf(buf, sizeof buf, "STYLE: %s", ctl_style_name((int)ctl_style_get()));
    foh_upper(buf);
    foh_text(rz, 16, yStyle, 1, buf,
             s->ctlRow == FOH_CTL_ROW_STYLE ? kAccent : kDim);
  }
  foh_text(rz, 16, yReset, 1, "RESET TO DEFAULTS",
           s->ctlRow == FOH_CTL_ROW_RESET ? kAccent : kDim);
  // the cursor caret, so the selected row reads at a glance at 240x240
  {
    const int yCur = s->ctlRow < FOH_CTL_ACTION_ROWS
                         ? 44 + s->ctlRow * 14
                         : (s->ctlRow == FOH_CTL_ROW_STYLE ? yStyle : yReset);
    foh_text(rz, 6, yCur, 1, ">", kAccent);
  }
  // A31 sub-item 3, answered rather than deleted blind: `REBIND: N/A` was
  // this screen saying out loud that DEVIATION D13's rebinder did not exist
  // — the honest caption for a read-only view of the mapping. It exists
  // now, so the caption names the control that does it.
  text_center(rz, 204, 1, "L/R: CHANGE   A: RESET", kDim);
  text_center(rz, 216, 1, "B: BACK", kDim);
}

// --- CREDITS (upstream menus/credits.js drawCredits :314-422 +
// drawCreditsInfo :285-311; MENU-SPEC §8; punch-list A7) ---------------------
//
// The screen has no title bar, because upstream's has none: it is the warp
// field edge to edge, with the score box and the info panel drawn over it.
//
// ONE THING UPSTREAM HAS THAT THIS CANNOT: TRAILS. Its per-frame background
// is `rgba(0,0,0,0.4)` painted OVER the previous frame (:316-317), so stars
// and laser bolts smear. This renderer is a pure function of FohState and
// clears the frame — that property is what makes every captured shot
// byte-stable x2, and every FOH check leans on it — so each frame draws only
// what the state says is there. Nothing is invented to fake the smear.
//
// D4 SCALE: x/5, y*0.32, the audio screen's note. Which quantities are in
// canvas units and which in raster pixels is settled in foh.h's FOH_CRED_*
// block; every literal below says which one it came from.

// laserColors (credits.js:32-37), in order — X cycles forward, Y back.
static const RastCol kCredLaser[4] = {{255, 15, 5, 256},
                                      {15, 5, 255, 256},
                                      {5, 255, 15, 256},
                                      {255, 85, 3, 256}};
// The info panel's own palette (drawCreditsInfo :288-290).
static const RastCol kCredFace = {0, 0, 0, 179};       // "rgba(0,0,0,0.7)"
static const RastCol kCredEdge = {255, 255, 255, 179}; // white 0.7
static const RastCol kCredWhite = {255, 255, 255, 256};
static const RastCol kCredShotName = {227, 89, 89, 256}; // :365
static const RastCol kCredRing = {255, 255, 255, 179};   // :372
static const RastCol kCredRingHot = {204, 0, 0, 179};    // :377

// A 1 px frame: upstream's `lineWidth = 2` on a 1200 px canvas is 0.4 px
// here, and `lineW8` refuses a translucent stroke by design.
static void cred_box(Raster *rz, int x, int y, int w, int h) {
  fill_rect(rz, x, y, w, h, kCredFace);
  fill_rect(rz, x, y, w, 1, kCredEdge);
  fill_rect(rz, x, y + h - 1, w, 1, kCredEdge);
  fill_rect(rz, x, y, 1, h, kCredEdge);
  fill_rect(rz, x + w - 1, y, 1, h, kCredEdge);
}

// Face 1 holds 40 columns across 240 px (advance 6). Upstream sets the blurb
// as one centred run of 25 px Consolas across a 1000 px bar; at this size the
// longest of the fourteen (67 characters) needs two lines, so it is
// word-wrapped. That is a 240 px LAYOUT adaptation of the same authored
// string — no word dropped, shortened or reordered, and check-credits.sh
// asserts the wrapped lines rejoin to the authored blurb exactly.
#define CRED_INFO_COLS 40
#define CRED_INFO_LINES 2
static int cred_wrap(const char *s,
                     char out[CRED_INFO_LINES][CRED_INFO_COLS + 1]) {
  int n = 0, len = 0;
  out[0][0] = 0;
  for (const char *p = s; *p;) {
    const char *e = p;
    while (*e && *e != ' ') e++;
    const int wlen = (int)(e - p);
    if (wlen > CRED_INFO_COLS) {
      gfx_fatal("foh_render: a credits blurb has an unbreakable word");
    }
    if (len > 0 && len + 1 + wlen > CRED_INFO_COLS) {
      if (n + 1 >= CRED_INFO_LINES) {
        // Unreachable for the authored fourteen (MEASURED: two lines, the
        // longest 39 columns) and asserted by check-credits.sh, so this is a
        // tripwire for an edited blurb, never a live arm.
        gfx_fatal("foh_render: a credits blurb does not fit two lines");
      }
      n++;
      len = 0;
      out[n][0] = 0;
    }
    if (len > 0) out[n][len++] = ' ';
    for (int k = 0; k < wlen; k++) out[n][len++] = p[k];
    out[n][len] = 0;
    p = e;
    while (*p == ' ') p++;
  }
  return n + 1;
}

// Uppercase a credit string for face 1 (the foh_upper contract above) and
// centre it on `cx`.
static void cred_text_center(Raster *rz, int cx, int y, const char *s,
                             RastCol col) {
  char buf[80];
  snprintf(buf, sizeof buf, "%s", s);
  foh_upper(buf);
  foh_text(rz, cx - foh_text_width(buf, 1) / 2, y, 1, buf, col);
}

static void render_credits(const FohState *s, Raster *rz) {
  // --- the warp field (:318-332). Upstream's 3x3 star is 0.6 px at D4's x
  // scale, so it is one pixel; the grey ramp `min(255, life*3)` is verbatim.
  for (int n = 0; n < FOH_CRED_STARS; n++) {
    const FohCredStar *st = &s->credStar[n];
    int c = st->life * 3; // :329
    if (c > 255) c = 255;
    if (c < 0) c = 0;
    const RastCol col = {(uint8_t)c, (uint8_t)c, (uint8_t)c, 256};
    fill_rect(rz, iround((float)st->x), iround((float)st->y), 1, 1, col);
  }

  // --- the laser bolts (:334-352). Drawn from lastPosition2 to position in
  // upstream's y-flipped space, in the selected laser colour, with a stroke
  // that thins as the bolt ages (`max(1, 20 - life)`, /5 for D4).
  for (int n = 0; n < FOH_CRED_SHOTS; n++) {
    const FohCredShot *sh = &s->credShot[n];
    if (!sh->live) continue;
    float lw = (float)(20 - sh->life) / 5.0f; // :344
    if (lw < 1.0f) lw = 1.0f;
    lineW8(rz, (float)sh->l2x, (float)((double)RAST_H - sh->l2y), (float)sh->x,
           (float)((double)RAST_H - sh->y), lw, kCredLaser[s->credLaser],
           256); // :347-348
  }

  // --- the names (:359-371). `canRender` gates the draw upstream and gates
  // it here; a name that has been shot turns red and stays on screen.
  FohHandRect rect[FOH_CRED_NAMES];
  foh_credits_name_rects(s, rect);
  for (int i = 0; i < FOH_CRED_NAMES; i++) {
    if (!s->credNameRender[i]) continue;
    char buf[32];
    snprintf(buf, sizeof buf, "%s", foh_credits[i].name);
    foh_upper(buf);
    foh_text(rz, rect[i].x, rect[i].y, 1, buf,
             s->credNameShot[i] ? kCredShotName : kCredWhite);
  }

  // --- the reticle (:372-421). Upstream's ring is r 35 / stroke 9 on a
  // 1200 px canvas = r 7 / stroke 2 here, and the four spokes run from r 10
  // to r 35 = 2 to 7 (cRectSpace / cRectLength + cRectSpace, :19-20). The
  // spokes are 1 px rather than 2: a thick stroke must be opaque (lineW8's
  // own guard) and this one is deliberately the 0.7-alpha white upstream
  // paints it.
  //
  // The ring goes red while it sits on an unshot name — upstream scans every
  // name and only ever ASSIGNS red (:373-381), never back, so any hover
  // reddens it. Same predicate as the shot test, off the same rect table.
  RastCol ring = kCredRing;
  for (int i = 0; i < FOH_CRED_NAMES; i++) {
    if (s->credNameShot[i]) continue;
    if (s->credX >= (double)rect[i].x &&
        s->credX <= (double)(rect[i].x + rect[i].w) &&
        s->credY >= (double)rect[i].y &&
        s->credY <= (double)(rect[i].y + rect[i].h)) {
      ring = kCredRingHot;
    }
  }
  {
    const float cx = (float)s->credX, cy = (float)s->credY;
    ring8(rz, cx, cy, 7.0f, 2.0f, ring, ring.a256);
    // cDefaultAngles (:20) plus the running cursor angle in radians (:403).
    static const double kBase[4] = {0.0, 0.5 * 3.141592653589793,
                                    3.141592653589793,
                                    1.5 * 3.141592653589793};
    const double rad = (s->credCursorAngle / 180.0) * 3.141592653589793;
    for (int i = 0; i < 4; i++) {
      const float c = (float)fd_cos(kBase[i] + rad);
      const float sn = (float)fd_sin(kBase[i] + rad);
      line8(rz, cx + c * 2.0f, cy + sn * 2.0f, cx + c * 7.0f, cy + sn * 7.0f,
            ring, ring.a256); // :406-413
    }
  }
  // The `initc === true` reticle (:382-395, a cross with gaps) is NOT drawn:
  // upstream can show it because its draw loop is independent of its tick, so
  // one frame lands between changeGamemode(13) and the first credits() call.
  // Here the tick always runs first and clears `credInit`, so that arm is
  // unreachable by construction rather than omitted.

  // --- the info panel (drawCreditsInfo :285-311). The BOXES are always on
  // screen (upstream paints them into the persistent `ui` layer on entry and
  // repaints on every hit); the TEXT appears only while a hit's 600-frame
  // dwell is running.
  //
  // 240 px LAYOUT adaptation: upstream sets the name and the role SIDE BY
  // SIDE, in a 330 px and a 670 px box. At face 1's advance the longest role
  // ("Animation Assistant, Level Design") is 197 px, so the two boxes are
  // stacked full width instead — same two fields, same order, same band of
  // the screen (upstream's 560..700 of 750 is 179..224 of 240).
  cred_box(rz, 20, 179, 200, 11); // :293-296 name box
  cred_box(rz, 20, 190, 200, 11); // :294-296 role box
  cred_box(rz, 20, 205, 200, 19); // :291-292 information bar
  cred_box(rz, 200, 16, 30, 16);  // :297-298 score box
  if (!s->credHitCleared) {       // :301
    const FohCredit *c = &foh_credits[s->credHitIdx];
    cred_text_center(rz, 120, 181, c->name, kCredWhite);     // :303
    cred_text_center(rz, 120, 192, c->position, kCredWhite); // :305
    char line[CRED_INFO_LINES][CRED_INFO_COLS + 1];
    const int nl = cred_wrap(c->info, line); // :307
    for (int k = 0; k < nl; k++) {
      cred_text_center(rz, 120, 206 + k * 9, line[k], kCredWhite);
    }
  }
  {
    char sc[16]; // `cScore + " Hit"` (:310), uppercased for face 1
    snprintf(sc, sizeof sc, "%d HIT", s->credScore);
    cred_text_center(rz, 215, 20, sc, kCredWhite);
  }
}

static void render_match(Raster *rz) {
  // The FOH machine is terminal here; the driver owns the sim/renderer.
  text_center(rz, 110, 2, "LAUNCHING", kText);
}

// target-select (upstream drawTSS/drawTSSInit, stages/targetselect.js:
// drawTSSInit :183-242 whole; drawTSS :244-~420 of its real :244-540 span
// (tail :421-540 NOT read); rewritten at 240x240 — foh.h rewrite deltas). Slots are the
// upstream 2-col x 5-row authored layout (col = floor(j/5), row = j%5)
// plus the refusing "+ ADD CODE" slot; the records line is the honest
// fresh-boot value (targetRecords ≡ -1 -> "--:--:--", targetplay.js:40 +
// targetselect.js:411-412; READ/persistence = task 13, medal/dev times
// deferred — foh.h note).
static void render_tss(const FohState *s, Raster *rz) {
  // MEASURED LOOK (drawTSSInit :183-242 whole + drawTSS :244-~:420 of its
  // real :244-540 span; the tail :421-540 was NOT read), which the
  // previous pass did not carry at all: a dark BROWN linear ramp
  // rgb(66,42,6) -> rgb(26,2,2) (:184-187) under the same white 30 px
  // lattice every other menu has (:257-265); slot bodies are BLACK
  // fillRects 250x50 at (50 + col*260, 110 + row*60) with "Target N" in
  // white 0.6 (:196-203); the slot BORDER is a stroke that is grey
  // rgb(166,166,166) idle and FLASHES rgb(251,116,155)/rgb(255,182,204) on
  // an 8-frame cycle when hovered (:270-277) — the same pink flash the SSS
  // uses, NOT an orange one; the brown/orange in this screen is the BRONZE
  // MEDAL gradient rgb(180,123,65)->rgb(236,179,120) (:325-327). Below sits
  // a black-0.5 / white-0.5 info panel (:206-208) carrying Personal Best,
  // and a rounded character plate with a chevron above and below (:210-241).
  //
  // NOT DRAWN, and it is a data deferral rather than a styling one: the
  // three medal discs per slot, the three big medal-time discs and the
  // Developer Record row all read medalTimes[][][] / devRecords[][] /
  // medalsEarned[][][], which are AUTHORED UPSTREAM DATA that the M1
  // pipeline does not emit (measured: no medalTimes/devRecords stage).
  // HARD RULE 5 forbids retyping engine data by hand, so drawing them
  // would mean inventing numbers, and drawing empty medal outlines would
  // assert "not earned" for a player who has earned them. The pipeline
  // extension stays registered (foh.h, iter 99).
  {
    const RastCol b0 = {66, 42, 6, 256}, b1 = {26, 2, 2, 256};
    rrect_v(rz, 0, 0, RAST_W, RAST_H, 0, b0, b1);
    const RastCol white = {255, 255, 255, 256};
    grid_shine(rz, s->frame, white, 22, 0);
  }
  header(rz, "TARGET TEST");
  // The hovered-slot flash, off the LOOK plane. targetselect.js:271
  // verbatim: `targetSelectTimer % 8 > 4` — an 8-frame cycle that is HOT on
  // 3 of every 8 frames (remainders 5,6,7), NOT a 16-frame 50/50 one
  // (review-r1). The counter is `tssTimer`, the SCREEN-LOCAL timer, not the
  // global `frame`, because upstream only advances it while this screen
  // draws (review-r3); foh_look_canonical pins tssTimer == 0 for every
  // judged shot, so the flash is cold in shots and target-independent.
  const int flash = (s->tssTimer % 8) > 4;
  const RastCol hot = flash ? (RastCol){251, 116, 155, 256}
                            : (RastCol){255, 182, 204, 256};
  const RastCol idle = {166, 166, 166, 256};         // :279
  const RastCol slotBg = {0, 0, 0, 256};             // :195 fillStyle black
  // DEVIATION D24 (A25a, owner-reported "the highlighting around test 1 and
  // + ADD CODE you selected is not really visible to the eye"). Upstream's
  // whole selection signal is a ONE-PIXEL stroke that goes grey -> pink
  // (:270-279); at 1200 px that reads, at 240 it is 242 changed pixels
  // around a black box on a busy brown gradient. Two changes, both inside
  // upstream's own idiom of "the BORDER carries the state, never the label":
  // the selected slot's border is drawn TWICE (an outer ring 1 px further
  // out, so 2 px total) and its body is lifted off black to this pink-tinted
  // ink. The label stays :201's grey in both states (:2049's constant).
  const RastCol slotSel = {52, 22, 32, 256};
  // :201 `rgba(255,255,255,0.6)` over the black slot body == 153 opaque.
  // CONSTANT: upstream never brightens a label on hover, only its border.
  const RastCol slotTx = {153, 153, 153, 256};
  // 2x5 grid of authored target stages (ids 0..9 == tstage ids), upstream's
  // own col = floor(j/5) / row = j%5 mapping. A25(c): the rects come from
  // foh_tss_slots(), the same table the hand is hit-tested against, so the
  // drawn extent and the hit region cannot drift apart (D4).
  FohHandRect slot[FOH_TSS_SLOTS];
  foh_tss_slots(slot);
  for (int k = 0; k < 10; k++) {
    const int x = slot[k].x, y = slot[k].y;
    const int sel = k == s->tssCursor;
    if (sel) rrect(rz, x - 2, y - 2, slot[k].w + 4, slot[k].h + 4, 0, hot);
    rrect(rz, x - 1, y - 1, slot[k].w + 2, slot[k].h + 2, 0, sel ? hot : idle);
    fill_rect(rz, x, y, slot[k].w, slot[k].h, sel ? slotSel : slotBg);
    char label[10] = "TARGET ";
    if (k == 9) { label[7] = '1'; label[8] = '0'; label[9] = 0; }
    else { label[7] = (char)('1' + k); label[8] = 0; }
    foh_text(rz, x + 6, y + 6, 1, label, slotTx);
  }
  // the refusing "+ Add Code" slot (builder plane; foh.h note). Upstream
  // puts it at the head of the CUSTOM column (:206, i == 10 -> x = 635);
  // the rewrite puts it in the row below the grid — slot 10 of the same
  // shared table, so the hand hovers exactly the box drawn here.
  {
    const int x = slot[10].x, y = slot[10].y;
    const int w = slot[10].w, h = slot[10].h;
    const int sel = s->tssCursor == 10;
    // D24, with one measured asymmetry: the ring grows 1 px on the top and
    // both sides but NOT the bottom, because the info panel below starts at
    // y == 160 and its face is a 50% black fill (:2164) that would half
    // darken exactly that row across the panel's width.
    if (sel) rrect(rz, x - 2, y - 2, w + 4, h + 3, 0, hot);
    rrect(rz, x - 1, y - 1, w + 2, h + 2, 0, sel ? hot : idle);
    fill_rect(rz, x, y, w, h, sel ? slotSel : slotBg);
    foh_text(rz, x + 6, y + 5, 1, "+ ADD CODE", slotTx);
  }
  // the character plate (:210-241): a rounded gradient tile with a chevron
  // on each side. Upstream stacks its chevrons vertically; ours point along
  // the axis the control actually uses — the SHOULDER buttons
  // (targetselect.js:60-74), which is what the L/R caps say.
  {
    const RastCol p0 = {41, 47, 68, 256}, p1 = {85, 95, 128, 256};
    const RastCol ed = {157, 157, 157, 256}, chev = {180, 180, 180, 256};
    const int px = 20, py = 160, pw = 60, ph = 62;
    rrect(rz, px - 1, py - 1, pw + 2, ph + 2, 4, ed);
    rrect_v(rz, px, py, pw, ph, 4, p0, p1);
    {
      // centre-cropped like the CSS cells (the portraits are wider than
      // this plate); top-aligned so the head fills it.
      // The A9 portraits are 58 px wide but only 37-45 TALL, and the
      // height differs per character (marth 40, puff 37, falco 45), so the
      // crop is clamped to the source rather than assumed square — the
      // same lesson the CSS cells learned with their own 40x18 crop.
      const Img1Image *im = art(kCharArt[s->p1Char]);
      const int sw = (pw - 4 < im->w) ? pw - 4 : im->w;
      const int shWant = ph - 14;
      const int sh = (shWant < im->h) ? shWant : im->h;
      blit_img_rect(rz, im, px + 2, py + 2, (im->w - sw) / 2, 0, sw, sh, 1);
    }
    text_in(rz, px, pw, py + ph - 9, 1, kCharShort[s->p1Char], kText);
    const float lt[6] = {(float)(px - 5), (float)(py + 30),
                         (float)(px + 3), (float)(py + 24),
                         (float)(px + 3), (float)(py + 36)};
    const float rt[6] = {(float)(px + pw + 5), (float)(py + 30),
                         (float)(px + pw - 3), (float)(py + 24),
                         (float)(px + pw - 3), (float)(py + 36)};
    poly8(rz, lt, 3, chev, 256);
    poly8(rz, rt, 3, chev, 256);
    foh_text(rz, px - 8, py + ph - 9, 1, "L", chev);
    foh_text(rz, px + pw + 3, py + ph - 9, 1, "R", chev);
  }
  // the info panel (:206-208: black 0.5 fill, white 0.5 stroke) carrying
  // the Personal Best row (:404-419).
  {
    const RastCol face = {0, 0, 0, 128}, edge = {255, 255, 255, 128};
    const int x = 92, y = 160, w = 140, h = 62;
    fill_rect(rz, x, y, w, h, face);
    rrect(rz, x, y, w, 1, 0, edge);
    rrect(rz, x, y + h - 1, w, 1, 0, edge);
    rrect(rz, x, y, 1, h, 0, edge);
    rrect(rz, x + w - 1, y, 1, h, 0, edge);
    // records line (task 13 — the READ path through the persist plane):
    // upstream format targetselect.js:411-419 — -1 -> "--:--:--", else
    // "0"+floor(rec/60)+":"+((rec%60).toFixed(2), 5-char left-padded).
    // C form: integer centiseconds cs = (long)(rec*100 + 0.5) — no libc
    // float formatting on the device path (the iter-38/74 musl rounding
    // class; registered formatting delta, AGENT-LOG iter 100). The
    // addcode slot (cursor 10) keeps the dashes (foh.h note), and so does
    // upstream: it swaps the whole row for "Add custom stage" there
    // (:403), which is exactly what the refusal already says.
    // Sized for the full `long` range the compiler can see (arm gcc 10.2
    // computes a 17-byte worst case and rejects a 16-byte buffer under
    // -Werror=format-truncation=; R5, 2026-07-31 — host clang was silent,
    // so this path had only ever been compiled on the host). Reachable
    // records are far smaller; widening changes no rendered byte.
    char line[24] = "--:--:--";
    if (s->tssCursor <= 9) {
      const double rec = s->targetRecords[s->p1Char][s->tssCursor];
      if (rec != -1.0) {
        const long cs = (long)(rec * 100.0 + 0.5);
        snprintf(line, sizeof line, "0%ld:%02ld.%02ld", cs / 6000,
                 (cs % 6000) / 100, cs % 100);
      }
    }
    // Whose record it is: upstream's PB row is indexed by
    // characterSelections[targetPlayer] (:406), so the name belongs on it.
    text_in(rz, x, w, y + 8, 1, kCharNames[s->p1Char], kText);
    text_in(rz, x, w, y + 22, 1, "PERSONAL BEST", kDim);
    text_in(rz, x, w, y + 38, 2, line, kAccent);
  }
  text_center(rz, 228, 1, "A: GO   B: BACK", kDim);
  // The hand LAST, over everything, as the CSS draws it (A25c / D29). Type 0 —
  // handPoint — is the only sprite this screen has a meaning for.
  draw_hand(rz, s->tssHandX, s->tssHandY, 0);
}

static void render_tmatch(Raster *rz) {
  // Terminal like `match`; the driver owns the target sim/renderer.
  text_center(rz, 110, 2, "LAUNCHING", kText);
}

// COLD-FRAME WARM-UP (B3, iter 123). The FIRST title frame builds BG_TITLE
// (two full-screen radial ramps) and leaves the radial memo on the centre
// bloom; the FIRST menu frame builds BG_MENU. Inside the paced loop those are
// multi-millisecond one-off spikes that cost a render SKIP apiece, and the
// device leg is judged skips == 0. Building them BEFORE the loop starts moves
// the cost off the frame budget entirely. Bit-identical by construction: both
// caches are pure functions of compile-time constants, which is the exact
// property bg_begin/bg_end already rely on to serve frame 1 and frame N the
// same bytes. Order matters — the menu pass is LAST because it draws no
// radial, so the memo is left holding the bloom the title redraws every frame.
void foh_render_warm(Raster *rz) {
  FohState w;
  foh_init(&w);
  w.screen = FOH_TITLE;
  foh_render(&w, rz);
  w.screen = FOH_MENU_TOP;
  foh_render(&w, rz);
}

void foh_render(const FohState *s, Raster *rz) {
  // One-time, guarded by a flag: the artwork read lands on the FIRST FOH
  // frame (the startup screen, before any paced loop) on every app path,
  // never inside a frame budget. See art_load's header note.
  art_load();
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
    case FOH_OPT_AUDIO: render_opt_audio(s, rz); break;
    case FOH_CTRL_PAD: render_ctrl_pad(rz); break;
    case FOH_CTRL_KEY: render_ctrl_key(s, rz); break;
    case FOH_CREDITS: render_credits(s, rz); break;
    case FOH_MATCH: render_match(rz); break;
    case FOH_TSS: render_tss(s, rz); break;
    case FOH_TMATCH: render_tmatch(rz); break;
    default: gfx_fatal("foh_render: invalid screen");
  }
}
