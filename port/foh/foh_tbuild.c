// port/foh/foh_tbuild.c — A45 T4, the target builder's engine.
// Contract, scope, and WHY this is a separate TU behind a pointer seam:
// foh_tbuild.h. Read that first.
//
// Upstream is `targetBuilderControls` (src/target/targetbuilder.js:159-855).
// Cited per arm below. The three ported tools are 5 TARGET (:560-571),
// 6 MOVE (:572-618) and 7 DELETE (:619-738); tools 0-4 and 8-9 are the
// spike's T5-T8 and are simply not here.
#include "foh_tbuild.h"

#include <stdio.h>
#include <string.h>

#include "../gfx/raster.h"    // gfx_fatal
#include "../sim/ml_js.h"     // js_round (ECMAScript Math.round)
#include "../sim/ml_ser.h"    // ml_sha256_hex
#include "../sim/stage_code.h" // MlkStage, mlk_encode, mlk_parse (A45 T1)
#include "foh_persist.h"      // foh_persist_dir, foh_persist_publish

// The SIM's target cap, restated. `port/sim/target/target_play.h:43` owns
// it (`_Static_assert`-tied to upstream's 10-element `targetDestroyed`
// literal, targetplay.js:37) but that header transitively needs the
// GENERATED ml_stages.h, so it cannot be included from port/foh (measured).
// check-tbuild.sh leg [1] compiles the two together and asserts they are
// EQUAL, so this copy cannot drift silently.
//
// R2 IS AN OPEN OWNER RULING AND THIS IS THE SAFE HALF OF IT. The codec
// holds 20 targets (MLK_MAX_TARGETS, upstream's own targetbuilder.js:563
// cap) and the sim holds 10. JS arrays grow, so a 20-target stage genuinely
// plays upstream. The cap is NOT raised here; the BUILDER refuses the 11th
// at the moment the player presses A, which is where the message is useful
// — the design spike's own words, and A45 T2's note ("the builder (T4)
// should refuse the 11th where the message is useful").
#define FOH_TB_PLAYABLE_TARGETS 10

// --- the document -----------------------------------------------------------
//
// MODULE STATE, exactly like upstream's `export var stageTemp`
// (targetbuilder.js:53-72). It is deliberately NOT a FohState field:
// sizeof(MlkStage) is 45,344 bytes (measured this session) against a 7,224
// byte FohState, and every witness in port/foh declares a FohState by value.
// Its LIFETIME is upstream's too — menu.js:87-90 enters the builder without
// resetting stageTemp, so a second visit keeps the document you left.
static MlkStage g_doc;
static bool g_docReady;

// The previous frame's crosshair, upstream's prevCrossHairPos /
// prevRealCrossHair (:777-778). MOVE's centerItem reads the DELTA for
// surfaces; for targets and starting points it snaps (:1546-1552), so only
// the snap arms are reachable in T4 — the deltas are kept because the
// clamp arms below rewrite the ungridded position and the pair must stay
// consistent frame to frame.
static double g_prevX, g_prevY;

// gridSizes (targetbuilder.js:80). Index 4 is FREE movement.
static const int kGridSizes[5] = {80, 40, 20, 10, 0};
#define FOH_TB_GRIDS 5

// --- DEVIATION D51: a new stage starts from a floor -------------------------
//
// Upstream's fresh `stageTemp` (:53-72) has NO ground, NO ceiling and NO
// walls — you build them with the WALL and POLYGON tools, which are T5 and
// T7. A targets-only editor that started there could only ever produce a
// stage with nothing to stand on: the player would spawn, fall through the
// blastzone and the "stage" would be unplayable. That is not a faithful
// port of a feature, it is a feature that cannot be used.
//
// So a NEW stage starts from a minimal playable template: one ground
// surface, upstream's own four starting points and its own blastzone and
// scale. Everything except the ground line is upstream's literal
// (:62 startingPoint, :64 blastzone, :65 scale). The ground is 200 world
// units wide at y = 0, centred — wide enough to stand and move on at the
// default scale 3, well inside the blastzone.
//
// It is a DEVIATION and it is registered as one (MENU-SPEC D51). When T5/T7
// land the WALL and POLYGON tools the honest thing is to keep it as the
// default and let the player delete it, not to go back to an empty stage.
static void doc_template(MlkStage *st) {
  memset(st, 0, sizeof *st);
  // :62 — `[new Vec2D(-10,0), new Vec2D(10,0), new Vec2D(-30,0), new Vec2D(30,0)]`
  const double spx[4] = {-10.0, 10.0, -30.0, 30.0};
  for (int i = 0; i < 4; i++) {
    st->startingPoint[i].x = spx[i];
    st->startingPoint[i].y = 0.0;
  }
  st->startingPointCount = 4;
  // `hasStartingFace` false: the builder's stageTemp genuinely has no
  // startingFace key, and stage_code.h records that an ABSENT key emits
  // "1,1,1,1" while an EMPTY array emits nothing (encode.js:20-30). False
  // is the builder's own shape.
  st->hasStartingFace = false;
  st->startingFaceCount = 0;
  // D51's one non-upstream line: the floor.
  st->s.ground.items[0].p0.x = -100.0;
  st->s.ground.items[0].p0.y = 0.0;
  st->s.ground.items[0].p1.x = 100.0;
  st->s.ground.items[0].p1.y = 0.0;
  st->s.ground.count = 1;
  // :64 — `new Box2D([-250,-250],[250,250])`
  st->blastzone.min.x = -250.0;
  st->blastzone.min.y = -250.0;
  st->blastzone.max.x = 250.0;
  st->blastzone.max.y = 250.0;
  st->scale = 3.0; // :65
}

