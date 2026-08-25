// port/foh/tbuild/mlstage_probe.c — A45 T4's DIFFERENTIAL other half.
//
// Runs the SIM's OWN, UNMODIFIED `mlk_slot_load` (port/sim/target/
// custom_stage.c, A45 T2) over a .mlstage file and prints its verdict.
// check-tbuild.sh puts this side by side with the builder's reader
// (foh_tbuild.c's slot_read) over the same corpus and requires them to
// agree on every file.
//
// WHY A SECOND BINARY AND NOT ONE WITNESS. `custom_stage.h` transitively
// includes `sim/sim.h`, which needs the GENERATED ml_stages.h, so port/foh
// cannot include it (measured). Linking it into the FOH witness would drag
// the whole sim into a menu check. This probe carries that weight instead,
// and the witness stays a menu binary.
//
// THE THREE SHIMS BELOW ARE LINK SHIMS, NOT STUBS. `mlk_slot_load` shares a
// translation unit with `tp_setup_target_custom`, so the linker pulls the
// whole object and wants three symbols this probe never calls (measured —
// they are the only three). Each ABORTS LOUDLY: a probe that somehow
// reached a setup path fails the check rather than reporting a lie.
#include <stdio.h>
#include <stdlib.h>

#include "target/custom_stage.h"

void sim_fatal(const char *m) {
  fprintf(stderr, "mlstage_probe: sim_fatal reached (%s) — this probe only\n"
                  "loads; reaching a sim path means the differential is\n"
                  "measuring something other than mlk_slot_load\n",
          m ? m : "?");
  abort();
}
void tp_setup_target_core(GameState *g, int charId, double playingId,
                          const MlStageX *stage, const Vec2D *targets,
                          int targetCount, Vec2D startingPoint) {
  (void)g; (void)charId; (void)playingId; (void)stage; (void)targets;
  (void)targetCount; (void)startingPoint;
  sim_fatal("tp_setup_target_core");
}
bool (*tp_custom_setup)(GameState *, int, const char *, int, const char **);

int main(int argc, char **argv) {
  if (argc < 3) {
    fprintf(stderr, "usage: mlstage_probe <dir> <slot>\n");
    return 2;
  }
  MlkStage st;
  const char *why = 0;
  const int slot = atoi(argv[2]);
  if (mlk_slot_load(argv[1], slot, &st, &why)) {
    printf("SIMLOAD OK targets=%d sp=%d ground=%d\n", st.targetCount,
           st.startingPointCount, st.s.ground.count);
    return 0;
  }
  printf("SIMLOAD REFUSED %s\n", why ? why : "?");
  return 1;
}
