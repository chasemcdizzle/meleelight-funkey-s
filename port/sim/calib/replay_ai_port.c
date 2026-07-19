// replay_ai_port.c — M4 task 4 replay driver: feeds recorded aiport
// runAI records (build/<id>.aiport.jsonl, FORMAT.md "The aiport spec") to
// the C ai.c translation and compares the canon-v1.1 post envelope
// {bank, bk, rng} byte-for-byte. A single differing bit anywhere is a
// divergence.
//
// The seeded-RNG stream is a CHAIN (the asshort pattern): `rngBoot`
// seeds the C mulberry32 and fast-forwards the boot draws; every
// standalone `Math.random` record burns one draw and must match
// bit-exactly (the single frame-0 standalone draw is the off-step
// startGame draw — asserted exactly one); frame-0 runAI records are the
// synthetic-domain SWEEP and draw from a separate generator seeded
// 0x0badf00d, mirroring the capture's Math.random swap (rule 12).
// ai.c's draws go through ml_random() (logged in ml_events.rng), so the
// post `rng` list verifies count AND values at their exact sites; a
// dropped/extra draw additionally shifts the chain for every later
// record (cascade tooth).
//
// Usage: replay_ai_port <capture.jsonl> [--strict] [--max-print N]
//                       [--stop-first] [--cover]
//                       [--expect records=N,runAI=N,Math.random=N,rngBoot=N]
//                       [--cover-gate N]
//
// Marshalling is STRICT (prevention rule 7): any shape outside the
// captured domain aborts with exit 3 — never guess. An under-projected
// spec read-set surfaces here as a missing-key hard-fail, not a silent
// default.
//
// EVIDENCE COMPLETENESS (iter 77, review-75 M4): --expect pins the
// per-record-type inventory (strict grammar — exactly those four keys,
// each once, integer values; anything else is a parse death) and the
// getline loop is ferror-checked — a truncated or mid-read-failed
// capture is CORRUPTION, never a 0-divergence pass. --expect is
// REQUIRED under --strict (fail-closed evidence mode).
// COVERAGE GATE (iter 77, review-75 M7): --cover-gate N asserts the
// arm table is fully populated, exactly N arms have hits, and the 3
// documented-dead arms (H_LEDGE_CTA / GEN_RUT_UPTILT /
// FOX_RESPAWN_INARR — measured-dead upstream, AGENT-LOG iter 75) stay
// ZERO. A dead arm going live or a live arm going dead is loud.
// COVERAGE-TABLE PIN (iter 79, review-77 round-2 M2): --ncov-pin N
// asserts the compiled ML_AI_NCOV == the frozen pin (twin-pinned in
// check-ai-replay.sh + expected-capture-aiport.json coverage.ncov) and
// --dead-pin pins the dead-arm NAMES against the compiled list — a
// grown counter universe (new named-but-unhit arm) is a reviewed
// change, never a silent pass. Every strtol in this TU is
// errno/ERANGE-guarded (saturation = corruption, review-77 M1).
#include <errno.h>
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../ai.h"
#include "../ai_bridge.h" // ai_bridge_field_names / ai_row_field (tagged rows)
#include "../ml_events.h"
#include "../ml_rng.h"
#include "canon.h"

static const char *g_file = "?";
static long g_lineno = 0;

static void fail(const char *msg) {
  fprintf(stderr, "MARSHAL FAIL %s:%ld: %s\n", g_file, g_lineno, msg);
  exit(3);
}

void ml_canon_fail(const char *msg) { fail(msg); }
void ml_events_fail(const char *what) { fail(what); }
void ml_ai_out_of_domain(const char *what) {
  fprintf(stderr, "OUT OF DOMAIN %s:%ld: %s\n", g_file, g_lineno, what);
  exit(3);
}
void ml_input_out_of_domain(const char *what) { // ai_bridge.c linkage
  fprintf(stderr, "OUT OF DOMAIN %s:%ld: %s\n", g_file, g_lineno, what);
  exit(3);
}

// --- RNG chain ---------------------------------------------------------------

static MlRng g_rng;       // the seeded match stream (rngBoot seeds it)
static MlRng g_sweep_rng; // the sweep generator (spec-aiport.js literal)
static bool g_boot_seen = false;
static long g_frame0_draws = 0; // standalone frame-0 draws (must end == 1)