// --- the .mlstage file plane ------------------------------------------------
//
// A45 T2's contract (port/sim/target/custom_stage.h), byte for byte:
//   MLSTAGE1\n
//   <share code>\n
//   SUM <64 lowercase hex>\n
// SUM is sha256 over every preceding byte.
//
// THIS IS A SECOND IMPLEMENTATION OF T2's READER AND THAT IS DELIBERATE,
// NOT AN OVERSIGHT. `mlk_slot_load` lives in custom_stage.c, whose header
// transitively includes `sim/sim.h` and therefore the GENERATED ml_stages.h
// — measured this session: port/foh cannot even INCLUDE it, let alone link
// it, without dragging the whole sim into sixteen check scripts. The
// project's answer to "two implementations of one grammar" is the fmt_diff
// discipline: check-tbuild.sh leg [4] runs BOTH over the same corpus of
// files (good, truncated, bad-SUM, over-cap, bad-grammar) and requires them
// to agree on every verdict. A shared body could not be proven to agree;
// this is.
#define TB_FILE_MAX (MLK_CODE_MAX + 128) // == MLK_FILE_MAX (custom_stage.h)
#define TB_NAME_MAX 32

static void slot_name(int slot, char out[TB_NAME_MAX]) {
  if (slot < 0 || slot >= FOH_TB_SLOTS) gfx_fatal("foh_tbuild: slot range");
  snprintf(out, TB_NAME_MAX, "custom%d.mlstage", slot);
}

static bool slot_path(int slot, char *buf, size_t cap) {
  char name[TB_NAME_MAX];
  slot_name(slot, name);
  const int w = snprintf(buf, cap, "%s/%s", foh_persist_dir(), name);
  return w > 0 && (size_t)w < cap;
}

static bool hexdigit(char c) {
  return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
}

// --- playability ------------------------------------------------------------

// A surface carries damage exactly when the SIM says it does. Physics reads
// `wall[2] !== undefined ? wall[2].damageType : null` and tests it for
// TRUTHINESS, so props whose damageType is NULL are inert — and those are
// precisely what upstream BUG 1 emits for the sixth surface of every type
// (encode.js:39's `i !== 5`). Refusing those would reject codes the browser
// plays fine, which is why only a real type string counts. This mirrors
// custom_stage.c's list_has_damage; check-tbuild.sh leg [4] proves the two
// agree on a corpus rather than trusting that they do.
static bool list_has_damage(const SurfaceList *l) {
  for (int k = 0; k < l->count; k++) {
    const Surface *sf = &l->items[k];
    if (sf->hasProps && sf->propsDamageType.tag == DT_STR) return true;
  }
  return false;
}

