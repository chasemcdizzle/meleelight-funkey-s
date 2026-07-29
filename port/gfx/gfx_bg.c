// port/gfx/gfx_bg.c — background art (M4 task 2). Structure-parallel to
// src/stages/stagerender.js drawBackgroundInit + drawBackground (holiday
// !== 1 arms): the bg1 vertical gradient, then per frame either
// drawStars() (backgroundType 0: drifting starfield + two animated
// mountain silhouettes) or drawTunnel() (type 1: ray/circle tunnel).
//
// PLANE SPLIT (U1). Upstream draws this art onto TWO canvas layers:
// drawBackgroundInit fills layers.BG1 with the static gradient ONCE per
// match (clearScreen never clears BG1), and drawStars/drawTunnel draw
// onto layers.BG2, which clearScreen DOES clear every frame. Neither is
// in the fg1|fg2|UI silhouette mask, so by default this pass runs with
// rast_ink_enable(0): the device framebuffer gets the art without
// polluting the judged fg ink plane.
//
// U1 makes the BG2 analogue judgeable without changing that: arming
// gfx_bg_ink_sink() inks ONLY the drawStars/drawTunnel pass (the BG1
// gradient stays ink-suppressed — upstream's BG1 is opaque everywhere,
// so a silhouette of it is vacuous), hands the resulting mask to the
// sink, and clears the ink plane again before the caller's fg passes.
// Unarmed (device, FOH, every non-check build) the SEAMS contribute
// nothing: no inking, no observation, no recording. That identity claim
// covers the seams only — U1 also, deliberately, changed the gradient
// itself (grad8 rounds where a cast truncated, matching how canvas
// composites), which shifts the packed RGB565 value on 24 device rows of
// shipped output, so device evidence pinning background pixels must be
// re-taken (review-u1 r4/r5). The gradient is judged separately, by
// colour, and OBSERVATIONALLY: gfx_bg_grad_sink fires once the gradient
// rows are down (framebuffer holds them and nothing else) and the row
// loop records the 8-bit colour it handed the rasterizer, which the
// judge cross-checks against that framebuffer. Judging it by calling the
// colour function directly was the rejected design — it stayed green
// when the row loop was deleted (review-u1 r1 H2).
//
// Randomness is the render-LOCAL stream class (own mulberry32, never the
// seeded chain); all state below is reset by gfx_bg_reset so x2 replays
// stay byte-identical. The browser reference capture mirrors that exact
// stream (same seed, same draw order) so both sides walk the starfield
// and the mountain control points identically — see port/gfx/
// gfx-pagelib.js __gfxBgInit.
//
// drawStars also animates the module-level boxFill (the stage polygon
// fill drawStage consumes) — mirrored here via gfx_bg_box_fill(), which
// gfx_render.c's stage pass reads (hsla(hue,100%,50%,0.15) for type 0;
// the drawBackgroundInit constant otherwise).
#include "gfx_vfx.h"

#include <math.h>
#include <string.h>

#include "../fdlibm/fdlibm.h"
#include "../sim/ml_js.h" // js_round (ECMAScript Math.round)
#include "../sim/ml_rng.h"

#define BG_PI 3.14159265358979323846

static MlRng g_bgrng;

typedef struct {
  double px, py, vx, vy;
  double life;
  double colR, colG, colB;
} BgStar;

static BgStar g_star[20];
static double g_sparkle;
static double g_bgPos[2][9];
static double g_dir[2][9];
static double g_circleSize[5];
static double g_ang;
static int g_boxFillType; // backgroundType at init
static RastCol g_boxFill;
static void (*g_bg2_sink)(Gfx *);   // NULL = never ink the bg (default)
static void (*g_grad_sink)(Gfx *);  // NULL = no gradient observation (default)
static void (*g_star_sink)(Gfx *);  // NULL = no star-only observation (default)

static double brand(void) { return ml_rng_next(&g_bgrng); }

static double jsign(double x) { return x > 0 ? 1 : (x < 0 ? -1 : 0); }

