// replay_envcoll.c — M2-CAL replay driver: feeds recorded module-boundary
// calls (port/sim/calib/build/<id>.envcoll.jsonl, FORMAT.md) to the C
// translation of environmentalCollision.js and compares canon-v1
// serializations byte-for-byte. A single differing bit anywhere in the
// return value is a divergence.
//
// Usage: replay_envcoll <capture.jsonl> [--strict] [--max-print N]
//                       [--only-fn NAME] [--stop-first]
//   --strict     exit 2 if any divergence (the gate mode); default exit 0
//                after a completed run (task-2 "runs end-to-end" mode)
//   --max-print  divergence details to print (default 5)
//   --only-fn    replay only records of one function
//   --stop-first stop at the first divergence
//
// Marshalling is STRICT: any argument shape outside the captured domain
// (unknown keys, unexpected types) aborts with exit 3 — never guess.
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../environmental_collision.h"
#include "canon.h"

static const char *g_file = "?";
static long g_lineno = 0;

static void fail(const char *msg) {
  fprintf(stderr, "MARSHAL FAIL %s:%ld: %s\n", g_file, g_lineno, msg);
  exit(3);
}

// --- marshalling (canon tree -> C values) ---------------------------------

static double cv_number(const CanonVal *v) {
  if (v->type == CV_NUM) return v->num;
  // Pinned invariant (expected-capture.json): undef appears in ARG position
  // only; ToNumber(undefined) is the canonical NaN.
  if (v->type == CV_UNDEF) return js_nan();
  fail("expected number");
  return 0;
}

static Vec2D cv_vec2d(const CanonVal *v) {
  if (v->type != CV_OBJ || v->nkeys != 2 ||
      strcmp(v->keys[0], "x") != 0 || strcmp(v->keys[1], "y") != 0) {
    fail("expected Vec2D {x,y}");
  }
  return vec2d(cv_number(v->vals[0]), cv_number(v->vals[1]));
}

static Line2 cv_line(const CanonVal *v) {
  if (v->type != CV_ARR || v->count != 2) fail("expected line [Vec2D,Vec2D]");
  return line2(cv_vec2d(v->items[0]), cv_vec2d(v->items[1]));
}

static ECB cv_ecb(const CanonVal *v) {
  if (v->type != CV_ARR || v->count != 4) fail("expected ECB [Vec2D x4]");
  ECB e;
  for (int i = 0; i < 4; i++) e.pt[i] = cv_vec2d(v->items[i]);
  return e;
}

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
  s.p0 = cv_vec2d(v->items[0]);
  s.p1 = cv_vec2d(v->items[1]);
  if (v->count == 3) {
    const CanonVal *props = v->items[2];
    if (props->type == CV_UNDEF || props->type == CV_NULL) {
      // wall[2] === undefined behaves as "no props"; a literal null third
      // element would crash upstream (wall[2].damageType) — out of domain.
      fail("length-3 surface with undef/null props (out of captured domain)");
    }
    if (props->type != CV_OBJ) fail("surface props not an object");
    s.hasProps = true;
    for (int i = 0; i < props->nkeys; i++) {
      if (strcmp(props->keys[i], "damageType") == 0) {
        s.propsHasDamageTypeKey = true;
        s.propsDamageType = cv_damage_value(props->vals[i]);
      } else {
        fail("surface props carries a key besides damageType (extend the model)");
      }
    }
  }
  return s;
}

static char cv_char_str(const CanonVal *v) {
  if (v->type != CV_STR || strlen(v->str) != 1) fail("expected 1-char string");
  return v->str[0];
}

static LabelledSurface cv_labelled_surface(const CanonVal *v) {
  if (v->type != CV_ARR || v->count != 2) fail("expected [surface,[type,index]]");
  LabelledSurface ls;
  ls.surface = cv_surface(v->items[0]);
  const CanonVal *lab = v->items[1];
  if (lab->type != CV_ARR || lab->count != 2) fail("expected [type,index] label");
  ls.type = cv_char_str(lab->items[0]);
  ls.index = cv_number(lab->items[1]);
  return ls;
}