// THE THREE RULES THE SIM ENFORCES, MIRRORED HERE — not a subset.
//
// This is not belt-and-braces, it closes a CRASH PATH found by the
// differential while building this ticket: the FOH decides what
// target-select draws as a playable custom slot, so a stage this function
// waves through but `mlk_stage_playable` refuses would be shown as present,
// launched by A, and then die inside the sim's loud refusal — a crash where
// the player asked for a game. Whatever the sim will refuse, the FOH must
// refuse first, in words, where the player can act on it.
//
// The rule set is custom_stage.c:34-66 verbatim in substance. The MESSAGES
// are shorter because these ones have to fit on a 240 px line.
static const char *doc_unplayable(const MlkStage *st) {
  // (1) R2 — the codec's 20 against the sim's 10. Refuse, never truncate.
  if (st->targetCount < 1) return "place at least 1 target";
  if (st->targetCount > FOH_TB_PLAYABLE_TARGETS) return "10 targets max";
  // (2) the CONCATENATION cap: five lists that each pass their own 64 cap
  //     can still overflow runCollisionRoutine's single 96-entry list.
  {
    const long total = (long)st->s.wallL.count + st->s.wallR.count +
                       st->s.ground.count + st->s.ceiling.count +
                       st->s.platform.count;
    if (total > ML_MAX_LABELLED_SURFACES) return "too many surfaces";
  }
  // (3) the DAMAGE plane, which has never executed and which no golden
  //     covers. A45 T6 owes that golden; until then the refusal is the
  //     marker, and it must appear at LOAD, not mid-match.
  if (list_has_damage(&st->s.ground) || list_has_damage(&st->s.ceiling) ||
      list_has_damage(&st->s.wallL) || list_has_damage(&st->s.wallR) ||
      list_has_damage(&st->s.platform)) {
    return "damaging surface (unsupported)";
  }
  if (st->startingPointCount < 1) return "no starting point";
  return 0;
}

// Read + verify + parse. True and *out filled on success. On false NOTHING
// usable is in *out and *why names the RULE that refused — never a generic
// "bad file", because the player has to be able to act on it.
//
// VALIDATE ON READ, ALWAYS. The file is user-supplied (he may have copied
// it onto the SD card himself) and this device has a MEASURED power-loss
// risk on a journal-less vfat, so a TRUNCATED file is an expected input,
// not a hypothetical one. The read is BOUNDED (a file over the cap is
// refused unread rather than truncated into a parse), the grammar is exact,
// and SUM is verified BEFORE mlk_parse ever sees a byte.
static bool slot_read(int slot, MlkStage *out, const char **why) {
#define RD_FAIL(m)                                                             \
  do {                                                                         \
    if (why) *why = (m);                                                       \
    if (f) fclose(f);                                                          \
    return false;                                                              \
  } while (0)
  FILE *f = 0;
  char path[512];
  if (!slot_path(slot, path, sizeof path)) RD_FAIL("path too long");
  f = fopen(path, "rb");
  if (!f) RD_FAIL("empty");
  // BOUNDED READ. One byte more than the cap is asked for on purpose: a
  // short read then proves the file fits, without a seek/stat race.
  static char buf[TB_FILE_MAX + 1];
  const size_t n = fread(buf, 1, sizeof buf, f);
  if (ferror(f)) RD_FAIL("unreadable");
  if (n > TB_FILE_MAX) RD_FAIL("file too large");
  fclose(f);
  f = 0;
  // GRAMMAR. Exactly three LF-terminated lines, nothing after the third.
  const char kHdr[] = "MLSTAGE1\n";
  const size_t hdr = sizeof kHdr - 1;
  if (n < hdr || memcmp(buf, kHdr, hdr) != 0) RD_FAIL("not a .mlstage file");
  // The SUM line is the LAST 69 bytes: "SUM " + 64 hex + "\n".
  const size_t sumLen = 4 + 64 + 1;
  if (n < hdr + 1 + sumLen) RD_FAIL("truncated");
  const size_t sumAt = n - sumLen;
  if (memcmp(buf + sumAt, "SUM ", 4) != 0) RD_FAIL("no SUM line");
  for (int i = 0; i < 64; i++) {
    if (!hexdigit(buf[sumAt + 4 + (size_t)i])) RD_FAIL("bad SUM digits");
  }
  if (buf[n - 1] != '\n') RD_FAIL("bad SUM line");
  // The code line is everything between the header and the SUM line, and it
  // must be ONE line: exactly one '\n', at its end.
  const size_t codeAt = hdr, codeLen = sumAt - hdr;
  if (codeLen < 2 || buf[sumAt - 1] != '\n') RD_FAIL("bad code line");
  if (memchr(buf + codeAt, '\n', codeLen - 1) != 0) RD_FAIL("bad code line");
  // SUM BEFORE PARSE. A corrupt file must never reach the parser.
  {
    char hex[65];
    ml_sha256_hex(buf, sumAt, hex);
    if (memcmp(hex, buf + sumAt + 4, 64) != 0) RD_FAIL("SUM mismatch");
  }
  // NUL-terminate the code in place (over its own '\n') and parse.
  buf[sumAt - 1] = '\0';
  if (!mlk_parse(buf + codeAt, out, why)) {
    if (why && !*why) *why = "invalid code";
    return false;
  }
  // AND THE PLAYABILITY RULES, HERE, at the read. A slot that the sim will
  // refuse must never read back as present: target-select draws its custom
  // page from this verdict, and a slot shown as playable that dies at launch
  // is a crash where the player asked for a game.
  {
    const char *bad = doc_unplayable(out);
    if (bad) {
      if (why) *why = bad;
      return false;
    }
  }
  return true;
#undef RD_FAIL
}