// --- generic canon helpers ---------------------------------------------------

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

// Representability guard (iter 77, review-75 M5): every cast of a
// CAPTURED double to an integer type proves membership in [lo, hi] and
// integrality FIRST — NaN fails the range comparison, so NaN/inf/huge/
// fractional bit-flips are a rule-7 marshal death, never UB. All
// integer-domain casts in this TU go through here (sole exception:
// atoi on the operator's own --max-print argv, which is not captured
// data and not decision-bearing).
static long cv_int_range(const CanonVal *v, double lo, double hi,
                         const char *what) {
  const double d = cv_num(v);
  if (!(d >= lo && d <= hi) || floor(d) != d) fail(what);
  return (long)d;
}

// number-or-undefined -> ToNumber (prevention rule 2): the ECBp
// components can hold undefined pre-first-physics (ml_player.h ecbpUndef
// class); ai.js only consumes them in comparisons where
// ToNumber(undefined) = NaN is exact.
static double cv_num_or_undef(const CanonVal *v) {
  if (v->type == CV_NUM) return v->num;
  if (v->type == CV_UNDEF) return nan("");
  fail("expected number|undef");
  return 0;
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
  if (o->type != CV_OBJ || o->nkeys != n) {
    char msg[128];
    snprintf(msg, sizeof msg,
             "unexpected object key count (want %d, got %d, first key %s)", n,
             o->type == CV_OBJ ? o->nkeys : -1,
             (o->type == CV_OBJ && o->nkeys > 0) ? o->keys[0] : "?");
    fail(msg);
  }
}

static Vec2D cv_vec2(const CanonVal *v) {
  if (v->type != CV_OBJ || v->nkeys != 2 || strcmp(v->keys[0], "x") != 0 ||
      strcmp(v->keys[1], "y") != 0) {
    fail("expected {x,y}");
  }
  Vec2D out = {cv_num(v->vals[0]), cv_num(v->vals[1])};
  return out;
}

static void cv_strcap(const CanonVal *v, char *dst) {
  const char *s = cv_str(v);
  if (strlen(s) >= ML_STR_CAP) fail("string exceeds ML_STR_CAP");
  strcpy(dst, s);
}

// --- tagged-row canon marshal/emit (canon key order == field order;
// the replay_ai.c pattern) ----------------------------------------------------

static MlAiVal aiv_from_canon(const CanonVal *v) {
  if (v->type == CV_BOOL) return aiv_bool(v->b);
  if (v->type == CV_NUM) return aiv_num(v->num);
  if (v->type == CV_UNDEF) return aiv_undef();
  fail("input-plane value outside the bool|number|undefined domain");
  return aiv_undef(); // unreachable
}

static MlAiInput ai_input_from_canon(const CanonVal *v) {
  if (v->type != CV_OBJ || v->nkeys != AI_BRIDGE_NFIELDS) {
    fail("expected a 22-key Input object");
  }
  MlAiInput row;
  for (int k = 0; k < AI_BRIDGE_NFIELDS; k++) {
    if (strcmp(v->keys[k], ai_bridge_field_names[k]) != 0) {
      fail("Input object key set/order drifted from the canon key order");
    }
    *ai_row_field(&row, k) = aiv_from_canon(v->vals[k]);
  }
  return row;
}

static void aiv_canon(CanonBuf *b, MlAiVal v) {
  if (v.tag == AIV_BOOL) cb_puts(b, v.b ? "T" : "F");
  else if (v.tag == AIV_NUM) cb_num(b, v.num);
  else cb_puts(b, "undef");
}

static void ai_input_canon(CanonBuf *b, const MlAiInput *row) {
  cb_putc(b, '{');
  for (int k = 0; k < AI_BRIDGE_NFIELDS; k++) {
    if (k) cb_putc(b, ',');
    cb_qstr(b, ai_bridge_field_names[k]);
    cb_putc(b, ':');
    aiv_canon(b, *ai_row_field_const(row, k));
  }
  cb_putc(b, '}');
}

// --- the pre-envelope marshal (FORMAT.md "The aiport spec") -----------------

