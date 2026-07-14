// canon.h — canon-v1 value parsing + emission for the M2-CAL replay driver
// (FORMAT.md "canon v1"). Parser builds a CanonVal tree from a recorded
// serialization; the emitter helpers build canon strings from C values.
// All allocation is from a per-record bump arena (reset between records).
#ifndef ML_CANON_H
#define ML_CANON_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

typedef enum {
  CV_NULL, CV_UNDEF, CV_BOOL, CV_NUM, CV_STR, CV_FN, CV_CYC, CV_ARR, CV_OBJ
} CvType;

typedef struct CanonVal CanonVal;
struct CanonVal {
  CvType type;
  bool b;            // CV_BOOL
  double num;        // CV_NUM (bit-exact from d:<hex16>)
  const char *str;   // CV_STR (no escape sequences in the domain; the
                     // parser hard-fails on backslash)
  CanonVal **items;  // CV_ARR
  int count;
  const char **keys; // CV_OBJ (in serialized == sorted order)
  CanonVal **vals;
  int nkeys;
};

// Arena: reset per record.
void canon_arena_reset(void);
void *canon_alloc(size_t n);

// Parse a full canon value from `s`; returns NULL and sets *err on any
// syntax violation. On success the whole string must be consumed.
const CanonVal *canon_parse(const char *s, const char **err);

// --- emission ------------------------------------------------------------
typedef struct {
  char *buf;
  size_t len, cap;
} CanonBuf;

void cb_init(CanonBuf *b);
void cb_free(CanonBuf *b);
void cb_puts(CanonBuf *b, const char *s);
void cb_putc(CanonBuf *b, char c);
void cb_num(CanonBuf *b, double d);       // d:<16 hex>
void cb_str1(CanonBuf *b, char c);        // "c"
void cb_qstr(CanonBuf *b, const char *s); // "s" (no escaping in domain)

#endif // ML_CANON_H
