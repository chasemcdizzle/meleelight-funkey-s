// canon.c — see canon.h. -ffp-contract=off like every TU (no FP math here
// beyond bit moves, but the rule is uniform).
#include "canon.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// --- arena -----------------------------------------------------------------

#define ARENA_CAP (8u << 20)
static unsigned char arena[ARENA_CAP];
static size_t arena_used = 0;

void canon_arena_reset(void) { arena_used = 0; }

void *canon_alloc(size_t n) {
  n = (n + 15) & ~(size_t)15;
  if (arena_used + n > ARENA_CAP) {
    fprintf(stderr, "canon: arena exhausted (%zu + %zu)\n", arena_used, n);
    exit(3);
  }
  void *p = arena + arena_used;
  arena_used += n;
  return p;
}

// --- parser ----------------------------------------------------------------

typedef struct { const char *p; const char *err; } Parser;

static CanonVal *cv_new(CvType t) {
  CanonVal *v = canon_alloc(sizeof(CanonVal));
  memset(v, 0, sizeof *v);
  v->type = t;
  return v;
}

static CanonVal *parse_value(Parser *ps);

static bool lit(Parser *ps, const char *word) {
  size_t n = strlen(word);
  if (strncmp(ps->p, word, n) == 0) { ps->p += n; return true; }
  return false;
}

static const char *parse_string_token(Parser *ps) {
  // '"' ... '"' — domain has no escapes; hard-fail on backslash.
  if (*ps->p != '"') { ps->err = "expected string"; return NULL; }
  ps->p++;
  const char *start = ps->p;
  while (*ps->p && *ps->p != '"') {
    if (*ps->p == '\\') { ps->err = "escape sequence in string (out of domain)"; return NULL; }
    ps->p++;
  }
  if (*ps->p != '"') { ps->err = "unterminated string"; return NULL; }
  size_t n = (size_t)(ps->p - start);
  char *s = canon_alloc(n + 1);
  memcpy(s, start, n);
  s[n] = 0;
  ps->p++;
  return s;
}

static int hexval(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  return -1;
}

