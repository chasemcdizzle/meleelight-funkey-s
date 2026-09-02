// port/foh/foh_credits_view.c — the credits screen's view state, across a lid.
//
// D63 (2026-09-01). #27 made the credits RESUME — the boot comes back to the
// credits screen — and that was all it made survive. Everything the screen
// actually holds was rebuilt from scratch on arrival, which the owner put
// plainly:
//
//   "the credits doesn't go back to its same state either it just goes back
//    to the credits in general. what about if there is text being shown in
//    the box? how far into the credits we are? cursor position?"
//
// ...and then named the part that matters most:
//
//   "we also want to know which and how many names have been shot by the
//    user (that's the whole premise) and that is restored too, it's kind of
//    a game"
//
// That is exactly right, and it is the difference between a screen and a
// GAME. A scroll position is a convenience; `credNameShot` and `credScore`
// are the player's progress, and dropping them on a lid close is the same
// class of loss as dropping a match. The shot flags are therefore not an
// afterthought at the end of this table — they are the reason it exists.
//
// WHY A SIDECAR, not persist rows: ~700 values that no other screen reads.
// The full argument is in foh_viewfile.h, which is the shared mechanism this
// and the target builder both use; the builder was the first instance and the
// credits are what turned it into a class (AGENTS.md HARD RULE 8).
#include "foh_credits_view.h"

#include <stdio.h>

#include "foh_viewfile.h"

#define CV_NAME "credview.dat"
#define CV_MAGIC "MLCREDVIEW1"

// The scalars, as one list so the writer and the reader cannot disagree about
// their order. Each carries the inclusive domain the reader enforces; a value
// outside it is a refusal, never a clamp onto something the player never had.
//
//   I(field, lo, hi)  int      B(field)  bool, written 0/1
//   D(field)          double, stored as its exact bit pattern and required
//                     finite on read (a NaN cursor is not a cursor)
#define CV_SCALARS(I, B, D)                                                    \
  /* credInit FIRST, and it is the field this whole file turns on. foh.c:1684
     is `if (s->credInit) cred_reset(s);` on the first credits tick, and
     foh_init sets it TRUE (foh.c:309). Omit it and the restore lands, the log
     says "credits view restored", and the very next tick wipes every field
     back to a fresh screen — which is exactly what the owner saw and what I
     shipped. A view that restores everything except the flag saying "you are
     already initialised" restores nothing. */                                 \
  B(credInit)                                                                  \
  /* HOW FAR INTO THE CREDITS YOU ARE — the owner asked for this by name and
     the first pass did not carry it. It is cScrollingPos (+2/frame) and it is
     also the EXIT: foh.c:1822 ends the credits at 5000. Without it a resumed
     screen restarts its scroll from zero, so the sequence you were most of the
     way through plays again from the top. Found by the field-coverage guard in
     check-hibernate.sh, not by re-reading this list. */                       \
  I(credScrollPos, 0, 100000)                                                  \
  I(credScore, 0, FOH_CRED_NAMES)                                              \
  I(credCool, 0, 4096)                                                         \
  B(credShootBuf)                                                              \
  I(credLaser, 0, 3)                                                           \
  D(credCursorAngle)                                                           \
  I(credHitTimer, 0, 4096)                                                     \
  I(credHitIdx, 0, FOH_CRED_NAMES - 1)                                         \
  B(credHitCleared)                                                            \
  D(credX) D(credY)

void foh_credits_view_save(const FohState *s) {
  static FohViewOut o; // ~12 KB of rows; never on the stack
  foh_view_begin(&o, CV_MAGIC);
#define CV_WI(f, lo, hi) foh_view_put_int(&o, #f, s->f);
#define CV_WB(f) foh_view_put_int(&o, #f, s->f ? 1 : 0);
#define CV_WD(f) foh_view_put_double(&o, #f, s->f);
  CV_SCALARS(CV_WI, CV_WB, CV_WD)
#undef CV_WI
#undef CV_WB
#undef CV_WD
  // THE GAME. Which names are down, and where each one has scrolled to.
  for (int i = 0; i < FOH_CRED_NAMES; i++) {
    foh_view_put_int_at(&o, "nshot", i, s->credNameShot[i] ? 1 : 0);
    foh_view_put_int_at(&o, "nrender", i, s->credNameRender[i] ? 1 : 0);
    foh_view_put_int_at(&o, "nx", i, s->credNameX[i]);
    foh_view_put_int_at(&o, "ny", i, s->credNameY[i]);
    foh_view_put_int_at(&o, "nxv", i, s->credNameXVal[i]);
    foh_view_put_int_at(&o, "nxm", i, s->credNameXMax[i]);
    foh_view_put_int_at(&o, "nxd", i, s->credNameXDir[i]);
  }
  // The starfield and the shots in flight. Decoration — nobody would have
  // built this mechanism for them — but free once it exists, and a starfield
  // that jumps on resume is a visible seam where there does not need to be
  // one.
  for (int i = 0; i < FOH_CRED_STARS; i++) {
    foh_view_put_double_at(&o, "sx", i, s->credStar[i].x);
    foh_view_put_double_at(&o, "sy", i, s->credStar[i].y);
    foh_view_put_double_at(&o, "sdx", i, s->credStar[i].dx);
    foh_view_put_double_at(&o, "sdy", i, s->credStar[i].dy);
    foh_view_put_int_at(&o, "slife", i, s->credStar[i].life);
  }
  for (int i = 0; i < FOH_CRED_SHOTS; i++) {
    const FohCredShot *c = &s->credShot[i];
    foh_view_put_int_at(&o, "clive", i, c->live ? 1 : 0);
    foh_view_put_int_at(&o, "clife", i, c->life);
    foh_view_put_double_at(&o, "cx", i, c->x);
    foh_view_put_double_at(&o, "cy", i, c->y);
    foh_view_put_double_at(&o, "clx", i, c->lx);
    foh_view_put_double_at(&o, "cly", i, c->ly);
    foh_view_put_double_at(&o, "cl2x", i, c->l2x);
    foh_view_put_double_at(&o, "cl2y", i, c->l2y);
    foh_view_put_double_at(&o, "ctx", i, c->tx);
    foh_view_put_double_at(&o, "cty", i, c->ty);
    foh_view_put_double_at(&o, "cvel", i, c->vel);
    foh_view_put_double_at(&o, "csx", i, c->sx);
    foh_view_put_double_at(&o, "csy", i, c->sy);
  }
  // The FOH-local random stream (D38). Carried so the stars that spawn AFTER
  // the resume continue the sequence instead of restarting it — the same
  // reason the stars themselves are here.
  foh_view_put_int(&o, "rng", (int)(s->credRng & 0x7FFFFFFFu));
  const char *why = 0;
  if (!foh_view_publish(&o, CV_NAME, &why)) {
    // NOT fatal. A resume to the credits with a fresh starfield is worth
    // having; refusing the resume because a decoration could not be written
    // is not. The player's score is in this file too, which is why the
    // failure is SAID rather than swallowed.
    fprintf(stderr, "foh_credits: view NOT kept (%s)\n", why ? why : "?");
  }
}

