// replay_hitdet.c — M2 task 6 replay driver: feeds recorded hitdet-spec
// boundary calls (build/<id>.hitdet.jsonl, FORMAT.md "The hitdet spec")
// to the C translations (port/sim/hit_detection.c) and compares
// canon-v1.1 serializations byte-for-byte. A single differing bit
// anywhere is a divergence.
//
// Record types:
// - rngBoot (first record): seeds the ONE chained C mulberry32 and
//   fast-forwards it by the recorded boot-draw count.
// - Math.random: a seeded draw OUTSIDE any hitdet boundary (moves during
//   update, startGame) — burns one chain draw and compares. Eager
//   file-order processing is sound because Math.randomW (draws inside a
//   dispatch window BELOW a hitdet boundary — chain-order-ambiguous) is
//   pinned to ZERO records; encountering one here is a hard failure.
// - hdFlags (frame 0): loads the actionStates flag table (hd_flags seam);
//   drift-guarded in the capture by finalCheck().
// - dispatch: FIFO-queued seam records consumed by the following pipeline
//   record IN CALL ORDER — verify [phase, moveName, [slot, ...extras]],
//   then RESYNC players + alias probes + the hitQueue from the recorded
//   post {alias, hq, players} (moves are tasks 7-12).
// - getLaunchAngle / getHorizontal|VerticalVelocity / -Decay /
//   getKnockback / getHitstun / segmentSegmentCollision: PURE replays.
// - knockbackSounds: pure logic + sound events; post {rng, snd} compared.
// - hitDetect / executeHits / checkPhantoms: marshal the PRE envelope
//   {alias, characterSelections, gameMode, gameSettings, hq, phq,
//   playerType, players}, run the C pipeline fn, compare the
//   {alias, hq, phq, players, rng, snd} post envelope bit-exactly.
// - resetHitQueue / setPhantonQueue: lean queue-only records.
//
// Marshalling is STRICT (prevention rule 7): any shape outside the
// captured domain aborts with exit 3 — never guess.
//
// Usage: hitdet_replay <capture.jsonl> [--strict] [--max-print N]
//                      [--stop-first]
#include <inttypes.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../hit_detection.h"
#include "../ml_events.h"
#include "../ml_rng.h"
#include "canon.h"
#include "ml_tables.h" // ML_CHARS
#include "player_canon.h"

static const char *g_file = "?";
static long g_lineno = 0;

static void fail(const char *msg) {
  fprintf(stderr, "MARSHAL FAIL %s:%ld: %s\n", g_file, g_lineno, msg);
  exit(3);
}

void pc_fail(const char *msg) { fail(msg); }
void ml_canon_fail(const char *msg) { fail(msg); }
void ml_events_fail(const char *what) { fail(what); }
void ml_asshort_out_of_domain(const char *what) {
  fprintf(stderr, "OUT OF DOMAIN %s:%ld: %s\n", g_file, g_lineno, what);
  exit(3);
}
void ml_hd_out_of_domain(const char *what) {
  fprintf(stderr, "OUT OF DOMAIN %s:%ld: %s\n", g_file, g_lineno, what);
  exit(3);
}

// --- divergence accounting ------------------------------------------------------

static long g_divergences = 0;
static long g_first_div_line = -1;
static long g_printed = 0;
static int g_max_print = 5;
static jmp_buf g_rec_jmp; // seam mismatch aborts the current pipeline record

static void report_div(const char *kind, long lineno, const char *want,
                       const char *got) {
  g_divergences++;
  if (g_first_div_line == -1) g_first_div_line = lineno;
  if (g_printed < g_max_print) {
    g_printed++;
    size_t d = 0;
    while (want[d] && got[d] && want[d] == got[d]) d++;
    fprintf(stderr,
            "DIVERGENCE (%s) line %ld (first diff at byte %zu)\n"
            "  expected: ...%.160s\n"
            "  got:      ...%.160s\n",
            kind, lineno, d, want + (d > 60 ? d - 60 : 0),
            got + (d > 60 ? d - 60 : 0));
  }
}

// --- small marshal helpers -------------------------------------------------------

