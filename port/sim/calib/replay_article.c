// replay_article.c — M2 task 13 replay driver: feeds recorded article
// boundary calls (build/<id>.article.jsonl, FORMAT.md "The article spec")
// to the C article translation (port/sim/article.c) with task 7's shared
// move bodies + task 6's hit_detection linked as the REAL nested tree,
// and compares canon-v1.1 serializations byte-for-byte.
//
// Record types:
// - mvData (frame 0): the moves-shared chars dump — registers the shared
//   C bodies for all five chars from the measured sharedOrigin map and
//   serves the mv_* data seams (rule 15).
// - hdFlags (frame 0): the actionStates flag table (hd_flags seam —
//   executeArticleHits reads crouch/vCancel).
// - rngBoot / Math.random: the chained seeded mulberry32; frame-0 records
//   replay on the SEPARATE sweep mulberry32 (0x0badf00d).
// - mdispatch: FIFO seams — per-char move fns reached from inside an
//   article boundary's dispatch windows (args verified, window rng
//   advanced, players/alias/hq resynced). Measured zero over the
//   carriers; teeth negative-test-proven.
// - ainit: articles.{LASER,ILLUSION}.init — marshal the options + spawn
//   pre (players/stage/queues), run the REAL C init (which runs the
//   spawn-frame main incl. the wall check), compare the lean post.
//   These are the SAME crossings tasks 8/9 verified as 4-field article
//   seam FIFOs from the fox/falco side — the seam-to-body conversion is
//   verified here bit-exactly (FORMAT.md "The article spec").
// - destroyArticles / executeArticles / articlesHitDetection /
//   executeArticleHits / resetAArticles: marshal the (lean-when-empty)
//   pre, run the C body, compare the post bit-exactly.
//
// QUEUE CHAIN INSTRUMENT: the article queues are C module state chained
// across records (every upstream mutation site is a captured boundary).
// From the first in-match record on, the chained state is COMPARED
// against each record's pre queues before being re-marshaled (authorita-
// tive) — a queue mutation the C got wrong flags at the very next record
// even when that record's own body is correct. Frame-0 sweep records
// tear queues down between arms by direct pokes, so the chain only arms
// on frames >= 1.
//
// Marshalling is STRICT (prevention rule 7): any shape outside the
// captured domain aborts with exit 3 — never guess.
//
// Usage: replay_article <capture.jsonl> [--strict] [--max-print N]
//                       [--stop-first]
#include <inttypes.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../article.h"
#include "../characters/shared/moves.h"
#include "../ml_events.h"
#include "canon.h"
#include "input_canon.h"
#include "ml_tables.h"
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
void ml_phys_out_of_domain(const char *what) {
  fprintf(stderr, "OUT OF DOMAIN %s:%ld: %s\n", g_file, g_lineno, what);
  exit(3);
}
void ml_hd_out_of_domain(const char *what) {
  fprintf(stderr, "OUT OF DOMAIN %s:%ld: %s\n", g_file, g_lineno, what);
  exit(3);
}
void mv_out_of_domain(const char *what) {
  fprintf(stderr, "OUT OF DOMAIN %s:%ld: %s\n", g_file, g_lineno, what);
  exit(3);
}
void ml_art_out_of_domain(const char *what) {
  fprintf(stderr, "OUT OF DOMAIN %s:%ld: %s\n", g_file, g_lineno, what);
  exit(3);
}

// hit_detection.c's dispatch seam: unreachable from the article surface
// (article.c dispatches through mv_dispatch; hitDetect/executeHits are
// never called here).
void hd_dispatch(MlSim *sim, HdQueues *q, const HdDispCall *call) {
  (void)sim;
  (void)q;
  (void)call;
  fail("hd_dispatch reached from the article replay");
}

// --- divergence accounting ---------------------------------------------------
static long g_divergences = 0;
static long g_first_div_line = -1;
static long g_printed = 0;
static int g_max_print = 5;
static jmp_buf g_rec_jmp;

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

