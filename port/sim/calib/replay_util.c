// replay_util.c — M2 task 1 replay driver: feeds recorded util-module
// boundary calls (port/sim/calib/build/<id>.util.jsonl, FORMAT.md spec
// "util") to the C translations of the util/math substrate and compares
// canon-v1 serializations byte-for-byte. A single differing bit anywhere
// in a return value is a divergence.
//
// Usage: replay_util <capture.jsonl> [--strict] [--max-print N]
//                    [--only-fn NAME] [--stop-first]
//
// Marshalling is STRICT (prevention rule 7): any argument shape outside
// the captured domain aborts with exit 3 — never guess.
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../environmental_collision.h" // Line2 helpers' consumer types + cIP
#include "../util/box2d.h"
#include "../util/detect_intersections.h"
#include "../util/lin_alg.h"
#include "../util/line_angle.h"
#include "../util/segment2d.h"
#include "../util/solve_quadratic_equation.h"
#include "../util/to_list.h"
#include "../util/zip_labels.h"
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
  // ToNumber(undefined) -> canonical NaN (prevention rule 2) — valid ONLY
  // for arithmetic-context arguments; accessor-context marshalling uses
  // cv_js_num (rule 8).
  if (v->type == CV_UNDEF) return js_nan();
  fail("expected number");
  return 0;
}

// strict: undefined is OUT of this argument's captured domain
static double cv_number_strict(const CanonVal *v) {
  if (v->type != CV_NUM) fail("expected number (undef out of domain here)");
  return v->num;
}

// accessor-context number: undefined stays undefined (rule 8)
static JsNum cv_js_num(const CanonVal *v) {
  if (v->type == CV_NUM) return js_num(v->num);
  if (v->type == CV_UNDEF) return js_undef();
  fail("expected number|undefined");
  return js_num(0);
}

static Vec2D cv_vec2d(const CanonVal *v) {
  if (v->type != CV_OBJ || v->nkeys != 2 ||
      strcmp(v->keys[0], "x") != 0 || strcmp(v->keys[1], "y") != 0) {
    fail("expected Vec2D {x,y}");
  }
  return vec2d(cv_number(v->vals[0]), cv_number(v->vals[1]));
}