static double cv_num(const CanonVal *v) {
  if (v->type != CV_NUM) fail("expected number");
  return v->num;
}
static bool cv_bool(const CanonVal *v) {
  if (v->type != CV_BOOL) fail("expected boolean");
  return v->b;
}
// truthiness domain bool|undefined (article's `crouch`/`vCancel` flag reads
// pass the raw actionStates value — undefined for most states)
static bool cv_truthy_bu(const CanonVal *v) {
  if (v->type == CV_UNDEF) return false;
  if (v->type == CV_BOOL) return v->b;
  fail("expected boolean/undefined");
  return false;
}
static const char *cv_str(const CanonVal *v) {
  if (v->type != CV_STR) fail("expected string");
  return v->str;
}
static Vec2D cv_vec2(const CanonVal *v) {
  if (v->type != CV_OBJ || v->nkeys != 2 || strcmp(v->keys[0], "x") != 0 ||
      strcmp(v->keys[1], "y") != 0) {
    fail("expected {x,y}");
  }
  Vec2D out = {cv_num(v->vals[0]), cv_num(v->vals[1])};
  return out;
}
static const CanonVal *obj_get(const CanonVal *o, const char *key) {
  if (o->type != CV_OBJ) fail("expected object");
  for (int i = 0; i < o->nkeys; i++) {
    if (strcmp(o->keys[i], key) == 0) return o->vals[i];
  }
  return 0;
}
static const CanonVal *obj_req(const CanonVal *o, const char *key) {
  const CanonVal *v = obj_get(o, key);
  if (!v) fail("missing object key");
  return v;
}

// --- RNG chain --------------------------------------------------------------------

static MlRng g_rng;
static bool g_boot_seen = false;

// --- the hdFlags table (frame-0 record -> hd_flags seam) ---------------------------

#define FT_STATES 256
#define FT_STR 48

typedef struct {
  char state[FT_STR];
  char name[FT_STR];
  HdFlags f;
} FtEntry;

static FtEntry g_ft[ML_CHARS][FT_STATES];
static int g_ft_count[ML_CHARS];
static bool g_ft_loaded = false;

static bool ft_truthy(const CanonVal *v, const char *what) {
  if (v->type == CV_UNDEF) return false;
  if (v->type == CV_BOOL) return v->b;
  if (v->type == CV_NUM) return v->num != 0 && v->num == v->num;
  fail(what);
  return false;
}

static void ft_copy_str(char dst[FT_STR], const char *s, const char *what) {
  if (strlen(s) >= FT_STR) fail(what);
  strcpy(dst, s);
}

static void load_flags(const CanonVal *dump) {
  if (g_ft_loaded) fail("hdFlags: seen twice");
  if (dump->type != CV_OBJ) fail("hdFlags: expected object");
  for (int ci = 0; ci < dump->nkeys; ci++) {
    const int c = atoi(dump->keys[ci]);
    if (c < 0 || c >= ML_CHARS) fail("hdFlags: char key out of range");
    const CanonVal *tbl = dump->vals[ci];
    if (tbl->type != CV_OBJ) fail("hdFlags: expected state table object");
    if (tbl->nkeys > FT_STATES) fail("hdFlags: too many states");
    g_ft_count[c] = tbl->nkeys;
    for (int si = 0; si < tbl->nkeys; si++) {
      FtEntry *e = &g_ft[c][si];
      memset(e, 0, sizeof *e);
      ft_copy_str(e->state, tbl->keys[si], "hdFlags: state name too long");
      const CanonVal *fo = tbl->vals[si];
      if (fo->type != CV_OBJ || fo->nkeys != 7) fail("hdFlags: flag key count");
      ft_copy_str(e->name, cv_str(obj_req(fo, "name")),
                  "hdFlags: move name too long");
      e->f.name = e->name;
      e->f.canBeGrabbed = ft_truthy(obj_req(fo, "canBeGrabbed"), "canBeGrabbed");
      e->f.crouch = ft_truthy(obj_req(fo, "crouch"), "crouch");
      e->f.downed = ft_truthy(obj_req(fo, "downed"), "downed");
      e->f.specialClank =
          ft_truthy(obj_req(fo, "specialClank"), "specialClank");
      e->f.specialOnHit =
          ft_truthy(obj_req(fo, "specialOnHit"), "specialOnHit");
      e->f.vCancel = ft_truthy(obj_req(fo, "vCancel"), "vCancel");
    }
  }
  g_ft_loaded = true;
}

const HdFlags *hd_flags(double charId, const char *state) {
  const int c = (int)charId;
  if (!g_ft_loaded) fail("hd_flags before hdFlags record");
  if (c < 0 || c >= ML_CHARS) ml_hd_out_of_domain("hd_flags char");
  for (int k = 0; k < g_ft_count[c]; k++) {
    if (strcmp(g_ft[c][k].state, state) == 0) return &g_ft[c][k].f;
  }
  ml_hd_out_of_domain("hd_flags: unknown action state");
  return 0;
}

// --- pending-seam FIFO (dispatch records) --------------------------------------------

#define FIFO_CAP 128

typedef struct {
  long lineno;
  char *args;
  char *post;
} SeamRec;

static SeamRec g_fifo[FIFO_CAP];
static int g_fifo_head = 0, g_fifo_len = 0;

