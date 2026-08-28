#ifndef FOH_MATCH_SNAP_H
#define FOH_MATCH_SNAP_H
// port/foh/foh_match_snap.h — ticket #29: a match survives the lid closing.
//
// WHAT THIS IS. Ticket #28 gave the sim a snapshot that is proven to restore
// a match into a DIFFERENT PROCESS (port/sim/sim/sim_snapshot.h: write, read
// back, and the frames after the restore point are identical to an
// uninterrupted run, on every golden, judged by the unchanged
// oracle/harness/verify-stream.js). This file is the seam that hands that
// mechanism to the front of house, so that the thing which restores it is a
// real lid close rather than an environment variable.
//
// CONTEXT.md, "Resume": the port shipped SCREEN resume and the name implied
// STATE resume. FOH_MATCH was mapped to FOH_CSS precisely because there was
// no state to come back to. There is now, so the map changes — but only for
// a match whose state actually reached the card, which is the whole of the
// interface below.
//
// THE THREE RULES, all measured, none of them ours to relax:
//
//  1. THE SNAPSHOT IS WRITTEN FIRST, before the settings record. The grace
//     window between the lid switch and the power cut is ~100 ms and already
//     carries a ~22 ms settings write; a 160 KB snapshot with an fsync costs
//     ~33 ms. It fits — and it is ordered first anyway, so that if the window
//     is ever missed what is lost is the snapshot and not the player's
//     settings.
//
//  2. A FAILED OR REFUSED SNAPSHOT DISARMS THE RESUME. An armed resume with
//     no state behind it is worse than no resume: it is a promise the next
//     boot cannot keep. This is not a new rule — it is the one the target
//     builder's document resume already follows (foh_dev.c's
//     tdev_hibernate_check, A45/A26), and `arm` below returns false for
//     exactly the same reason FohTbuildOps.suspend does.
//
//  3. THE RESUME RESTORES THE SIM AND THE SCREEN TOGETHER. Coming back to a
//     match that restarted is not a resume; the acceptance criterion is that
//     percent, stocks, positions and the clock are unchanged and that the
//     frames after the resume point are identical to an uninterrupted run.
//
// THE LINK SEAM. `foh_match_snap_ops` is defined NULL by foh_dev.c (the app
// that owns it — foh_tbuild_ops' arrangement, moved up one level because this
// header needs GameState and foh.c deliberately does not see the sim) and is
// installed by foh_match_snap.c's constructor, so it stays NULL unless that TU
// is linked — the FohTbuildOps / ml_sim_runai_live / tp_custom_setup pattern
// (CLAUDE.md). A build without it behaves EXACTLY as the port did before this
// ticket: `arm` is unreachable, the hibernate path takes its "no match
// snapshot in this build" arm, and the resume downgrades to the character
// select, which is the pre-#29 behaviour verbatim.
//
// TWO FILES, AND WHY. The snapshot itself is `mlfk-match.sim` (MLSIM1, #28's
// format and #28's writer). Beside it sits `mlfk-match.hdr`, a five-line
// ASCII header carrying the STAGE, the FRAME and the snapshot's BUILD
// identity. It exists because the resume has to know the stage BEFORE it can
// load anything: gfx_init and sim_setup_match_ports both take a stage id and
// both run before the sim state can be put back, so the stage cannot come out
// of the snapshot it precedes. FRAME and BUILD are carried in the same file
// because they make the PAIR checkable — see the ordering note in
// foh_match_snap.c, which is where the torn-pair argument lives.
//
// The header is published through `foh_persist_publish`, the ONE atomic
// publish (foh_persist.h) — not a second write path. The snapshot keeps
// ss_save's own publish, which is the same tmp/fsync/rename discipline and is
// the writer the sim's rigs already prove; it cannot call into the FOH
// because port/sim must not depend on port/foh.
#include "../sim/sim/sim.h"

typedef struct {
  // Write the match state, then arm. Returns false with *why naming the rule
  // that stopped it, and on false NOTHING is armed: the caller must downgrade
  // the resume, and foh_dev.c does. `stageSel` is the launched stage, which
  // is the one thing the resume needs before it can read the snapshot.
  //
  // Called from the hibernate path, i.e. inside the grace window, so it does
  // exactly two writes and no reads.
  bool (*arm)(const GameState *g, int stageSel, const char **why);

  // Read the header ONLY: the ~150 bytes the launch needs before a match can
  // be set up. Refuses by name — bad grammar, bad SUM, or a build identity
  // that is not this binary's — WITHOUT touching the 160 KB snapshot, so a
  // stale card costs a file read and not a restore.
  bool (*peek)(int *stageSel, long *frame, const char **why);

  // Put the sim state back over an ALREADY SET UP match (the ss_load
  // precondition: boot, setup and any RECON row are the caller's job and have
  // run). Verifies that what came back is the match the header described —
  // same stage, same frame — and refuses if not. On success the pair is
  // disarmed, so a crash later in the session cannot resume this match twice.
  bool (*restore)(GameState *g, int stageSel, long frame, const char **why);

  // Remove the pair. Idempotent, and silent about a file that was not there.
  void (*disarm)(void);
} FohMatchSnapOps;

extern const FohMatchSnapOps *foh_match_snap_ops;

// The two names, exported so a check can look for them by name rather than
// re-spelling them (the CONTEXT.md "Tooth" rule: assert the outcome, not a
// string that happens to produce it).
#define FOH_MATCH_SNAP_FILE "mlfk-match.sim"
#define FOH_MATCH_SNAP_HDR "mlfk-match.hdr"

#endif // FOH_MATCH_SNAP_H
