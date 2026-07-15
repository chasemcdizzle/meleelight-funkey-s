// ai_bridge.c — AIBRIDGE1 artifact loader + apply (M2 task 16). See
// ai_bridge.h for the contract and format.
#include "ai_bridge.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char *const ai_bridge_field_names[AI_BRIDGE_NFIELDS] = {
  "a", "b", "csX", "csY", "dd", "dl", "dr", "du", "l", "lA", "lsX", "lsY",
  "r", "rA", "rawX", "rawY", "rawcsX", "rawcsY", "s", "x", "y", "z",
};

// measured AI write set (ai.js live assignment sites; see ai_bridge.h)
const bool ai_bridge_field_ai_written[AI_BRIDGE_NFIELDS] = {
  /* a     */ true,  /* b   */ true,  /* csX  */ true,  /* csY    */ true,
  /* dd    */ false, /* dl  */ false, /* dr   */ false, /* du     */ false,
  /* l     */ true,  /* lA  */ true,  /* lsX  */ true,  /* lsY    */ true,
  /* r     */ false, /* rA  */ false, /* rawX */ false, /* rawY   */ false,
  /* rawcsX*/ false, /* rawcsY */ false, /* s   */ false, /* x     */ true,
  /* y     */ true,  /* z   */ true,
};

MlAiVal *ai_row_field(MlAiInput *row, int idx) {
  switch (idx) {
    case 0: return &row->a;       case 1: return &row->b;
    case 2: return &row->csX;     case 3: return &row->csY;
    case 4: return &row->dd;      case 5: return &row->dl;
    case 6: return &row->dr;      case 7: return &row->du;
    case 8: return &row->l;       case 9: return &row->lA;
    case 10: return &row->lsX;    case 11: return &row->lsY;
    case 12: return &row->r;      case 13: return &row->rA;
    case 14: return &row->rawX;   case 15: return &row->rawY;
    case 16: return &row->rawcsX; case 17: return &row->rawcsY;
    case 18: return &row->s;      case 19: return &row->x;
    case 20: return &row->y;      case 21: return &row->z;
    default: return NULL;
  }
}

const MlAiVal *ai_row_field_const(const MlAiInput *row, int idx) {
  return ai_row_field((MlAiInput *)row, idx);
}

static int br_err(const char *path, long lineno, const char *msg) {
  fprintf(stderr, "AI BRIDGE LOAD FAIL %s:%ld: %s\n", path, lineno, msg);
  return 1;
}

static int parse_hex16(const char *s, uint64_t *out) {
  uint64_t v = 0;
  for (int i = 0; i < 16; i++) {
    const char c = s[i];
    uint64_t d;
    if (c >= '0' && c <= '9') d = (uint64_t)(c - '0');
    else if (c >= 'a' && c <= 'f') d = (uint64_t)(c - 'a' + 10);
    else return 0;
    v = (v << 4) | d;
  }
  *out = v;
  return 1;
}

static double bits_to_double(uint64_t u) {
  double d;
  memcpy(&d, &u, sizeof d);
  return d;
}