// pre.p[k] — the grep-measured runAI read-set projection of one player.
static void cv_ai_player(const CanonVal *v, MlPlayer *out, MlAiSim *sim,
                         int k) {
  const CanonVal *cta = obj_get(v, "curentAction");
  obj_expect_keys(v, cta ? 12 : 11);
  memset(out, 0, sizeof *out);

  cv_strcap(obj_req(v, "actionState"), out->actionState);

  const CanonVal *ca = obj_req(v, "charAttributes");
  obj_expect_keys(ca, 1);
  const CanonVal *mj = obj_req(ca, "multiJump");
  if (mj->type == CV_UNDEF) {
    sim->multiJumpUndef[k] = true;
    sim->multiJump[k] = false;
  } else {
    sim->multiJumpUndef[k] = false;
    sim->multiJump[k] = cv_bool(mj);
  }

  if (cta) {
    sim->hasCurentAction[k] = true;
    cv_strcap(cta, sim->curentAction[k]);
  } else {
    sim->hasCurentAction[k] = false;
    sim->curentAction[k][0] = 0;
  }

  cv_strcap(obj_req(v, "currentAction"), out->currentAction);
  cv_strcap(obj_req(v, "currentSubaction"), out->currentSubaction);
  out->difficulty = cv_num(obj_req(v, "difficulty"));

  // q1: player-TOP-LEVEL `grounded` is never a property upstream — the
  // projection records it and this marshal hard-asserts it stays
  // undefined (ai.js:262's `!(undefined)` shape depends on it).
  if (obj_req(v, "grounded")->type != CV_UNDEF) {
    fail("player.grounded (top-level) is defined — q1 premise broken");
  }

  out->hasHit = cv_bool(obj_req(v, "hasHit"));

  const CanonVal *hit = obj_req(v, "hit");
  obj_expect_keys(hit, 2);
  out->hit.hitlag = cv_num(obj_req(hit, "hitlag"));
  out->hit.hitstun = cv_num(obj_req(hit, "hitstun"));

  out->lastMash = cv_num(obj_req(v, "lastMash"));

  const CanonVal *ph = obj_req(v, "phys");
  obj_expect_keys(ph, 12);
  const CanonVal *ecbp = obj_req(ph, "ECBp");
  if (ecbp->type != CV_ARR || ecbp->count != 4) fail("ECBp must be [4]");
  for (int j = 0; j < 4; j++) {
    const CanonVal *pt = ecbp->items[j];
    if (pt->type != CV_OBJ || pt->nkeys != 2 ||
        strcmp(pt->keys[0], "x") != 0 || strcmp(pt->keys[1], "y") != 0) {
      fail("ECBp point must be {x,y}");
    }
    out->phys.ECBp[j].x = cv_num_or_undef(pt->vals[0]);
    out->phys.ECBp[j].y = cv_num_or_undef(pt->vals[1]);
  }
  out->phys.cVel = cv_vec2(obj_req(ph, "cVel"));
  out->phys.doubleJumped = cv_bool(obj_req(ph, "doubleJumped"));
  out->phys.face = cv_num(obj_req(ph, "face"));
  out->phys.fastfalled = cv_bool(obj_req(ph, "fastfalled"));
  out->phys.grounded = cv_bool(obj_req(ph, "grounded"));
  out->phys.hurtBoxState = cv_num(obj_req(ph, "hurtBoxState"));
  out->phys.jumpsUsed = cv_num(obj_req(ph, "jumpsUsed"));
  out->phys.kVel = cv_vec2(obj_req(ph, "kVel"));
  out->phys.onLedge = cv_num(obj_req(ph, "onLedge"));
  out->phys.pos = cv_vec2(obj_req(ph, "pos"));
  out->phys.shieldHP = cv_num(obj_req(ph, "shieldHP"));

  out->timer = cv_num(obj_req(v, "timer"));
}

static void cv_surf_list(const CanonVal *v, Vec2D (*out)[2], int *outLen,
                         int cap) {
  if (v->type != CV_ARR || v->count > cap) fail("surface list shape");
  *outLen = v->count;
  for (int j = 0; j < v->count; j++) {
    const CanonVal *s = v->items[j];
    if (s->type != CV_ARR || s->count != 2) fail("surface must be a pair");
    out[j][0] = cv_vec2(s->items[0]);
    out[j][1] = cv_vec2(s->items[1]);
  }
}

