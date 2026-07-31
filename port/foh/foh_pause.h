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
  FOH_PAUSE_QUIT_MENU,      // back to the FOH menus (in-process; see below)
  FOH_PAUSE_QUIT_FRONTEND,  // back to the FunKey frontend (gmenu2x)
  // punch-list C19: back to the SELECT screen — the CSS. NOT a new feature:
  // upstream reaches exactly this destination from a paused match via
  // A+L+R+Start (main.js:738-747 -> endGame -> changeGamemode(2)), a combo the
  // FunKey has no buttons for.
  //
  // VS ONLY, and that is structural rather than an omission (review-mexit-r3
  // Medium): this overlay opens on START, and in TARGET mode START is
  // upstream's own endGame quit (main.js:1013-1015), which foh_dev.c's target
  // loop implements directly via tp_endgame_hook. Upstream has no target
  // pause menu to port, so there is no target arm to reach here and no
  // `targetMode` argument to select one with. A target quit's destination
  // (target select) is decided at that arm, not here.
  FOH_PAUSE_QUIT_SELECT
} FohPauseResult;

// A19 RESOLVED (punch-list C18/C19/B4/A19, one increment): the return to the
// FOH is now IN-PROCESS. The outer loop the note below said no play arm had
// is foh_dev.c's `foh_phase:` re-entry, and it turned out to be cheap rather
// than a restructure — FOH_MATCH/FOH_TMATCH are already screens in the FOH
// state machine and FohState is a POD that keeps the player's selections, so
// re-entry is "put the screen back, clear `launched`, run the phase again".
//
// The process boundary had to go because it produced the WRONG behaviour,
// not merely a slower one: upstream endGame KEEPS characterSelections and
// snaps the tokens back onto them, whereas a relaunch boots into the title
// with defaults — losing exactly the selections C19 exists to let the player
// change.
//
// THE EXIT CODE IS GONE (review-mexit-r3 Low found the note stale;
// review-mexit-r5 Low then ruled the code itself scaffolding). There used to
// be a `FOH_PAUSE_RC_MENU 70` here, answered by a relaunch loop in
// mlfk-foh.sh, so that "quit to menu" could be served across a process
// boundary. A19 removed the need: QUIT TO MENU is FOH_PAUSE_QUIT_MENU ->
// MEX_TITLE -> foh_dev.c's `foh_phase:` re-entry, in-process, and the process
// ends rc 0. `quitRc` was then never assigned 70 on any path.
//
// Keeping the constant and the loop "for a future arm" is exactly the
// scaffolding HARD RULE 2 forbids, and it was not inert: a future defect that
// happened to exit 70 would have been silently answered by relaunching the
// game instead of surfacing. Both are deleted. If a process boundary is ever
// genuinely needed, add the code and its producer together.

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
// This overlay is the GAME's pause menu and opens on START only. MENU/HOME
// opens the FunKey SYSTEM menu instead (below) — two buttons, two overlays.
//
// It is a VS-match overlay, and takes NO match-kind argument: see the
// FOH_PAUSE_QUIT_SELECT note above for why there cannot be a target arm. It
// used to take a `targetMode` int that only relabelled QUIT TO SELECT; the
// sole caller passed a literal 0 and the target loop never DISPATCHES this
// overlay, so the parameter advertised a mode that could not occur —
// (precisely: the shared live arm DOES install the hook for both match kinds,
// foh_dev.c:2401, and the target loop observes the pointer at :2574 only to
// prime its edge state; the target loop's START is upstream's own endGame
// quit and its MENU opens the SYSTEM menu, so no target frame ever calls
// foh_pause_open) —
// scaffolding for later, which HARD RULE 2 forbids. Deleted rather than
// wired, because upstream has no target pause menu to be faithful to.
FohPauseResult foh_pause_open(Raster *rz, uint64_t *pausedNs,
                              long *presentFails);

// NULL by default. Installed only by the live play arm; see header note.
extern FohPauseResult (*foh_pause_hook)(Raster *rz, uint64_t *pausedNs,
                                        long *presentFails);