static char *xstrdup(const char *s) {
  char *d = strdup(s);
  if (!d) { fprintf(stderr, "oom\n"); exit(1); }
  return d;
}

static void fifo_push(long lineno, const char *args, const char *post) {
  if (g_fifo_len >= FIFO_CAP) fail("seam FIFO overflow");
  SeamRec *r = &g_fifo[(g_fifo_head + g_fifo_len) % FIFO_CAP];
  r->lineno = lineno;
  r->args = xstrdup(args);
  r->post = xstrdup(post);
  g_fifo_len++;
}

static SeamRec *fifo_pop(void) {
  if (g_fifo_len == 0) return 0;
  SeamRec *r = &g_fifo[g_fifo_head];
  g_fifo_head = (g_fifo_head + 1) % FIFO_CAP;
  g_fifo_len--;
  return r; // caller frees fields
}

static void seam_free(SeamRec *r) {
  free(r->args);
  free(r->post);
}

static void fifo_drain(void) {
  SeamRec *r;
  while ((r = fifo_pop()) != 0) seam_free(r);
}

// --- alias probes ---------------------------------------------------------------------

static MlSim g_sim;
static HdQueues g_q;

static void apply_alias4(MlSim *sim, const CanonVal *alias) {
  if (alias->type != CV_ARR || alias->count != 4) fail("alias probe shape");
  for (int k = 0; k < 4; k++) {
    const CanonVal *a = alias->items[k];
    if (a->type == CV_NULL) {
      if (sim->playerPresent[k]) fail("alias probe null for present player");
      continue;
    }
    if (a->type != CV_ARR || a->count != 4) fail("alias probe entry shape");
    sim->aliasPosEcb1[k] = cv_bool(a->items[0]);
    sim->aliasHbActive[k] = cv_bool(a->items[1]);
    sim->aliasHbHitList[k] = cv_bool(a->items[2]);
    sim->aliasHbId[k] = cv_bool(a->items[3]);
  }
}

static void ser_alias3(CanonBuf *b, const MlSim *sim) {
  cb_putc(b, '[');
  for (int k = 0; k < 4; k++) {
    if (k) cb_putc(b, ',');
    if (!sim->playerPresent[k]) { cb_puts(b, "null"); continue; }
    cb_putc(b, '[');
    cb_puts(b, sim->aliasHbActive[k] ? "T" : "F");
    cb_putc(b, ',');
    cb_puts(b, sim->aliasHbHitList[k] ? "T" : "F");
    cb_putc(b, ',');
    cb_puts(b, sim->aliasHbId[k] ? "T" : "F");
    cb_putc(b, ']');
  }
  cb_putc(b, ']');
}

// --- players ----------------------------------------------------------------------------

static void marshal_players(MlSim *sim, const CanonVal *players) {
  if (players->type != CV_ARR || players->count != 4) fail("players shape");
  for (int k = 0; k < 4; k++) {
    if (players->items[k]->type == CV_NULL) {
      if (sim->playerPresent[k]) fail("players: null for present slot");
      continue;
    }
    if (!sim->playerPresent[k]) fail("players: value for absent slot");
    cv_player(players->items[k], &sim->player[k]);
  }
}

static void ser_players(CanonBuf *b, const MlSim *sim) {
  cb_putc(b, '[');
  for (int k = 0; k < 4; k++) {
    if (k) cb_putc(b, ',');
    if (!sim->playerPresent[k]) { cb_puts(b, "null"); continue; }
    ser_player(b, &sim->player[k]);
  }
  cb_putc(b, ']');
}

// --- hitQueue / phantomQueue marshal + canon ----------------------------------------------

static void marshal_hq_row(const CanonVal *row, HdRow *out) {
  memset(out, 0, sizeof *out);
  if (row->type != CV_ARR || (row->count != 6 && row->count != 7)) {
    fail("hq row shape");
  }
  out->v = cv_num(row->items[0]);
  const CanonVal *a = row->items[1];
  if (a->type == CV_NUM) {
    out->aIsObj = false;
    out->a = a->num;
  } else if (a->type == CV_OBJ) {
    // physics' collisionData {angular, corner, normal} (sorted keys)
    if (a->nkeys != 3) fail("hq stage row shape");
    out->aIsObj = true;
    out->angular = cv_num(obj_req(a, "angular"));
    out->corner = cv_bool(obj_req(a, "corner"));
    out->normal = cv_vec2(obj_req(a, "normal"));
  } else {
    fail("hq row attacker domain");
  }
  out->h = cv_num(row->items[2]);
  out->shieldHit = cv_bool(row->items[3]);
  out->isThrow = cv_bool(row->items[4]);
  out->drawBounce = cv_bool(row->items[5]);
  out->hasPhantom = row->count == 7;
  out->phantom = out->hasPhantom ? cv_bool(row->items[6]) : false;
}

