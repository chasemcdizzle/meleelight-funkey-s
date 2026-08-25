// stage_code.c <- src/stages/encode.js, structure-parallel (A45 T1).
// Contract, format table, the two carried upstream bugs and deviation D39
// are documented in stage_code.h. Judged differentially against upstream's
// OWN executed encode.js by port/sim/check-stage-code.sh.
//
// PLAN §2: doubles only, no arithmetic reordering, -ffp-contract=off on the
// TU. The only floating-point operation in the whole file is the single
// `(double)hundredths / 100.0` in parse_num — everything else, including
// toFixed's rounding, is exact integer work.

#include "stage_code.h"

#include <stdint.h>
#include <string.h>

#include "ml_fmt.h"

// ---------------------------------------------------------------------------
// toFixed(2) — ECMA-262 §21.1.3.3 with fractionDigits = 2.
// ---------------------------------------------------------------------------
//
// Step 7 asks for the integer n minimising |n/100 - x| over the REAL value
// of x, ties to the larger n; i.e. n = floor(100*x + 1/2) in exact
// arithmetic. Writing that as round(x * 100.0) is wrong — the product is
// itself rounded and can cross the .5 boundary in the wrong direction — and
// for x >= 2^53 the answer has more digits than String(x) is even willing to
// print (V8: (2**60).toFixed(2) === "1152921504606846976.00", while
// String(2**60) === "1152921504606847000"). So it is done on the bits.
//
// With x = M * 2^E exactly (M the 53-bit significand, E the unbiased scale):
//   E >= 0 : n = 100*M*2^E, an exact integer. x < 1e21 and M < 2^53 force
//            E <= 17, so 100*M (< 2^60, fits a uint64) doubled E times in
//            decimal is the whole computation.
//   E <  0 : with s = -E, n = floor((200*M + 2^s) / 2^(s+1)). 200*M < 2^61
//            always, so for s >= 61 the numerator is below 2^(s+1) and n is
//            0; for s <= 60 the numerator is below 2^62 and the whole
//            expression fits a uint64.
int ml_to_fixed2(double x, char *buf) {
  uint64_t bits;
  memcpy(&bits, &x, sizeof bits);
  const uint64_t ieeeMantissa = bits & ((1ull << 52) - 1);
  const uint32_t ieeeExponent = (uint32_t)((bits >> 52) & 0x7ffu);

  // Step 3: NaN answers before the sign is ever looked at.
  if (ieeeExponent == 0x7ffu && ieeeMantissa != 0) {
    memcpy(buf, "NaN", 4);
    return 3;
  }

  char *p = buf;
  double ax = x;
  if (x < 0) {  // step 4 — note -0 is NOT < 0, so (-0).toFixed(2) is "0.00"
    *p++ = '-';
    ax = -x;
  }
  // Step 6: |x| >= 10^21 (and Infinity) falls back to ToString. 1e21 is
  // exactly representable, so the comparison is the spec's comparison.
  if (!(ax < 1e21)) {
    const int n = ml_fmt_dtoa(ax, p);
    return (int)(p - buf) + n;
  }

  bits &= ~(1ull << 63);
  const uint64_t am = bits & ((1ull << 52) - 1);
  const uint32_t ae = (uint32_t)((bits >> 52) & 0x7ffu);
  uint64_t M;
  int E;
  if (ae == 0) {  // subnormal (and zero)
    M = am;
    E = -1074;
  } else {
    M = am | (1ull << 52);
    E = (int)ae - 1075;
  }

  // Decimal digits of n, least significant first. 100*M < 2^60 is 19
  // digits; E <= 17 doublings add at most 6 more.
  char d[40];
  int nd = 0;
  if (E >= 0) {
    uint64_t A = 100ull * M;
    if (A == 0) {
      d[nd++] = '0';
    } else {
      while (A != 0) {
        d[nd++] = (char)('0' + (int)(A % 10));
        A /= 10;
      }
    }
    for (int i = 0; i < E; ++i) {
      int carry = 0;
      for (int j = 0; j < nd; ++j) {
        const int v = (d[j] - '0') * 2 + carry;
        d[j] = (char)('0' + v % 10);
        carry = v / 10;
      }
      if (carry != 0) d[nd++] = (char)('0' + carry);
    }
  } else {
    const int s = -E;
    uint64_t n;
    if (s >= 61) {
      n = 0;  // 200*M < 2^61 <= 2^s, so the quotient is zero
    } else {
      n = (200ull * M + (1ull << s)) >> (s + 1);
    }
    if (n == 0) {
      d[nd++] = '0';
    } else {
      while (n != 0) {
        d[nd++] = (char)('0' + (int)(n % 10));
        n /= 10;
      }
    }
  }

  // Steps 8-10: zero-pad to f+1 digits, then split off the last f.
  while (nd < 3) d[nd++] = '0';
  for (int i = nd - 1; i >= 0; --i) {
    if (i == 1) *p++ = '.';
    *p++ = d[i];
  }
  *p = '\0';
  return (int)(p - buf);
}

