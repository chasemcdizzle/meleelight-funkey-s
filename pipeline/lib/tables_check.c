/* pipeline/lib/tables_check.c — round-trip validator for the generated
 * CTAB1 engine tables (fix_plan §M1 task 2; pipeline/FORMATS.md §3.6).
 *
 * Compiled (cc -ffp-contract=off) against a generated ml_tables.c and
 * prints the canonical leaf dump: one line per value, doubles as the 16
 * lowercase hex digits of their IEEE-754 bit pattern (reconstructed from
 * the compiled C data via ml_f64 and re-extracted with memcpy — an actual
 * store/load round trip through a C double), ints as decimal. The dump
 * must be BYTE-IDENTICAL to a fresh executed-JS walk of the extractor
 * bundle (pipeline/lib/tables-dump.js); check-tables.sh cmp(1)s the two.
 *
 * This file implements FORMATS.md §3, NOT the generator's source code:
 * field sets/orders come from the ML_*_FIELDS X-macros in the generated
 * header, so a generator/spec drift shows up as a byte diff here.
 */

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "ml_tables.h"

/* Round-trip bits -> double -> bits through an actual C double. */
static uint64_t rt(uint64_t bits) {
  double d = ml_f64(bits);
  uint64_t out;
  memcpy(&out, &d, 8);
  return out;
}

int main(void) {
  for (int c = 0; c < ML_CHARS; c++) {
    const char *cn = ml_char_names[c];
    const ml_attributes_t *a = &ml_attributes[c];

#define PF(f) printf("attr/%s/%s=%016" PRIx64 "\n", cn, #f, rt(a->f));
    ML_ATTR_F64_FIELDS(PF)
#undef PF
#define PI(f) printf("attr/%s/%s=%" PRId32 "\n", cn, #f, a->f);
    ML_ATTR_I32_FIELDS(PI)
#undef PI
#define PV(f, n)                                                     \
    for (int i = 0; i < (n); i++) {                                  \
      printf("attr/%s/%s[%d]=%" PRId32 "\n", cn, #f, i, a->f[i]);   \
    }
    ML_ATTR_I32V_FIELDS(PV)
#undef PV
#define PB(f) printf("attr/%s/%s=%d\n", cn, #f, (int)a->f);
    ML_ATTR_BOOL_FIELDS(PB)
#undef PB

    for (int i = 0; i < ml_frames_count[c]; i++) {
      const ml_frames_entry_t *e = &ml_frames_data[c][i];
      printf("frames/%s/%s=%" PRId32 "\n", cn, e->name, e->frames);
    }

    for (int i = 0; i < ml_intang_count[c]; i++) {
      const ml_intang_entry_t *e = &ml_intang_data[c][i];
      printf("intang/%s/%s=%" PRId32 ",%" PRId32 "\n",
             cn, e->name, e->start, e->length);
    }

    for (int i = 0; i < ml_ecb_state_count[c]; i++) {
      const ml_ecb_state_t *s = &ml_ecb_states[c][i];
      for (int f = 0; f < s->frameCount; f++) {
        const ml_ecb_frame_t *fr = &s->frames[f];
        printf("ecb/%s/%s/%d=%d,%d,%d,%d\n", cn, s->name, f,
               (int)fr->v[0], (int)fr->v[1], (int)fr->v[2], (int)fr->v[3]);
      }
    }

    for (int i = 0; i < ml_hitbox_move_count[c]; i++) {
      const ml_hitbox_move_t *m = &ml_hitbox_moves[c][i];
      for (int id = 0; id < 4; id++) {
        const ml_hitbox_t *h = m->id[id];
        if (h == NULL) continue;
        printf("hb/%s/%s/id%d/offsetIsArray=%d\n",
               cn, m->name, id, (int)h->offsetIsArray);
        for (int k = 0; k < h->offsetCount; k++) {
          printf("hb/%s/%s/id%d/offset[%d]=%016" PRIx64 ",%016" PRIx64 "\n",
                 cn, m->name, id, k, rt(h->offset[k].x), rt(h->offset[k].y));
        }
#define PF(f)                                                        \
        printf("hb/%s/%s/id%d/%s=%016" PRIx64 "\n",                  \
               cn, m->name, id, #f, rt(h->f));
        ML_HB_F64_FIELDS(PF)
#undef PF
#define PI(f)                                                        \
        printf("hb/%s/%s/id%d/%s=%" PRId32 "\n", cn, m->name, id, #f, h->f);
        ML_HB_I32_FIELDS(PI)
#undef PI
        printf("hb/%s/%s/id%d/throwextra=%d\n",
               cn, m->name, id, (int)h->throwextra);
      }
    }
  }
  return 0;
}
