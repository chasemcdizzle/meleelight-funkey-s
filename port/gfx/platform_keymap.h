// port/gfx/platform_keymap.h — THE keymap definition site (M4 iter 97,
// review-95 M-b).
//
// ONE source definition of the logical-button → FLOW1 letter → device
// letter-keysym mapping, consumed by BOTH sides of the seam:
//   - platform_sdl1.c's platform_poll TRANSLATION ARM (the device input
//     path) loops over this table — k[keysym] → PlatformInput field;
//   - foh_dev.c emits it verbatim via --dump-keymap (cmp'd against the
//     frozen port/foh/keymap-frozen.txt by check-device-foh.sh every
//     run) and drives FLOW1 parse_buttons from the flowLetter column.
// A mapping refactor therefore CANNOT drift silently: any change to
// this site changes the compiled dump, which dies against the frozen
// file; the check's T12 copy-build tooth proves that death, and the
// device tooth (T-devswap) proves the runtime chain end-to-end.
//
// SINGLE-LINK-SYMBOL REFUTATION (AGENT-LOG iter 97, pre-registered):
// a dedicated table TU cannot be linked — the frozen rig_arm_build
// heredoc compiles a fixed TU list, and the host --dump-keymap binary
// links platform_headless.c, not platform_sdl1.c. Hence this header:
// one SOURCE definition site, compiled into each consuming TU.
//
// The keysym column is the FunKey firmware's measured letter keysym
// (CLAUDE.md §Commands "Device access"); SDL1.2 letter keysyms equal
// their ASCII codes (platform_sdl1.c _Static_asserts it).
#ifndef GFX_PLATFORM_KEYMAP_H
#define GFX_PLATFORM_KEYMAP_H

#include <stddef.h>

#include "platform.h"

typedef struct {
  const char *logical; // PlatformInput field name (KEYMAP1 dump column)
  char flowLetter;     // FLOW1 button letter
  char keysym;         // device letter keysym (== ASCII code in SDL1.2)
  size_t fieldOff;     // offsetof(PlatformInput, <logical>)
} PlatformKeymapRow;

#define PLATFORM_KEYMAP_ROWS 12

static const PlatformKeymapRow kPlatformKeymap[PLATFORM_KEYMAP_ROWS] = {
    {"up", 'U', 'u', offsetof(PlatformInput, up)},
    {"down", 'D', 'd', offsetof(PlatformInput, down)},
    {"left", 'L', 'l', offsetof(PlatformInput, left)},
    {"right", 'R', 'r', offsetof(PlatformInput, right)},
    {"a", 'A', 'a', offsetof(PlatformInput, a)},
    {"b", 'B', 'b', offsetof(PlatformInput, b)},
    {"x", 'X', 'x', offsetof(PlatformInput, x)},
    {"y", 'Y', 'y', offsetof(PlatformInput, y)},
    {"start", 'S', 's', offsetof(PlatformInput, start)},
    {"l", 'K', 'k', offsetof(PlatformInput, l)},
    {"r", 'N', 'n', offsetof(PlatformInput, r)},
    {"menu", 'Q', 'q', offsetof(PlatformInput, menu)},
};

static inline bool *platform_keymap_field(PlatformInput *in, int idx) {
  return (bool *)((char *)in + kPlatformKeymap[idx].fieldOff);
}

#endif // GFX_PLATFORM_KEYMAP_H
