// replay_ai.c — M2 task 16 replay driver: the AI-input bridge over the CPU
// goldens (g07/g08). Feeds recorded ai-spec boundary records
// (port/sim/calib/build/<id>.ai.jsonl, FORMAT.md "The ai spec") plus the
// distilled AIBRIDGE1 artifact (<id>.ai-bridge.txt, build-ai-bridge.js)
// through the C input chain and compares canon-v1.1 serializations
// byte-for-byte. A single differing bit anywhere is a divergence.
//
// What is proven per golden:
// - the full-trace interpretInputs CHAIN for BOTH active slots: the human
//   slot through ml_interpret_inputs (the plain-MlInput wrapper), the CPU
//   slot through the tagged core (ai_input.h) with the aiInputBank row
//   chained by the C BRIDGE — slot 0's pollInputs alias write-through
//   modeled by re-copying the chained bank into the built buffer before
//   the physics compare;
// - the seeded mulberry32 chain draw-for-draw over the WHOLE record
//   stream: standalone Math.random records + every runAI record's
//   attributed draw list, burned at its exact call site via
//   ml_ai_bridge_apply (a dropped/extra draw shifts the chain and
//   cascades);
// - the bridge ARTIFACT is what drives the bank (task 17's consumption
//   path): every runAI record is cross-checked against the artifact's
//   next entry (frame/slot structural, draws + row values as divergences)
//   and the bank install comes from the ARTIFACT, so an artifact that
//   disagrees with the capture cannot pass;
// - rule 18 bank chain-verify: every CPU-slot pollInputs ret (the bank
//   row read at interpretInputs time, i.e. the previous runAI's output)
//   is compared against the C chained row — any unrecorded bank write
//   upstream, or any wrong C install, flags at the very next record;
// - the C write-set instrument: ml_ai_bridge_apply verifies the artifact
//   row's never-AI-written fields (dd,dl,dr,du,r,rA,raw*,s) against the
//   chain before installing.
//
// Usage: replay_ai <capture.jsonl> <bridge.txt> [--strict] [--max-print N]
//                  [--stop-first]
//
// Marshalling is STRICT (prevention rule 7): any shape outside the
// captured domain aborts with exit 3 — never guess.
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../ai_bridge.h"
#include "../input/interpret_inputs.h"
#include "canon.h"
#include "input_canon.h"

static const char *g_file = "?";
static long g_lineno = 0;

static void fail(const char *msg) {
  fprintf(stderr, "MARSHAL FAIL %s:%ld: %s\n", g_file, g_lineno, msg);
  exit(3);
}

void ml_canon_fail(const char *msg) { fail(msg); }

void ml_input_out_of_domain(const char *what) {
  fprintf(stderr, "OUT OF DOMAIN %s:%ld: %s\n", g_file, g_lineno, what);
  exit(3);
}

// --- tagged-row canon marshal/emit (canon key order == field order) --------

static MlAiVal aiv_from_canon(const CanonVal *v) {
  if (v->type == CV_BOOL) return aiv_bool(v->b);
  if (v->type == CV_NUM) return aiv_num(v->num);
  if (v->type == CV_UNDEF) return aiv_undef();
  fail("input-plane value outside the bool|number|undefined domain");
  return aiv_undef(); // unreachable
}

static MlAiInput ai_input_from_canon(const CanonVal *v) {
  if (v->type != CV_OBJ || v->nkeys != AI_BRIDGE_NFIELDS) {
    fail("expected a 22-key Input object");
  }
  MlAiInput row;
  for (int k = 0; k < AI_BRIDGE_NFIELDS; k++) {
    if (strcmp(v->keys[k], ai_bridge_field_names[k]) != 0) {
      fail("Input object key set/order drifted from the canon key order");
    }
    *ai_row_field(&row, k) = aiv_from_canon(v->vals[k]);
  }
  return row;
}

static void aiv_canon(CanonBuf *b, MlAiVal v) {
  if (v.tag == AIV_BOOL) cb_puts(b, v.b ? "T" : "F");
  else if (v.tag == AIV_NUM) cb_num(b, v.num);
  else cb_puts(b, "undef");
}

static void ai_input_canon(CanonBuf *b, const MlAiInput *row) {
  cb_putc(b, '{');
  for (int k = 0; k < AI_BRIDGE_NFIELDS; k++) {
    if (k) cb_putc(b, ',');
    cb_qstr(b, ai_bridge_field_names[k]);
    cb_putc(b, ':');
    aiv_canon(b, *ai_row_field_const(row, k));
  }
  cb_putc(b, '}');
}

