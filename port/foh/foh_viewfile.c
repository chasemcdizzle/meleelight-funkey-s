// port/foh/foh_viewfile.c — see foh_viewfile.h for why this is shared.
#include "foh_viewfile.h"

#include <errno.h>
#include <stdarg.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../sim/ml_ser.h" // ml_sha256_hex
#include "foh_persist.h"

#define VF_SUMLEN (4 + 64 + 1) // "SUM " + 64 hex + LF

static void vf_hex16(double v, char out[17]) {
  uint64_t b;
  memcpy(&b, &v, 8);
  static const char *H = "0123456789abcdef";
  for (int i = 0; i < 16; i++) out[i] = H[(b >> (60 - 4 * i)) & 0xFu];
  out[16] = 0;
}

static bool vf_unhex16(const char *p, double *out) {
  uint64_t b = 0;
  for (int i = 0; i < 16; i++) {
    const char c = p[i];
    int d;
    if (c >= '0' && c <= '9') d = c - '0';
    else if (c >= 'a' && c <= 'f') d = 10 + (c - 'a');
    else return false;
    b = (b << 4) | (uint64_t)d;
  }
  memcpy(out, &b, 8);
  // A view carries positions and timers. NaN and infinity are not values any
  // of them can legitimately hold, and letting one in would put a screen into
  // a state its own arithmetic cannot leave.
  return isfinite(*out);
}

// --- writing ---------------------------------------------------------------

static void vf_emit(FohViewOut *o, const char *fmt, ...) {
  if (o->overflow) return;
  va_list ap;
  va_start(ap, fmt);
  const int w = vsnprintf(o->buf + o->n, sizeof o->buf - o->n, fmt, ap);
  va_end(ap);
  if (w <= 0 || (size_t)w >= sizeof o->buf - o->n) {
    o->overflow = true;
    return;
  }
  o->n += (size_t)w;
}

void foh_view_begin(FohViewOut *o, const char *magic) {
  o->n = 0;
  o->overflow = false;
  vf_emit(o, "%s\n", magic);
}

void foh_view_put_int(FohViewOut *o, const char *key, int v) {
  vf_emit(o, "%s %d\n", key, v);
}

void foh_view_put_double(FohViewOut *o, const char *key, double v) {
  char hx[17];
  vf_hex16(v, hx);
  vf_emit(o, "%s %s\n", key, hx);
}

void foh_view_put_int_at(FohViewOut *o, const char *key, int i, int v) {
  vf_emit(o, "%s %d %d\n", key, i, v);
}

void foh_view_put_double_at(FohViewOut *o, const char *key, int i, double v) {
  char hx[17];
  vf_hex16(v, hx);
  vf_emit(o, "%s %d %s\n", key, i, hx);
}

bool foh_view_publish(FohViewOut *o, const char *name, const char **why) {
  if (o->overflow || o->n + VF_SUMLEN > sizeof o->buf) {
    if (why) *why = "VIEW TOO LARGE";
    return false;
  }
  char hex[65];
  ml_sha256_hex(o->buf, o->n, hex);
  memcpy(o->buf + o->n, "SUM ", 4);
  memcpy(o->buf + o->n + 4, hex, 64);
  o->buf[o->n + 68] = '\n';
  const char *pubWhy = 0;
  if (!foh_persist_publish(name, o->buf, o->n + VF_SUMLEN, &pubWhy)) {
    if (why) *why = pubWhy ? pubWhy : "SAVE FAILED";
    return false;
  }
  if (why) *why = 0;
  return true;
}

// --- reading ---------------------------------------------------------------

static bool vf_path(const char *name, char *buf, size_t cap) {
  const int n = snprintf(buf, cap, "%s/%s", foh_persist_dir(), name);
  return n > 0 && (size_t)n < cap;
}

static bool vf_fail(FohViewIn *in, const char *why) {
  if (!in->bad) {
    in->bad = true;
    in->why = why; // FIRST refusal wins; a later one cannot mask it
  }
  return false;
}