// hsl (s=100%, arbitrary l) -> rgb 0..255
// UNREACHABLE UPSTREAM ARMS, registered (review-u1 fallback round): the
// whole getTransparency() === false side of stagerender.js — star colour
// hsl(h,100%,15%) at :399-404, the missing globalAlpha ramp at :482-484,
// and boxFill hsl(h,100%,7%) at :554-561 — is not translated, because
// `transparency` is true at module load (transparency.js:1) and its only
// mutator is main.js:1598's jQuery #alphaButton click handler, which the
// port exposes no input path to. Not an oversight: unreachable.
//
// boxFillBG (stagerender.js:421/:556/:561) is likewise not modelled: its
// only readers are drawStage's activeStage.background.{polygon,line}
// blocks (:307/:321), and STAB1 pins `background` absent on all six VS
// stages, so it has no consumer here.
static void hsl_rgb(double h, double l, double *r, double *g, double *b) {
  h = fmod(h, 360.0);
  if (h < 0) h += 360.0;
  const double c = (1 - fabs(2 * l - 1)) * 1.0; // s = 1
  const double hp = h / 60.0;
  const double x = c * (1 - fabs(fmod(hp, 2.0) - 1));
  double rr = 0, gg = 0, bb = 0;
  if (hp < 1) { rr = c; gg = x; }
  else if (hp < 2) { rr = x; gg = c; }
  else if (hp < 3) { gg = c; bb = x; }
  else if (hp < 4) { gg = x; bb = c; }
  else if (hp < 5) { rr = x; bb = c; }
  else { rr = c; bb = x; }
  const double m = l - c / 2;
  *r = (rr + m) * 255;
  *g = (gg + m) * 255;
  *b = (bb + m) * 255;
}

static void star_velocity(BgStar *s) {
  const double vSeed = brand();
  s->vx = 5 * vSeed * jsign(0.5 - brand());
  s->vy = 5 * (1 - vSeed) * jsign(0.5 - brand());
}

void gfx_bg_reset(void) {
  ml_rng_seed(&g_bgrng, 0xBADD00D5u); // render-local; never the chain
  for (int p = 0; p < 20; p++) {
    BgStar *s = &g_star[p];
    star_velocity(s);
    hsl_rgb(358 * brand(), 0.5, &s->colR, &s->colG, &s->colB);
    s->px = 600 + 100 * brand() * s->vx;
    s->py = 375 + 100 * brand() * s->vy;
    s->life = 0;
  }
  g_sparkle = 3;
  const double bgPos0[2][9] = {
    { -30, 500, 300, 500, 900, 500, 1230, 450, 358 },
    { -30, 400, 300, 400, 900, 400, 1230, 350, 179 },
  };
  const double dir0[2][9] = {
    { 1, -1, 1, -1, 1, -1, 1, -1, 1 },
    { -1, 1, -1, 1, -1, 1, -1, 1, -1 },
  };
  memcpy(g_bgPos, bgPos0, sizeof g_bgPos);
  memcpy(g_dir, dir0, sizeof g_dir);
  for (int i = 0; i < 5; i++) g_circleSize[i] = i * 40;
  g_ang = 0;
  g_boxFillType = -1;
}

RastCol gfx_bg_box_fill(void) { return g_boxFill; }

// device-space helpers (canvas -> device retarget)
static float bx_(double cx) { return (float)(cx * GFX_K); }
static float by_(double cy) { return (float)(cy * GFX_K + GFX_DY); }

// drawBackgroundInit: bg1 linear gradient rgb(24,17,66) at canvas y 0 ->
// black at 500. A canvas linear gradient CLAMPS to its end stops outside
// [0,500] (black below 500, the first stop above 0), which is what the
// t clamp reproduces. One canvas row -> one colour.
//
// FAITHFULNESS (review-u1 r2 M1): canvas composites the interpolated
// gradient by ROUNDING to 8 bits; a C cast truncates, which biased every
// row low by up to 1. Measured against the browser read-back, rounding
// is the faithful form.
// Chrome parses hsl()/hsla() to 8 bits by ROUNDING, exactly as it
// composites a gradient, so every colour this file hands the rasterizer
// goes through here (HARD RULE 8 zoom-out: the truncating cast was a
// class, not a gradient-only instance — review-u1 fallback round).
static uint8_t col8(double v) { return (uint8_t)(v + 0.5); }

static RastCol gfx_bg_grad_col(double cy) {
  double t = cy / 500.0;
  if (t < 0) t = 0;
  if (t > 1) t = 1;
  const RastCol col = { col8(24 * (1 - t)), col8(17 * (1 - t)),
                        col8(66 * (1 - t)), 256 };
  return col;
}

// Observed 8-bit row colours, recorded BY the row loop as it runs.
#define BG_GRAD_ROWS 240
static uint8_t g_gradObs[BG_GRAD_ROWS][3];
static int g_gradObsLo, g_gradObsHi;

void gfx_bg_grad_observed(int *lo, int *hi, const uint8_t (**rows)[3]) {
  *lo = g_gradObsLo;
  *hi = g_gradObsHi;
  *rows = g_gradObs;
}

