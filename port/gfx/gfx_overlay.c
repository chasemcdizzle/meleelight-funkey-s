// port/gfx/gfx_overlay.c — HUD overlay + executed glyph atlas (M4 task 2).
//
// Structure-parallel translation of src/main/render.js renderOverlay(true)
// in the harness domain (versusMode == 0 measured -> the timer branch
// DRAWS; gameMode 3):
//   - match timer "MM:SS" (900 40px Arial, centre 590,70) + centiseconds
//     (900 25px, centre 670,70), fill white / stroke black lw2;
//   - per-player percent (900 53px under scale(0.8,1), textAlign end at
//     (450+i*145)*1.25, y 670; fill rgb(255, 255-p, 255-p), stroke black)
//     — percentShake is the CHECKSUM.md section-7 timing-dependent
//     exclusion: C draws unshaken (documented residual);
//   - stock icons (arc r12 at 337+i*145+j*30, 600; fill palettes[pPal[i]][0],
//     stroke black lw2);
//   - the lost-stock burst (8 white 4x4 rects + expanding ring, 20 frames).
//     Upstream physics.js:979 pushes into lostStockQueue at the death tick
//     (render-plane state; the C sim keeps it a documented no-op) — the C
//     renderer derives the push by watching per-slot stock decrements in
//     GameState, which is the same observable.
//
// TEXT: C has no font engine. Glyphs are the browser's OWN Arial
// rasterization, dumped at capture time (VFXGLYPHS1: per-glyph fill+stroke
// alpha masks at device scale + advances; plus the static Ready/Go!
// composite sprites) and committed as port/gfx/vfxglyphs-frozen.txt.
#include "gfx_vfx.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// --- VFXGLYPHS1 -------------------------------------------------------------

typedef struct {
  int present;
  int w, h;
  double dx, dy, advance; // device px, relative to the pen (baseline) point
  const uint8_t *fmask, *smask;
} Glyph;

typedef struct {
  int present;
  int w, h;
  double dx, dy;
  const uint8_t *rgba;
} Sprite;

static Glyph g_glyph[GFX_FONT_COUNT][128];
static Sprite g_ready, g_go;

#define GPOOL 1048576
static uint8_t g_pool[GPOOL];
static size_t g_pool_used;
static int g_glyphs_loaded;

static uint8_t *pool_take(size_t n) {
  if (g_pool_used + n > GPOOL) gfx_fatal("glyphs: pool overflow");
  uint8_t *p = &g_pool[g_pool_used];
  g_pool_used += n;
  return p;
}

static int hexv(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  gfx_fatal("glyphs: bad hex digit");
  return 0;
}

static const uint8_t *parse_hex_line(FILE *f, const char *tag, size_t want) {
  char *line = NULL;
  size_t lcap = 0;
  const ssize_t n = getline(&line, &lcap, f);
  if (n <= 0) gfx_fatal("glyphs: truncated artifact");
  const size_t taglen = strlen(tag);
  if (strncmp(line, tag, taglen) != 0 || line[taglen] != ' ') {
    fprintf(stderr, "glyphs: expected %s line, got: %.32s\n", tag, line);
    gfx_fatal("glyphs: grammar violation");
  }
  const char *s = line + taglen + 1;
  uint8_t *out = pool_take(want);
  for (size_t i = 0; i < want; i++) {
    if (s[2 * i] == 0 || s[2 * i + 1] == 0) gfx_fatal("glyphs: short hex line");
    out[i] = (uint8_t)((hexv(s[2 * i]) << 4) | hexv(s[2 * i + 1]));
  }
  const char tail = s[2 * want];
  if (tail != '\n' && tail != 0) gfx_fatal("glyphs: long hex line");
  free(line);
  return out;
}