static void marshal_hq(const CanonVal *v, HdQueues *q) {
  if (v->type != CV_ARR || v->count > HD_HQ_CAP) fail("hq shape");
  q->hqCount = v->count;
  for (int i = 0; i < v->count; i++) marshal_hq_row(v->items[i], &q->hq[i]);
}

static void marshal_phq(const CanonVal *v, HdQueues *q) {
  if (v->type != CV_ARR || v->count > HD_PHQ_CAP) fail("phq shape");
  q->phqCount = v->count;
  for (int i = 0; i < v->count; i++) {
    const CanonVal *row = v->items[i];
    if (row->type != CV_ARR || row->count != 2) fail("phq row shape");
    q->phq[i][0] = cv_num(row->items[0]);
    q->phq[i][1] = cv_num(row->items[1]);
  }
}

static void ser_hq(CanonBuf *b, const HdQueues *q) {
  cb_putc(b, '[');
  for (int i = 0; i < q->hqCount; i++) {
    if (i) cb_putc(b, ',');
    const HdRow *r = &q->hq[i];
    cb_putc(b, '[');
    cb_num(b, r->v);
    cb_putc(b, ',');
    if (r->aIsObj) {
      cb_puts(b, "{\"angular\":");
      cb_num(b, r->angular);
      cb_puts(b, ",\"corner\":");
      cb_puts(b, r->corner ? "T" : "F");
      cb_puts(b, ",\"normal\":{\"x\":");
      cb_num(b, r->normal.x);
      cb_puts(b, ",\"y\":");
      cb_num(b, r->normal.y);
      cb_puts(b, "}}");
    } else {
      cb_num(b, r->a);
    }
    cb_putc(b, ',');
    cb_num(b, r->h);
    cb_putc(b, ',');
    cb_puts(b, r->shieldHit ? "T" : "F");
    cb_putc(b, ',');
    cb_puts(b, r->isThrow ? "T" : "F");
    cb_putc(b, ',');
    cb_puts(b, r->drawBounce ? "T" : "F");
    if (r->hasPhantom) {
      cb_putc(b, ',');
      cb_puts(b, r->phantom ? "T" : "F");
    }
    cb_putc(b, ']');
  }
  cb_putc(b, ']');
}

static void ser_phq(CanonBuf *b, const HdQueues *q) {
  cb_putc(b, '[');
  for (int i = 0; i < q->phqCount; i++) {
    if (i) cb_putc(b, ',');
    cb_putc(b, '[');
    cb_num(b, q->phq[i][0]);
    cb_putc(b, ',');
    cb_num(b, q->phq[i][1]);
    cb_putc(b, ']');
  }
  cb_putc(b, ']');
}

// --- the dispatch seam callback --------------------------------------------------------------

void hd_dispatch(MlSim *sim, HdQueues *q, const HdDispCall *call) {
  SeamRec *r = fifo_pop();
  if (!r) {
    fprintf(stderr, "SEAM DIVERGENCE line ~%ld: C reached dispatch %s:%s but "
                    "the recording has no pending seam record\n",
            g_lineno, call->phase, call->state);
    report_div("seam-underflow", g_lineno, "(record)", call->state);
    longjmp(g_rec_jmp, 1);
  }
  // expected move name from the flags table (state key -> entry.name)
  const HdFlags *f = hd_flags(call->charId, call->state);
  CanonBuf b;
  cb_init(&b);
  cb_putc(&b, '[');
  cb_qstr(&b, call->phase);
  cb_putc(&b, ',');
  cb_qstr(&b, f->name);
  cb_puts(&b, ",[");
  cb_num(&b, call->slot);
  for (int i = 0; i < call->extraCount; i++) {
    cb_putc(&b, ',');
    switch (call->extras[i].kind) {
      case DX_NUM: cb_num(&b, call->extras[i].num); break;
      case DX_STR: cb_qstr(&b, call->extras[i].str); break;
      case DX_BOOL: cb_puts(&b, call->extras[i].b ? "T" : "F"); break;
      case DX_VEC:
        cb_puts(&b, "{\"x\":");
        cb_num(&b, call->extras[i].vec.x);
        cb_puts(&b, ",\"y\":");
        cb_num(&b, call->extras[i].vec.y);
        cb_putc(&b, '}');
        break;
    }
  }
  cb_puts(&b, "]]");
  const bool args_ok = strcmp(b.buf, r->args) == 0;
  if (!args_ok) {
    report_div("dispatch-args", r->lineno, r->args, b.buf);
  }
  cb_free(&b);

  // RESYNC from the recorded post-dispatch state {alias, hq, players}
  canon_arena_reset(); // pipeline records marshal everything up front
  const char *err = 0;
  const CanonVal *post = canon_parse(r->post, &err);
  if (!post) {
    fprintf(stderr, "PARSE FAIL dispatch post line %ld: %s\n", r->lineno, err);
    exit(3);
  }
  if (post->type != CV_OBJ || post->nkeys != 3) fail("dispatch post shape");
  marshal_players(sim, obj_req(post, "players"));
  apply_alias4(sim, obj_req(post, "alias"));
  marshal_hq(obj_req(post, "hq"), q);
  seam_free(r);
  if (!args_ok) longjmp(g_rec_jmp, 1);
}

