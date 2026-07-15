// ml_ser.c — CHECKSUM.md §3 serialization primitives + §4 hash (M2 task
// 15). See ml_ser.h for the contract and the pagelib.js line citations.
#include "ml_ser.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ml_fmt.h"
#include "sha256.h" // oracle/qjs/sha256.c — FIPS 180-4, self-tested

static void sb_grow(MlSb *b, size_t need) {
  if (b->len + need + 1 <= b->cap) return;
  size_t cap = b->cap ? b->cap : 256;
  while (b->len + need + 1 > cap) cap *= 2;
  char *nb = (char *) realloc(b->buf, cap);
  if (!nb) {
    fprintf(stderr, "ml_ser: out of memory (%zu bytes)\n", cap);
    exit(1);
  }
  b->buf = nb;
  b->cap = cap;
}

void ml_sb_init(MlSb *b) {
  b->buf = NULL;
  b->len = b->cap = 0;
  sb_grow(b, 0);
  b->buf[0] = '\0';
}

void ml_sb_free(MlSb *b) {
  free(b->buf);
  b->buf = NULL;
  b->len = b->cap = 0;
}

void ml_sb_reset(MlSb *b) {
  b->len = 0;
  if (b->buf) b->buf[0] = '\0';
}

void ml_sb_write(MlSb *b, const char *s, size_t n) {
  sb_grow(b, n);
  memcpy(b->buf + b->len, s, n);
  b->len += n;
  b->buf[b->len] = '\0';
}

void ml_sb_putc(MlSb *b, char c) { ml_sb_write(b, &c, 1); }

void ml_sb_puts(MlSb *b, const char *s) { ml_sb_write(b, s, strlen(s)); }

// §3.4 — pagelib.js:10-13: Object.is(v, -0) -> "-0"; else String(v).
void ml_sb_num(MlSb *b, double x) {
  uint64_t bits;
  memcpy(&bits, &x, 8);
  if (bits == UINT64_C(0x8000000000000000)) { // negative zero, bit-exact
    ml_sb_write(b, "-0", 2);
    return;
  }
  char tmp[ML_FMT_DTOA_MAX];
  int n = ml_fmt_dtoa(x, tmp);
  ml_sb_write(b, tmp, (size_t) n);
}

// §3.5 — JSON.stringify string quoting (ECMA-262 QuoteJSONString).
void ml_sb_jsonstr(MlSb *b, const char *s) {
  ml_sb_putc(b, '"');
  for (const unsigned char *p = (const unsigned char *) s; *p; ++p) {
    unsigned char c = *p;
    switch (c) {
      case '"': ml_sb_write(b, "\\\"", 2); break;
      case '\\': ml_sb_write(b, "\\\\", 2); break;
      case '\b': ml_sb_write(b, "\\b", 2); break;
      case '\t': ml_sb_write(b, "\\t", 2); break;
      case '\n': ml_sb_write(b, "\\n", 2); break;
      case '\f': ml_sb_write(b, "\\f", 2); break;
      case '\r': ml_sb_write(b, "\\r", 2); break;
      default:
        if (c < 0x20) {
          char esc[7];
          static const char hexd[] = "0123456789abcdef";
          esc[0] = '\\'; esc[1] = 'u'; esc[2] = '0'; esc[3] = '0';
          esc[4] = hexd[c >> 4]; esc[5] = hexd[c & 0xf]; esc[6] = '\0';
          ml_sb_write(b, esc, 6);
        } else {
          ml_sb_putc(b, (char) c); // UTF-8 bytes pass through verbatim
        }
    }
  }
  ml_sb_putc(b, '"');
}

// §3.6 — pagelib.js:19.
void ml_sb_bool(MlSb *b, bool v) { ml_sb_putc(b, v ? 'T' : 'F'); }

// §4 — pagelib.js:66-73: SHA-256 over UTF-8 bytes, lowercase hex.
void ml_sha256_hex(const void *data, size_t len, char out_hex[65]) {
  uint8_t digest[32];
  sha256((const uint8_t *) data, len, digest);
  static const char hexd[] = "0123456789abcdef";
  for (int i = 0; i < 32; ++i) {
    out_hex[2 * i] = hexd[digest[i] >> 4];
    out_hex[2 * i + 1] = hexd[digest[i] & 0xf];
  }
  out_hex[64] = '\0';
}