void gfx_glyphs_load(const char *path) {
  memset(g_glyph, 0, sizeof g_glyph);
  memset(&g_ready, 0, sizeof g_ready);
  memset(&g_go, 0, sizeof g_go);
  g_pool_used = 0;
  FILE *f = fopen(path, "r");
  if (!f) gfx_fatal("glyphs: cannot open artifact");
  char *line = NULL;
  size_t lcap = 0;
  ssize_t n;
  int seenEnd = 0;
  if ((n = getline(&line, &lcap, f)) <= 0 || strcmp(line, "VFXGLYPHS1\n") != 0) {
    gfx_fatal("glyphs: bad magic");
  }
  while ((n = getline(&line, &lcap, f)) > 0) {
    if (line[n - 1] == '\n') line[--n] = 0;
    if (n == 0) continue;
    if (strcmp(line, "END") == 0) { seenEnd = 1; break; }
    if (strncmp(line, "GLYPH ", 6) == 0) {
      int fid, code, w, h;
      double dx, dy, adv;
      if (sscanf(line + 6, "%d %d %d %d %lf %lf %lf", &fid, &code, &w, &h,
                 &dx, &dy, &adv) != 7 ||
          fid < 0 || fid >= GFX_FONT_COUNT || code < 32 || code >= 128 ||
          w < 0 || h < 0 || w > 255 || h > 255) {
        gfx_fatal("glyphs: bad GLYPH line");
      }
      Glyph *gl = &g_glyph[fid][code];
      if (gl->present) gfx_fatal("glyphs: duplicate glyph");
      gl->present = 1;
      gl->w = w;
      gl->h = h;
      gl->dx = dx;
      gl->dy = dy;
      gl->advance = adv;
      if (w * h > 0) {
        gl->fmask = parse_hex_line(f, "FMASK", (size_t)(w * h));
        gl->smask = parse_hex_line(f, "SMASK", (size_t)(w * h));
      }
    } else if (strncmp(line, "SPRITE ", 7) == 0) {
      char name[16];
      int w, h;
      double dx, dy;
      if (sscanf(line + 7, "%15s %d %d %lf %lf", name, &w, &h, &dx, &dy) != 5 ||
          w <= 0 || h <= 0 || w > 400 || h > 400) {
        gfx_fatal("glyphs: bad SPRITE line");
      }
      Sprite *sp = strcmp(name, "ready") == 0 ? &g_ready
                   : strcmp(name, "go") == 0  ? &g_go
                                              : 0;
      if (!sp) gfx_fatal("glyphs: unknown sprite");
      if (sp->present) gfx_fatal("glyphs: duplicate sprite");
      sp->present = 1;
      sp->w = w;
      sp->h = h;
      sp->dx = dx;
      sp->dy = dy;
      sp->rgba = parse_hex_line(f, "RGBA", (size_t)(4 * w * h));
    } else {
      fprintf(stderr, "glyphs: unknown line: %s\n", line);
      gfx_fatal("glyphs: unknown line");
    }
  }
  free(line);
  fclose(f);
  if (!seenEnd) gfx_fatal("glyphs: missing END");
  if (!g_ready.present || !g_go.present) gfx_fatal("glyphs: missing banner sprite");
  // every font needs at least the digits
  for (int fid = 0; fid < GFX_FONT_COUNT; fid++) {
    for (char c = '0'; c <= '9'; c++) {
      if (!g_glyph[fid][(int)c].present) gfx_fatal("glyphs: digit coverage hole");
    }
  }
  g_glyphs_loaded = 1;
}

// --- blitters ----------------------------------------------------------------

static void blit_mask(Gfx *g, const uint8_t *mask, int w, int h, int x0,
                      int y0, RastCol col) {
  // M4 task 3 (measured-hotspot class fix): the pixel loop rides the
  // -O3 batch primitive — arithmetic exactly the old per-pixel
  // rast_blend_px calls, bit-identical (raster.c note).
  rast_blit_a8mask(&g->rz, mask, w, h, x0, y0, col);
}

static const Glyph *glyph_get(int fontId, char c) {
  if (fontId < 0 || fontId >= GFX_FONT_COUNT) gfx_fatal("glyphs: font id range");
  const int code = (unsigned char)c;
  if (code < 32 || code >= 128) gfx_fatal("glyphs: glyph out of ascii range");
  const Glyph *gl = &g_glyph[fontId][code];
  if (!gl->present) {
    fprintf(stderr, "glyphs: font %d has no glyph '%c'\n", fontId, c);
    gfx_fatal("glyphs: missing glyph");
  }
  return gl;
}

double gfx_glyph_text_width(int fontId, const char *s) {
  if (!g_glyphs_loaded) gfx_fatal("glyphs: text before gfx_glyphs_load");
  double w = 0;
  for (; *s; s++) w += glyph_get(fontId, *s)->advance;
  return w;
}