static void cv_ai_stage(const CanonVal *v, MlAiStage *out) {
  obj_expect_keys(v, 3);
  cv_surf_list(obj_req(v, "ground"), out->ground, &out->groundLen,
               ML_AI_MAX_SURF);
  const CanonVal *lp = obj_req(v, "ledgePos");
  if (lp->type != CV_ARR || lp->count > ML_AI_MAX_LEDGE) {
    fail("ledgePos shape");
  }
  // rule 7 (iter 77, review-75 M6): ai.c's NearestLedge reads
  // ledgePos[0] unconditionally (ai.js:897 semantics) — an empty list
  // is outside the consumable domain and must die at the marshal, not
  // feed an out-of-bounds read. Every captured record carries 2 ledges
  // (measured, both goldens).
  if (lp->count < 1) {
    fail("ledgePos empty — ai.c NearestLedge reads [0] unconditionally");
  }
  out->ledgePosLen = lp->count;
  for (int j = 0; j < lp->count; j++) out->ledgePos[j] = cv_vec2(lp->items[j]);
  cv_surf_list(obj_req(v, "platform"), out->platform, &out->platformLen,
               ML_AI_MAX_SURF);
}

// --- the runAI record ------------------------------------------------------

static MlPlayer g_players[4];
static MlAiInput g_bank[4][8];
static MlAiStage g_stage;

static void run_ai_record(long frame, const CanonVal *args, CanonBuf *post) {
  if (args->type != CV_ARR || args->count != 2) fail("runAI: want [i, pre]");
  // representability BEFORE the cast (review-75 M5): NaN/inf/huge die here
  const int slot = (int)cv_int_range(args->items[0], 0, 3, "runAI: bad slot");
  const CanonVal *pre = args->items[1];
  obj_expect_keys(pre, 6);

  MlAiSim sim;
  memset(&sim, 0, sizeof sim);
  sim.aS = &g_stage;
  sim.bank = g_bank;

  cv_ai_stage(obj_req(pre, "as"), &g_stage);

  const CanonVal *bank2 = obj_req(pre, "bank2");
  if (bank2->type != CV_ARR || bank2->count != 2) fail("bank2 must be [2]");
  memset(g_bank, 0, sizeof g_bank);
  g_bank[slot][0] = ai_input_from_canon(bank2->items[0]);
  g_bank[slot][1] = ai_input_from_canon(bank2->items[1]);

  const CanonVal *cs = obj_req(pre, "cs");
  const CanonVal *pt = obj_req(pre, "pt");
  if (cs->type != CV_ARR || cs->count != 4) fail("cs must be [4]");
  if (pt->type != CV_ARR || pt->count != 4) fail("pt must be [4]");
  for (int k = 0; k < 4; k++) {
    sim.cS[k] = cv_num(cs->items[k]);
    sim.playerType[k] = cv_num(pt->items[k]);
  }

  const CanonVal *gs = obj_req(pre, "gs");
  obj_expect_keys(gs, 1);
  // gameSettings.turbo is a NUMBER in the live domain (the qjs
  // getGameplayCookies zeroing class — Number("") -> 0) and a boolean in
  // the sweep presets; ai.js consumes it by truthiness only. Marshal the
  // measured bool|number domain by JS truthiness.
  const CanonVal *turbo = obj_req(gs, "turbo");
  if (turbo->type == CV_BOOL) sim.turbo = turbo->b;
  else if (turbo->type == CV_NUM) {
    sim.turbo = turbo->num != 0 && !isnan(turbo->num);
  } else {
    fail("gameSettings.turbo outside the bool|number domain");
  }

  const CanonVal *p4 = obj_req(pre, "p");
  if (p4->type != CV_ARR || p4->count != 4) fail("p must be [4]");
  for (int k = 0; k < 4; k++) {
    cv_ai_player(p4->items[k], &g_players[k], &sim, k);
    sim.player[k] = &g_players[k];
  }

  // frame-0 records are the sweep (Math.random swapped in the capture);
  // in-match records draw the chained seeded stream.
  ml_active_rng = frame == 0 ? &g_sweep_rng : &g_rng;
  ml_ev_reset();

  ml_runAI(&sim, slot);

  // ai.c has no sound/vfx/dispatch surface — anything here is a bug.
  if (ml_events.snd_count || ml_events.dsp_count || ml_events.vfx_count) {
    fail("ai.c touched a non-rng event queue");
  }

  // post envelope {bank, bk, rng} — byte-parallel to the spec's literal.
  // bk is the FOUR-SLOT bookkeeping array (iter 77, review-75 M3): a
  // foreign-slot bookkeeping write in either implementation is now a
  // divergence, never silently oracle-fed into the next record.
  cb_puts(post, "{\"bank\":");
  ai_input_canon(post, &g_bank[slot][0]);
  cb_puts(post, ",\"bk\":[");
  for (int k = 0; k < 4; k++) {
    if (k) cb_putc(post, ',');
    cb_puts(post, "{\"ca\":");
    cb_qstr(post, g_players[k].currentAction);
    cb_puts(post, ",\"cs\":");
    cb_qstr(post, g_players[k].currentSubaction);
    cb_puts(post, ",\"cta\":");
    if (sim.hasCurentAction[k]) cb_qstr(post, sim.curentAction[k]);
    else cb_puts(post, "undef");
    cb_puts(post, ",\"lm\":");
    cb_num(post, g_players[k].lastMash);
    cb_putc(post, '}');
  }
  cb_puts(post, "],\"rng\":[");
  for (int k = 0; k < ml_events.rng_count; k++) {
    if (k) cb_putc(post, ',');
    cb_num(post, ml_events.rng[k]);
  }
  cb_puts(post, "]}");
}