// accessor-context Vec2D: components may be undefined at rest (rule 8)
static JsVec2D cv_js_vec2d(const CanonVal *v) {
  if (v->type != CV_OBJ || v->nkeys != 2 ||
      strcmp(v->keys[0], "x") != 0 || strcmp(v->keys[1], "y") != 0) {
    fail("expected Vec2D {x,y}");
  }
  JsVec2D out;
  out.x = cv_js_num(v->vals[0]);
  out.y = cv_js_num(v->vals[1]);
  return out;
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

static SurfaceList cv_surface_list(const CanonVal *v) {
  if (v->type != CV_ARR) fail("expected surface list");
  if (v->count > ML_MAX_SURFACES) fail("surface list exceeds ML_MAX_SURFACES");
  SurfaceList out;
  out.count = v->count;
  for (int i = 0; i < v->count; i++) out.items[i] = cv_surface(v->items[i]);
  return out;
}

static char cv_char_str(const CanonVal *v) {
  if (v->type != CV_STR || strlen(v->str) != 1) fail("expected 1-char string");
  return v->str[0];
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

// [[x1,x2],[y1,y2]]
static Mat2 cv_mat2(const CanonVal *v) {
  if (v->type != CV_ARR || v->count != 2 ||
      v->items[0]->type != CV_ARR || v->items[0]->count != 2 ||
      v->items[1]->type != CV_ARR || v->items[1]->count != 2) {
    fail("expected 2x2 matrix [[n,n],[n,n]]");
  }
  Mat2 m;
  m.x1 = cv_number(v->items[0]->items[0]);
  m.x2 = cv_number(v->items[0]->items[1]);
  m.y1 = cv_number(v->items[1]->items[0]);
  m.y2 = cv_number(v->items[1]->items[1]);
  return m;
}

static NumPair cv_numpair(const CanonVal *v) {
  if (v->type != CV_ARR || v->count != 2) fail("expected [number,number]");
  NumPair p;
  p.a = cv_number(v->items[0]);
  p.b = cv_number(v->items[1]);
  return p;
}

// Segment2D instance canon: sorted own keys
// {project:fn, segLength:fn, vecx, vecy, x, y}
static Segment2D cv_segment2d(const CanonVal *v) {
  if (v->type != CV_OBJ || v->nkeys != 6 ||
      strcmp(v->keys[0], "project") != 0 || v->vals[0]->type != CV_FN ||
      strcmp(v->keys[1], "segLength") != 0 || v->vals[1]->type != CV_FN ||
      strcmp(v->keys[2], "vecx") != 0 || strcmp(v->keys[3], "vecy") != 0 ||
      strcmp(v->keys[4], "x") != 0 || strcmp(v->keys[5], "y") != 0) {
    fail("expected Segment2D instance {project:fn,segLength:fn,vecx,vecy,x,y}");
  }
  return segment2d(cv_number(v->vals[4]), cv_number(v->vals[5]),
                   cv_number(v->vals[2]), cv_number(v->vals[3]));
}

#define ML_MAX_MAYBE 256
typedef struct { MaybeNum items[ML_MAX_MAYBE]; int count; } MaybeNumList;

static MaybeNumList cv_maybe_num_list(const CanonVal *v) {
  if (v->type != CV_ARR) fail("expected Array<number|null>");
  if (v->count > ML_MAX_MAYBE) fail("number list exceeds ML_MAX_MAYBE");
  MaybeNumList out;
  out.count = v->count;
  for (int i = 0; i < v->count; i++) {
    if (v->items[i]->type == CV_NULL) out.items[i] = maybe_null();
    else out.items[i] = maybe_num(cv_number(v->items[i]));
  }
  return out;
}

static MaybeNum cv_maybe_num(const CanonVal *v) {
  if (v->type == CV_NULL) return maybe_null();
  return maybe_num(cv_number(v));
}

static LineList cv_line_list(const CanonVal *v) {
  if (v->type != CV_ARR) fail("expected line list");
  if (v->count > ML_MAX_LINES) fail("line list exceeds ML_MAX_LINES");
  LineList out;
  out.count = v->count;
  for (int i = 0; i < v->count; i++) out.items[i] = cv_line(v->items[i]);
  return out;
}

static PolygonPts cv_polygon(const CanonVal *v) {
  if (v->type != CV_ARR || v->count < 1) fail("expected non-empty polygon");
  if (v->count > ML_MAX_LINES) fail("polygon exceeds ML_MAX_LINES");
  PolygonPts out;
  out.count = v->count;
  for (int i = 0; i < v->count; i++) out.items[i] = cv_vec2d(v->items[i]);
  return out;
}

// --- serialization (C results -> canon) ------------------------------------

static void ser_vec2d(CanonBuf *b, Vec2D v) {
  cb_puts(b, "{\"x\":");
  cb_num(b, v.x);
  cb_puts(b, ",\"y\":");
  cb_num(b, v.y);
  cb_putc(b, '}');
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

// generic echo writer: re-serialize a parsed canon tree byte-identically
// (numbers survive bit-exactly — parse and emit are both bit-pattern hex)
static void canon_write(CanonBuf *b, const CanonVal *v) {
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
        canon_write(b, v->items[i]);
      }
      cb_putc(b, ']');
      break;
    case CV_OBJ:
      cb_putc(b, '{');
      for (int i = 0; i < v->nkeys; i++) {
        if (i) cb_putc(b, ',');
        cb_qstr(b, v->keys[i]);
        cb_putc(b, ':');
        canon_write(b, v->vals[i]);
      }
      cb_putc(b, '}');
      break;
    default: fail("canon_write: bad type");
  }
}

// --- dispatch ---------------------------------------------------------------

static void expect_argc(const CanonVal *args, int n) {
  if (args->type != CV_ARR || args->count != n) fail("bad argument count");
}

static void dispatch(const char *fn, const CanonVal *args, CanonBuf *out) {
  // --- Vec2D module ---------------------------------------------------------
  if (strcmp(fn, "getXOrYCoord") == 0) {
    expect_argc(args, 2);
    // accessor: undefined components are echoed verbatim (rule 8)
    const JsNum r = getXOrYCoordJs(cv_js_vec2d(args->items[0]),
                                   (XOrY)cv_char_str(args->items[1]));
    if (r.isUndef) cb_puts(out, "undef");
    else cb_num(out, r.v);
  } else if (strcmp(fn, "putXOrYCoord") == 0) {
    expect_argc(args, 2);
    // constructor echo: an undefined coord would echo into the result
    // object — out of the captured domain (0 records), strict marshal
    ser_vec2d(out, putXOrYCoord(cv_number_strict(args->items[0]),
                                (XOrY)cv_char_str(args->items[1])));
  } else if (strcmp(fn, "flipXOrY") == 0) {
    expect_argc(args, 1);
    cb_str1(out, flipXOrY((XOrY)cv_char_str(args->items[0])));
  } else if (strcmp(fn, "Vec2D#dot") == 0) {
    expect_argc(args, 2);
    cb_num(out, vec2d_dot(cv_vec2d(args->items[0]), cv_vec2d(args->items[1])));
  }
  // --- linAlg -----------------------------------------------------------------
  else if (strcmp(fn, "dotProd") == 0) {
    expect_argc(args, 2);
    cb_num(out, dotProd(cv_vec2d(args->items[0]), cv_vec2d(args->items[1])));
  } else if (strcmp(fn, "scalarProd") == 0) {
    expect_argc(args, 2);
    ser_vec2d(out, scalarProd(cv_number(args->items[0]), cv_vec2d(args->items[1])));
  } else if (strcmp(fn, "norm") == 0) {
    expect_argc(args, 1);
    cb_num(out, norm(cv_vec2d(args->items[0])));
  } else if (strcmp(fn, "add") == 0) {
    expect_argc(args, 2);
    ser_vec2d(out, ml_add(cv_vec2d(args->items[0]), cv_vec2d(args->items[1])));
  } else if (strcmp(fn, "subtract") == 0) {
    expect_argc(args, 2);
    ser_vec2d(out, ml_subtract(cv_vec2d(args->items[0]), cv_vec2d(args->items[1])));
  } else if (strcmp(fn, "euclideanDist") == 0) {
    expect_argc(args, 2);
    cb_num(out, euclideanDist(cv_vec2d(args->items[0]), cv_vec2d(args->items[1])));
  } else if (strcmp(fn, "manhattanDist") == 0) {
    expect_argc(args, 2);
    cb_num(out, manhattanDist(cv_vec2d(args->items[0]), cv_vec2d(args->items[1])));
  } else if (strcmp(fn, "orthogonalProjection") == 0) {
    expect_argc(args, 2);
    ser_vec2d(out, orthogonalProjection(cv_vec2d(args->items[0]),
                                        cv_line(args->items[1])));
  } else if (strcmp(fn, "inverseMatrix") == 0) {
    expect_argc(args, 1);
    const MaybeMat2 r = inverseMatrix(cv_mat2(args->items[0]));
    if (!r.present) {
      cb_puts(out, "null");
    } else {
      cb_putc(out, '[');
      cb_putc(out, '[');
      cb_num(out, r.m.x1);
      cb_putc(out, ',');
      cb_num(out, r.m.x2);
      cb_puts(out, "],[");
      cb_num(out, r.m.y1);
      cb_putc(out, ',');
      cb_num(out, r.m.y2);
      cb_puts(out, "]]");
    }
  } else if (strcmp(fn, "multMatVect") == 0) {
    expect_argc(args, 2);
    const NumPair r = multMatVect(cv_mat2(args->items[0]),
                                  cv_numpair(args->items[1]));
    cb_putc(out, '[');
    cb_num(out, r.a);
    cb_putc(out, ',');
    cb_num(out, r.b);
    cb_putc(out, ']');
  } else if (strcmp(fn, "reflect") == 0) {
    expect_argc(args, 2);
    ser_vec2d(out, reflect(cv_vec2d(args->items[0]), cv_vec2d(args->items[1])));
  }
  // --- small pure modules -------------------------------------------------------
  else if (strcmp(fn, "solveQuadraticEquation") == 0) {
    if (args->type != CV_ARR || (args->count != 3 && args->count != 4)) {
      fail("solveQuadraticEquation: expected 3 or 4 args");
    }
    const double sign = args->count == 4 ? cv_number(args->items[3]) : 1.0;
    ser_maybe_num(out, solveQuadraticEquation(cv_number(args->items[0]),
                                              cv_number(args->items[1]),
                                              cv_number(args->items[2]), sign));
  } else if (strcmp(fn, "lineAngle") == 0) {
    expect_argc(args, 1);
    cb_num(out, lineAngle(cv_line(args->items[0])));
  } else if (strcmp(fn, "findSmallestWithin") == 0) {
    if (args->type != CV_ARR || (args->count != 3 && args->count != 4)) {
      fail("findSmallestWithin: expected 3 or 4 args");
    }
    const MaybeNumList list = cv_maybe_num_list(args->items[0]);
    const MaybeNum seed = args->count == 4 ? cv_maybe_num(args->items[3])
                                           : maybe_null();
    ser_maybe_num(out, findSmallestWithinFrom(list.items, list.count,
                                              cv_number(args->items[1]),
                                              cv_number(args->items[2]), seed));
  } else if (strcmp(fn, "pickSmallestSweep") == 0) {
    expect_argc(args, 1);
    const CanonVal *list = args->items[0];
    if (list->type != CV_ARR) fail("pickSmallestSweep: expected array");
    if (list->count > ML_MAX_MAYBE) fail("pickSmallestSweep: list too long");
    bool present[ML_MAX_MAYBE];
    double sweep[ML_MAX_MAYBE];
    for (int i = 0; i < list->count; i++) {
      const CanonVal *it = list->items[i];
      if (it->type == CV_NULL) {
        present[i] = false;
        sweep[i] = 0;
      } else if (it->type == CV_OBJ) {
        present[i] = true;
        bool found = false;
        for (int k = 0; k < it->nkeys; k++) {
          if (strcmp(it->keys[k], "sweep") == 0) {
            sweep[i] = cv_number(it->vals[k]);
            found = true;
            break;
          }
        }
        if (!found) fail("pickSmallestSweep: item lacks sweep key");
      } else {
        fail("pickSmallestSweep: item not null/object");
      }
    }
    const int idx = pickSmallestSweep(present, sweep, list->count);
    if (idx == -1) cb_puts(out, "null");
    else canon_write(out, list->items[idx]); // echo the picked object
  } else if (strcmp(fn, "extremePoint") == 0) {
    expect_argc(args, 2);
    const Surface s = cv_surface(args->items[0]);
    ser_vec2d(out, extremePoint(surface_geom(&s), cv_char_str(args->items[1])));
  }
  // --- ecbTransform ---------------------------------------------------------------
  else if (strcmp(fn, "moveECB") == 0) {
    expect_argc(args, 2);
    ser_ecb(out, moveECB(cv_ecb(args->items[0]), cv_vec2d(args->items[1])));
  } else if (strcmp(fn, "squashECBAt") == 0) {
    expect_argc(args, 2);
    ser_ecb(out, squashECBAt(cv_ecb(args->items[0]),
                             cv_squash_datum(args->items[1])));
  } else if (strcmp(fn, "ecbFocusFromAngularParameter") == 0) {
    expect_argc(args, 2);
    const ECB e = cv_ecb(args->items[0]);
    if (args->items[1]->type == CV_NULL) {
      ser_vec2d(out, ecbFocusFromAngularParameter(e, true, 0));
    } else {
      ser_vec2d(out, ecbFocusFromAngularParameter(e, false,
                                                  cv_number(args->items[1])));
    }
  } else if (strcmp(fn, "interpolateECB") == 0) {
    expect_argc(args, 3);
    ser_ecb(out, interpolateECB(cv_ecb(args->items[0]), cv_ecb(args->items[1]),
                                cv_number(args->items[2])));
  } else if (strcmp(fn, "makeECB") == 0) {
    expect_argc(args, 3);
    ser_ecb(out, makeECB(cv_vec2d(args->items[0]), cv_number(args->items[1]),
                         cv_number(args->items[2])));
  }
  // --- zipLabels / toList -----------------------------------------------------------
  else if (strcmp(fn, "zipLabels") == 0) {
    expect_argc(args, 2);
    const SurfaceList list = cv_surface_list(args->items[0]);
    const char label = cv_char_str(args->items[1]);
    LabelledSurfaceList out_list;
    out_list.count = 0;
    zipLabelsInto(&out_list, &list, label);
    cb_putc(out, '[');
    for (int i = 0; i < out_list.count; i++) {
      if (i) cb_putc(out, ',');
      cb_putc(out, '[');
      ser_surface_echo(out, &out_list.items[i].surface);
      cb_puts(out, ",[");
      cb_str1(out, out_list.items[i].type);
      cb_putc(out, ',');
      cb_num(out, out_list.items[i].index);
      cb_puts(out, "]]");
    }
    cb_putc(out, ']');
  } else if (strcmp(fn, "toList") == 0) {
    expect_argc(args, 1);
    const SurfaceList list = cv_surface_list(args->items[0]);
    const SurfaceList copied = toList_surfaces(&list);
    cb_putc(out, '[');
    for (int i = 0; i < copied.count; i++) {
      if (i) cb_putc(out, ',');
      ser_surface_echo(out, &copied.items[i]);
    }
    cb_putc(out, ']');
  }
  // --- detectIntersections ---------------------------------------------------------
  else if (strcmp(fn, "intersectsAny") == 0) {
    expect_argc(args, 2);
    const LineList lines = cv_line_list(args->items[1]);
    cb_puts(out, intersectsAny(cv_line(args->items[0]), &lines) ? "T" : "F");
  } else if (strcmp(fn, "distanceToLine") == 0) {
    expect_argc(args, 2);
    cb_num(out, distanceToLine(cv_vec2d(args->items[0]), cv_line(args->items[1])));
  } else if (strcmp(fn, "distanceToPolygon") == 0) {
    expect_argc(args, 2);
    const PolygonPts poly = cv_polygon(args->items[1]);
    cb_num(out, distanceToPolygon(cv_vec2d(args->items[0]), &poly));
  } else if (strcmp(fn, "lineDistanceToLines") == 0) {
    expect_argc(args, 2);
    const LineList lines = cv_line_list(args->items[1]);
    cb_num(out, lineDistanceToLines(cv_line(args->items[0]), &lines));
  }
  // --- Segment2D ---------------------------------------------------------------------
  else if (strcmp(fn, "Segment2D.new") == 0) {
    expect_argc(args, 4);
    const Segment2D s = segment2d(cv_number(args->items[0]),
                                  cv_number(args->items[1]),
                                  cv_number(args->items[2]),
                                  cv_number(args->items[3]));
    // instance canon: sorted own keys {project:fn,segLength:fn,vecx,vecy,x,y}
    cb_puts(out, "{\"project\":fn,\"segLength\":fn,\"vecx\":");
    cb_num(out, s.vecx);
    cb_puts(out, ",\"vecy\":");
    cb_num(out, s.vecy);
    cb_puts(out, ",\"x\":");
    cb_num(out, s.x);
    cb_puts(out, ",\"y\":");
    cb_num(out, s.y);
    cb_putc(out, '}');
  } else if (strcmp(fn, "Segment2D#segLength") == 0) {
    expect_argc(args, 1);
    cb_num(out, segment2d_segLength(cv_segment2d(args->items[0])));
  } else if (strcmp(fn, "Segment2D#project") == 0) {
    expect_argc(args, 2);
    ser_vec2d(out, segment2d_project(cv_segment2d(args->items[0]),
                                     cv_segment2d(args->items[1])));
  } else {
    fail("unknown boundary function");
  }
}

// --- main --------------------------------------------------------------------

#define NFN 34
static const char *fn_names[NFN] = {
  "getXOrYCoord", "putXOrYCoord", "flipXOrY", "Vec2D#dot",
  "dotProd", "scalarProd", "norm", "add", "subtract", "euclideanDist",
  "manhattanDist", "orthogonalProjection", "inverseMatrix", "multMatVect",
  "reflect",
  "solveQuadraticEquation", "lineAngle", "findSmallestWithin",
  "pickSmallestSweep", "extremePoint",
  "moveECB", "squashECBAt", "ecbFocusFromAngularParameter", "interpolateECB",
  "makeECB",
  "zipLabels", "toList",
  "intersectsAny", "distanceToLine", "distanceToPolygon", "lineDistanceToLines",
  "Segment2D.new", "Segment2D#segLength", "Segment2D#project",
};

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
    fprintf(stderr, "usage: replay_util <capture.jsonl> [--strict] "
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
