// port/gfx/gfx_bg.c — background art (M4 task 2). Structure-parallel to
// src/stages/stagerender.js drawBackgroundInit + drawBackground (holiday
// !== 1 arms): the bg1 vertical gradient, then per frame either
// drawStars() (backgroundType 0: drifting starfield + two animated
// mountain silhouettes) or drawTunnel() (type 1: ray/circle tunnel).
//
// The bg planes are NOT part of the silhouette IoU mask on either side
// (the browser reference masks fg1|fg2|UI only, and its reduced sequence
// does not run drawBackground) — the C compositor draws them with
// rast_ink_enable(0) so the device framebuffer gets the art without
// polluting the judged ink plane. Randomness is the render-LOCAL stream
// class (own mulberry32, never the seeded chain); all state below is
// reset by gfx_bg_reset so x2 replays stay byte-identical.
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

static double brand(void) { return ml_rng_next(&g_bgrng); }

static double jsign(double x) { return x > 0 ? 1 : (x < 0 ? -1 : 0); }

// hsl (s=100%, arbitrary l) -> rgb 0..255
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

static void bg_gradient(Gfx *g) {
  // drawBackgroundInit: bg1 linear gradient rgb(24,17,66) at canvas y 0 ->
  // black at 500, black below; drawn as flat rows into the fb (ink off).
  for (int y = g->rz.clipY0; y < g->rz.clipY1; y++) {
    const double cy = ((double)y - GFX_DY) / GFX_K;
    double t = cy / 500.0;
    if (t < 0) t = 0;
    if (t > 1) t = 1;
    const RastCol col = { (uint8_t)(24 * (1 - t)), (uint8_t)(17 * (1 - t)),
                          (uint8_t)(66 * (1 - t)), 256 };
    for (int x = 0; x < RAST_W; x++) rast_blend_px(&g->rz, x, y, col, 256);
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
      RastCol col = { (uint8_t)s->colR, (uint8_t)s->colG, (uint8_t)s->colB, 0 };
      col.a256 = (uint16_t)(a * 256);
      rast_circle(&g->rz, bx_(s->px), by_(s->py), (float)(5 * GFX_K), col);
    }
  }
  if (g_sparkle == 0) g_sparkle = 2;
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
    RastCol fill = { (uint8_t)r, (uint8_t)gg, (uint8_t)b,
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
  // strokeStyle is a radial gradient rgba(94,173,255, 0 -> 0.2);
  // representative solid at the mid stop.
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

void gfx_render_background(Gfx *g) {
  rast_ink_enable(0); // bg planes never touch the judged ink plane
  bg_gradient(g);
  if (g_boxFillType != g->backgroundType) {
    // drawBackgroundInit's boxFill baseline for this match
    g_boxFillType = g->backgroundType;
    g_boxFill = g->backgroundType == 1
                    ? (RastCol){ 94, 173, 255, (uint16_t)(0.3 * 256) }
                    : (RastCol){ 0, 0, 0, (uint16_t)(0.1 * 256) };
  }
  if (g->backgroundType == 0) draw_stars(g);
  else draw_tunnel(g);
  rast_ink_enable(1);
}
