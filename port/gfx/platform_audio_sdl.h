// port/gfx/platform_audio_sdl.h — the SDL audio implementation of the
// platform.h audio seam (M3 task 6), SHARED by platform_sdl1.c (device,
// SDL 1.2) and platform_sdl2.c (host dev window, SDL2's legacy
// SDL_OpenAudio API — same functions, same callback signature, same
// SDL_AudioSpec shape). This header carries NON-static definitions and
// is included by exactly ONE backend TU per binary (the platform-seam
// rule), so the symbols never collide.
//
// Config = the measured spike verdict (docs/research/audio-spike.md /
// PLAN §7): 44100 Hz, AUDIO_S16LSB, 2 channels; buffer size is the
// caller's parameter (512 = the smallest clean buffer under 80% load).
// The obtained spec must EQUAL the request exactly — the spike measured
// every request granted verbatim on the device (log 01-probe.log); any
// renegotiation here is therefore an anomaly and a LOUD start failure,
// never a silent resample path.
//
// UNDERRUN COUNTER (the spike's proxy, measured basis): late200 =
// inter-callback gap > 2x the nominal period (512/44100 = 11.61 ms),
// CLOCK_MONOTONIC read inside the callback. A >2-period gap drains more
// than the two periods of queued audio -> audible dropout (spike §2);
// gaps below that are absorbed by the queue and NOT counted (matching
// the spike's clean/dirty classification). BOUNDARY INTERVALS (iter 59,
// review-57 M2): the gap chain is SEEDED at audio-open (the unpause
// timestamp), so the open->first-callback interval is judged by the
// same rule, and platform_audio_stop judges the terminal
// last-callback->stop interval — SDL_PauseAudio(1) FIRST (quiescing
// the callback source), then the sample under SDL_LockAudio (iter 61,
// review-59 M: no callback can START after the terminal sample) — a
// stalled callback START, a late finish, or a callback thread that
// silently died mid-run all land in `underruns` (a dead thread's
// terminal gap is open->stop).
// badlen counts callbacks whose len differs from the granted spec's
// byte size — the ABI tripwire (SDL_AudioSpec layout: int freq, Uint16
// format, Uint8 channels, Uint8 silence, Uint16 samples, Uint16
// padding, Uint32 size, callback, userdata — verified against the SDK
// sysroot SDL_audio.h; no time-bearing fields, and the audio spike
// already exercised this exact struct in-process against the device
// libSDL) — plus clock-anomaly sentinels (below).
//
// THREAD PRIORITY (measured exposure, iter 59): SDL 1.2 exposes NO
// audio-thread priority API (SDL_SetThreadPriority does not exist in
// 1.2; the callback thread runs at the platform-default scheduling
// policy). There is nothing to assert — recorded as MEASURED EXPOSURE
// (PROCESS §8) in docs/AGENT-LOG.md iter 59. Compensating controls:
// the boundary-inclusive gap accounting above and the check's standing
// 64-sample starvation probe (proves the accounting live per run).
// DMA-XRUN BLINDNESS (same exposure entry): a kernel/driver-level xrun
// that emits silence while callbacks stay timely is invisible to this
// proxy BY CONSTRUCTION; the closure path is the M4 mixer-fidelity
// seed (captured-reference audible-plane comparison, AGENT-LOG
// iter 57). "underruns == 0" reads "no callback-observable
// starvation", never "no audible dropout".
//
// QUEUE EXHAUSTION is structurally impossible in this design: there is
// no main->callback ring buffer. The main thread mutates mixer state
// directly under SDL_LockAudio()/SDL_UnlockAudio() (the SDL-sanctioned
// mutex around the callback), so no command can ever be dropped; the
// only starvation mode is the callback itself running late (counted
// above).
#ifndef GFX_PLATFORM_AUDIO_SDL_H
#define GFX_PLATFORM_AUDIO_SDL_H

#include <time.h>

// (SDL.h, platform.h, stdio.h are included by the backend TU before
// this header.)

static PlatformAudioFill g_pa_fill;
static void *g_pa_ud;
static SDL_AudioSpec g_pa_granted;
static int g_pa_open;
static uint64_t g_pa_cbs, g_pa_underruns, g_pa_badlen;
static uint64_t g_pa_last_ns;   // 0 = no previous callback yet
static uint64_t g_pa_nominal_ns;

static uint64_t pa_now_ns(void) {
  struct timespec ts;
  // clock failure inside the audio callback cannot call sim_fatal
  // (wrong thread); a zero timestamp would poison the gap math, so
  // count it as a badlen-class anomaly via a sentinel instead.
  if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) return 0;
  return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

static void pa_callback(void *ud, Uint8 *stream, int len) {
  (void)ud;
  g_pa_cbs++;
  const uint64_t now = pa_now_ns();
  if (now == 0) {
    g_pa_badlen++; // clock anomaly — impossible-by-spec, counted loud
  } else {
    if (g_pa_last_ns != 0 && now - g_pa_last_ns > 2 * g_pa_nominal_ns) {
      g_pa_underruns++;
    }
    g_pa_last_ns = now;
  }
  if ((Uint32)len != g_pa_granted.size) {
    g_pa_badlen++;
    memset(stream, g_pa_granted.silence, (size_t)(len > 0 ? len : 0));
    return;
  }
  // len == samples * channels * 2 (S16 stereo) — render sample frames
  g_pa_fill(g_pa_ud, (int16_t *)(void *)stream, (int)g_pa_granted.samples);
}

