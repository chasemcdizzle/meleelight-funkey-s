// foh_phantom_witness.c — BEHAVIORAL witness for the phantomThreshold engine
// value (Tier A+ round-20 MAJORs 1-3).
//
// WHY THIS EXISTS. check-foh-flows.sh leg [1p] asks a HARD RULE 5 question:
// "does any C site hand-type the CHECKSUM-SURFACE engine value
// phantomThreshold, and does every one of them still carry upstream's
// authored value?" Through round 20 that question was answered by READING the
// source with regexes, and codex defeated three successive readings in three
// successive rounds -- `.01`, then `+0.01` / `(0.01)`, then
// `0x1.47ae147ae147bp-7` / `0.01f` / `(double)0.01`, a named
// `static const double phantom_default = .01;`, and a comment wedged before
// the `=`. Each fix widened the regex and each round found another spelling.
//
// That arms race is unwinnable, and losing it is a CLASS defect, not three
// instances: a regex enumerates the spellings its author thought of, while C
// admits infinitely many spellings of one double. This lane has already made
// the same move three times this arc (the FRAME, FIELD-VALUE and SEPARATOR
// planes of check-judge-regression.sh) and the standing lesson is the same
// one: A BEHAVIORAL WITNESS, NOT A STATIC READ. Only the compiler can say
// what value the program uses, so this TU asks the compiler.
//
// WHAT IT PROVES. It CALLS each of the four initialisers that own a
// phantomThreshold site and reads the field back out of the initialised
// object, comparing the resulting double BIT-FOR-BIT against upstream's
// authored value (passed in argv[1], parsed by check-foh-flows.sh from the
// upstream `src/settings.js` bytes -- never retyped here). After compilation
// no spelling survives: a hex float, an `f` suffix, a cast, a macro, a named
// constant, a comment before the `=` and a plain literal are all the same
// bits by the time this reads them, and any value drift is a bit mismatch
// whatever the spelling.
//
// WHAT IT DOES NOT PROVE. It says nothing about which FILES assign the field
// or how many hand-type it -- that structural question stays with [1p]'s
// whole-tree sweep. The two are complementary: the sweep pins the SHAPE of
// the site set, this pins the VALUE every site actually produces.
//
// ROUND-21 M2 -- WHY EACH SITE IS READ THROUGH A POISONED SLOT. The first form
// of this witness read every value through a hard-coded field expression, and
// codex showed what that leaves open: changing site 3's read from `g_target` to
// `g_match` produced BYTE-IDENTICAL output, because all four objects
// legitimately carry the same double. tp_setup_target() was still CALLED, but
// nothing it produced was ever JUDGED -- a probe that cannot fail. That is this
// lane's standing class one level down: a plane's coverage must be a function
// of the RULE it claims to cover, not of which object its author happened to
// name.
//
// The fix makes the four objects DISTINGUISHABLE at read time:
//   1. every slot is POISONED with a per-site "never initialised" sentinel
//      before any initialiser runs, and
//   2. every slot is STAMPED with a per-site "already measured" sentinel the
//      instant its value is taken.
// A read aimed at another site's object therefore yields that site's sentinel
// instead of the authored value: pointing site 3 at g_match reads SPENT(2) and
// pointing site 0 at g_target reads UNSET(3), and both are reported by NAME.
// The per-site rows printed below pin the same property from the outside: the
// leg cross-binds them against [1p]'s pinned literal-site set, so a duplicated
// or missing setup path is a changed output, never a no-op.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "foh.h"
#include "foh_persist.h"
#include "sim/sim.h"
#include "ml_events.h"   // ml_active_rng
#include "ml_rng.h"
#include "target/target_play.h"

// Objects are static, not automatic: GameState is far larger than a default
// thread stack.
static FohState  g_foh;
static FohPersist g_persist;
static GameState g_match;
static GameState g_target;

// raster.h:114 declares gfx_fatal host-provided; foh.c calls it on an invalid
// screen/token. Same posture as foh_snd_witness.c: a domain slip is a loud
// non-zero exit, never a limp-on that could let this witness "pass" without
// having initialised anything.
void gfx_fatal(const char *what) {
  fprintf(stderr, "foh_phantom_witness: gfx_fatal: %s\n", what);
  exit(3);
}

static unsigned long long bits_of(double d)
{
  unsigned long long u;
  memcpy(&u, &d, sizeof u);
  return u;
}

// Every site is named by the FUNCTION that produces it, because that is what
// this witness actually executes, and its value is read through a SLOT POINTER
// aimed at the object that function initialises (see the round-21 M2 note).
struct site {
  const char *name;
  double     *slot;
  double      got;
};

#define PHW_NSITES 4

// Sentinels. Both families are far outside any plausible phantomThreshold and
// are per-site, so a mis-aimed read names the site whose object it hit.
static double phw_unset(int i) { return -1000.0 - (double)i; }  /* not yet initialised */
static double phw_spent(int i) { return -2000.0 - (double)i; }  /* already measured    */

// Take site i's value and immediately stamp its slot, so any LATER read aimed
// at this object reads SPENT(i) instead of the authored value.
static void phw_take(struct site *s, int i)
{
  s->got  = *s->slot;
  *s->slot = phw_spent(i);
}

