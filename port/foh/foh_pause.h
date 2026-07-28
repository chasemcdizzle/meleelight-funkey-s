// port/foh/foh_pause.h — in-match pause overlay (punch-list A11/A12).
//
// PATTERN REFERENCE (owner-directed): Chase's own MIT ssb64-funkey-s port,
// port/gfx/fk_menu.c + port/include/fk_menu.h — a modal in-game menu drawn
// over the current frame that BLOCKS the game loop until dismissed and
// returns a quit/continue verdict, opened by the FunKey MENU button (keysym
// 'q'). Same shape here; no code copied (that port draws through SDL_ttf /
// SDL_image onto an SDL_Surface, this one draws through THIS port's raster +
// platform seam and its own 6x9 face). NOTICES carries the attribution.
//
// FAITHFULNESS (the reason this lives OUTSIDE the sim):
//   Upstream meleelight's pause is `playing` (main.js:114), toggled by
//   interpretPause (main.js:847-859) on the START rising edge in gameMode
//   3/5. When `playing` is false, gameTick SKIPS its whole sim branch
//   (main.js:1045-1146): no physics, no hit detection, no articles, no
//   timer, and therefore ZERO RNG draws. Pause is not on the checksum
//   surface either (oracle/CHECKSUM.md §2 is an exhaustive allow-list —
//   no `pause`, no `playing`, no `gameMode`). A paused upstream frame is
//   thus a no-op for everything the goldens hash, and freezing the DRIVER's
//   loop outside the sim is behaviourally equivalent to it while leaving
//   every checksummed TU byte-untouched (port/sim/** is not edited by
//   A11/A12; `bash port/sim/check-sim.sh` is the proof).
//
//   The sim's OWN pause machine (ml_interpret_pause, interpret_inputs.c:
//   40-53; the pastOffset freeze, ai_input.h:198-204) is left exactly as
//   translated, so replay conformance is structurally unchanged. What the
//   play path stops doing is FEEDING it: see the `.s` mask at the live-row
//   site in foh_dev.c — with no `playing || frameByFrame` gate in
//   sim_tick.c, a live START press could only ever reach the pastOffset
//   freeze (input history stops shifting) with no pause to compensate it.
//
// LIVE-PLAY-ONLY BY CONSTRUCTION: `foh_pause_hook` is NULL by default and
// is installed at exactly one site, inside foh_dev.c's `--bridge live`
// arm. Evidence / flow / trace-fed runs leave it NULL, so the overlay
// branch in the match loop is unreachable for them — the tp_endgame_hook
// pattern (target_play.c), not a runtime flag anyone can flip.
#ifndef FOH_FOH_PAUSE_H
#define FOH_FOH_PAUSE_H

#include <stdint.h>

#include "../gfx/raster.h"

typedef enum {
  FOH_PAUSE_RESUME = 0,
  FOH_PAUSE_QUIT_MENU,      // back to the FOH menus (see FOH_PAUSE_RC_MENU)
  FOH_PAUSE_QUIT_FRONTEND   // back to the FunKey frontend (gmenu2x)
} FohPauseResult;

// Process exit code that asks the OPK launcher to run the app again, i.e.
// "quit to menu" — the app boots into the FOH, so a relaunch IS the menus.
// ponytail: an in-process return to the FOH needs the FOH/match outer loop
// no play arm has (foh_dev.c:2076-2087, already a registered deviation);
// a clean process boundary costs 3 lines of launcher instead of restructuring
// a 1300-line main(). Upgrade path if boot latency ever annoys.
#define FOH_PAUSE_RC_MENU 70

// Modal overlay over rz's CURRENT frame: freezes the caller's loop, polls
// and presents at ~60 Hz itself, returns when dismissed. *pausedNs receives
// the wall clock consumed so the caller can shift its pace epoch (otherwise
// every remaining frame is "late" and the catch-up arm skips all rendering).
// *presentFails is INCREMENTED (never reset) by every failed present the
// overlay performs, so the driver's `failed presents` summary — the number
// the device checks gate on — stays honest across the frozen window.
//
// On RESUME it does not return until the dismissing button is RELEASED: the
// caller feeds the very next poll straight into gameplay, so returning on a
// still-held A/B would swing an attack, and a still-held START would hit
// target mode's endGame arm. Quit results return immediately (the caller is
// leaving the loop anyway).
FohPauseResult foh_pause_open(Raster *rz, uint64_t *pausedNs,
                              long *presentFails);

// NULL by default. Installed only by the live play arm; see header note.
extern FohPauseResult (*foh_pause_hook)(Raster *rz, uint64_t *pausedNs,
                                        long *presentFails);

#endif // FOH_FOH_PAUSE_H
