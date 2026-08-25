// port/foh/foh_font.c — self-authored 5x7 bitmap font for the REWRITTEN
// front-of-house screens (fix_plan §M4 task 9; pre-registration AGENT-LOG
// iter 88). The FOH is a rewrite, not a transliteration (§M4 conventions:
// upstream menus are jQuery+DOM hybrids — no browser-rasterized text is
// faithful to carry), and menus are NOT checksummed; visual authority is
// Chase's acceptance playthrough. Every glyph below was authored by hand
// for this project — NO third-party font bytes (no NOTICES entry needed).
//
// Format: 7 rows per glyph, 5 bits per row, bit 4 = leftmost pixel.
// Coverage: A-Z 0-9 space and the punctuation the FOH screens use.
// Unknown characters are a LOUD failure (gfx_fatal), never a silent blank
// (HARD RULE 2 — a missing glyph is a bug, not a fallback).
//
// A14 (second half) moved foh_text2 off the SECOND hand-authored face in
// this file — the 6x9 display face — and onto the browser's own Arial, via
// the VFXGLYPHS1 atlas. The 5x7 face above did not move; see the note at
// foh_text2 for why the swap stopped where it did.
#include "foh.h"

#include <math.h>

#include "../gfx/gfx_glyphs.h"

typedef struct {
  char ch;
  uint8_t rows[7];
} FohGlyph;