int main(int argc, char **argv)
{
  struct site sites[PHW_NSITES];
  unsigned long long want_bits;
  double want;
  char *end;
  int i, j, bad = 0;

  if (argc != 2) {
    fprintf(stderr, "usage: foh_phantom_witness <upstream-authored-value>\n");
    return 2;
  }
  end = NULL;
  want = strtod(argv[1], &end);
  if (end == NULL || *end != '\0' || end == argv[1]) {
    fprintf(stderr, "phantom witness: argv[1] '%s' is not a parseable "
                    "decimal value\n", argv[1]);
    return 2;
  }
  want_bits = bits_of(want);

  // The four owning initialisers and the slot each one writes. Declared BEFORE
  // anything is called so that every slot can be poisoned first.
  sites[0].name = "foh_init";
  sites[0].slot = &g_foh.phantomThreshold;
  sites[1].name = "foh_persist_defaults";
  sites[1].slot = &g_persist.phantomThreshold;
  sites[2].name = "sim_setup_match";
  sites[2].slot = &g_match.sim.phantomThreshold;
  sites[3].name = "tp_setup_target";
  sites[3].slot = &g_target.sim.phantomThreshold;

  for (i = 0; i < PHW_NSITES; i++) {
    for (j = 0; j < i; j++) {
      if (sites[j].slot == sites[i].slot) {
        fprintf(stderr,
                "phantom witness: sites %d (%s) and %d (%s) read the SAME "
                "slot -- one owning initialiser would go unjudged\n",
                j, sites[j].name, i, sites[i].name);
        return 2;
      }
    }
    *sites[i].slot = phw_unset(i);
  }

  // The four owning initialisers, CALLED. Arguments are ordinary in-domain
  // values: the field under test is a settings default, not a function of
  // these, and the witness fails loudly if that ever stops being true.
  foh_init(&g_foh);
  phw_take(&sites[0], 0);

  foh_persist_defaults(&g_persist);
  phw_take(&sites[1], 1);

  // sim_setup_match consumes the ONE seeded background draw, so an RNG must
  // be ARMED first (foh_app.c:692-695 does the same before its own call).
  // foh_app.c additionally burns ML_BOOT_DRAWS to align the launched stream
  // with the harness domain; that is deliberately NOT replicated here,
  // because this witness reads a SETTINGS DEFAULT, not the stream, and
  // copying the boot-draw constant would put a second unpinned copy of it in
  // the tree for no gain. The seed is fixed only to keep the run
  // deterministic.
  ml_active_rng = &g_match.rng;
  ml_rng_seed(&g_match.rng, 1337u);
  sim_setup_match(&g_match, 2, 0, 0, 1, 0);
  phw_take(&sites[2], 2);

  ml_active_rng = &g_target.rng;
  ml_rng_seed(&g_target.rng, 1337u);
  tp_setup_target(&g_target, 2, 0);
  phw_take(&sites[3], 3);

  for (i = 0; i < PHW_NSITES; i++) {
    unsigned long long b = bits_of(sites[i].got);
    if (b == want_bits)
      continue;
    bad = 1;
    // A sentinel read means the SLOT was wrong, not the value: report which
    // site's object was actually hit, because "unjudged path" and "drifted
    // value" need different repairs.
    for (j = 0; j < PHW_NSITES; j++) {
      if (b == bits_of(phw_unset(j))) {
        if (i == j)
          fprintf(stderr,
                  "phantom witness: site %d (%s) still holds its UNSET poison "
                  "-- %s() did not write phantomThreshold (it was not called, "
                  "or it stopped owning this value)\n",
                  i, sites[i].name, sites[i].name);
        else
          fprintf(stderr,
                  "phantom witness: site %d (%s) read the slot of site %d "
                  "(%s), which had not been initialised yet -- %s()'s own "
                  "output is UNJUDGED (round-21 M2 class)\n",
                  i, sites[i].name, j, sites[j].name, sites[i].name);
        break;
      }
      if (b == bits_of(phw_spent(j))) {
        fprintf(stderr,
                "phantom witness: site %d (%s) read the slot of site %d (%s), "
                "already measured -- %s()'s own output is UNJUDGED "
                "(round-21 M2 class)\n",
                i, sites[i].name, j, sites[j].name, sites[i].name);
        break;
      }
    }
    if (j < PHW_NSITES)
      continue;
    fprintf(stderr,
            "phantom witness: %s() produced phantomThreshold bits %016llx "
            "but upstream settings.js authors %s (bits %016llx) -- a "
            "CHECKSUM-SURFACE engine value has drifted (HARD RULE 5)\n",
            sites[i].name, b, argv[1], want_bits);
  }
  if (bad) return 1;

  // One LABELLED row per setup path (round-21 M2). The leg cross-binds these
  // labels against [1p]'s pinned literal-site set, so a duplicated, missing or
  // renamed path changes this output instead of vanishing into an identical
  // aggregate line.
  for (i = 0; i < PHW_NSITES; i++)
    printf("PHANTOM SITE %d %s bits=%016llx\n",
           i, sites[i].name, bits_of(sites[i].got));
  printf("PHANTOM WITNESS OK (sites=%d value=%s bits=%016llx)\n",
         PHW_NSITES, argv[1], want_bits);
  return 0;
}
