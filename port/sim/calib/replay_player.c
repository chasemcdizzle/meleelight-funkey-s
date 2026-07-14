// replay_player.c — M2 task 2 replay driver: proves the C player value
// model (ml_player.h) represents EVERY captured post-update(i) player
// snapshot bit-exactly. For each record of a player-spec capture
// (port/sim/calib/build/<id>.player.jsonl, FORMAT.md spec "player"):
//
//   1. ROUND-TRIP: parse the post-state canon -> strict marshal into
//      MlPlayer (rule 7: anything outside the captured domain aborts) ->
//      serialize back to canon -> byte-compare against the recorded
//      post-state. A single differing bit anywhere is a divergence.
//   2. COPY INDEPENDENCE: ml_player_copy the struct, SCRIBBLE the
//      original, serialize the copy -> must still match (the
//      type-specialized deepCopy is deep, not aliased).
//   3. MERGE PROPERTY: ml_hitboxes_merge_from(prevFrameHitboxes',
//      hitboxes) must equal the source field-for-field EXCEPT the
//      target's own `frames` persisting when the source lacks it —
//      the executed semantics of upstream's 3-arg deepObjectMerge call
//      (physics.js:1070; see ml_player.h). Exercised on every record's
//      real value pair; live in-frame aliasing is task 5's surface.
//
// Usage: replay_player <capture.jsonl> [--strict] [--max-print N]
//                      [--stop-first]
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "canon.h"
#include "player_canon.h"

static const char *g_file = "?";
static long g_lineno = 0;

void pc_fail(const char *msg) {
  fprintf(stderr, "MARSHAL FAIL %s:%ld: %s\n", g_file, g_lineno, msg);
  exit(3);
}

static void report_div(const char *kind, const char *frame, long lineno,
                       const char *want, const char *got) {
  size_t d = 0;
  while (want[d] && got[d] && want[d] == got[d]) d++;
  fprintf(stderr,
          "DIVERGENCE (%s) line %ld frame %s (first diff at byte %zu)\n"
          "  expected: ...%.120s\n"
          "  got:      ...%.120s\n",
          kind, lineno, frame, d,
          want + (d > 40 ? d - 40 : 0), got + (d > 40 ? d - 40 : 0));
}

