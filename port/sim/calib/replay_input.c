// replay_input.c — M2 task 3 replay driver: feeds recorded input-cluster
// boundary calls (port/sim/calib/build/<id>.input.jsonl, FORMAT.md spec
// "input") to the C translations and compares canon-v1.1 serializations
// byte-for-byte. A single differing bit anywhere is a divergence.
//
// Two record families:
// - pure records (meleeInputs sweep + live deaden/nullInputs/inputData/
//   nullInput): marshal args -> call C -> compare ret.
// - the interpretInputs CHAIN: pollInputs records inject the per-frame
//   polled input; each physics record's projected args [i, inputBuffers[i]]
//   is the buffer interpretInputs produced for slot i this frame — the
//   driver runs the C interpretInputs state machine on ITS OWN chained
//   buffers (prev frame's C output, never the capture's) and compares the
//   produced 8-deep buffer, so the full-trace recurrence (z/s always-shift,
//   pause-aware pastOffset, pause/frameAdvance bookkeeping, end-of-tick
//   frameByFrame handling) must hold bit-exactly over all 3600 frames.
//   Frames with no pollInputs record for a slot replay the 'starting'
//   window's fresh nullInputs() (gameTick main.js:919).
//
// Usage: replay_input <capture.jsonl> [--strict] [--max-print N]
//                     [--only-fn NAME] [--stop-first]
//
// Marshalling is STRICT (prevention rule 7): any argument shape outside
// the captured domain aborts with exit 3 — never guess.
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

// --- marshalling (canon tree -> C values) ---------------------------------

static double cv_number_strict(const CanonVal *v) {
  if (v->type != CV_NUM) fail("expected number (undef out of domain here)");
  return v->num;
}

static bool cv_bool(const CanonVal *v) {
  if (v->type != CV_BOOL) fail("expected boolean");
  return v->b;
}

// StickCardinals | null | undefined (sorted keys: center,down,left,right,up)
static MaybeCardinals cv_cardinals(const CanonVal *v) {
  MaybeCardinals out;
  if (v->type == CV_NULL || v->type == CV_UNDEF) {
    out.present = false;
    memset(&out.c, 0, sizeof out.c);
    return out;
  }
  if (v->type != CV_OBJ || v->nkeys != 5 ||
      strcmp(v->keys[0], "center") != 0 || strcmp(v->keys[1], "down") != 0 ||
      strcmp(v->keys[2], "left") != 0 || strcmp(v->keys[3], "right") != 0 ||
      strcmp(v->keys[4], "up") != 0) {
    fail("expected StickCardinals {center,down,left,right,up}");
  }
  const CanonVal *c = v->vals[0];
  if (c->type != CV_OBJ || c->nkeys != 2 ||
      strcmp(c->keys[0], "x") != 0 || strcmp(c->keys[1], "y") != 0) {
    fail("expected cardinals center {x,y}");
  }
  out.present = true;
  out.c.center = vec2d(cv_number_strict(c->vals[0]),
                       cv_number_strict(c->vals[1]));
  out.c.down = cv_number_strict(v->vals[1]);
  out.c.left = cv_number_strict(v->vals[2]);
  out.c.right = cv_number_strict(v->vals[3]);
  out.c.up = cv_number_strict(v->vals[4]);
  return out;
}

// the positional 18-list (12 booleans then 6 numbers, ml_input.h)
static MlInputList cv_input_list(const CanonVal *v) {
  if (v->type != CV_ARR || v->count != 18) fail("expected 18-element InputList");
  MlInputList l;
  for (int i = 0; i < 12; i++) l.flags[i] = cv_bool(v->items[i]);
  for (int i = 0; i < 6; i++) l.nums[i] = cv_number_strict(v->items[12 + i]);
  return l;
}

// --- serialization helpers --------------------------------------------------

static void ser_numpair(CanonBuf *b, NumPair p) {
  cb_putc(b, '[');
  cb_num(b, p.a);
  cb_putc(b, ',');
  cb_num(b, p.b);
  cb_putc(b, ']');
}

