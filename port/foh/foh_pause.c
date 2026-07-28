// port/foh/foh_pause.c — in-match pause overlay (punch-list A11/A12).
// Contract, faithfulness argument and provenance: foh_pause.h.

#include "foh_pause.h"

#include <string.h>

#include "../gfx/attrib.h" // attrib_mono_ns(): the ONE shared CLOCK_MONOTONIC
#include "../gfx/pace.h"   // pace_sleep_until_ns(): the shared frame pacer
#include "../gfx/platform.h"
#include "foh.h" // foh_text / foh_text2 (foh_font.c) — reused, never copied

FohPauseResult (*foh_pause_hook)(Raster *rz, uint64_t *pausedNs,
                                 long *presentFails) = 0;

#define PAUSE_BUDGET_NS 16666667ull

enum { OPT_RESUME = 0, OPT_MENU, OPT_FRONTEND, OPT_COUNT };

static const char *const kOptName[OPT_COUNT] = {
    "RESUME", "QUIT TO MENU", "QUIT TO OS"};

// Panel geometry (240x240; RAST_W/RAST_H).
#define PANEL_X0 26
#define PANEL_X1 214
#define PANEL_Y0 54
#define PANEL_Y1 186
#define ROW0_Y 96
#define ROW_DY 24

static const RastCol kTitle = {255, 200, 60, 256};
static const RastCol kItem = {210, 210, 225, 256};
static const RastCol kItemSel = {255, 240, 200, 256};
static const RastCol kHint = {130, 130, 155, 256};

static inline uint16_t pack565(int r, int g, int b) {
  return (uint16_t)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
}

static void fb_rect(uint16_t *fb, int x0, int y0, int x1, int y1, uint16_t c) {
  if (x0 < 0) x0 = 0;
  if (y0 < 0) y0 = 0;
  if (x1 > RAST_W) x1 = RAST_W;
  if (y1 > RAST_H) y1 = RAST_H;
  for (int y = y0; y < y1; y++)
    for (int x = x0; x < x1; x++) fb[y * RAST_W + x] = c;
}

static void text2_center(Raster *rz, int y, int scale, const char *s,
                         RastCol col) {
  const int w = foh_text2_width(s, scale);
  foh_text2(rz, (RAST_W - w) / 2, y, scale, 0, s, col);
}