// Serialise + publish. The ONLY writer. Goes through foh_persist_publish,
// which is foh_persist_save's own atomic publish generalised for exactly
// this caller (A45 T2's instruction, quoted in foh_tbuild.h).
static bool slot_write(int slot, const MlkStage *st, const char **why) {
  static char code[MLK_CODE_MAX];
  const int cn = mlk_encode(st, code, sizeof code);
  if (cn < 0) {
    if (why) *why = "stage too large to encode";
    return false;
  }
  static char file[TB_FILE_MAX];
  const char kHdr[] = "MLSTAGE1\n";
  const size_t hdr = sizeof kHdr - 1;
  // header + code + '\n' + "SUM " + 64 + '\n'
  const size_t body = hdr + (size_t)cn + 1;
  if (body + 69 > sizeof file) {
    if (why) *why = "stage too large to encode";
    return false;
  }
  memcpy(file, kHdr, hdr);
  memcpy(file + hdr, code, (size_t)cn);
  file[hdr + (size_t)cn] = '\n';
  char hex[65];
  ml_sha256_hex(file, body, hex);
  memcpy(file + body, "SUM ", 4);
  memcpy(file + body + 4, hex, 64);
  file[body + 68] = '\n';
  char name[TB_NAME_MAX];
  slot_name(slot, name);
  const char *pubWhy = 0;
  if (!foh_persist_publish(name, file, body + 69, &pubWhy)) {
    if (why) *why = pubWhy ? pubWhy : "save failed";
    return false;
  }
  // A publish that landed but could not prove its directory entry durable
  // is a SUCCESS with a caveat, never a failure (foh_persist.h). It is not
  // surfaced to the player: the bytes are on the card.
  if (why) *why = 0;
  return true;
}

static void slots_scan(bool present[FOH_TB_SLOTS],
                       const char *reason[FOH_TB_SLOTS]) {
  // BY INDEX, no append, no length cursor — D43. A refused slot is reported
  // AS refused and stays in its own place; nothing shifts up to fill it,
  // which is the whole shape of the owner's ruling.
  static MlkStage scratch; // 45 KB — never on the stack
  for (int i = 0; i < FOH_TB_SLOTS; i++) {
    const char *why = 0;
    present[i] = slot_read(i, &scratch, &why);
    reason[i] = present[i] ? 0 : (why ? why : "empty");
  }
}

// --- crosshair --------------------------------------------------------------

// calculateGriddedCrossHair (targetbuilder.js:134-152), verbatim.
// `600 % gridSize` / `375 % gridSize` are INTEGER remainders upstream
// (both operands are integer literals), so they are integer here too.
static void grid_snap(FohState *s) {
  const int g = kGridSizes[s->tbGrid];
  if (s->tbGrid == 4) { // :135-137 free movement
    s->tbX = s->tbUnX;
    s->tbY = s->tbUnY;
    return;
  }
  const double sc = g_doc.scale;
  if (s->tbUnX == 0.0) {
    s->tbX = (double)(600 % g) / sc;
  } else {
    s->tbX = js_round(s->tbUnX / ((double)g / sc)) * (double)g / sc +
             (double)(600 % g) / sc;
  }
  if (s->tbUnY == 0.0) {
    s->tbY = (double)(375 % g) / sc;
  } else {
    s->tbY = js_round(s->tbUnY / ((double)g / -sc)) * (double)g / -sc +
             (double)(375 % g) / sc;
  }
}

