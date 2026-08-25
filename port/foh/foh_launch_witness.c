// foh_launch_witness.c — the CSS LAUNCH-GUARD grid witness (codex review
// round 17/18 finding B1). A check-owned host leg for check-foh-flows.sh,
// built and linked exactly like foh_snd_witness.c (same [2] objects, same
// -Wl,-dead_strip link recipe).
//
// WHY. foh.c's CSS START arm carries the launch-plane refusal. A44 replaced
// the two-port form with three conditions — port 0 must be HMN, no port
// above 1 may be CPU, and at least two ports must participate — but the
// reason for a witness is unchanged and is now stronger.
//
// Every committed flow drives the ONE configuration the shipped menu can
// actually reach (P1 HMN, P2 HMN or CPU), so the refusing side of that
// predicate had NO behavioral coverage at all: the arm could be deleted,
// inverted, or narrowed and every green flow would stay green. A static
// read of the condition is not coverage either — it is the same expression
// the code already contains, so restating it here would prove only that a
// string equals itself.
//
// WHAT THIS PROVES. The table below is AUTHORED — one cell per point of the
// FULL FOUR-PORT grid over {-1, 0, 1}^4, all 81 of them (A44; it was 3x3
// while the FOH had two ports). -1 = N/A, 0 = HMN, 1 = CPU. Each cell states
// by hand what the port configuration MEANS and therefore what the machine
// owes: either a launch (menuForward + a css->sss transition caused by
// "start") or a refusal (deny + a `refused portconfig` selection event, with
// the screen NOT moving). Nothing in a cell is computed from the predicate
// under test, so narrowing or widening the guard moves cells and this
// witness fails.
//
// THE CPU CELLS ON PORTS 2/3 ARE THE POINT OF THE WIDENING. DEVIATION
// D40(b) keeps CPU off those ports' type CYCLE, so the widget cannot reach
// them — but "the widget cannot produce it" is not the same claim as "the
// launch plane refuses it", and it is the second one that matters: the sim
// can replay exactly one AI slot (A46's OPEN/OWED), so a CPU P3 that ever
// reached the launch would boot a match no golden could verify. This grid
// drives those cells by writing the type plane DIRECTLY, which is the only
// way to test a refusal whose usual producer is another refusal.
//
// The grid is driven through the REAL foh_tick with cssReady forced true.
// Forcing it is the point: the ready rule and the launch guard are two
// different gates (foh.c's own note — cssReady is LAST frame's draw-pass
// value, which is exactly why the N/A race reaches the guard at all), and
// this witness is about the second one. A cell that the ready rule would
// also have stopped is still a cell the guard must refuse on its own.
//
// Usage: foh_launch_witness   (no arguments)
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "foh.h"

// Host-provided fatals (raster.h / the sim slice), same stubs as the
// sound witness: a fatal here is a loud non-zero exit, never a pass.
void gfx_fatal(const char *what) {
  fprintf(stderr, "foh_launch_witness: gfx_fatal: %s\n", what);
  exit(3);
}
void sim_fatal(const char *what);
void sim_fatal(const char *what) {
  fprintf(stderr, "foh_launch_witness: sim_fatal: %s\n", what);
  exit(3);
}