static SurfaceList cv_surface_list(const CanonVal *v) {
  if (v->type != CV_ARR) fail("expected surface list");
  if (v->count > ML_MAX_SURFACES) fail("surface list exceeds ML_MAX_SURFACES");
  SurfaceList out;
  out.count = v->count;
  for (int i = 0; i < v->count; i++) out.items[i] = cv_surface(v->items[i]);
  return out;
}

static Stage cv_stage(const CanonVal *v) {
  // canon object keys are sorted: ceiling, ground, platform, wallL, wallR
  if (v->type != CV_OBJ || v->nkeys != 5 ||
      strcmp(v->keys[0], "ceiling") != 0 || strcmp(v->keys[1], "ground") != 0 ||
      strcmp(v->keys[2], "platform") != 0 || strcmp(v->keys[3], "wallL") != 0 ||
      strcmp(v->keys[4], "wallR") != 0) {
    fail("expected stage projection {ceiling,ground,platform,wallL,wallR}");
  }
  Stage st;
  st.ceiling = cv_surface_list(v->vals[0]);
  st.ground = cv_surface_list(v->vals[1]);
  st.platform = cv_surface_list(v->vals[2]);
  st.wallL = cv_surface_list(v->vals[3]);
  st.wallR = cv_surface_list(v->vals[4]);
  return st;
}

static SquashDatum cv_squash_datum(const CanonVal *v) {
  // sorted keys: factor, location
  if (v->type != CV_OBJ || v->nkeys != 2 ||
      strcmp(v->keys[0], "factor") != 0 || strcmp(v->keys[1], "location") != 0) {
    fail("expected SquashDatum {factor,location}");
  }
  SquashDatum s;
  s.factor = cv_number(v->vals[0]);
  if (v->vals[1]->type == CV_NULL) {
    s.locationIsNull = true;
    s.location = 0;
  } else {
    s.locationIsNull = false;
    s.location = cv_number(v->vals[1]);
  }
  return s;
}

static bool cv_bool(const CanonVal *v) {
  if (v->type != CV_BOOL) fail("expected boolean");
  return v->b;
}

static PlayerStatusInfo cv_status(const CanonVal *v) {
  // sorted keys: grounded, ignoringPlatforms, immune
  if (v->type != CV_OBJ || v->nkeys != 3 ||
      strcmp(v->keys[0], "grounded") != 0 ||
      strcmp(v->keys[1], "ignoringPlatforms") != 0 ||
      strcmp(v->keys[2], "immune") != 0) {
    fail("expected PlayerStatusInfo {grounded,ignoringPlatforms,immune}");
  }
  PlayerStatusInfo p;
  p.grounded = cv_bool(v->vals[0]);
  p.ignoringPlatforms = cv_bool(v->vals[1]);
  p.immune = cv_bool(v->vals[2]);
  return p;
}

// --- serialization (C results -> canon) ------------------------------------

static void ser_vec2d(CanonBuf *b, Vec2D v) {
  cb_puts(b, "{\"x\":");
  cb_num(b, v.x);
  cb_puts(b, ",\"y\":");
  cb_num(b, v.y);
  cb_putc(b, '}');
}

static void ser_line(CanonBuf *b, Line2 l) {
  cb_putc(b, '[');
  ser_vec2d(b, l.a);
  cb_putc(b, ',');
  ser_vec2d(b, l.b);
  cb_putc(b, ']');
}

static void ser_ecb(CanonBuf *b, ECB e) {
  cb_putc(b, '[');
  for (int i = 0; i < 4; i++) {
    if (i) cb_putc(b, ',');
    ser_vec2d(b, e.pt[i]);
  }
  cb_putc(b, ']');
}

static void ser_damage_value(CanonBuf *b, DamageType d) {
  switch (d.tag) {
    case DT_NULL: cb_puts(b, "null"); break;
    case DT_UNDEF: cb_puts(b, "undef"); break;
    case DT_STR: cb_qstr(b, d.str); break;
    default: fail("serializing DT_ABSENT damage value");
  }
}

static void ser_surface_echo(CanonBuf *b, const Surface *s) {
  cb_putc(b, '[');
  ser_vec2d(b, s->p0);
  cb_putc(b, ',');
  ser_vec2d(b, s->p1);
  if (s->hasProps) {
    cb_puts(b, ",{");
    if (s->propsHasDamageTypeKey) {
      cb_puts(b, "\"damageType\":");
      ser_damage_value(b, s->propsDamageType);
    }
    cb_putc(b, '}');
  }
  cb_putc(b, ']');
}

