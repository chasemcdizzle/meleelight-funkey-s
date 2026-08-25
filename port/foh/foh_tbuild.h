// port/foh/foh_tbuild.h — A45 T4: the TARGET BUILDER, targets only.
//
// THE TICKET. The owner clicked TARGET BUILDER, reported *"nothing
// happened"*, and he was right: menu-top row 2 played `deny` and emitted
// `ev_refused(s, "targetbuilder")` (foh.c). That row now opens a real
// editor — place, move and delete targets on a grid, then save the stage to
// one of ten custom slots and play it from Target Test.
//
// SCOPE, STATED SO THE OMISSIONS ARE NOT MISTAKEN FOR STUBS. Upstream's
// `targetBuilderControls` (targetbuilder.js:159-855) has TEN tools. This
// ticket ships THREE — 5 TARGET (:560-571), 6 MOVE (:572-618) and 7 DELETE
// (:619-738), restricted to targets and starting points. Tools 0-4 and 8-9
// (Polygon / Platform / Wall / Ledge / Damage / Scale / Draw Mode) are the
// design spike's T5-T8 and are NOT here, NOT stubbed, and NOT drawn: an
// absent tool is absent, and the tool cycle has three entries, not ten with
// seven that beep.
//
// -----------------------------------------------------------------------
// WHY THIS IS ITS OWN TU BEHIND A POINTER SEAM
//
// The editor's document is an `MlkStage` and its save path is A45 T1's
// `mlk_encode`, so it needs `port/sim/stage_code.{c,h}` linked. MEASURED
// this session: SIXTEEN check scripts and rigs compile `port/foh/foh.c`
// (grep over port/foh/*.sh, port/gfx/opk/*.sh, port/sim/device/*.sh), and
// four of them are DEVICE rigs that cannot be run from here. Making foh.c
// depend on the codec would have meant editing every one of those TU lists
// blind.
//
// So the builder lives here, and foh.c reaches it through ONE pointer:
//
//     const FohTbuildOps *foh_tbuild_ops;   // NULL unless this TU is linked
//
// installed by a constructor in foh_tbuild.c. That is this project's own
// established shape for exactly this problem — `tp_custom_setup`
// (port/sim/target/target_play.h, A45 T2) and `ml_sim_runai_live`
// (port/sim/sim/sim_ai_live.c, M4 task 5), whose CLAUDE.md note reads
// *"frozen-build link seams — new symbols behind a constructor-installed
// pointer ... never referenced from frozen-list TUs"*. Every existing check
// keeps its exact TU list and its exact bytes.
//
// AND THE UNLINKED BUILD REFUSES VISIBLY, WHICH IS THE POINT. A build
// without this TU still HAS the screen and still enters it; it draws
//
//     TARGET BUILDER UNAVAILABLE IN THIS BUILD
//
// on screen (foh_render.c's render_tbuild) and B leaves. It is not a silent
// no-op and it is not a sound the owner cannot hear — the failure the
// original ticket was filed about.
//
// -----------------------------------------------------------------------
// WHAT IT WRITES, AND HOW
//
// One `.mlstage` file per slot in the persist directory, exactly A45 T2's
// on-disk contract (custom_stage.h): three LF lines — `MLSTAGE1`, the share
// code, `SUM <64 lowercase hex>` over every preceding byte.
//
// It is published through `foh_persist_publish` (foh_persist.h) — T2's own
// instruction, verbatim: *"generalise foh_persist_save's existing publish
// ... NOT ... a second file-writing path"*. So the builder inherits tmp ->
// fsync -> rename -> fsync(dir), every rc checked, plus the statvfs
// free-space check that generalisation added. `/mnt` is vfat with no
// journal, mounted errors=remount-ro, and this device has a MEASURED
// power-loss risk (the owner powered it off mid-operation), so a failed
// write is an expected input. Every failure is NAMED ON SCREEN.
//
// READS ARE VALIDATED, ALWAYS, and the validator is not a second opinion:
// `foh_tbuild.c`'s reader bounds the read, requires the exact grammar and
// verifies SUM BEFORE parsing, and `check-tbuild.sh` proves DIFFERENTIALLY
// that it accepts exactly what the sim's own unmodified `mlk_slot_load`
// accepts, over the same corpus. Sharing the code was not available:
// `custom_stage.h` transitively includes `sim/sim.h`, which needs the
// GENERATED `ml_stages.h`, so foh.c cannot even INCLUDE it (measured). A
// differential against the original is this project's answer to that
// (the `fmt_diff` discipline) and is stronger than a shared body.
#ifndef FOH_TBUILD_H
#define FOH_TBUILD_H

#include <stdbool.h>

#include "../gfx/platform.h" // PlatformInput
#include "foh.h"             // FohState

