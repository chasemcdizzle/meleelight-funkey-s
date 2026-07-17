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

// --- audio (M3 task 6) --------------------------------------------------------
//
// The measured spike config (docs/research/audio-spike.md verdict /
// PLAN §7): 44100 Hz, AUDIO_S16LSB, 2 channels — rate and channel count
// are FIXED by design; only the buffer size is a parameter (512 is the
// smallest clean buffer under load; 1024 is the sanctioned fallback;
// smaller values exist for the underrun-tooth negative test only).
//
// fill(ud, out, frames) renders `frames` interleaved stereo S16 sample
// frames. It runs on the BACKEND's audio callback thread (SDL) — the
// main thread must bracket every mixer-state mutation with
// platform_audio_lock()/platform_audio_unlock().
//
// Backends:
//   SDL1 (device) / SDL2 (host dev): SDL_OpenAudio with the exact spec;
//     any renegotiation (obtained != requested) is a LOUD start failure,
//     never a silent resample. Stats count callbacks, underruns (the
//     spike's late200 proxy: inter-callback gap > 2x the nominal period,
//     CLOCK_MONOTONIC measured inside the callback — the measured
//     audible-dropout class) and badlen (callback len != the granted
//     byte size — the ABI tripwire; SDL_AudioSpec verified against the
//     SDK sysroot SDL_audio.h this build compiles against: int freq,
//     Uint16 format, Uint8 channels+silence, Uint16 samples+padding,
//     Uint32 size, callback fnptr, void* userdata — no time_t fields;
//     the audio spike already exercised this exact struct against the
//     device libSDL, spike logs 01-10).
//   headless: ACCEPT-AND-IDLE — start succeeds, no callback thread ever
//     runs, lock/unlock are no-ops, stats report zeros and a granted
//     spec of 0/0/0 (never a faked 44100/512/2). This keeps the app's
//     main-thread event-scheduling path (the mixer's voice bookkeeping)
//     running deterministically on host truth legs; the check pins the
//     granted-spec fields per leg, so a headless run can never
//     masquerade as a device audio run.
typedef void (*PlatformAudioFill)(void *ud, int16_t *stereoOut, int frames);

typedef struct {
  uint64_t cbs;       // callbacks fired
  uint64_t underruns; // late200: gap > 2x nominal callback period
  uint64_t badlen;    // callback len != granted spec size (ABI tripwire)
  int rate;           // granted spec (0 on headless)
  int samples;
  int channels;
} PlatformAudioStats;

// Open + start the audio device (samples = requested buffer size in
// sample frames). Returns 0 on success; nonzero = loud failure (caller
// bails — audio must never silently run degraded).
int platform_audio_start(PlatformAudioFill fill, void *ud, int samples);
void platform_audio_stop(void);
void platform_audio_lock(void);
void platform_audio_unlock(void);
void platform_audio_stats(PlatformAudioStats *out);

#endif // GFX_PLATFORM_H