static void ai_buffer_canon(CanonBuf *b, const MlAiInputBuffer *buf) {
  cb_putc(b, '[');
  for (int k = 0; k < 8; k++) {
    if (k) cb_putc(b, ',');
    ai_input_canon(b, &buf->slot[k]);
  }
  cb_putc(b, ']');
}

// generic echo writer (replay_util.c pattern)
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

// --- chain state -------------------------------------------------------------

static MlInputSimState g_st;
static MlRng g_seeded;
static bool g_boot_seen = false;

static int g_kind[4];                 // -1 unseen, 0 human, 1 ai (pinned)
static MlInputBuffer g_prev[4];       // human chain
static MlInput g_pending[4];
static MlAiInputBuffer g_prev_ai[4];  // ai chain
static MlAiInput g_pending_ai[4];
static MlAiInput g_bank[4];           // chained aiInputBank[slot][0]
static bool g_pending_set[4];
static long g_pending_frame[4];
static long g_cur_frame = 0;

static MlAiBridge g_bridge;

static void chain_init(void) {
  ml_input_sim_state_init(&g_st);
  g_st.gameMode = 3;   // harnessSetupMatch changeGamemode(3)
  g_st.playing = true; // startGame
  for (int i = 0; i < 4; i++) {
    g_kind[i] = -1;
    nullInputs(&g_prev[i]);
    ai_null_inputs(&g_prev_ai[i]);
    g_bank[i] = ai_null_input(); // aiInputBank rows = new inputData()
    g_pending_set[i] = false;
  }
}

static void advance_frame(long frame) {
  if (frame < g_cur_frame) fail("frame order broken");
  for (long f = g_cur_frame; f < frame; f++) {
    if (f >= 1) ml_input_end_of_tick(&g_st);
  }
  g_cur_frame = frame;
}

// --- divergence bookkeeping ---------------------------------------------------

#define NFN 5
static const char *fn_names[NFN] = {
  "rngBoot", "Math.random", "runAI", "pollInputs", "physics",
};
static long fn_total[NFN] = {0}, fn_div[NFN] = {0};
static long divergences = 0, printed = 0, first_div_line = -1;
static int max_print = 5;
static bool stop_first_hit = false, stop_first = false;

static void report_div(int fni, const char *expected, const char *got) {
  divergences++;
  fn_div[fni]++;
  if (first_div_line == -1) first_div_line = g_lineno;
  if (printed < max_print) {
    printed++;
    size_t d = 0;
    while (expected[d] && got[d] && expected[d] == got[d]) d++;
    fprintf(stderr,
            "DIVERGENCE line %ld frame %ld fn %s (first diff at byte %zu)\n"
            "  expected: %.300s\n"
            "  got:      %.300s\n"
            "  context:  ...%.80s VS ...%.80s\n",
            g_lineno, g_cur_frame, fn_names[fni], d, expected, got,
            expected + (d > 40 ? d - 40 : 0), got + (d > 40 ? d - 40 : 0));
  }
  if (stop_first) stop_first_hit = true;
}

static void report_div_msg(int fni, const char *msg) {
  divergences++;
  fn_div[fni]++;
  if (first_div_line == -1) first_div_line = g_lineno;
  if (printed < max_print) {
    printed++;
    fprintf(stderr, "DIVERGENCE line %ld frame %ld fn %s: %s\n",
            g_lineno, g_cur_frame, fn_names[fni], msg);
  }
  if (stop_first) stop_first_hit = true;
}

// --- helpers -------------------------------------------------------------------

static void expect_argc(const CanonVal *args, int n) {
  if (args->type != CV_ARR || args->count != n) fail("bad argument count");
}

static double cv_number_strict(const CanonVal *v) {
  if (v->type != CV_NUM) fail("expected number");
  return v->num;
}

static bool cv_bool(const CanonVal *v) {
  if (v->type != CV_BOOL) fail("expected boolean");
  return v->b;
}

static int cv_slot(const CanonVal *v) {
  const double d = cv_number_strict(v);
  const int i = (int)d;
  if (d != (double)i || i < 0 || i > 3) fail("slot index out of range");
  return i;
}

static bool bits_eq(double a, double b) {
  uint64_t ua, ub;
  memcpy(&ua, &a, sizeof ua);
  memcpy(&ub, &b, sizeof ub);
  return ua == ub;
}

