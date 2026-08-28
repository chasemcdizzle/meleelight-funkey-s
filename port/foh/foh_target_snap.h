#ifndef FOH_TARGET_SNAP_H
#define FOH_TARGET_SNAP_H
// port/foh/foh_target_snap.h — ticket #30: a TARGET RUN survives the lid.
//
// WHAT THIS IS, AND WHY IT IS NOT A COPY OF #29. Ticket #29 wired a VS match
// to a real lid close (port/foh/foh_match_snap.h) and left this seam alone on
// purpose, saying so in foh_persist.c's own resume map: "a target match is a
// different serialization surface and #30 owes it its own frozen trace". The
// difference is not the plumbing — that is deliberately the same, line for
// line — it is WHAT IS BEING RESTORED. A target run's state is not in
// GameState at all: which targets are broken, how many, and whether the run
// has ended live in `MlTargets TP` (port/sim/target/target_play.h), a plane
// that is NOT on the CHECKSUM.md §2 surface and has its OWN frozen stream and
// its OWN verifier (port/goldens-m4/verify-target-stream.js). A restore that
// got the sim right and that plane wrong would pass every assertion #29 wrote
// and would still hand the player back a run whose broken targets came back.
//
// Ticket #30 step 1 classified all ten MlTargets fields at the struct
// (target_play.c's TS_PERSISTED / TS_DERIVED lists, guarded by a
// _Static_assert over sizeof(MlTargets)) and put the persisted half in the
// MLSIM1 payload as the `mod:targets` row. This file is the seam that hands
// that mechanism to the front of house.
//
// THE THREE RULES, inherited from #29 whole, none of them ours to relax:
//
//  1. THE SNAPSHOT IS WRITTEN FIRST, before the settings record — so that if
//     the ~100 ms grace window is ever missed, what is lost is the snapshot
//     and never the player's settings (which, on this screen, carry his
//     RECORDS).
//
//  2. A FAILED OR REFUSED SNAPSHOT DISARMS THE RESUME. The downgrade is to
//     FOH_TSS, which is where a target run's own exit lands, i.e. exactly
//     where the port put the player before this ticket.
//
//  3. THE RECORD IS ONE-SHOT. A resumed run must not be resumable a second
//     time: the player is playing it now.
//
// AND ONE RULE THAT IS THIS TICKET'S OWN: A FINISHED RUN IS NOT ARMED. If the
// lid closes during the ~180-frame COMPLETE!/FAILURE hold, `TP.gameEnd` is
// already true — the run is over, its record has already gone through
// foh_persist_record_update's improve-or-first chokepoint, and the settings
// save this same hibernate performs carries it. "Continuing" it would mean
// replaying the hold in a process whose banner state is fresh, and re-entering
// the finish path is the one way this feature could DOUBLE-COUNT a record. So
// `arm` refuses by name and the boot lands on target select with the record
// intact. The refusal is the honest answer, not a limitation being hidden.
//
// TWO FILES, AND WHY. The snapshot itself is `mlfk-tmatch.sim` (MLSIM1,
// ticket #28's format and #28's writer). Beside it sits `mlfk-tmatch.hdr`, a
// seven-line ASCII header carrying everything the LAUNCH needs before any
// state can be put back — because gfx_target_init, the .mlstage load and
// tp_setup_target_core all run before ss_load can, so none of it can come out
// of the snapshot it precedes:
//
//   TSTAGE  which stage (0-9 authored, 10-19 == MLK_PLAYING_BASE + custom
//           slot, upstream's own numbering at targetselect.js:140-146).
//   CHAR    the character. It is ALSO on the settings record, and it is
//           carried here anyway so the pair is self-describing and `restore`
//           can check that the run being set up is the run the header names.
//   FRAME   where the run got to.
//   SRC     THE STAGE'S SOURCE IDENTITY — see below. This is the field that
//           makes this header different from #29's.
//   BUILD   the snapshot's build identity (ss_build_identity), refused here
//           so a stale card costs one small read and not a 160 KB one.
//
// HOW A CUSTOM STAGE IS RE-FOUND, AND WHAT HAPPENS IF THE CARD CHANGED.
// A custom target stage is played from `custom<N>.mlstage` on the SD card
// (A45 T2/T3, port/sim/target/custom_stage.h), and the card is the one thing
// that can have changed while the machine was off — the player can pull it
// out and edit it on a PC. The DECISION, stated:
//
//   The resume re-loads slot N from the card through the SAME `mlk_slot_load`
//   the launch uses — it is not carried in the snapshot and it is not
//   inferred — and the header's SRC line pins WHICH stage that has to be. If
//   the file is gone, refused, or now holds a different stage, the resume is
//   refused BY NAME and the player lands on target select.
//
// It is re-loaded rather than carried because the RENDERER needs the parsed
// stage anyway (gfx_target_init_custom), and a resume whose physics came from
// the snapshot while the renderer drew whatever the card now holds would be a
// game that lies about its own ground. Both sides therefore come from one
// place, and SRC is what proves it is the same place as last time.
//
// SRC is the ops `src` member below, and it is ONE definition used by both the
// arm and the restore. For a CUSTOM slot it is the sha256 of the stage's
// canonical share code (`mlk_encode`, which A45 T1 proved is a fixed point of
// the codec), so it is a digest of the STAGE and not of the file's incidental
// bytes. For an AUTHORED stage there is no code and no card: its geometry is
// the compiled TTAB1 table, which the BUILD line already pins, so SRC is the
// sha256 of the ASCII descriptor `authored:<id>` — a real, deterministic
// value in the same column rather than a magic blank, recomputed identically
// on both sides.
//
// THE LINK SEAM. `foh_target_snap_ops` is defined NULL by foh_dev.c (the app
// that owns it — foh_match_snap_ops' arrangement, followed exactly) and is
// installed by foh_target_snap.c's constructor, so it stays NULL unless that
// TU is linked. A build without it behaves EXACTLY as the port did before this
// ticket: `arm` is unreachable, the hibernate path takes its "no target
// snapshot in this build" arm, and the resume downgrades to target select.
#include "../sim/stage_code.h"      // MlkStage (the custom-stage value)
#include "../sim/sim/sim.h"