// ---------------------------------------------------------------------------
// createStageCode
// ---------------------------------------------------------------------------

typedef struct {
  char *p;
  size_t left;
  bool ovf;
} Sink;

static void sput(Sink *k, const char *s, size_t n) {
  if (k->ovf || n > k->left) {
    k->ovf = true;
    return;
  }
  memcpy(k->p, s, n);
  k->p += n;
  k->left -= n;
}

static void sputc(Sink *k, char c) { sput(k, &c, 1); }

static void sputf2(Sink *k, double x) {
  char b[ML_TO_FIXED2_MAX];
  const int n = ml_to_fixed2(x, b);
  sput(k, b, (size_t)n);
}

// String(x) — encode.js concatenates the ledge index/side and the starting
// faces as bare numbers (`tCode += stage.ledge[i][1]`), not via toFixed.
static void sputnum(Sink *k, double x) {
  char b[ML_FMT_DTOA_MAX];
  const int n = ml_fmt_dtoa(x, b);
  sput(k, b, (size_t)n);
}

static void put_surfaces(Sink *k, const SurfaceList *l) {
  for (int i = 0; i < l->count; ++i) {
    const Surface *s = &l->items[i];
    sputf2(k, s->p0.x);
    sputc(k, ',');
    sputf2(k, s->p0.y);
    sputc(k, ',');
    sputf2(k, s->p1.x);
    sputc(k, ',');
    sputf2(k, s->p1.y);
    sputc(k, ',');
    // UPSTREAM BUG 1, CARRIED VERBATIM (encode.js:39 `if (i !== 5)`).
    // `i` is the index of the SURFACE inside this list; the guard was
    // plainly meant to test `n`, the index of the TYPE, so that the damage
    // digit is skipped for type 5 (background.line). As written, the SIXTH
    // SURFACE OF EVERY TYPE silently loses its damage type — measured this
    // session by executing upstream: seven fire wallR surfaces in,
    // ["fire","fire","fire","fire","fire",null,"fire"] out. Every share
    // code the browser ever produced has this hole in it, so "fixing" it
    // would diverge from all of them (HARD RULE 5). Do not repair it here;
    // it is an owner ruling, not an implementation choice. Note the
    // trailing comma above is emitted either way, which is why the record
    // still splits into five tokens and re-encodes identically.
    if (i != 5) {
      char digit = '0';
      if (s->hasProps && s->propsDamageType.tag == DT_STR) {
        const char *dt = s->propsDamageType.str;
        if (strcmp(dt, "fire") == 0) {
          digit = '1';
        } else if (strcmp(dt, "electric") == 0) {
          digit = '2';
        } else if (strcmp(dt, "slash") == 0) {
          digit = '3';
        } else if (strcmp(dt, "darkness") == 0) {
          digit = '4';
        }
      }
      sputc(k, digit);
    }
    if (i != l->count - 1) sputc(k, '~');
  }
}

static void put_polygons(Sink *k, const MlkPolygonList *l) {
  for (int i = 0; i < l->count; ++i) {
    const MlkPolygon *poly = &l->items[i];
    for (int j = 0; j < poly->count; ++j) {
      sputf2(k, poly->pts[j].x);
      sputc(k, ',');
      sputf2(k, poly->pts[j].y);
      if (j != poly->count - 1) sputc(k, ',');
    }
    if (i != l->count - 1) sputc(k, '~');
  }
}