int main(int argc, char **argv) {
  const char *path = NULL;
  bool strict = false, stop_first = false;
  int max_print = 5;
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--strict") == 0) strict = true;
    else if (strcmp(argv[i], "--stop-first") == 0) stop_first = true;
    else if (strcmp(argv[i], "--max-print") == 0 && i + 1 < argc) max_print = atoi(argv[++i]);
    else if (!path) path = argv[i];
    else { fprintf(stderr, "unknown arg: %s\n", argv[i]); return 1; }
  }
  if (!path) {
    fprintf(stderr, "usage: replay_player <capture.jsonl> [--strict] "
                    "[--max-print N] [--stop-first]\n");
    return 1;
  }
  FILE *f = fopen(path, "r");
  if (!f) { fprintf(stderr, "cannot open %s\n", path); return 1; }
  g_file = path;

  char *line = NULL;
  size_t linecap = 0;
  long replayed = 0, divergences = 0, printed = 0;
  long merge_checked = 0, merge_retention = 0;
  long first_div_line = -1;

  static MlPlayer p, cp;
  static MlHitboxes merged, expect_hb;
  CanonBuf out, out2;
  cb_init(&out);
  cb_init(&out2);

  ssize_t n;
  while ((n = getline(&line, &linecap, f)) > 0) {
    g_lineno++;
    if (line[n - 1] == '\n') line[n - 1] = 0;
    if (line[0] == 0) continue;

    // <frame> \t physics \t <args> \t <ret> \t <post>
    char *tab1 = strchr(line, '\t');
    char *tab2 = tab1 ? strchr(tab1 + 1, '\t') : NULL;
    char *tab3 = tab2 ? strchr(tab2 + 1, '\t') : NULL;
    char *tab4 = tab3 ? strchr(tab3 + 1, '\t') : NULL;
    if (!tab1 || !tab2 || !tab3 || !tab4) pc_fail("malformed record (need 5 tab fields)");
    *tab1 = 0; *tab2 = 0; *tab3 = 0; *tab4 = 0;
    const char *frame = line;
    const char *fn = tab1 + 1;
    const char *args_s = tab2 + 1;
    const char *ret_s = tab3 + 1;
    const char *post_s = tab4 + 1;

    if (strcmp(fn, "physics") != 0) pc_fail("unknown function name in record");
    if (strcmp(ret_s, "undef") != 0) pc_fail("physics is void: expected ret undef");

    canon_arena_reset();
    const char *err = NULL;
    const CanonVal *args = canon_parse(args_s, &err);
    if (!args) { fprintf(stderr, "PARSE FAIL %s:%ld args: %s\n", path, g_lineno, err); return 3; }
    if (args->type != CV_ARR || args->count != 1 || args->items[0]->type != CV_NUM) {
      pc_fail("expected args [slotIndex]");
    }
    const CanonVal *post = canon_parse(post_s, &err);
    if (!post) { fprintf(stderr, "PARSE FAIL %s:%ld post: %s\n", path, g_lineno, err); return 3; }

    // 1. round-trip
    cv_player(post, &p);
    out.len = 0;
    out.buf[0] = 0;
    ser_player(&out, &p);
    replayed++;
    bool diverged = false;
    if (strcmp(out.buf, post_s) != 0) {
      diverged = true;
      if (printed < max_print) { printed++; report_div("roundtrip", frame, g_lineno, post_s, out.buf); }
    }

    // 2. copy independence: copy, scribble the original, re-serialize copy
    if (!diverged) {
      ml_player_copy(&cp, &p);
      memset(&p, 0xAB, sizeof p);
      out.len = 0;
      out.buf[0] = 0;
      ser_player(&out, &cp);
      if (strcmp(out.buf, post_s) != 0) {
        diverged = true;
        if (printed < max_print) { printed++; report_div("deep-copy", frame, g_lineno, post_s, out.buf); }
      }
    }

    // 3. merge property (executed deepObjectMerge semantics, ml_player.h)
    if (!diverged) {
      ml_hitboxes_copy(&merged, &cp.phys.prevFrameHitboxes);
      ml_hitboxes_merge_from(&merged, &cp.hitboxes);
      ml_hitboxes_copy(&expect_hb, &cp.hitboxes);
      if (!cp.hitboxes.hasFrames && cp.phys.prevFrameHitboxes.hasFrames) {
        expect_hb.hasFrames = true;
        expect_hb.frames = cp.phys.prevFrameHitboxes.frames;
        merge_retention++;
      }
      out.len = 0;
      out.buf[0] = 0;
      ser_hitboxes(&out, &merged);
      out2.len = 0;
      out2.buf[0] = 0;
      ser_hitboxes(&out2, &expect_hb);
      merge_checked++;
      if (strcmp(out.buf, out2.buf) != 0) {
        diverged = true;
        if (printed < max_print) { printed++; report_div("merge", frame, g_lineno, out2.buf, out.buf); }
      }
    }

    if (diverged) {
      divergences++;
      if (first_div_line == -1) first_div_line = g_lineno;
      if (stop_first) break;
    }
  }
  free(line);
  fclose(f);

  fprintf(stderr, "merge property checked on %ld records (%ld frames-retention cases)\n",
          merge_checked, merge_retention);
  printf("PLAYER MODEL REPLAY %ld records, %ld divergences", replayed, divergences);
  if (first_div_line != -1) printf(" (first at line %ld)", first_div_line);
  printf("\n");
  cb_free(&out);
  cb_free(&out2);
  if (strict && divergences > 0) return 2;
  return 0;
}