FohPauseResult foh_pause_open(Raster *rz, uint64_t *pausedNs,
                              long *presentFails) {
  const uint64_t t0 = attrib_mono_ns();

  // The frozen frame, dimmed ONCE (the fk_menu.c pattern: >>1 & 0x7BEF
  // halves every 565 channel). Static, not stack: 115 KB.
  static uint16_t backdrop[RAST_W * RAST_H];
  for (int i = 0; i < RAST_W * RAST_H; i++)
    backdrop[i] = (uint16_t)((rz->fb[i] >> 1) & 0x7BEF);

  // The match renderer leaves its own clip band set; text must not be
  // scissored by it. Restored on the way out.
  const int clipY0 = rz->clipY0, clipY1 = rz->clipY1;
  rz->clipY0 = 0;
  rz->clipY1 = RAST_H;

  const uint16_t cPanel = pack565(18, 18, 34);
  const uint16_t cEdge = pack565(90, 160, 255);
  const uint16_t cSel = pack565(52, 62, 110);

  // Seed the edge detector from the CURRENT state so the button that
  // opened this menu (still held) cannot also act inside it.
  PlatformInput prev;
  platform_poll(&prev);
  // SDL_QUIT is ONE-SHOT: platform_poll memsets the struct and only sets
  // .quit for events drained by THAT call (platform_sdl1.c:117-121). The
  // seed poll above can therefore swallow a pending quit, so latch it and
  // carry it forward instead of dropping it on the floor.
  int sawQuit = prev.quit ? 1 : 0;

  int sel = OPT_RESUME;
  int presentDead = 0; // latched: once the display is proven dead, count it
                       // ONCE and stop presenting (never abandon the loop
                       // state machine, and never re-count the same fault).
  FohPauseResult out = FOH_PAUSE_RESUME;
  for (;;) {
    const uint64_t deadline = attrib_mono_ns() + PAUSE_BUDGET_NS;

    PlatformInput in;
    platform_poll(&in);
#define EDGE(f) (in.f && !prev.f)
    if (in.quit) sawQuit = 1;
    int done = 0;
    if (sawQuit) { // backend asked us to go (no window manager on device)
      out = FOH_PAUSE_QUIT_FRONTEND;
      done = 1;
    } else if (EDGE(up)) {
      sel = (sel + OPT_COUNT - 1) % OPT_COUNT;
    } else if (EDGE(down)) {
      sel = (sel + 1) % OPT_COUNT;
    } else if (EDGE(a)) {
      out = (sel == OPT_MENU)       ? FOH_PAUSE_QUIT_MENU
            : (sel == OPT_FRONTEND) ? FOH_PAUSE_QUIT_FRONTEND
                                    : FOH_PAUSE_RESUME;
      done = 1;
    } else if (EDGE(b) || EDGE(start) || EDGE(menu)) {
      // B cancels; START/MENU toggle back out (they opened it).
      out = FOH_PAUSE_RESUME;
      done = 1;
    }
#undef EDGE
    prev = in;
    if (done) break;

    memcpy(rz->fb, backdrop, sizeof backdrop);
    fb_rect(rz->fb, PANEL_X0, PANEL_Y0, PANEL_X1, PANEL_Y1, cEdge);
    fb_rect(rz->fb, PANEL_X0 + 2, PANEL_Y0 + 2, PANEL_X1 - 2, PANEL_Y1 - 2,
            cPanel);
    fb_rect(rz->fb, PANEL_X0 + 6, ROW0_Y + sel * ROW_DY - 4, PANEL_X1 - 6,
            ROW0_Y + sel * ROW_DY + 20, cSel);

    text2_center(rz, PANEL_Y0 + 12, 2, "PAUSED", kTitle);
    for (int i = 0; i < OPT_COUNT; i++)
      text2_center(rz, ROW0_Y + i * ROW_DY, 2, kOptName[i],
                   i == sel ? kItemSel : kItem);
    foh_text(rz, PANEL_X0 + 12, PANEL_Y1 - 18, 1, "A SELECT   B RESUME",
             kHint);

    if (platform_present(rz->fb) != 0) {
      // A dead display cannot be navigated. Count it into the caller's
      // `failed presents` total (the number the device checks gate on) and
      // hand control back rather than trapping the player in an invisible
      // menu — the match loop's own presents keep counting from there.
      (*presentFails)++;
      presentDead = 1;
      out = FOH_PAUSE_RESUME;
      break;
    }
    pace_sleep_until_ns(deadline);
  }

  // RESUME only: do not hand a STILL-HELD dismiss button to gameplay. The
  // caller's next poll goes straight into the S1 row, so a held A/B would
  // swing an attack on the resume frame and a held START would hit target
  // mode's endGame arm. Keep presenting the frozen frame until release.
  // Unbounded on purpose: a finger that never lifts is indistinguishable
  // from staying paused, and *pausedNs keeps the pace epoch honest for it.
  if (out == FOH_PAUSE_RESUME) {
    // presentDead is carried in from the loop above: a failed present must
    // not ABANDON the drain (returning with the button still held is the
    // exact bug this loop exists to prevent), and one dead display must not
    // be counted twice.
    for (;;) {
      const uint64_t deadline = attrib_mono_ns() + PAUSE_BUDGET_NS;
      PlatformInput in;
      platform_poll(&in);
      if (in.quit) { // the other legitimate way out of a held button
        out = FOH_PAUSE_QUIT_FRONTEND;
        break;
      }
      if (!in.a && !in.b && !in.start && !in.menu) break;
      if (!presentDead && platform_present(rz->fb) != 0) {
        (*presentFails)++;
        presentDead = 1; // stop presenting, keep draining
      }
      pace_sleep_until_ns(deadline);
    }
  }

  rz->clipY0 = clipY0;
  rz->clipY1 = clipY1;
  *pausedNs = attrib_mono_ns() - t0;
  return out;
}