static int g_fails;
static void bad(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
static void bad(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  fprintf(stderr, "foh_launch_witness: FAIL: ");
  vfprintf(stderr, fmt, ap);
  fprintf(stderr, "\n");
  va_end(ap);
  g_fails++;
}

// Join the tick's emitted sound tokens, so a mismatch prints both sides.
static void join_snd(const FohState *s, char *out, size_t cap) {
  out[0] = 0;
  for (int i = 0; i < s->nsnd; i++) {
    if (i) strncat(out, ",", cap - strlen(out) - 1);
    strncat(out, s->snd[i], cap - strlen(out) - 1);
  }
}

// The authored verdicts. One character per cell: 'L' = this configuration
// owes a launch, 'R' = it owes a refusal. The layout is fixed and is stated
// here rather than derived:
//
//   OUTER index  = (p1Type + 1) * 3 + (p2Type + 1)   -> one row below
//   INNER index  = (p3Type + 1) * 3 + (p4Type + 1)   -> one char in that row
//
// so each row is the 3x3 (p3,p4) sub-grid for one (p1,p2) pair, read
// p3 = -1,0,1 in groups of three and p4 = -1,0,1 within each group.
//
// WHY EACH ROW READS AS IT DOES — the claims, in words, one per row:
//   * every row with p1Type != 0 is ALL REFUSALS. Port 0 is the port the
//     physical controller drives; a CPU or absent port 0 would boot a match
//     with no input source and make the LAUNCH record a lie (HARD RULE 2).
//     That is rows 0-2 (p1 = N/A) and rows 6-8 (p1 = CPU), whatever the
//     other three ports hold.
//   * inside the three p1 = HMN rows, a cell refuses if EITHER a port above
//     1 is CPU (the third of each group of three, and the whole last group
//     of three, are p4 = CPU and p3 = CPU respectively) OR fewer than two
//     ports participate. The second clause is upstream's own readyToFight
//     rule (css.js:1167-1181) re-checked at the seam, and it is why row 3's
//     first cell is 'R': P1 alone, with P2, P3 and P4 all absent, is a
//     one-port match, and it reaches the guard only through upstream's own
//     one-frame draw-pass race.
//   * row 3 (p1 HMN, p2 N/A) differs from rows 4 and 5 in exactly one cell,
//     its first — with P2 present the pair is already made, so P3 and P4 may
//     both be absent. That single-cell difference is the whole content of
//     "P1 + P3 with P2 off is a legal match", which is the configuration
//     A44 exists to add and which the pre-A44 guard refused by construction.
static const char *const kVerdicts[9] = {
    "RRRRRRRRR", // p1 N/A, p2 N/A   — no human port 0
    "RRRRRRRRR", // p1 N/A, p2 HMN   — port 0 must be the human
    "RRRRRRRRR", // p1 N/A, p2 CPU   — still no human port 0
    "RLRLLRRRR", // p1 HMN, p2 N/A   — needs a second port from 2/3, no CPU there
    "LLRLLRRRR", // p1 HMN, p2 HMN   — the pair is made; 2/3 optional, never CPU
    "LLRLLRRRR", // p1 HMN, p2 CPU   — the sim carries p2Type + difficulty
    "RRRRRRRRR", // p1 CPU, p2 N/A   — the sim would boot a HUMAN P1 (a lie)
    "RRRRRRRRR", // p1 CPU, p2 HMN   — the port roles are swapped
    "RRRRRRRRR", // p1 CPU, p2 CPU   — no human port at all
};

int main(void) {
  int nLaunch = 0, nRefuse = 0;
  char got[128];
  // The verdict table's own shape, checked before it is trusted: nine rows
  // of nine characters over the two-letter alphabet. A typo'd row would
  // otherwise silently authorise whatever the machine happened to do.
  for (int o = 0; o < 9; o++) {
    if (strlen(kVerdicts[o]) != 9) {
      bad("authored row %d is %zu chars, want 9", o, strlen(kVerdicts[o]));
    }
    for (int q = 0; kVerdicts[o][q]; q++) {
      if (kVerdicts[o][q] != 'L' && kVerdicts[o][q] != 'R') {
        bad("authored row %d char %d is '%c', want 'L' or 'R'", o, q,
            kVerdicts[o][q]);
      }
    }
  }
  if (g_fails) {
    fprintf(stderr, "foh_launch_witness: %d failure(s)\n", g_fails);
    return 1;
  }

  for (int p1 = -1; p1 <= 1; p1++) {
    for (int p2 = -1; p2 <= 1; p2++) {
      for (int p3 = -1; p3 <= 1; p3++) {
        for (int p4 = -1; p4 <= 1; p4++) {
          const int outer = (p1 + 1) * 3 + (p2 + 1);
          const int inner = (p3 + 1) * 3 + (p4 + 1);
          const int wantLaunch = kVerdicts[outer][inner] == 'L';
          const char *wantSnd = wantLaunch ? "menuForward" : "deny";
          FohState s;
          foh_init(&s);
          s.screen = FOH_CSS;
          // The launch guard is what is under test, not the ready rule:
          // force the gate in front of it open so every cell reaches it.
          s.cssReady = true;
          s.portType[0] = p1;
          s.portType[1] = p2;
          s.portType[2] = p3;
          s.portType[3] = p4;
          memset(&s.prev, 0, sizeof s.prev); // released -> START rising edge
          PlatformInput in;
          memset(&in, 0, sizeof in);
          in.start = true;
          foh_tick(&s, &in);

          join_snd(&s, got, sizeof got);
          if (strcmp(got, wantSnd) != 0) {
            bad("(p1=%d,p2=%d,p3=%d,p4=%d) sound plane is '%s', want '%s'",
                p1, p2, p3, p4, got, wantSnd);
          }
          if (s.nev != 1) {
            bad("(p1=%d,p2=%d,p3=%d,p4=%d) emitted %d events, want exactly 1",
                p1, p2, p3, p4, s.nev);
            continue;
          }
          const FohEvent *e = &s.ev[0];
          if (wantLaunch) {
            nLaunch++;
            if (e->kind != FOH_EV_TRANS || !e->from || !e->to || !e->cause ||
                strcmp(e->from, "css") != 0 || strcmp(e->to, "sss") != 0 ||
                strcmp(e->cause, "start") != 0) {
              bad("(p1=%d,p2=%d,p3=%d,p4=%d) want the css->sss 'start' "
                  "transition", p1, p2, p3, p4);
            }
            if (strcmp(foh_screen_token(s.screen), "sss") != 0) {
              bad("(p1=%d,p2=%d,p3=%d,p4=%d) screen is '%s', want 'sss'", p1,
                  p2, p3, p4, foh_screen_token(s.screen));
            }
          } else {
            nRefuse++;
            if (e->kind != FOH_EV_SEL || !e->field || !e->sval ||
                strcmp(e->field, "refused") != 0 ||
                strcmp(e->sval, "portconfig") != 0) {
              bad("(p1=%d,p2=%d,p3=%d,p4=%d) want the 'refused portconfig' "
                  "event", p1, p2, p3, p4);
            }
            // A refusal that still moved the screen would be a launch
            // wearing a deny sound — the screen is the load-bearing half.
            if (strcmp(foh_screen_token(s.screen), "css") != 0) {
              bad("(p1=%d,p2=%d,p3=%d,p4=%d) refused but the screen moved to "
                  "'%s'", p1, p2, p3, p4, foh_screen_token(s.screen));
            }
          }
        }
      }
    }
  }

  // Shape pins: the grid must stay the FULL {-1,0,1}^4, and it must keep
  // both sides. An all-refuse table (a guard inverted to refuse everything)
  // would satisfy every cell above only if the table said so — these pins
  // make the split itself a declared, checkable fact. 11 launching cells:
  // 4 under each of (p1 HMN, p2 HMN) and (p1 HMN, p2 CPU), plus 3 under
  // (p1 HMN, p2 N/A), which loses the all-absent cell to the two-port rule.
  if (nLaunch + nRefuse != 81) {
    bad("drove %d cells, want the full {-1,0,1}^4 grid", nLaunch + nRefuse);
  }
  if (nLaunch != 11) bad("%d launching cells, want 11", nLaunch);
  if (nRefuse != 70) bad("%d refusing cells, want 70", nRefuse);

  if (g_fails) {
    // Trailer grammar mirrors foh_snd_witness.c so the check side can parse
    // the WHOLE output, not just look for a substring.
    fprintf(stderr, "foh_launch_witness: %d failure(s)\n", g_fails);
    return 1;
  }
  printf("LAUNCH GUARD WITNESS OK (cells=81 launch=11 refuse=70)\n");
  return 0;
}