// generic echo writer (replay_util.c pattern): re-serialize a parsed canon
// tree byte-identically (numbers survive bit-exactly — parse and emit are
// both bit-pattern hex).
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

// --- interpretInputs chain state ---------------------------------------------

static MlInputSimState g_st;
static MlInputBuffer g_prev[4];      // oldInputBuffers[i] — the C chain
static bool g_slot_seen[4];          // mType/currentPlayers initialized
static MlInput g_pending[4];         // polled input awaiting physics(i)
static bool g_pending_set[4];
static double g_pending_ptype[4];
static long g_pending_frame[4];
static long g_cur_frame = 0;

static void chain_init(void) {
  ml_input_sim_state_init(&g_st);
  // match setup slice (harnessSetupMatch + startGame, the capture domain):
  // changeGamemode(3), playing = true; per-slot mType/currentPlayers are
  // applied lazily from each slot's first pollInputs record.
  g_st.gameMode = 3;
  g_st.playing = true;
  for (int i = 0; i < 4; i++) {
    nullInputs(&g_prev[i]);
    g_slot_seen[i] = false;
    g_pending_set[i] = false;
  }
}

static void advance_frame(long frame) {
  if (frame < g_cur_frame) fail("frame order broken");
  // mode-3 end-of-tick bookkeeping for every completed sim frame
  // (main.js:1087-1091); frame 0 records are pre-match setup/sweep.
  for (long f = g_cur_frame; f < frame; f++) {
    if (f >= 1) ml_input_end_of_tick(&g_st);
  }
  g_cur_frame = frame;
}

// --- dispatch ---------------------------------------------------------------

static void expect_argc(const CanonVal *args, int n) {
  if (args->type != CV_ARR || args->count != n) fail("bad argument count");
}

static int cv_slot(const CanonVal *v) {
  const double d = cv_number_strict(v);
  const int i = (int)d;
  if (d != (double)i || i < 0 || i > 3) fail("slot index out of range");
  return i;
}

