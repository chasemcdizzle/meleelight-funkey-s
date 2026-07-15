// replay_platforms.c — M2 task 14 replay driver: feeds recorded
// movingPlatforms boundary calls (build/<id>.platforms.jsonl, FORMAT.md
// "The platforms spec") to the C stage-tick translation
// (port/sim/stages/{moving_platforms,ystory,fountain}.c) and compares
// canon-v1.1 serializations byte-for-byte.
//
// Record types:
// - rngBoot / Math.random: the chained seeded mulberry32; frame-0 records
//   replay on the SEPARATE sweep mulberry32 (0x0badf00d).
// - movingPlatforms: args [stageName, pre]; marshal the per-stage pre
//   envelope, run the C body, compare the post bit-exactly. Fountain's
//   owner draws replay through ml_random on the active chain.
//
// STAGE-PLANE CHAIN INSTRUMENT (fix_plan §M2 rule 18): the platform plane
// (EVERY platform, static ones included) and fountain's platformStates
// are C module state chained across records — movingPlatforms is the only
// upstream writer of both (measured; the chain turns that enclosure
// assumption into a per-record measurement). From the first in-match
// record on, the chained state is COMPARED against each record's pre
// before being re-marshaled (authoritative) — a wrong platform/ps
// mutation flags at the very next record even when that record's own
// body replays clean. Frame-0 sweep records poke state directly, so the
// chain only arms on frames >= 1.
//
// Marshalling is STRICT (prevention rule 7): any shape outside the
// captured domain aborts with exit 3 — never guess.
//
// Usage: replay_platforms <capture.jsonl> [--strict] [--max-print N]
//                         [--stop-first]
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../ml_events.h"
#include "../stages/moving_platforms.h"
#include "canon.h"

static const char *g_file = "?";
static long g_lineno = 0;

static void fail(const char *msg) {
  fprintf(stderr, "MARSHAL FAIL %s:%ld: %s\n", g_file, g_lineno, msg);
  exit(3);
}

void ml_canon_fail(const char *msg) { fail(msg); }
void ml_events_fail(const char *what) { fail(what); }

// --- divergence accounting -------------------------------------------------
static long g_divergences = 0;
static long g_first_div_line = -1;
static long g_printed = 0;
static int g_max_print = 5;

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

// --- small marshal helpers ---------------------------------------------------
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
static Vec2D cv_vec2(const CanonVal *v) {
  if (v->type != CV_OBJ || v->nkeys != 2 || strcmp(v->keys[0], "x") != 0 ||
      strcmp(v->keys[1], "y") != 0) {
    fail("expected {x,y}");
  }
  Vec2D out = {cv_num(v->vals[0]), cv_num(v->vals[1])};
  return out;
}
static const CanonVal *obj_req(const CanonVal *o, const char *key) {
  if (o->type != CV_OBJ) fail("expected object");
  for (int i = 0; i < o->nkeys; i++) {
    if (strcmp(o->keys[i], key) == 0) return o->vals[i];
  }
  fail("missing object key");
  return 0;
}
static char *xstrdup(const char *s) {
  char *d = strdup(s);
  if (!d) { fprintf(stderr, "oom\n"); exit(1); }
  return d;
}

