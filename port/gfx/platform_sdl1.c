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
// CLAUDE.md §Commands "Device access"): u/d/l/r, a/b/x/y, s, m/n, q.
// (L is 'm', NOT 'k' — measured off /dev/input/event0 on 2026-08-24;
// fix_plan A25b + A3, provenance in platform_keymap.h.)
// Polled via SDL_GetKeyState after pumping the event queue.
#include <SDL.h> // via sdl-config --cflags include path
#include <dlfcn.h>
#include <stdio.h>
#include <string.h>

#include "platform.h"
#include "platform_keymap.h"
#include "raster.h"

// Audio seam (M3 task 6): the SDL implementation is shared with the
// SDL2 backend — see the header for the config/underrun/ABI notes.
#include "platform_audio_sdl.h"

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
  // FunKey letter keysyms (measured mapping — CLAUDE.md "Device
  // access"). Keymap SSOT (iter 97, review-95 M-b): the translation
  // arm consumes platform_keymap.h's SINGLE definition site — the
  // same compiled table foh_dev --dump-keymap emits (cmp'd against
  // the frozen keymap-frozen.txt by check-device-foh.sh). SDL1.2
  // letter keysyms are their ASCII codes (asserted at compile time).
  _Static_assert(SDLK_a == 'a' && SDLK_z == 'z',
                 "SDL1.2 letter keysyms must equal ASCII");
  // The arm itself lives in the header (A25b) so it can be RUN on a host
  // with no SDL at all — see platform_keymap_translate's note. SDL's Uint8
  // IS unsigned char, so this is the same array type, not a reinterpret.
  platform_keymap_translate(in, (const unsigned char *)k);
}

void platform_quit(void) {
  if (g_screen) {
    SDL_Quit();
    g_screen = 0;
  }
}

// --- OS artwork loader (platform.h; punch-list A12c) ----------------------
//
// SDL_image is opened with dlopen instead of linked. Linking it would make
// libSDL_image-1.2.so.0 a hard NEEDED entry, so a device that does not ship
// it could not START THE GAME AT ALL — a catastrophic failure mode traded
// for a background image. dlopen degrades to "no artwork", which is exactly
// the donor's own contract (ssb64 fk_menu.c: a NULL IMG_Load falls back to
// a dimmed frame). The soname list is the one FunKey OS ships (SDL_image
// 1.2 for SDL 1.2), plus the unversioned dev symlink for host/SDK runs.
static SDL_Surface *(*g_imgLoad)(const char *) = 0;
static int g_imgTried;

static void img_open(void) {
  if (g_imgTried) return;
  g_imgTried = 1;
  static const char *const kSonames[] = {"libSDL_image-1.2.so.0",
                                         "libSDL_image-1.2.so",
                                         "libSDL_image.so"};
  for (size_t i = 0; i < sizeof kSonames / sizeof kSonames[0]; i++) {
    void *h = dlopen(kSonames[i], RTLD_LAZY | RTLD_LOCAL);
    if (!h) continue;
    // POSIX says convert via an object pointer; the cast through a union
    // avoids the ISO C function/object-pointer conversion warning that
    // -Wall -Wextra -Werror would otherwise reject.
    union {
      void *obj;
      SDL_Surface *(*fn)(const char *);
    } u;
    u.obj = dlsym(h, "IMG_Load");
    if (u.obj) {
      g_imgLoad = u.fn;
      return; // handle deliberately leaked: process-lifetime, freed at exit
    }
    dlclose(h);
  }
}

int platform_image_load565(const char *path, uint16_t *out, uint8_t *opaque) {
  if (!path || !out || !opaque) return 0;
  img_open();
  if (!g_imgLoad) return 0;
  SDL_Surface *raw = g_imgLoad(path);
  if (!raw) return 0;
  if (raw->w != RAST_W || raw->h != RAST_H) {
    SDL_FreeSurface(raw);
    return 0;
  }
  // Normalise to a known 32-bit layout so the reader below needs no format
  // cases: SDL does the conversion, including any palette and colour key.
  SDL_Surface *s = SDL_CreateRGBSurface(SDL_SWSURFACE, RAST_W, RAST_H, 32,
                                        0x00FF0000u, 0x0000FF00u,
                                        0x000000FFu, 0xFF000000u);
  if (!s) {
    SDL_FreeSurface(raw);
    return 0;
  }
  // Clear to fully transparent BEFORE the blit (review-mexit-r2 Medium). A
  // colour-keyed source SKIPS its keyed pixels, so whatever the destination
  // held shows through the `a >= 128` opacity test below. SDL 1.2 happens to
  // zero a fresh surface, but that is an implementation detail of one libSDL
  // and this reads it as data — state it instead of inheriting it.
  if (SDL_FillRect(s, 0, 0) != 0) {
    SDL_FreeSurface(raw);
    SDL_FreeSurface(s);
    return 0;
  }
  SDL_SetAlpha(raw, 0, 255); // copy the source alpha verbatim, never blend
  const int rc = SDL_BlitSurface(raw, 0, s, 0);
  SDL_FreeSurface(raw);
  if (rc != 0) {
    SDL_FreeSurface(s);
    return 0;
  }
  if (SDL_MUSTLOCK(s) && SDL_LockSurface(s) != 0) {
    SDL_FreeSurface(s);
    return 0;
  }
  for (int y = 0; y < RAST_H; y++) {
    const uint8_t *row = (const uint8_t *)s->pixels + (size_t)y * s->pitch;
    for (int x = 0; x < RAST_W; x++) {
      uint32_t px;
      memcpy(&px, row + (size_t)x * 4u, 4u);
      const unsigned a = (px >> 24) & 0xFFu;
      const unsigned r = (px >> 16) & 0xFFu;
      const unsigned g = (px >> 8) & 0xFFu;
      const unsigned b = px & 0xFFu;
      const size_t k = (size_t)y * RAST_W + (size_t)x;
      // Half-transparent pixels are treated as opaque: this port has no
      // alpha blend on the overlay path, and the OS asset is a hard-edged
      // colour-keyed panel, so there is nothing to blend.
      opaque[k] = (a >= 128u) ? 1u : 0u;
      out[k] = (uint16_t)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
    }
  }
  if (SDL_MUSTLOCK(s)) SDL_UnlockSurface(s);
  SDL_FreeSurface(s);
  return 1;
}
