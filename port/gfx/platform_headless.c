// port/gfx/platform_headless.c — the loop/CI backend (M3 task 4).
//
// No display, no SDL, no input source. This is what makes the
// autonomous loop possible (CLAUDE.md "SDL / platform seam"): host
// replays link THIS TU and run the exact same app loop the device runs,
// minus the panel. present() is a no-op that still touches nothing;
// poll() reports an all-false input struct and never requests quit.
#include <stdlib.h>
#include <string.h>

#include "platform.h"

int platform_init(const char *title) {
  (void)title;
  return 0;
}

int platform_present(const uint16_t *fb565) {
  (void)fb565;
  // Negative-testing seam (iter 52, review-50 M2 tooth): report every
  // present as FAILED when MLFK_HEADLESS_PRESENT_FAIL=1, proving the
  // app's failure counter + the check's failed-presents gate end to
  // end. Default (env absent/anything else): success, unchanged.
  const char *t = getenv("MLFK_HEADLESS_PRESENT_FAIL");
  return (t && t[0] == '1' && t[1] == 0) ? 1 : 0;
}

void platform_poll(PlatformInput *in) { memset(in, 0, sizeof *in); }

void platform_quit(void) {}

// --- audio (M3 task 6): ACCEPT-AND-IDLE ---------------------------------------
// No callback thread ever runs headless: start succeeds so the app's
// main-thread event-scheduling path (mixer voice bookkeeping) runs
// deterministically on host truth legs, but no audio is rendered and
// the granted spec reports 0/0/0 — the check pins those fields per leg,
// so a headless run can never masquerade as a device audio run.
static int g_pa_headless_open;

int platform_audio_start(PlatformAudioFill fill, void *ud, int samples) {
  (void)fill;
  (void)ud;
  if (samples <= 0 || samples > 65535) return 1;
  g_pa_headless_open = 1;
  return 0;
}

void platform_audio_stop(void) { g_pa_headless_open = 0; }
void platform_audio_lock(void) {}
void platform_audio_unlock(void) {}

void platform_audio_stats(PlatformAudioStats *out) {
  memset(out, 0, sizeof *out); // zeros: cbs/underruns/badlen, spec 0/0/0
}