static void ser_maybe_num(CanonBuf *b, MaybeNum m) {
  if (!m.present) cb_puts(b, "null");
  else cb_num(b, m.v);
}

static void ser_collision_datum(CanonBuf *b, const CollisionDatum *cd) {
  if (cd->kind == CD_NULL) {
    cb_puts(b, "null");
  } else if (cd->kind == CD_SURFACE) {
    // sorted keys: index, kind, pt, surface, sweep, type
    cb_puts(b, "{\"index\":");
    cb_num(b, cd->ps.index);
    cb_puts(b, ",\"kind\":\"surface\",\"pt\":");
    cb_num(b, cd->ps.pt);
    cb_puts(b, ",\"surface\":");
    ser_surface_echo(b, &cd->ps.surface);
    cb_puts(b, ",\"sweep\":");
    cb_num(b, cd->ps.sweep);
    cb_puts(b, ",\"type\":");
    cb_str1(b, cd->ps.type);
    cb_putc(b, '}');
  } else {
    // sorted keys: angular, corner, damageType, kind, sweep
    cb_puts(b, "{\"angular\":");
    cb_num(b, cd->es.angular);
    cb_puts(b, ",\"corner\":");
    ser_vec2d(b, cd->es.corner);
    cb_puts(b, ",\"damageType\":");
    ser_damage_value(b, cd->es.damageType);
    cb_puts(b, ",\"kind\":\"corner\",\"sweep\":");
    cb_num(b, cd->es.sweep);
    cb_putc(b, '}');
  }
}

static void ser_simple_touching(CanonBuf *b, const SimpleTouchingDatum *t) {
  if (!t->present) {
    cb_puts(b, "null");
    return;
  }
  if (t->kind == CD_SURFACE) {
    // sorted: [damageType,] index, kind, pt, type
    cb_putc(b, '{');
    if (t->damageType.tag != DT_ABSENT) {
      cb_puts(b, "\"damageType\":");
      ser_damage_value(b, t->damageType);
      cb_putc(b, ',');
    }
    cb_puts(b, "\"index\":");
    cb_num(b, t->index);
    cb_puts(b, ",\"kind\":\"surface\",\"pt\":");
    cb_num(b, t->pt);
    cb_puts(b, ",\"type\":");
    cb_str1(b, t->type);
    cb_putc(b, '}');
  } else {
    // sorted: angular, [damageType,] kind
    cb_puts(b, "{\"angular\":");
    cb_num(b, t->angular);
    if (t->damageType.tag != DT_ABSENT) {
      cb_puts(b, ",\"damageType\":");
      ser_damage_value(b, t->damageType);
    }
    cb_puts(b, ",\"kind\":\"corner\"}");
  }
}

static void ser_squash_datum(CanonBuf *b, SquashDatum s) {
  cb_puts(b, "{\"factor\":");
  cb_num(b, s.factor);
  cb_puts(b, ",\"location\":");
  if (s.locationIsNull) cb_puts(b, "null");
  else cb_num(b, s.location);
  cb_putc(b, '}');
}

static void ser_collision_routine_result(CanonBuf *b,
                                         const CollisionRoutineResult *r) {
  // sorted keys: ecb, position, squashDatum, touching
  cb_puts(b, "{\"ecb\":");
  ser_ecb(b, r->ecb);
  cb_puts(b, ",\"position\":");
  ser_vec2d(b, r->position);
  cb_puts(b, ",\"squashDatum\":");
  ser_squash_datum(b, r->squashDatum);
  cb_puts(b, ",\"touching\":");
  ser_simple_touching(b, &r->touching);
  cb_putc(b, '}');
}

// --- dispatch ---------------------------------------------------------------

static void expect_argc(const CanonVal *args, int n) {
  if (args->type != CV_ARR || args->count != n) fail("bad argument count");
}

