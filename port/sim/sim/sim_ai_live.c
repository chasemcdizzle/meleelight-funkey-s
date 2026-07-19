// sim_ai_live.c — M4 task 5: the LIVE C AI driver behind the
// ml_sim_runai_live pointer seam (sim.h).
//
// WHY A SEPARATE TU + POINTER SEAM: port/sim/check-sim.sh (the M2 EXIT
// GATE) is never edited (HARD RULE 3) and its frozen TU list does not
// include port/sim/ai.c — so no TU on that list may reference ml_runAI
// directly. This TU is linked ONLY by builds that also link ai.c
// (check-ai-live.sh's sim_host_live); its constructor installs the seam
// before main, and the pointers live outside GameState so
// sim_boot_page's memset cannot wipe them. The M2-gate binary keeps a
// NULL seam: bridge-only, behavior preserved bit-for-bit.
//
// MlAiSim POPULATION (AGENT-LOG iter-75 task-5 notes, followed exactly):
// - player[k]: the LIVE &G.sim.player[k] — runAI's private bookkeeping
//   writes (currentAction/currentSubaction/lastMash) land on the same
//   objects upstream mutates; none are on the checksum surface.
// - cS/playerType: copies of the god-module globals (constant mid-match).
// - turbo: gameSettings.turbo by TRUTHINESS (MlSim carries the bool; the
//   live harness domain is Number("")-zeroed cookies -> 0 -> false).
// - multiJump[k]: CTAB1 ml_attributes[cS[k]].multiJump (M1 table plane —
//   the ai.h header note's "task-5 integration wires the CTAB1 lookup").
//   Every boot player carries charAttributes, so never undefined here.
// - aS: an MlAiStage VIEW rebuilt per call — ledgePos from STAB1
//   (upstream never mutates aS.ledgePos) but ground/platform from the
//   LIVE stage plane (G.sim.stage.s), so movingPlatforms' per-frame
//   platform writes (ystory/fountain) are visible exactly as upstream's
//   live activeStage object is.
// - bank: the full 4x8 tagged plane (GameState.bank; ai.js:357 reads
//   [i][1] — a page-boot inputData() row, never written).
// - curentAction (the ai.js:1254 upstream TYPO field, write-only): slice
//   state persisting across frames — lives in the static MlAiSim here
//   (one match per process, upstream: one page per run).
//
// RNG: ml_runAI draws through the logged ml_random() on ml_active_rng —
// the SAME seeded chain the whole sim runs on. No bridge draw-burn.
#include <stdio.h>

#include "../ai.h"
#include "ml_tables.h" // CTAB1 (generated; -I pipeline build dir)
#include "sim.h"

void ml_ai_out_of_domain(const char *what) { sim_fatal(what); }

static MlAiSim g_ai;       // persistent: curentAction slice state
static MlAiStage g_stage;  // the per-call activeStage view
static bool g_ai_bound;

// Rebuild the aS view from STAB1 (static ledges) + the live stage plane.
static void ai_stage_refresh(GameState *g) {
  const int stageId = (int)g->stageSelect;
  if (stageId < 0 || stageId >= ML_STAGE_COUNT) {
    sim_fatal("ai live: stageSelect outside the STAB1 domain");
  }
  const ml_stage_t *st = &ml_stages[stageId];
  if (st->ledgeCount > ML_AI_MAX_LEDGE) {
    sim_fatal("ai live: ledgePos count over the MlAiStage cap");
  }
  g_stage.ledgePosLen = st->ledgeCount;
  for (int j = 0; j < st->ledgeCount; j++) {
    g_stage.ledgePos[j].x = ml_stage_f64(st->ledgePos[j].x);
    g_stage.ledgePos[j].y = ml_stage_f64(st->ledgePos[j].y);
  }
  const Stage *s = &g->sim.stage.s;
  if (s->ground.count > ML_AI_MAX_SURF ||
      s->platform.count > ML_AI_MAX_SURF) {
    sim_fatal("ai live: surface count over the MlAiStage cap");
  }
  g_stage.groundLen = s->ground.count;
  for (int j = 0; j < s->ground.count; j++) {
    g_stage.ground[j][0] = s->ground.items[j].p0;
    g_stage.ground[j][1] = s->ground.items[j].p1;
  }
  g_stage.platformLen = s->platform.count;
  for (int j = 0; j < s->platform.count; j++) {
    g_stage.platform[j][0] = s->platform.items[j].p0;
    g_stage.platform[j][1] = s->platform.items[j].p1;
  }
}

static void runai_live(GameState *g, int i) {
  if (!g_ai_bound) {
    for (int k = 0; k < 4; k++) g_ai.player[k] = &g->sim.player[k];
    g_ai.bank = g->bank;
    g_ai.aS = &g_stage;
    g_ai_bound = true;
  }
  for (int k = 0; k < 4; k++) {
    g_ai.playerType[k] = g->sim.playerType[k];
    g_ai.cS[k] = g->sim.characterSelections[k];
    const int c = (int)g->sim.characterSelections[k];
    if (c < 0 || c >= ML_CHARS) {
      sim_fatal("ai live: characterSelections outside 0..4");
    }
    g_ai.multiJump[k] = ml_attributes[c].multiJump != 0;
    g_ai.multiJumpUndef[k] = false; // every boot player has charAttributes
  }
  g_ai.turbo = g->sim.turbo; // truthiness (header note)
  ai_stage_refresh(g);
  ml_runAI(&g_ai, i);
}

// --ai-cover (sim_main): the ml_ai_cov arm table, stderr, post-run.
// Diagnostic only — never on the judged stdout stream.
static void cov_dump(void) {
  for (int k = 0; k < ML_AI_NCOV; k++) {
    fprintf(stderr, "AICOV %s %ld\n", ml_ai_cov_names[k], ml_ai_cov[k]);
  }
  fprintf(stderr, "AICOV-END\n");
}

__attribute__((constructor)) static void sim_ai_live_install(void) {
  ml_sim_runai_live = runai_live;
  ml_sim_ai_cov_dump = cov_dump;
}