bool foh_credits_view_load(FohState *s) {
  static FohViewIn in;
  const char *why = 0;
  bool ok = foh_view_load(&in, CV_NAME, CV_MAGIC, &why);
  if (ok) {
    // Parsed into a SCRATCH copy: a refusal half way down must not leave the
    // screen holding half of a bad file.
    static FohState tmp;
    tmp = *s;
#define CV_RI(f, lo, hi) tmp.f = foh_view_get_int(&in, #f, lo, hi);
#define CV_RB(f) tmp.f = foh_view_get_int(&in, #f, 0, 1) != 0;
#define CV_RD(f) tmp.f = foh_view_get_double(&in, #f);
    CV_SCALARS(CV_RI, CV_RB, CV_RD)
#undef CV_RI
#undef CV_RB
#undef CV_RD
    for (int i = 0; i < FOH_CRED_NAMES; i++) {
      tmp.credNameShot[i] = foh_view_get_int_at(&in, "nshot", i, 0, 1) != 0;
      tmp.credNameRender[i] = foh_view_get_int_at(&in, "nrender", i, 0, 1) != 0;
      tmp.credNameX[i] = foh_view_get_int_at(&in, "nx", i, -32768, 32767);
      tmp.credNameY[i] = foh_view_get_int_at(&in, "ny", i, -32768, 32767);
      tmp.credNameXVal[i] = foh_view_get_int_at(&in, "nxv", i, -32768, 32767);
      tmp.credNameXMax[i] = foh_view_get_int_at(&in, "nxm", i, -32768, 32767);
      tmp.credNameXDir[i] = foh_view_get_int_at(&in, "nxd", i, -1, 1);
    }
    for (int i = 0; i < FOH_CRED_STARS; i++) {
      tmp.credStar[i].x = foh_view_get_double_at(&in, "sx", i);
      tmp.credStar[i].y = foh_view_get_double_at(&in, "sy", i);
      tmp.credStar[i].dx = foh_view_get_double_at(&in, "sdx", i);
      tmp.credStar[i].dy = foh_view_get_double_at(&in, "sdy", i);
      tmp.credStar[i].life = foh_view_get_int_at(&in, "slife", i, -4096, 4096);
    }
    for (int i = 0; i < FOH_CRED_SHOTS; i++) {
      FohCredShot *c = &tmp.credShot[i];
      c->live = foh_view_get_int_at(&in, "clive", i, 0, 1) != 0;
      c->life = foh_view_get_int_at(&in, "clife", i, -4096, 4096);
      c->x = foh_view_get_double_at(&in, "cx", i);
      c->y = foh_view_get_double_at(&in, "cy", i);
      c->lx = foh_view_get_double_at(&in, "clx", i);
      c->ly = foh_view_get_double_at(&in, "cly", i);
      c->l2x = foh_view_get_double_at(&in, "cl2x", i);
      c->l2y = foh_view_get_double_at(&in, "cl2y", i);
      c->tx = foh_view_get_double_at(&in, "ctx", i);
      c->ty = foh_view_get_double_at(&in, "cty", i);
      c->vel = foh_view_get_double_at(&in, "cvel", i);
      c->sx = foh_view_get_double_at(&in, "csx", i);
      c->sy = foh_view_get_double_at(&in, "csy", i);
    }
    tmp.credRng = (uint32_t)foh_view_get_int(&in, "rng", 0, 0x7FFFFFFF);
    ok = foh_view_ok(&in, &why);
    if (ok) *s = tmp; // commit, only now
  }
  if (!ok) {
    fprintf(stderr, "foh_credits: view not restored (%s)\n", why ? why : "?");
  }
  // CONSUMED either way: it means "where you were when the lid closed", and an
  // unreadable one left behind would retry the same failure on every boot.
  foh_view_consume(CV_NAME);
  return ok;
}