int mlk_encode(const MlkStage *st, char *buf, size_t cap) {
  if (cap == 0) return -1;
  Sink k;
  k.p = buf;
  k.left = cap - 1;  // reserve the NUL
  k.ovf = false;

  for (int i = 0; i < st->startingPointCount; ++i) {
    sputf2(&k, st->startingPoint[i].x);
    sputc(&k, ',');
    sputf2(&k, st->startingPoint[i].y);
    if (i != st->startingPointCount - 1) sputc(&k, '~');
  }

  sputc(&k, '&');
  if (!st->hasStartingFace) {
    sput(&k, "1,1,1,1", 7);  // encode.js:20-22
  } else {
    for (int i = 0; i < st->startingFaceCount; ++i) {
      sputnum(&k, st->startingFace[i]);
      if (i != st->startingFaceCount - 1) sputc(&k, ',');
    }
  }

  // encode.js:31 — the type order is fixed and load-bearing.
  const SurfaceList *types[6] = {&st->s.ground, &st->s.ceiling, &st->s.wallL,
                                 &st->s.wallR,  &st->s.platform, &st->bgLine};
  for (int n = 0; n < 6; ++n) {
    sputc(&k, '&');
    put_surfaces(&k, types[n]);
  }

  const MlkPolygonList *ptypes[2] = {&st->polygon, &st->bgPolygon};
  for (int n = 0; n < 2; ++n) {
    sputc(&k, '&');
    put_polygons(&k, ptypes[n]);
  }

  sputc(&k, '&');
  for (int i = 0; i < st->ledgeCount; ++i) {
    // encode.js:90 writes stage.ledge[i][0][0] — the FIRST CHARACTER of the
    // type name only, which is exactly MlLedge.list ('g' | 'p').
    sputc(&k, st->ledge[i].list);
    sputc(&k, ',');
    sputnum(&k, st->ledge[i].index);
    sputc(&k, ',');
    sputnum(&k, st->ledge[i].point);
    if (i != st->ledgeCount - 1) sputc(&k, '~');
  }

  sputc(&k, '&');
  for (int i = 0; i < st->targetCount; ++i) {
    sputf2(&k, st->target[i].x);
    sputc(&k, ',');
    sputf2(&k, st->target[i].y);
    if (i != st->targetCount - 1) sputc(&k, '~');
  }

  sputc(&k, '&');
  sputf2(&k, st->blastzone.min.x);
  sputc(&k, ',');
  sputf2(&k, st->blastzone.min.y);
  sputc(&k, ',');
  sputf2(&k, st->blastzone.max.x);
  sputc(&k, ',');
  sputf2(&k, st->blastzone.max.y);

  sputc(&k, '&');
  sputf2(&k, st->scale);

  if (k.ovf) return -1;
  *k.p = '\0';
  return (int)(k.p - buf);
}

// ---------------------------------------------------------------------------
// parseStageCode
// ---------------------------------------------------------------------------

typedef struct {
  const char *s;
  size_t n;
} Slice;

// Upstream's `sep` (encode.js:179-181): an empty (or absent) string yields
// ZERO pieces; anything else yields String.prototype.split, so N delimiters
// yield N+1 pieces and empty pieces are kept.
typedef struct {
  const char *p;
  const char *end;
  char d;
  bool done;
} SepIt;

static void sep_init(SepIt *it, Slice f, char d) {
  it->p = f.s;
  it->end = f.s + f.n;
  it->d = d;
  it->done = (f.n == 0);
}

static bool sep_next(SepIt *it, Slice *out) {
  if (it->done) return false;
  const char *q = (const char *)memchr(it->p, it->d, (size_t)(it->end - it->p));
  if (q != NULL) {
    out->s = it->p;
    out->n = (size_t)(q - it->p);
    it->p = q + 1;
  } else {
    out->s = it->p;
    out->n = (size_t)(it->end - it->p);
    it->done = true;
  }
  return true;
}

static int sep_count(Slice f, char d) {
  SepIt it;
  Slice t;
  int n = 0;
  sep_init(&it, f, d);
  while (sep_next(&it, &t)) ++n;
  return n;
}