int ml_ai_bridge_load(MlAiBridge *br, const char *path) {
  memset(br, 0, sizeof *br);
  FILE *f = fopen(path, "r");
  if (!f) return br_err(path, 0, "cannot open");

  char *line = NULL;
  size_t cap = 0;
  long lineno = 0;

  // header
  if (getline(&line, &cap, f) <= 0) { fclose(f); return br_err(path, 1, "empty file"); }
  lineno = 1;
  {
    unsigned long seed;
    long boot, n;
    char golden[16];
    if (sscanf(line, "AIBRIDGE1 %15s seed=%lu boot=%ld entries=%ld",
               golden, &seed, &boot, &n) != 4 || n < 0 || boot < 0 ||
        seed > 0xffffffffUL) {
      free(line); fclose(f);
      return br_err(path, 1, "bad AIBRIDGE1 header");
    }
    snprintf(br->golden, sizeof br->golden, "%s", golden);
    br->seed = (uint32_t)seed;
    br->boot = boot;
    br->nentries = n;
  }
  br->entries = calloc((size_t)(br->nentries ? br->nentries : 1),
                       sizeof *br->entries);
  if (!br->entries) { free(line); fclose(f); return br_err(path, 1, "oom"); }

  long got = 0;
  ssize_t len;
  while ((len = getline(&line, &cap, f)) > 0) {
    lineno++;
    if (line[len - 1] == '\n') line[--len] = 0;
    if (len == 0) continue;
    if (got >= br->nentries) { free(line); fclose(f); return br_err(path, lineno, "more entries than header count"); }
    MlAiBridgeEntry *e = &br->entries[got];

    char *p = line;
    char *end;
    e->frame = strtol(p, &end, 10);
    if (end == p || *end != ' ' || e->frame < 1) { free(line); fclose(f); return br_err(path, lineno, "bad frame"); }
    p = end + 1;
    long slot = strtol(p, &end, 10);
    if (end == p || *end != ' ' || slot < 0 || slot > 3) { free(line); fclose(f); return br_err(path, lineno, "bad slot"); }
    e->slot = (int)slot;
    p = end + 1;
    long nd = strtol(p, &end, 10);
    if (end == p || nd < 0 || nd > 1000000) { free(line); fclose(f); return br_err(path, lineno, "bad ndraws"); }
    e->ndraws = (int)nd;
    p = end;
    e->draws = nd ? malloc((size_t)nd * sizeof *e->draws) : NULL;
    if (nd && !e->draws) { free(line); fclose(f); return br_err(path, lineno, "oom"); }
    for (long k = 0; k < nd; k++) {
      if (*p != ' ') { free(line); fclose(f); return br_err(path, lineno, "truncated draws"); }
      p++;
      uint64_t u;
      if (!parse_hex16(p, &u)) { free(line); fclose(f); return br_err(path, lineno, "bad draw hex"); }
      e->draws[k] = bits_to_double(u);
      p += 16;
    }
    for (int k = 0; k < AI_BRIDGE_NFIELDS; k++) {
      if (*p != ' ') { free(line); fclose(f); return br_err(path, lineno, "truncated fields"); }
      p++;
      if (p[0] == 'B' && (p[1] == '0' || p[1] == '1')) {
        e->field[k] = aiv_bool(p[1] == '1');
        p += 2;
      } else if (p[0] == 'U') {
        e->field[k] = aiv_undef();
        p += 1;
      } else if (p[0] == 'N') {
        uint64_t u;
        if (!parse_hex16(p + 1, &u)) { free(line); fclose(f); return br_err(path, lineno, "bad field hex"); }
        e->field[k] = aiv_num(bits_to_double(u));
        p += 17;
      } else {
        free(line); fclose(f);
        return br_err(path, lineno, "bad field token");
      }
    }
    if (*p != 0) { free(line); fclose(f); return br_err(path, lineno, "trailing bytes"); }
    if (got > 0) {
      const MlAiBridgeEntry *prev = &br->entries[got - 1];
      if (e->frame < prev->frame) { free(line); fclose(f); return br_err(path, lineno, "frame order broken"); }
    }
    got++;
  }
  free(line);
  fclose(f);
  if (got != br->nentries) return br_err(path, lineno, "fewer entries than header count");
  br->cursor = 0;
  return 0;
}

void ml_ai_bridge_free(MlAiBridge *br) {
  for (long i = 0; i < br->nentries; i++) free(br->entries[i].draws);
  free(br->entries);
  memset(br, 0, sizeof *br);
}

const MlAiBridgeEntry *ml_ai_bridge_peek(const MlAiBridge *br) {
  return br->cursor < br->nentries ? &br->entries[br->cursor] : NULL;
}

void ml_ai_bridge_advance(MlAiBridge *br) {
  if (br->cursor < br->nentries) br->cursor++;
}

MlAiBridgeApplyResult ml_ai_bridge_apply(const MlAiBridgeEntry *e,
                                         MlRng *rng, MlAiInput *bankRow) {
  MlAiBridgeApplyResult r;
  r.bad_draw = -1;
  r.bad_draw_got = 0;
  r.bad_field = -1;

  // 1. burn the recorded draws on the CHAINED stream, bit-verified
  for (int k = 0; k < e->ndraws; k++) {
    const double got = ml_rng_next(rng);
    uint64_t ug, ue;
    memcpy(&ug, &got, sizeof ug);
    memcpy(&ue, &e->draws[k], sizeof ue);
    if (ug != ue && r.bad_draw == -1) {
      r.bad_draw = k;
      r.bad_draw_got = got;
    }
  }

  // 2. the never-AI-written fields must match the chained row (the
  //    recording carries them as data, but they are the CHAIN's values —
  //    interpretInputs stash/defaults — never the AI's to change)
  for (int k = 0; k < AI_BRIDGE_NFIELDS; k++) {
    if (ai_bridge_field_ai_written[k]) continue;
    if (!aiv_eq(e->field[k], *ai_row_field_const(bankRow, k)) &&
        r.bad_field == -1) {
      r.bad_field = k;
    }
  }

  // 3. install the recorded post-runAI row (authoritative)
  for (int k = 0; k < AI_BRIDGE_NFIELDS; k++) {
    *ai_row_field(bankRow, k) = e->field[k];
  }
  return r;
}