int main(int argc, char **argv) {
  const char *path = NULL;
  bool strict = false, stop_first = false;
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--strict") == 0) strict = true;
    else if (strcmp(argv[i], "--stop-first") == 0) stop_first = true;
    else if (strcmp(argv[i], "--max-print") == 0 && i + 1 < argc) g_max_print = atoi(argv[++i]);
    else if (!path) path = argv[i];
    else { fprintf(stderr, "unknown arg: %s\n", argv[i]); return 1; }
  }
  if (!path) {
    fprintf(stderr, "usage: hitdet_replay <capture.jsonl> [--strict] "
                    "[--max-print N] [--stop-first]\n");
    return 1;
  }
  FILE *f = fopen(path, "r");
  if (!f) { fprintf(stderr, "cannot open %s\n", path); return 1; }
  g_file = path;

  ml_active_rng = &g_rng;

  char *line = NULL;
  size_t linecap = 0;
  long replayed = 0;
  long pipeline_records = 0, pure_records = 0, seam_records = 0,
       rng_records = 0;

  CanonBuf out;
  cb_init(&out);

  ssize_t n;
  while ((n = getline(&line, &linecap, f)) > 0) {
    g_lineno++;
    if (line[n - 1] == '\n') line[n - 1] = 0;
    if (line[0] == 0) continue;

    char *tab1 = strchr(line, '\t');
    char *tab2 = tab1 ? strchr(tab1 + 1, '\t') : NULL;
    char *tab3 = tab2 ? strchr(tab2 + 1, '\t') : NULL;
    if (!tab1 || !tab2 || !tab3) fail("malformed record");
    *tab1 = 0; *tab2 = 0; *tab3 = 0;
    const char *fn = tab1 + 1;
    const char *args_s = tab2 + 1;
    char *ret_s = tab3 + 1;
    char *tab4 = strchr(ret_s, '\t');
    const char *post_s = NULL;
    if (tab4) { *tab4 = 0; post_s = tab4 + 1; }

    if (strcmp(fn, "rngBoot") == 0) {
      if (g_boot_seen) fail("rngBoot: seen twice");
      canon_arena_reset();
      const char *err = 0;
      const CanonVal *args = canon_parse(args_s, &err);
      if (!args || args->type != CV_ARR || args->count != 2) fail("rngBoot args");
      const double seed = cv_num(args->items[0]);
      const double boot = cv_num(args->items[1]);
      if (seed < 0 || seed != (double)(uint32_t)seed) fail("rngBoot: bad seed");
      if (boot < 0 || boot != (double)(long)boot) fail("rngBoot: bad count");
      ml_rng_seed(&g_rng, (uint32_t)seed);
      for (long i = 0; i < (long)boot; i++) (void)ml_rng_next(&g_rng);
      out.len = 0; out.buf[0] = 0;
      cb_num(&out, (double)g_rng.a);
      if (strcmp(out.buf, ret_s) != 0) report_div("rngBoot", g_lineno, ret_s, out.buf);
      g_boot_seen = true;
      replayed++;
      continue;
    }

    if (strcmp(fn, "Math.randomW") == 0) {
      // pinned ZERO: a draw inside a dispatch window below a hitdet
      // boundary is chain-order-ambiguous vs the boundary's own draws
      fail("Math.randomW record: outside the measured RNG-order domain");
    }

    if (strcmp(fn, "Math.random") == 0) {
      if (!g_boot_seen) fail("Math.random before rngBoot");
      const double v = ml_rng_next(&g_rng);
      out.len = 0; out.buf[0] = 0;
      cb_num(&out, v);
      if (strcmp(out.buf, ret_s) != 0) {
        report_div("Math.random", g_lineno, ret_s, out.buf);
        if (stop_first) break;
      }
      rng_records++;
      replayed++;
      continue;
    }

    if (strcmp(fn, "hdFlags") == 0) {
      canon_arena_reset();
      const char *err = 0;
      const CanonVal *dump = canon_parse(ret_s, &err);
      if (!dump) { fprintf(stderr, "PARSE FAIL hdFlags: %s\n", err); return 3; }
      load_flags(dump);
      replayed++;
      continue;
    }

    if (strcmp(fn, "dispatch") == 0) {
      if (!post_s) fail("dispatch: missing post");
      fifo_push(g_lineno, args_s, post_s);
      seam_records++;
      replayed++;
      continue;
    }

    // --- pure records ------------------------------------------------------
    if (strcmp(fn, "getLaunchAngle") == 0 ||
        strcmp(fn, "getHorizontalVelocity") == 0 ||
        strcmp(fn, "getVerticalVelocity") == 0 ||
        strcmp(fn, "getHorizontalDecay") == 0 ||
        strcmp(fn, "getVerticalDecay") == 0 ||
        strcmp(fn, "getKnockback") == 0 || strcmp(fn, "getHitstun") == 0 ||
        strcmp(fn, "segmentSegmentCollision") == 0) {
      if (post_s) fail("pure record with post field");
      canon_arena_reset();
      const char *err = 0;
      const CanonVal *args = canon_parse(args_s, &err);
      if (!args) { fprintf(stderr, "PARSE FAIL %s:%ld: %s\n", path, g_lineno, err); return 3; }
      out.len = 0; out.buf[0] = 0;
      if (strcmp(fn, "getLaunchAngle") == 0) {
        if (args->type != CV_ARR || args->count != 7) fail("getLaunchAngle argc");
        const CanonVal *rev = args->items[2];
        bool hasReverse = rev->type != CV_UNDEF;
        bool reverse = false;
        if (rev->type == CV_BOOL) reverse = rev->b;
        else if (rev->type != CV_UNDEF) fail("getLaunchAngle reverse domain");
        // 7th = the lazily-read player[v].phys.grounded (null <=> never
        // read upstream, i.e. knockback >= 80)
        const CanonVal *g = args->items[6];
        bool grounded = false;
        if (g->type == CV_BOOL) grounded = g->b;
        else if (g->type != CV_NULL) fail("getLaunchAngle grounded domain");
        cb_num(&out, hd_getLaunchAngle(cv_num(args->items[0]),
                                       cv_num(args->items[1]), hasReverse,
                                       reverse, cv_num(args->items[3]),
                                       cv_num(args->items[4]), grounded));
      } else if (strcmp(fn, "getHorizontalVelocity") == 0) {
        if (args->type != CV_ARR || args->count != 2) fail("getHV argc");
        cb_num(&out, hd_getHorizontalVelocity(cv_num(args->items[0]),
                                              cv_num(args->items[1])));
      } else if (strcmp(fn, "getVerticalVelocity") == 0) {
        if (args->type != CV_ARR || args->count != 4) fail("getVV argc");
        cb_num(&out, hd_getVerticalVelocity(
                         cv_num(args->items[0]), cv_num(args->items[1]),
                         cv_bool(args->items[2]), cv_num(args->items[3])));
      } else if (strcmp(fn, "getHorizontalDecay") == 0) {
        if (args->type != CV_ARR || args->count != 1) fail("getHD argc");
        cb_num(&out, hd_getHorizontalDecay(cv_num(args->items[0])));
      } else if (strcmp(fn, "getVerticalDecay") == 0) {
        if (args->type != CV_ARR || args->count != 1) fail("getVD argc");
        cb_num(&out, hd_getVerticalDecay(cv_num(args->items[0])));
      } else if (strcmp(fn, "getKnockback") == 0) {
        if (args->type != CV_ARR || args->count != 7) fail("getKnockback argc");
        const CanonVal *hb = args->items[0];
        if (hb->type != CV_OBJ || hb->nkeys != 3) fail("getKnockback hb shape");
        HdKbSpec spec;
        spec.bk = cv_num(obj_req(hb, "bk"));
        spec.kg = cv_num(obj_req(hb, "kg"));
        spec.sk = cv_num(obj_req(hb, "sk"));
        cb_num(&out, hd_getKnockback(spec, cv_num(args->items[1]),
                                     cv_num(args->items[2]),
                                     cv_num(args->items[3]),
                                     cv_num(args->items[4]),
                                     cv_truthy_bu(args->items[5]),
                                     cv_truthy_bu(args->items[6])));
      } else if (strcmp(fn, "getHitstun") == 0) {
        if (args->type != CV_ARR || args->count != 1) fail("getHitstun argc");
        cb_num(&out, hd_getHitstun(cv_num(args->items[0])));
      } else {
        if (args->type != CV_ARR || args->count != 4) fail("segSeg argc");
        cb_puts(&out, hd_segmentSegmentCollision(
                          cv_vec2(args->items[0]), cv_vec2(args->items[1]),
                          cv_vec2(args->items[2]), cv_vec2(args->items[3]))
                          ? "T"
                          : "F");
      }
      if (strcmp(out.buf, ret_s) != 0) {
        report_div(fn, g_lineno, ret_s, out.buf);
        if (stop_first) break;
      }
      pure_records++;
      replayed++;
      continue;
    }

    // --- knockbackSounds (pure logic + sound events) -------------------------
    if (strcmp(fn, "knockbackSounds") == 0) {
      if (!post_s) fail("knockbackSounds: missing post");
      if (strcmp(ret_s, "undef") != 0) fail("knockbackSounds: ret not undef");
      canon_arena_reset();
      const char *err = 0;
      const CanonVal *args = canon_parse(args_s, &err);
      if (!args || args->type != CV_ARR || args->count != 3) fail("kbs argc");
      ml_ev_reset();
      hd_knockbackSounds(cv_num(args->items[0]), cv_num(args->items[1]),
                         cv_num(args->items[2]));
      out.len = 0; out.buf[0] = 0;
      cb_puts(&out, "{\"rng\":[");
      for (int s = 0; s < ml_events.rng_count; s++) {
        if (s) cb_putc(&out, ',');
        cb_num(&out, ml_events.rng[s]);
      }
      cb_puts(&out, "],\"snd\":[");
      for (int s = 0; s < ml_events.snd_count; s++) {
        if (s) cb_putc(&out, ',');
        cb_qstr(&out, ml_events.snd[s]);
      }
      cb_puts(&out, "]}");
      if (strcmp(out.buf, post_s) != 0) {
        report_div("knockbackSounds-post", g_lineno, post_s, out.buf);
        if (stop_first) break;
      }
      pure_records++;
      replayed++;
      continue;
    }

    // --- lean queue records ----------------------------------------------------
    if (strcmp(fn, "resetHitQueue") == 0 || strcmp(fn, "setPhantonQueue") == 0) {
      if (!post_s) fail("queue record: missing post");
      if (strcmp(ret_s, "undef") != 0) fail("queue record: ret not undef");
      canon_arena_reset();
      const char *err = 0;
      const CanonVal *args = canon_parse(args_s, &err);
      if (!args || args->type != CV_ARR) fail("queue record args");
      memset(&g_q, 0, sizeof g_q);
      out.len = 0; out.buf[0] = 0;
      if (strcmp(fn, "resetHitQueue") == 0) {
        if (args->count != 1) fail("resetHitQueue argc");
        marshal_hq(args->items[0], &g_q);
        hd_resetHitQueue(&g_q);
        cb_puts(&out, "{\"hq\":");
        ser_hq(&out, &g_q);
        cb_putc(&out, '}');
      } else {
        if (args->count != 2) fail("setPhantonQueue argc");
        marshal_phq(args->items[1], &g_q);
        // marshal the new value through the phq row model
        HdQueues val;
        memset(&val, 0, sizeof val);
        marshal_phq(args->items[0], &val);
        hd_setPhantonQueue(&g_q, val.phq, val.phqCount);
        cb_puts(&out, "{\"phq\":");
        ser_phq(&out, &g_q);
        cb_putc(&out, '}');
      }
      if (strcmp(out.buf, post_s) != 0) {
        report_div(fn, g_lineno, post_s, out.buf);
        if (stop_first) break;
      }
      pipeline_records++;
      replayed++;
      continue;
    }

    // --- the pipeline mutators ----------------------------------------------------
    if (strcmp(fn, "hitDetect") != 0 && strcmp(fn, "executeHits") != 0 &&
        strcmp(fn, "checkPhantoms") != 0) {
      // internal-only exports are pinned at 0 records; anything else is
      // outside the frozen record vocabulary
      fail("unknown record function");
    }
    if (!post_s) fail("pipeline record: missing post-state field");
    if (strcmp(ret_s, "undef") != 0) fail("pipeline fn is void: ret not undef");

    canon_arena_reset();
    const char *err = 0;
    const CanonVal *args = canon_parse(args_s, &err);
    if (!args) { fprintf(stderr, "PARSE FAIL %s:%ld args: %s\n", path, g_lineno, err); return 3; }
    const bool isHitDetect = strcmp(fn, "hitDetect") == 0;
    const int wantArgc = isHitDetect ? 2 : 1;
    if (args->type != CV_ARR || args->count != wantArgc) fail("pipeline argc");
    const double slot_p = isHitDetect ? cv_num(args->items[0]) : 0;
    const CanonVal *pre = args->items[wantArgc - 1];
    if (pre->type != CV_OBJ || pre->nkeys != 8) fail("pipeline pre shape");

    memset(&g_sim, 0, sizeof g_sim);
    memset(&g_q, 0, sizeof g_q);
    const CanonVal *pt = obj_req(pre, "playerType");
    if (pt->type != CV_ARR || pt->count != 4) fail("playerType shape");
    for (int k = 0; k < 4; k++) {
      g_sim.playerType[k] = cv_num(pt->items[k]);
      g_sim.playerPresent[k] = g_sim.playerType[k] > -1;
    }
    const CanonVal *cs = obj_req(pre, "characterSelections");
    if (cs->type != CV_ARR || cs->count != 4) fail("characterSelections shape");
    for (int k = 0; k < 4; k++) g_sim.characterSelections[k] = cv_num(cs->items[k]);
    g_sim.gameMode = cv_num(obj_req(pre, "gameMode"));
    const CanonVal *gs = obj_req(pre, "gameSettings");
    if (gs->type != CV_OBJ || gs->nkeys != 1) fail("gameSettings shape");
    g_sim.phantomThreshold = cv_num(obj_req(gs, "phantomThreshold"));
    marshal_players(&g_sim, obj_req(pre, "players"));
    apply_alias4(&g_sim, obj_req(pre, "alias"));
    marshal_hq(obj_req(pre, "hq"), &g_q);
    marshal_phq(obj_req(pre, "phq"), &g_q);
    ml_ev_reset();

    if (setjmp(g_rec_jmp) == 0) {
      if (isHitDetect) {
        if (!g_sim.playerPresent[(int)slot_p]) fail("hitDetect: absent slot");
        hd_hitDetect(&g_sim, &g_q, slot_p);
      } else if (strcmp(fn, "executeHits") == 0) {
        hd_executeHits(&g_sim, &g_q);
      } else {
        hd_checkPhantoms(&g_sim, &g_q);
      }

      if (g_fifo_len != 0) {
        SeamRec *r = fifo_pop();
        fprintf(stderr, "SEAM DIVERGENCE line %ld: recorded dispatch never "
                        "reached by C %s\n", r->lineno, fn);
        report_div("seam-unconsumed", r->lineno, r->args, "(not reached)");
        seam_free(r);
        fifo_drain();
      } else {
        out.len = 0; out.buf[0] = 0;
        cb_puts(&out, "{\"alias\":");
        ser_alias3(&out, &g_sim);
        cb_puts(&out, ",\"hq\":");
        ser_hq(&out, &g_q);
        cb_puts(&out, ",\"phq\":");
        ser_phq(&out, &g_q);
        cb_puts(&out, ",\"players\":");
        ser_players(&out, &g_sim);
        cb_puts(&out, ",\"rng\":[");
        for (int s = 0; s < ml_events.rng_count; s++) {
          if (s) cb_putc(&out, ',');
          cb_num(&out, ml_events.rng[s]);
        }
        cb_puts(&out, "],\"snd\":[");
        for (int s = 0; s < ml_events.snd_count; s++) {
          if (s) cb_putc(&out, ',');
          cb_qstr(&out, ml_events.snd[s]);
        }
        cb_puts(&out, "],\"vfx\":["); // M4 task 1: full-config vfx
        for (int s = 0; s < ml_events.vfx_count; s++) {
          if (s) cb_putc(&out, ',');
          cb_vfx(&out, &ml_events.vfx[s]);
        }
        cb_puts(&out, "]}");
        if (strcmp(out.buf, post_s) != 0) {
          report_div("pipeline-post", g_lineno, post_s, out.buf);
        }
      }
    } else {
      // seam mismatch aborted the record; drop its remaining seam records
      fifo_drain();
    }

    pipeline_records++;
    replayed++;
    if (g_divergences > 0 && stop_first) break;
  }
  free(line);
  fclose(f);

  if (!g_boot_seen) fail("capture carries no rngBoot record");
  if (!g_ft_loaded) fail("capture carries no hdFlags record");
  if (g_fifo_len != 0) {
    fprintf(stderr, "SEAM DIVERGENCE: %d seam records left unconsumed at EOF\n",
            g_fifo_len);
    g_divergences += g_fifo_len;
    fifo_drain();
  }

  fprintf(stderr,
          "replayed: %ld pipeline, %ld pure, %ld standalone rng draws, "
          "%ld dispatch seams consumed in call order\n",
          pipeline_records, pure_records, rng_records, seam_records);
  printf("HITDET REPLAY RAN %ld records, %ld divergences", replayed,
         g_divergences);
  if (g_first_div_line != -1) printf(" (first at line %ld)", g_first_div_line);
  printf("\n");
  cb_free(&out);
  if (strict && g_divergences > 0) return 2;
  return 0;
}