static CanonVal *parse_value(Parser *ps) {
  switch (*ps->p) {
    case 'n': if (lit(ps, "null")) return cv_new(CV_NULL); break;
    case 'u': if (lit(ps, "undef")) return cv_new(CV_UNDEF); break;
    case 'T': ps->p++; { CanonVal *v = cv_new(CV_BOOL); v->b = true; return v; }
    case 'F': ps->p++; { CanonVal *v = cv_new(CV_BOOL); v->b = false; return v; }
    case 'f': if (lit(ps, "fn")) return cv_new(CV_FN); break;
    case 'c': if (lit(ps, "cyc")) return cv_new(CV_CYC); break;
    case 'd': {
      if (ps->p[1] != ':') break;
      uint64_t bits = 0;
      for (int i = 0; i < 16; i++) {
        int h = hexval(ps->p[2 + i]);
        if (h < 0) { ps->err = "bad d:<hex16> number"; return NULL; }
        bits = (bits << 4) | (uint64_t)h;
      }
      ps->p += 18;
      CanonVal *v = cv_new(CV_NUM);
      memcpy(&v->num, &bits, 8);
      return v;
    }
    case '"': {
      const char *s = parse_string_token(ps);
      if (!s) return NULL;
      CanonVal *v = cv_new(CV_STR);
      v->str = s;
      return v;
    }
    case '[': {
      ps->p++;
      CanonVal *v = cv_new(CV_ARR);
      // two-pass-free growth: collect into a temporary chain on the arena
      int cap = 8;
      v->items = canon_alloc(sizeof(CanonVal *) * (size_t)cap);
      if (*ps->p == ']') { ps->p++; return v; }
      for (;;) {
        CanonVal *item = parse_value(ps);
        if (!item) return NULL;
        if (v->count == cap) {
          cap *= 2;
          CanonVal **ni = canon_alloc(sizeof(CanonVal *) * (size_t)cap);
          memcpy(ni, v->items, sizeof(CanonVal *) * (size_t)v->count);
          v->items = ni;
        }
        v->items[v->count++] = item;
        if (*ps->p == ',') { ps->p++; continue; }
        if (*ps->p == ']') { ps->p++; return v; }
        ps->err = "expected , or ] in array";
        return NULL;
      }
    }
    case '{': {
      ps->p++;
      CanonVal *v = cv_new(CV_OBJ);
      int cap = 8;
      v->keys = canon_alloc(sizeof(char *) * (size_t)cap);
      v->vals = canon_alloc(sizeof(CanonVal *) * (size_t)cap);
      if (*ps->p == '}') { ps->p++; return v; }
      for (;;) {
        const char *k = parse_string_token(ps);
        if (!k) return NULL;
        if (*ps->p != ':') { ps->err = "expected : in object"; return NULL; }
        ps->p++;
        CanonVal *val = parse_value(ps);
        if (!val) return NULL;
        if (v->nkeys == cap) {
          cap *= 2;
          const char **nk = canon_alloc(sizeof(char *) * (size_t)cap);
          CanonVal **nv = canon_alloc(sizeof(CanonVal *) * (size_t)cap);
          memcpy(nk, v->keys, sizeof(char *) * (size_t)v->nkeys);
          memcpy(nv, v->vals, sizeof(CanonVal *) * (size_t)v->nkeys);
          v->keys = nk;
          v->vals = nv;
        }
        v->keys[v->nkeys] = k;
        v->vals[v->nkeys] = val;
        v->nkeys++;
        if (*ps->p == ',') { ps->p++; continue; }
        if (*ps->p == '}') { ps->p++; return v; }
        ps->err = "expected , or } in object";
        return NULL;
      }
    }
    default: break;
  }
  if (!ps->err) ps->err = "unexpected character";
  return NULL;
}

const CanonVal *canon_parse(const char *s, const char **err) {
  Parser ps; ps.p = s; ps.err = NULL;
  CanonVal *v = parse_value(&ps);
  if (v && *ps.p != 0) { ps.err = "trailing characters"; v = NULL; }
  if (err) *err = ps.err;
  return v;
}

// --- emission ----------------------------------------------------------------

void cb_init(CanonBuf *b) {
  b->cap = 4096;
  b->buf = malloc(b->cap);
  b->len = 0;
  b->buf[0] = 0;
}

void cb_free(CanonBuf *b) { free(b->buf); }

static void cb_reserve(CanonBuf *b, size_t extra) {
  if (b->len + extra + 1 > b->cap) {
    while (b->len + extra + 1 > b->cap) b->cap *= 2;
    b->buf = realloc(b->buf, b->cap);
    if (!b->buf) { fprintf(stderr, "canon: oom\n"); exit(3); }
  }
}

void cb_puts(CanonBuf *b, const char *s) {
  size_t n = strlen(s);
  cb_reserve(b, n);
  memcpy(b->buf + b->len, s, n);
  b->len += n;
  b->buf[b->len] = 0;
}

void cb_putc(CanonBuf *b, char c) {
  cb_reserve(b, 1);
  b->buf[b->len++] = c;
  b->buf[b->len] = 0;
}

void cb_num(CanonBuf *b, double d) {
  uint64_t bits;
  memcpy(&bits, &d, 8);
  char tmp[24];
  snprintf(tmp, sizeof tmp, "d:%016llx", (unsigned long long)bits);
  cb_puts(b, tmp);
}

void cb_str1(CanonBuf *b, char c) {
  cb_putc(b, '"');
  cb_putc(b, c);
  cb_putc(b, '"');
}

void cb_qstr(CanonBuf *b, const char *s) {
  cb_putc(b, '"');
  cb_puts(b, s);
  cb_putc(b, '"');
}