// ==========================================================================
// The FunKey S SYSTEM menu (punch-list A12b).
//
// PROVENANCE (owner-directed, docs/LICENSING.md): this is a port of Chase's
// own MIT ssb64-funkey-s `port/gfx/fk_menu.c` + `port/include/fk_menu.h`,
// which is itself a clean reimplementation of the standard FunKey overlay
// over the OS's SHARED assets. NOTICES carries the attribution entry, added
// BEFORE this code landed. Behaviour, option list, geometry constants and the
// OS-tool delegation are carried across; the DRAWING is re-authored because
// this port has a different rendering seam (see the SDL note below).
//
// WHAT IT IS, AND WHY IT IS NOT THE PAUSE MENU: the FunKey firmware delivers
// the MENU/HOME button as keysym 'q'. Every FunKey game answers it with the
// same OS-styled overlay — VOLUME / BRIGHTNESS / QUIT / POWER OFF — and that
// is what the owner asked for. It is a SYSTEM menu, so it opens wherever you
// are: in the FOH menus AND mid-match. The GAME's own pause menu
// (Resume / VS screen / Main menu / Quit to OS) is a separate overlay on
// START — see foh_pause.h. Two buttons, two overlays, exactly as the donor
// port has it.
//
// FAITHFULNESS: nothing here is upstream meleelight behaviour — the browser
// game has no system menu — so this is device-integration furniture that
// lives strictly OUTSIDE the sim, on the same terms as the pause overlay:
// NULL-default hook, installed only by the live play arm, so no evidence,
// flow or trace-fed run can reach it and `bash port/sim/check-sim.sh` is
// untouched.
//
// THE SDL RULE (CLAUDE.md: exactly ONE TU owns the SDL API, behind
// platform_init/present/poll/quit): the donor draws with SDL_ttf and
// SDL_image onto an SDL_Surface. Doing that here would put SDL calls in a
// second TU, so instead this file:
//   * loads the OS's OWN `zone_bg.png` (240x240) through the platform seam
//     `platform_image_load565()` (platform.h) and blits it under the mask
//     that seam returns, so the PANEL is the authentic one, pixel for pixel.
//     The SDL1 backend decodes it by dlopen-ing SDL_image rather than
//     LINKING it: linking would make libSDL_image-1.2.so.0 a hard NEEDED
//     entry and a device without it could not start the game at all. Every
//     other backend returns 0 and the fallback below stands.
//     (An earlier shape read a `zone_bg.bmp` with a hand-rolled BMP parser.
//     review-mexit-r1 found the OS ships PNG only — fk_menu.c:29 names
//     `.png` — so that path could never have loaded on any device, and it
//     carried its own bounds/truncation/overflow surface. Deleted.);
//   * draws the arrows as triangles rather than loading arrow_top.png /
//     arrow_bottom.png, because those ship as PNG only and decoding PNG
//     would mean linking SDL_image for two 292-byte glyphs;
//   * renders text with this port's own face instead of OpenSans-Bold.
//
// ponytail: the font is the one visible difference from the stock overlay.
// SDL_ttf IS present both in the SDK sysroot (headers + lib) and on the
// device (/usr/lib/libSDL_ttf-2.0.so.0), so the upgrade path is real: add a
// platform_text_* seam to the ONE SDL TU and render OpenSans-Bold through it.
// Not taken here because it trades a hard architectural rule for a font.
//
// DEGRADES SAFELY (the donor's own contract): if zone_bg.png cannot be
// loaded — missing file, no SDL_image on the device, wrong size — the
// overlay falls back to a dimmed snapshot of the live frame; if the OS tools
// are missing, `volume get` / `brightness get` fall back to 50 and the set
// commands are harmless no-ops. The menu never becomes unusable.
// Modal overlay over rz's CURRENT frame, same contract as foh_pause_open:
// freezes the caller's loop, polls and presents at ~60 Hz itself.
// *pausedNs receives the wall clock consumed so the caller can shift its pace
// epoch; *presentFails is INCREMENTED (never reset) so the driver's
// `failed presents` summary stays honest across the frozen window.
//
// Returns 1 if the player chose QUIT (the caller should leave to the
// frontend), else 0. POWER OFF does not return meaningfully — the OS takes
// the device down — but is treated as a close if it somehow does.
int foh_sysmenu_open(Raster *rz, uint64_t *pausedNs, long *presentFails);

// NULL by default. Installed only by the live play arm; see the note above.
extern int (*foh_sysmenu_hook)(Raster *rz, uint64_t *pausedNs,
                               long *presentFails);


#endif // FOH_FOH_PAUSE_H
