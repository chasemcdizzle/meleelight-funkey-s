// replay_moves_shared.c — M2 task 7 replay driver: feeds recorded
// moves-shared boundary calls (build/<id>.moves-shared.jsonl, FORMAT.md
// "The moves-shared spec") to the C shared-move translations
// (port/sim/characters/shared/) and compares canon-v1.1 serializations
// byte-for-byte. A single differing bit anywhere is a divergence.
//
// Record types:
// - mvData (frame 0): loads the executed move-data plane (per-char state
//   name map + sharedOrigin registry + setVelocities/posOffset/
//   setPositions/actionSounds/palettes) and REGISTERS the shared C bodies
//   into the task-4 actionStates scaffolding per the measured
//   sharedOrigin map (puff lacks FURAFURA/JUMPAERIALB/JUMPAERIALF).
// - rngBoot / Math.random: the ONE chained seeded mulberry32 (asshort
//   discipline). Frame-0 "move" records replay against a SEPARATE sweep
//   mulberry32 (seed 0x0badf00d) chained across all sweep records — the
//   spec swaps Math.random for the sweep generator pre-match (rule 12).
// - mdispatch: FIFO-queued oracle-fed seams consumed by the following
//   move record IN CALL ORDER: args (phase/name/slot) verified
//   bit-exactly, the window's rng list advanced+verified on the active
//   chain, players/alias/hq RESYNCED from the recorded post.
// - move: marshal the PRE state (players/stage/globals/aliases/inputs),
//   run the C body via mv_dispatch, compare ret + the
//   {alias,hq,players,rng,snd,vfx} post envelope bit-exactly. hq is
//   carried as an OPAQUE reserialized canon string (no shared move
//   touches the hitQueue — no hitDetection import upstream; seam resyncs
//   swap the whole string).
//
// Marshalling is STRICT (prevention rule 7): any shape outside the
// captured domain aborts with exit 3 — never guess.
//
// Usage: replay_moves_shared <capture.jsonl> [--strict] [--max-print N]
//                            [--stop-first]
#include <inttypes.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
void mv_out_of_domain(const char *what) {
  fprintf(stderr, "OUT OF DOMAIN %s:%ld: %s\n", g_file, g_lineno, what);
  exit(3);
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

// --- generic canon reserializer (opaque hq carrying) ---------------------------
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

// --- the mvData tables -----------------------------------------------------------
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
  // setVelocities[6]: DOWNSTANDB, DOWNSTANDF, ESCAPEB, ESCAPEF, TECHB, TECHF
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
      // shared entries: table key == move object name (registry contract)
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

// --- the mvData-backed data seams (moves.h) ---------------------------------------
double mv_setVelocity(double charId, const char *moveKey, double idx) {
  const int c = (int)charId;
  if (c < 0 || c >= ML_CHARS) mv_out_of_domain("setVelocity char");
  for (int k = 0; k < 6; k++) {
    if (strcmp(SETVEL_KEYS[k], moveKey) == 0) {
      const int i = (int)idx;
      if (idx != (double)i || i < 0 || i >= g_mv[c].setVelLen[k]) {
        return js_nan(); // setVelocities[t] === undefined -> NaN in arithmetic
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

// --- RNG chains --------------------------------------------------------------------
static MlRng g_seeded;
static MlRng g_sweep;
static MlRng *g_active = 0;
static bool g_boot_seen = false;

// --- pending-seam FIFO ----------------------------------------------------------------
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

// --- alias probes ---------------------------------------------------------------------
static MlSim g_sim;
static char *g_hq = 0; // opaque canon string (header note)

static void set_hq_owned(char *s) {
  if (g_hq) free(g_hq);
  g_hq = s;
}

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

// --- stage (moves projection: ground/ledge/platform/respawn) ----------------------------
static void marshal_surface_list(const CanonVal *v, SurfaceList *out,
                                 const char *what) {
  if (v->type != CV_ARR || v->count > ML_MAX_SURFACES) fail(what);
  out->count = v->count;
  for (int i = 0; i < v->count; i++) {
    const CanonVal *s = v->items[i];
    if (s->type != CV_ARR || s->count != 2) fail(what);
    Surface *sf = &out->items[i];
    memset(sf, 0, sizeof *sf);
    sf->p0 = cv_vec2(s->items[0]);
    sf->p1 = cv_vec2(s->items[1]);
    sf->hasProps = false;
    sf->propsHasDamageTypeKey = false;
    sf->propsDamageType = damage_absent();
  }
}

static void marshal_stage(const CanonVal *v, MlStageX *out) {
  memset(out, 0, sizeof *out);
  if (v->type == CV_NULL) return; // sweep records run before setupMatch
  if (v->type != CV_OBJ || v->nkeys != 5) fail("stage projection shape");
  marshal_surface_list(obj_req(v, "ground"), &out->s.ground, "stage.ground");
  marshal_surface_list(obj_req(v, "platform"), &out->s.platform,
                       "stage.platform");
  const CanonVal *ledge = obj_req(v, "ledge");
  if (ledge->type != CV_ARR || ledge->count > ML_MAX_LEDGES) fail("stage.ledge");
  out->ledgeCount = ledge->count;
  for (int i = 0; i < ledge->count; i++) {
    const CanonVal *L = ledge->items[i];
    if (L->type != CV_ARR || L->count != 3) fail("stage.ledge entry");
    const char *list = cv_str(L->items[0]);
    if (strcmp(list, "ground") == 0) out->ledge[i].list = 'g';
    else if (strcmp(list, "platform") == 0) out->ledge[i].list = 'p';
    else fail("stage.ledge list name");
    out->ledge[i].index = cv_num(L->items[1]);
    out->ledge[i].point = cv_num(L->items[2]);
  }
  const CanonVal *rp = obj_req(v, "respawnPoints");
  const CanonVal *rf = obj_req(v, "respawnFace");
  if (rp->type != CV_ARR || rp->count > 4 || rf->type != CV_ARR ||
      rf->count != rp->count) {
    fail("stage.respawn shape");
  }
  out->respawnCount = rp->count;
  for (int i = 0; i < rp->count; i++) {
    out->respawnPoints[i] = cv_vec2(rp->items[i]);
    out->respawnFace[i] = cv_num(rf->items[i]);
  }
}

// --- inputs -------------------------------------------------------------------------------
static MlInputBuffer g_in[4];

static void marshal_inputs(const CanonVal *v) {
  memset(g_in, 0, sizeof g_in);
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

// --- extras -------------------------------------------------------------------------------
static void marshal_extras(const CanonVal *slotExtras, double *slot, MvX *ex) {
  if (slotExtras->type != CV_ARR || slotExtras->count < 1 ||
      slotExtras->count > 3) {
    fail("[slot, ...extras] shape");
  }
  *slot = cv_num(slotExtras->items[0]);
  ex->count = slotExtras->count - 1;
  for (int i = 0; i < ex->count; i++) {
    const CanonVal *e = slotExtras->items[i + 1];
    MlDispExtra *x = &ex->x[i];
    memset(x, 0, sizeof *x);
    switch (e->type) {
      case CV_NUM: x->kind = DX_NUM; x->num = e->num; break;
      case CV_BOOL: x->kind = DX_BOOL; x->b = e->b; break;
      case CV_STR:
        // arena strings do not survive seam resyncs, and no upstream move
        // extra is a string — outside the captured domain (rule 7)
        fail("string extra arg");
        break;
      case CV_OBJ: x->kind = DX_VEC; x->vec = cv_vec2(e); break;
      case CV_NULL:
        // STOPCEIL's explicit `normal = null` default never appears as an
        // ARG upstream (callers omit it) — outside the captured domain.
        fail("null extra arg");
        break;
      default: fail("extra arg kind");
    }
  }
}

// --- the oracle-fed seam (moves.h mv_seam) ---------------------------------------------------
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
    // ret: T | F | undef (per-char inits return undefined)
    if (strcmp(r->ret, "T") == 0) ret = AS_TRUE;
    else if (strcmp(r->ret, "F") == 0) ret = AS_FALSE;
    else if (strcmp(r->ret, "undef") == 0) ret = AS_UNDEF;
    else fail("seam ret domain");
  }
  const long seam_line = r->lineno;
  seam_free(r);

  // RESYNC from the recorded post {alias, hq, players, rng}
  canon_arena_reset();
  const char *err = 0;
  const CanonVal *post = canon_parse(post_dup, &err);
  if (!post) {
    fprintf(stderr, "PARSE FAIL mdispatch post line %ld: %s\n", seam_line, err);
    exit(3);
  }
  if (post->type != CV_OBJ || post->nkeys != 4) fail("mdispatch post shape");
  // window draws: advance the ACTIVE chain draw-for-draw (rule 14
  // strengthened: the seam sits at an exact structural point of the C
  // body, so owner/window interleaving is fully determined)
  const CanonVal *rng = obj_req(post, "rng");
  if (rng->type != CV_ARR) fail("mdispatch rng shape");
  for (int i = 0; i < rng->count; i++) {
    const double want = cv_num(rng->items[i]);
    const double got = ml_rng_next(g_active);
    if (memcmp(&want, &got, sizeof(double)) != 0) {
      CanonBuf wb, gb;
      cb_init(&wb); cb_init(&gb);
      cb_num(&wb, want); cb_num(&gb, got);
      report_div("mdispatch-rng", seam_line, wb.buf, gb.buf);
      cb_free(&wb); cb_free(&gb);
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
    fprintf(stderr, "usage: replay_moves_shared <capture.jsonl> [--strict] "
                    "[--max-print N] [--stop-first]\n");
    return 1;
  }
  FILE *f = fopen(path, "r");
  if (!f) { fprintf(stderr, "cannot open %s\n", path); return 1; }
  g_file = path;

  ml_rng_seed(&g_sweep, 0x0badf00d);

  char *line = NULL;
  size_t linecap = 0;
  long replayed = 0;
  long move_records = 0, seam_records = 0, rng_records = 0;

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
      out.len = 0; out.buf[0] = 0;
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

    if (strcmp(fn, "mdispatch") == 0) {
      if (!post_s) fail("mdispatch: missing post");
      fifo_push(g_lineno, args_s, ret_s, post_s);
      seam_records++;
      replayed++;
      continue;
    }

    if (strcmp(fn, "move") != 0) fail("unknown record function");
    if (!post_s) fail("move: missing post-state field");
    if (!g_mv_loaded) fail("move record before mvData");
    if (!g_boot_seen) fail("move record before rngBoot");

    // args: [phase, name, [slot, ...extras], inputs, pre]
    canon_arena_reset();
    const char *err = 0;
    const CanonVal *args = canon_parse(args_s, &err);
    if (!args) {
      fprintf(stderr, "PARSE FAIL %s:%ld args: %s\n", path, g_lineno, err);
      return 3;
    }
    if (args->type != CV_ARR || args->count != 5) fail("move args shape");
    const char *phase = cv_str(args->items[0]);
    const char *mname = cv_str(args->items[1]);
    double slot = 0;
    MvX ex;
    marshal_extras(args->items[2], &slot, &ex);
    const CanonVal *pre = args->items[4];
    if (pre->type != CV_OBJ || pre->nkeys != 9) fail("move pre shape");

    memset(&g_sim, 0, sizeof g_sim);
    const CanonVal *pt = obj_req(pre, "playerType");
    if (pt->type != CV_ARR || pt->count != 4) fail("playerType shape");
    for (int k = 0; k < 4; k++) {
      g_sim.playerType[k] = cv_num(pt->items[k]);
      g_sim.playerPresent[k] = g_sim.playerType[k] > -1;
    }
    const CanonVal *cs = obj_req(pre, "characterSelections");
    if (cs->type != CV_ARR || cs->count != 4) fail("characterSelections shape");
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
    marshal_stage(obj_req(pre, "stage"), &g_sim.stage);
    set_hq_owned(reser_dup(obj_req(pre, "hq")));
    marshal_inputs(args->items[3]);

    const int islot = (int)slot;
    if (islot < 0 || islot > 3 || !g_sim.playerPresent[islot]) {
      fail("move: absent slot");
    }
    // the record's move name is the actionStates KEY for shared moves
    // (registry contract asserted at mvData load)
    const double charId = g_sim.characterSelections[islot];
    if (as_lookup((int)charId, mname) == 0) {
      fail("move record for an unregistered (non-shared) state");
    }

    // frame-0 move records are the rule-12 sweep (plus the zero-draw
    // setup-time ENTRANCE inits): they replay on the SWEEP chain
    g_active = frame == 0 ? &g_sweep : &g_seeded;
    ml_active_rng = g_active;
    ml_ev_reset();

    if (setjmp(g_rec_jmp) == 0) {
      const AsTri ret = mv_dispatch(&g_sim, charId, mname, phase, slot, g_in,
                                    ex.count ? &ex : 0);
      // ret compare
      const char *retc = ret == AS_TRUE ? "T" : (ret == AS_FALSE ? "F"
                                                                 : "undef");
      if (strcmp(retc, ret_s) != 0) {
        report_div("move-ret", g_lineno, ret_s, retc);
      }
      if (g_fifo_len != 0) {
        SeamRec *r = fifo_pop();
        fprintf(stderr, "SEAM DIVERGENCE line %ld: recorded mdispatch never "
                        "reached by the C move\n", r->lineno);
        report_div("seam-unconsumed", r->lineno, r->args, "(not reached)");
        seam_free(r);
        fifo_drain();
      } else {
        out.len = 0; out.buf[0] = 0;
        cb_puts(&out, "{\"alias\":");
        ser_alias4(&out, &g_sim);
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
          cb_vfx(&out, &ml_events.vfx[s]); // M4 task 1: full-config canon
        }
        cb_puts(&out, "]}");
        if (strcmp(out.buf, post_s) != 0) {
          report_div("move-post", g_lineno, post_s, out.buf);
        }
      }
    } else {
      fifo_drain(); // seam mismatch aborted the record
    }

    move_records++;
    replayed++;
    if (g_divergences > 0 && stop_first) break;
  }
  free(line);
  fclose(f);

  if (!g_mv_loaded) fail("capture carries no mvData record");
  if (!g_boot_seen) fail("capture carries no rngBoot record");
  if (g_fifo_len != 0) {
    fprintf(stderr, "SEAM DIVERGENCE: %d seam records left unconsumed at EOF\n",
            g_fifo_len);
    g_divergences += g_fifo_len;
    fifo_drain();
  }

  fprintf(stderr,
          "replayed: %ld move records, %ld seams consumed in call order, "
          "%ld standalone draws\n",
          move_records, seam_records, rng_records);
  printf("MOVES-SHARED REPLAY RAN %ld records, %ld divergences", replayed,
         g_divergences);
  if (g_first_div_line != -1) printf(" (first at line %ld)", g_first_div_line);
  printf("\n");
  cb_free(&out);
  if (strict && g_divergences > 0) return 2;
  return 0;
}
