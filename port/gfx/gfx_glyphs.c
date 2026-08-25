// port/gfx/gfx_glyphs.c — the VFXGLYPHS1 glyph atlas (see gfx_glyphs.h for
// why this is its own TU). Moved out of gfx_overlay.c by A14 second half;
// the parser, the pool, the strict grammar and every loud failure below are
// gfx_overlay.c's bytes unchanged. What is NEW here is only the upscale
// (`up`) argument, the cap-ascent accessor and the lazy-load fallback.

#include "gfx_glyphs.h"

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

// --- lazy load ---------------------------------------------------------------
//
// The HUD's callers (foh_dev, gfx_app, gfx_replay) know their artifact path
// and call gfx_glyphs_load explicitly; nothing about them changed. A14 added
// a SECOND kind of caller — nineteen menu witnesses that link foh_font.o and
// have never had a data dir to be told about. Rather than add an init call to
// each, resolve the artifact the same way foh_render.c's art_load resolves
// menu.img1, with the same discipline:
//
// AN EXPLICIT OVERRIDE IS AUTHORITATIVE — but only MLFK_GLYPHS is one. If it
// is set, that file is the ONLY candidate and a failed open is fatal: falling
// through after it would draw DIFFERENT glyphs than the operator asked for,
// and two targets rendering different text is exactly the twin divergence the
// fail-loud rule exists to prevent.
//
// MLFK_DATA_DIR is NOT that. It names a data directory — the menu witnesses
// point it at their own build dir, which holds menu.img1 and has never held
// this artifact — so a miss there means "not here", not "the operator asked
// for this and it is broken", and the search continues. (Measured: treating
// it as authoritative failed check-controls-labels, check-css-back and
// check-hand on their own data dirs.) Only MLFK_GLYPHS is a promise about
// THIS file, and only MLFK_GLYPHS is enforced as one.
//
// This is a fallback for the PATH, never for the CONTENT: a named artifact
// that will not parse still dies inside gfx_glyphs_load, and a glyph that is
// not in the atlas is still fatal at draw time (HARD RULE 2 — a silent blank
// would have hidden exactly the D8 bug this atlas exists to fix).

static char g_glyph_msg[640];

static void glyph_die(const char *why, const char *path) {
  snprintf(g_glyph_msg, sizeof g_glyph_msg,
           "glyphs: %s (%s) — vfxglyphs-frozen.txt is a committed artifact; "
           "set MLFK_GLYPHS to it, or MLFK_DATA_DIR to a directory holding "
           "it, or run from the repo root",
           why, path);
  gfx_fatal(g_glyph_msg);
}

static int try_load(const char *path) {
  FILE *probe = fopen(path, "r");
  if (!probe) return 0;
  fclose(probe);
  gfx_glyphs_load(path); // loud on any grammar/coverage problem
  return 1;
}

static void ensure_loaded(void) {
  if (g_glyphs_loaded) return;
  const char *env = getenv("MLFK_GLYPHS");
  if (env != NULL && *env != '\0') {
    if (!try_load(env)) glyph_die("MLFK_GLYPHS unreadable", env);
    return;
  }
  const char *dd = getenv("MLFK_DATA_DIR");
  if (dd != NULL && *dd != '\0') {
    char buf[512];
    const int n = snprintf(buf, sizeof buf, "%s/vfxglyphs-frozen.txt", dd);
    // A truncated path names a DIFFERENT file; never open it.
    if (n < 0 || n >= (int)sizeof buf)
      glyph_die("MLFK_DATA_DIR path too long (no open attempted)", dd);
    if (try_load(buf)) return; // a miss here is "not here", not a failure
  }
  // DELIBERATELY NOT the device mount points art_load searches. On device
  // every caller passes --glyphs explicitly, so a mount-point fallback here
  // could only ever RESCUE a forgotten --glyphs — turning a loud "text before
  // gfx_glyphs_load" (the pre-A14 behaviour of this guard) into a silent
  // success from a file nobody named. The source-tree path below cannot do
  // that: it does not exist on device, so a forgotten --glyphs still dies.
  static const char *const cand = "port/gfx/vfxglyphs-frozen.txt";
  if (try_load(cand)) return;
  glyph_die("artifact not found (set MLFK_GLYPHS or MLFK_DATA_DIR)", cand);
}

// --- blitters ----------------------------------------------------------------

// Nearest-neighbour integer upscale of one alpha mask. up == 1 never reaches
// here, so the HUD path blits the frozen bytes exactly as before — a menu
// caller asking for scale 5 is the only thing that pays for this.
//
// 128x128 is measured headroom, not a guess: the largest FOH draw is font 0
// (max glyph 9x8) at scale 5, i.e. 45x40. The bound is checked, not assumed.
#define UPMAX 128
static uint8_t g_up[UPMAX * UPMAX];