// D39's grammar: -?\d+(\.\d{1,2})? — exactly what toFixed(2) can emit, plus
// the shorter forms, and nothing else. The value is assembled as an integer
// number of hundredths and divided once: |h| <= 2^53 makes (double)h exact,
// and IEEE division is correctly rounded, so the result is bit-identical to
// a correct strtod of the same token. No libc parse, no subnormal exposure.
#define MLK_HUNDREDTHS_MAX (1ull << 53)

static bool parse_num(Slice t, double *out) {
  size_t i = 0;
  bool neg = false;
  if (i < t.n && t.s[i] == '-') {
    neg = true;
    ++i;
  }
  const size_t ds = i;
  uint64_t v = 0;
  while (i < t.n && t.s[i] >= '0' && t.s[i] <= '9') {
    if (i - ds >= 16) return false;  // keeps v*100 inside a uint64
    v = v * 10 + (uint64_t)(t.s[i] - '0');
    ++i;
  }
  if (i == ds) return false;  // at least one integer digit
  uint64_t frac = 0;
  int fd = 0;
  if (i < t.n && t.s[i] == '.') {
    ++i;
    while (i < t.n && t.s[i] >= '0' && t.s[i] <= '9') {
      if (fd == 2) return false;  // 3+ fraction digits would need rounding
      frac = frac * 10 + (uint64_t)(t.s[i] - '0');
      ++fd;
      ++i;
    }
    if (fd == 0) return false;  // "3." is not in the alphabet
  }
  if (i != t.n) return false;  // trailing junk
  if (fd == 1) frac *= 10;
  const uint64_t h = v * 100 + frac;
  if (h > MLK_HUNDREDTHS_MAX) return false;
  const double d = (double)h / 100.0;
  *out = neg ? -d : d;  // "-0.00" parses to -0, exactly as parseFloat does
  return true;
}

// JS parseInt over this alphabet: optional sign, then digits, stopping at
// the first non-digit ("3.00" is 3). No digits at all is NaN.
static bool parse_int_js(Slice t, double *out) {
  size_t i = 0;
  bool neg = false;
  if (i < t.n && (t.s[i] == '-' || t.s[i] == '+')) {
    neg = (t.s[i] == '-');
    ++i;
  }
  const size_t ds = i;
  double v = 0;
  while (i < t.n && t.s[i] >= '0' && t.s[i] <= '9') {
    v = v * 10 + (double)(t.s[i] - '0');
    ++i;
  }
  if (i == ds) return false;  // NaN
  *out = neg ? -v : v;
  return true;
}

#define FAIL(msg)          \
  do {                     \
    if (reason) *reason = (msg); \
    return false;          \
  } while (0)

static bool parse_points(Slice field, Vec2D *dst, int cap, int *count,
                         const char **reason, const char *capMsg) {
  SepIt it;
  Slice rec;
  int n = 0;
  sep_init(&it, field, '~');
  while (sep_next(&it, &rec)) {
    if (n >= cap) FAIL(capMsg);
    SepIt ti;
    Slice t;
    double xy[2];
    int k = 0;
    sep_init(&ti, rec, ',');
    while (k < 2 && sep_next(&ti, &t)) {
      if (!parse_num(t, &xy[k])) FAIL("bad number");
      ++k;
    }
    if (k < 2) FAIL("point needs two numbers");
    dst[n].x = xy[0];
    dst[n].y = xy[1];
    ++n;
  }
  *count = n;
  return true;
}