void gfx_glyph_text(Gfx *g, int fontId, const char *s, double penX,
                    double penY, RastCol fill, RastCol stroke,
                    int strokeFirst) {
  if (!g_glyphs_loaded) gfx_fatal("glyphs: text before gfx_glyphs_load");
  double x = penX;
  for (; *s; s++) {
    const Glyph *gl = glyph_get(fontId, *s);
    if (gl->w * gl->h > 0) {
      const int gx = (int)lround(x + gl->dx);
      const int gy = (int)lround(penY + gl->dy);
      if (strokeFirst) {
        blit_mask(g, gl->smask, gl->w, gl->h, gx, gy, stroke);
        blit_mask(g, gl->fmask, gl->w, gl->h, gx, gy, fill);
      } else {
        blit_mask(g, gl->fmask, gl->w, gl->h, gx, gy, fill);
        blit_mask(g, gl->smask, gl->w, gl->h, gx, gy, stroke);
      }
    }
    x += gl->advance;
  }
}

void gfx_sprite_blit(Gfx *g, const char *name, double anchorCanvasX,
                     double anchorCanvasY) {
  if (!g_glyphs_loaded) gfx_fatal("glyphs: sprite before gfx_glyphs_load");
  const Sprite *sp = strcmp(name, "ready") == 0 ? &g_ready
                     : strcmp(name, "go") == 0  ? &g_go
                                                : 0;
  if (!sp || !sp->present) gfx_fatal("glyphs: unknown sprite blit");
  const int x0 = (int)lround(anchorCanvasX * GFX_K + sp->dx);
  const int y0 = (int)lround(anchorCanvasY * GFX_K + GFX_DY + sp->dy);
  // M4 task 3 (measured-hotspot class fix): -O3 batch primitive,
  // bit-identical to the old per-pixel loop (raster.c note).
  rast_blit_rgba(&g->rz, sp->rgba, sp->w, sp->h, x0, y0);
}

// --- renderOverlay -----------------------------------------------------------

typedef struct { int slot, stockIdx, timer; } LostStock;
#define LSQ_CAP 32
static LostStock g_lsq[LSQ_CAP];
static int g_lsn;
static int g_prevStocks[4];
static int g_prevValid;

void gfx_overlay_reset(void) {
  g_lsn = 0;
  g_prevValid = 0;
}

static float dev_x(double cx) { return (float)(cx * GFX_K); }
static float dev_y(double cy) { return (float)(cy * GFX_K + GFX_DY); }

// renderOverlay's TIMER block (render.js:395-411; the `!versusMode ||
// gameMode == 5` arm) — extracted iter 99 (M4 task 12) so the target
// compositor can call renderOverlay(false) semantics; the VS overlay
// below calls THIS function (same bytes, behavioral identity).
void gfx_render_overlay_timer(Gfx *g, const GameState *st) {
  const RastCol black = { 0, 0, 0, 256 };
  const RastCol white = { 255, 255, 255, 256 };
  {
    const double mt = st->matchTimer;
    const int min = (int)floor(mt / 60);
    char minStr[16], secStr[16];
    snprintf(minStr, sizeof minStr, "%d", min);
    snprintf(secStr, sizeof secStr, "%.2f", fmod(mt, 60.0)); // (mt%60).toFixed(2)
    const size_t secLen = strlen(secStr);
    // 24 >= minStr's worst case (15 chars of a 16-byte %d buffer) + ":cc"
    // + NUL — sized so arm gcc 10.2 -Werror=format-truncation can PROVE
    // no truncation (M4 task 3: first arm build of the M4 overlay TU;
    // in-domain output is unchanged, minutes are 1-2 digits).
    char timerText[24];
    char c0, c1;
    if (secLen < 5) { c0 = '0'; c1 = secStr[0]; }
    else            { c0 = secStr[0]; c1 = secStr[1]; }
    if (strlen(minStr) < 2) {
      snprintf(timerText, sizeof timerText, "0%s:%c%c", minStr, c0, c1);
    } else {
      snprintf(timerText, sizeof timerText, "%s:%c%c", minStr, c0, c1);
    }
    const double w = gfx_glyph_text_width(GFX_FONT_T40, timerText);
    gfx_glyph_text(g, GFX_FONT_T40, timerText, dev_x(590) - w / 2, dev_y(70),
                   white, black, 0);
    char centis[3];
    if (secLen < 5) { centis[0] = secStr[2]; centis[1] = secStr[3]; }
    else            { centis[0] = secStr[3]; centis[1] = secStr[4]; }
    centis[2] = 0;
    const double w2 = gfx_glyph_text_width(GFX_FONT_T25, centis);
    gfx_glyph_text(g, GFX_FONT_T25, centis, dev_x(670) - w2 / 2, dev_y(70),
                   white, black, 0);
  }
}

