// replay_asshort.c — M2 task 4 replay driver: feeds recorded
// actionStateShortcuts boundary calls (build/<id>.asshort.jsonl, FORMAT.md
// "asshort") to the C translations and compares canon-v1.1 serializations
// byte-for-byte — return value AND (for mutation-captured functions) the
// post-state field {dsp,mut,rng,snd}. A single differing bit anywhere is a
// divergence.
//
// The seeded-RNG stream is replayed as a CHAIN: `rngBoot` (first record)
// seeds the C mulberry32 with the golden's seed and fast-forwards the
// recorded boot-draw count; every standalone `Math.random` record burns
// one draw and must match bit-exactly (draw-for-draw, INCLUDING the one
// off-step pre-frame-1 startGame draw — asserted to be exactly one
// frame-0 standalone draw); frame-0 randomShout records (the KO-shout
// sweep) draw from a SEPARATE sweep generator seeded 0x0badf00d,
// mirroring the capture's Math.random swap (FORMAT.md "asshort sweep").
//
// Usage: replay_asshort <capture.jsonl> [--strict] [--max-print N]
//                       [--stop-first]
//
// Marshalling is STRICT (prevention rule 7): any argument shape outside
// the captured domain aborts with exit 3 — never guess.
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../action_state_shortcuts.h"
#include "../ml_events.h"
#include "../ml_rng.h"
#include "canon.h"
#include "input_canon.h"

static const char *g_file = "?";
static long g_lineno = 0;

static void fail(const char *msg) {
  fprintf(stderr, "MARSHAL FAIL %s:%ld: %s\n", g_file, g_lineno, msg);
  exit(3);
}

void ml_canon_fail(const char *msg) { fail(msg); }
void ml_events_fail(const char *what) { fail(what); }
void ml_asshort_out_of_domain(const char *what) {
  fprintf(stderr, "OUT OF DOMAIN %s:%ld: %s\n", g_file, g_lineno, what);
  exit(3);
}

// --- RNG chain ---------------------------------------------------------------

static MlRng g_rng;         // the seeded match stream (rngBoot seeds it)
static MlRng g_sweep_rng;   // the sweep generator (spec-asshort.js literal)
static bool g_boot_seen = false;
static long g_frame0_draws = 0; // standalone frame-0 draws (must end == 1)

// --- marshalling helpers -------------------------------------------------------

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

// truthiness-only parameters: measured domain is bool | null | undefined
static bool cv_truthy(const CanonVal *v) {
  if (v->type == CV_BOOL) return v->b;
  if (v->type == CV_NULL || v->type == CV_UNDEF) return false;
  fail("expected bool/null/undef (truthiness domain)");
  return false;
}

static void expect_argc(const CanonVal *args, int n) {
  if (args->type != CV_ARR || args->count != n) fail("bad argument count");
}