// --- small marshal helpers -----------------------------------------------------
static double cv_num(const CanonVal *v) {
  if (v->type != CV_NUM) fail("expected number");
  return v->num;
}
static bool cv_bool(const CanonVal *v) {
  if (v->type != CV_BOOL) fail("expected boolean");
  return v->b;
}
static const char *cv_str(const CanonVal *v) {
  if (v->type != CV_STR) fail("expected string");
  return v->str;
}
static double cv_truthy_num(const CanonVal *v) {
  if (v->type == CV_NUM) return v->num;
  if (v->type == CV_BOOL) return v->b ? 1 : 0;
  fail("expected number/boolean");
  return 0;
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
static char *xstrdup(const char *s) {
  char *d = strdup(s);
  if (!d) { fprintf(stderr, "oom\n"); exit(1); }
  return d;
}

// --- generic canon reserializer (opaque hq / chain-compare carriers) -----------
static void reser(CanonBuf *b, const CanonVal *v) {
  switch (v->type) {
    case CV_NULL: cb_puts(b, "null"); break;
    case CV_UNDEF: cb_puts(b, "undef"); break;
    case CV_FN: cb_puts(b, "fn"); break;
    case CV_CYC: cb_puts(b, "cyc"); break;
    case CV_BOOL: cb_puts(b, v->b ? "T" : "F"); break;
    case CV_NUM: cb_num(b, v->num); break;
    case CV_STR:
      cb_qstr(b, v->str);
      break;
    case CV_ARR:
      cb_putc(b, '[');
      for (int i = 0; i < v->count; i++) {
        if (i) cb_putc(b, ',');
        reser(b, v->items[i]);
      }
      cb_putc(b, ']');
      break;
    case CV_OBJ:
      cb_putc(b, '{');
      for (int i = 0; i < v->nkeys; i++) {
        if (i) cb_putc(b, ',');
        cb_qstr(b, v->keys[i]);
        cb_putc(b, ':');
        reser(b, v->vals[i]);
      }
      cb_putc(b, '}');
      break;
  }
}
static char *reser_dup(const CanonVal *v) {
  CanonBuf b;
  cb_init(&b);
  reser(&b, v);
  char *out = xstrdup(b.buf);
  cb_free(&b);
  return out;
}

// --- the mvData tables (moves-shared chars dump; identical loader) -------------
#define MV_STATES 192
#define MV_NAME 40
#define MV_ARR_CAP 64
#define MV_SND_KEYS 16
#define MV_SND_ROWS 16

typedef struct {
  int nStates;
  char state[MV_STATES][MV_NAME];
  char name[MV_STATES][MV_NAME];
  bool shared[MV_STATES];
  double setVel[6][MV_ARR_CAP];
  int setVelLen[6];
  Vec2D posOffCC[MV_ARR_CAP];
  int posOffCCLen;
  double setPosCD[MV_ARR_CAP];
  int setPosCDLen;
  int nSndKeys;
  char sndKey[MV_SND_KEYS][MV_NAME];
  AsSoundRow sndRows[MV_SND_KEYS][MV_SND_ROWS];
  char sndName[MV_SND_KEYS][MV_SND_ROWS][MV_NAME];
  int sndLen[MV_SND_KEYS];
} MvCharData;

static const char *const SETVEL_KEYS[6] = {
    "DOWNSTANDB", "DOWNSTANDF", "ESCAPEB", "ESCAPEF", "TECHB", "TECHF"};

static MvCharData g_mv[ML_CHARS];
static char g_palette0[4][MV_NAME];
static bool g_mv_loaded = false;

static void copy_name(char dst[MV_NAME], const char *s, const char *what) {
  if (strlen(s) >= MV_NAME) fail(what);
  strcpy(dst, s);
}

static int mv_state_idx(int c, const char *state) {
  for (int i = 0; i < g_mv[c].nStates; i++) {
    if (strcmp(g_mv[c].state[i], state) == 0) return i;
  }
  return -1;
}

static void load_mvdata(const CanonVal *dump) {
  if (g_mv_loaded) fail("mvData: seen twice");
  if (dump->type != CV_OBJ || dump->nkeys != 2) fail("mvData shape");
  const CanonVal *chars = obj_req(dump, "chars");
  const CanonVal *pal = obj_req(dump, "palettes0");
  if (pal->type != CV_ARR || pal->count != 4) fail("mvData palettes0");
  for (int k = 0; k < 4; k++) {
    copy_name(g_palette0[k], cv_str(pal->items[k]), "palette string too long");
  }
  if (chars->type != CV_OBJ || chars->nkeys != ML_CHARS) fail("mvData chars");
  for (int ci = 0; ci < chars->nkeys; ci++) {
    const int c = atoi(chars->keys[ci]);
    if (c < 0 || c >= ML_CHARS) fail("mvData char key");
    MvCharData *d = &g_mv[c];
    const CanonVal *cd = chars->vals[ci];
    if (cd->type != CV_OBJ || cd->nkeys != 7) fail("mvData char shape");
    const CanonVal *names = obj_req(cd, "name");
    const CanonVal *shared = obj_req(cd, "shared");
    if (names->type != CV_OBJ || shared->type != CV_OBJ ||
        names->nkeys != shared->nkeys || names->nkeys > MV_STATES) {
      fail("mvData name/shared shape");
    }
    d->nStates = names->nkeys;
    for (int i = 0; i < names->nkeys; i++) {
      copy_name(d->state[i], names->keys[i], "state key too long");
      copy_name(d->name[i], cv_str(names->vals[i]), "move name too long");
      if (strcmp(shared->keys[i], names->keys[i]) != 0) fail("shared key order");
      d->shared[i] = cv_bool(shared->vals[i]);
      if (d->shared[i] && strcmp(d->state[i], d->name[i]) != 0) {
        fail("shared state key != move name");
      }
    }
    const CanonVal *sv = obj_req(cd, "setVelocities");
    if (sv->type != CV_OBJ || sv->nkeys != 6) fail("mvData setVelocities");
    for (int k = 0; k < 6; k++) {
      const CanonVal *arr = obj_req(sv, SETVEL_KEYS[k]);
      if (arr->type != CV_ARR || arr->count > MV_ARR_CAP) fail("setVel shape");
      d->setVelLen[k] = arr->count;
      for (int i = 0; i < arr->count; i++) {
        d->setVel[k][i] = cv_num(arr->items[i]);
      }
    }
    const CanonVal *cc = obj_req(cd, "posOffsetCliffCatch");
    if (cc->type != CV_ARR || cc->count > MV_ARR_CAP) fail("posOffsetCC shape");
    d->posOffCCLen = cc->count;
    for (int i = 0; i < cc->count; i++) {
      const CanonVal *pair = cc->items[i];
      if (pair->type != CV_ARR || pair->count != 2) fail("posOffsetCC pair");
      d->posOffCC[i].x = cv_num(pair->items[0]);
      d->posOffCC[i].y = cv_num(pair->items[1]);
    }
    (void)obj_req(cd, "posOffsetCliffWait"); // dumped; no C consumer here
    const CanonVal *sp = obj_req(cd, "setPositionsCaptureDamage");
    if (sp->type != CV_ARR || sp->count > MV_ARR_CAP) fail("setPositions shape");
    d->setPosCDLen = sp->count;
    for (int i = 0; i < sp->count; i++) d->setPosCD[i] = cv_num(sp->items[i]);
    const CanonVal *snd = obj_req(cd, "actionSounds");
    if (snd->type != CV_OBJ || snd->nkeys > MV_SND_KEYS) fail("actionSounds");
    d->nSndKeys = snd->nkeys;
    for (int k = 0; k < snd->nkeys; k++) {
      copy_name(d->sndKey[k], snd->keys[k], "actionSounds key too long");
      const CanonVal *rows = snd->vals[k];
      if (rows->type != CV_ARR || rows->count > MV_SND_ROWS) fail("snd rows");
      d->sndLen[k] = rows->count;
      for (int i = 0; i < rows->count; i++) {
        const CanonVal *row = rows->items[i];
        if (row->type != CV_ARR || row->count != 2) fail("snd row shape");
        d->sndRows[k][i].frame = cv_num(row->items[0]);
        copy_name(d->sndName[k][i], cv_str(row->items[1]),
                  "snd name too long");
        d->sndRows[k][i].name = d->sndName[k][i];
      }
    }
  }
  // register the shared C bodies into the task-4 scaffolding
  for (int c = 0; c < ML_CHARS; c++) {
    static AsMoveEntry entries[ML_CHARS][AS_MAX_STATES];
    int count = 0;
    for (int i = 0; i < g_mv[c].nStates; i++) {
      if (!g_mv[c].shared[i]) continue;
      const MlMoveDef *def = mv_shared_def(g_mv[c].state[i]);
      if (def == 0) fail("sharedOrigin state has no C body");
      entries[c][count].name = g_mv[c].state[i];
      entries[c][count].def = def;
      count++;
    }
    const int want = c == 1 ? 76 : 79; // puff overrides 3 shared states
    if (count != want) fail("shared registration count");
    as_setupActionStates(c, entries[c], count);
  }
  g_mv_loaded = true;
}

// --- the mvData-backed data seams (shared moves.h) ------------------------------
double mv_setVelocity(double charId, const char *moveKey, double idx) {
  const int c = (int)charId;
  if (c < 0 || c >= ML_CHARS) mv_out_of_domain("setVelocity char");
  for (int k = 0; k < 6; k++) {
    if (strcmp(SETVEL_KEYS[k], moveKey) == 0) {
      const int i = (int)idx;
      if (idx != (double)i || i < 0 || i >= g_mv[c].setVelLen[k]) {
        return js_nan();
      }
      return g_mv[c].setVel[k][i];
    }
  }
  mv_out_of_domain("setVelocity move key");
  return 0;
}

double mv_setPosition_capturedamage(double charId, double idx) {
  const int c = (int)charId;
  if (c < 0 || c >= ML_CHARS) mv_out_of_domain("setPositions char");
  const int i = (int)idx;
  if (idx != (double)i || i < 0 || i >= g_mv[c].setPosCDLen) return js_nan();
  return g_mv[c].setPosCD[i];
}

bool mv_posOffsetCliffCatch(double charId, double idx, Vec2D *out) {
  const int c = (int)charId;
  if (c < 0 || c >= ML_CHARS) mv_out_of_domain("posOffset char");
  const int i = (int)idx;
  if (idx != (double)i || i < 0 || i >= g_mv[c].posOffCCLen) return false;
  *out = g_mv[c].posOffCC[i];
  return true;
}

int mv_actionSounds(double charId, const char *state, const AsSoundRow **rows) {
  const int c = (int)charId;
  if (c < 0 || c >= ML_CHARS) mv_out_of_domain("actionSounds char");
  for (int k = 0; k < g_mv[c].nSndKeys; k++) {
    if (strcmp(g_mv[c].sndKey[k], state) == 0) {
      *rows = g_mv[c].sndRows[k];
      return g_mv[c].sndLen[k];
    }
  }
  return -1;
}

const char *mv_palette0(double slot) {
  const int k = (int)slot;
  if (k < 0 || k > 3) mv_out_of_domain("palette slot");
  return g_palette0[k];
}

// --- the hdFlags table (frame-0 record -> hd_flags seam) -------------------------
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

// --- RNG chains --------------------------------------------------------------------
static MlRng g_seeded;
static MlRng g_sweep;
static MlRng *g_active = 0;
static bool g_boot_seen = false;

// --- pending-seam FIFO (mdispatch) ---------------------------------------------------
#define FIFO_CAP 64

typedef struct {
  long lineno;
  char *args;
  char *ret;
  char *post;
} SeamRec;

static SeamRec g_fifo[FIFO_CAP];
static int g_fifo_head = 0, g_fifo_len = 0;

static void fifo_push(long lineno, const char *args, const char *ret,
                      const char *post) {
  if (g_fifo_len >= FIFO_CAP) fail("seam FIFO overflow");
  SeamRec *r = &g_fifo[(g_fifo_head + g_fifo_len) % FIFO_CAP];
  r->lineno = lineno;
  r->args = xstrdup(args);
  r->ret = xstrdup(ret);
  r->post = xstrdup(post);
  g_fifo_len++;
}

static SeamRec *fifo_pop(void) {
  if (g_fifo_len == 0) return 0;
  SeamRec *r = &g_fifo[g_fifo_head];
  g_fifo_head = (g_fifo_head + 1) % FIFO_CAP;
  g_fifo_len--;
  return r;
}

static void seam_free(SeamRec *r) {
  free(r->args);
  free(r->ret);
  free(r->post);
}

static void fifo_drain(void) {
  SeamRec *r;
  while ((r = fifo_pop()) != 0) seam_free(r);
}

// --- sim state + article state (chained) + opaque hq ----------------------------------
static MlSim g_sim;
static MlArticles g_art;
static bool g_chain_armed = false;
static char *g_hq = 0;

static void set_hq_owned(char *s) {
  if (g_hq) free(g_hq);
  g_hq = s;
}

void mv_hq_push6(MlSim *S, double a, double b, double c, bool d, bool e,
                 bool f) {
  (void)S;
  if (!g_hq) fail("mv_hq_push6: no hq carrier");
  CanonBuf out;
  cb_init(&out);
  const size_t len = strlen(g_hq);
  if (len < 2 || g_hq[0] != '[' || g_hq[len - 1] != ']') {
    fail("mv_hq_push6: hq carrier not an array");
  }
  for (size_t i = 0; i + 1 < len; i++) cb_putc(&out, g_hq[i]);
  if (len > 2) cb_putc(&out, ',');
  cb_putc(&out, '[');
  cb_num(&out, a);
  cb_putc(&out, ',');
  cb_num(&out, b);
  cb_putc(&out, ',');
  cb_num(&out, c);
  cb_putc(&out, ',');
  cb_puts(&out, d ? "T" : "F");
  cb_putc(&out, ',');
  cb_puts(&out, e ? "T" : "F");
  cb_putc(&out, ',');
  cb_puts(&out, f ? "T" : "F");
  cb_puts(&out, "]]");
  set_hq_owned(xstrdup(out.buf));
  cb_free(&out);
}

// --- alias probes -------------------------------------------------------------------------
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

static void ser_alias4(CanonBuf *b, const MlSim *sim) {
  cb_putc(b, '[');
  for (int k = 0; k < 4; k++) {
    if (k) cb_putc(b, ',');
    if (!sim->playerPresent[k]) { cb_puts(b, "null"); continue; }
    cb_putc(b, '[');
    cb_puts(b, sim->aliasPosEcb1[k] ? "T" : "F");
    cb_putc(b, ',');
    cb_puts(b, sim->aliasHbActive[k] ? "T" : "F");
    cb_putc(b, ',');
    cb_puts(b, sim->aliasHbHitList[k] ? "T" : "F");
    cb_putc(b, ',');
    cb_puts(b, sim->aliasHbId[k] ? "T" : "F");
    cb_putc(b, ']');
  }
  cb_putc(b, ']');
}

static void marshal_playerType(MlSim *sim, const CanonVal *pt) {
  if (pt->type != CV_ARR || pt->count != 4) fail("playerType shape");
  for (int k = 0; k < 4; k++) {
    sim->playerType[k] = cv_num(pt->items[k]);
    sim->playerPresent[k] = sim->playerType[k] > -1;
  }
}

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

// --- the article stage projection (five surface lists, envcoll marshal) --------------
static DamageType cv_damage_value(const CanonVal *v) {
  DamageType d = damage_absent();
  if (v->type == CV_NULL) { d.tag = DT_NULL; return d; }
  if (v->type == CV_UNDEF) { d.tag = DT_UNDEF; return d; }
  if (v->type == CV_STR) {
    if (strlen(v->str) >= sizeof d.str) fail("damageType string too long");
    d.tag = DT_STR;
    strcpy(d.str, v->str);
    return d;
  }
  fail("expected damageType null/undef/string");
  return d;
}

static Surface cv_surface(const CanonVal *v) {
  Surface s;
  memset(&s, 0, sizeof s);
  if (v->type != CV_ARR || (v->count != 2 && v->count != 3)) {
    fail("expected surface [Vec2D,Vec2D(,props)]");
  }
  s.p0 = cv_vec2(v->items[0]);
  s.p1 = cv_vec2(v->items[1]);
  s.propsDamageType = damage_absent();
  if (v->count == 3) {
    const CanonVal *props = v->items[2];
    if (props->type == CV_UNDEF || props->type == CV_NULL) {
      fail("length-3 surface with undef/null props (out of captured domain)");
    }
    if (props->type != CV_OBJ) fail("surface props not an object");
    s.hasProps = true;
    for (int i = 0; i < props->nkeys; i++) {
      if (strcmp(props->keys[i], "damageType") == 0) {
        s.propsHasDamageTypeKey = true;
        s.propsDamageType = cv_damage_value(props->vals[i]);
      } else {
        fail("surface props carries a key besides damageType");
      }
    }
  }
  return s;
}

static void cv_surface_list(const CanonVal *v, SurfaceList *out,
                            const char *what) {
  if (v->type != CV_ARR || v->count > ML_MAX_SURFACES) fail(what);
  out->count = v->count;
  for (int i = 0; i < v->count; i++) out->items[i] = cv_surface(v->items[i]);
}

static void marshal_stage5(const CanonVal *v, MlStageX *out) {
  memset(out, 0, sizeof *out);
  if (v->type == CV_NULL) return; // never expected here; defensive
  if (v->type != CV_OBJ || v->nkeys != 5) fail("stage projection shape");
  cv_surface_list(obj_req(v, "ceiling"), &out->s.ceiling, "stage.ceiling");
  cv_surface_list(obj_req(v, "ground"), &out->s.ground, "stage.ground");
  cv_surface_list(obj_req(v, "platform"), &out->s.platform, "stage.platform");
  cv_surface_list(obj_req(v, "wallL"), &out->s.wallL, "stage.wallL");
  cv_surface_list(obj_req(v, "wallR"), &out->s.wallR, "stage.wallR");
}

// --- inputs -----------------------------------------------------------------------------------
static MlInputBuffer g_in[4];

static void marshal_inputs(const CanonVal *v) {
  memset(g_in, 0, sizeof g_in);
  if (v->type == CV_NULL) return; // lean executeArticleHits records
  if (v->type != CV_ARR || v->count != 4) fail("inputs shape");
  for (int k = 0; k < 4; k++) {
    if (v->items[k]->type == CV_NULL) {
      if (g_sim.playerPresent[k]) fail("inputs: null for present slot");
      continue;
    }
    if (!g_sim.playerPresent[k]) fail("inputs: value for absent slot");
    g_in[k] = ml_input_buffer_from_canon(v->items[k]);
  }
}

// --- the article value model: canon <-> MlArticle ------------------------------------------------

// hb: the 12-key createHitbox key set, offset a single Vec2D (article.h).
static void cv_art_hb(const CanonVal *v, MlHitboxSpec *out) {
  memset(out, 0, sizeof *out);
  if (v->type != CV_OBJ || v->nkeys != 12) fail("article hb key count");
  static const char *const KEYS[12] = {
      "angle", "bk", "clank", "dmg", "hitAirborne", "hitGrounded",
      "kg", "offset", "size", "sk", "throwextra", "type"};
  for (int i = 0; i < 12; i++) {
    if (strcmp(v->keys[i], KEYS[i]) != 0) fail("article hb key set");
  }
  out->shape = ML_HB_CHARDATA;
  out->offsetSingle = true;
  out->angle = cv_num(v->vals[0]);
  out->bk = cv_num(v->vals[1]);
  out->clank = cv_num(v->vals[2]);
  out->dmg = cv_num(v->vals[3]);
  out->hitAirborne = cv_num(v->vals[4]);
  out->hitGrounded = cv_num(v->vals[5]);
  out->kg = cv_num(v->vals[6]);
  out->offset = cv_vec2(v->vals[7]);
  out->size = cv_num(v->vals[8]);
  out->sk = cv_num(v->vals[9]);
  out->throwextra = cv_bool(v->vals[10]);
  out->type = cv_num(v->vals[11]);
}

static void ser_vec(CanonBuf *b, Vec2D v) {
  cb_puts(b, "{\"x\":");
  cb_num(b, v.x);
  cb_puts(b, ",\"y\":");
  cb_num(b, v.y);
  cb_putc(b, '}');
}

static void ser_art_hb(CanonBuf *b, const MlHitboxSpec *hb) {
  cb_puts(b, "{\"angle\":");
  cb_num(b, hb->angle);
  cb_puts(b, ",\"bk\":");
  cb_num(b, hb->bk);
  cb_puts(b, ",\"clank\":");
  cb_num(b, hb->clank);
  cb_puts(b, ",\"dmg\":");
  cb_num(b, hb->dmg);
  cb_puts(b, ",\"hitAirborne\":");
  cb_num(b, hb->hitAirborne);
  cb_puts(b, ",\"hitGrounded\":");
  cb_num(b, hb->hitGrounded);
  cb_puts(b, ",\"kg\":");
  cb_num(b, hb->kg);
  cb_puts(b, ",\"offset\":");
  ser_vec(b, hb->offset);
  cb_puts(b, ",\"size\":");
  cb_num(b, hb->size);
  cb_puts(b, ",\"sk\":");
  cb_num(b, hb->sk);
  cb_puts(b, ",\"throwextra\":");
  cb_puts(b, hb->throwextra ? "T" : "F");
  cb_puts(b, ",\"type\":");
  cb_num(b, hb->type);
  cb_putc(b, '}');
}

static void cv_article(const CanonVal *v, MlArticle *out) {
  memset(out, 0, sizeof *out);
  if (v->type != CV_OBJ || v->nkeys != 3) fail("article entry shape");
  if (strcmp(v->keys[0], "instance") != 0 || strcmp(v->keys[1], "name") != 0 ||
      strcmp(v->keys[2], "player") != 0) {
    fail("article entry key set");
  }
  const char *name = cv_str(v->vals[1]);
  if (strcmp(name, "LASER") == 0) out->kind = ART_LASER;
  else if (strcmp(name, "ILLUSION") == 0) out->kind = ART_ILLUSION;
  else fail("article name domain");
  out->player = cv_num(v->vals[2]);
  const CanonVal *in = v->vals[0];
  const bool isLaser = out->kind == ART_LASER;
  // LASER instance: 13 keys; ILLUSION: 8 (survey-measured)
  if (in->type != CV_OBJ || in->nkeys != (isLaser ? 13 : 8)) {
    fail("article instance key count");
  }
  int k = 0;
  const CanonVal *val;
#define ART_KEY(K) \
  if (strcmp(in->keys[k], K) != 0) fail("article instance key set"); \
  val = in->vals[k]; \
  k++;
  ART_KEY("clank") out->clank = cv_bool(val);
  ART_KEY("destroyOnHit") out->destroyOnHit = cv_bool(val);
  ART_KEY("ecb") {
    if (val->type != CV_ARR || val->count != 4) fail("article ecb shape");
    for (int i = 0; i < 4; i++) out->ecb[i] = cv_vec2(val->items[i]);
  }
  ART_KEY("hb") cv_art_hb(val, &out->hb);
  ART_KEY("hitList") {
    if (val->type != CV_ARR || val->count > ART_HITLIST_CAP) {
      fail("article hitList shape");
    }
    out->hitListLen = val->count;
    for (int i = 0; i < val->count; i++) {
      out->hitList[i] = cv_num(val->items[i]);
    }
  }
  ART_KEY("pos") out->pos = cv_vec2(val);
  ART_KEY("posPrev") out->posPrev = cv_vec2(val);
  if (isLaser) {
    ART_KEY("posPrev1") out->posPrev1 = cv_vec2(val);
    ART_KEY("posPrev2") out->posPrev2 = cv_vec2(val);
    ART_KEY("posPrev3") out->posPrev3 = cv_vec2(val);
    ART_KEY("rotate") out->rotate = cv_num(val);
  }
  ART_KEY("timer") out->timer = cv_num(val);
  if (isLaser) {
    ART_KEY("vel") out->vel = cv_vec2(val);
  }
#undef ART_KEY
  if (k != in->nkeys) fail("article instance key walk");
  // consistency with the article table (LASER destroyOnHit/clank etc. are
  // authored constants — a mismatch means the capture domain drifted)
  if (isLaser && (out->destroyOnHit != true || out->clank != false)) {
    fail("LASER table constants drifted");
  }
  if (!isLaser && (out->destroyOnHit != false || out->clank != true)) {
    fail("ILLUSION table constants drifted");
  }
}

static void ser_article(CanonBuf *b, const MlArticle *it) {
  const bool isLaser = it->kind == ART_LASER;
  cb_puts(b, "{\"instance\":{\"clank\":");
  cb_puts(b, it->clank ? "T" : "F");
  cb_puts(b, ",\"destroyOnHit\":");
  cb_puts(b, it->destroyOnHit ? "T" : "F");
  cb_puts(b, ",\"ecb\":[");
  for (int i = 0; i < 4; i++) {
    if (i) cb_putc(b, ',');
    ser_vec(b, it->ecb[i]);
  }
  cb_puts(b, "],\"hb\":");
  ser_art_hb(b, &it->hb);
  cb_puts(b, ",\"hitList\":[");
  for (int i = 0; i < it->hitListLen; i++) {
    if (i) cb_putc(b, ',');
    cb_num(b, it->hitList[i]);
  }
  cb_puts(b, "],\"pos\":");
  ser_vec(b, it->pos);
  cb_puts(b, ",\"posPrev\":");
  ser_vec(b, it->posPrev);
  if (isLaser) {
    cb_puts(b, ",\"posPrev1\":");
    ser_vec(b, it->posPrev1);
    cb_puts(b, ",\"posPrev2\":");
    ser_vec(b, it->posPrev2);
    cb_puts(b, ",\"posPrev3\":");
    ser_vec(b, it->posPrev3);
    cb_puts(b, ",\"rotate\":");
    cb_num(b, it->rotate);
  }
  cb_puts(b, ",\"timer\":");
  cb_num(b, it->timer);
  if (isLaser) {
    cb_puts(b, ",\"vel\":");
    ser_vec(b, it->vel);
  }
  cb_puts(b, "},\"name\":");
  cb_qstr(b, isLaser ? "LASER" : "ILLUSION");
  cb_puts(b, ",\"player\":");
  cb_num(b, it->player);
  cb_putc(b, '}');
}

static void ser_aArt(CanonBuf *b, const MlArticles *A) {
  cb_putc(b, '[');
  for (int i = 0; i < A->count; i++) {
    if (i) cb_putc(b, ',');
    ser_article(b, &A->a[i]);
  }
  cb_putc(b, ']');
}

static void ser_dstq(CanonBuf *b, const MlArticles *A) {
  cb_putc(b, '[');
  for (int i = 0; i < A->destroyCount; i++) {
    if (i) cb_putc(b, ',');
    cb_num(b, A->destroyQ[i]);
  }
  cb_putc(b, ']');
}

static void ser_ahq(CanonBuf *b, const MlArticles *A) {
  cb_putc(b, '[');
  for (int i = 0; i < A->hitCount; i++) {
    if (i) cb_putc(b, ',');
    cb_putc(b, '[');
    cb_num(b, A->hitQ[i].a);
    cb_putc(b, ',');
    cb_num(b, A->hitQ[i].v);
    cb_putc(b, ',');
    cb_puts(b, A->hitQ[i].shieldHit ? "T" : "F");
    cb_putc(b, ']');
  }
  cb_putc(b, ']');
}

static void marshal_queues(const CanonVal *pre) {
  const CanonVal *aArt = obj_req(pre, "aArt");
  if (aArt->type != CV_ARR || aArt->count > ART_CAP) fail("aArt shape");
  g_art.count = aArt->count;
  for (int i = 0; i < aArt->count; i++) cv_article(aArt->items[i], &g_art.a[i]);
  const CanonVal *dstq = obj_req(pre, "dstq");
  if (dstq->type != CV_ARR || dstq->count > ART_CAP) fail("dstq shape");
  g_art.destroyCount = dstq->count;
  for (int i = 0; i < dstq->count; i++) {
    g_art.destroyQ[i] = cv_num(dstq->items[i]);
  }
  const CanonVal *ahq = obj_req(pre, "ahq");
  if (ahq->type != CV_ARR || ahq->count > ART_CAP) fail("ahq shape");
  g_art.hitCount = ahq->count;
  for (int i = 0; i < ahq->count; i++) {
    const CanonVal *row = ahq->items[i];
    if (row->type != CV_ARR || row->count != 3) fail("ahq row shape");
    g_art.hitQ[i].a = cv_num(row->items[0]);
    g_art.hitQ[i].v = cv_num(row->items[1]);
    g_art.hitQ[i].shieldHit = cv_bool(row->items[2]);
  }
}

// chain instrument: compare the CHAINED C queues against a record's pre
// queues (armed from the first in-match record on).
static void chain_check(const CanonVal *pre, long lineno) {
  static const char *const QKEYS[3] = {"aArt", "ahq", "dstq"};
  for (int q = 0; q < 3; q++) {
    char *want = reser_dup(obj_req(pre, QKEYS[q]));
    CanonBuf b;
    cb_init(&b);
    if (q == 0) ser_aArt(&b, &g_art);
    else if (q == 1) ser_ahq(&b, &g_art);
    else ser_dstq(&b, &g_art);
    if (strcmp(b.buf, want) != 0) {
      report_div(q == 0 ? "chain-aArt" : (q == 1 ? "chain-ahq" : "chain-dstq"),
                 lineno, want, b.buf);
    }
    cb_free(&b);
    free(want);
  }
}

// --- the oracle-fed mdispatch seam (shared moves.h mv_seam) ----------------------------
static const char *state_name(double charId, const char *state) {
  const int c = (int)charId;
  if (c < 0 || c >= ML_CHARS) mv_out_of_domain("state_name char");
  const int i = mv_state_idx(c, state);
  if (i == -1) mv_out_of_domain("state_name: unknown state key");
  return g_mv[c].name[i];
}

AsTri mv_seam(MlSim *S, double charId, const char *state, const char *phase,
              double slot, const MvX *ex) {
  if (ex != 0 && ex->count != 0) fail("seam dispatch with extras");
  SeamRec *r = fifo_pop();
  if (!r) {
    fprintf(stderr, "SEAM DIVERGENCE line ~%ld: C reached %s.%s but the "
                    "recording has no pending seam record\n",
            g_lineno, state, phase);
    report_div("seam-underflow", g_lineno, "(record)", state);
    longjmp(g_rec_jmp, 1);
  }
  CanonBuf b;
  cb_init(&b);
  cb_putc(&b, '[');
  cb_qstr(&b, phase);
  cb_putc(&b, ',');
  cb_qstr(&b, state_name(charId, state));
  cb_puts(&b, ",[");
  cb_num(&b, slot);
  cb_puts(&b, "]]");
  const bool args_ok = strcmp(b.buf, r->args) == 0;
  if (!args_ok) report_div("mdispatch-args", r->lineno, r->args, b.buf);
  cb_free(&b);

  AsTri ret = AS_UNDEF;
  char *post_dup = xstrdup(r->post);
  {
    if (strcmp(r->ret, "T") == 0) ret = AS_TRUE;
    else if (strcmp(r->ret, "F") == 0) ret = AS_FALSE;
    else if (strcmp(r->ret, "undef") == 0) ret = AS_UNDEF;
    else fail("seam ret domain");
  }
  const long seam_line = r->lineno;
  seam_free(r);

  // RESYNC from the recorded post {alias, hq, players, rng}. NOTE: the
  // caller record's pre tree is already fully marshaled — the arena reset
  // here is safe (the physics/hitdet/moves seam discipline).
  canon_arena_reset();
  const char *err = 0;
  const CanonVal *post = canon_parse(post_dup, &err);
  if (!post) {
    fprintf(stderr, "PARSE FAIL mdispatch post line %ld: %s\n", seam_line, err);
    exit(3);
  }
  if (post->type != CV_OBJ || post->nkeys != 4) fail("mdispatch post shape");
  const CanonVal *rng = obj_req(post, "rng");
  if (rng->type != CV_ARR) fail("mdispatch rng shape");
  for (int i = 0; i < rng->count; i++) {
    const double want = cv_num(rng->items[i]);
    const double got = ml_rng_next(g_active);
    if (memcmp(&want, &got, sizeof(double)) != 0) {
      CanonBuf wb, gb;
      cb_init(&wb);
      cb_init(&gb);
      cb_num(&wb, want);
      cb_num(&gb, got);
      report_div("mdispatch-rng", seam_line, wb.buf, gb.buf);
      cb_free(&wb);
      cb_free(&gb);
      free(post_dup);
      longjmp(g_rec_jmp, 1);
    }
  }
  marshal_players(S, obj_req(post, "players"));
  apply_alias4(S, obj_req(post, "alias"));
  set_hq_owned(reser_dup(obj_req(post, "hq")));
  free(post_dup);
  if (!args_ok) longjmp(g_rec_jmp, 1);
  return ret;
}

// --- options marshal for ainit records ------------------------------------------------
static ArtOptBool opt_bool(const CanonVal *opts, const char *key) {
  ArtOptBool o;
  o.has = false;
  o.v = false;
  const CanonVal *v = obj_get(opts, key);
  if (v) {
    o.has = true;
    o.v = cv_bool(v);
  }
  return o;
}

// --- main ---------------------------------------------------------------------------------------
int main(int argc, char **argv) {
  const char *path = NULL;
  bool strict = false, stop_first = false;
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--strict") == 0) strict = true;
    else if (strcmp(argv[i], "--stop-first") == 0) stop_first = true;
    else if (strcmp(argv[i], "--max-print") == 0 && i + 1 < argc)
      g_max_print = atoi(argv[++i]);
    else if (!path) path = argv[i];
    else { fprintf(stderr, "unknown arg: %s\n", argv[i]); return 1; }
  }
  if (!path) {
    fprintf(stderr, "usage: replay_article <capture.jsonl> [--strict] "
                    "[--max-print N] [--stop-first]\n");
    return 1;
  }
  FILE *f = fopen(path, "r");
  if (!f) { fprintf(stderr, "cannot open %s\n", path); return 1; }
  g_file = path;

  ml_rng_seed(&g_sweep, 0x0badf00d);
  memset(&g_art, 0, sizeof g_art);

  char *line = NULL;
  size_t linecap = 0;
  long replayed = 0;
  long pipe_records = 0, init_records = 0, seam_records = 0, rng_records = 0;

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
    *tab1 = 0;
    *tab2 = 0;
    *tab3 = 0;
    const long frame = atol(line);
    const char *fn = tab1 + 1;
    const char *args_s = tab2 + 1;
    char *ret_s = tab3 + 1;
    char *tab4 = strchr(ret_s, '\t');
    const char *post_s = NULL;
    if (tab4) { *tab4 = 0; post_s = tab4 + 1; }

    if (strcmp(fn, "mvData") == 0) {
      canon_arena_reset();
      const char *err = 0;
      const CanonVal *dump = canon_parse(ret_s, &err);
      if (!dump) { fprintf(stderr, "PARSE FAIL mvData: %s\n", err); return 3; }
      load_mvdata(dump);
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
      ml_rng_seed(&g_seeded, (uint32_t)seed);
      for (long i = 0; i < (long)boot; i++) (void)ml_rng_next(&g_seeded);
      out.len = 0;
      out.buf[0] = 0;
      cb_num(&out, (double)g_seeded.a);
      if (strcmp(out.buf, ret_s) != 0) {
        report_div("rngBoot", g_lineno, ret_s, out.buf);
      }
      g_boot_seen = true;
      replayed++;
      continue;
    }

    if (strcmp(fn, "Math.random") == 0) {
      if (!g_boot_seen) fail("Math.random before rngBoot");
      const double v = ml_rng_next(&g_seeded);
      out.len = 0;
      out.buf[0] = 0;
      cb_num(&out, v);
      if (strcmp(out.buf, ret_s) != 0) {
        report_div("Math.random", g_lineno, ret_s, out.buf);
        if (stop_first) break;
      }
      rng_records++;
      replayed++;
      continue;
    }

    if (strcmp(fn, "mdispatch") == 0) {
      if (!post_s) fail("mdispatch: missing post");
      fifo_push(g_lineno, args_s, ret_s, post_s);
      seam_records++;
      replayed++;
      continue;
    }

    // --- article-family records --------------------------------------------------
    const bool isInit = strcmp(fn, "ainit") == 0;
    const bool isExec = strcmp(fn, "executeArticles") == 0;
    const bool isDestroy = strcmp(fn, "destroyArticles") == 0;
    const bool isReset = strcmp(fn, "resetAArticles") == 0;
    const bool isAhd = strcmp(fn, "articlesHitDetection") == 0;
    const bool isEah = strcmp(fn, "executeArticleHits") == 0;
    if (!isInit && !isExec && !isDestroy && !isReset && !isAhd && !isEah) {
      fail("unknown record function");
    }
    if (!post_s) fail("article record: missing post-state field");
    if (!g_mv_loaded) fail("article record before mvData");
    if (!g_ft_loaded) fail("article record before hdFlags");
    if (!g_boot_seen) fail("article record before rngBoot");
    if (strcmp(ret_s, "undef") != 0) fail("article record ret domain");

    canon_arena_reset();
    const char *err = 0;
    const CanonVal *args = canon_parse(args_s, &err);
    if (!args) {
      fprintf(stderr, "PARSE FAIL %s:%ld args: %s\n", path, g_lineno, err);
      return 3;
    }
    if (args->type != CV_ARR) fail("args shape");
    const CanonVal *pre = NULL;
    const CanonVal *opts = NULL;
    const char *initName = NULL;
    const CanonVal *inputs = NULL;
    if (isInit) {
      if (args->count != 3) fail("ainit args shape");
      initName = cv_str(args->items[0]);
      opts = args->items[1];
      pre = args->items[2];
    } else if (isEah) {
      if (args->count != 2) fail("executeArticleHits args shape");
      inputs = args->items[0];
      pre = args->items[1];
    } else {
      if (args->count != 1) fail("pipeline args shape");
      pre = args->items[0];
    }
    if (pre->type != CV_OBJ) fail("pre shape");

    // envelope kind by key count (lean-when-empty, FORMAT.md):
    // lean 3 {aArt,ahq,dstq} · spawn 6 (+playerType,players,stage) ·
    // ahd-full 5 (+playerType,players) · eah-full 12 (moves superset)
    const bool lean = pre->nkeys == 3;
    if (isInit && lean) fail("ainit records are never lean");
    if ((isDestroy || isReset) && !lean) fail("destroy/reset must be lean");

    // chain instrument (in-match records only; sweep pokes are direct)
    if (g_chain_armed && frame > 0) chain_check(pre, g_lineno);

    memset(&g_sim, 0, sizeof g_sim);
    marshal_queues(&(*pre)); // authoritative pre queues
    if (!lean) {
      if (isInit || isExec) {
        if (pre->nkeys != 6) fail("spawn envelope key count");
        marshal_playerType(&g_sim, obj_req(pre, "playerType"));
        marshal_players(&g_sim, obj_req(pre, "players"));
        marshal_stage5(obj_req(pre, "stage"), &g_sim.stage);
      } else if (isAhd) {
        if (pre->nkeys != 5) fail("ahd envelope key count");
        marshal_playerType(&g_sim, obj_req(pre, "playerType"));
        marshal_players(&g_sim, obj_req(pre, "players"));
      } else { // eah full
        if (pre->nkeys != 12) fail("eah envelope key count");
        marshal_playerType(&g_sim, obj_req(pre, "playerType"));
        const CanonVal *cs = obj_req(pre, "characterSelections");
        if (cs->type != CV_ARR || cs->count != 4) {
          fail("characterSelections shape");
        }
        for (int k = 0; k < 4; k++) {
          g_sim.characterSelections[k] = cv_num(cs->items[k]);
        }
        g_sim.gameMode = cv_num(obj_req(pre, "gameMode"));
        g_sim.versusMode = cv_truthy_num(obj_req(pre, "versusMode"));
        const CanonVal *gs = obj_req(pre, "gameSettings");
        if (gs->type != CV_OBJ || gs->nkeys != 4) fail("gameSettings shape");
        g_sim.tapJumpOff[0] = cv_num(obj_req(gs, "tapJumpOffp1"));
        g_sim.tapJumpOff[1] = cv_num(obj_req(gs, "tapJumpOffp2"));
        g_sim.tapJumpOff[2] = cv_num(obj_req(gs, "tapJumpOffp3"));
        g_sim.tapJumpOff[3] = cv_num(obj_req(gs, "tapJumpOffp4"));
        marshal_players(&g_sim, obj_req(pre, "players"));
        apply_alias4(&g_sim, obj_req(pre, "alias"));
        marshal_stage5(obj_req(pre, "stage"), &g_sim.stage);
        set_hq_owned(reser_dup(obj_req(pre, "hq")));
      }
    }
    if (isEah) marshal_inputs(inputs ? inputs : NULL);

    // ainit options (marshaled BEFORE the arena can be reset by a seam)
    double o_p = 0, o_x = 0, o_y = 0, o_rotate = 0, o_type = 0;
    ArtOptBool o_isFox = {false, false}, o_partOfThrow = {false, false};
    bool initIsLaser = false;
    if (isInit) {
      if (opts->type != CV_OBJ) fail("ainit options shape");
      initIsLaser = strcmp(initName, "LASER") == 0;
      if (!initIsLaser && strcmp(initName, "ILLUSION") != 0) {
        fail("ainit name domain");
      }
      int expected = 0;
      o_p = cv_num(obj_req(opts, "p"));
      expected++;
      if (initIsLaser) {
        o_x = cv_num(obj_req(opts, "x"));
        o_y = cv_num(obj_req(opts, "y"));
        o_rotate = cv_num(obj_req(opts, "rotate"));
        expected += 3;
        o_isFox = opt_bool(opts, "isFox");
        if (o_isFox.has) expected++;
        o_partOfThrow = opt_bool(opts, "partOfThrow");
        if (o_partOfThrow.has) expected++;
      } else {
        o_type = cv_num(obj_req(opts, "type"));
        expected++;
        o_isFox = opt_bool(opts, "isFox");
        if (o_isFox.has) expected++;
      }
      if (opts->nkeys != expected) fail("ainit options key set");
    }

    // frame-0 records are the rule-12 sweep: separate sweep chain
    g_active = frame == 0 ? &g_sweep : &g_seeded;
    ml_active_rng = g_active;
    ml_ev_reset();

    if (setjmp(g_rec_jmp) == 0) {
      if (isInit) {
        if (initIsLaser) {
          art_laser_init(&g_sim, &g_art, o_p, o_x, o_y, o_rotate, o_isFox,
                         o_partOfThrow);
        } else {
          art_illusion_init(&g_sim, &g_art, o_p, o_type, o_isFox);
        }
        if (ml_events.rng_count || ml_events.snd_count || ml_events.vfx_count) {
          fail("events escaped an article init");
        }
      } else if (isExec) {
        art_executeArticles(&g_sim, &g_art);
        if (ml_events.rng_count || ml_events.snd_count || ml_events.vfx_count) {
          fail("events escaped executeArticles");
        }
      } else if (isDestroy) {
        art_destroyArticles(&g_art);
      } else if (isReset) {
        art_resetAArticles(&g_art);
      } else if (isAhd) {
        art_articlesHitDetection(&g_sim, &g_art);
        if (ml_events.rng_count || ml_events.vfx_count) {
          fail("rng/vfx escaped articlesHitDetection");
        }
      } else {
        art_executeArticleHits(&g_sim, &g_art, g_in);
      }

      if (g_fifo_len != 0) {
        SeamRec *r = fifo_pop();
        fprintf(stderr, "SEAM DIVERGENCE line %ld: recorded seam never "
                        "reached by the C body\n",
                r->lineno);
        report_div("seam-unconsumed", r->lineno, r->args, "(not reached)");
        seam_free(r);
        fifo_drain();
      } else {
        // post envelope
        out.len = 0;
        out.buf[0] = 0;
        const bool postLean = isInit || isExec || isDestroy || isReset || lean;
        if (postLean) {
          cb_puts(&out, "{\"aArt\":");
          ser_aArt(&out, &g_art);
          cb_puts(&out, ",\"ahq\":");
          ser_ahq(&out, &g_art);
          cb_puts(&out, ",\"dstq\":");
          ser_dstq(&out, &g_art);
          cb_putc(&out, '}');
        } else if (isAhd) {
          cb_puts(&out, "{\"aArt\":");
          ser_aArt(&out, &g_art);
          cb_puts(&out, ",\"ahq\":");
          ser_ahq(&out, &g_art);
          cb_puts(&out, ",\"dstq\":");
          ser_dstq(&out, &g_art);
          cb_puts(&out, ",\"players\":");
          ser_players(&out, &g_sim);
          cb_puts(&out, ",\"snd\":[");
          for (int s = 0; s < ml_events.snd_count; s++) {
            if (s) cb_putc(&out, ',');
            cb_qstr(&out, ml_events.snd[s]);
          }
          cb_puts(&out, "]}");
        } else { // eah full
          cb_puts(&out, "{\"aArt\":");
          ser_aArt(&out, &g_art);
          cb_puts(&out, ",\"ahq\":");
          ser_ahq(&out, &g_art);
          cb_puts(&out, ",\"alias\":");
          ser_alias4(&out, &g_sim);
          cb_puts(&out, ",\"dstq\":");
          ser_dstq(&out, &g_art);
          cb_puts(&out, ",\"hq\":");
          cb_puts(&out, g_hq);
          cb_puts(&out, ",\"players\":");
          ser_players(&out, &g_sim);
          cb_puts(&out, ",\"rng\":[");
          for (int i = 0; i < ml_events.rng_count; i++) {
            if (i) cb_putc(&out, ',');
            cb_num(&out, ml_events.rng[i]);
          }
          cb_puts(&out, "],\"snd\":[");
          for (int s = 0; s < ml_events.snd_count; s++) {
            if (s) cb_putc(&out, ',');
            cb_qstr(&out, ml_events.snd[s]);
          }
          cb_puts(&out, "],\"vfx\":[");
          for (int s = 0; s < ml_events.vfx_count; s++) {
            if (s) cb_putc(&out, ',');
            cb_qstr(&out, ml_events.vfx[s]);
          }
          cb_puts(&out, "]}");
        }
        if (strcmp(out.buf, post_s) != 0) {
          report_div(fn, g_lineno, post_s, out.buf);
        }
      }
    } else {
      fifo_drain(); // seam mismatch aborted the record
    }

    if (frame > 0) g_chain_armed = true;
    if (isInit) init_records++;
    else pipe_records++;
    replayed++;
    if (g_divergences > 0 && stop_first) break;
  }
  free(line);
  fclose(f);

  if (!g_mv_loaded) fail("capture carries no mvData record");
  if (!g_ft_loaded) fail("capture carries no hdFlags record");
  if (!g_boot_seen) fail("capture carries no rngBoot record");
  if (g_fifo_len != 0) {
    fprintf(stderr, "SEAM DIVERGENCE: %d seam records left unconsumed at EOF\n",
            g_fifo_len);
    g_divergences += g_fifo_len;
    fifo_drain();
  }

  fprintf(stderr,
          "replayed: %ld pipeline records, %ld ainit records, %ld mdispatch "
          "seams consumed in call order, %ld standalone draws\n",
          pipe_records, init_records, seam_records, rng_records);
  printf("ARTICLE REPLAY RAN %ld records, %ld divergences", replayed,
         g_divergences);
  if (g_first_div_line != -1) printf(" (first at line %ld)", g_first_div_line);
  printf("\n");
  cb_free(&out);
  if (strict && g_divergences > 0) return 2;
  return 0;
}