// --- the --expect inventory (iter 77, review-75 M4) --------------------------
// Strict grammar (whitelist rule, PROCESS section 3): exactly the four
// known keys, each exactly once, comma-separated, values nonnegative
// decimal integers with no trailing bytes. Anything else — unknown key,
// duplicate, malformed number — is a parse death, never a partial pin.

static const char *const g_exp_keys[4] = {"records", "runAI", "Math.random",
                                          "rngBoot"};
static long g_exp_vals[4];
static bool g_exp_seen[4];

static void parse_expect(const char *spec) {
  char buf[256];
  if (strlen(spec) >= sizeof buf) {
    fprintf(stderr, "EXPECT PARSE FAIL: spec too long\n");
    exit(1);
  }
  strcpy(buf, spec);
  char *save = NULL;
  for (char *tok = strtok_r(buf, ",", &save); tok;
       tok = strtok_r(NULL, ",", &save)) {
    char *eq = strchr(tok, '=');
    if (!eq || eq == tok || eq[1] == 0) {
      fprintf(stderr, "EXPECT PARSE FAIL: token '%s' is not key=value\n", tok);
      exit(1);
    }
    *eq = 0;
    int idx = -1;
    for (int k = 0; k < 4; k++) {
      if (strcmp(tok, g_exp_keys[k]) == 0) idx = k;
    }
    if (idx == -1) {
      fprintf(stderr, "EXPECT PARSE FAIL: unknown key '%s'\n", tok);
      exit(1);
    }
    if (g_exp_seen[idx]) {
      fprintf(stderr, "EXPECT PARSE FAIL: duplicate key '%s'\n", tok);
      exit(1);
    }
    char *end = NULL;
    errno = 0; // ERANGE class guard (iter 79, review-77 round-2 M1)
    const long v = strtol(eq + 1, &end, 10);
    if (errno == ERANGE || end == eq + 1 || *end != 0 || v < 0) {
      fprintf(stderr, "EXPECT PARSE FAIL: bad count '%s' for key '%s'\n",
              eq + 1, tok);
      exit(1);
    }
    g_exp_seen[idx] = true;
    g_exp_vals[idx] = v;
  }
  for (int k = 0; k < 4; k++) {
    if (!g_exp_seen[k]) {
      fprintf(stderr, "EXPECT PARSE FAIL: missing key '%s'\n", g_exp_keys[k]);
      exit(1);
    }
  }
}

// The 3 documented-dead arms (measured-dead upstream, AGENT-LOG iter 75;
// FORMAT.md "The aiport spec"): pinned ZERO by --cover-gate.
static const char *const g_dead_arms[3] = {"H_LEDGE_CTA", "GEN_RUT_UPTILT",
                                           "FOX_RESPAWN_INARR"};

// --- main --------------------------------------------------------------------