int platform_audio_start(PlatformAudioFill fill, void *ud, int samples) {
  if (g_pa_open) {
    fprintf(stderr, "platform_audio: start called while already open\n");
    return 1;
  }
  if (samples <= 0 || samples > 65535) {
    fprintf(stderr, "platform_audio: bad samples %d\n", samples);
    return 1;
  }
  if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
    fprintf(stderr, "platform_audio: SDL audio init failed: %s\n",
            SDL_GetError());
    return 1;
  }
  g_pa_fill = fill;
  g_pa_ud = ud;
  g_pa_cbs = g_pa_underruns = g_pa_badlen = 0;
  g_pa_last_ns = 0;
  SDL_AudioSpec want;
  memset(&want, 0, sizeof want);
  want.freq = 44100;
  want.format = AUDIO_S16LSB;
  want.channels = 2;
  want.samples = (Uint16)samples;
  want.callback = pa_callback;
  memset(&g_pa_granted, 0, sizeof g_pa_granted);
  if (SDL_OpenAudio(&want, &g_pa_granted) != 0) {
    fprintf(stderr, "platform_audio: SDL_OpenAudio failed: %s\n",
            SDL_GetError());
    return 1;
  }
  // The spike measured every request granted EXACTLY (log 01); any
  // renegotiation is an anomaly -> loud failure, never silent degrade.
  if (g_pa_granted.freq != 44100 || g_pa_granted.format != AUDIO_S16LSB ||
      g_pa_granted.channels != 2 || g_pa_granted.samples != (Uint16)samples ||
      g_pa_granted.size !=
          (Uint32)samples * g_pa_granted.channels * 2u) {
    fprintf(stderr,
            "platform_audio: obtained spec differs from request "
            "(freq=%d format=0x%04x ch=%d samples=%d size=%u)\n",
            g_pa_granted.freq, (unsigned)g_pa_granted.format,
            (int)g_pa_granted.channels, (int)g_pa_granted.samples,
            (unsigned)g_pa_granted.size);
    SDL_CloseAudio();
    return 1;
  }
  g_pa_nominal_ns = (uint64_t)g_pa_granted.samples * 1000000000ull /
                    (uint64_t)g_pa_granted.freq;
  // Boundary accounting (iter 59, review-57 M2): seed the gap chain at
  // the OPEN/unpause timestamp so the interval to the FIRST callback is
  // measured by the same >2-period rule (a stalled callback start is an
  // underrun, not a blind spot). No callback can run before
  // SDL_PauseAudio(0), so this write is unraced. Clock failure here is
  // the same impossible-by-spec class as in-callback: badlen sentinel,
  // and the chain falls back to first-callback baselining (last_ns 0).
  {
    const uint64_t t0 = pa_now_ns();
    if (t0 == 0) g_pa_badlen++;
    g_pa_last_ns = t0;
  }
  g_pa_open = 1;
  SDL_PauseAudio(0);
  return 0;
}

void platform_audio_stop(void) {
  if (!g_pa_open) return;
  // Terminal boundary interval (iter 59, review-57 M2; ORDERING iter
  // 61, review-59 M): SDL_PauseAudio(1) FIRST — pausing quiesces the
  // callback source, so no NEW callback can start after the terminal
  // sample below is taken (the old sample->pause order left a window
  // where a callback could run AFTER the terminal gap was judged and
  // never be gap-checked). Then the sample is taken under the SDL
  // audio lock, which waits out any IN-FLIGHT callback (the callback
  // runs with the audio mutex held), so its g_pa_last_ns write is
  // ordered before the read. Same >2-period rule, accounting semantics
  // otherwise identical: a callback thread that silently died mid-run
  // lands here as underruns (its terminal gap spans to open/last-cb)
  // even when the coarse cbs window would not notice.
  SDL_PauseAudio(1);
  SDL_LockAudio();
  {
    const uint64_t tEnd = pa_now_ns();
    if (tEnd == 0) {
      g_pa_badlen++; // clock anomaly — impossible-by-spec, counted loud
    } else if (g_pa_last_ns != 0 &&
               tEnd - g_pa_last_ns > 2 * g_pa_nominal_ns) {
      g_pa_underruns++;
    }
  }
  SDL_UnlockAudio();
  SDL_CloseAudio(); // joins the callback thread — stats are safe after
  g_pa_open = 0;
}

void platform_audio_lock(void) {
  if (g_pa_open) SDL_LockAudio();
}

void platform_audio_unlock(void) {
  if (g_pa_open) SDL_UnlockAudio();
}

void platform_audio_stats(PlatformAudioStats *out) {
  out->cbs = g_pa_cbs;
  out->underruns = g_pa_underruns;
  out->badlen = g_pa_badlen;
  out->rate = g_pa_granted.freq;
  out->samples = (int)g_pa_granted.samples;
  out->channels = (int)g_pa_granted.channels;
}

#endif // GFX_PLATFORM_AUDIO_SDL_H
