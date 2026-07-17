// port/gfx/platform.h — THE thin platform seam (M3 task 4; CLAUDE.md
// "SDL / platform seam" verbatim, lifted from ssb64's port/gfx pattern —
// docs/research/funkey-envelope.md §3).
//
// Exactly ONE backend TU is linked per target:
//   platform_headless.c — the loop/CI backend (no display, no SDL; what
//                         autonomous host replays link)
//   platform_sdl2.c     — host dev window (SDL2)
//   platform_sdl1.c     — the FunKey-S device (SDL 1.2, 240x240x16,
//                         SetVideoMode fallback chain, SDL_Flip)
//
// The renderer knows NOTHING about SDL: it composites into Raster.fb
// (RGB565, RAST_W x RAST_H) and a backend PRESENTS that buffer. Input
// flows the other way through PlatformInput — the M3 task-5 S1 layer
// consumes this struct at the pollInputs seam; this task only pumps it.
//
// FunKey buttons arrive as SDL letter keysyms (measured, CLAUDE.md
// §Commands "Device access"): u/d/l/r d-pad, a/b/x/y face, s START,
// k/n L/R shoulders, q MENU. The SDL2 host backend maps the same letters
// (plus arrow keys OR'd onto the d-pad) so one table serves both.
#ifndef GFX_PLATFORM_H
#define GFX_PLATFORM_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
  // d-pad (device keysyms u/d/l/r)
  bool up, down, left, right;
  // face buttons (a/b/x/y)
  bool a, b, x, y;
  // START (s), shoulders L (k) / R (n), MENU (q)
  bool start, l, r, menu;
  // backend asks the app to quit (window close / SDL_QUIT)
  bool quit;
} PlatformInput;

// Init the display (240x240, 16bpp on device). Returns 0 on success;
// nonzero = loud failure (the caller must bail, never limp on).
int platform_init(const char *title);

// Present one RAST_W x RAST_H RGB565 frame. Returns 0 iff the frame
// verifiably reached the backend (SDL_Flip / render-copy rc propagated);
// nonzero = the present FAILED (iter 52, review-50 M2: the app counts
// failures and the check gates on 0 — a silently no-op flip must never
// pass while nothing reaches the screen).
int platform_present(const uint16_t *fb565);

// Pump events + read current input state into *in (all-false when the
// backend has no input source).
void platform_poll(PlatformInput *in);

void platform_quit(void);

#endif // GFX_PLATFORM_H