static void bg_gradient(Gfx *g) {
  // Drawn as flat rows into the fb (ink off — BG1 is opaque everywhere,
  // a silhouette of it carries no information).
  // M4 task 3 (measured-hotspot class fix): the row loop rides the -O3
  // batch primitive — exactly `for x: rast_blend_px(x, y, col, 256)`,
  // bit-identical (raster.c note).
  // Bookkeeping ONLY when a sink is armed (review-u1 r5): a shipped build
  // leaves g_grad_sink NULL and pays nothing but the hoisted branch.
  const int observe = (g_grad_sink != NULL);
  if (observe) {
    g_gradObsLo = g->rz.clipY0;
    g_gradObsHi = g->rz.clipY1;
  }
  for (int y = g->rz.clipY0; y < g->rz.clipY1; y++) {
    const RastCol col = gfx_bg_grad_col(((double)y - GFX_DY) / GFX_K);
    if (observe && y >= 0 && y < BG_GRAD_ROWS) {
      g_gradObs[y][0] = col.r;
      g_gradObs[y][1] = col.g;
      g_gradObs[y][2] = col.b;
    }
    rast_fill_row_opaque(&g->rz, y, col);
  }
}

static void draw_stars(Gfx *g) {
  g_sparkle--;
  for (int p = 0; p < 20; p++) {
    BgStar *s = &g_star[p];
    if (s->px > 1250 || s->py > 800 || s->px < -50 || s->py < -50) {
      s->px = 600;
      s->py = 375;
      s->life = 0;
      star_velocity(s);
    }
    s->px += s->vx;
    s->py += s->vy;
    s->life++;
    if (g_sparkle == 0) {
      double a = s->life / 300.0;
      if (a > 1) a = 1;
      RastCol col = { col8(s->colR), col8(s->colG), col8(s->colB), 0 };
      col.a256 = (uint16_t)(a * 256);
      rast_circle(&g->rz, bx_(s->px), by_(s->py), (float)(5 * GFX_K), col);
    }
  }
  if (g_sparkle == 0) g_sparkle = 2;
  // STAR-ONLY OBSERVATION (review-u1 r4). Fired here, between the star
  // circles and the mountains and WITHOUT clearing, so the sink sees
  // exactly the starfield while the later bg2 sink still sees the whole
  // plane. This replaced a row-range argument ("mountains cannot reach
  // above canvas y=350"): stars also draw BELOW that line, so judging
  // them only in the mountain-free rows left a renderer that clips just
  // the lower stars passing. Measuring the star plane directly needs no
  // such argument at all.
  if (g_star_sink != NULL) g_star_sink(g);
  // the two mountain silhouettes (k = 1 then k = 0; boxFill keeps k=0's)
  for (int k = 1; k > -1; k--) {
    for (int j = 0; j < 9; j++) {
      if (j == 8) g_bgPos[k][j] += g_dir[k][j] * 0.2 * brand();
      else g_bgPos[k][j] += g_dir[k][j] * 1 * brand();
      double lo = 0, hi = 0;
      int bounded = 1;
      switch (j) {
        case 0: lo = -200; hi = -10; break;
        case 1: case 3: case 5: lo = 450 - k * 100; hi = 550 - k * 100; break;
        case 2: lo = 0; hi = 550; break;
        case 4: lo = 600; hi = 1150; break;
        case 6: lo = 1210; hi = 1400; break;
        case 7: lo = 450 - k * 100; hi = 550 - k * 100; break;
        case 8: lo = 1; hi = 357; break;
        default: bounded = 0; break;
      }
      if (bounded) {
        if ((g_dir[k][j] == 1 && g_bgPos[k][j] > hi) ||
            (g_dir[k][j] == -1 && g_bgPos[k][j] < lo)) {
          g_dir[k][j] *= -1;
        }
      }
    }
    double r, gg, b;
    hsl_rgb(g_bgPos[k][8], 0.5, &r, &gg, &b);
    RastCol fill = { col8(r), col8(gg), col8(b),
                     (uint16_t)((0.15 - k * 0.07) * 256) };
    g_boxFill = fill; // hsla boxFill (loop keeps the last, k=0)
    // bezier mound via the raster path (device coords)
    rast_path_reset();
    rast_sub_begin(bx_(g_bgPos[k][0]), by_(g_bgPos[k][1]));
    {
      // flatten the cubic in device space
      rast_sub_cubic(bx_(g_bgPos[k][2]), by_(g_bgPos[k][3]),
                     bx_(g_bgPos[k][4]), by_(g_bgPos[k][5]),
                     bx_(g_bgPos[k][6]), by_(g_bgPos[k][7]));
      if (k == 1) {
        rast_sub_line(bx_(g_bgPos[0][6]), by_(g_bgPos[0][7]));
        rast_sub_cubic(bx_(g_bgPos[0][4]), by_(g_bgPos[0][5]),
                       bx_(g_bgPos[0][2]), by_(g_bgPos[0][3]),
                       bx_(g_bgPos[0][0]), by_(g_bgPos[0][1]));
      } else {
        rast_sub_line(bx_(1200), by_(750));
        rast_sub_line(bx_(0), by_(750));
      }
    }
    rast_sub_close();
    rast_fill(&g->rz, fill);
  }
}