// :171-201. The crosshair is the free hand's motion model in world units:
// integrate the stick, snap to the grid, project to upstream's 1200x750
// canvas, and CLAMP BY REWRITING the ungridded position so the two stay
// consistent (the design spike's own correction — "that is a second
// position, not a second clamp").
//
// `foh_hand.h` supplies the shape this follows (D29, the shared free
// cursor): doubles, integrated per frame from a d-pad whose axes are
// -1/0/+1 (DEVIATION D1), clamped to the logical canvas. It is not CALLED
// here because its clamp is to a rect in RASTER pixels while this one must
// rewrite `unGriddedCrossHairPos` in WORLD units — the same reason foh_hand.h
// itself says hit-testing stays with the caller. Same model, same doubles,
// upstream's own body.
static void crosshair_step(FohState *s, const PlatformInput *in) {
  // :171 — X or Y held is the PRECISION modifier (multi 1 vs 5).
  // DEVIATION D50: upstream reads `y || x`; the port reads X ONLY, freeing
  // Y for the grid cycle, because the FunKey-S has no `z` button (the key
  // upstream's :207 binds the grid to — CLAUDE.md's device keysym list is
  // u/d/l/r, a/b/x/y, s, k/n, q). Upstream's two modifier buttons are
  // redundant with each other, so one of them is free by measurement.
  const double multi = in->x ? 1.0 : 5.0;
  const double lsX = (in->right ? 1.0 : 0.0) - (in->left ? 1.0 : 0.0);
  const double lsY = (in->up ? 1.0 : 0.0) - (in->down ? 1.0 : 0.0);
  const double sc = g_doc.scale;
  s->tbUnX += lsX * multi * 3.0 / sc; // :175
  s->tbUnY += lsY * multi * 3.0 / sc; // :176
  grid_snap(s);
  // :178 realCrossHair, then :182-201's four clamp arms verbatim.
  double rx = s->tbX * sc + 600.0, ry = s->tbY * -sc + 375.0;
  if (rx < 0.0) {
    s->tbUnX = -600.0 / sc;
    grid_snap(s);
  }
  if (rx > 1200.0) {
    s->tbUnX = 600.0 / sc;
    grid_snap(s);
  }
  if (ry > 750.0) {
    s->tbUnY = 375.0 / -sc;
    grid_snap(s);
  }
  if (ry < 0.0) {
    s->tbUnY = -375.0 / -sc;
    grid_snap(s);
  }
}

// findStartingPoint (:1453-1463) then findTarget (:1465-1476) — upstream's
// MOVE hover priority (:582-590), with the polygon/line arms absent because
// T4 has no polygons or lines to hover. Both test the GRIDDED position in
// WORLD units with a +/-5 box, upstream's own `Math.abs(...) <= 5`.
static int hover_find(const FohState *s, bool withStartingPoints) {
  if (withStartingPoints) {
    for (int i = 0; i < g_doc.startingPointCount; i++) {
      const double dx = s->tbX - g_doc.startingPoint[i].x;
      const double dy = s->tbY - g_doc.startingPoint[i].y;
      if ((dx < 0 ? -dx : dx) <= 5.0 && (dy < 0 ? -dy : dy) <= 5.0) {
        return FOH_TB_SP + i;
      }
    }
  }
  for (int i = 0; i < g_doc.targetCount; i++) {
    const double dx = s->tbX - g_doc.target[i].x;
    const double dy = s->tbY - g_doc.target[i].y;
    if ((dx < 0 ? -dx : dx) <= 5.0 && (dy < 0 ? -dy : dy) <= 5.0) return i;
  }
  return FOH_TB_NONE;
}

// centerItem (:1542-1556), the two arms T4 can reach: a starting point and
// a target both SNAP to the crosshair. (The surface/polygon arms offset by
// the frame delta instead; nothing in T4 can hover one.)
static void center_item(const FohState *s, int item) {
  if (item >= FOH_TB_SP) {
    const int i = item - FOH_TB_SP;
    if (i < 0 || i >= g_doc.startingPointCount) return;
    g_doc.startingPoint[i].x = s->tbX;
    g_doc.startingPoint[i].y = s->tbY;
  } else if (item >= 0 && item < g_doc.targetCount) {
    g_doc.target[item].x = s->tbX;
    g_doc.target[item].y = s->tbY;
  }
}

// --- the status line --------------------------------------------------------
//
// EVERY refusal in this screen puts a STRING ON THE SCREEN. The ticket this
// implements exists because a refusal was a `deny` sound the owner could not
// hear, so "played a sound" is not an acceptable way to say no anywhere in
// this file. 120 frames == upstream's own toast life (:224 toolInfoTimer).
static void say(FohState *s, const char *msg) {
  s->tbMsg = msg;
  s->tbMsgTimer = 120;
}

// --- entering ---------------------------------------------------------------