static const FohGlyph kGlyphs[] = {
    {'A', {0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11}},
    {'B', {0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E}},
    {'C', {0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E}},
    {'D', {0x1C, 0x12, 0x11, 0x11, 0x11, 0x12, 0x1C}},
    {'E', {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F}},
    {'F', {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10}},
    {'G', {0x0E, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0F}},
    {'H', {0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11}},
    {'I', {0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E}},
    {'J', {0x07, 0x02, 0x02, 0x02, 0x02, 0x12, 0x0C}},
    {'K', {0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11}},
    {'L', {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F}},
    {'M', {0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11}},
    {'N', {0x11, 0x11, 0x19, 0x15, 0x13, 0x11, 0x11}},
    {'O', {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E}},
    {'P', {0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10}},
    {'Q', {0x0E, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0D}},
    {'R', {0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11}},
    {'S', {0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E}},
    {'T', {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04}},
    {'U', {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E}},
    {'V', {0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04}},
    {'W', {0x11, 0x11, 0x11, 0x15, 0x15, 0x15, 0x0A}},
    {'X', {0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11}},
    {'Y', {0x11, 0x11, 0x0A, 0x04, 0x04, 0x04, 0x04}},
    {'Z', {0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F}},
    {'0', {0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E}},
    {'1', {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E}},
    {'2', {0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F}},
    {'3', {0x1F, 0x02, 0x04, 0x02, 0x01, 0x11, 0x0E}},
    {'4', {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02}},
    {'5', {0x1F, 0x10, 0x1E, 0x01, 0x01, 0x11, 0x0E}},
    {'6', {0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E}},
    {'7', {0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08}},
    {'8', {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E}},
    {'9', {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x0C}},
    {' ', {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {'-', {0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00}},
    {'+', {0x00, 0x04, 0x04, 0x1F, 0x04, 0x04, 0x00}}, // iter 99 (tss)
    {'.', {0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C}},
    {':', {0x00, 0x0C, 0x0C, 0x00, 0x0C, 0x0C, 0x00}},
    {'/', {0x01, 0x01, 0x02, 0x04, 0x08, 0x10, 0x10}},
    {'\'', {0x0C, 0x04, 0x08, 0x00, 0x00, 0x00, 0x00}},
    {'>', {0x08, 0x04, 0x02, 0x01, 0x02, 0x04, 0x08}},
    {'<', {0x02, 0x04, 0x08, 0x10, 0x08, 0x04, 0x02}},
    {'!', {0x04, 0x04, 0x04, 0x04, 0x04, 0x00, 0x04}},
    {',', {0x00, 0x00, 0x00, 0x00, 0x0C, 0x04, 0x08}},
    {'(', {0x02, 0x04, 0x08, 0x08, 0x08, 0x04, 0x02}},
    {')', {0x08, 0x04, 0x02, 0x02, 0x02, 0x04, 0x08}},
    // A7 (credits): two of the fourteen authored blurbs contain '&'
    // (credits.js:117-118 and :125-126), and those strings are OTHER
    // PEOPLE'S ATTRIBUTION — substituting "AND" would edit a credit, and
    // face 2 is too wide to set a 67-character blurb inside upstream's
    // information bar at 240 px. See the note below for why this is not the
    // widening that note forbids.
    {'&', {0x0C, 0x12, 0x12, 0x0C, 0x1A, 0x12, 0x0D}},
};
// NOTE (A1 restyle): face 1's coverage is deliberately NOT widened for
// convenience. The finish-banner tooth in check-foh-flows.sh proves the
// missing-glyph gfx_fatal by rendering "COMPLETE?" through this face —
// adding '?' here would silently defuse it, so '?' stays out and menu
// strings that need it render in face 2 below, which has its own guard.
// A7 added '&' above and that does NOT touch the tooth: it is a different
// character, and "COMPLETE?" still dies here.


// --- FACE 2, RETAINED (A14 second half) -----------------------------------
//
// The 6x9 display face below is NO LONGER what foh_text2 draws — the atlas
// swap at the bottom of this file took that over. It is kept, at the
// driver's explicit instruction, because deleting it is a SEPARATE step that
// only happens once the swap is green everywhere, and because one committed
// instrument still reads this table AS DATA: check-live-arms.sh computes the
// system overlay's expected "VOLUME" bitmap by parsing kGlyphs2[] out of
// this file. That decoder now expects a face the renderer no longer draws
// and is an OWED device-leg rewrite; the table stays until it lands, so that
// rewrite has the bytes it is replacing in front of it.
//
// The two entry points are renamed foh_text2_face2 / foh_text2_face2_width
// so nothing can reach the retired face by habit: the live names mean the
// atlas, and only the atlas.
//
// WHAT DOES NOT CHANGE IS THE CONTRACT. A character this face lacks is a
// gfx_fatal, never a placeholder box, and the same is true of the atlas path
// that replaced it (gfx_glyphs.c's glyph_get). A silent fallback on either
// side would have hidden exactly the D8 bug the atlas widening exists to fix
// — the loud guard is precisely the part that survives the swap intact.
// --- FACE 2: the heavy 6x9 display face (A1 restyle Phase 0) --------------
// Upstream's menus set "700 35px Arial" for the bar/explanation text and
// "italic 900 48px Arial" for the mode title (menu.js:436-440). Neither is
// carryable (no browser text rasterizer, and no third-party font bytes may
// enter the tree — LICENSING) so this is a SECOND hand-authored face: 6
// columns x 9 rows, 2px stems, advance 7 at scale 1. Every glyph below was
// drawn by hand for this project, same provenance as the 5x7 face above.
//
// The ITALIC variant is not a second glyph set: it is this face sheared at
// blit time (lean = (8 - row) / 3, i.e. 0..2 px of right-lean), which is
// what "italic 900" reads as at 9 px. One data table, two looks.
//
// Format: 9 rows per glyph, 6 bits per row, bit 5 = leftmost pixel.
typedef struct {
  char ch;
  uint8_t rows[9];
} FohGlyph2;

static const FohGlyph2 kGlyphs2[] = {
    {'A', {0x0C, 0x1E, 0x33, 0x33, 0x3F, 0x33, 0x33, 0x33, 0x33}},
    {'B', {0x3E, 0x33, 0x33, 0x33, 0x3E, 0x33, 0x33, 0x33, 0x3E}},
    {'C', {0x1E, 0x33, 0x33, 0x30, 0x30, 0x30, 0x33, 0x33, 0x1E}},
    {'D', {0x3C, 0x36, 0x33, 0x33, 0x33, 0x33, 0x33, 0x36, 0x3C}},
    {'E', {0x3F, 0x30, 0x30, 0x30, 0x3C, 0x30, 0x30, 0x30, 0x3F}},
    {'F', {0x3F, 0x30, 0x30, 0x30, 0x3C, 0x30, 0x30, 0x30, 0x30}},
    {'G', {0x1E, 0x33, 0x30, 0x30, 0x37, 0x33, 0x33, 0x33, 0x1E}},
    {'H', {0x33, 0x33, 0x33, 0x33, 0x3F, 0x33, 0x33, 0x33, 0x33}},
    {'I', {0x3F, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x3F}},
    {'J', {0x0F, 0x06, 0x06, 0x06, 0x06, 0x06, 0x36, 0x36, 0x1C}},
    {'K', {0x33, 0x36, 0x3C, 0x38, 0x30, 0x38, 0x3C, 0x36, 0x33}},
    {'L', {0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x3F}},
    {'M', {0x33, 0x3F, 0x3F, 0x33, 0x33, 0x33, 0x33, 0x33, 0x33}},
    {'N', {0x33, 0x3B, 0x3B, 0x3F, 0x37, 0x37, 0x33, 0x33, 0x33}},
    {'O', {0x1E, 0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x1E}},
    {'P', {0x3E, 0x33, 0x33, 0x33, 0x3E, 0x30, 0x30, 0x30, 0x30}},
    {'Q', {0x1E, 0x33, 0x33, 0x33, 0x33, 0x33, 0x37, 0x36, 0x1D}},
    {'R', {0x3E, 0x33, 0x33, 0x33, 0x3E, 0x38, 0x34, 0x36, 0x33}},
    {'S', {0x1E, 0x33, 0x30, 0x38, 0x1E, 0x07, 0x03, 0x33, 0x1E}},
    {'T', {0x3F, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C}},
    {'U', {0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x1E}},
    {'V', {0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x1E, 0x0C}},
    {'W', {0x33, 0x33, 0x33, 0x33, 0x33, 0x3F, 0x3F, 0x3F, 0x33}},
    {'X', {0x33, 0x33, 0x1E, 0x1E, 0x0C, 0x1E, 0x1E, 0x33, 0x33}},
    {'Y', {0x33, 0x33, 0x33, 0x1E, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C}},
    {'Z', {0x3F, 0x03, 0x06, 0x0C, 0x0C, 0x18, 0x30, 0x30, 0x3F}},
    {'0', {0x1E, 0x33, 0x33, 0x37, 0x3F, 0x3B, 0x33, 0x33, 0x1E}},
    {'1', {0x0C, 0x1C, 0x3C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x3F}},
    {'2', {0x1E, 0x33, 0x03, 0x06, 0x0C, 0x18, 0x30, 0x30, 0x3F}},
    {'3', {0x3F, 0x06, 0x0C, 0x1E, 0x03, 0x03, 0x03, 0x33, 0x1E}},
    {'4', {0x06, 0x0E, 0x1E, 0x36, 0x36, 0x3F, 0x06, 0x06, 0x06}},
    {'5', {0x3F, 0x30, 0x30, 0x3E, 0x03, 0x03, 0x03, 0x33, 0x1E}},
    {'6', {0x0E, 0x18, 0x30, 0x30, 0x3E, 0x33, 0x33, 0x33, 0x1E}},
    {'7', {0x3F, 0x03, 0x06, 0x06, 0x0C, 0x0C, 0x18, 0x18, 0x18}},
    {'8', {0x1E, 0x33, 0x33, 0x1E, 0x1E, 0x33, 0x33, 0x33, 0x1E}},
    {'9', {0x1E, 0x33, 0x33, 0x33, 0x1F, 0x03, 0x03, 0x06, 0x1C}},
    {' ', {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {'.', {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C}},
    {',', {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C, 0x18}},
    {':', {0x00, 0x00, 0x0C, 0x0C, 0x00, 0x00, 0x0C, 0x0C, 0x00}},
    {'!', {0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x00, 0x0C, 0x0C}},
    {'?', {0x1E, 0x33, 0x03, 0x06, 0x0C, 0x0C, 0x00, 0x0C, 0x0C}},
    {'-', {0x00, 0x00, 0x00, 0x00, 0x3F, 0x00, 0x00, 0x00, 0x00}},
    {'+', {0x00, 0x00, 0x0C, 0x0C, 0x3F, 0x0C, 0x0C, 0x00, 0x00}},
    {'/', {0x03, 0x03, 0x06, 0x06, 0x0C, 0x18, 0x18, 0x30, 0x30}},
    {'&', {0x1C, 0x36, 0x36, 0x1C, 0x3B, 0x37, 0x36, 0x37, 0x1D}},
    {'\'', {0x0C, 0x0C, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {'(', {0x06, 0x0C, 0x18, 0x18, 0x18, 0x18, 0x18, 0x0C, 0x06}},
    {')', {0x18, 0x0C, 0x06, 0x06, 0x06, 0x06, 0x06, 0x0C, 0x18}},
    {'>', {0x18, 0x0C, 0x06, 0x03, 0x06, 0x0C, 0x18, 0x00, 0x00}},
    {'<', {0x06, 0x0C, 0x18, 0x30, 0x18, 0x0C, 0x06, 0x00, 0x00}},
};

static const FohGlyph *glyph_for(char c) {
  const int n = (int)(sizeof kGlyphs / sizeof kGlyphs[0]);
  for (int k = 0; k < n; k++) {
    if (kGlyphs[k].ch == c) return &kGlyphs[k];
  }
  gfx_fatal("foh_font: no glyph for requested character");
}

// Advance = 6 columns per glyph (5 px + 1 px gap) at scale 1.
int foh_text_width(const char *s, int scale) {
  int n = 0;
  for (const char *p = s; *p; p++) n++;
  if (n == 0) return 0;
  return (n * 6 - 1) * scale;
}

void foh_text(Raster *rz, int x, int y, int scale, const char *s,
              RastCol col) {
  int penX = x;
  for (const char *p = s; *p; p++) {
    const FohGlyph *g = glyph_for(*p);
    // B9: emit RUNS, not pixels. Adjacent set columns in a glyph row are
    // merged, so one row of a run of `n` set columns is a single
    // n*scale-wide rast_blend_px_run instead of n*scale*scale cross-TU
    // rast_blend_px calls. Every pixel is still visited exactly once, with
    // the same colour and the same alpha, so the composited bytes are
    // unchanged (partial-alpha source-over is not idempotent, which is why
    // "exactly once" is the property that matters — runs never overlap).
    for (int r = 0; r < 7; r++) {
      int c = 0;
      while (c < 5) {
        if (!(g->rows[r] & (0x10u >> c))) { c++; continue; }
        int c2 = c;
        while (c2 < 5 && (g->rows[r] & (0x10u >> c2))) c2++;
        const int xa = penX + c * scale, xb = penX + c2 * scale;
        for (int sy = 0; sy < scale; sy++) {
          rast_blend_px_run(rz, y + r * scale + sy, xa, xb, col, col.a256);
        }
        c = c2;
      }
    }
    penX += 6 * scale;
  }
}

// --- face 2 blitter ------------------------------------------------------

static const FohGlyph2 *glyph2_for(char c) {
  const int n = (int)(sizeof kGlyphs2 / sizeof kGlyphs2[0]);
  for (int k = 0; k < n; k++) {
    if (kGlyphs2[k].ch == c) return &kGlyphs2[k];
  }
  gfx_fatal("foh_font: no face-2 glyph for requested character");
}

// Advance = 7 columns per glyph (6 px + 1 px gap) at scale 1.
int foh_text2_face2_width(const char *s, int scale) {
  int n = 0;
  for (const char *p = s; *p; p++) n++;
  if (n == 0) return 0;
  return (n * 7 - 1) * scale;
}

// Italic lean, in unscaled glyph columns: 2 at the cap line down to 0 at the
// baseline. The whole string leans as one block (per-glyph shear only, no
// advance change) — the same trick a synthetic-oblique renderer uses.
void foh_text2_face2(Raster *rz, int x, int y, int scale, int italic,
                     const char *s, RastCol col) {
  int penX = x;
  for (const char *p = s; *p; p++) {
    const FohGlyph2 *g = glyph2_for(*p);
    for (int r = 0; r < 9; r++) {
      // B9 run emission — see foh_text. `lean` is constant across the row,
      // so merged columns stay contiguous under the synthetic oblique too.
      const int lean = italic ? ((8 - r) / 3) * scale : 0;
      int c = 0;
      while (c < 6) {
        if (!(g->rows[r] & (0x20u >> c))) { c++; continue; }
        int c2 = c;
        while (c2 < 6 && (g->rows[r] & (0x20u >> c2))) c2++;
        const int xa = penX + lean + c * scale;
        const int xb = penX + lean + c2 * scale;
        for (int sy = 0; sy < scale; sy++) {
          rast_blend_px_run(rz, y + r * scale + sy, xa, xb, col, col.a256);
        }
        c = c2;
      }
    }
    penX += 7 * scale;
  }
}

// --- FACE 2 IS NOW THE BROWSER'S ARIAL (A14, second half) -----------------
//
// This file used to carry a second hand-authored face — 6 columns x 9 rows,
// fixed 7 px advance, italic faked by shearing it at blit time — because the
// port has no font engine and no third-party font bytes may enter the tree
// (LICENSING). The VFXGLYPHS1 atlas removes that constraint: it is the
// BROWSER'S OWN Arial rasterization, dumped at capture time from upstream's
// own draw calls, so the menus can be set in the face upstream actually uses
// instead of an imitation of it. A14's first half widened atlas fonts 0 and
// 3 to the FOH's character set; this is the swap onto them.
//
// FONT MAPPING (fix_plan, A14 second half):
//   italic  -> font 3, "italic 700 70px Arial" — upstream's own menu weight
//   upright -> font 0, "900 40px Arial"
//
// SCALE. The atlas holds each spec at ONE size (font 0's ink cell is 9
// device px, font 3's caps stand 12), while callers ask for scale 1, 2, 3
// and 5. `scale` therefore became an integer upscale of the glyph mask, and
// the two faces take it differently — measured, not chosen:
//
//   * font 0's ink cell is EXACTLY 9 px, which is exactly what the old 6x9
//     face occupied at scale 1. up = scale is a box-for-box drop-in at every
//     upright call site; only the advances go proportional, which narrows
//     each upright string to 0.67-0.88 of its old width (the '!' warn glyph,
//     alone in being a single narrow character, goes to 0.44). Nothing
//     overflows: measured against every screen's own budget, the worst case
//     is the explanation bar at 163.5 px of 232.
//   * font 3 is 1.75x font 0 (70px vs 40px) — its caps already stand 12 px
//     where the old face stood 9. up = 1 is the only integer that fits: up =
//     2 would set the mode titles 24 px tall inside an 18 px budget, and the
//     menu bars are 22 px high. So every italic string draws at native size
//     regardless of the scale it was asked for.
//
// The consequence to judge with an eye rather than a check: italic scale 1
// (the menu bar labels) and italic scale 2 (the mode titles, the CSS header,
// READY TO FIGHT) now render at the SAME size, because font 3 has only one.
// Bar labels grow 1.20-1.44x — the widest, "TARGET BUILDER", sets 123.7 px
// against roughly 128 px of bar, the tightest fit on any screen — and the
// titles shrink to 0.61-0.71x. Chase's acceptance playthrough is the
// authority on whether that reads right. If it does not, this function is
// the ONE place the metrics move.
//
// WHY foh_text (the 5x7 face) DID NOT MOVE WITH IT, though the brief named
// both. Two committed instruments read that face AS DATA, and the atlas has
// no equivalent:
//   * check-foh-flows.sh's banner tooth renders "COMPLETE?" through it and
//     requires the missing-glyph gfx_fatal. Atlas font 0 HAS '?', so routing
//     foh_text at the atlas would have silently defused the tooth — the
//     precise shape HARD RULE 3 forbids.
//   * decode-pb-glyphs.js reads device banner shots back through kGlyphs[],
//     and check-device-foh.sh's VSF_BANNER_* pins are face-1 metrics.
// Moving face 1 is its own change, with its own evidence.

// A missing glyph stays a LOUD failure: gfx_glyph_text_menu and
// gfx_glyph_text_width abort inside the atlas rather than drawing a
// placeholder box — the same contract glyph_for above keeps.

static int face2_font(int italic) {
  return italic ? GFX_FONT_C70 : GFX_FONT_T40;
}

static int face2_up(int italic, int scale) { return italic ? 1 : scale; }

// Total advance in device px. Per-glyph now, not (n * 7 - 1) * scale — which
// is why this needs the same `italic` the draw will use (foh.h).
int foh_text2_width(const char *s, int scale, int italic) {
  const int fid = face2_font(italic);
  return (int)lround(gfx_glyph_text_width(fid, s) * face2_up(italic, scale));
}

// `y` stays what every call site already means by it: the TOP of the text
// box. The atlas draws from a baseline, so the cap ascent — read out of the
// atlas itself, never hardcoded — converts one to the other.
void foh_text2(Raster *rz, int x, int y, int scale, int italic,
               const char *s, RastCol col) {
  const int fid = face2_font(italic);
  const int up = face2_up(italic, scale);
  // One colour and BINARY coverage: the FOH does its own outlining by
  // overdrawing (text2_outlined in foh_render.c), and every FOH witness
  // reads text back off a frame by overdrawing too — which is only a proof
  // if compositing is idempotent, and source-over at partial alpha is not
  // (gfx_glyphs.h carries the measurement).
  gfx_glyph_text_menu(rz, fid, s, (double)x,
                      (double)(y + gfx_glyph_cap_ascent(fid) * up), col, up);
}
