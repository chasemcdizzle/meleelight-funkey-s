/* pipeline/lib/stages_check.c — round-trip validator for the generated
 * STAB1 VS-stage geometry tables (fix_plan §M1 task 3;
 * pipeline/FORMATS.md §4.5).
 *
 * Compiled (cc -ffp-contract=off) against a generated ml_stages.c and
 * prints the canonical leaf dump: one line per value, doubles as the 16
 * lowercase hex digits of their IEEE-754 bit pattern (reconstructed from
 * the compiled C data via ml_stage_f64 and re-extracted with memcpy — an
 * actual store/load round trip through a C double), ints as decimal. The
 * dump must be BYTE-IDENTICAL to a fresh executed-JS walk of the
 * extractor bundle (pipeline/lib/stages-dump.js); check-stages.sh cmp(1)s
 * the two.
 *
 * This file implements FORMATS.md §4, NOT the generator's source code —
 * a generator/spec drift shows up as a byte diff here.
 */

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "ml_stages.h"

/* Round-trip bits -> double -> bits through an actual C double. */
static uint64_t rt(uint64_t bits) {
  double d = ml_stage_f64(bits);
  uint64_t out;
  memcpy(&out, &d, 8);
  return out;
}

static void pvec(const char *sn, const char *field, int i,
                 const ml_stage_vec2b_t *p) {
  printf("stage/%s/%s[%d]=%016" PRIx64 ",%016" PRIx64 "\n",
         sn, field, i, rt(p->x), rt(p->y));
}

static void pconn(const char *sn, const char *tag, int i, int side,
                  const ml_stage_conn_label_t *lab) {
  if (lab->present) {
    printf("stage/%s/connected/%s[%d][%d]=%" PRId32 ",%" PRId32 "\n",
           sn, tag, i, side, lab->type, lab->index);
  } else {
    printf("stage/%s/connected/%s[%d][%d]=-\n", sn, tag, i, side);
  }
}

int main(void) {
  for (int c = 0; c < ML_STAGE_COUNT; c++) {
    const ml_stage_t *s = &ml_stages[c];
    const char *sn = s->name;

    for (int r = 0; r < s->polygonCount; r++) {
      for (int v = 0; v < s->polygonVertCounts[r]; v++) {
        printf("stage/%s/polygon/%d/%d=%016" PRIx64 ",%016" PRIx64 "\n",
               sn, r, v, rt(s->polygons[r][v].x), rt(s->polygons[r][v].y));
      }
    }

#define PSURF(kind)                                                     \
    for (int i = 0; i < s->kind##Count; i++) {                          \
      const ml_stage_surface_t *sf = &s->kind[i];                       \
      printf("stage/%s/" #kind "[%d]=%016" PRIx64 ",%016" PRIx64        \
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
      printf("stage/%s/ledge[%d]=%" PRId32 ",%" PRId32 ",%" PRId32 "\n",
             sn, i, s->ledge[i].type, s->ledge[i].index, s->ledge[i].side);
    }
    for (int i = 0; i < s->ledgeCount; i++) {
      pvec(sn, "ledgePos", i, &s->ledgePos[i]);
    }

    for (int i = 0; i < ML_STAGE_PLAYERS; i++) {
      pvec(sn, "startingPoint", i, &s->startingPoint[i]);
    }
    for (int i = 0; i < ML_STAGE_PLAYERS; i++) {
      printf("stage/%s/startingFace[%d]=%" PRId32 "\n", sn, i, s->startingFace[i]);
    }
    for (int i = 0; i < ML_STAGE_PLAYERS; i++) {
      pvec(sn, "respawnPoints", i, &s->respawnPoints[i]);
    }
    for (int i = 0; i < ML_STAGE_PLAYERS; i++) {
      printf("stage/%s/respawnFace[%d]=%" PRId32 "\n", sn, i, s->respawnFace[i]);
    }

    printf("stage/%s/blastzone=%016" PRIx64 ",%016" PRIx64
           ",%016" PRIx64 ",%016" PRIx64 "\n", sn,
           rt(s->blastzone[0]), rt(s->blastzone[1]),
           rt(s->blastzone[2]), rt(s->blastzone[3]));
    printf("stage/%s/scale=%016" PRIx64 "\n", sn, rt(s->scale));
    printf("stage/%s/offset=%" PRId32 ",%" PRId32 "\n",
           sn, s->offset[0], s->offset[1]);

    printf("stage/%s/hasConnected=%d\n", sn, (int)s->hasConnected);
    if (s->hasConnected) {
      for (int i = 0; i < s->groundCount; i++) {
        pconn(sn, "g", i, 0, &s->connectedGround[i].l);
        pconn(sn, "g", i, 1, &s->connectedGround[i].r);
      }
      for (int i = 0; i < s->platformCount; i++) {
        pconn(sn, "p", i, 0, &s->connectedPlatform[i].l);
        pconn(sn, "p", i, 1, &s->connectedPlatform[i].r);
      }
    }

    for (int i = 0; i < s->movingPlatCount; i++) {
      printf("stage/%s/movingPlats[%d]=%" PRId32 "\n", sn, i, s->movingPlats[i]);
    }
  }
  return 0;
}