void gfx_render_overlay(Gfx *g, const GameState *st) {
  const RastCol black = { 0, 0, 0, 256 };
  const RastCol white = { 255, 255, 255, 256 };

  // --- timer (versusMode == 0 in the harness domain -> branch draws) ------
  gfx_render_overlay_timer(g, st);

  // --- percents (textAlign "end"; ui.scale(0.8,1) is baked into the p53
  // atlas, so the effective pen x is (450+i*145)*1.25*0.8 = (450+i*145))
  for (int i = 0; i < 4; i++) {
    if (!st->sim.playerPresent[i]) continue;
    const MlPlayer *p = &st->sim.player[i];
    char txt[16];
    snprintf(txt, sizeof txt, "%d%%", (int)floor(p->percent));
    const double sub = 255 - p->percent;
    const RastCol fill = { 255, (uint8_t)(sub < 0 ? 0 : sub),
                           (uint8_t)(sub < 0 ? 0 : sub), 256 };
    const double endX = (450 + i * 145) * 1.25 * 0.8 * GFX_K;
    const double w = gfx_glyph_text_width(GFX_FONT_P53, txt);
    gfx_glyph_text(g, GFX_FONT_P53, txt, endX - w, dev_y(670), fill, black, 0);
  }

  // --- stock icons ---------------------------------------------------------
  for (int i = 0; i < 4; i++) {
    if (!st->sim.playerPresent[i]) continue;
    const GfxRgb pc = g->data.palettes[g->data.pPal[i]][0];
    const RastCol fill = { pc.r, pc.g, pc.b, 256 };
    const int stocks = (int)st->sim.player[i].stocks;
    for (int j = 0; j < stocks; j++) {
      const float cx = dev_x(337 + i * 145 + j * 30);
      const float cy = dev_y(600);
      rast_circle(&g->rz, cx, cy, (float)(12 * GFX_K), fill);
      rast_ring(&g->rz, cx, cy, (float)(12 * GFX_K), (float)(2 * GFX_K), black);
    }
  }

  // --- lost-stock burst ----------------------------------------------------
  // derive upstream's physics.js:979 push from the per-slot stock delta
  for (int i = 0; i < 4; i++) {
    const int cur = st->sim.playerPresent[i] ? (int)st->sim.player[i].stocks : -1;
    if (g_prevValid && cur >= 0 && g_prevStocks[i] >= 0 &&
        cur < g_prevStocks[i]) {
      if (g_lsn >= LSQ_CAP) gfx_fatal("gfx_overlay: lostStock overflow");
      g_lsq[g_lsn++] = (LostStock){ i, cur, 0 };
    }
    g_prevStocks[i] = cur;
  }
  g_prevValid = 1;
  int w = 0;
  for (int k = 0; k < g_lsn; k++) {
    LostStock *ls = &g_lsq[k];
    ls->timer++;
    if (ls->timer > 20) continue; // popped
    const double bx = 337 + ls->slot * 145 + ls->stockIdx * 30 - 2;
    const double by = 600 - 2;
    const double t = ls->timer;
    const double rects[8][2] = {
      { t, 0 }, { t, t }, { -t, t }, { t, -t },
      { -t, -t }, { -t, 0 }, { 0, t }, { 0, -t },
    };
    for (int r = 0; r < 8; r++) {
      const float x0 = dev_x(bx + rects[r][0]);
      const float y0 = dev_y(by + rects[r][1]);
      const float quad[8] = { x0, y0, x0 + (float)(4 * GFX_K), y0,
                              x0 + (float)(4 * GFX_K), y0 + (float)(4 * GFX_K),
                              x0, y0 + (float)(4 * GFX_K) };
      rast_poly(&g->rz, quad, 4, white);
    }
    rast_ring(&g->rz, dev_x(bx + 2), dev_y(by + 2), (float)(t / 2 * GFX_K),
              (float)(2 * GFX_K), white);
    g_lsq[w++] = *ls;
  }
  g_lsn = w;
}