// Returns the canon serialization of the C call's return value.
static void dispatch(const char *fn, const CanonVal *args, CanonBuf *out) {
  if (strcmp(fn, "runCollisionRoutine") == 0) {
    expect_argc(args, 6);
    const ECB ecb1 = cv_ecb(args->items[0]);
    const ECB ecbp = cv_ecb(args->items[1]);
    const Vec2D position = cv_vec2d(args->items[2]);
    const SquashDatum sq = cv_squash_datum(args->items[3]);
    const PlayerStatusInfo psi = cv_status(args->items[4]);
    const Stage stage = cv_stage(args->items[5]);
    const CollisionRoutineResult r =
        runCollisionRoutine(ecb1, ecbp, position, sq, psi, &stage);
    ser_collision_routine_result(out, &r);
  } else if (strcmp(fn, "findCollision") == 0) {
    expect_argc(args, 3);
    const ECB ecb1 = cv_ecb(args->items[0]);
    const ECB ecbp = cv_ecb(args->items[1]);
    const LabelledSurface ls = cv_labelled_surface(args->items[2]);
    const CollisionDatum r = findCollision(ecb1, ecbp, ls);
    ser_collision_datum(out, &r);
  } else if (strcmp(fn, "moveAlongGround") == 0) {
    expect_argc(args, 5);
    const Vec2D pos = cv_vec2d(args->items[0]);
    const Vec2D posNext = cv_vec2d(args->items[1]);
    const double ecbHeight = cv_number(args->items[2]);
    const Surface ground = cv_surface(args->items[3]);
    const SurfaceList ceilings = cv_surface_list(args->items[4]);
    ser_maybe_num(out, moveAlongGround(pos, posNext, ecbHeight, ground, &ceilings));
  } else if (strcmp(fn, "groundedECBSquashFactor") == 0) {
    expect_argc(args, 3);
    const Vec2D top = cv_vec2d(args->items[0]);
    const Vec2D bottom = cv_vec2d(args->items[1]);
    const SurfaceList ceilings = cv_surface_list(args->items[2]);
    ser_maybe_num(out, groundedECBSquashFactor(top, bottom, &ceilings));
  } else if (strcmp(fn, "coordinateInterceptParameter") == 0) {
    expect_argc(args, 2);
    cb_num(out, coordinateInterceptParameter(cv_line(args->items[0]),
                                             cv_line(args->items[1])));
  } else if (strcmp(fn, "coordinateIntercept") == 0) {
    expect_argc(args, 2);
    ser_vec2d(out, coordinateIntercept(cv_line(args->items[0]),
                                       cv_line(args->items[1])));
  } else if (strcmp(fn, "outwardsWallNormal") == 0) {
    expect_argc(args, 3);
    ser_vec2d(out, outwardsWallNormal(cv_vec2d(args->items[0]),
                                      cv_vec2d(args->items[1]),
                                      cv_char_str(args->items[2])));
  } else if (strcmp(fn, "getSameAndOther") == 0) {
    expect_argc(args, 1);
    const SameOther so = getSameAndOther(cv_number(args->items[0]));
    cb_putc(out, '[');
    cb_num(out, so.same);
    cb_putc(out, ',');
    cb_num(out, so.other);
    cb_putc(out, ']');
  } else if (strcmp(fn, "hLineThrough") == 0) {
    expect_argc(args, 1);
    ser_line(out, hLineThrough(cv_vec2d(args->items[0])));
  } else if (strcmp(fn, "hLineAt") == 0) {
    expect_argc(args, 1);
    ser_line(out, hLineAt(cv_number(args->items[0])));
  } else if (strcmp(fn, "vLineThrough") == 0) {
    expect_argc(args, 1);
    ser_line(out, vLineThrough(cv_vec2d(args->items[0])));
  } else if (strcmp(fn, "vLineAt") == 0) {
    expect_argc(args, 1);
    ser_line(out, vLineAt(cv_number(args->items[0])));
  } else if (strcmp(fn, "lineThrough") == 0) {
    expect_argc(args, 2);
    ser_line(out, lineThrough(cv_vec2d(args->items[0]),
                              (XOrY)cv_char_str(args->items[1])));
  } else {
    fail("unknown boundary function");
  }
}

// --- main --------------------------------------------------------------------