static void cv_input4(const CanonVal *v, MlInput in[4]) {
  if (v->type != CV_ARR || v->count != 4) fail("expected 4-deep input slice");
  for (int i = 0; i < 4; i++) in[i] = ml_input_from_canon(v->items[i]);
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

static void obj_expect_keys(const CanonVal *o, int n) {
  if (o->type != CV_OBJ || o->nkeys != n) fail("unexpected object key count");
}

// hitboxes slice {active:[4 bools], hitList:[nums]}
static void cv_hitboxes(const CanonVal *v, MlHitboxes *hb) {
  obj_expect_keys(v, 2);
  const CanonVal *act = obj_req(v, "active");
  const CanonVal *hl = obj_req(v, "hitList");
  if (act->type != CV_ARR || act->count != 4) fail("active must be [4]");
  memset(hb, 0, sizeof *hb);
  for (int i = 0; i < 4; i++) hb->active[i] = cv_bool(act->items[i]);
  if (hl->type != CV_ARR || hl->count > ML_HITLIST_CAP) fail("hitList shape");
  hb->hitListLen = hl->count;
  for (int i = 0; i < hl->count; i++) hb->hitList[i] = cv_num(hl->items[i]);
}

static int cv_char(const CanonVal *v) {
  const double d = cv_num(v);
  const int c = (int)d;
  if (d != (double)c || c < 0 || c > 4) fail("charId out of range");
  return c;
}

// --- serialization helpers -------------------------------------------------------

static void ser_bool(CanonBuf *b, bool v) { cb_puts(b, v ? "T" : "F"); }

static void ser_pair(CanonBuf *b, AsPair p) {
  cb_putc(b, '[');
  ser_bool(b, p.flag);
  cb_putc(b, ',');
  if (p.kind == AS_P_FALSE) {
    cb_puts(b, "F");
  } else if (p.kind == AS_P_STR) {
    cb_qstr(b, p.str);
  } else {
    cb_num(b, p.num);
  }
  cb_putc(b, ']');
}

static void ser_tri(CanonBuf *b, AsTri t) {
  if (t == AS_UNDEF) cb_puts(b, "undef");
  else ser_bool(b, t == AS_TRUE);
}

static void ser_vec2(CanonBuf *b, Vec2D v) {
  cb_puts(b, "{\"x\":");
  cb_num(b, v.x);
  cb_puts(b, ",\"y\":");
  cb_num(b, v.y);
  cb_putc(b, '}');
}

static void ser_hitboxes(CanonBuf *b, const MlHitboxes *hb) {
  cb_puts(b, "{\"active\":[");
  for (int i = 0; i < 4; i++) {
    if (i) cb_putc(b, ',');
    ser_bool(b, hb->active[i]);
  }
  cb_puts(b, "],\"hitList\":[");
  for (int i = 0; i < hb->hitListLen; i++) {
    if (i) cb_putc(b, ',');
    cb_num(b, hb->hitList[i]);
  }
  cb_puts(b, "]}");
}

// post-state envelope: {"dsp":[...],"mut":<mut>,"rng":[...],"snd":[...]}
// (keys sorted d < m < r < s), event lists from the ml_events queues.
static void ser_post(CanonBuf *b, const char *mut) {
  cb_puts(b, "{\"dsp\":[");
  for (int i = 0; i < ml_events.dsp_count; i++) {
    if (i) cb_putc(b, ',');
    cb_qstr(b, ml_events.dsp[i]);
  }
  cb_puts(b, "],\"mut\":");
  cb_puts(b, mut);
  cb_puts(b, ",\"rng\":[");
  for (int i = 0; i < ml_events.rng_count; i++) {
    if (i) cb_putc(b, ',');
    cb_num(b, ml_events.rng[i]);
  }
  cb_puts(b, "],\"snd\":[");
  for (int i = 0; i < ml_events.snd_count; i++) {
    if (i) cb_putc(b, ',');
    cb_qstr(b, ml_events.snd[i]);
  }
  cb_puts(b, "]}");
}

// --- the mutation-captured function set (must match expected-capture pins) ---

static const char *POST_FNS[] = {
  "randomShout", "executeIntangibility", "playSounds", "turnOffHitboxes",
  "shieldTilt", "reduceByTraction", "airDrift", "fastfall", "shieldDepletion",
  "shieldSize", "checkForSmashes", "checkForSpecials", "checkForIASA",
  "turboAirborneInterrupt", "turboGroundedInterrupt",
};

static bool is_post_fn(const char *fn) {
  for (size_t i = 0; i < sizeof POST_FNS / sizeof *POST_FNS; i++) {
    if (strcmp(POST_FNS[i], fn) == 0) return true;
  }
  return false;
}

// --- dispatch ---------------------------------------------------------------------

// ret goes into `out`; for post fns the mut object goes into `mut`.
static void dispatch(const char *fn, long frame, const CanonVal *args,
                     CanonBuf *out, CanonBuf *mut) {
  MlInput in[4];

  if (strcmp(fn, "rngBoot") == 0) {
    expect_argc(args, 2);
    if (g_boot_seen) fail("rngBoot: seen twice");
    g_boot_seen = true;
    const double seed = cv_num(args->items[0]);
    const double boot = cv_num(args->items[1]);
    if (seed < 0 || seed != (double)(uint32_t)seed) fail("rngBoot: bad seed");
    if (boot < 0 || boot != (double)(long)boot) fail("rngBoot: bad count");
    ml_rng_seed(&g_rng, (uint32_t)seed);
    for (long i = 0; i < (long)boot; i++) (void)ml_rng_next(&g_rng);
    // ret pins the fast-forwarded state (additive-advance formula check)
    cb_num(out, (double)g_rng.a);
  } else if (strcmp(fn, "Math.random") == 0) {
    expect_argc(args, 0);
    if (!g_boot_seen) fail("Math.random before rngBoot");
    if (frame == 0) g_frame0_draws++;
    cb_num(out, ml_rng_next(&g_rng));
  } else if (strcmp(fn, "randomShout") == 0) {
    expect_argc(args, 1);
    // frame-0 records are the KO-shout sweep: the capture swapped
    // Math.random for the local sweep generator (spec-asshort.js)
    ml_active_rng = frame == 0 ? &g_sweep_rng : &g_rng;
    as_randomShout(cv_num(args->items[0]));
    cb_puts(out, "undef");
    cb_puts(mut, "{}");
  } else if (strcmp(fn, "executeIntangibility") == 0) {
    expect_argc(args, 4);
    const CanonVal *pre = args->items[3];
    obj_expect_keys(pre, 2);
    double hurtBoxState = cv_num(obj_req(pre, "hurtBoxState"));
    double intangibleTimer = cv_num(obj_req(pre, "intangibleTimer"));
    as_executeIntangibility(cv_str(args->items[0]), cv_char(args->items[1]),
                            cv_num(args->items[2]), &intangibleTimer,
                            &hurtBoxState);
    cb_puts(out, "undef");
    cb_puts(mut, "{\"hurtBoxState\":");
    cb_num(mut, hurtBoxState);
    cb_puts(mut, ",\"intangibleTimer\":");
    cb_num(mut, intangibleTimer);
    cb_putc(mut, '}');
  } else if (strcmp(fn, "playSounds") == 0) {
    expect_argc(args, 4);
    const CanonVal *sched = args->items[3];
    if (sched->type != CV_ARR || sched->count > 16) fail("schedule shape");
    AsSoundRow rows[16];
    for (int i = 0; i < sched->count; i++) {
      const CanonVal *row = sched->items[i];
      if (row->type != CV_ARR || row->count != 2) fail("schedule row shape");
      rows[i].frame = cv_num(row->items[0]);
      rows[i].name = cv_str(row->items[1]);
    }
    as_playSounds(rows, sched->count, cv_num(args->items[2]));
    cb_puts(out, "undef");
    cb_puts(mut, "{}");
  } else if (strcmp(fn, "isFinalDeath") == 0) {
    expect_argc(args, 1);
    const CanonVal *st = args->items[0];
    obj_expect_keys(st, 4);
    AsFinalDeathState s;
    s.gameMode = cv_num(obj_req(st, "gameMode"));
    s.versusMode = cv_num(obj_req(st, "versusMode"));
    const CanonVal *pt = obj_req(st, "playerType");
    const CanonVal *sk = obj_req(st, "stocks");
    if (pt->type != CV_ARR || pt->count != 4) fail("playerType shape");
    if (sk->type != CV_ARR || sk->count != 4) fail("stocks shape");
    for (int j = 0; j < 4; j++) {
      s.playerType[j] = cv_num(pt->items[j]);
      if (sk->items[j]->type == CV_NULL) {
        s.stocksPresent[j] = false;
        s.stocks[j] = 0;
      } else {
        s.stocksPresent[j] = true;
        s.stocks[j] = cv_num(sk->items[j]);
      }
    }
    ser_bool(out, as_isFinalDeath(&s));
  } else if (strcmp(fn, "getAngle") == 0) {
    expect_argc(args, 2);
    cb_num(out, as_getAngle(cv_num(args->items[0]), cv_num(args->items[1])));
  } else if (strcmp(fn, "turnOffHitboxes") == 0) {
    expect_argc(args, 1);
    (void)cv_num(args->items[0]); // slot index (state-free constant writes)
    MlHitboxes hb;
    memset(&hb, 0, sizeof hb);
    // pre-state is irrelevant: the function overwrites both fields; give
    // it a dirty struct so the writes are observable
    hb.active[0] = hb.active[1] = hb.active[2] = hb.active[3] = true;
    hb.hitListLen = 1;
    hb.hitList[0] = 1;
    as_turnOffHitboxes(&hb);
    cb_puts(out, "undef");
    ser_hitboxes(mut, &hb);
  } else if (strcmp(fn, "shieldTilt") == 0) {
    expect_argc(args, 4);
    const CanonVal *pre = args->items[2];
    obj_expect_keys(pre, 4);
    AsShieldTiltState st;
    st.face = cv_num(obj_req(pre, "face"));
    st.inCSS = cv_truthy(obj_req(pre, "inCSS"));
    st.pos = cv_vec2(obj_req(pre, "pos"));
    st.shieldPosition = cv_vec2(obj_req(pre, "shieldPosition"));
    st.shieldPositionReal.x = st.shieldPositionReal.y = 0;
    cv_input4(args->items[3], in);
    as_shieldTilt(cv_truthy(args->items[0]), cv_char(args->items[1]), &st, in);
    cb_puts(out, "undef");
    cb_puts(mut, "{\"shieldPosition\":");
    ser_vec2(mut, st.shieldPosition);
    cb_puts(mut, ",\"shieldPositionReal\":");
    ser_vec2(mut, st.shieldPositionReal);
    cb_putc(mut, '}');
  } else if (strcmp(fn, "reduceByTraction") == 0) {
    expect_argc(args, 3);
    double cVelX = cv_num(args->items[2]);
    as_reduceByTraction(cv_truthy(args->items[0]), cv_char(args->items[1]),
                        &cVelX);
    cb_puts(out, "undef");
    cb_puts(mut, "{\"cVelX\":");
    cb_num(mut, cVelX);
    cb_putc(mut, '}');
  } else if (strcmp(fn, "airDrift") == 0) {
    expect_argc(args, 3);
    double cVelX = cv_num(args->items[1]);
    cv_input4(args->items[2], in);
    as_airDrift(cv_char(args->items[0]), &cVelX, in);
    cb_puts(out, "undef");
    cb_puts(mut, "{\"cVelX\":");
    cb_num(mut, cVelX);
    cb_putc(mut, '}');
  } else if (strcmp(fn, "fastfall") == 0) {
    expect_argc(args, 3);
    const CanonVal *pre = args->items[1];
    obj_expect_keys(pre, 2);
    double cVelY = cv_num(obj_req(pre, "cVelY"));
    bool fastfalled = cv_bool(obj_req(pre, "fastfalled"));
    cv_input4(args->items[2], in);
    as_fastfall(cv_char(args->items[0]), &cVelY, &fastfalled, in);
    cb_puts(out, "undef");
    cb_puts(mut, "{\"cVelY\":");
    cb_num(mut, cVelY);
    cb_puts(mut, ",\"fastfalled\":");
    ser_bool(mut, fastfalled);
    cb_putc(mut, '}');
  } else if (strcmp(fn, "shieldDepletion") == 0) {
    expect_argc(args, 3);
    const CanonVal *pre = args->items[1];
    obj_expect_keys(pre, 5);
    AsShieldDepState st;
    st.grounded = cv_bool(obj_req(pre, "grounded"));
    st.kDec = cv_vec2(obj_req(pre, "kDec"));
    st.kVel = cv_vec2(obj_req(pre, "kVel"));
    st.shieldHP = cv_num(obj_req(pre, "shieldHP"));
    st.shielding = cv_bool(obj_req(pre, "shielding"));
    cv_input4(args->items[2], in);
    as_shieldDepletion(cv_char(args->items[0]), &st, in);
    cb_puts(out, "undef");
    cb_puts(mut, "{\"grounded\":");
    ser_bool(mut, st.grounded);
    cb_puts(mut, ",\"kDec\":");
    ser_vec2(mut, st.kDec);
    cb_puts(mut, ",\"kVel\":");
    ser_vec2(mut, st.kVel);
    cb_puts(mut, ",\"shieldHP\":");
    cb_num(mut, st.shieldHP);
    cb_puts(mut, ",\"shielding\":");
    ser_bool(mut, st.shielding);
    cb_putc(mut, '}');
  } else if (strcmp(fn, "shieldSize") == 0) {
    expect_argc(args, 4);
    const CanonVal *pre = args->items[2];
    obj_expect_keys(pre, 1);
    AsShieldSizeOut o;
    cv_input4(args->items[3], in);
    as_shieldSize(cv_truthy(args->items[0]), cv_char(args->items[1]),
                  cv_num(obj_req(pre, "shieldHP")), &o, in);
    cb_puts(out, "undef");
    cb_puts(mut, "{\"shieldAnalog\":");
    cb_num(mut, o.shieldAnalog);
    cb_puts(mut, ",\"shieldSize\":");
    cb_num(mut, o.shieldSize);
    cb_putc(mut, '}');
  } else if (strcmp(fn, "mashOut") == 0) {
    expect_argc(args, 1);
    cv_input4(args->items[0], in);
    ser_bool(out, as_mashOut(in));
  } else if (strcmp(fn, "checkForSmashes") == 0) {
    expect_argc(args, 2);
    double face = cv_num(args->items[0]);
    cv_input4(args->items[1], in);
    ser_pair(out, as_checkForSmashes(&face, in));
    cb_puts(mut, "{\"face\":");
    cb_num(mut, face);
    cb_putc(mut, '}');
  } else if (strcmp(fn, "checkForTilts") == 0) {
    if (args->type != CV_ARR || (args->count != 2 && args->count != 3)) {
      fail("checkForTilts: expected 2 or 3 args");
    }
    cv_input4(args->items[1], in);
    const bool has_rev = args->count == 3;
    ser_pair(out, as_checkForTilts(cv_num(args->items[0]), in, has_rev,
                                   has_rev ? cv_num(args->items[2]) : 0));
  } else if (strcmp(fn, "checkForIASA") == 0) {
    expect_argc(args, 5);
    const CanonVal *pre = args->items[0];
    AsIasaState st;
    const CanonVal *iasa = obj_get(pre, "IASATimer");
    st.hasIASATimer = iasa != 0;
    st.IASATimer = iasa ? cv_num(iasa) : 0;
    obj_expect_keys(pre, st.hasIASATimer ? 5 : 4);
    st.doubleJumped = cv_bool(obj_req(pre, "doubleJumped"));
    st.face = cv_num(obj_req(pre, "face"));
    st.jumpsUsed = cv_num(obj_req(pre, "jumpsUsed"));
    st.timer = cv_num(obj_req(pre, "timer"));
    cv_input4(args->items[3], in);
    ser_tri(out, as_checkForIASA(&st, cv_num(args->items[1]),
                                 cv_num(args->items[2]), in,
                                 cv_bool(args->items[4])));
    cb_puts(mut, "{}");
  } else if (strcmp(fn, "checkForSpecials") == 0) {
    expect_argc(args, 2);
    const CanonVal *pre = args->items[0];
    obj_expect_keys(pre, 4);
    double face = cv_num(obj_req(pre, "face"));
    cv_input4(args->items[1], in);
    ser_pair(out,
             as_checkForSpecials(&face,
                                 cv_num(obj_req(pre, "bTurnaroundTimer")),
                                 cv_num(obj_req(pre, "bTurnaroundDirection")),
                                 cv_bool(obj_req(pre, "grounded")), in));
    cb_puts(mut, "{\"face\":");
    cb_num(mut, face);
    cb_putc(mut, '}');
  } else if (strcmp(fn, "checkForAerials") == 0) {
    expect_argc(args, 2);
    cv_input4(args->items[1], in);
    ser_pair(out, as_checkForAerials(cv_num(args->items[0]), in));
  } else if (strcmp(fn, "checkForDash") == 0) {
    expect_argc(args, 2);
    cv_input4(args->items[1], in);
    ser_bool(out, as_checkForDash(cv_num(args->items[0]), in));
  } else if (strcmp(fn, "checkForSmashTurn") == 0) {
    expect_argc(args, 2);
    cv_input4(args->items[1], in);
    ser_bool(out, as_checkForSmashTurn(cv_num(args->items[0]), in));
  } else if (strcmp(fn, "tiltTurnDashBuffer") == 0) {
    expect_argc(args, 2);
    cv_input4(args->items[1], in);
    ser_bool(out, as_tiltTurnDashBuffer(cv_num(args->items[0]), in));
  } else if (strcmp(fn, "checkForTiltTurn") == 0) {
    expect_argc(args, 2);
    cv_input4(args->items[1], in);
    ser_bool(out, as_checkForTiltTurn(cv_num(args->items[0]), in));
  } else if (strcmp(fn, "checkForJump") == 0) {
    expect_argc(args, 2);
    cv_input4(args->items[1], in);
    ser_pair(out, as_checkForJump(cv_num(args->items[0]), in));
  } else if (strcmp(fn, "checkForDoubleJump") == 0) {
    expect_argc(args, 2);
    cv_input4(args->items[1], in);
    ser_bool(out, as_checkForDoubleJump(cv_num(args->items[0]), in));
  } else if (strcmp(fn, "checkForMultiJump") == 0) {
    expect_argc(args, 2);
    cv_input4(args->items[1], in);
    ser_bool(out, as_checkForMultiJump(cv_num(args->items[0]), in));
  } else if (strcmp(fn, "checkForSquat") == 0) {
    expect_argc(args, 1);
    cv_input4(args->items[0], in);
    ser_bool(out, as_checkForSquat(in));
  } else if (strcmp(fn, "turboAirborneInterrupt") == 0) {
    expect_argc(args, 5);
    const CanonVal *pre = args->items[2];
    obj_expect_keys(pre, 8);
    double face = cv_num(obj_req(pre, "face"));
    MlHitboxes hb;
    cv_hitboxes(obj_req(pre, "hitboxes"), &hb);
    AsTurboAirState st;
    st.actionState = cv_str(obj_req(pre, "actionState"));
    st.face = &face;
    st.doubleJumped = cv_bool(obj_req(pre, "doubleJumped"));
    st.jumpsUsed = cv_num(obj_req(pre, "jumpsUsed"));
    st.bTurnaroundDirection = cv_num(obj_req(pre, "bTurnaroundDirection"));
    st.bTurnaroundTimer = cv_num(obj_req(pre, "bTurnaroundTimer"));
    st.grounded = cv_bool(obj_req(pre, "grounded"));
    st.hitboxes = &hb;
    cv_input4(args->items[4], in);
    ser_bool(out, as_turboAirborneInterrupt(&st, cv_num(args->items[1]),
                                            cv_num(args->items[3]), in));
    cb_puts(mut, "{\"face\":");
    cb_num(mut, face);
    cb_puts(mut, ",\"hitboxes\":");
    ser_hitboxes(mut, &hb);
    cb_putc(mut, '}');
  } else if (strcmp(fn, "turboGroundedInterrupt") == 0) {
    expect_argc(args, 5);
    const CanonVal *pre = args->items[2];
    const CanonVal *dbuf = obj_get(pre, "dashbuffer");
    obj_expect_keys(pre, dbuf ? 7 : 6);
    double face = cv_num(obj_req(pre, "face"));
    MlHitboxes hb;
    cv_hitboxes(obj_req(pre, "hitboxes"), &hb);
    bool dashbuffer = dbuf ? cv_bool(dbuf) : false;
    bool hasDashbuffer = dbuf != 0;
    AsTurboGroundState st;
    st.actionState = cv_str(obj_req(pre, "actionState"));
    st.bTurnaroundDirection = cv_num(obj_req(pre, "bTurnaroundDirection"));
    st.bTurnaroundTimer = cv_num(obj_req(pre, "bTurnaroundTimer"));
    st.face = &face;
    st.grounded = cv_bool(obj_req(pre, "grounded"));
    st.hitboxes = &hb;
    st.dashbuffer = &dashbuffer;
    st.hasDashbuffer = &hasDashbuffer;
    cv_input4(args->items[4], in);
    ser_bool(out, as_turboGroundedInterrupt(&st, cv_num(args->items[1]),
                                            cv_num(args->items[3]), in));
    cb_putc(mut, '{');
    if (hasDashbuffer) {
      cb_puts(mut, "\"dashbuffer\":");
      ser_bool(mut, dashbuffer);
      cb_putc(mut, ',');
    }
    cb_puts(mut, "\"face\":");
    cb_num(mut, face);
    cb_puts(mut, ",\"hitboxes\":");
    ser_hitboxes(mut, &hb);
    cb_putc(mut, '}');
  } else if (strcmp(fn, "setupActionStates") == 0) {
    // fires only at module load, BEFORE the capture installs — a live
    // record is outside the captured domain by construction
    fail("setupActionStates: live record out of captured domain");
  } else {
    fail("unknown boundary function");
  }
  (void)frame;
}

// --- registry self-check (the scaffolding's own mechanics) --------------------

static void registry_self_check(void) {
  static int d1, d2;
  const MlMoveDef *m1 = (const MlMoveDef *)(void *)&d1;
  const MlMoveDef *m2 = (const MlMoveDef *)(void *)&d2;
  AsMoveEntry list[2] = {{"FALL", m1}, {"WAIT", m2}};
  as_setupActionStates(2, list, 2);
  if (as_lookup(2, "FALL") != m1) fail("registry: lookup FALL");
  if (as_lookup(2, "DASH") != 0) fail("registry: phantom entry");
  if (as_lookup(3, "FALL") != 0) fail("registry: char isolation");
  ml_ev_reset();
  if (as_dispatch(2, "WAIT", "init") != m2) fail("registry: dispatch lookup");
  if (ml_events.dsp_count != 1 ||
      strcmp(ml_events.dsp[0], "init:WAIT") != 0) {
    fail("registry: dispatch note");
  }
  as_setupActionStates(2, list, 0); // clear
  ml_ev_reset();
}

// --- main ------------------------------------------------------------------------

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
    fprintf(stderr, "usage: replay_asshort <capture.jsonl> [--strict] "
                    "[--max-print N] [--stop-first]\n");
    return 1;
  }
  FILE *f = fopen(path, "r");
  if (!f) { fprintf(stderr, "cannot open %s\n", path); return 1; }
  g_file = path;

  registry_self_check();
  ml_rng_seed(&g_sweep_rng, 0x0badf00du);
  ml_active_rng = &g_rng;

  char *line = NULL;
  size_t linecap = 0;
  long replayed = 0, divergences = 0, printed = 0;
  long first_div_line = -1;

  CanonBuf out, mut, post;
  cb_init(&out);
  cb_init(&mut);
  cb_init(&post);

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
    const char *frame_s = line;
    const char *fn = tab1 + 1;
    const char *args_s = tab2 + 1;
    char *ret_s = tab3 + 1;
    char *tab4 = strchr(ret_s, '\t');
    const char *post_s = NULL;
    if (tab4) { *tab4 = 0; post_s = tab4 + 1; }
    const long frame = strtol(frame_s, NULL, 10);

    const bool want_post = is_post_fn(fn);
    if (want_post && !post_s) fail("missing post-state field");
    if (!want_post && post_s) fail("unexpected post-state field");

    canon_arena_reset();
    const char *err = NULL;
    const CanonVal *args = canon_parse(args_s, &err);
    if (!args) {
      fprintf(stderr, "PARSE FAIL %s:%ld: %s\n", path, g_lineno, err);
      return 3;
    }

    out.len = 0; out.buf[0] = 0;
    mut.len = 0; mut.buf[0] = 0;
    post.len = 0; post.buf[0] = 0;

    ml_ev_reset();
    ml_active_rng = &g_rng;
    dispatch(fn, frame, args, &out, &mut);

    bool diverged = strcmp(out.buf, ret_s) != 0;
    const char *got2 = NULL, *exp2 = NULL;
    if (want_post) {
      ser_post(&post, mut.buf);
      if (strcmp(post.buf, post_s) != 0) { diverged = true; got2 = post.buf; exp2 = post_s; }
    } else {
      // pure boundaries must not touch the seams
      if (ml_events.snd_count || ml_events.dsp_count || ml_events.rng_count) {
        fail("events escaped a pure boundary function");
      }
    }

    replayed++;
    if (diverged) {
      divergences++;
      if (first_div_line == -1) first_div_line = g_lineno;
      if (printed < max_print) {
        printed++;
        fprintf(stderr,
                "DIVERGENCE line %ld frame %s fn %s\n"
                "  ret expected: %.300s\n"
                "  ret got:      %.300s\n",
                g_lineno, frame_s, fn, ret_s, out.buf);
        if (got2) {
          fprintf(stderr,
                  "  post expected: %.400s\n  post got:      %.400s\n",
                  exp2, got2);
        }
      }
      if (stop_first) break;
    }
  }
  free(line);
  fclose(f);

  if (!g_boot_seen) fail("capture carries no rngBoot record");
  if (g_frame0_draws != 1) {
    // oracle/CHECKSUM.md section 6: exactly ONE off-step draw (startGame)
    fprintf(stderr, "RNG PIN FAIL: %ld standalone frame-0 draws (want 1)\n",
            g_frame0_draws);
    return 2;
  }

  printf("ASSHORT REPLAY RAN %ld records, %ld divergences", replayed,
         divergences);
  if (first_div_line != -1) printf(" (first at line %ld)", first_div_line);
  printf("\n");
  cb_free(&out);
  cb_free(&mut);
  cb_free(&post);
  if (strict && divergences > 0) return 2;
  return 0;
}
