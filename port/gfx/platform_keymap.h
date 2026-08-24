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
//
// THE `l` ROW WAS WRONG UNTIL 2026-08-24 (fix_plan A25b + A3). It said
// 'k' — inherited from ssb64 donor archaeology (docs/research/
// funkey-envelope.md:77) and never measured on THIS device. The owner
// pressed both shoulders while the driver decoded /dev/input/event0's
// 16-byte input_event records off `fkgpiod`, the single virtual node:
//     L shoulder -> KEY_M (code 50)      R shoulder -> KEY_N (code 49)
// So R was right and L was dead, at every call site at once — L did
// nothing in target-select (A25b) and never shielded in a match (A3).
// No check could see it: every device check injects through our OWN
// uinput node while the physical buttons come from fkgpiod, which the
// rig's quiesce bracket stops for the run — the rig is structurally
// blind to physical-button -> keycode for ALL twelve rows, and 'k' was
// the one row with no independent confirmation.
//
// ONLY THE KEYSYM MOVED. The flowLetter column is a SCRIPT alphabet,
// not a device fact, and 'K' is hard-pinned in three places that a
// rename would break for nothing: flow-to-fkscript.js's
// KEYMAP_FLOW_LETTERS ("UDLRABXYSKNQ"), foh_app.c's parse_buttons
// switch (`case 'K': in.l = true;`), and the committed flow
// port/foh/flows/f07-target-t02.flow (`I 400 K`). Those keep working
// unchanged and now inject 'm', because the injector resolves FLOW
// letter -> keysym through THIS table.
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
    {"l", 'K', 'm', offsetof(PlatformInput, l)}, // KEY_M — measured 2026-08-24
    {"r", 'N', 'n', offsetof(PlatformInput, r)},
    {"menu", 'Q', 'q', offsetof(PlatformInput, menu)},
};

static inline bool *platform_keymap_field(PlatformInput *in, int idx) {
  return (bool *)((char *)in + kPlatformKeymap[idx].fieldOff);
}

// THE TRANSLATION ARM ITSELF (hoisted out of platform_sdl1.c, A25b).
// `keystate` is an ASCII-indexed held-key array with at least 256 entries —
// exactly what SDL_GetKeyState(0) returns on the device, where letter
// keysyms ARE their ASCII codes. It lives here rather than in the backend
// so the arm the DEVICE runs can be executed on a host with no SDL at all
// (port/gfx/ctl_input_witness.c drives a synthetic keystate through this
// very function); a backend-private copy could only ever be read, not run.
// Every row is assigned unconditionally, so the button plane is total and
// no stale bit can survive; fields outside the table (quit) are untouched,
// which is why the callers memset BEFORE draining events rather than here.
static inline void platform_keymap_translate(PlatformInput *in,
                                             const unsigned char *keystate) {
  for (int i = 0; i < PLATFORM_KEYMAP_ROWS; i++) {
    *platform_keymap_field(in, i) =
        keystate[(unsigned char)kPlatformKeymap[i].keysym] != 0;
  }
}

#endif // GFX_PLATFORM_KEYMAP_H
