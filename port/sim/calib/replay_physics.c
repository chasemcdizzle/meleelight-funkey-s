// replay_physics.c — M2 task 5 replay driver: feeds recorded physics-spec
// boundary calls (build/<id>.physics.jsonl, FORMAT.md "The physics spec")
// to the C translations and compares canon-v1.1 serializations
// byte-for-byte. A single differing bit anywhere is a divergence.
//
// Record types:
// - asFlags (frame 0): loads the actionStates flag table (physics.c's
//   mlp_flags seam); drift-guarded in the capture by finalCheck().
// - sweepCircleVsSweepCircle / sweepCircleVsAABB: PURE replays through
//   port/sim/interpolated_collision.c (live + rule-11 sweep records).
// - getLaunchAngle / getHorizontal|VerticalVelocity / -Decay and
//   dispatch: FIFO-queued seam records consumed by the following physics
//   record IN CALL ORDER — a C physics that reaches a seam out of order,
//   with different arguments, or fails to reach one, diverges.
// - physics: marshal the PRE state (players/stage/globals/alias probes)
//   from the args envelope, run ml_physics (port/sim/physics.c), compare
//   the {alias,hq,players,snd} post envelope bit-exactly.
//   ecbSquashData is physics.js MODULE state: chained across the whole
//   file (initialized to the shared nullSquashDatum value, evolved only
//   by the C code — physics.h note 3).
//
// Marshalling is STRICT (prevention rule 7): any shape outside the
// captured domain aborts with exit 3 — never guess.
//
// Usage: replay_physics <capture.jsonl> [--strict] [--max-print N]
//                       [--stop-first]
#include <inttypes.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../interpolated_collision.h"
#include "../ml_events.h"
#include "../physics.h"
#include "canon.h"
#include "ml_tables.h" // ML_CHARS
#include "input_canon.h"
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

// --- divergence accounting ------------------------------------------------------