static void dispatch(const char *fn, long frame, const CanonVal *args,
                     CanonBuf *out) {
  // --- meleeInputs -----------------------------------------------------------
  if (strcmp(fn, "deaden") == 0) {
    if (args->type != CV_ARR || (args->count != 1 && args->count != 2)) {
      fail("deaden: expected 1 or 2 args");
    }
    const double dead = args->count == 2 ? cv_number_strict(args->items[1])
                                         : ml_deadzoneConst();
    cb_num(out, deaden(cv_number_strict(args->items[0]), dead));
  } else if (strcmp(fn, "meleeRound") == 0) {
    expect_argc(args, 1);
    cb_num(out, meleeRound(cv_number_strict(args->items[0])));
  } else if (strcmp(fn, "tasRescale") == 0) {
    if (args->type != CV_ARR || (args->count != 2 && args->count != 3)) {
      fail("tasRescale: expected 2 or 3 args");
    }
    if (args->count == 3) cv_bool(args->items[2]); // isDeadzoned: ignored
                                                   // upstream, shape-checked
    ser_numpair(out, tasRescale(cv_number_strict(args->items[0]),
                                cv_number_strict(args->items[1])));
  } else if (strcmp(fn, "scaleToGCTrigger") == 0) {
    expect_argc(args, 3);
    cb_num(out, scaleToGCTrigger(cv_number_strict(args->items[0]),
                                 cv_number_strict(args->items[1]),
                                 cv_number_strict(args->items[2])));
  } else if (strcmp(fn, "scaleToUnitAxes") == 0) {
    expect_argc(args, 5);
    ser_numpair(out, scaleToUnitAxes(cv_number_strict(args->items[0]),
                                     cv_number_strict(args->items[1]),
                                     cv_cardinals(args->items[2]),
                                     cv_number_strict(args->items[3]),
                                     cv_number_strict(args->items[4])));
  } else if (strcmp(fn, "scaleToMeleeAxes") == 0) {
    if (args->type != CV_ARR || (args->count != 4 && args->count != 6)) {
      fail("scaleToMeleeAxes: expected 4 or 6 args");
    }
    const double ccx =
        args->count == 6 ? cv_number_strict(args->items[4]) : 0; // JS defaults
    const double ccy =
        args->count == 6 ? cv_number_strict(args->items[5]) : 0;
    ser_numpair(out, scaleToMeleeAxes(cv_number_strict(args->items[0]),
                                      cv_number_strict(args->items[1]),
                                      cv_bool(args->items[2]),
                                      cv_cardinals(args->items[3]), ccx, ccy));
  }
  // --- input record constructors ---------------------------------------------
  else if (strcmp(fn, "inputData") == 0) {
    if (args->type != CV_ARR || (args->count != 0 && args->count != 1)) {
      fail("inputData: expected 0 or 1 args");
    }
    const MlInputList l = args->count == 1 ? cv_input_list(args->items[0])
                                           : ml_default_input_list();
    const MlInput in = inputData(&l);
    ml_input_canon(out, &in);
  } else if (strcmp(fn, "nullInput") == 0) {
    expect_argc(args, 0);
    const MlInput in = nullInput();
    ml_input_canon(out, &in);
  } else if (strcmp(fn, "nullInputs") == 0) {
    expect_argc(args, 0);
    MlInputBuffer buf;
    nullInputs(&buf);
    ml_input_buffer_canon(out, &buf);
  }
  // --- the interpretInputs chain ---------------------------------------------
  else if (strcmp(fn, "pollInputs") == 0) {
    // (gameMode, frameByFrame, mType[i], playerSlot, currentPlayers[i],
    //  keys, playertype) — main.js:678. Ground every argument against the
    // C chain state; the RET becomes the injected input for this
    // (frame, slot) and its C canon must echo the record byte-exactly
    // (marshal<->serialize round trip).
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
    if (ptype != 0) fail("pollInputs: playertype != 0 (AI bridge is task 16)");
    if (!g_slot_seen[slot]) {
      g_st.mType[slot] = ML_MTYPE_KEYBOARD; // harnessSetupMatch
      g_st.currentPlayers[slot] = controllerIndex;
      g_slot_seen[slot] = true;
    } else if (g_st.currentPlayers[slot] != controllerIndex) {
      fail("pollInputs: controllerIndex changed mid-trace");
    }
    if (g_pending_set[slot]) fail("pollInputs: previous poll never consumed");
    g_pending_set[slot] = true; // the caller marshals the ret into g_pending
    g_pending_ptype[slot] = ptype;
    g_pending_frame[slot] = frame;
    // ret marshal happens in the caller (needs the ret tree)
    (void)out;
  } else if (strcmp(fn, "physics") == 0) {
    // projected args [i, inputBuffers[i]] (spec-input.js): the buffer slot
    // i received this frame = interpretInputs' return, or the fresh
    // nullInputs() of the 'starting' window.
    expect_argc(args, 2);
    const int slot = cv_slot(args->items[0]);
    MlInputBuffer built;
    if (g_pending_set[slot]) {
      if (g_pending_frame[slot] != frame) {
        fail("physics: pending pollInputs from a different frame");
      }
      ml_interpret_inputs(&g_st, slot, true, g_pending_ptype[slot],
                          &g_prev[slot], &g_pending[slot], &built);
      g_pending_set[slot] = false;
    } else {
      nullInputs(&built); // gameTick main.js:919 (starting window)
    }
    ml_input_buffer_canon(out, &built);
    g_prev[slot] = built; // the chain continues from the C value
  } else {
    fail("unknown boundary function");
  }
}

// --- main --------------------------------------------------------------------