static void tb_enter(FohState *s, int slot) {
  if (!g_docReady) { // first entry this process
    doc_template(&g_doc);
    g_docReady = true;
    s->tbSlot = -1;
  }
  if (slot >= 0) {
    // targetselect.js:113-119's edit arm: resetStageTemp + load + setEditingStage.
    const char *why = 0;
    if (slot_read(slot, &g_doc, &why)) {
      s->tbSlot = slot;
      say(s, "loaded");
    } else {
      doc_template(&g_doc);
      s->tbSlot = -1;
      say(s, why ? why : "could not load");
    }
  }
  // menu.js:87-90 enters with editingStage = -1 and does NOT reset
  // stageTemp, so a re-entry keeps the document. Only the VIEW is reset.
  s->tbPaused = false;
  s->tbPauseRow = 0;
  s->tbPane = FOH_TB_PANE_NONE;
  s->tbPaneRow = 0;
  s->tbHover = FOH_TB_NONE;
  s->tbGrab = FOH_TB_NONE;
  s->tbHoldA = false;
  grid_snap(s);
  g_prevX = s->tbX;
  g_prevY = s->tbY;
}

// --- the pause menu ---------------------------------------------------------
//
// :780-844. Upstream's rows are Test / Save / Quit; ours are
// LOAD / SAVE / DELETE / QUIT (foh_tbuild.h explains both changes). The
// cursor arms are upstream's stick thresholds reduced to d-pad edges (D1).
static void pane_open(FohState *s, int pane) {
  s->tbPane = pane;
  s->tbPaneRow = s->tbSlot >= 0 ? s->tbSlot : 0;
}

static FohTbVerdict step_paused(FohState *s, const PlatformInput *in,
                                const PlatformInput *pv) {
  const bool aE = in->a && !pv->a;
  const bool bE = in->b && !pv->b;
  const bool sE = in->start && !pv->start;
  const bool uE = in->up && !pv->up;
  const bool dE = in->down && !pv->down;

  if (s->tbPane != FOH_TB_PANE_NONE) {
    // the slot list
    if (bE) { // back to the pause rows
      s->tbPane = FOH_TB_PANE_NONE;
      foh_snd_push(s, "menuBack");
      return FOH_TB_STAY;
    }
    if (uE) {
      s->tbPaneRow = (s->tbPaneRow + FOH_TB_SLOTS - 1) % FOH_TB_SLOTS;
      foh_snd_push(s, "menuSelect");
      return FOH_TB_STAY;
    }
    if (dE) {
      s->tbPaneRow = (s->tbPaneRow + 1) % FOH_TB_SLOTS;
      foh_snd_push(s, "menuSelect");
      return FOH_TB_STAY;
    }
    if (!aE) return FOH_TB_STAY;
    const int slot = s->tbPaneRow;
    const char *why = 0;
    switch (s->tbPane) {
      case FOH_TB_PANE_LOAD:
        if (slot_read(slot, &g_doc, &why)) {
          s->tbSlot = slot;
          s->tbHover = FOH_TB_NONE;
          s->tbGrab = FOH_TB_NONE;
          s->tbHoldA = false;
          s->tbPane = FOH_TB_PANE_NONE;
          s->tbPaused = false;
          foh_snd_push(s, "menuForward");
          say(s, "loaded");
        } else {
          foh_snd_push(s, "deny");
          say(s, why ? why : "empty");
        }
        break;
      case FOH_TB_PANE_SAVE: {
        // Refuse BEFORE writing anything a player would then have to
        // discover is unplayable at launch. This is R2's safe half.
        const char *bad = doc_unplayable(&g_doc);
        if (bad) {
          foh_snd_push(s, "deny");
          say(s, bad);
          break;
        }
        if (slot_write(slot, &g_doc, &why)) {
          s->tbSlot = slot;
          s->tbPane = FOH_TB_PANE_NONE;
          s->tbPaused = false;
          foh_snd_push(s, "menuForward");
          say(s, "saved");
        } else {
          // The named publish failure, ON SCREEN: "disk full",
          // "cannot open the temp file", "rename publish failed"...
          foh_snd_push(s, "deny");
          say(s, why ? why : "save failed");
        }
        break;
      }
      case FOH_TB_PANE_DELETE: {
        char path[512];
        if (!slot_path(slot, path, sizeof path)) {
          foh_snd_push(s, "deny");
          say(s, "path too long");
          break;
        }
        // D43: deleting slot i removes slot i's FILE and touches nothing
        // else. Upstream shifts every higher cookie down
        // (targetselect.js:83-97), which is the same clobbering family the
        // owner ruled against — a shift makes "Custom 4" mean a different
        // stage than it did a second ago, and its records with it.
        if (remove(path) == 0) {
          if (s->tbSlot == slot) s->tbSlot = -1;
          foh_snd_push(s, "menuBack");
          say(s, "deleted");
        } else {
          foh_snd_push(s, "deny");
          say(s, "empty");
        }
        break;
      }
      default: break;
    }
    return FOH_TB_STAY;
  }

  // the four pause rows
  if (uE) {
    s->tbPauseRow = (s->tbPauseRow + FOH_TB_PAUSE_ROWS - 1) % FOH_TB_PAUSE_ROWS;
    foh_snd_push(s, "menuSelect"); // :785
    return FOH_TB_STAY;
  }
  if (dE) {
    s->tbPauseRow = (s->tbPauseRow + 1) % FOH_TB_PAUSE_ROWS;
    foh_snd_push(s, "menuSelect"); // :792
    return FOH_TB_STAY;
  }
  if (sE || bE) { // :838-842 — START closes the pause menu (menuBack)
    s->tbPaused = false;
    s->tbPauseRow = 0;
    foh_snd_push(s, "menuBack");
    return FOH_TB_STAY;
  }
  if (!aE) return FOH_TB_STAY;
  switch (s->tbPauseRow) {
    case FOH_TB_PAUSE_LOAD:
      foh_snd_push(s, "menuForward");
      pane_open(s, FOH_TB_PANE_LOAD);
      break;
    case FOH_TB_PAUSE_SAVE:
      foh_snd_push(s, "menuForward"); // :803
      pane_open(s, FOH_TB_PANE_SAVE);
      break;
    case FOH_TB_PAUSE_DELETE:
      foh_snd_push(s, "menuForward");
      pane_open(s, FOH_TB_PANE_DELETE);
      break;
    default: // QUIT — :832-835 changeGamemode(1)
      foh_snd_push(s, "menuForward");
      return FOH_TB_QUIT;
  }
  return FOH_TB_STAY;
}

