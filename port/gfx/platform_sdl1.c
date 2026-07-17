// port/gfx/platform_sdl1.c — the FunKey-S device backend (M3 task 4).
//
// SDL 1.2, 240x240x16. LICENSING (CLAUDE.md hard rule / LICENSING.md):
// SDL 1.2 is LGPL — DYNAMIC linking only; the build asserts the binary
// is dynamically linked against libSDL-1.2.so.0, never a static libSDL.
//
// SetVideoMode FALLBACK CHAIN (CLAUDE.md gotcha notes + the measured
// ssb64 pattern, docs/research/funkey-envelope.md §1): the FunKey driver
// may not honor the optimistic flags —
//   HWSURFACE|DOUBLEBUF -> SWSURFACE|DOUBLEBUF -> SWSURFACE -> 0
// and BitsPerPixel MUST be 16 after init or we bail LOUDLY (the present
// blit assumes 16bpp; a silent 32bpp surface would render garbage).
// SDL_ShowCursor off; SDL_Flip presents; lock/unlock via SDL_MUSTLOCK.
//
// Raster.fb is already RGB565 — the device surface format — so present
// is a straight row-copy (pitch-aware); ssb64's per-pixel LUT conversion
// disappears entirely (PLAN §5 PROVISIONAL, confirmed here at init:
// we additionally verify the surface masks ARE 565 and bail if not).
//
// Input: FunKey firmware delivers buttons as LETTER keysyms (measured;
// CLAUDE.md §Commands "Device access"): u/d/l/r, a/b/x/y, s, k/n, q.
// Polled via SDL_GetKeyState after pumping the event queue.
#include <SDL.h> // via sdl-config --cflags include path
#include <stdio.h>
#include <string.h>

#include "platform.h"
#include "raster.h"

static SDL_Surface *g_screen;

int platform_init(const char *title) {
  if (SDL_Init(SDL_INIT_VIDEO) != 0) {
    fprintf(stderr, "platform_sdl1: SDL_Init failed: %s\n", SDL_GetError());
    return 1;
  }
  SDL_WM_SetCaption(title ? title : "meleelight", 0);
  static const Uint32 kChain[] = {
      SDL_HWSURFACE | SDL_DOUBLEBUF,
      SDL_SWSURFACE | SDL_DOUBLEBUF,
      SDL_SWSURFACE,
      0,
  };
  g_screen = 0;
  for (unsigned i = 0; i < sizeof kChain / sizeof *kChain; i++) {
    g_screen = SDL_SetVideoMode(RAST_W, RAST_H, 16, kChain[i]);
    if (g_screen) {
      fprintf(stderr, "platform_sdl1: SetVideoMode ok (chain step %u, flags 0x%x)\n",
              i, (unsigned)g_screen->flags);
      break;
    }
  }
  if (!g_screen) {
    fprintf(stderr, "platform_sdl1: SDL_SetVideoMode failed on the whole fallback chain: %s\n",
            SDL_GetError());
    SDL_Quit();
    return 1;
  }
  if (g_screen->format->BitsPerPixel != 16) {
    fprintf(stderr, "platform_sdl1: surface is %d bpp, need 16 — bailing\n",
            (int)g_screen->format->BitsPerPixel);
    SDL_Quit();
    return 1;
  }
  // present() memcpys RGB565 rows — verify the surface really is 565
  // (PLAN §5: render directly in the surface format; if a device ever
  // hands back 16bpp-but-not-565, that is a loud stop, not a LUT).
  if (g_screen->format->Rmask != 0xF800u || g_screen->format->Gmask != 0x07E0u ||
      g_screen->format->Bmask != 0x001Fu) {
    fprintf(stderr, "platform_sdl1: 16bpp surface masks are not RGB565 (R=%04x G=%04x B=%04x) — bailing\n",
            (unsigned)g_screen->format->Rmask, (unsigned)g_screen->format->Gmask,
            (unsigned)g_screen->format->Bmask);
    SDL_Quit();
    return 1;
  }
  SDL_ShowCursor(SDL_DISABLE);
  return 0;
}

// MEASURED DEVICE BASELINE (iter 52 evidence probe,
// .loop/m3-task4r52-probe-sdlflip.log): the FunKey kernel fb driver
// rejects FBIOPAN_DISPLAY, so the device's patched libSDL-1.2 returns
// -1 from EVERY SDL_Flip with exactly this error string — while the
// present demonstrably runs (the flip's rotation blit costs ~1.0-1.5 ms
// per frame, identical to the iter-50 known-good run; gmenu2x rides the
// same driver). Whitelist-grammar posture (PROCESS §3, applied to a C
// API): accept rc 0 OR exactly this pinned benign signature; ANY other
// flip failure is a REAL failed present the app counts and the check
// gates on. Physical-panel truth stays with the M3 human gate (render
// is non-checksummed by design). Changing this pin is a reviewed change.
static const char kBenignFlipErr[] = "ioctl(FBIOPAN_DISPLAY) failed";

int platform_present(const uint16_t *fb565) {
  if (SDL_MUSTLOCK(g_screen)) {
    if (SDL_LockSurface(g_screen) != 0) return 1; // failed present (counted by the app)
  }
  const int pitch = g_screen->pitch;
  uint8_t *dst = (uint8_t *)g_screen->pixels;
  for (int y = 0; y < RAST_H; y++) {
    memcpy(dst + (size_t)y * pitch, fb565 + (size_t)y * RAST_W,
           (size_t)RAST_W * 2);
  }
  if (SDL_MUSTLOCK(g_screen)) SDL_UnlockSurface(g_screen);
  // SDL_Flip rc propagated (iter 52, review-50 M2): a failing flip is
  // COUNTED unless it matches the pinned measured-benign signature.
  if (SDL_Flip(g_screen) == 0) return 0;
  const char *err = SDL_GetError();
  return (err && strcmp(err, kBenignFlipErr) == 0) ? 0 : 1;
}

void platform_poll(PlatformInput *in) {
  memset(in, 0, sizeof *in);
  SDL_Event e;
  while (SDL_PollEvent(&e)) {
    if (e.type == SDL_QUIT) in->quit = true;
  }
  const Uint8 *k = SDL_GetKeyState(0);
  // FunKey letter keysyms (measured mapping — CLAUDE.md "Device access")
  in->up = k[SDLK_u] != 0;
  in->down = k[SDLK_d] != 0;
  in->left = k[SDLK_l] != 0;
  in->right = k[SDLK_r] != 0;
  in->a = k[SDLK_a] != 0;
  in->b = k[SDLK_b] != 0;
  in->x = k[SDLK_x] != 0;
  in->y = k[SDLK_y] != 0;
  in->start = k[SDLK_s] != 0;
  in->l = k[SDLK_k] != 0;
  in->r = k[SDLK_n] != 0;
  in->menu = k[SDLK_q] != 0;
}

void platform_quit(void) {
  if (g_screen) {
    SDL_Quit();
    g_screen = 0;
  }
}
