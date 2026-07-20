/* pipeline/lib/targets_check.c — round-trip validator for the generated
 * TTAB1 target-test stage tables (fix_plan §M4 task 11;
 * pipeline/FORMATS.md §6).
 *
 * Compiled (cc -ffp-contract=off) against a generated ml_targets.c and
 * prints the canonical leaf dump: one line per value, doubles as the 16
 * lowercase hex digits of their IEEE-754 bit pattern (reconstructed from
 * the compiled C data via ml_target_f64 and re-extracted with memcpy — an
 * actual store/load round trip through a C double), ints as decimal. The
 * dump must be BYTE-IDENTICAL to a fresh executed-JS walk of the
 * extractor bundle (pipeline/lib/targets-dump.js); check-targets.sh
 * cmp(1)s the two.
 *
 * This file implements FORMATS.md §6, NOT the generator's source code —
 * a generator/spec drift shows up as a byte diff here.
 */

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "ml_targets.h"

/* Round-trip bits -> double -> bits through an actual C double. */
static uint64_t rt(uint64_t bits) {
  double d = ml_target_f64(bits);
  uint64_t out;
  memcpy(&out, &d, 8);
  return out;
}

static void pvec(const char *sn, const char *field, int i,
                 const ml_tstage_vec2b_t *p) {
  printf("tstage/%s/%s[%d]=%016" PRIx64 ",%016" PRIx64 "\n",
         sn, field, i, rt(p->x), rt(p->y));
}

int main(void) {
  for (int c = 0; c < ML_TSTAGE_COUNT; c++) {
    const ml_tstage_t *s = &ml_tstages[c];
    const char *sn = s->name;

    pvec(sn, "startingPoint", 0, &s->startingPoint);

    for (int i = 0; i < s->boxCount; i++) {
      const ml_tstage_box_t *b = &s->box[i];
      printf("tstage/%s/box[%d]=%016" PRIx64 ",%016" PRIx64
             ",%016" PRIx64 ",%016" PRIx64 "\n",
             sn, i, rt(b->minX), rt(b->minY), rt(b->maxX), rt(b->maxY));
    }

#define PSURF(kind)                                                     \
    for (int i = 0; i < s->kind##Count; i++) {                          \
      const ml_tstage_surface_t *sf = &s->kind[i];                      \
      printf("tstage/%s/" #kind "[%d]=%016" PRIx64 ",%016" PRIx64       \
             ",%016" PRIx64 ",%016" PRIx64 "\n",                        \
             sn, i, rt(sf->x1), rt(sf->y1), rt(sf->x2), rt(sf->y2));    \
    }
    PSURF(ground)
    PSURF(platform)
    PSURF(ceiling)
    PSURF(wallL)
    PSURF(wallR)
#undef PSURF

    for (int i = 0; i < s->ledgeCount; i++) {
      printf("tstage/%s/ledge[%d]=%" PRId32 ",%" PRId32 ",%" PRId32 "\n",
             sn, i, s->ledge[i].type, s->ledge[i].index, s->ledge[i].side);
    }

    printf("tstage/%s/hasLedgePos=%d\n", sn, (int)s->hasLedgePos);
    if (s->hasLedgePos) {
      for (int i = 0; i < s->ledgeCount; i++) {
        pvec(sn, "ledgePos", i, &s->ledgePos[i]);
      }
    }

    for (int i = 0; i < s->targetCount; i++) {
      pvec(sn, "target", i, &s->target[i]);
    }

    printf("tstage/%s/scale=%016" PRIx64 "\n", sn, rt(s->scale));
    printf("tstage/%s/blastzone=%016" PRIx64 ",%016" PRIx64
           ",%016" PRIx64 ",%016" PRIx64 "\n", sn,
           rt(s->blastzone[0]), rt(s->blastzone[1]),
           rt(s->blastzone[2]), rt(s->blastzone[3]));
    printf("tstage/%s/offset=%" PRId32 ",%" PRId32 "\n",
           sn, s->offset[0], s->offset[1]);
  }
  return 0;
}