static void draw_tunnel(Gfx *g) {
  // REGISTERED DEVIATION (pre-existing, quantified by U1). Upstream's
  // strokeStyle here is the radial gradient drawBackgroundInit builds:
  // rgba(94,173,255,0) at canvas r=1 -> rgba(94,173,255,0.2) at r=800.
  // We stroke a flat solid 0.15 instead, so the tunnel does not fade in
  // from the centre the way upstream's does: 0.15 is NOT the ramp's mid
  // stop (that is ~0.1) but roughly its 75%-radius alpha, so the inner
  // tunnel is over-brightened and only the outer quarter is close.
  // ALPHA-ONLY: geometry (ray angles, ring radii, line widths) is
  // faithful, and the U1 background judge — a silhouette mask, alpha>0
  // — cannot see this, so it is recorded rather than "fixed" blind. An
  // exact fix is cheap for the 5 rings (constant radius => constant
  // gradient alpha) but needs per-pixel gradient support in the raster
  // for the 16 rays; it wants its own colour-plane judge first.
  const RastCol col = { 94, 173, 255, (uint16_t)(0.15 * 256) };
  g_ang += 0.005;
  double angB = g_ang;
  for (int i = 0; i < 16; i++) {
    // rotateVector(0, 800, angB)
    const double vx = -800 * fd_sin(angB), vy = 800 * fd_cos(angB);
    rast_stroke_seg(&g->rz, bx_(600), by_(375), bx_(600 + vx), by_(375 + vy),
                    (float)(2 * GFX_K), col);
    angB += BG_PI / 8;
  }
  for (int i = 0; i < 5; i++) {
    g_circleSize[i]++;
    if (g_circleSize[i] > 200) g_circleSize[i] = 0;
    double lw = js_round(3 * (g_circleSize[i] / 60.0)); // Math.max(1, Math.round(...))
    if (lw < 1) lw = 1;
    rast_ring(&g->rz, bx_(600), by_(375), (float)(g_circleSize[i] * 4 * GFX_K),
              (float)(lw * GFX_K), col);
  }
}

void gfx_bg_ink_sink(void (*fn)(Gfx *)) { g_bg2_sink = fn; }
void gfx_bg_grad_sink(void (*fn)(Gfx *)) { g_grad_sink = fn; }
void gfx_bg_star_sink(void (*fn)(Gfx *)) { g_star_sink = fn; }

void gfx_render_background(Gfx *g) {
  rast_ink_enable(0); // the BG1 gradient never touches any judged plane
  bg_gradient(g);
  // The caller cleared the raster immediately before this pass, so the
  // framebuffer now holds the gradient and NOTHING else. Observing it
  // here judges the whole shipped path — the row loop, the canvas->device
  // mapping, the clip range and the RGB565 write — not just the colour
  // function they call. (review-u1 r1 H2: the earlier dump called
  // gfx_bg_grad_col directly, so deleting the row loop kept it green.)
  if (g_grad_sink != NULL) g_grad_sink(g);
  if (g_boxFillType != g->backgroundType) {
    // drawBackgroundInit's boxFill baseline for this match
    g_boxFillType = g->backgroundType;
    g_boxFill = g->backgroundType == 1
                    ? (RastCol){ 94, 173, 255, (uint16_t)(0.3 * 256) }
                    : (RastCol){ 0, 0, 0, (uint16_t)(0.1 * 256) };
  }
  // The BG2 analogue. Armed only by the browser-parity check: ink the
  // starfield/tunnel pass, hand the mask to the sink, then wipe the ink
  // plane so every following pass sees exactly what it saw before.
  rast_ink_enable(g_bg2_sink != NULL || g_star_sink != NULL);
  if (g->backgroundType == 0) draw_stars(g);
  else draw_tunnel(g);
  rast_ink_enable(1);
  if (g_bg2_sink != NULL) g_bg2_sink(g);
  if (g_bg2_sink != NULL || g_star_sink != NULL) {
    memset(g->rz.ink, 0, sizeof g->rz.ink);
  }
}
