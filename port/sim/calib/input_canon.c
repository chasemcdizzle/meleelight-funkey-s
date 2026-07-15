// input_canon.c — see input_canon.h.
#include "input_canon.h"

#include <string.h>

// The 22 Input keys in canon (byte-wise sorted) order, paired with their
// field kind. Sort check: uppercase sorts before lowercase, so
// l < lA < lsX and r < rA < rawX < rawY < rawcsX < rawcsY.
typedef enum { FK_BOOL, FK_NUM } FieldKind;

typedef struct {
  const char *key;
  FieldKind kind;
  size_t off; // offsetof in MlInput
} FieldSpec;

#include <stddef.h>

static const FieldSpec FIELDS[22] = {
  { "a",      FK_BOOL, offsetof(MlInput, a) },
  { "b",      FK_BOOL, offsetof(MlInput, b) },
  { "csX",    FK_NUM,  offsetof(MlInput, csX) },
  { "csY",    FK_NUM,  offsetof(MlInput, csY) },
  { "dd",     FK_BOOL, offsetof(MlInput, dd) },
  { "dl",     FK_BOOL, offsetof(MlInput, dl) },
  { "dr",     FK_BOOL, offsetof(MlInput, dr) },
  { "du",     FK_BOOL, offsetof(MlInput, du) },
  { "l",      FK_BOOL, offsetof(MlInput, l) },
  { "lA",     FK_NUM,  offsetof(MlInput, lA) },
  { "lsX",    FK_NUM,  offsetof(MlInput, lsX) },
  { "lsY",    FK_NUM,  offsetof(MlInput, lsY) },
  { "r",      FK_BOOL, offsetof(MlInput, r) },
  { "rA",     FK_NUM,  offsetof(MlInput, rA) },
  { "rawX",   FK_NUM,  offsetof(MlInput, rawX) },
  { "rawY",   FK_NUM,  offsetof(MlInput, rawY) },
  { "rawcsX", FK_NUM,  offsetof(MlInput, rawcsX) },
  { "rawcsY", FK_NUM,  offsetof(MlInput, rawcsY) },
  { "s",      FK_BOOL, offsetof(MlInput, s) },
  { "x",      FK_BOOL, offsetof(MlInput, x) },
  { "y",      FK_BOOL, offsetof(MlInput, y) },
  { "z",      FK_BOOL, offsetof(MlInput, z) },
};

MlInput ml_input_from_canon(const CanonVal *v) {
  MlInput in;
  if (v->type != CV_OBJ || v->nkeys != 22) {
    ml_canon_fail("expected 22-key Input object");
  }
  for (int i = 0; i < 22; i++) {
    if (strcmp(v->keys[i], FIELDS[i].key) != 0) {
      ml_canon_fail("Input key set/order out of domain");
    }
    char *base = (char *)&in;
    if (FIELDS[i].kind == FK_BOOL) {
      if (v->vals[i]->type == CV_BOOL) {
        *(bool *)(base + FIELDS[i].off) = v->vals[i]->b;
      } else if (v->vals[i]->type == CV_NUM) {
        // Measured domain extension (M2 task 8, g08 CPU golden): ai.js
        // writes NUMBERS into button fields of aiInputBank inputs
        // (`aiInputBank[i][0].l = 0` / `= 1.0`, ai.js:37/170). Every sim
        // consumer of the button fields is truthiness-only (verified: no
        // raw `= input[p][k].<button>` propagation into player state in
        // characters/ or actionStateShortcuts.js), so JS truthiness is
        // the faithful bool mapping. -0/NaN are falsy.
        const double d = v->vals[i]->num;
        *(bool *)(base + FIELDS[i].off) = d == d && d != 0;
      } else {
        ml_canon_fail("Input boolean field not T/F/number (out of domain)");
      }
    } else {
      if (v->vals[i]->type != CV_NUM) {
        ml_canon_fail("Input number field not a number (undef out of domain)");
      }
      *(double *)(base + FIELDS[i].off) = v->vals[i]->num;
    }
  }
  return in;
}

MlInputBuffer ml_input_buffer_from_canon(const CanonVal *v) {
  MlInputBuffer buf;
  if (v->type != CV_ARR || v->count != 8) {
    ml_canon_fail("expected 8-deep InputBuffer array");
  }
  for (int k = 0; k < 8; k++) buf.slot[k] = ml_input_from_canon(v->items[k]);
  return buf;
}

void ml_input_canon(CanonBuf *b, const MlInput *in) {
  cb_putc(b, '{');
  for (int i = 0; i < 22; i++) {
    if (i) cb_putc(b, ',');
    cb_qstr(b, FIELDS[i].key);
    cb_putc(b, ':');
    const char *base = (const char *)in;
    if (FIELDS[i].kind == FK_BOOL) {
      cb_puts(b, *(const bool *)(base + FIELDS[i].off) ? "T" : "F");
    } else {
      cb_num(b, *(const double *)(base + FIELDS[i].off));
    }
  }
  cb_putc(b, '}');
}

void ml_input_buffer_canon(CanonBuf *b, const MlInputBuffer *buf) {
  cb_putc(b, '[');
  for (int k = 0; k < 8; k++) {
    if (k) cb_putc(b, ',');
    ml_input_canon(b, &buf->slot[k]);
  }
  cb_putc(b, ']');
}