int main(int argc, char **argv) {
  const char *path = NULL;
  const char *expect_spec = NULL;
  bool strict = false, stop_first = false, cover = false;
  long cover_gate = -1;
  long ncov_pin = -1;              // iter 79: pinned ML_AI_NCOV
  const char *dead_pin_spec = NULL; // iter 79: pinned dead-arm names (csv)
  int max_print = 5;
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--strict") == 0) strict = true;
    else if (strcmp(argv[i], "--stop-first") == 0) stop_first = true;
    else if (strcmp(argv[i], "--cover") == 0) cover = true;
    else if (strcmp(argv[i], "--expect") == 0 && i + 1 < argc) expect_spec = argv[++i];
    else if (strcmp(argv[i], "--cover-gate") == 0 && i + 1 < argc) {
      char *end = NULL;
      errno = 0; // ERANGE class guard (iter 79)
      cover_gate = strtol(argv[++i], &end, 10);
      if (errno == ERANGE || end == argv[i] || *end != 0 || cover_gate < 0) {
        fprintf(stderr, "bad --cover-gate value '%s'\n", argv[i]);
        return 1;
      }
    }
    else if (strcmp(argv[i], "--ncov-pin") == 0 && i + 1 < argc) {
      char *end = NULL;
      errno = 0; // ERANGE class guard (iter 79)
      ncov_pin = strtol(argv[++i], &end, 10);
      if (errno == ERANGE || end == argv[i] || *end != 0 || ncov_pin < 0) {
        fprintf(stderr, "bad --ncov-pin value '%s'\n", argv[i]);
        return 1;
      }
    }
    else if (strcmp(argv[i], "--dead-pin") == 0 && i + 1 < argc) {
      dead_pin_spec = argv[++i];
    }
    else if (strcmp(argv[i], "--max-print") == 0 && i + 1 < argc) max_print = atoi(argv[++i]);
    else if (!path) path = argv[i];
    else { fprintf(stderr, "unknown arg: %s\n", argv[i]); return 1; }
  }
  if (!path) {
    fprintf(stderr, "usage: replay_ai_port <capture.jsonl> [--strict] "
                    "[--max-print N] [--stop-first] [--cover]\n"
                    "       [--expect records=N,runAI=N,Math.random=N,rngBoot=N]"
                    " [--cover-gate N]\n"
                    "       [--ncov-pin N] [--dead-pin name1,name2,name3]\n");
    return 1;
  }
  // fail-closed evidence mode (review-75 M4): strict replay without the
  // record inventory would accept a truncated capture as 0 divergences.
  if (strict && !expect_spec) {
    fprintf(stderr, "replay_ai_port: --strict requires --expect "
                    "(evidence-completeness inventory; review-75 M4)\n");
    return 1;
  }
  if (expect_spec) parse_expect(expect_spec);
  FILE *f = fopen(path, "r");
  if (!f) { fprintf(stderr, "cannot open %s\n", path); return 1; }
  g_file = path;

  ml_rng_seed(&g_sweep_rng, 0x0badf00du);
  ml_active_rng = &g_rng;

  char *line = NULL;
  size_t linecap = 0;
  long replayed = 0, divergences = 0, printed = 0;
  long n_runai = 0, n_mr = 0, n_boot = 0; // per-record-type inventory (M4)
  long first_div_line = -1;

  CanonBuf out, post;
  cb_init(&out);
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
    // frame field: strict parse (M5 sibling audit, iter 77) — the value
    // routes the RNG-generator decision (frame 0 = sweep), so a
    // garbage-to-0 permissive parse would silently misroute a record.
    // ERANGE (iter 79, review-77 round-2 M1): an overflowing decimal
    // saturates to LONG_MAX with no other symptom — routing only
    // distinguishes zero from nonzero, so saturation would pass every
    // downstream gate. Overflow is CORRUPTION, never a lucky route.
    char *frame_end = NULL;
    errno = 0;
    const long frame = strtol(frame_s, &frame_end, 10);
    if (errno == ERANGE || frame_end == frame_s || *frame_end != 0 ||
        frame < 0) {
      fail("malformed frame field");
    }

    canon_arena_reset();
    const char *err = NULL;
    const CanonVal *args = canon_parse(args_s, &err);
    if (!args) {
      fprintf(stderr, "PARSE FAIL %s:%ld: %s\n", path, g_lineno, err);
      return 3;
    }

    out.len = 0; out.buf[0] = 0;
    post.len = 0; post.buf[0] = 0;
    bool want_post = false;

    if (strcmp(fn, "rngBoot") == 0) {
      n_boot++;
      if (args->type != CV_ARR || args->count != 2) fail("rngBoot shape");
      if (g_boot_seen) fail("rngBoot: seen twice");
      g_boot_seen = true;
      // representability BEFORE the casts (review-75 M5)
      const uint32_t seed = (uint32_t)cv_int_range(
          args->items[0], 0, 4294967295.0, "rngBoot: bad seed");
      const long boot = cv_int_range(args->items[1], 0, 2147483647.0,
                                     "rngBoot: bad count");
      ml_rng_seed(&g_rng, seed);
      for (long k = 0; k < boot; k++) (void)ml_rng_next(&g_rng);
      cb_num(&out, (double)g_rng.a); // additive-advance formula pin
    } else if (strcmp(fn, "Math.random") == 0) {
      n_mr++;
      if (args->type != CV_ARR || args->count != 0) fail("Math.random shape");
      if (!g_boot_seen) fail("Math.random before rngBoot");
      if (frame == 0) g_frame0_draws++;
      cb_num(&out, ml_rng_next(&g_rng));
    } else if (strcmp(fn, "runAI") == 0) {
      n_runai++;
      if (!g_boot_seen) fail("runAI before rngBoot");
      if (!post_s) fail("runAI: missing post-state field");
      want_post = true;
      run_ai_record(frame, args, &post);
      cb_puts(&out, "undef"); // void mutator
    } else if (strcmp(fn, "wsViol") == 0) {
      fail("wsViol record present (pinned ZERO — capture is invalid)");
    } else {
      fail("unknown boundary function");
    }

    if (!want_post && post_s) fail("unexpected post-state field");

    bool diverged = strcmp(out.buf, ret_s) != 0;
    const char *got2 = NULL, *exp2 = NULL;
    if (want_post && strcmp(post.buf, post_s) != 0) {
      diverged = true;
      got2 = post.buf;
      exp2 = post_s;
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
                  "  post expected: %.600s\n  post got:      %.600s\n",
                  exp2, got2);
        }
      }
      if (stop_first) break;
    }
  }
  // EVIDENCE COMPLETENESS (iter 77, review-75 M4): getline returns -1
  // on error AND on EOF — a mid-file read error must be a corruption
  // death, never a short-but-clean replay.
  if (ferror(f)) {
    fprintf(stderr, "READ FAIL %s:%ld: stream error mid-capture "
                    "(ferror set — CORRUPT evidence, never EOF)\n",
            path, g_lineno);
    return 3;
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

  // per-record-type inventory vs the pinned counts (review-75 M4):
  // truncation preserves well-formedness of every surviving record —
  // only the inventory can see it.
  if (expect_spec) {
    const long got[4] = {replayed, n_runai, n_mr, n_boot};
    bool inv_fail = false;
    for (int k = 0; k < 4; k++) {
      if (got[k] != g_exp_vals[k]) {
        fprintf(stderr,
                "INVENTORY FAIL: %s count %ld, pinned %ld "
                "(a truncated/partial capture is CORRUPT evidence)\n",
                g_exp_keys[k], got[k], g_exp_vals[k]);
        inv_fail = true;
      }
    }
    if (inv_fail) return 2;
  }

  if (cover) {
    fprintf(stderr, "-- ai.c arm coverage (records that hit each arm) --\n");
    for (int k = 0; k < ML_AI_NCOV && ml_ai_cov_names[k]; k++) {
      fprintf(stderr, "COV %-24s %ld\n", ml_ai_cov_names[k], ml_ai_cov[k]);
    }
  }

  // COVERAGE-TABLE PIN (iter 79, review-77 round-2 M2): the counter
  // UNIVERSE itself is pinned. The round-2 accident: growing ML_AI_NCOV
  // together with a new NAMED-but-unhit arm grows both sides of the
  // named==ML_AI_NCOV check in lockstep — live stays at the gate value,
  // the dead trio stays zero, and a newly UNCOVERED branch passes.
  // --ncov-pin carries the frozen table size (twin-pinned:
  // check-ai-replay.sh literal + expected-capture-aiport.json
  // coverage.ncov); table growth is a REVIEWED change touching all
  // three pin sites, never a silent bump.
  if (ncov_pin >= 0 && ncov_pin != ML_AI_NCOV) {
    fprintf(stderr,
            "COVERAGE PIN FAIL: compiled ML_AI_NCOV %d != pinned %ld "
            "(the counter universe changed — re-measure live/dead arms "
            "and update the twin pins deliberately)\n",
            ML_AI_NCOV, ncov_pin);
    return 2;
  }
  // --dead-pin: the pinned dead-arm NAMES must be exactly the compiled
  // g_dead_arms list (bijection, no dupes) — a renamed/dropped/added
  // dead arm is drift between the pins file and the binary.
  if (dead_pin_spec) {
    bool matched[3] = {false, false, false};
    int ntok = 0;
    char dbuf[256];
    if (strlen(dead_pin_spec) >= sizeof dbuf) {
      fprintf(stderr, "bad --dead-pin value (too long)\n");
      return 1;
    }
    strcpy(dbuf, dead_pin_spec);
    for (char *tok = strtok(dbuf, ","); tok; tok = strtok(NULL, ",")) {
      ntok++;
      int hit = -1;
      for (int d = 0; d < 3; d++) {
        if (strcmp(tok, g_dead_arms[d]) == 0) hit = d;
      }
      if (hit == -1 || matched[hit]) {
        fprintf(stderr,
                "DEAD-ARM PIN FAIL: pinned name '%s' %s the compiled dead "
                "list — the pins file and the binary disagree; re-measure "
                "deadness, never rename silently\n",
                tok, hit == -1 ? "is not in" : "duplicates within");
        return 2;
      }
      matched[hit] = true;
    }
    if (ntok != 3) {
      fprintf(stderr,
              "DEAD-ARM PIN FAIL: %d pinned dead arms, compiled list has 3\n",
              ntok);
      return 2;
    }
  }

  // COVERAGE GATE (iter 77, review-75 M7): live-arm count pinned, dead
  // arms pinned ZERO — replacing a live preset with a duplicate (counts
  // preserved, chain untouched) or a dead arm going live is now loud.
  if (cover_gate >= 0) {
    int named = 0;
    long live = 0;
    for (int k = 0; k < ML_AI_NCOV && ml_ai_cov_names[k]; k++) {
      named++;
      if (ml_ai_cov[k] > 0) live++;
    }
    if (named != ML_AI_NCOV) {
      fprintf(stderr,
              "COVERAGE GATE FAIL: arm table has %d named arms, want %d "
              "(a shrunken counter universe is CORRUPT instrumentation)\n",
              named, ML_AI_NCOV);
      return 2;
    }
    if (live != cover_gate) {
      fprintf(stderr,
              "COVERAGE GATE FAIL: %ld arms hit, pinned %ld "
              "(a live arm going dead — e.g. a duplicated preset — or a "
              "dead arm going live is drift, never a pass)\n",
              live, cover_gate);
      return 2;
    }
    for (int d = 0; d < 3; d++) {
      int idx = -1;
      for (int k = 0; k < ML_AI_NCOV && ml_ai_cov_names[k]; k++) {
        if (strcmp(ml_ai_cov_names[k], g_dead_arms[d]) == 0) idx = k;
      }
      if (idx == -1) {
        fprintf(stderr,
                "COVERAGE GATE FAIL: documented-dead arm '%s' missing from "
                "the table (renamed/dropped counter — fail closed)\n",
                g_dead_arms[d]);
        return 2;
      }
      if (ml_ai_cov[idx] != 0) {
        fprintf(stderr,
                "COVERAGE GATE FAIL: documented-dead arm '%s' went LIVE "
                "(%ld hits) — the deadness measurement (AGENT-LOG iter 75) "
                "no longer holds; reclassify before passing\n",
                g_dead_arms[d], ml_ai_cov[idx]);
        return 2;
      }
    }
  }

  printf("AI PORT REPLAY RAN %ld records, %ld divergences", replayed,
         divergences);
  if (first_div_line != -1) printf(" (first at line %ld)", first_div_line);
  printf("\n");
  cb_free(&out);
  cb_free(&post);
  if (strict && divergences > 0) return 2;
  return 0;
}