// `binary` collapses every sample to 0 or 255 at the halfway point, which is
// what makes overdrawing idempotent for the FOH witnesses (gfx_glyphs.h).
static const uint8_t *upscale(const uint8_t *mask, int w, int h, int up,
                              int binary) {
  if (w * up > UPMAX || h * up > UPMAX) gfx_fatal("glyphs: upscale overflow");
  for (int y = 0; y < h * up; y++) {
    const uint8_t *src = &mask[(size_t)(y / up) * (size_t)w];
    uint8_t *dst = &g_up[(size_t)y * (size_t)(w * up)];
    for (int x = 0; x < w * up; x++) {
      const uint8_t a = src[x / up];
      dst[x] = binary ? (uint8_t)(a >= 128 ? 255 : 0) : a;
    }
  }
  return g_up;
}

static void blit_mask_rz(Raster *rz, const uint8_t *mask, int w, int h, int x0,
                         int y0, RastCol col, int up, int binary) {
  if (!mask || col.a256 == 0) return;
  // M4 task 3 (measured-hotspot class fix): the pixel loop rides the
  // -O3 batch primitive — arithmetic exactly the old per-pixel
  // rast_blend_px calls, bit-identical (raster.c note). The HUD takes the
  // straight-through arm, so its bytes did not move in the A14 split.
  if (up == 1 && !binary) {
    rast_blit_a8mask(rz, mask, w, h, x0, y0, col);
    return;
  }
  rast_blit_a8mask(rz, upscale(mask, w, h, up, binary), w * up, h * up, x0, y0,
                   col);
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
  ensure_loaded();
  double w = 0;
  for (; *s; s++) w += glyph_get(fontId, *s)->advance;
  return w;
}

int gfx_glyph_cap_ascent(int fontId) {
  ensure_loaded();
  // 'H': flat-topped, no round overshoot, and — unlike font 3's 'M', whose
  // declared box is 19 rows with seven of them blank — no bbox padding.
  // Measured: font 0 -> 6, font 3 -> 11.
  return (int)lround(-glyph_get(fontId, 'H')->dy);
}

static void draw(Raster *rz, int fontId, const char *s, double penX,
                 double penY, RastCol fill, RastCol stroke, int strokeFirst,
                 int up, int binary) {
  ensure_loaded();
  if (up < 1) gfx_fatal("glyphs: upscale below 1");
  double x = penX;
  for (; *s; s++) {
    const Glyph *gl = glyph_get(fontId, *s);
    if (gl->w * gl->h > 0) {
      const int gx = (int)lround(x + gl->dx * up);
      const int gy = (int)lround(penY + gl->dy * up);
      if (strokeFirst) {
        blit_mask_rz(rz, gl->smask, gl->w, gl->h, gx, gy, stroke, up, binary);
        blit_mask_rz(rz, gl->fmask, gl->w, gl->h, gx, gy, fill, up, binary);
      } else {
        blit_mask_rz(rz, gl->fmask, gl->w, gl->h, gx, gy, fill, up, binary);
        blit_mask_rz(rz, gl->smask, gl->w, gl->h, gx, gy, stroke, up, binary);
      }
    }
    x += gl->advance * up;
  }
}

void gfx_glyph_text_rz(Raster *rz, int fontId, const char *s, double penX,
                       double penY, RastCol fill, RastCol stroke,
                       int strokeFirst) {
  draw(rz, fontId, s, penX, penY, fill, stroke, strokeFirst, 1, 0);
}

void gfx_glyph_text_menu(Raster *rz, int fontId, const char *s, double penX,
                         double penY, RastCol col, int up) {
  const RastCol nostroke = {0, 0, 0, 0};
  draw(rz, fontId, s, penX, penY, col, nostroke, 0, up, 1);
}

void gfx_sprite_blit_rz(Raster *rz, const char *name, double anchorDevX,
                        double anchorDevY) {
  ensure_loaded();
  const Sprite *sp = strcmp(name, "ready") == 0 ? &g_ready
                     : strcmp(name, "go") == 0  ? &g_go
                                                : 0;
  if (!sp || !sp->present) gfx_fatal("glyphs: unknown sprite blit");
  const int x0 = (int)lround(anchorDevX + sp->dx);
  const int y0 = (int)lround(anchorDevY + sp->dy);
  // M4 task 3 (measured-hotspot class fix): -O3 batch primitive,
  // bit-identical to the old per-pixel loop (raster.c note).
  rast_blit_rgba(rz, sp->rgba, sp->w, sp->h, x0, y0);
}