// What the header says, filled by `peek` and handed back to `restore`.
typedef struct {
  int tstage;   // 0..19; >= MLK_PLAYING_BASE is a custom slot
  int charId;   // 0..4
  long frame;   // the frame the run had reached; always > 0
  char src[65]; // the stage source identity, lowercase hex, NUL terminated
} FohTmatchHdr;

typedef struct {
  // THE ONE definition of the stage source identity (SRC above). `custom` is
  // the parsed slot for tstage >= MLK_PLAYING_BASE and is ignored (may be
  // NULL) otherwise. Deterministic, and computed at LAUNCH time on the write
  // side — never inside the grace window, which does two writes and no work.
  //
  // IT IS AN OPS MEMBER RATHER THAN A FREE FUNCTION, and that is not a style
  // choice: foh_dev.c calls it at the launch seam, which every build reaches,
  // so a free symbol would make the app fail to LINK without this TU and the
  // "a build without it behaves exactly as the port did before #30" claim
  // would be false. Everything this seam owns crosses the same pointer.
  void (*src)(int tstage, const MlkStage *custom, char out[65]);

  // Write the run's state, then arm. Returns false with *why naming the rule
  // that stopped it, and on false NOTHING is armed: the caller must downgrade
  // the resume, and foh_dev.c does. Called from the hibernate path, i.e.
  // inside the grace window, so it does exactly two writes and no reads.
  bool (*arm)(const GameState *g, int tstage, int charId, const char *srcHex,
              const char **why);

  // Read the header ONLY: the ~255 bytes the launch needs before a run can be
  // set up. Refuses by name — bad grammar, bad SUM, a build identity that is
  // not this binary's — WITHOUT touching the snapshot.
  bool (*peek)(FohTmatchHdr *out, const char **why);

  // Put the state back over an ALREADY SET UP run (the ss_load precondition).
  // `want` is what `peek` returned; `liveSrc` is `src` recomputed
  // from the stage the launch ACTUALLY loaded, which is what makes a changed
  // card a named refusal instead of a wrong game. Verifies that what came back
  // is the run the header described — same stage, same character, same frame,
  // same source — and refuses if not. On success the pair is disarmed.
  bool (*restore)(GameState *g, const FohTmatchHdr *want, const char *liveSrc,
                  const char **why);

  // Remove the pair. Idempotent, and silent about a file that was not there.
  void (*disarm)(void);
} FohTargetSnapOps;

extern const FohTargetSnapOps *foh_target_snap_ops;

// The two names, exported so a check can look for them by name rather than
// re-spelling them (CONTEXT.md's "Tooth" rule: assert the outcome, not a
// string that happens to produce it today).
#define FOH_TMATCH_SNAP_FILE "mlfk-tmatch.sim"
#define FOH_TMATCH_SNAP_HDR "mlfk-tmatch.hdr"

#endif // FOH_TARGET_SNAP_H