// --- the editor -------------------------------------------------------------

static FohTbVerdict tb_step(FohState *s, const PlatformInput *in,
                            const PlatformInput *pv) {
  if (s->tbMsgTimer > 0) s->tbMsgTimer--;
  if (s->tbToolTimer > 0) s->tbToolTimer--;
  if (s->tbPaused) return step_paused(s, in, pv);

  const bool aE = in->a && !pv->a;
  const bool bE = in->b && !pv->b;
  const bool sE = in->start && !pv->start;
  const bool yE = in->y && !pv->y;
  const bool lE = in->l && !pv->l;
  const bool rE = in->r && !pv->r;

  // DEVIATION D50: B LEAVES. Upstream's builder has no B exit at all — you
  // leave through START -> Quit (:832-835), which this screen also has. B
  // is added because it is the FOH's universal back edge on every other
  // screen (menu.js:164-190, css.js:186-194, stageselect.js:79,
  // targetselect.js:76-81), and a screen where the back button silently
  // does something else is a trap the owner would find by falling into it.
  // It takes the SAME edge and the SAME sound as Quit.
  if (bE) {
    foh_snd_push(s, "menuBack");
    return FOH_TB_QUIT;
  }
  if (sE) { // :774-777 — START pauses
    s->tbPaused = true;
    s->tbPauseRow = 0;
    s->tbPane = FOH_TB_PANE_NONE;
    s->tbHoldA = false;
    s->tbGrab = FOH_TB_NONE;
    foh_snd_push(s, "pause");
    return FOH_TB_STAY;
  }

  crosshair_step(s, in);

  // :207-212's grid cycle, moved from `z` to Y (D50 — no z button on this
  // hardware; the note is at crosshair_step's `multi`).
  if (yE) {
    s->tbGrid = (s->tbGrid + 1) % FOH_TB_GRIDS;
    grid_snap(s);
    foh_snd_push(s, "menuSelect");
    s->tbToolTimer = 120;
  }
  // :213-230's tool cycle: L back, R forward, both wrapping, both arming
  // the 120-frame name toast. Three tools, not ten (see foh_tbuild.h).
  // Upstream also accepts d-pad left/right here; the port cannot, because
  // the d-pad is the crosshair (the spike's own table says so).
  if (lE) {
    s->tbTool = (s->tbTool + FOH_TB_TOOLS - 1) % FOH_TB_TOOLS;
    foh_snd_push(s, "menuSelect"); // :215
    s->tbToolTimer = 120;          // :219
    s->tbGrab = FOH_TB_NONE;
    s->tbHoldA = false;
  } else if (rE) {
    s->tbTool = (s->tbTool + 1) % FOH_TB_TOOLS;
    foh_snd_push(s, "menuSelect"); // :224
    s->tbToolTimer = 120;
    s->tbGrab = FOH_TB_NONE;
    s->tbHoldA = false;
  }

  s->tbHover = FOH_TB_NONE;
  switch (s->tbTool) {
    case FOH_TB_TOOL_TARGET: // :560-571
      if (aE) {
        // Upstream's cap here is 20 (:563). The PORT's is the SIM's 10 —
        // R2, refused where the message is useful rather than by raising a
        // cap that is _Static_assert-tied to upstream's own 10-element
        // targetDestroyed literal. Upstream plays `deny` and says nothing;
        // this says why, on screen.
        if (g_doc.targetCount < FOH_TB_PLAYABLE_TARGETS) {
          g_doc.target[g_doc.targetCount].x = s->tbX;
          g_doc.target[g_doc.targetCount].y = s->tbY;
          g_doc.targetCount++;
          foh_snd_push(s, "blunthit"); // :566
        } else {
          foh_snd_push(s, "deny"); // :568
          say(s, "10 targets max");
        }
      }
      break;
    case FOH_TB_TOOL_MOVE: // :572-618
      if (s->tbGrab == FOH_TB_NONE) {
        s->tbHover = hover_find(s, true); // :582-583 priority: sp then target
      } else {
        s->tbHover = s->tbGrab; // :589-590
      }
      if (s->tbHover != FOH_TB_NONE) {
        if (!s->tbHoldA) {
          if (aE) { // :594-598 initiate
            center_item(s, s->tbHover);
            s->tbGrab = s->tbHover;
            s->tbHoldA = true;
          }
        } else if (in->a) { // :601-603 moving
          center_item(s, s->tbHover);
        } else { // :604-611 release
          center_item(s, s->tbHover);
          s->tbHoldA = false;
          s->tbGrab = FOH_TB_NONE;
          foh_snd_push(s, "blunthit"); // :609
        }
      }
      break;
    default: // DELETE, :619-738 — the `target` arm (:656-658)
      s->tbHover = hover_find(s, false); // :628 — no starting-point arm
      if (s->tbHover != FOH_TB_NONE && aE) {
        const int i = s->tbHover;
        for (int k = i; k + 1 < g_doc.targetCount; k++) {
          g_doc.target[k] = g_doc.target[k + 1];
        }
        g_doc.targetCount--; // :657 splice
        s->tbHover = FOH_TB_NONE;
        foh_snd_push(s, "menuBack"); // :658
      }
      break;
  }
  g_prevX = s->tbX; // :778-779
  g_prevY = s->tbY;
  return FOH_TB_STAY;
}