#define NFN 11
static const char *fn_names[NFN] = {
  "deaden", "meleeRound", "tasRescale", "scaleToGCTrigger",
  "scaleToUnitAxes", "scaleToMeleeAxes",
  "inputData", "nullInput", "nullInputs",
  "pollInputs", "physics",
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
    fprintf(stderr, "usage: replay_input <capture.jsonl> [--strict] "
                    "[--max-print N] [--only-fn NAME] [--stop-first]\n");
    return 1;
  }
  if (only_fn) {
    // the chain records (pollInputs/physics) are stateful — filtering is
    // only sound for the pure families
    if (strcmp(only_fn, "pollInputs") == 0 || strcmp(only_fn, "physics") == 0) {
      fprintf(stderr, "--only-fn cannot isolate chain records\n");
      return 1;
    }
  }
  FILE *f = fopen(path, "r");
  if (!f) { fprintf(stderr, "cannot open %s\n", path); return 1; }
  g_file = path;

  chain_init();

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
    const char *frame_s = line;
    const char *fn = tab1 + 1;
    const char *args_s = tab2 + 1;
    const char *ret_s = tab3 + 1;
    const long frame = strtol(frame_s, NULL, 10);

    int fni = -1;
    for (int i = 0; i < NFN; i++) {
      if (strcmp(fn, fn_names[i]) == 0) { fni = i; break; }
    }
    if (fni == -1) fail("unknown function name in record");

    const bool is_chain = strcmp(fn, "pollInputs") == 0 ||
                          strcmp(fn, "physics") == 0;
    if (only_fn && strcmp(fn, only_fn) != 0 && !is_chain) continue;

    advance_frame(frame);

    canon_arena_reset();
    const char *err = NULL;
    const CanonVal *args = canon_parse(args_s, &err);
    if (!args) {
      fprintf(stderr, "PARSE FAIL %s:%ld: %s\n", path, g_lineno, err);
      return 3;
    }

    const char *expected = ret_s;
    out.len = 0;
    out.buf[0] = 0;

    if (strcmp(fn, "pollInputs") == 0) {
      // stateful: dispatch grounds the args, then the ret is marshaled as
      // the injected input and its C canon must echo the record
      dispatch(fn, frame, args, &out);
      const CanonVal *ret = canon_parse(ret_s, &err);
      if (!ret) {
        fprintf(stderr, "PARSE FAIL %s:%ld (ret): %s\n", path, g_lineno, err);
        return 3;
      }
      const int slot = cv_slot(args->items[3]);
      g_pending[slot] = ml_input_from_canon(ret);
      ml_input_canon(&out, &g_pending[slot]);
    } else if (strcmp(fn, "physics") == 0) {
      // expected side: the projected buffer's canon, re-emitted from the
      // parsed tree (byte-identical echo); got side: the C chain's buffer
      static CanonBuf exp_buf;
      static bool exp_init = false;
      if (!exp_init) { cb_init(&exp_buf); exp_init = true; }
      exp_buf.len = 0;
      exp_buf.buf[0] = 0;
      canon_write(&exp_buf, args->items[1]);
      expected = exp_buf.buf;
      dispatch(fn, frame, args, &out);
    } else {
      dispatch(fn, frame, args, &out);
    }

    replayed++;
    fn_total[fni]++;

    if (strcmp(out.buf, expected) != 0) {
      divergences++;
      fn_div[fni]++;
      if (first_div_line == -1) first_div_line = g_lineno;
      if (printed < max_print) {
        printed++;
        size_t d = 0;
        while (out.buf[d] && expected[d] && out.buf[d] == expected[d]) d++;
        fprintf(stderr,
                "DIVERGENCE line %ld frame %s fn %s (first diff at byte %zu)\n"
                "  expected: %.300s\n"
                "  got:      %.300s\n"
                "  context:  ...%.80s VS ...%.80s\n",
                g_lineno, frame_s, fn, d, expected, out.buf,
                expected + (d > 40 ? d - 40 : 0),
                out.buf + (d > 40 ? d - 40 : 0));
      }
      if (stop_first) break;
    }
  }
  free(line);
  fclose(f);

  fprintf(stderr, "\nper-function: (replayed/diverged)\n");
  for (int i = 0; i < NFN; i++) {
    if (fn_total[i] > 0) {
      fprintf(stderr, "  %-20s %ld/%ld\n", fn_names[i], fn_total[i], fn_div[i]);
    }
  }
  printf("INPUT REPLAY RAN %ld records, %ld divergences", replayed, divergences);
  if (first_div_line != -1) printf(" (first at line %ld)", first_div_line);
  printf("\n");
  cb_free(&out);
  if (strict && divergences > 0) return 2;
  return 0;
}