static long g_divergences = 0;
static long g_first_div_line = -1;
static long g_printed = 0;
static int g_max_print = 5;
static jmp_buf g_rec_jmp; // seam mismatch aborts the current physics record

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
static const char *cv_str(const CanonVal *v) {
  if (v->type != CV_STR) fail("expected string");
  return v->str;
}
static double cv_truthy_num(const CanonVal *v) {
  // number|boolean truthiness domain (gameSettings.turbo, versusMode)
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
static void cv_input4(const CanonVal *v, MlInput in[4]) {
  if (v->type != CV_ARR || v->count != 4) fail("expected 4-deep input slice");
  for (int i = 0; i < 4; i++) in[i] = ml_input_from_canon(v->items[i]);
}

// --- the actionStates flag table (asFlags record -> mlp_flags seam) -------------

#define FT_STATES 256
#define FT_STR 48

typedef struct {
  char state[FT_STR];
  char name[FT_STR];
  char airborne[FT_STR];
  MlAsFlags f;
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
  if (g_ft_loaded) fail("asFlags: seen twice");
  if (dump->type != CV_OBJ) fail("asFlags: expected object");
  for (int ci = 0; ci < dump->nkeys; ci++) {
    const int c = atoi(dump->keys[ci]);
    if (c < 0 || c >= ML_CHARS) fail("asFlags: char key out of range");
    const CanonVal *tbl = dump->vals[ci];
    if (tbl->type != CV_OBJ) fail("asFlags: expected state table object");
    if (tbl->nkeys > FT_STATES) fail("asFlags: too many states");
    g_ft_count[c] = tbl->nkeys;
    for (int si = 0; si < tbl->nkeys; si++) {
      FtEntry *e = &g_ft[c][si];
      memset(e, 0, sizeof *e);
      ft_copy_str(e->state, tbl->keys[si], "asFlags: state name too long");
      const CanonVal *fo = tbl->vals[si];
      if (fo->type != CV_OBJ || fo->nkeys != 14) fail("asFlags: flag key count");
      const CanonVal *v;
      v = obj_req(fo, "name");
      ft_copy_str(e->name, cv_str(v), "asFlags: move name too long");
      e->f.name = e->name;
      e->f.canEdgeCancel = ft_truthy(obj_req(fo, "canEdgeCancel"), "canEdgeCancel");
      e->f.disableTeeter = ft_truthy(obj_req(fo, "disableTeeter"), "disableTeeter");
      e->f.inGrab = ft_truthy(obj_req(fo, "inGrab"), "inGrab");
      e->f.headBonk = ft_truthy(obj_req(fo, "headBonk"), "headBonk");
      e->f.specialWallCollide =
          ft_truthy(obj_req(fo, "specialWallCollide"), "specialWallCollide");
      e->f.canPassThrough =
          ft_truthy(obj_req(fo, "canPassThrough"), "canPassThrough");
      e->f.dead = ft_truthy(obj_req(fo, "dead"), "dead");
      e->f.missfoot = ft_truthy(obj_req(fo, "missfoot"), "missfoot");
      e->f.ignoreCollision =
          ft_truthy(obj_req(fo, "ignoreCollision"), "ignoreCollision");
      v = obj_req(fo, "wallJumpAble");
      if (v->type == CV_UNDEF) e->f.wallJumpAble = js_bool_undef();
      else if (v->type == CV_BOOL) e->f.wallJumpAble = js_bool(v->b);
      else fail("asFlags: wallJumpAble domain");
      v = obj_req(fo, "landType");
      if (v->type == CV_UNDEF) e->f.hasLandType = false;
      else { e->f.hasLandType = true; e->f.landType = cv_num(v); }
      v = obj_req(fo, "airborneState");
      if (v->type == CV_UNDEF) e->f.hasAirborneState = false;
      else {
        e->f.hasAirborneState = true;
        ft_copy_str(e->airborne, cv_str(v), "asFlags: airborneState too long");
        // MlAsFlags carries the value inline:
        if (strlen(e->airborne) >= ML_STR_CAP) fail("airborneState cap");
        strcpy(e->f.airborneState, e->airborne);
      }
      v = obj_req(fo, "canGrabLedge");
      if (v->type == CV_UNDEF) {
        // canGrabLedge[0] on undefined THROWS upstream — hasCanGrabLedge
        // false makes the C read trap (mirrors the throw).
        e->f.hasCanGrabLedge = false;
      } else if (v->type == CV_BOOL) {
        // measured domain includes plain `false` (15 states): boolean[0]
        // is undefined in JS (no throw) — both element reads are falsy.
        e->f.hasCanGrabLedge = true;
        e->f.canGrabLedge[0] = false;
        e->f.canGrabLedge[1] = false;
      } else {
        if (v->type != CV_ARR || v->count != 2) fail("asFlags: canGrabLedge shape");
        e->f.hasCanGrabLedge = true;
        e->f.canGrabLedge[0] = ft_truthy(v->items[0], "canGrabLedge[0]");
        e->f.canGrabLedge[1] = ft_truthy(v->items[1], "canGrabLedge[1]");
      }
    }
  }
  g_ft_loaded = true;
}

const MlAsFlags *mlp_flags(const MlSim *sim, double charId,
                           const char *state) {
  (void)sim;
  const int c = (int)charId;
  if (!g_ft_loaded) fail("mlp_flags before asFlags record");
  if (c < 0 || c >= ML_CHARS) ml_phys_out_of_domain("mlp_flags char");
  for (int k = 0; k < g_ft_count[c]; k++) {
    if (strcmp(g_ft[c][k].state, state) == 0) return &g_ft[c][k].f;
  }
  ml_phys_out_of_domain("mlp_flags: unknown action state");
  return 0;
}

// --- pending-seam FIFO (dispatch + getter records) ----------------------------

#define FIFO_CAP 128

typedef struct {
  long lineno;
  char *fn;
  char *args;
  char *ret;
  char *post; // dispatch only
} SeamRec;

static SeamRec g_fifo[FIFO_CAP];
static int g_fifo_head = 0, g_fifo_len = 0;

static char *xstrdup(const char *s) {
  char *d = strdup(s);
  if (!d) { fprintf(stderr, "oom\n"); exit(1); }
  return d;
}

static void fifo_push(long lineno, const char *fn, const char *args,
                      const char *ret, const char *post) {
  if (g_fifo_len >= FIFO_CAP) fail("seam FIFO overflow");
  SeamRec *r = &g_fifo[(g_fifo_head + g_fifo_len) % FIFO_CAP];
  r->lineno = lineno;
  r->fn = xstrdup(fn);
  r->args = xstrdup(args);
  r->ret = xstrdup(ret);
  r->post = post ? xstrdup(post) : 0;
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
  free(r->fn); free(r->args); free(r->ret);
  if (r->post) free(r->post);
}

static void fifo_drain(void) {
  SeamRec *r;
  while ((r = fifo_pop()) != 0) seam_free(r);
}

// --- alias probes ---------------------------------------------------------------

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

// --- players ---------------------------------------------------------------------

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

// --- stage -----------------------------------------------------------------------

static void marshal_surface_list(const CanonVal *v, SurfaceList *out,
                                 const char *what) {
  if (v->type != CV_ARR || v->count > ML_MAX_SURFACES) fail(what);
  out->count = v->count;
  for (int i = 0; i < v->count; i++) {
    const CanonVal *s = v->items[i];
    // VS-stage surfaces are [Vec2D, Vec2D]; a 3rd (props) element is
    // outside the captured domain (no VS surface carries one).
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

static char conn_type(const char *s) {
  if (strlen(s) != 1) fail("connected: type string");
  const char c = s[0];
  if (c != 'g' && c != 'p' && c != 'l' && c != 'r') fail("connected: type char");
  return c;
}

static void marshal_conn_list(const CanonVal *v, MlConnPair *out, int *count,
                              const char *what) {
  if (v->type != CV_ARR || v->count > ML_MAX_SURFACES) fail(what);
  *count = v->count;
  for (int i = 0; i < v->count; i++) {
    const CanonVal *pair = v->items[i];
    if (pair->type != CV_ARR || pair->count != 2) fail(what);
    for (int side = 0; side < 2; side++) {
      const CanonVal *h = pair->items[side];
      MlConnHalf *half = side == 0 ? &out[i].l : &out[i].r;
      if (h->type == CV_NULL) {
        half->present = false;
        half->type = 0;
        half->index = 0;
      } else {
        if (h->type != CV_ARR || h->count != 2) fail(what);
        half->present = true;
        half->type = conn_type(cv_str(h->items[0]));
        half->index = cv_num(h->items[1]);
      }
    }
  }
}

static void marshal_stage(const CanonVal *v, MlStageX *out) {
  memset(out, 0, sizeof *out);
  if (v->type != CV_OBJ || v->nkeys != 8) fail("stage projection shape");
  marshal_surface_list(obj_req(v, "ground"), &out->s.ground, "stage.ground");
  marshal_surface_list(obj_req(v, "platform"), &out->s.platform, "stage.platform");
  marshal_surface_list(obj_req(v, "ceiling"), &out->s.ceiling, "stage.ceiling");
  marshal_surface_list(obj_req(v, "wallL"), &out->s.wallL, "stage.wallL");
  marshal_surface_list(obj_req(v, "wallR"), &out->s.wallR, "stage.wallR");
  const CanonVal *conn = obj_req(v, "connected");
  if (conn->type == CV_NULL) {
    out->hasConnected = false;
  } else {
    if (conn->type != CV_ARR || conn->count != 2) fail("stage.connected shape");
    out->hasConnected = true;
    marshal_conn_list(conn->items[0], out->connGround, &out->connGroundCount,
                      "stage.connected[0]");
    marshal_conn_list(conn->items[1], out->connPlatform,
                      &out->connPlatformCount, "stage.connected[1]");
  }
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
  const CanonVal *bz = obj_req(v, "blastzone");
  if (bz->type != CV_OBJ || bz->nkeys != 2 ||
      strcmp(bz->keys[0], "max") != 0 || strcmp(bz->keys[1], "min") != 0) {
    fail("stage.blastzone shape");
  }
  const Vec2D mx = cv_vec2(bz->vals[0]);
  const Vec2D mn = cv_vec2(bz->vals[1]);
  out->blastzone = box2d(mn.x, mn.y, mx.x, mx.y);
}

// --- seam callbacks from ml_physics -----------------------------------------------

static MlSim g_sim;

static SeamRec *seam_expect(const char *fn, const char *ctxmsg) {
  SeamRec *r = fifo_pop();
  if (!r) {
    fprintf(stderr, "SEAM DIVERGENCE line ~%ld: C reached %s (%s) but the "
                    "recording has no pending seam record\n",
            g_lineno, fn, ctxmsg);
    report_div("seam-underflow", g_lineno, "(record)", fn);
    longjmp(g_rec_jmp, 1);
  }
  if (strcmp(r->fn, fn) != 0) {
    fprintf(stderr, "SEAM DIVERGENCE line %ld: C reached %s (%s) but the "
                    "recording expects %s\n",
            r->lineno, fn, ctxmsg, r->fn);
    report_div("seam-order", r->lineno, r->fn, fn);
    seam_free(r);
    longjmp(g_rec_jmp, 1);
  }
  return r;
}

void mlp_dispatch(MlSim *sim, const MlDispCall *call) {
  SeamRec *r = seam_expect("dispatch", call->phase);
  // expected move name from the flags table (state key -> entry.name)
  const MlAsFlags *f = mlp_flags(sim, call->charId, call->state);
  // build the C-side args canon [phase, name, [slot, ...extras]]
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

  // RESYNC from the recorded post-dispatch state {alias, players}
  canon_arena_reset(); // NOTE: invalidates the caller record's parse trees?
  const char *err = 0;
  const CanonVal *post = canon_parse(r->post, &err);
  if (!post) { fprintf(stderr, "PARSE FAIL dispatch post line %ld: %s\n", r->lineno, err); exit(3); }
  if (post->type != CV_OBJ || post->nkeys != 2) fail("dispatch post shape");
  marshal_players(sim, obj_req(post, "players"));
  apply_alias4(sim, obj_req(post, "alias"));
  seam_free(r);
  if (!args_ok) longjmp(g_rec_jmp, 1);
}

static double getter_common(const char *name, const char *ctxmsg,
                            CanonBuf *args) {
  SeamRec *r = seam_expect(name, ctxmsg);
  const bool args_ok = strcmp(args->buf, r->args) == 0;
  if (!args_ok) report_div("getter-args", r->lineno, r->args, args->buf);
  double ret = 0;
  // ret is a plain number (bit-parsed)
  {
    canon_arena_reset();
    const char *err = 0;
    const CanonVal *rv = canon_parse(r->ret, &err);
    if (!rv) { fprintf(stderr, "PARSE FAIL getter ret line %ld: %s\n", r->lineno, err); exit(3); }
    ret = cv_num(rv);
  }
  seam_free(r);
  if (!args_ok) longjmp(g_rec_jmp, 1);
  return ret;
}

double mlp_hd_getLaunchAngle(MlSim *sim, double trajectory, double knockback,
                             bool hasReverse, bool reverse, double x, double y,
                             double v) {
  (void)sim;
  CanonBuf b;
  cb_init(&b);
  cb_putc(&b, '[');
  cb_num(&b, trajectory);
  cb_putc(&b, ',');
  cb_num(&b, knockback);
  cb_putc(&b, ',');
  cb_puts(&b, hasReverse ? (reverse ? "T" : "F") : "undef");
  cb_putc(&b, ',');
  cb_num(&b, x);
  cb_putc(&b, ',');
  cb_num(&b, y);
  cb_putc(&b, ',');
  cb_num(&b, v);
  cb_putc(&b, ']');
  const double ret = getter_common("getLaunchAngle", "launch", &b);
  cb_free(&b);
  return ret;
}

double mlp_hd_getHorizontalVelocity(MlSim *sim, double knockback,
                                    double angle) {
  (void)sim;
  CanonBuf b;
  cb_init(&b);
  cb_putc(&b, '[');
  cb_num(&b, knockback);
  cb_putc(&b, ',');
  cb_num(&b, angle);
  cb_putc(&b, ']');
  const double ret = getter_common("getHorizontalVelocity", "launch", &b);
  cb_free(&b);
  return ret;
}

double mlp_hd_getVerticalVelocity(MlSim *sim, double knockback, double angle,
                                  bool grounded, double trajectory) {
  (void)sim;
  CanonBuf b;
  cb_init(&b);
  cb_putc(&b, '[');
  cb_num(&b, knockback);
  cb_putc(&b, ',');
  cb_num(&b, angle);
  cb_putc(&b, ',');
  cb_puts(&b, grounded ? "T" : "F");
  cb_putc(&b, ',');
  cb_num(&b, trajectory);
  cb_putc(&b, ']');
  const double ret = getter_common("getVerticalVelocity", "launch", &b);
  cb_free(&b);
  return ret;
}

double mlp_hd_getHorizontalDecay(MlSim *sim, double angle) {
  (void)sim;
  CanonBuf b;
  cb_init(&b);
  cb_putc(&b, '[');
  cb_num(&b, angle);
  cb_putc(&b, ']');
  const double ret = getter_common("getHorizontalDecay", "launch", &b);
  cb_free(&b);
  return ret;
}

double mlp_hd_getVerticalDecay(MlSim *sim, double angle) {
  (void)sim;
  CanonBuf b;
  cb_init(&b);
  cb_putc(&b, '[');
  cb_num(&b, angle);
  cb_putc(&b, ']');
  const double ret = getter_common("getVerticalDecay", "launch", &b);
  cb_free(&b);
  return ret;
}

// --- pure interpolatedCollision replays --------------------------------------------

static void ser_maybe_vec(CanonBuf *b, MaybeVec2D m) {
  if (!m.present) {
    cb_puts(b, "null");
    return;
  }
  cb_puts(b, "{\"x\":");
  cb_num(b, m.v.x);
  cb_puts(b, ",\"y\":");
  cb_num(b, m.v.y);
  cb_putc(b, '}');
}

// --- physics record replay -----------------------------------------------------------

// ecbSquashData chain: physics.js module state, initialized to the shared
// nullSquashDatum VALUE ({location: null, factor: 1}) at module load.
static SquashDatum g_squash[4];

static void squash_chain_init(void) {
  for (int k = 0; k < 4; k++) g_squash[k] = squash_datum(1, true, 0);
}

static void ser_hq(CanonBuf *b, const MlSim *sim) {
  cb_putc(b, '[');
  for (int i = 0; i < sim->hqCount; i++) {
    if (i) cb_putc(b, ',');
    const MlHqRow *r = &sim->hq[i];
    // [i, {angular, corner, normal}, damageTypeIndex, false, false, true]
    cb_putc(b, '[');
    cb_num(b, r->i);
    cb_puts(b, ",{\"angular\":");
    cb_num(b, r->angular);
    cb_puts(b, ",\"corner\":");
    cb_puts(b, r->corner ? "T" : "F");
    cb_puts(b, ",\"normal\":{\"x\":");
    cb_num(b, r->normal.x);
    cb_puts(b, ",\"y\":");
    cb_num(b, r->normal.y);
    cb_puts(b, "}},");
    cb_num(b, r->damageTypeIndex);
    cb_puts(b, ",F,F,T]");
  }
  cb_putc(b, ']');
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
    fprintf(stderr, "usage: replay_physics <capture.jsonl> [--strict] "
                    "[--max-print N] [--stop-first]\n");
    return 1;
  }
  FILE *f = fopen(path, "r");
  if (!f) { fprintf(stderr, "cannot open %s\n", path); return 1; }
  g_file = path;

  squash_chain_init();

  char *line = NULL;
  size_t linecap = 0;
  long replayed = 0;
  long phys_records = 0, sweep_records = 0, seam_records = 0;

  CanonBuf out;
  cb_init(&out);

  ssize_t n;
  while ((n = getline(&line, &linecap, f)) > 0) {
    g_lineno++;
    if (line[n - 1] == '\n') line[n - 1] = 0;
    if (line[0] == 0) continue;

    // <frame> \t <fn> \t <args> \t <ret> [\t <post>]
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

    if (strcmp(fn, "asFlags") == 0) {
      canon_arena_reset();
      const char *err = 0;
      const CanonVal *dump = canon_parse(ret_s, &err);
      if (!dump) { fprintf(stderr, "PARSE FAIL asFlags: %s\n", err); return 3; }
      load_flags(dump);
      replayed++;
      continue;
    }

    if (strcmp(fn, "dispatch") == 0 || strcmp(fn, "getLaunchAngle") == 0 ||
        strcmp(fn, "getHorizontalVelocity") == 0 ||
        strcmp(fn, "getVerticalVelocity") == 0 ||
        strcmp(fn, "getHorizontalDecay") == 0 ||
        strcmp(fn, "getVerticalDecay") == 0) {
      if (strcmp(fn, "dispatch") == 0 && !post_s) fail("dispatch: missing post");
      if (strcmp(fn, "dispatch") != 0 && post_s) fail("getter: unexpected post");
      fifo_push(g_lineno, fn, args_s, ret_s, post_s);
      seam_records++;
      replayed++;
      continue;
    }

    if (strcmp(fn, "sweepCircleVsSweepCircle") == 0 ||
        strcmp(fn, "sweepCircleVsAABB") == 0) {
      if (post_s) fail("pure record with post field");
      canon_arena_reset();
      const char *err = 0;
      const CanonVal *args = canon_parse(args_s, &err);
      if (!args) { fprintf(stderr, "PARSE FAIL %s:%ld: %s\n", path, g_lineno, err); return 3; }
      out.len = 0; out.buf[0] = 0;
      if (strcmp(fn, "sweepCircleVsSweepCircle") == 0) {
        if (args->type != CV_ARR || args->count != 8) fail("sweepCC argc");
        ser_maybe_vec(&out, sweepCircleVsSweepCircle(
            cv_vec2(args->items[0]), cv_num(args->items[1]),
            cv_vec2(args->items[2]), cv_num(args->items[3]),
            cv_vec2(args->items[4]), cv_num(args->items[5]),
            cv_vec2(args->items[6]), cv_num(args->items[7])));
      } else {
        if (args->type != CV_ARR || args->count != 6) fail("sweepAABB argc");
        ser_maybe_vec(&out, sweepCircleVsAABB(
            cv_vec2(args->items[0]), cv_num(args->items[1]),
            cv_vec2(args->items[2]), cv_num(args->items[3]),
            cv_vec2(args->items[4]), cv_vec2(args->items[5])));
      }
      if (strcmp(out.buf, ret_s) != 0) {
        report_div(fn, g_lineno, ret_s, out.buf);
        if (stop_first) break;
      }
      sweep_records++;
      replayed++;
      continue;
    }

    if (strcmp(fn, "physics") != 0) fail("unknown record function");
    if (!post_s) fail("physics: missing post-state field");
    if (strcmp(ret_s, "undef") != 0) fail("physics is void: expected ret undef");

    // marshal the pre state
    canon_arena_reset();
    const char *err = 0;
    const CanonVal *args = canon_parse(args_s, &err);
    if (!args) { fprintf(stderr, "PARSE FAIL %s:%ld args: %s\n", path, g_lineno, err); return 3; }
    if (args->type != CV_ARR || args->count != 3) fail("physics args shape");
    const double slot_i = cv_num(args->items[0]);
    MlInput in[4];
    cv_input4(args->items[1], in);
    const CanonVal *pre = args->items[2];
    if (pre->type != CV_OBJ || pre->nkeys != 8) fail("physics pre shape");

    memset(&g_sim, 0, sizeof g_sim);
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
    g_sim.versusMode = cv_truthy_num(obj_req(pre, "versusMode"));
    const CanonVal *gs = obj_req(pre, "gameSettings");
    if (gs->type != CV_OBJ || gs->nkeys != 3) fail("gameSettings shape");
    g_sim.lCancelType = cv_num(obj_req(gs, "lCancelType"));
    g_sim.phantomThreshold = cv_num(obj_req(gs, "phantomThreshold"));
    g_sim.turbo = cv_truthy_num(obj_req(gs, "turbo")) != 0;
    for (int k = 0; k < 4; k++) g_sim.tapJumpOff[k] = 0; // turbo-only read
    marshal_players(&g_sim, obj_req(pre, "players"));
    apply_alias4(&g_sim, obj_req(pre, "alias"));
    marshal_stage(obj_req(pre, "stage"), &g_sim.stage);
    for (int k = 0; k < 4; k++) g_sim.ecbSquashData[k] = g_squash[k];
    g_sim.hqCount = 0;
    ml_ev_reset();

    if (!g_sim.playerPresent[(int)slot_i]) fail("physics: absent slot");

    if (setjmp(g_rec_jmp) == 0) {
      // NOTE: seam callbacks reset the canon arena (parse trees above are
      // consumed before the call; nothing is read from them afterwards).
      ml_physics(&g_sim, slot_i, in);

      // every recorded seam for this call must have been consumed
      if (g_fifo_len != 0) {
        SeamRec *r = fifo_pop();
        fprintf(stderr, "SEAM DIVERGENCE line %ld: recorded %s never "
                        "reached by C physics\n", r->lineno, r->fn);
        report_div("seam-unconsumed", r->lineno, r->fn, "(not reached)");
        seam_free(r);
        fifo_drain();
      } else {
        // post envelope compare (M4 task 1 widened):
        // {"alias":...,"hq":...,"players":...,"snd":...,"vfx":[...]}
        out.len = 0; out.buf[0] = 0;
        cb_puts(&out, "{\"alias\":");
        ser_alias3(&out, &g_sim);
        cb_puts(&out, ",\"hq\":");
        ser_hq(&out, &g_sim);
        cb_puts(&out, ",\"players\":");
        ser_players(&out, &g_sim);
        cb_puts(&out, ",\"snd\":[");
        for (int s = 0; s < ml_events.snd_count; s++) {
          if (s) cb_putc(&out, ',');
          cb_qstr(&out, ml_events.snd[s]);
        }
        cb_puts(&out, "],\"vfx\":[");
        for (int s = 0; s < ml_events.vfx_count; s++) {
          if (s) cb_putc(&out, ',');
          cb_vfx(&out, &ml_events.vfx[s]);
        }
        cb_puts(&out, "]}");
        if (strcmp(out.buf, post_s) != 0) {
          report_div("physics-post", g_lineno, post_s, out.buf);
        }
      }
    } else {
      // seam mismatch aborted the record; drop its remaining seam records
      fifo_drain();
    }

    // chain the module-state squash data forward regardless (documented:
    // a diverged record can poison the chain — use --stop-first to debug)
    for (int k = 0; k < 4; k++) g_squash[k] = g_sim.ecbSquashData[k];

    phys_records++;
    replayed++;
    if (g_divergences > 0 && stop_first) break;
  }
  free(line);
  fclose(f);

  if (!g_ft_loaded) fail("capture carries no asFlags record");
  if (g_fifo_len != 0) {
    fprintf(stderr, "SEAM DIVERGENCE: %d seam records left unconsumed at EOF\n",
            g_fifo_len);
    g_divergences += g_fifo_len;
    fifo_drain();
  }

  fprintf(stderr,
          "replayed: %ld physics, %ld pure interpolatedCollision, %ld seam "
          "records consumed in call order\n",
          phys_records, sweep_records, seam_records);
  printf("PHYSICS REPLAY RAN %ld records, %ld divergences", replayed,
         g_divergences);
  if (g_first_div_line != -1) printf(" (first at line %ld)", g_first_div_line);
  printf("\n");
  cb_free(&out);
  if (strict && g_divergences > 0) return 2;
  return 0;
}
