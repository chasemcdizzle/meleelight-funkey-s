// port/gfx/gfx_glyphs.h — the VFXGLYPHS1 glyph atlas, on the RASTER plane.
//
// Split out of gfx_overlay.c by A14 (second half). The atlas is the browser's
// OWN Arial rasterization, dumped at capture time (per-glyph fill+stroke alpha
// masks at device scale + advances, plus the Ready/Go! composite sprites),
// committed as port/gfx/vfxglyphs-frozen.txt. Nothing here changed in the
// move: the parser, the pool, the strict grammar and every loud failure are
// the bytes gfx_overlay.c carried.
//
// WHY IT IS ITS OWN TU (measured, not taste). The FOH menus now set their
// display text from this atlas (A14), so foh_font.o references it — and
// foh_font.o is linked by nineteen menu checks that have no data-plane
// dependency at all. gfx_overlay.c cannot be that dependency: it includes
// gfx_vfx.h -> gfx.h -> sim/sim/sim.h -> ml_stages.h, i.e. the GENERATED
// pipeline headers (`cc -Iport/sim port/gfx/gfx_overlay.c` fails with
// "'ml_stages.h' file not found"). Linking the HUD into check-legibility.sh
// would have made a menu-legibility check depend on `node pipeline/run.js`.
// This header depends on raster.h and nothing else.
//
// gfx_overlay.c keeps the Gfx-shaped surface (gfx_glyph_text,
// gfx_sprite_blit) as one-line delegations, so no HUD caller moved.
#ifndef GFX_GFX_GLYPHS_H
#define GFX_GFX_GLYPHS_H

#include "raster.h"

// The atlas's font specs, in the upstream draw forms they were captured from
// (renderOverlay / start.js). Fonts 0 and 3 additionally carry the FOH's menu
// character set (A14, first half); fonts 1 and 2 are digits-only.
typedef enum {
  GFX_FONT_T40 = 0, // "900 40px Arial", strokeText lineWidth 2 (timer MM:SS)
  GFX_FONT_T25 = 1, // "900 25px Arial", lineWidth 2 (timer centiseconds)
  GFX_FONT_P53 = 2, // "900 53px Arial" under scale(0.8,1), lw 2 (percents)
  GFX_FONT_C70 = 3, // "italic 700 70px Arial", lw 10 (Ready countdown)
  GFX_FONT_COUNT = 4
} GfxFontId;

// Load the VFXGLYPHS1 artifact at `path`. Strict grammar, loud on anything
// unexpected. Callers that already know their artifact path (foh_dev,
// gfx_app, gfx_replay) keep calling this exactly as before.
void gfx_glyphs_load(const char *glyphsPath);

// Total advance of `s` in device px at native size (multiply by the caller's
// integer upscale). Missing glyph is FATAL — never a silent skip.
double gfx_glyph_text_width(int fontId, const char *s);

// Rows from the baseline to the top of a flat capital, measured from the
// atlas itself via 'H' (a cap with neither round overshoot nor a padded
// bounding box — font 3's 'M' is declared 19 rows tall with SEVEN blank rows
// on top, so max(-dy) over the font is NOT a usable ascent). Callers that
// think in top-of-box coordinates add this to get the pen baseline.
int gfx_glyph_cap_ascent(int fontId);

// Draw `s` with its pen (x = left edge of the first glyph's advance box,
// y = baseline) at (penX, penY), fill and stroke composited at the atlas's
// own ANTIALIASED coverage. strokeFirst mirrors upstream call order
// (timer/percents: fill then stroke; countdown: stroke then fill). This is
// the HUD's draw and is byte-for-byte what gfx_overlay.c did before the
// split.
void gfx_glyph_text_rz(Raster *rz, int fontId, const char *s, double penX,
                       double penY, RastCol fill, RastCol stroke,
                       int strokeFirst);

// The FOH menus' draw: ONE colour, an integer nearest-neighbour upscale of
// the glyph mask (`up`, 1 = native), and BINARY coverage — a mask sample is
// either fully painted or not painted at all.
//
// The binary part is not a shortcut, it is what keeps the menus PROVABLE.
// Every FOH witness reads text off a rendered frame by OVERDRAWING it: draw
// the claimed string, in the claimed colour, at the claimed spot, and require
// zero changed pixels. That instrument is exactly as strong as compositing
// being idempotent, and source-over at partial alpha is NOT — painting the
// same antialiased glyph twice darkens its fringe. Drawn antialiased, every
// one of those assertions fails for a frame that is in fact correct
// (measured: check-controls-labels reports 388-474 px of "difference" on
// labels it is looking straight at). At binary coverage the property holds
// exactly, and the menus keep the crisp look the two hand-authored faces
// had on a 240x240 panel.
//
// The HUD is untouched by this: it keeps its antialiasing, because nothing
// overdraws the HUD.
void gfx_glyph_text_menu(Raster *rz, int fontId, const char *s, double penX,
                         double penY, RastCol col, int up);

// Blit a composite banner sprite ("ready" / "go") with its top-left at
// device (x0, y0) — the sprite's own dx/dy are already folded in by the
// caller that knows the canvas anchor.
void gfx_sprite_blit_rz(Raster *rz, const char *name, double anchorDevX,
                        double anchorDevY);

#endif
