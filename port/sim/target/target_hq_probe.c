// target_hq_probe.c — the standing mechanical probe for the STAGE-DAMAGE
// hq-row CONSUME path (fix_plan §M4 task 11).
//
// MEASURED (iter 94): NO authored stage — VS or target — carries a
// SurfaceProperties damageType, so dealWithDamagingStageCollision's rows
// are IMPOSSIBLE in the whole in-scope domain (builder/custom stages are
// scope-excluded). The task text's "untrap the stage-damage hq rows"
// premise is therefore refuted for authored data; what CAN be proven
// mechanically is the already-translated CONSUME path (hit_detection.c
// executeHits :1154-1176 stage-row arm + executeRegularHit :930-964):
// this probe drives hd_executeHits with the exact synthetic aIsObj row a
// live damaging surface WOULD produce (physics.c:202-203 row shape,
// [i, {normal,angular,corner}, damageTypeIndex, false, false, true]) and
// asserts the upstream-specified effects:
//   - the synthesized constructor-shape hitbox {dmg:10, kg:100, bk:0,
//     sk:150, angle: atan2-normal, type: damageTypeIndex}
//     (hitDetection.js:742-755) lands as percent += 10;
//   - stageDamageImmunity = 20 + the presence flag
//     (hitDetection.js:573 / hit_detection.c:963-964);
//   - hitlag = floor(10/3 + 3) = 6 (the damage-10 constant);
//   - hit.hitPoint = ECBp[angular] (corner=false arm).
// Drop tooth (check-target-sim.sh): --drop skips the row append — every
// assertion fails loudly (exit 1), proving the probe bites.
//
// Usage: target_hq_probe --simdata <s.txt> [--drop]
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../fdlibm/fdlibm.h"
#include "../ml_events.h" // ml_active_rng
#include "target_play.h"

static int g_fail;
static void expect(bool ok, const char *what) {
  if (!ok) {
    fprintf(stderr, "PROBE FAIL: %s\n", what);
    g_fail = 1;
  }
}

int main(int argc, char **argv) {
  const char *simdataPath = 0;
  bool drop = false;
  for (int i = 1; i < argc; i++) {
    const char *a = argv[i];
    if (strcmp(a, "--simdata") == 0 && i + 1 < argc) simdataPath = argv[++i];
    else if (strcmp(a, "--drop") == 0) drop = true;
    else {
      fprintf(stderr, "target_hq_probe: bad argument %s\n", a);
      return 1;
    }
  }
  if (!simdataPath) {
    fprintf(stderr, "usage: target_hq_probe --simdata s.txt [--drop]\n");
    return 1;
  }

  sim_boot_page(&G);
  sim_data_load(simdataPath);
  sim_data_register();

  // Fixed seeded stream (screenShake burns 4 logged draws per regular
  // hit — the probe exercises that chain too).
  ml_active_rng = &G.rng;
  ml_rng_seed(&G.rng, 1337u);

  // A target match: fox on targetstage1 (the probe just needs a present
  // slot-0 player with sane physics + ECBp values).
  tp_setup_target(&G, 2, 0);
  G.frame = 1;
  MlPlayer *p0 = &G.sim.player[0];
  // Give ECBp real components (frame-1 rule-8 undef masks would trap the
  // collisionPoint read — a live row can only occur AFTER physics ran, so
  // real components are the faithful pre-state).
  for (int k = 0; k < 4; k++) {
    p0->phys.ECBp[k] = vec2d(1 + k, 2 + k);
  }
  p0->phys.ecbpUndef = 0;

  const double percent0 = p0->percent;

  // The row physics.c:196-211 WOULD push for damageType "fire" (index 3),
  // routed the way the single upstream hitQueue consumes it.
  if (!drop) {
    HdRow *r = &G.hq.hq[G.hq.hqCount++];
    memset(r, 0, sizeof *r);
    r->v = 0;            // row [i, ...]: the damaged player
    r->aIsObj = true;    // row[1] is the collisionData object
    r->normal = vec2d(0, 1);
    r->angular = 2;      // ECBp index (corner=false arm)
    r->corner = false;
    r->h = 3;            // damageTypeIndex ("fire")
    r->shieldHit = false;
    r->isThrow = false;
    r->drawBounce = true; // row[5] === true (physics.js:73)
    r->hasPhantom = false;
  }

  hd_executeHits(&G.sim, &G.hq);

  // hitDetection.js:742-755 synthesized hitbox: dmg 10 -> percent += 10.
  expect(p0->percent == percent0 + 10, "percent += 10 (synthesized dmg)");
  // hitDetection.js:573: stageDamageImmunity = 20.
  expect(p0->phys.hasStageDamageImmunity &&
             p0->phys.stageDamageImmunity == 20,
         "stageDamageImmunity == 20");
  // hitlag = floor(damage/3 + 3) = 6 (hit_detection.c:927).
  expect(p0->hit.hitlag == 6, "hitlag == floor(10/3 + 3) == 6");
  // corner=false arm: hitPoint = ECBp[angular] (hit_detection.c:955-960).
  expect(p0->hit.hitPoint.x == p0->phys.ECBp[2].x &&
             p0->hit.hitPoint.y == p0->phys.ECBp[2].y,
         "hitPoint == ECBp[angularParameter]");

  if (g_fail) {
    fprintf(stderr, "TARGET HQ PROBE FAIL%s\n", drop ? " (drop arm)" : "");
    return 1;
  }
  printf("TARGET HQ PROBE OK\n");
  return 0;
}