static bool parse_surfaces(Slice field, SurfaceList *l, const char **reason) {
  SepIt it;
  Slice rec;
  l->count = 0;
  sep_init(&it, field, '~');
  while (sep_next(&it, &rec)) {
    if (l->count >= ML_MAX_SURFACES) FAIL("too many surfaces");
    const int nt = sep_count(rec, ',');
    if (nt < 4) FAIL("surface needs four numbers");
    Surface *s = &l->items[l->count];
    memset(s, 0, sizeof *s);
    SepIt ti;
    Slice t;
    double c[4];
    sep_init(&ti, rec, ',');
    for (int k = 0; k < 4; ++k) {
      sep_next(&ti, &t);
      if (!parse_num(t, &c[k])) FAIL("bad number");
    }
    s->p0 = vec2d(c[0], c[1]);
    s->p1 = vec2d(c[2], c[3]);
    // parseSurface (encode.js:118-142): the properties object exists iff a
    // FIFTH token was present, and it always carries a damageType key whose
    // value is null for anything outside "1".."4" — including the EMPTY
    // token that upstream bug 1 leaves behind on surface index 5. Tokens
    // past the fifth are ignored, exactly as the 5-parameter JS signature
    // ignores them.
    if (nt >= 5) {
      sep_next(&ti, &t);
      s->hasProps = true;
      s->propsHasDamageTypeKey = true;
      const char *dt = NULL;
      if (t.n == 1) {
        switch (t.s[0]) {
          case '1': dt = "fire"; break;
          case '2': dt = "electric"; break;
          case '3': dt = "slash"; break;
          case '4': dt = "darkness"; break;
          default: break;
        }
      }
      if (dt != NULL) {
        s->propsDamageType.tag = DT_STR;
        memcpy(s->propsDamageType.str, dt, strlen(dt) + 1);
      } else {
        s->propsDamageType = damage_null();
      }
    }
    ++l->count;
  }
  return true;
}

static bool parse_polygons(Slice field, MlkPolygonList *l,
                           const char **reason) {
  SepIt it;
  Slice rec;
  l->count = 0;
  sep_init(&it, field, '~');
  while (sep_next(&it, &rec)) {
    if (l->count >= MLK_MAX_POLYGONS) FAIL("too many polygons");
    MlkPolygon *poly = &l->items[l->count];
    poly->count = 0;
    const int nt = sep_count(rec, ',');
    // parsePolygon (encode.js:168-177) returns an EMPTY polygon on an odd
    // token count rather than failing. Carried: the record survives as a
    // zero-point polygon and re-encodes as an empty record.
    if (nt % 2 == 0) {
      SepIt ti;
      Slice t;
      sep_init(&ti, rec, ',');
      for (int j = 0; j < nt / 2; ++j) {
        double xy[2];
        for (int k = 0; k < 2; ++k) {
          sep_next(&ti, &t);
          if (!parse_num(t, &xy[k])) FAIL("bad number");
        }
        if (poly->count >= MLK_MAX_POLY_POINTS) FAIL("polygon too large");
        poly->pts[poly->count++] = vec2d(xy[0], xy[1]);
      }
    }
    ++l->count;
  }
  return true;
}