// Ten slots, addressed BY INDEX — D43 (owner ruling, A45 T2). Upstream's
// list clobbers on both the add path and the boot reload
// (targetselect.js:164-166, :551-552); the port has no append and no length
// cursor anywhere, here included. Slot i is `custom<i>.mlstage` and nothing
// else, so writing slot i cannot touch slot j and there is no `length - 1`
// to be off by one. Mirrors MLK_MAX_SLOTS without including the header that
// defines it (see the TU note above).
#define FOH_TB_SLOTS FOH_TB_SLOT_CACHE

// The three tools, in upstream's own cycle order (targetbuilder.js:36's
// toolInfo, indices 5/6/7 — TARGET, MOVE, DELETE).
#define FOH_TB_TOOL_TARGET 0
#define FOH_TB_TOOL_MOVE 1
#define FOH_TB_TOOL_DELETE 2
#define FOH_TB_TOOLS 3

// The pause rows. Upstream's are Test / Save / Quit (targetbuilder.js:
// 793-836). DELETE is added because the FunKey-S has NO `z` BUTTON — the
// key upstream's target-select binds delete to (targetselect.js:82) — and
// TEST is not here (see the header note on scope). The verb lands where the
// stage it destroys is on screen, which is the most legible place for it.
#define FOH_TB_PAUSE_LOAD 0
#define FOH_TB_PAUSE_SAVE 1
#define FOH_TB_PAUSE_DELETE 2
#define FOH_TB_PAUSE_QUIT 3
#define FOH_TB_PAUSE_ROWS 4

// Which slot list the pause menu has open, if any.
#define FOH_TB_PANE_NONE 0
#define FOH_TB_PANE_LOAD 1
#define FOH_TB_PANE_SAVE 2
#define FOH_TB_PANE_DELETE 3

// Hover/grab encoding, one integer so MOVE and DELETE share a cursor:
// -1 nothing, 0..19 target index, FOH_TB_SP + i starting point i.
#define FOH_TB_NONE (-1)
#define FOH_TB_SP 1000

// The verdict step() returns to foh.c, which owns every transition and
// sound-with-a-transition (ev_trans / snd_push are file-static there).
typedef enum {
  FOH_TB_STAY = 0,
  FOH_TB_QUIT,   // -> menu-top; upstream's changeGamemode(1) (:833-835)
} FohTbVerdict;

// Everything the RENDERER needs, filled on demand. Deliberately a flat
// snapshot rather than accessors: the renderer must never hold a pointer
// into the document, and one call is cheaper than twenty through a seam.
// Sizes are the codec's own caps (stage_code.h) restated locally.
#define FOH_TB_MAX_TARGETS 20
#define FOH_TB_MAX_SP 8
#define FOH_TB_MAX_LINES 64

typedef struct {
  int nTarget;
  double tx[FOH_TB_MAX_TARGETS], ty[FOH_TB_MAX_TARGETS];
  int nSp;
  double spx[FOH_TB_MAX_SP], spy[FOH_TB_MAX_SP];
  // The collision surfaces, drawn so the player can see the floor he is
  // placing targets on. T4 edits none of them — they come from the D51
  // template or from the loaded slot and are written straight back out.
  int nLine;
  double lx0[FOH_TB_MAX_LINES], ly0[FOH_TB_MAX_LINES];
  double lx1[FOH_TB_MAX_LINES], ly1[FOH_TB_MAX_LINES];
  double scale; // world -> upstream-canvas factor (targetbuilder.js:65)
  // Slot presence for the pane list. `reason[i]` is NULL when present,
  // else the rule that refused it — a corrupt slot shows up NAMED rather
  // than silently vanishing (D43's argument: never shift, never hide).
  bool present[FOH_TB_SLOTS];
  const char *reason[FOH_TB_SLOTS];
} FohTbView;

typedef struct {
  // Enter the editor. slot < 0 = a fresh stage from the D51 template;
  // slot >= 0 = that slot loaded, editingStage set (targetselect.js:113-119).
  // Upstream's menu arm (menu.js:87-90) does setEditingStage(-1) and does
  // NOT reset stageTemp, so a second visit keeps the last document —
  // module lifetime, carried verbatim.
  void (*enter)(FohState *s, int slot);
  FohTbVerdict (*step)(FohState *s, const PlatformInput *in,
                       const PlatformInput *pv);
  void (*view)(const FohState *s, FohTbView *out);
  // Slot presence for the TARGET-SELECT custom page (T3). Same data as
  // FohTbView.present, without materialising a whole view.
  void (*slots)(bool present[FOH_TB_SLOTS], const char *reason[FOH_TB_SLOTS]);
} FohTbuildOps;

// NULL unless foh_tbuild.c is linked. Read it, never assume it.
extern const FohTbuildOps *foh_tbuild_ops;

#endif // FOH_TBUILD_H
