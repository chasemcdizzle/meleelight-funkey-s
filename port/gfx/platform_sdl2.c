// port/gfx/platform_sdl2.c — the host dev-window backend (M3 task 4).
//
// SDL2, 240x240 logical (2x window for eyeballs), RGB565 streaming
// texture. This TU exists so a developer can WATCH a replay on the host;
// the loop/CI never links it (headless does that) and the device links
// platform_sdl1.c. Input maps the SAME FunKey letter keysyms as the
// device backend, with the arrow keys OR'd onto the d-pad so a desktop
// keyboard is usable — one chord table (M3 task 5) will serve both.
#include <SDL.h> // via sdl2-config --cflags include path
#include <stdio.h>
#include <string.h>

#include "platform.h"
#include "raster.h"

// Audio seam (M3 task 6): SDL2's legacy SDL_OpenAudio API matches
// SDL 1.2's — one shared implementation header serves both backends.
#include "platform_audio_sdl.h"

static SDL_Window *g_win;
static SDL_Renderer *g_ren;
static SDL_Texture *g_tex;

int platform_init(const char *title) {
  if (SDL_Init(SDL_INIT_VIDEO) != 0) {
    fprintf(stderr, "platform_sdl2: SDL_Init failed: %s\n", SDL_GetError());
    return 1;
  }
  g_win = SDL_CreateWindow(title ? title : "meleelight",
                           SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                           RAST_W * 2, RAST_H * 2, 0);
  if (!g_win) {
    fprintf(stderr, "platform_sdl2: CreateWindow failed: %s\n", SDL_GetError());
    SDL_Quit();
    return 1;
  }
  g_ren = SDL_CreateRenderer(g_win, -1, SDL_RENDERER_PRESENTVSYNC);
  if (!g_ren) g_ren = SDL_CreateRenderer(g_win, -1, 0);
  if (!g_ren) {
    fprintf(stderr, "platform_sdl2: CreateRenderer failed: %s\n", SDL_GetError());
    SDL_Quit();
    return 1;
  }
  g_tex = SDL_CreateTexture(g_ren, SDL_PIXELFORMAT_RGB565,
                            SDL_TEXTUREACCESS_STREAMING, RAST_W, RAST_H);
  if (!g_tex) {
    fprintf(stderr, "platform_sdl2: CreateTexture failed: %s\n", SDL_GetError());
    SDL_Quit();
    return 1;
  }
  return 0;
}

int platform_present(const uint16_t *fb565) {
  // rc propagated (iter 52, review-50 M2): a failed texture update /
  // copy is a FAILED present (RenderPresent itself returns void).
  int rc = 0;
  if (SDL_UpdateTexture(g_tex, 0, fb565, RAST_W * 2) != 0) rc = 1;
  if (SDL_RenderClear(g_ren) != 0) rc = 1;
  if (SDL_RenderCopy(g_ren, g_tex, 0, 0) != 0) rc = 1;
  SDL_RenderPresent(g_ren);
  return rc;
}

void platform_poll(PlatformInput *in) {
  memset(in, 0, sizeof *in);
  SDL_Event e;
  while (SDL_PollEvent(&e)) {
    if (e.type == SDL_QUIT) in->quit = true;
  }
  const Uint8 *k = SDL_GetKeyboardState(0);
  // FunKey letter keysyms + desktop arrows OR'd in
  in->up = k[SDL_SCANCODE_U] || k[SDL_SCANCODE_UP];
  in->down = k[SDL_SCANCODE_D] || k[SDL_SCANCODE_DOWN];
  in->left = k[SDL_SCANCODE_L] || k[SDL_SCANCODE_LEFT];
  in->right = k[SDL_SCANCODE_R] || k[SDL_SCANCODE_RIGHT];
  in->a = k[SDL_SCANCODE_A] != 0;
  in->b = k[SDL_SCANCODE_B] != 0;
  in->x = k[SDL_SCANCODE_X] != 0;
  in->y = k[SDL_SCANCODE_Y] != 0;
  in->start = k[SDL_SCANCODE_S] != 0;
  // L is M, not K (A25b: the device's L shoulder emits KEY_M — measured
  // 2026-08-24). This backend is a SECOND, scancode-indexed copy of the
  // mapping and cannot consume platform_keymap.h's ASCII-keysym table, so
  // it is kept in step by hand and named as a duplicate here rather than
  // silently: SDL2 scancodes are a different index space from SDL1.2's
  // ASCII keysyms, so there is no shared array to point both at.
  in->l = k[SDL_SCANCODE_M] != 0;
  in->r = k[SDL_SCANCODE_N] != 0;
  in->menu = k[SDL_SCANCODE_Q] != 0;
}

void platform_quit(void) {
  if (g_tex) SDL_DestroyTexture(g_tex);
  if (g_ren) SDL_DestroyRenderer(g_ren);
  if (g_win) SDL_DestroyWindow(g_win);
  if (g_win) SDL_Quit();
  g_tex = 0;
  g_ren = 0;
  g_win = 0;
}

// A12c artwork loader (platform.h): there is no OS artwork on a host run —
// /usr/games/menu_resources/ is the FunKey OS's own directory, so only the
// device backend has any business reading it. (The "links no SDL" reason
// belongs to the headless backend and was copy-pasted here; this backend
// obviously links SDL2.) The caller's documented fallback — a dimmed
// snapshot of the live frame — is what host/CI runs therefore always render.
int platform_image_load565(const char *path, uint16_t *out, uint8_t *opaque) {
  (void)path;
  (void)out;
  (void)opaque;
  return 0;
}