// --- main ----------------------------------------------------------------------

int main(int argc, char **argv) {
  const char *path = NULL, *bridge_path = NULL;
  bool strict = false;
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--strict") == 0) strict = true;
    else if (strcmp(argv[i], "--stop-first") == 0) stop_first = true;
    else if (strcmp(argv[i], "--max-print") == 0 && i + 1 < argc) max_print = atoi(argv[++i]);
    else if (!path) path = argv[i];
    else if (!bridge_path) bridge_path = argv[i];
    else { fprintf(stderr, "unknown arg: %s\n", argv[i]); return 1; }
  }
  if (!path || !bridge_path) {
    fprintf(stderr, "usage: replay_ai <capture.jsonl> <bridge.txt> "
                    "[--strict] [--max-print N] [--stop-first]\n");
    return 1;
  }
  if (ml_ai_bridge_load(&g_bridge, bridge_path) != 0) return 3;

  FILE *f = fopen(path, "r");
  if (!f) { fprintf(stderr, "cannot open %s\n", path); return 1; }
  g_file = path;

  chain_init();

  char *line = NULL;
  size_t linecap = 0;
  long replayed = 0;

  CanonBuf out, exp_buf;
  cb_init(&out);
  cb_init(&exp_buf);

  ssize_t n;
  while ((n = getline(&line, &linecap, f)) > 0 && !stop_first_hit) {
    g_lineno++;
    if (line[n - 1] == '\n') line[n - 1] = 0;
    if (line[0] == 0) continue;

    // <frame> \t <fn> \t <args> \t <ret> [\t <post>]
    char *tab1 = strchr(line, '\t');
    char *tab2 = tab1 ? strchr(tab1 + 1, '\t') : NULL;
    char *tab3 = tab2 ? strchr(tab2 + 1, '\t') : NULL;
    if (!tab1 || !tab2 || !tab3) fail("malformed record (need >= 4 tab fields)");
    char *tab4 = strchr(tab3 + 1, '\t');
    *tab1 = 0; *tab2 = 0; *tab3 = 0;
    if (tab4) *tab4 = 0;
    const char *fn = tab1 + 1;
    const char *args_s = tab2 + 1;
    const char *ret_s = tab3 + 1;
    const char *post_s = tab4 ? tab4 + 1 : NULL;
    const long frame = strtol(line, NULL, 10);

    int fni = -1;
    for (int i = 0; i < NFN; i++) {
      if (strcmp(fn, fn_names[i]) == 0) { fni = i; break; }
    }
    if (fni == -1) {
      if (strcmp(fn, "wsViol") == 0) {
        fail("wsViol record present — the capture's write-set recon fired");
      }
      fail("unknown boundary function");
    }

    advance_frame(frame);

    canon_arena_reset();
    const char *err = NULL;
    const CanonVal *args = canon_parse(args_s, &err);
    if (!args) {
      fprintf(stderr, "PARSE FAIL %s:%ld: %s\n", path, g_lineno, err);
      return 3;
    }

    replayed++;
    fn_total[fni]++;

    if (fni == 0) { // rngBoot
      if (g_boot_seen) fail("rngBoot: seen twice");
      expect_argc(args, 2);
      const double seed = cv_number_strict(args->items[0]);
      const double boot = cv_number_strict(args->items[1]);
      if (seed < 0 || seed != (double)(uint32_t)seed) fail("rngBoot: bad seed");
      if (boot < 0 || boot != (double)(long)boot) fail("rngBoot: bad count");
      // artifact agreement (structural)
      if ((uint32_t)seed != g_bridge.seed || (long)boot != g_bridge.boot) {
        fail("rngBoot: bridge artifact header disagrees with the capture");
      }
      ml_rng_seed(&g_seeded, (uint32_t)seed);
      for (long i = 0; i < (long)boot; i++) (void)ml_rng_next(&g_seeded);
      out.len = 0; out.buf[0] = 0;
      cb_num(&out, (double)g_seeded.a);
      if (strcmp(out.buf, ret_s) != 0) report_div(fni, ret_s, out.buf);
      g_boot_seen = true;
    } else if (fni == 1) { // Math.random (standalone, chain order faithful)
      if (!g_boot_seen) fail("Math.random before rngBoot");
      const double v = ml_rng_next(&g_seeded);
      out.len = 0; out.buf[0] = 0;
      cb_num(&out, v);
      if (strcmp(out.buf, ret_s) != 0) report_div(fni, ret_s, out.buf);
    } else if (fni == 2) { // runAI
      if (!g_boot_seen) fail("runAI before rngBoot");
      if (!post_s) fail("runAI: missing post-state field");
      expect_argc(args, 1);
      const int slot = cv_slot(args->items[0]);
      if (g_kind[slot] != 1) fail("runAI on a non-AI slot");
      const CanonVal *post = canon_parse(post_s, &err);
      if (!post) {
        fprintf(stderr, "PARSE FAIL %s:%ld (post): %s\n", path, g_lineno, err);
        return 3;
      }
      if (post->type != CV_OBJ || post->nkeys != 3 ||
          strcmp(post->keys[0], "bank") != 0 ||
          strcmp(post->keys[1], "bk") != 0 ||
          strcmp(post->keys[2], "rng") != 0) {
        fail("runAI: post envelope is not {bank,bk,rng}");
      }
      const MlAiInput recBank = ai_input_from_canon(post->vals[0]);
      const CanonVal *rng = post->vals[2];
      if (rng->type != CV_ARR) fail("runAI: rng is not a list");

      // artifact cross-check: structural desync is fatal, value
      // disagreement is a divergence (the artifact then drives the chain
      // — a perturbed artifact visibly diverges, never silently wins)
      const MlAiBridgeEntry *e = ml_ai_bridge_peek(&g_bridge);
      if (!e) fail("runAI: bridge artifact exhausted");
      if (e->frame != frame || e->slot != slot) {
        fail("runAI: bridge artifact (frame,slot) desync");
      }
      if (e->ndraws != rng->count) {
        report_div_msg(fni, "artifact draw count != recorded draw count");
      }
      for (int k = 0; k < rng->count && k < e->ndraws; k++) {
        if (rng->items[k]->type != CV_NUM) fail("runAI: rng entry not a number");
        if (!bits_eq(rng->items[k]->num, e->draws[k])) {
          report_div_msg(fni, "artifact draw value != recorded draw value");
          break;
        }
      }
      for (int k = 0; k < AI_BRIDGE_NFIELDS; k++) {
        if (!aiv_eq(e->field[k], *ai_row_field_const(&recBank, k))) {
          report_div_msg(fni, "artifact bank row != recorded bank row");
          break;
        }
      }

      // burn draws + write-set verify + install (the bridge proper)
      const MlAiBridgeApplyResult r =
          ml_ai_bridge_apply(e, &g_seeded, &g_bank[slot]);
      if (r.bad_draw != -1) {
        report_div_msg(fni, "seeded chain draw mismatch inside runAI window");
      }
      if (r.bad_field != -1) {
        char msg[128];
        snprintf(msg, sizeof msg,
                 "never-AI-written field '%s' disagrees with the chain",
                 ai_bridge_field_names[r.bad_field]);
        report_div_msg(fni, msg);
      }
      ml_ai_bridge_advance(&g_bridge);
    } else if (fni == 3) { // pollInputs
      // (gameMode, frameByFrame, mType[i], playerSlot, currentPlayers[i],
      //  keys, playertype) — main.js:678. Ground every argument against
      // the C chain state.
      expect_argc(args, 7);
      if (cv_number_strict(args->items[0]) != g_st.gameMode) {
        fail("pollInputs: gameMode arg != chain state");
      }
      if (cv_bool(args->items[1]) != g_st.frameByFrame) {
        fail("pollInputs: frameByFrame arg != chain state");
      }
      if (args->items[2]->type != CV_STR ||
          strcmp(args->items[2]->str, "keyboard") != 0) {
        fail("pollInputs: mType out of captured domain (not \"keyboard\")");
      }
      const int slot = cv_slot(args->items[3]);
      const double controllerIndex = cv_number_strict(args->items[4]);
      if (args->items[5]->type != CV_OBJ || args->items[5]->nkeys != 0) {
        fail("pollInputs: keys object non-empty (out of captured domain)");
      }
      const double ptype = cv_number_strict(args->items[6]);
      if (ptype != 0 && ptype != 1) fail("pollInputs: playertype out of domain");
      if (g_kind[slot] == -1) {
        g_kind[slot] = (int)ptype;
        g_st.mType[slot] = ML_MTYPE_KEYBOARD; // harnessSetupMatch
        g_st.currentPlayers[slot] = controllerIndex;
      } else if (g_kind[slot] != (int)ptype) {
        fail("pollInputs: playertype changed mid-trace");
      } else if (g_st.currentPlayers[slot] != controllerIndex) {
        fail("pollInputs: controllerIndex changed mid-trace");
      }
      if (g_pending_set[slot]) fail("pollInputs: previous poll never consumed");
      const CanonVal *ret = canon_parse(ret_s, &err);
      if (!ret) {
        fprintf(stderr, "PARSE FAIL %s:%ld (ret): %s\n", path, g_lineno, err);
        return 3;
      }
      if (g_kind[slot] == 1) {
        // the ret IS the bank row at interpretInputs time — rule-18
        // chain-verify against the C bridge's chained row
        g_pending_ai[slot] = ai_input_from_canon(ret);
        out.len = 0; out.buf[0] = 0;
        ai_input_canon(&out, &g_bank[slot]);
        if (strcmp(out.buf, ret_s) != 0) report_div(fni, ret_s, out.buf);
      } else {
        g_pending[slot] = ml_input_from_canon(ret);
        out.len = 0; out.buf[0] = 0;
        ml_input_canon(&out, &g_pending[slot]);
        if (strcmp(out.buf, ret_s) != 0) report_div(fni, ret_s, out.buf);
      }
      g_pending_set[slot] = true;
      g_pending_frame[slot] = frame;
    } else { // physics — projected args [i, inputBuffers[i]]
      expect_argc(args, 2);
      const int slot = cv_slot(args->items[0]);
      // expected side: byte-identical echo of the projected buffer
      exp_buf.len = 0; exp_buf.buf[0] = 0;
      canon_write(&exp_buf, args->items[1]);
      out.len = 0; out.buf[0] = 0;

      if (g_kind[slot] == 1) {
        MlAiInputBuffer built;
        if (g_pending_set[slot]) {
          if (g_pending_frame[slot] != frame) {
            fail("physics: pending pollInputs from a different frame");
          }
          ml_ai_interpret_inputs(&g_st, slot, true, 1,
                                 &g_prev_ai[slot], &g_pending_ai[slot], &built);
          g_pending_set[slot] = false;
          // the pollInputs alias: runAI mutated the bank row between
          // interpretInputs and physics — slot 0 IS that row upstream
          built.slot[0] = g_bank[slot];
        } else {
          ai_null_inputs(&built); // gameTick main.js:919 (starting window)
        }
        ai_buffer_canon(&out, &built);
        if (strcmp(out.buf, exp_buf.buf) != 0) report_div(fni, exp_buf.buf, out.buf);
        g_prev_ai[slot] = built; // the chain continues from the C value
      } else {
        // human slot (or a slot never polled: the starting window covers
        // both kinds identically with fresh null buffers)
        MlInputBuffer built;
        if (g_pending_set[slot]) {
          if (g_pending_frame[slot] != frame) {
            fail("physics: pending pollInputs from a different frame");
          }
          if (g_kind[slot] != 0) fail("physics: poll pending on an unseen slot");
          ml_interpret_inputs(&g_st, slot, true, 0,
                              &g_prev[slot], &g_pending[slot], &built);
          g_pending_set[slot] = false;
        } else {
          nullInputs(&built);
        }
        ml_input_buffer_canon(&out, &built);
        if (strcmp(out.buf, exp_buf.buf) != 0) report_div(fni, exp_buf.buf, out.buf);
        g_prev[slot] = built;
      }
    }
  }
  free(line);
  fclose(f);

  if (!g_boot_seen) fail("capture carries no rngBoot record");
  if (!stop_first_hit && ml_ai_bridge_peek(&g_bridge) != NULL) {
    fail("bridge artifact has unconsumed entries after the capture ended");
  }

  fprintf(stderr, "\nper-function: (replayed/diverged)\n");
  for (int i = 0; i < NFN; i++) {
    if (fn_total[i] > 0) {
      fprintf(stderr, "  %-12s %ld/%ld\n", fn_names[i], fn_total[i], fn_div[i]);
    }
  }
  printf("AI BRIDGE REPLAY RAN %ld records, %ld divergences", replayed, divergences);
  if (first_div_line != -1) printf(" (first at line %ld)", first_div_line);
  printf("\n");
  cb_free(&out);
  cb_free(&exp_buf);
  ml_ai_bridge_free(&g_bridge);
  if (strict && divergences > 0) return 2;
  return 0;
}