bool mlk_parse(const char *code, MlkStage *out, const char **reason) {
  if (reason) *reason = NULL;
  if (code == NULL) FAIL("no code");
  const size_t len = strlen(code);
  if (len < 14) FAIL("code too short");  // encode.js:208-210

  Slice f[14];
  {
    const char *p = code;
    const char *end = code + len;
    int n = 0;
    for (;;) {
      const char *q = (const char *)memchr(p, '&', (size_t)(end - p));
      const char *stop = (q != NULL) ? q : end;
      if (n < 14) {
        f[n].s = p;
        f[n].n = (size_t)(stop - p);
      }
      ++n;
      if (q == NULL) break;
      p = q + 1;
    }
    // A missing field and an empty field are indistinguishable downstream:
    // sep(undefined) and sep("") both yield [], parseFloat of either is NaN.
    for (int i = n; i < 14; ++i) {
      f[i].s = code;
      f[i].n = 0;
    }
  }

  memset(out, 0, sizeof *out);

  if (!parse_points(f[0], out->startingPoint, MLK_MAX_STARTING_POINTS,
                    &out->startingPointCount, reason, "too many start points"))
    return false;

  // parseStageCode always installs a startingFace array (encode.js:222), so
  // a parsed stage never takes the "1,1,1,1" absent-key path on re-encode.
  out->hasStartingFace = true;
  {
    SepIt it;
    Slice t;
    sep_init(&it, f[1], ',');
    while (sep_next(&it, &t)) {
      if (out->startingFaceCount >= MLK_MAX_STARTING_POINTS)
        FAIL("too many start faces");
      // parseSign (encode.js:158-166) never fails: anything that is not
      // exactly -1 — NaN included — becomes +1.
      double v;
      out->startingFace[out->startingFaceCount++] =
          (parse_int_js(t, &v) && v == -1) ? -1.0 : 1.0;
    }
  }

  SurfaceList *types[6] = {&out->s.ground, &out->s.ceiling, &out->s.wallL,
                           &out->s.wallR,  &out->s.platform, &out->bgLine};
  for (int n = 0; n < 6; ++n) {
    if (!parse_surfaces(f[2 + n], types[n], reason)) return false;
  }

  if (!parse_polygons(f[8], &out->polygon, reason)) return false;
  if (!parse_polygons(f[9], &out->bgPolygon, reason)) return false;

  {
    SepIt it;
    Slice rec;
    sep_init(&it, f[10], '~');
    while (sep_next(&it, &rec)) {
      if (out->ledgeCount >= ML_MAX_LEDGES) FAIL("too many ledges");
      SepIt ti;
      Slice t;
      int nt = 0;
      Slice tok[3];
      sep_init(&ti, rec, ',');
      while (nt < 3 && sep_next(&ti, &t)) tok[nt++] = t;
      MlLedge *lg = &out->ledge[out->ledgeCount];
      // parseLedge (encode.js:145-156): only the first character decides.
      lg->list = (nt >= 1 && tok[0].n >= 1 && tok[0].s[0] == 'p') ? 'p' : 'g';
      double idx;
      if (nt < 2 || !parse_int_js(tok[1], &idx)) FAIL("bad ledge index");
      // encode.js:236 then evaluates stage[type][index][side]. An index
      // that is out of range (or NaN) dereferences undefined and THROWS,
      // and the catch at :247 turns that into the null return — so an
      // out-of-range ledge is a rejected code upstream too, not a repair.
      const SurfaceList *ref = (lg->list == 'p') ? &out->s.platform : &out->s.ground;
      if (!(idx >= 0 && idx < (double)ref->count)) FAIL("ledge out of range");
      lg->index = idx;
      double side = 0;
      if (nt >= 3 && parse_int_js(tok[2], &side)) {
        if (side != 0 && side != 1) side = 0;  // encode.js:147-149
      } else {
        side = 0;
      }
      lg->point = side;
      ++out->ledgeCount;
    }
  }

  if (!parse_points(f[11], out->target, MLK_MAX_TARGETS, &out->targetCount,
                    reason, "too many targets"))
    return false;

  {
    const int nt = sep_count(f[12], ',');
    if (nt < 4) FAIL("blastzone needs four numbers");
    SepIt ti;
    Slice t;
    double b[4];
    sep_init(&ti, f[12], ',');
    for (int k = 0; k < 4; ++k) {
      sep_next(&ti, &t);
      if (!parse_num(t, &b[k])) FAIL("bad number");
    }
    out->blastzone = box2d(b[0], b[1], b[2], b[3]);
  }

  {
    // encode.js:234 — `parseFloat(objects[13]) || 3`, so an absent, empty
    // or zero scale becomes 3.
    double sc;
    if (f[13].n == 0) {
      sc = 3.0;
    } else if (!parse_num(f[13], &sc)) {
      FAIL("bad scale");
    }
    if (sc == 0.0) sc = 3.0;
    out->scale = sc;
  }

  // encode.js:252-255 — the last gate, after everything else parsed.
  if (out->startingPointCount < 1) FAIL("missing starting point");

  // UPSTREAM BUG 2, CARRIED VERBATIM (encode.js:244
  // `stage.polygonMap = stage.polygon.map((p) => null)`). A decoded stage
  // has NO polygon <-> surface links at all: every entry is null. Upstream
  // does not rebuild them and only guards against crashing on them
  // (targetbuilder.js:698, :1566), so re-editing an imported stage really
  // does let you drag a polygon's outline away from its collision
  // surfaces. That is the behaviour of every code the browser produced;
  // re-deriving the map here would diverge from all of them.
  out->polygonMapCount = out->polygon.count;
  for (int i = 0; i < out->polygon.count; ++i) out->polygonMapIsNull[i] = true;

  return true;
}