bool foh_view_load(FohViewIn *in, const char *name, const char *magic,
                   const char **why) {
  memset(in, 0, sizeof *in);
  char path[512];
  if (!vf_path(name, path, sizeof path)) {
    if (why) *why = "PATH TOO LONG";
    return false;
  }
  FILE *f = fopen(path, "rb");
  if (!f) {
    if (why) *why = "EMPTY";
    return false;
  }
  // One byte over the cap on purpose: a short read then PROVES the file fits,
  // with no seek/stat race against a writer.
  const size_t n = fread(in->buf, 1, sizeof in->buf, f);
  const bool ferr = ferror(f) != 0;
  fclose(f);
  if (ferr) { if (why) *why = "UNREADABLE"; return false; }
  if (n > FOH_VIEW_MAX) { if (why) *why = "FILE TOO LARGE"; return false; }
  const size_t hdr = strlen(magic) + 1;
  if (n < hdr || memcmp(in->buf, magic, hdr - 1) != 0 ||
      in->buf[hdr - 1] != '\n') {
    if (why) *why = "NOT A VIEW FILE";
    return false;
  }
  if (n < hdr + VF_SUMLEN) { if (why) *why = "TRUNCATED"; return false; }
  const size_t sumAt = n - VF_SUMLEN;
  if (memcmp(in->buf + sumAt, "SUM ", 4) != 0 || in->buf[n - 1] != '\n') {
    if (why) *why = "NO SUM LINE";
    return false;
  }
  // INTEGRITY BEFORE MEANING — not one byte is parsed above this line.
  char hex[65];
  ml_sha256_hex(in->buf, sumAt, hex);
  if (memcmp(in->buf + sumAt + 4, hex, 64) != 0) {
    if (why) *why = "VIEW SUM MISMATCH";
    return false;
  }
  in->buf[sumAt] = 0;
  in->p = in->buf + hdr;
  in->end = in->buf + sumAt;
  if (why) *why = 0;
  return true;
}

// Match `<key> ` (and an index, when idx >= 0) and advance past it.
static bool vf_key(FohViewIn *in, const char *key, int idx) {
  if (in->bad) return false;
  const size_t kl = strlen(key);
  if ((size_t)(in->end - in->p) <= kl || memcmp(in->p, key, kl) != 0 ||
      in->p[kl] != ' ') {
    return vf_fail(in, "VIEW ORDER");
  }
  in->p += kl + 1;
  if (idx >= 0) {
    char *e = 0;
    const long got = strtol(in->p, &e, 10);
    if (e == in->p || *e != ' ' || got != (long)idx) {
      return vf_fail(in, "VIEW ORDER");
    }
    in->p = e + 1;
  }
  return true;
}

static int vf_int(FohViewIn *in, int lo, int hi) {
  char *e = 0;
  const long v = strtol(in->p, &e, 10);
  if (e == in->p || *e != '\n') { vf_fail(in, "VIEW GRAMMAR"); return lo; }
  if (v < (long)lo || v > (long)hi) { vf_fail(in, "VIEW DOMAIN"); return lo; }
  in->p = e + 1;
  return (int)v;
}

static double vf_double(FohViewIn *in) {
  double d = 0;
  if ((size_t)(in->end - in->p) < 17 || !vf_unhex16(in->p, &d) ||
      in->p[16] != '\n') {
    vf_fail(in, "VIEW BAD DOUBLE");
    return 0;
  }
  in->p += 17;
  return d;
}

int foh_view_get_int(FohViewIn *in, const char *key, int lo, int hi) {
  if (!vf_key(in, key, -1)) return lo;
  return vf_int(in, lo, hi);
}

double foh_view_get_double(FohViewIn *in, const char *key) {
  if (!vf_key(in, key, -1)) return 0;
  return vf_double(in);
}

int foh_view_get_int_at(FohViewIn *in, const char *key, int i, int lo,
                        int hi) {
  if (!vf_key(in, key, i)) return lo;
  return vf_int(in, lo, hi);
}

double foh_view_get_double_at(FohViewIn *in, const char *key, int i) {
  if (!vf_key(in, key, i)) return 0;
  return vf_double(in);
}

bool foh_view_ok(FohViewIn *in, const char **why) {
  if (in->bad) { if (why) *why = in->why; return false; }
  // A file with bytes we did not read is not the file we thought it was.
  if (in->p != in->end) { if (why) *why = "VIEW TRAILING BYTES"; return false; }
  if (why) *why = 0;
  return true;
}

void foh_view_consume(const char *name) {
  char path[512];
  if (!vf_path(name, path, sizeof path)) return;
  if (remove(path) != 0 && errno != ENOENT) {
    // Not fatal — the player already has their screen back — but SAID,
    // because a view that outlives its consumption is a stale file waiting to
    // be paired with a newer document.
    fprintf(stderr, "foh_view: stale %s could not be removed\n", name);
  }
}
