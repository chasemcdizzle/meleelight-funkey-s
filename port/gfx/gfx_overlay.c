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
//
// The atlas itself now lives in gfx_glyphs.c on the RASTER plane (A14 second
// half; gfx_glyphs.h says why it had to move out of this TU). What stays here
// is the Gfx-shaped surface every HUD caller already uses, as one-line
// delegations — an API extraction, not a translation layer.

void gfx_glyph_text(Gfx *g, int fontId, const char *s, double penX,
                    double penY, RastCol fill, RastCol stroke,
                    int strokeFirst) {
  gfx_glyph_text_rz(&g->rz, fontId, s, penX, penY, fill, stroke, strokeFirst);
}

void gfx_sprite_blit(Gfx *g, const char *name, double anchorCanvasX,
                     double anchorCanvasY) {
  gfx_sprite_blit_rz(&g->rz, name, anchorCanvasX * GFX_K,
                     anchorCanvasY * GFX_K + GFX_DY);
}

// --- renderOverlay -----------------------------------------------------------

typedef struct { int slot, stockIdx, timer; } LostStock;
#define LSQ_CAP 32
static LostStock g_lsq[LSQ_CAP];
static int g_lsn;
static int g_prevStocks[4];
static int g_prevValid;

// Finish-frame permission for the timer's domain guard (gfx_vfx.h). Default 0
// — every evidence, target and menu path leaves it there, so the guard is at
// its strictest everywhere except the one frame the driver names, and
// gfx_overlay_reset() below puts it back.
static int g_allowTimerExpiry;
void gfx_overlay_allow_timer_expiry(int on) { g_allowTimerExpiry = on ? 1 : 0; }