int main(int argc, char **argv) {
  const char *path = NULL;
  bool strict = false, stop_first = false;
  int max_print = 5;
  const char *only_fn = NULL;
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--strict") == 0) strict = true;
    else if (strcmp(argv[i], "--stop-first") == 0) stop_first = true;
    else if (strcmp(argv[i], "--max-print") == 0 && i + 1 < argc) max_print = atoi(argv[++i]);
    else if (strcmp(argv[i], "--only-fn") == 0 && i + 1 < argc) only_fn = argv[++i];
    else if (!path) path = argv[i];
    else { fprintf(stderr, "unknown arg: %s\n", argv[i]); return 1; }
  }
  if (!path) {
    fprintf(stderr, "usage: replay_envcoll <capture.jsonl> [--strict] "
                    "[--max-print N] [--only-fn NAME] [--stop-first]\n");
    return 1;
  }
  FILE *f = fopen(path, "r");
  if (!f) { fprintf(stderr, "cannot open %s\n", path); return 1; }
  g_file = path;

  char *line = NULL;
  size_t linecap = 0;
  long replayed = 0, divergences = 0, printed = 0;
  long first_div_line = -1;

  // per-function divergence counts
  enum { NFN = 13 };
  static const char *fn_names[NFN] = {
    "hLineThrough", "hLineAt", "vLineThrough", "vLineAt", "lineThrough",
    "outwardsWallNormal", "coordinateInterceptParameter",
    "coordinateIntercept", "findCollision", "getSameAndOther",
    "moveAlongGround", "groundedECBSquashFactor", "runCollisionRoutine",
  };
  long fn_total[NFN] = {0}, fn_div[NFN] = {0};

  CanonBuf out;
  cb_init(&out);

  ssize_t n;
  while ((n = getline(&line, &linecap, f)) > 0) {
    g_lineno++;
    if (line[n - 1] == '\n') line[n - 1] = 0;
    if (line[0] == 0) continue;

    // <frame> \t <fn> \t <args> \t <ret>
    char *tab1 = strchr(line, '\t');
    char *tab2 = tab1 ? strchr(tab1 + 1, '\t') : NULL;
    char *tab3 = tab2 ? strchr(tab2 + 1, '\t') : NULL;
    if (!tab1 || !tab2 || !tab3) fail("malformed record (need 4 tab fields)");
    *tab1 = 0; *tab2 = 0; *tab3 = 0;
    const char *frame = line;
    const char *fn = tab1 + 1;
    const char *args_s = tab2 + 1;
    const char *ret_s = tab3 + 1;

    if (only_fn && strcmp(fn, only_fn) != 0) continue;

    int fni = -1;
    for (int i = 0; i < NFN; i++) {
      if (strcmp(fn, fn_names[i]) == 0) { fni = i; break; }
    }
    if (fni == -1) fail("unknown function name in record");

    canon_arena_reset();
    const char *err = NULL;
    const CanonVal *args = canon_parse(args_s, &err);
    if (!args) {
      fprintf(stderr, "PARSE FAIL %s:%ld: %s\n", path, g_lineno, err);
      return 3;
    }

    out.len = 0;
    out.buf[0] = 0;
    dispatch(fn, args, &out);
    replayed++;
    fn_total[fni]++;

    if (strcmp(out.buf, ret_s) != 0) {
      divergences++;
      fn_div[fni]++;
      if (first_div_line == -1) first_div_line = g_lineno;
      if (printed < max_print) {
        printed++;
        // locate first differing byte
        size_t d = 0;
        while (out.buf[d] && ret_s[d] && out.buf[d] == ret_s[d]) d++;
        fprintf(stderr,
                "DIVERGENCE line %ld frame %s fn %s (first diff at byte %zu)\n"
                "  expected: %.300s\n"
                "  got:      %.300s\n"
                "  context:  ...%.80s VS ...%.80s\n",
                g_lineno, frame, fn, d, ret_s, out.buf,
                ret_s + (d > 40 ? d - 40 : 0), out.buf + (d > 40 ? d - 40 : 0));
      }
      if (stop_first) break;
    }
  }
  free(line);
  fclose(f);

  fprintf(stderr, "\nper-function: (replayed/diverged)\n");
  for (int i = 0; i < NFN; i++) {
    if (fn_total[i] > 0) {
      fprintf(stderr, "  %-28s %ld/%ld\n", fn_names[i], fn_total[i], fn_div[i]);
    }
  }
  printf("REPLAY RAN %ld records, %ld divergences", replayed, divergences);
  if (first_div_line != -1) printf(" (first at line %ld)", first_div_line);
  printf("\n");
  cb_free(&out);
  if (strict && divergences > 0) return 2;
  return 0;
}