// --- the renderer's snapshot ------------------------------------------------

static void tb_view(const FohState *s, FohTbView *out) {
  (void)s;
  memset(out, 0, sizeof *out);
  out->scale = g_doc.scale;
  out->nTarget = g_doc.targetCount;
  for (int i = 0; i < out->nTarget && i < FOH_TB_MAX_TARGETS; i++) {
    out->tx[i] = g_doc.target[i].x;
    out->ty[i] = g_doc.target[i].y;
  }
  out->nSp = g_doc.startingPointCount;
  for (int i = 0; i < out->nSp && i < FOH_TB_MAX_SP; i++) {
    out->spx[i] = g_doc.startingPoint[i].x;
    out->spy[i] = g_doc.startingPoint[i].y;
  }
  // Every collision surface, so the floor the player is placing targets on
  // is actually on screen. T4 edits none of them.
  const SurfaceList *lists[5] = {&g_doc.s.ground, &g_doc.s.ceiling,
                                 &g_doc.s.wallL, &g_doc.s.wallR,
                                 &g_doc.s.platform};
  for (int L = 0; L < 5; L++) {
    for (int i = 0; i < lists[L]->count; i++) {
      if (out->nLine >= FOH_TB_MAX_LINES) break;
      const int k = out->nLine++;
      out->lx0[k] = lists[L]->items[i].p0.x;
      out->ly0[k] = lists[L]->items[i].p0.y;
      out->lx1[k] = lists[L]->items[i].p1.x;
      out->ly1[k] = lists[L]->items[i].p1.y;
    }
  }
  slots_scan(out->present, out->reason);
}

// --- the seam ---------------------------------------------------------------

static const FohTbuildOps kOps = {tb_enter, tb_step, tb_view, slots_scan};

// Constructor-installed, the tp_custom_setup / ml_sim_runai_live shape
// (foh_tbuild.h). A build that does not link this TU leaves the pointer
// NULL and refuses VISIBLY on screen.
__attribute__((constructor)) static void tb_install(void) {
  foh_tbuild_ops = &kOps;
}