void gfx_overlay_reset(void) {
  g_lsn = 0;
  g_prevValid = 0;
  // The finish-frame permission is per-frame state and must never outlive the
  // match it was granted in (review-r3-r4 Medium). The driver brackets it
  // around one render; clearing it here as well means a path that leaves a
  // match without passing that bracket still cannot hand permission to the
  // next one — and the next one may be a TARGET match, whose clock counts up
  // and where any negative must trap.
  gfx_overlay_allow_timer_expiry(0);
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
    // EXPIRY CLAMP — a MEASURED shipping crash, found by the R3 successor rig
    // the first time anything ever ran a VS match to its natural clock end
    // (fix_plan R3; `foh_dev --bridge live --match-timer 2`, which died with
    // `glyphs: font 0 has no glyph '-'` / `SIM FATAL frame 210: glyphs:
    // missing glyph`). It is NOT a demo artefact: upstream's real clock is
    // 480 s and the same frame arrives in any full match, so this aborted the
    // game at every natural VS timeout on the device.
    //
    // THE MECHANISM. matchTimerTick decrements first and tests after
    // (main.js:339/347), so the frame that fires finishGame carries a
    // matchTimer that has just gone NEGATIVE — measured -0.00004 at
    // --match-timer 2. `Math.floor(mt/60)` is then -1 and `(mt%60).toFixed(2)`
    // is "-0.00", so BOTH halves of the timer string acquire a '-'. The T40 /
    // T25 atlases are digits and ':' only (they are Arial cut-outs of exactly
    // the glyphs the HUD draws), so gfx_glyph_text hits gfx_overlay's own
    // FATAL missing-glyph path — the iter-103 banner-font class, second
    // instance, in the one arm no committed leg reached.
    //
    // WHY CLAMPING IS THE FAITHFUL ANSWER, and not "add a '-' glyph".
    // UPSTREAM NEVER DRAWS THIS STRING. Its render is requestAnimationFrame-
    // driven and decoupled from gameTick, and the VS arm is gated
    // `else if (playing || frameByFrameRender)` (main.js:1243) while
    // finishGame sets `playing = false` (main.js:1423). So the frame whose
    // matchTimer went negative is never rendered at all upstream: the last
    // overlay it painted carried a NON-NEGATIVE timer, and finishGame's
    // banner goes over that. This port renders synchronously inside the tick
    // loop, so it paints one overlay frame upstream skips; the fix is to keep
    // that frame's text inside the domain upstream actually draws, not to
    // teach the atlas a glyph upstream's HUD never shows.
    //
    // BOUNDED, AND IT MASKS NOTHING (review-r3-r1 High). A blanket
    // `mt < 0 -> 0` would also turn a NaN or a wildly negative clock — real
    // corruption — into a legitimate-looking 00:00, which is precisely the
    // silent-pass shape this project keeps re-registering. So the rescue is
    // exactly as wide as the measured case and no wider:
    //
    //   * the ONLY legitimate out-of-domain value is the single finish frame's,
    //     at most ONE tick below zero (0.016667), because sim_tick.c calls the
    //     finish hook the moment the decrement crosses zero and this port's
    //     loop leaves the match on that same frame. That one is clamped.
    //   * anything else — NaN, or more than one tick negative — is a state the
    //     sim cannot produce, so it dies LOUDLY and says what it saw.
    //
    // Dying there is not a new risk, and that is the point: BEFORE this fix
    // every one of those values already killed the process, just via
    // gfx_glyph_text's missing-'-' abort with a message that named a font
    // instead of a clock. This trades a confusing death for an accurate one
    // and rescues only the case upstream itself never renders.
    //
    // Off the finish frame the expression is byte-unchanged, and no evidence
    // or golden run can reach it either way — with a NULL finish hook the sim
    // traps on expiry instead (sim_tick.c), so every committed trace-fed leg
    // keeps a strictly positive clock. The target compositor shares this
    // function and its clock counts UP from zero, so it never comes near here.
    //
    // REGISTERED ALTERNATIVE, deliberately not taken here (owner call): the
    // deeper mirror of upstream would be to skip gfx_render_frame entirely on
    // the finish frame, so the banner composites over the previous frame the
    // way an rAF render gated by `playing` does. That means inverting
    // foh_dev.c's `!g_vsFinish` skip exception, which is a RATIFIED port-level
    // decision (review-mexit-r3 M5, measured) covered by the match-exit lane's
    // GO verdicts, and it moves `matchSkips` accounting that committed rigs
    // parse. It is a behavioural change needing its own evidence and sign-off,
    // not a drive-by inside a rig increment — recorded in the writer's report.
    //
    // OFF THE CHECKSUM SURFACE: the HUD is render-only (oracle/CHECKSUM.md §2
    // is an exhaustive allow-list and carries no overlay field), so this
    // cannot move a stream. `bash port/sim/check-sim.sh` is the proof.
    const double raw = st->matchTimer;
    // THE DOMAIN GUARD. Two separate things can put a value here that this
    // function cannot draw, and both are rejected before anything is
    // formatted (review-r3-r2 Medium):
    //
    //   * NON-FINITE. `(int)floor(inf/60)` is undefined behaviour outright,
    //     and `%.2f` of an infinity or a NaN emits "inf"/"nan" — letters the
    //     digit-only atlas has never carried, i.e. the SAME missing-glyph
    //     abort with an even less informative message.
    //   * MORE THAN ONE TICK BELOW ZERO. The width is ML_MATCH_TIMER_TICK,
    //     read from sim.h so it is the SAME symbol matchTimerTick subtracts —
    //     a second hand-typed copy of an engine constant is exactly the drift
    //     HARD RULE 5 forbids.
    //
    // Spelled as a NEGATED `>=` so NaN is caught by the same test: every
    // ordinary comparison against a NaN is false, so `raw < -TICK` would let
    // it straight through.
    //
    // AND IT IS CONTEXT-BOUND, not merely value-bound (review-r3-r3 Medium).
    // A negative clock is legitimate on exactly ONE frame of ONE caller — the
    // VS finish frame — so the driver grants permission for that frame and
    // nothing else. The TARGET compositor calls this same function with a
    // clock that counts UP from zero, and there a negative is corruption; with
    // permission off it aborts instead of quietly rendering 00:00.
    if (!isfinite(raw) ||
        (raw < 0.0 &&
         (!g_allowTimerExpiry || !(raw >= -ML_MATCH_TIMER_TICK)))) {
      gfx_fatal("matchTimer out of domain for the HUD (non-finite, or negative "
                "outside the one-tick finish-frame window the sim can produce)");
    }
    const double mt = raw > 0.0 ? raw : 0.0;
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