// --- generic canon reserializer (chain-compare carrier) ----------------------
static void reser(CanonBuf *b, const CanonVal *v) {
  switch (v->type) {
    case CV_NULL: cb_puts(b, "null"); break;
    case CV_UNDEF: cb_puts(b, "undef"); break;
    case CV_FN: cb_puts(b, "fn"); break;
    case CV_CYC: cb_puts(b, "cyc"); break;
    case CV_BOOL: cb_puts(b, v->b ? "T" : "F"); break;
    case CV_NUM: cb_num(b, v->num); break;
    case CV_STR: cb_qstr(b, v->str); break;
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

// --- RNG chains ---------------------------------------------------------------
static MlRng g_seeded;
static MlRng g_sweep;
static bool g_boot_seen = false;

// --- the chained stage state ----------------------------------------------------
static MpSim g_state;
static bool g_chain_armed = false;

// --- stage-name table ------------------------------------------------------------
typedef struct {
  const char *name;
  MpStageKind kind;
  int envKeys; // pre envelope key count (lean 1 / ystory 2 / fountain 4)
} StageInfo;

static const StageInfo STAGE_INFO[6] = {
    {"battlefield", MP_BATTLEFIELD, 1}, {"ystory", MP_YSTORY, 2},
    {"pstadium", MP_PSTADIUM, 1},       {"dreamland", MP_DREAMLAND, 1},
    {"fdest", MP_FDEST, 1},             {"fountain", MP_FOUNTAIN, 4},
};

static const StageInfo *stage_info(const char *name) {
  for (int i = 0; i < 6; i++) {
    if (strcmp(STAGE_INFO[i].name, name) == 0) return &STAGE_INFO[i];
  }
  fail("unknown stage name");
  return 0;
}

// --- marshal / serialize ------------------------------------------------------------
static void marshal_plat(const CanonVal *v) {
  if (v->type != CV_ARR || v->count > MP_MAX_PLATFORMS) fail("plat shape");
  g_state.nPlat = v->count;
  for (int i = 0; i < v->count; i++) {
    const CanonVal *pair = v->items[i];
    if (pair->type != CV_ARR || pair->count != 2) fail("plat pair shape");
    g_state.platform[i][0] = cv_vec2(pair->items[0]);
    g_state.platform[i][1] = cv_vec2(pair->items[1]);
  }
}

static void marshal_players(const CanonVal *v) {
  if (v->type != CV_ARR || v->count != 4) fail("players shape");
  for (int j = 0; j < 4; j++) {
    const CanonVal *p = v->items[j];
    if (p->type != CV_OBJ || p->nkeys != 3 ||
        strcmp(p->keys[0], "grounded") != 0 ||
        strcmp(p->keys[1], "onSurface") != 0 ||
        strcmp(p->keys[2], "pos") != 0) {
      fail("player slice shape");
    }
    g_state.player[j].grounded = cv_bool(p->vals[0]);
    const CanonVal *os = p->vals[1];
    if (os->type != CV_ARR || os->count != 2) fail("onSurface shape");
    g_state.player[j].onSurface[0] = cv_num(os->items[0]);
    g_state.player[j].onSurface[1] = cv_num(os->items[1]);
    g_state.player[j].pos = cv_vec2(p->vals[2]);
  }
}

static void marshal_ps(const CanonVal *v) {
  if (v->type != CV_ARR || v->count != 2) fail("ps shape");
  for (int j = 0; j < 2; j++) {
    const CanonVal *e = v->items[j];
    if (e->type != CV_OBJ || e->nkeys != 3 ||
        strcmp(e->keys[0], "destination") != 0 ||
        strcmp(e->keys[1], "state") != 0 ||
        strcmp(e->keys[2], "timer") != 0) {
      fail("ps entry shape");
    }
    g_state.ps[j].destination = cv_num(e->vals[0]);
    const char *st = cv_str(e->vals[1]);
    if (strcmp(st, "static") == 0) g_state.ps[j].isStatic = true;
    else if (strcmp(st, "moving") == 0) g_state.ps[j].isStatic = false;
    else fail("ps state domain");
    g_state.ps[j].timer = cv_num(e->vals[2]);
  }
}

static void ser_vec(CanonBuf *b, Vec2D v) {
  cb_puts(b, "{\"x\":");
  cb_num(b, v.x);
  cb_puts(b, ",\"y\":");
  cb_num(b, v.y);
  cb_putc(b, '}');
}

static void ser_plat(CanonBuf *b) {
  cb_putc(b, '[');
  for (int i = 0; i < g_state.nPlat; i++) {
    if (i) cb_putc(b, ',');
    cb_putc(b, '[');
    ser_vec(b, g_state.platform[i][0]);
    cb_putc(b, ',');
    ser_vec(b, g_state.platform[i][1]);
    cb_putc(b, ']');
  }
  cb_putc(b, ']');
}

static void ser_players(CanonBuf *b) {
  cb_putc(b, '[');
  for (int j = 0; j < 4; j++) {
    if (j) cb_putc(b, ',');
    cb_puts(b, "{\"grounded\":");
    cb_puts(b, g_state.player[j].grounded ? "T" : "F");
    cb_puts(b, ",\"onSurface\":[");
    cb_num(b, g_state.player[j].onSurface[0]);
    cb_putc(b, ',');
    cb_num(b, g_state.player[j].onSurface[1]);
    cb_puts(b, "],\"pos\":");
    ser_vec(b, g_state.player[j].pos);
    cb_putc(b, '}');
  }
  cb_putc(b, ']');
}

static void ser_ps(CanonBuf *b) {
  cb_putc(b, '[');
  for (int j = 0; j < 2; j++) {
    if (j) cb_putc(b, ',');
    cb_puts(b, "{\"destination\":");
    cb_num(b, g_state.ps[j].destination);
    cb_puts(b, ",\"state\":");
    cb_qstr(b, g_state.ps[j].isStatic ? "static" : "moving");
    cb_puts(b, ",\"timer\":");
    cb_num(b, g_state.ps[j].timer);
    cb_putc(b, '}');
  }
  cb_putc(b, ']');
}

// chain instrument: compare the CHAINED platform plane (and, on fountain,
// platformStates) against a record's pre before re-marshaling.
static void chain_check(const CanonVal *pre, bool fountain, long lineno) {
  {
    char *want = reser_dup(obj_req(pre, "plat"));
    CanonBuf b;
    cb_init(&b);
    ser_plat(&b);
    if (strcmp(b.buf, want) != 0) report_div("chain-plat", lineno, want, b.buf);
    cb_free(&b);
    free(want);
  }
  if (fountain) {
    char *want = reser_dup(obj_req(pre, "ps"));
    CanonBuf b;
    cb_init(&b);
    ser_ps(&b);
    if (strcmp(b.buf, want) != 0) report_div("chain-ps", lineno, want, b.buf);
    cb_free(&b);
    free(want);
  }
}

// --- main --------------------------------------------------------------------------
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
    fprintf(stderr, "usage: replay_platforms <capture.jsonl> [--strict] "
                    "[--max-print N] [--stop-first]\n");
    return 1;
  }
  FILE *f = fopen(path, "r");
  if (!f) { fprintf(stderr, "cannot open %s\n", path); return 1; }
  g_file = path;

  ml_rng_seed(&g_sweep, 0x0badf00d);
  memset(&g_state, 0, sizeof g_state);

  char *line = NULL;
  size_t linecap = 0;
  long replayed = 0;
  long mp_records = 0, rng_records = 0;

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

    if (strcmp(fn, "movingPlatforms") != 0) fail("unknown record function");
    if (!post_s) fail("movingPlatforms: missing post-state field");
    if (!g_boot_seen) fail("movingPlatforms before rngBoot");
    if (strcmp(ret_s, "undef") != 0) fail("movingPlatforms ret domain");

    canon_arena_reset();
    const char *err = 0;
    const CanonVal *args = canon_parse(args_s, &err);
    if (!args) {
      fprintf(stderr, "PARSE FAIL %s:%ld args: %s\n", path, g_lineno, err);
      return 3;
    }
    if (args->type != CV_ARR || args->count != 2) fail("args shape");
    const StageInfo *si = stage_info(cv_str(args->items[0]));
    const CanonVal *pre = args->items[1];
    if (pre->type != CV_OBJ || pre->nkeys != si->envKeys) {
      fail("pre envelope key count");
    }
    const bool fountain = si->kind == MP_FOUNTAIN;
    const bool ystory = si->kind == MP_YSTORY;

    // chain instrument (in-match records only; sweep pokes are direct)
    if (g_chain_armed && frame > 0) chain_check(pre, fountain, g_lineno);

    // authoritative pre marshal
    marshal_plat(obj_req(pre, "plat"));
    if (ystory || fountain) marshal_players(obj_req(pre, "players"));
    if (fountain) {
      marshal_ps(obj_req(pre, "ps"));
      g_state.starting = cv_bool(obj_req(pre, "starting"));
    }

    // frame-0 records are the rule-12 sweep: separate sweep chain
    ml_active_rng = frame == 0 ? &g_sweep : &g_seeded;
    ml_ev_reset();

    mp_movingPlatforms(si->kind, &g_state);

    if (ml_events.snd_count || ml_events.vfx_count || ml_events.dsp_count) {
      fail("non-rng events escaped movingPlatforms");
    }
    if (!fountain && ml_events.rng_count) {
      fail("a non-fountain movingPlatforms drew the seeded stream");
    }

    // post envelope
    out.len = 0;
    out.buf[0] = 0;
    cb_puts(&out, "{\"plat\":");
    ser_plat(&out);
    if (ystory || fountain) {
      cb_puts(&out, ",\"players\":");
      ser_players(&out);
    }
    if (fountain) {
      cb_puts(&out, ",\"ps\":");
      ser_ps(&out);
      cb_puts(&out, ",\"rng\":[");
      for (int i = 0; i < ml_events.rng_count; i++) {
        if (i) cb_putc(&out, ',');
        cb_num(&out, ml_events.rng[i]);
      }
      cb_putc(&out, ']');
    }
    cb_putc(&out, '}');
    if (strcmp(out.buf, post_s) != 0) {
      report_div("movingPlatforms", g_lineno, post_s, out.buf);
    }

    if (frame > 0) g_chain_armed = true;
    mp_records++;
    replayed++;
    if (g_divergences > 0 && stop_first) break;
  }
  free(line);
  fclose(f);

  if (!g_boot_seen) fail("capture carries no rngBoot record");

  fprintf(stderr,
          "replayed: %ld movingPlatforms records (stage plane + "
          "platformStates chain-verified), %ld standalone draws\n",
          mp_records, rng_records);
  printf("PLATFORMS REPLAY RAN %ld records, %ld divergences", replayed,
         g_divergences);
  if (g_first_div_line != -1) printf(" (first at line %ld)", g_first_div_line);
  printf("\n");
  cb_free(&out);
  if (strict && g_divergences > 0) return 2;
  return 0;
}
