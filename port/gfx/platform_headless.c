// port/gfx/platform_headless.c — the loop/CI backend (M3 task 4).
//
// No display, no SDL, no input source. This is what makes the
// autonomous loop possible (CLAUDE.md "SDL / platform seam"): host
// replays link THIS TU and run the exact same app loop the device runs,
// minus the panel. present() is a no-op that still touches nothing;
// poll() reports an all-false input struct and never requests quit.
#include <string.h>

#include "platform.h"

int platform_init(const char *title) {
  (void)title;
  return 0;
}

void platform_present(const uint16_t *fb565) { (void)fb565; }

void platform_poll(PlatformInput *in) { memset(in, 0, sizeof *in); }

void platform_quit(void) {}
