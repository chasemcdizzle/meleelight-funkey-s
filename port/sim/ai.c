// ai.c — structure-parallel C port of upstream src/main/ai.js @ 27af171
// (M4 task 4; verified record-by-record by port/sim/calib/replay_ai_port.c
// against the aiport captures over g07/g08 — see calib/FORMAT.md "The
// aiport spec"). Every function, expression shape, evaluation order and
// literal value tag is carried verbatim (fix_plan §M2 rules 6/13/16);
// upstream typos and dead arms are carried AS TYPOS with a `q<N>` comment
// tag (AGENT-LOG iter 75 pre-registration's quirk registry).
//
// Line references `// ai.js:NN` are to the upstream pin.
#include <math.h>
#include <string.h>

#include "../fdlibm/fdlibm.h"
#include "ai.h"
#include "ml_events.h" // ml_random (logged, chained on ml_active_rng)
#include "ml_js.h"

// --- coverage instrument (Tier-B diagnostic; ai.h) --------------------------
long ml_ai_cov[ML_AI_NCOV];
#define COV_LIST(X)                                                          \
  X(GEN_GRABRELEASE)      /* ai.js:73  (inner idle-list arm dead) */         \
  X(GEN_CATCHWAIT)        /* ai.js:79  */                                    \
  X(GEN_DROPTHRU_CLEAR)   /* ai.js:95  */                                    \
  X(GEN_DROPTHRU_HOLD)    /* ai.js:98  */                                    \
  X(GEN_SUB_LR_CLEAR)     /* ai.js:105 */                                    \
  X(GEN_RUNOFF)           /* ai.js:109 */                                    \
  X(GEN_RUNOFF_WALK)      /* ai.js:118 */                                    \
  X(GEN_RUNOFF_SMASHTURN) /* ai.js:122 */                                    \
  X(GEN_RUNOFF_AIR)       /* ai.js:130/140 airborne fastfall arms */         \
  X(GEN_SUB_UPTILT_CLEAR) /* ai.js:150 */                                    \
  X(GEN_PLATDROP)         /* ai.js:156 (above-platform draw block) */        \
  X(GEN_SHIELD_CLEAR)     /* ai.js:192 */                                    \
  X(GEN_SHIELD_DO)        /* ai.js:196 CPUShield consumer */                 \
  X(GEN_LEDGESTALL_CLEAR) /* ai.js:212 */                                    \
  X(GEN_TW_CLEAR)         /* ai.js:228 */                                    \
  X(GEN_LEDGEDASH_CLEAR)  /* ai.js:235 */                                    \
  X(GEN_LEDGEDASH_DO)     /* ai.js:238 CPULedge consumer */                  \
  X(GEN_SDI)              /* ai.js:254 */                                    \
  X(GEN_RECOVER)          /* ai.js:262 */                                    \
  X(GEN_RUT_CLEAR)        /* ai.js:271 */                                    \
  X(GEN_RUT_REVERSE)      /* ai.js:275 */                                    \
  X(GEN_RUT_UPTILT)       /* ai.js:279 */                                    \
  X(GEN_MASHING_WAIT)     /* ai.js:289 */                                    \
  X(GEN_SMASHTURN_CLEAR)  /* ai.js:295 */                                    \
  X(GEN_TECH_CLEAR)       /* ai.js:300 */                                    \
  X(GEN_DMGFALL_ACT)      /* ai.js:309 hitstun<=0 draw arm */                \
  X(GEN_DMGFALL_TECH)     /* ai.js:326 CPUTech consumer */                   \
  X(GEN_DOWNWAIT)         /* ai.js:340 CPUMissedTech consumer */             \
  X(GEN_CAPTURE_MASH)     /* ai.js:349 */                                    \
  X(GEN_CAPTURE_BURST)    /* ai.js:353 lastMash rollover arms */             \
  X(GEN_WAVESHINE_DO)     /* ai.js:372 */                                    \
  X(GEN_GRABREL_DO)       /* ai.js:384 CPUGrabRelease consumer */            \
  X(GEN_MASHING_TYPOARM)  /* ai.js:401 (q2: `==` comparison statement) */    \
  X(GEN_HITSTUN_CLEAR)    /* ai.js:405 */                                    \
  X(GEN_REBIRTHWAIT)      /* ai.js:408 */                                    \
  X(GEN_LEDGE_DO)         /* ai.js:411 CPULedge consumer */                  \
  X(GEN_WALK)             /* ai.js:438 walk-toward-enemy */                  \
  X(MARTH_LEDGESTALL)     /* ai.js:464 */                                    \
  X(MARTH_TURN)           /* ai.js:496 SMASHTURN arm */                      \
  X(MARTH_TILT)           /* ai.js:502 tilt/grab draw block */               \
  X(MARTH_REACT)          /* ai.js:545 pdiff>=3 block */                     \
  X(JIGGS_TURN)           /* ai.js:595 */                                    \
  X(JIGGS_TILT)           /* ai.js:608 */                                    \
  X(JIGGS_REACT)          /* ai.js:652 */                                    \
  X(FOX_LEDGESTALL)       /* ai.js:691 */                                    \
  X(FOX_RESPAWN_TRIGGER)  /* ai.js:724 */                                    \
  X(FOX_RESPAWN_MACHINE)  /* ai.js:738 */                                    \
  X(FOX_RESPAWN_INARR)    /* ai.js:772 (q4 `in`-array arm) */                \
  X(FOX_SHDL_TRIGGER)     /* ai.js:793 */                                    \
  X(FOX_TURN)             /* ai.js:788 */                                    \
  X(FOX_TILT)             /* ai.js:801 */                                    \
  X(FOX_REACT)            /* ai.js:836 */                                    \
  X(FOX_SHDL_DO)          /* ai.js:860 CPUSHDL consumer */                   \
  X(H_TECH_TRY)           /* ai.js:1002 CPUTech tech-window arm */           \
  X(H_WAVESHINE_KNEEBEND) /* ai.js:1070 */                                   \
  X(H_GRABREL_FOX)        /* ai.js:1103 */                                   \
  X(H_GRABREL_OTHER)      /* ai.js:1128 */                                   \
  X(H_SDI_EVEN)           /* ai.js:1164 (q5 NaN-theta arm) */                \
  X(H_SDI_ODD)            /* ai.js:1178 */                                   \
  X(H_SHIELD_DOSOMETHING) /* ai.js:1213 */                                   \
  X(H_LEDGE_MAIN_DRAW)    /* ai.js:1258 CPULedge NONE draw */                \
  X(H_LEDGE_CTA)          /* ai.js:1254 curentAction typo write */           \
  X(H_RECOVER_FOX_CHARGE) /* ai.js:1465 */                                   \
  X(H_RECOVER_JUMPARM)    /* ai.js:1521 falling jump/up-b arms */
enum {
#define X(n) AI_C_##n,
  COV_LIST(X)
#undef X
  AI_C__COUNT
};
const char *const ml_ai_cov_names[ML_AI_NCOV] = {
#define X(n) #n,
    COV_LIST(X)
#undef X
};
#define AI_COV(n) (ml_ai_cov[AI_C_##n]++)

// --- small helpers ---------------------------------------------------------

// ToNumber(undefined) — the canonical quiet NaN (prevention rule 2).
static double ai_undef_num(void) { return nan(""); }

static bool ai_in(const char *s, const char *const *list, int n) {
  for (int k = 0; k < n; k++) {
    if (strcmp(s, list[k]) == 0) return true;
  }
  return false;
}

static void ai_strcpy(char *dst, const char *src) {
  if (strlen(src) >= ML_STR_CAP) ml_ai_out_of_domain("ai string overflow");
  strcpy(dst, src);
}

// charAttributes.multiJump (table plane; ai.h note). An undefined
// charAttributes would be an upstream TypeError — trap, never guess.
static bool ai_multiJump(const MlAiSim *sim, int p) {
  if (sim->multiJumpUndef[p]) {
    ml_ai_out_of_domain("charAttributes.multiJump read while undefined");
  }
  return sim->multiJump[p];
}

// ai.js:772 `player[i].currentSubaction in ["LASER1","LASER2","REVERSE"]`
// (q4): JS `in` on an ARRAY tests property keys, not membership — own keys
// are the indices "0","1","2" and "length". ai.js only ever writes
// action-ish subaction strings, so this is false across the whole write
// domain; prototype-chain property names (push/map/...) are likewise
// outside it. Own-key semantics modeled; carried verbatim.
static bool ai_in_array3(const char *s) {
  return strcmp(s, "0") == 0 || strcmp(s, "1") == 0 || strcmp(s, "2") == 0 ||
         strcmp(s, "length") == 0;
}

// --- geometry helpers (ai.js:888-973) --------------------------------------

// ai.js:888 NearestLedge(cpu) — returns the closest ledgePos entry (JS
// returns the live Vec2D reference; callers only read .x/.y — by value).
static Vec2D ai_NearestLedge(const MlAiSim *sim, const MlPlayer *cpu) {
  double closest0 = 0, closest1 = 10000; // :889 closest = [0, 10000]
  for (int i = 0; i < sim->aS->ledgePosLen; i++) { // :890
    const double closeness =
        fabs(cpu->phys.pos.x - sim->aS->ledgePos[i].x) +
        fabs(cpu->phys.pos.y - sim->aS->ledgePos[i].y); // :891
    if (closeness < closest1) { // :892
      closest0 = i;
      closest1 = closeness;
    }
  }
  return sim->aS->ledgePos[(int)closest0]; // :897
}

// ai.js:901 NearestFloor(cpu)
static double ai_NearestFloor(const MlAiSim *sim, const MlPlayer *cpu) {
  double nearestDist = 1000; // :903
  double nearestY = -1000;   // :904
  for (int i = 0; i < sim->aS->platformLen; i++) { // :905
    if (cpu->phys.pos.y > sim->aS->platform[i][0].y &&
        cpu->phys.pos.x >= sim->aS->platform[i][0].x &&
        cpu->phys.pos.x <= sim->aS->platform[i][1].x) { // :907
      if (cpu->phys.pos.y - sim->aS->platform[i][0].y < nearestDist) { // :909
        nearestDist = cpu->phys.pos.y - sim->aS->platform[i][0].y;
        nearestY = sim->aS->platform[i][0].y;
      }
    }
  }
  for (int i = 0; i < sim->aS->groundLen; i++) { // :915
    if (cpu->phys.pos.y > sim->aS->ground[i][0].y &&
        cpu->phys.pos.x >= sim->aS->ground[i][0].x &&
        cpu->phys.pos.x <= sim->aS->ground[i][1].x) { // :917
      if (cpu->phys.pos.y - sim->aS->ground[i][0].y < nearestDist) { // :919
        nearestDist = cpu->phys.pos.y - sim->aS->ground[i][0].y;
        nearestY = sim->aS->ground[i][0].y;
      }
    }
  }
  return nearestY; // :925
}

// ai.js:928 isAboveGround(x, y) -> [bool, "none"|"ground"|"platform", y]
typedef struct {
  bool above;       // returnValue[0]
  const char *kind; // returnValue[1]
  double y;         // returnValue[2]
} AiAboveGround;

static AiAboveGround ai_isAboveGround(const MlAiSim *sim, double x,
                                      double y) {
  AiAboveGround rv = {false, "none", 0}; // :929
  double closest = 1000;                 // :930
  double dist;                           // :931
  for (int i = 0; i < sim->aS->groundLen; i++) { // :932
    if (x >= sim->aS->ground[i][0].x && x <= sim->aS->ground[i][1].x &&
        y >= sim->aS->ground[i][0].y) { // :933
      dist = y - sim->aS->ground[i][0].y;
      if (dist < closest) {
        closest = dist;
        rv.above = true;
        rv.kind = "ground";
        rv.y = sim->aS->ground[i][0].y; // :937
      }
    }
  }
  for (int i = 0; i < sim->aS->platformLen; i++) { // :941
    if (x >= sim->aS->platform[i][0].x && x <= sim->aS->platform[i][1].x &&
        y >= sim->aS->platform[i][0].y) { // :942
      dist = y - sim->aS->platform[i][0].y;
      if (dist < closest) {
        closest = dist;
        rv.above = true;
        rv.kind = "platform";
        rv.y = sim->aS->platform[i][0].y; // :946
      }
    }
  }
  return rv; // :950
}

// ai.js:953 window.isOffstage (browser-global fn; module-level def).
// NOTE the verbatim shape: a GROUNDED cpu skips the loops and returns
// true — callers guard with !grounded where it matters.
static bool ai_isOffstage(const MlAiSim *sim, const MlPlayer *cpu) {
  if (cpu->phys.onLedge > -1) return false; // :955
  if (!cpu->phys.grounded) {                // :958
    for (int i = 0; i < sim->aS->groundLen; i++) { // :959
      if (cpu->phys.pos.x >= sim->aS->ground[i][0].x &&
          cpu->phys.pos.x <= sim->aS->ground[i][1].x &&
          cpu->phys.ECBp[0].y >= sim->aS->ground[i][0].y) { // :960
        return false;
      }
    }
    for (int i = 0; i < sim->aS->platformLen; i++) { // :965
      if (cpu->phys.pos.x >= sim->aS->platform[i][0].x &&
          cpu->phys.pos.x <= sim->aS->platform[i][1].x &&
          cpu->phys.ECBp[0].y >= sim->aS->platform[i][0].y) { // :966
        return false;
      }
    }
  }
  return true; // :972
}

// ai.js:8 NearestEnemy(cpu, p) — returns the slot index (JS number; only
// ever used to index player[]).
static int ai_NearestEnemy(const MlAiSim *sim, const MlPlayer *cpu, int p) {
  int nearestEnemy = -1;          // :9
  double enemyDistance = 100000;  // :10
  for (int i = 0; i < 4; i++) {   // :11
    if (sim->playerType[i] > -1) { // :12
      if (sim->playerType[i] > -1 && i != p &&
          strcmp(sim->player[i]->actionState, "SLEEP") != 0) { // :13
        if (i != p) { // :14
          const double dist =
              fd_pow(cpu->phys.pos.x - sim->player[i]->phys.pos.x, 2) +
              fd_pow(cpu->phys.pos.y - sim->player[i]->phys.pos.y, 2); // :15
          if (dist < enemyDistance) { // :17
            enemyDistance = dist;
            nearestEnemy = i;
          }
        }
      }
    }
  }
  if (nearestEnemy == -1) { // :25
    nearestEnemy = 0;       // :26 (console.log fail-safe noise dropped)
  }
  return nearestEnemy; // :30
}

// ai.js:880 isEnemyApproaching(cpu, player)
static bool ai_isEnemyApproaching(const MlPlayer *cpu,
                                  const MlPlayer *player) {
  if (fabs(cpu->phys.pos.x -
           (player->phys.pos.x + player->phys.cVel.x)) <
      fabs(cpu->phys.pos.x - player->phys.pos.x)) { // :881
    return true;
  }
  return false; // :884
}

// --- CPU* helper return literals (each mirrors its upstream object
// literal's exact key set and value types) ---------------------------------

typedef struct { bool x, b; } AiSHDLRet;                    // ai.js:975
typedef struct {                                            // ai.js:994
  double lsX;
  bool l;
  double lAnalog;
  bool hasLA;   // `.lA` is runtime-ADDED by the arms (:1009 etc.);
  double lA;    // consumer reads only lsX/l — carried for shape fidelity
} AiTechRet;
typedef struct { double lsX, lsY; bool a; } AiMissedTechRet; // ai.js:1028
typedef struct { double lsX, lsY; bool x, b, l; } AiWaveshineRet; // :1054
typedef struct {                                            // ai.js:1092
  double lsX, lsY;
  bool x, b, l;
  double csX, csY;
  bool a;
} AiGrabRet;
typedef struct { double lsX, lsY; } AiSDIRet;               // ai.js:1160
typedef struct {                                            // ai.js:1199
  double lsX, lsY;
  bool x, b, a, l;
  double csX, csY;
} AiShieldRet;
typedef struct {                                            // ai.js:1239
  double lsX, lsY;
  bool x, b, l;
  double csX, csY;
  bool a;
} AiLedgeRet;
typedef struct { double lsX, lsY; bool x, b; } AiRecoverRet; // ai.js:1456

// --- CPUSHDL (ai.js:974-992) -----------------------------------------------
static AiSHDLRet ai_CPUSHDL(MlAiSim *sim, MlPlayer *cpu, int p) {
  (void)sim;
  (void)p;
  AiSHDLRet returnInput = {false, false}; // :975
  if (strcmp(cpu->actionState, "WAIT") == 0 ||
      strcmp(cpu->actionState, "DASH") == 0 ||
      (strcmp(cpu->actionState, "LANDING") == 0 && cpu->timer > 3)) { // :979
    returnInput.x = true;
  } else if (strcmp(cpu->actionState, "KNEEBEND") == 0 &&
             cpu->timer >= 3) { // :981
    returnInput.b = true;
    ai_strcpy(cpu->currentSubaction, "LASER2"); // :983
  } else {
    if (cpu->timer == 10) { // :985
      returnInput.b = true;
      ai_strcpy(cpu->currentSubaction, "NONE"); // :987
      ai_strcpy(cpu->currentAction, "NONE");    // :988
    }
  }
  return returnInput; // :991
}

// --- CPUTech (ai.js:993-1026) -----------------------------------------------
static AiTechRet ai_CPUTech(MlAiSim *sim, MlPlayer *cpu, int p) {
  (void)p;
  AiTechRet returnInput = {0.0, false, 0.0, false, 0}; // :994 {lsX,l,lAnalog}
  if (cpu->phys.pos.y - ai_NearestFloor(sim, cpu) <= 3 &&
      cpu->phys.kVel.y + cpu->phys.cVel.y <= 0) { // :1002
    AI_COV(H_TECH_TRY);
    const double MissedTechPercent = 85 - (cpu->difficulty * 20); // :1004
    const double randomSeed =
        floor((ml_random() * (100 + MissedTechPercent)) + 1); // :1005
    if (randomSeed <= 34) { // :1006 inplace
      returnInput.lsX = 0.0;
      returnInput.l = true;
      returnInput.hasLA = true;
      returnInput.lA = 1.0; // :1009 (runtime-added key)
    } else if (randomSeed <= 67) { // :1011 roll left
      returnInput.l = true;
      returnInput.lsX = -1.0;
      returnInput.hasLA = true;
      returnInput.lA = 1.0;
    } else if (randomSeed <= 100) { // :1016 roll right
      returnInput.l = true;
      returnInput.lsX = 1.0;
      returnInput.hasLA = true;
      returnInput.lA = 1.0;
    } // otherwise miss tech
  }
  return returnInput; // :1024
}

// --- CPUMissedTech (ai.js:1027-1052) ----------------------------------------
static AiMissedTechRet ai_CPUMissedTech(MlAiSim *sim, MlPlayer *cpu, int p) {
  (void)sim;
  (void)cpu;
  (void)p;
  AiMissedTechRet returnInput = {0.0, 0.0, false}; // :1028
  const double randomSeed = floor((ml_random() * 10) + 1); // :1034
  if (randomSeed <= 2) { // :1037 getup attack
    returnInput.a = true;
  } else if (randomSeed <= 4) { // :1040 roll
    const double randomSeeds = floor((ml_random() * 2) + 1); // :1041
    if (randomSeeds == 1) { // :1042 left
      returnInput.lsX = -1.0;
    } else { // right
      returnInput.lsX = 1.0;
    }
  } else if (randomSeed <= 6) { // :1047 getup
    returnInput.lsY = 1.0;
  } // else do nothing
  return returnInput; // :1051
}

// --- CPUWaveshineAny (ai.js:1053-1090) ---------------------------------------
static AiWaveshineRet ai_CPUWaveshineAny(MlAiSim *sim, MlPlayer *cpu, int p) {
  (void)sim;
  (void)p;
  AiWaveshineRet returnInput = {0.0, 0.0, false, false, false}; // :1054
  if (strcmp(cpu->actionState, "WAIT") == 0) { // :1062
    returnInput.lsY = -1.0;
    returnInput.b = true;
  }
  if (strcmp(cpu->actionState, "DOWNSPECIALGROUND") == 0) { // :1066
    if (cpu->timer == 4) { // :1067
      returnInput.x = true;
    }
  } else if (strcmp(cpu->actionState, "KNEEBEND") == 0 &&
             (cpu->timer == 3)) { // :1070
    AI_COV(H_WAVESHINE_KNEEBEND);
    const double randomSeed = floor((ml_random() * 3) + 1); // :1071
    if (randomSeed == 1) { // :1072 forward
      returnInput.lsX = cpu->phys.face * 0.75;
      returnInput.lsY = -1.0;
      returnInput.l = true;
      ai_strcpy(cpu->currentAction, "NONE"); // :1076
    } else if (randomSeed == 2) { // :1077 in place
      returnInput.lsX = 0;
      returnInput.lsY = -1.0;
      returnInput.l = true;
      ai_strcpy(cpu->currentAction, "NONE");
    } else { // :1082 backwards
      returnInput.lsX = cpu->phys.face * -0.75;
      returnInput.lsY = -1.0;
      returnInput.l = true;
      ai_strcpy(cpu->currentAction, "NONE");
    }
  }
  return returnInput; // :1089
}

// --- CPUGrabRelease (ai.js:1091-1154) ----------------------------------------
static AiGrabRet ai_CPUGrabRelease(MlAiSim *sim, MlPlayer *cpu, int p) {
  AiGrabRet returnInput = {0.0, 0.0, false, false, false, 0.0, 0.0, false}; // :1092
  if (strcmp(cpu->actionState, "WAIT") == 0 ||
      strcmp(cpu->actionState, "CAPTURECUT") == 0) { // :1102
    if (sim->cS[p] == 2) { // :1103 is fox
      AI_COV(H_GRABREL_FOX);
      const double randomSeed = floor((ml_random() * 125) + 1); // :1104
      if (randomSeed < 4) { // :1105 waveshine
        returnInput.b = true;
        returnInput.lsY = -1.0;
        ai_strcpy(cpu->currentAction, "WAVESHINEANY"); // :1108
        return returnInput;
      } else if (randomSeed < 45) { // :1110 jab
        returnInput.a = true;
      } else if (randomSeed == 85) { // :1113 roll
        returnInput.l = true;
        const double randomSeed1 = floor((ml_random() * 3) + 1); // :1115
        if (randomSeed1 == 1) {
          returnInput.csX = 1.0;
        } else if (randomSeed1 == 2) {
          returnInput.csY = -1.0;
        } else {
          returnInput.csX = -1.0;
        }
      } else if (randomSeed <= 125) { // :1124 jump
        returnInput.x = true;
      }
    } else { // :1128 all other characters
      AI_COV(H_GRABREL_OTHER);
      const double randomSeed = floor((ml_random() * 5) + 1); // :1129
      if (randomSeed == 1) { // :1130 f-smash
        returnInput.csX = cpu->phys.face;
      } else if (randomSeed == 2) { // :1133 jab
        returnInput.a = true;
      } else if (randomSeed == 3) { // :1136 roll
        returnInput.l = true;
        const double randomSeed1 = floor((ml_random() * 3) + 1); // :1138
        if (randomSeed1 == 1) {
          returnInput.csX = 1.0;
        } else if (randomSeed1 == 2) {
          returnInput.csY = -1.0;
        } else {
          returnInput.csX = -1.0;
        }
      } else if (randomSeed == 4) { // :1147 jump
        returnInput.x = true;
      }
    }
  }
  return returnInput; // :1153
}

// --- CPUSDItoStage (ai.js:1158-1196) -----------------------------------------
static AiSDIRet ai_CPUSDItoStage(MlAiSim *sim, MlPlayer *cpu, int p) {
  (void)p;
  const Vec2D closest = ai_NearestLedge(sim, cpu); // :1159
  AiSDIRet returnInput = {0.0, 0.0};               // :1160
  if (fmod(cpu->timer, 2) == 0) { // :1164 cpu.timer % 2 == 0
    AI_COV(H_SDI_EVEN);
    // :1165 `+ imperfection` reads the hoisted-but-UNASSIGNED var (the
    // `var imperfection = 0` lives in the else branch, :1179): undefined
    // -> ToNumber NaN (q5). theta/newX/newY are NaN; the generalAI
    // consumer's isNaN guard maps them to 0. Verbatim.
    const double theta =
        fd_atan(((closest.y - 3.5) - cpu->phys.pos.y) /
                (closest.x - cpu->phys.pos.x)) +
        ai_undef_num(); // :1165
    double newX = fd_cos(theta); // :1166
    double newY = fd_sin(theta); // :1167
    if (closest.x < cpu->phys.pos.x) { // :1168
      newX *= -1;
      newY *= -1;
    }
    // :1173 dont go past 1.0 or -1.0
    newX = js_sign(newX) * js_min(1.0, fabs(newX));
    newY = js_sign(newY) * js_min(1.0, fabs(newY));
    returnInput.lsX = newX; // :1176
    returnInput.lsY = newY;
  } else {
    AI_COV(H_SDI_ODD);
    const double imperfection = 0; // :1179
    double theta = fd_atan(((closest.y - 3.5) - cpu->phys.pos.y) /
                           (closest.x - cpu->phys.pos.x)) +
                   imperfection; // :1180
    theta = theta + 0.25 * ((floor((ml_random() * 2) + 1) - 1) * -1.0) *
                        js_pi(); // :1181
    double newX = fd_cos(theta); // :1182
    double newY = fd_sin(theta); // :1183
    if (closest.x < cpu->phys.pos.x) { // :1184
      newX *= -1;
      newY *= -1;
    }
    newX = js_sign(newX) * js_min(1.0, fabs(newX)); // :1189
    newY = js_sign(newY) * js_min(1.0, fabs(newY)); // :1190
    returnInput.lsX = newX; // :1192
    returnInput.lsY = newY;
  }
  return returnInput; // :1195
}

// --- CPUShield (ai.js:1198-1236) ---------------------------------------------
static AiShieldRet ai_CPUShield(MlAiSim *sim, MlPlayer *cpu, int p) {
  (void)p;
  AiShieldRet returnInput = {0.0, 0.0, false, false, false, true,
                             0.0, 0.0}; // :1199 (l: true)
  const double doSomethingChance =
      js_min(100, 25 * fd_tan((js_pi() / 121) *
                              (60 - cpu->phys.shieldHP))); // :1210
  const double randomSeed = floor((ml_random() * 100) + 1); // :1212
  if (randomSeed <= doSomethingChance) { // :1213 do something
    AI_COV(H_SHIELD_DOSOMETHING);
    returnInput.l = false; // :1214
    const double extra = js_max(0, 15 - cpu->difficulty); // :1215
    const double randomSeed2 =
        floor((ml_random() * 30) + 1) + extra; // :1216 (JS re-var)
    if (randomSeed2 <= 30) { // :1217 jump or shield drop
      // :1218 isAboveGround(pos.x, pos.x) — the y argument is pos.x
      // (upstream typo q7, verbatim)
      const AiAboveGround ag =
          ai_isAboveGround(sim, cpu->phys.pos.x, cpu->phys.pos.x);
      if (strcmp(ag.kind, "platform") == 0 && cpu->difficulty >= 3) {
        // :1219 can shield drop
        const double randomSeed3 =
            floor((ml_random() * 2) + 1) + extra; // :1220
        if (randomSeed3 != 1) { // :1221 shield drop
          returnInput.lsY = -0.66;
          ai_strcpy(cpu->currentAction, "NONE"); // :1223
        } else {
          returnInput.x = true;
          ai_strcpy(cpu->currentAction, "NONE"); // :1226
        }
      } else {
        returnInput.x = true;
        ai_strcpy(cpu->currentAction, "NONE"); // :1230
      }
    }
  }
  return returnInput; // :1235
}

// --- CPULedge (ai.js:1237-1446) ----------------------------------------------
static AiLedgeRet ai_CPULedge(MlAiSim *sim, MlPlayer *cpu, int p) {
  AiLedgeRet returnInput = {0.0, 0.0, false, false, false, 0.0, 0.0,
                            false}; // :1239
  if (strcmp(cpu->actionState, "LANDINGFALLSPECIAL") == 0 &&
      strcmp(cpu->currentAction, "LEDGEDASH") == 0) { // :1249
    ai_strcpy(cpu->currentAction, "NONE");
    return returnInput; // :1251
  } else if (strcmp(cpu->currentAction, "TOURNAMENTWINNER") == 0) { // :1252
    if (strcmp(cpu->actionState, "FALLAERIAL") == 0) { // :1253
      // :1254 `cpu.curentAction = "NONE"` — the upstream TYPO field
      // (write-only; nothing anywhere reads it). Modeled on the sim
      // slice. This arm is measured-DEAD upstream (:228 clears
      // TOURNAMENTWINNER unless paction CLIFF*, :1253 needs FALLAERIAL
      // — contradiction; coverage arm H_LEDGE_CTA pinned ZERO by the
      // check's --cover-gate). T4-as-registered was therefore
      // undischargeable and was REFUTED (AGENT-LOG iter 75): the cta
      // plane is witnessed by tooth T4a (cta-serializer perturbation →
      // 3663 divergences, every runAI record) and the q2 comparison-typo
      // arm by tooth T4b (1 divergence via the sweep MASHING preset).
      AI_COV(H_LEDGE_CTA);
      sim->hasCurentAction[p] = true;
      ai_strcpy(sim->curentAction[p], "NONE");
    }
  }
  if (strcmp(cpu->currentAction, "NONE") == 0) { // :1257
    AI_COV(H_LEDGE_MAIN_DRAW);
    const double randomSeed = floor((ml_random() * 30) + 1); // :1258
    if (randomSeed <= 3) { // :1261 normal getup
      ai_strcpy(cpu->currentAction, "LEDGEGETUP");
      returnInput.lsX = cpu->phys.face;
    } else if (randomSeed <= 5) { // :1264 getup roll
      ai_strcpy(cpu->currentAction, "LEDGEROLL");
      returnInput.l = true;
    } else if (randomSeed <= 8) { // :1267 getup attack
      ai_strcpy(cpu->currentAction, "LEDGEATTACK");
      returnInput.a = true;
    } else if (randomSeed <= 9) { // :1270 tournament winner
      ai_strcpy(cpu->currentAction, "TOURNAMENTWINNER");
      returnInput.lsY = 1.0;
      // :1273-1283 ledge jump / ledgedash arms are commented out upstream
    } else if (randomSeed <= 20) { // :1284 ledgeairattack
      if (sim->player[p]->difficulty > 1) { // :1285
        ai_strcpy(cpu->currentAction, "LEDGEAIRATTACK");
        returnInput.lsY = -1.0;
      }
    } else if (randomSeed <= 22) { // :1289 ledgestall
      if (sim->player[p]->difficulty >= 1) { // :1290
        if (sim->cS[p] != 1) { // :1291
          ai_strcpy(cpu->currentAction, "LEDGESTALL");
          ai_strcpy(cpu->currentSubaction, "FALL");
          returnInput.lsY = -1.0;
        }
      }
    } // else does nothing
  } else if (strcmp(cpu->currentAction, "LEDGEDASH") == 0) { // :1298
    if (sim->cS[p] == 0) { // :1302 is marth
      if (cpu->timer == 18) { // :1323
        returnInput.lsX = cpu->phys.face;
        returnInput.lsY = -1.0;
        returnInput.l = true;
      } else {
        returnInput.x = true; // :1328
        returnInput.lsX = cpu->phys.face;
      }
    } else if (sim->cS[p] == 1) { // :1331 is jiggs
      if (cpu->timer == 6 &&
          strcmp(cpu->actionState, "JUMPAERIAL1") == 0) { // :1332
        returnInput.lsX = cpu->phys.face;
        returnInput.lsY = -1.0;
        returnInput.l = true;
      } else {
        returnInput.x = true;
        returnInput.lsX = cpu->phys.face;
      }
    } else if (sim->cS[p] == 2) { // :1340 is fox
      if (cpu->timer == 5) { // :1341
        returnInput.lsX = cpu->phys.face;
        returnInput.lsY = -1.0;
        returnInput.l = true;
      } else {
        returnInput.x = true;
        returnInput.lsX = cpu->phys.face;
      }
    }
  } else if (strcmp(cpu->currentAction, "LEDGEJUMP") == 0) { // :1350
    if (cpu->phys.grounded) { // :1351
      returnInput.lsX = 0;
      ai_strcpy(cpu->currentAction, "NONE");
    } else {
      if (strcmp(cpu->actionState, "FALL") == 0) { // :1355
        returnInput.x = true;
      }
      returnInput.lsX = cpu->phys.face; // :1358
    }
  } else if (strcmp(cpu->currentAction, "LEDGEAIRATTACK") == 0) { // :1360
    if (sim->cS[p] == 0) { // :1361 marth
      if (cpu->timer == 1) { // :1362
        returnInput.x = true;
      } else if (cpu->timer == 3) { // :1364
        const double randomSeed = floor((ml_random() * 4) + 1); // :1365
        returnInput.lsX = cpu->phys.face;
        if (randomSeed <= 2) { // :1367 fair
          returnInput.csX = cpu->phys.face;
        } else if (randomSeed == 3) { // :1369 nair
          returnInput.lsX = 0;
          returnInput.a = true;
        } else { // uair
          returnInput.csY = 1.0;
        }
        ai_strcpy(cpu->currentAction, "LEDGEAIRATTACK2"); // :1375
      } else {
        returnInput.lsX = cpu->phys.face; // :1377
      }
    } else if (sim->cS[p] == 1) { // :1379 puff
      if (cpu->timer == 1) {
        returnInput.x = true;
      } else if (cpu->timer == 3) { // :1382
        const double randomSeed = floor((ml_random() * 4) + 1); // :1383
        returnInput.lsX = cpu->phys.face;
        if (randomSeed <= 2) { // fair
          returnInput.csX = cpu->phys.face;
        } else if (randomSeed == 3) { // nair
          returnInput.lsX = 0;
          returnInput.a = true;
        } else { // uair
          returnInput.csY = 1.0;
        }
        ai_strcpy(cpu->currentAction, "LEDGEAIRATTACK2"); // :1393
      } else {
        returnInput.lsX = cpu->phys.face;
      }
    } else if (sim->cS[p] == 2) { // :1397 fox
      if (cpu->timer == 3) {
        returnInput.x = true;
      } else if (cpu->timer == 6) { // :1400
        const double randomSeed = floor((ml_random() * 4) + 1); // :1401
        returnInput.csX = 0.0; // :1402
        returnInput.a = false;
        returnInput.lsX = cpu->phys.face;
        if (randomSeed <= 2) { // :1405 nair
          returnInput.lsX = 0;
          returnInput.a = true;
        } else if (randomSeed == 3) { // dair
          returnInput.csY = -1.0;
        } else { // uair
          returnInput.csY = 1.0;
        }
        ai_strcpy(cpu->currentAction, "LEDGEAIRATTACK2"); // :1413
      } else {
        returnInput.lsX = cpu->phys.face;
      }
    }
  } else if (strcmp(cpu->currentAction, "LEDGEAIRATTACK2") == 0) { // :1418
    returnInput.lsX = cpu->phys.face; // :1419
    // :1421 l cancel
    if (strcmp(cpu->actionState, "ATTACKAIRN") == 0 ||
        strcmp(cpu->actionState, "ATTACKAIRF") == 0 ||
        strcmp(cpu->actionState, "ATTACKAIRB") == 0 ||
        strcmp(cpu->actionState, "ATTACKAIRU") == 0 ||
        strcmp(cpu->actionState, "ATTACKAIRD") == 0) {
      if (!ai_isOffstage(sim, cpu)) { // :1423
        if (cpu->phys.pos.y - ai_NearestFloor(sim, cpu) <= 5) { // :1424
          returnInput.l = true; // :1426
        }
        if (cpu->phys.cVel.y <= 0) { // :1428
          if (!(cpu->phys.fastfalled)) { // :1429
            if (cpu->phys.pos.y - ai_NearestFloor(sim, cpu) >= 0) { // :1430
              returnInput.lsY = -1.0;
            }
          }
        }
      }
    }
    if (cpu->phys.grounded || cpu->phys.onLedge > -1) { // :1441
      ai_strcpy(cpu->currentAction, "NONE");
    }
  }
  return returnInput; // :1445
}

// --- CPUrecover (ai.js:1451-1575) --------------------------------------------
static AiRecoverRet ai_CPUrecover(MlAiSim *sim, MlPlayer *cpu, int p) {
  const Vec2D closest = ai_NearestLedge(sim, cpu); // :1454
  // :1455 `var returnInput = [0.0, 0.0, false, false]` immediately
  // overwritten by the object literal (:1456) — the array is dead.
  AiRecoverRet returnInput = {0.0, 0.0, false, false}; // :1456
  if (sim->cS[p] == 2) { // :1463 fox
    // :1464 perfect imperfect firefox angles
    if (strcmp(cpu->actionState, "UPSPECIALCHARGE") == 0) { // :1465
      AI_COV(H_RECOVER_FOX_CHARGE);
      returnInput.lsX = 0.0;
      returnInput.lsY = 0.0;
      if ((cpu->timer >= 40 && cpu->timer <= 43)) { // :1468
        const double imperfection = 0; // :1470
        const double theta =
            fd_atan(((closest.y - 3.5) - cpu->phys.pos.y) /
                    (closest.x - cpu->phys.pos.x)) +
            imperfection; // :1471
        double newX = fd_cos(theta); // :1472
        double newY = fd_sin(theta); // :1473
        if (closest.x < cpu->phys.pos.x) { // :1474
          newX *= -1;
          newY *= -1;
        }
        newX = js_sign(newX) * js_min(1.0, fabs(newX)); // :1479
        newY = js_sign(newY) * js_min(1.0, fabs(newY)); // :1480
        returnInput.lsX = newX;
        returnInput.lsY = newY;
        return returnInput; // :1484
      }
    } else if (strcmp(cpu->actionState, "UPSPECIALLAUNCH") == 0) { // :1486
      returnInput.lsX = 0.0;
      returnInput.lsY = 0.0;
      return returnInput; // :1489
    }
  }
  if (strncmp(cpu->actionState, "JUMP", 4) == 0 ||
      strcmp(cpu->actionState, "FALLAERIAL") == 0 ||
      strcmp(cpu->actionState, "DAMAGEFALL") == 0 ||
      strcmp(cpu->actionState, "FALL") == 0 ||
      strcmp(cpu->actionState, "FALLSPECIAL") == 0) { // :1492
    // :1494 not in up-b or some shit
    if (cpu->phys.pos.x < closest.x) { // :1495
      returnInput.lsX = 1.0;
    } else if (cpu->phys.pos.x > closest.x) { // :1497
      returnInput.lsX = -1.0;
    }
    if (sim->cS[p] == 0 &&
        ((fabs(closest.x - cpu->phys.pos.x) > 25) &&
         (!cpu->phys.doubleJumped ||
          (cpu->phys.jumpsUsed < 5 && ai_multiJump(sim, p))) &&
         ((closest.y - cpu->phys.pos.y < 5) ||
          ((closest.y - cpu->phys.pos.y < 30 &&
            fabs(closest.x - cpu->phys.pos.x) > 40))))) { // :1500
      // :1503 side-b
      if (fabs(cpu->phys.cVel.x) > 0.8) { // :1505
        if (cpu->phys.pos.x < closest.x) { // :1506
          returnInput.lsX = 1.0;
        } else if (cpu->phys.pos.x > closest.x) {
          returnInput.lsX = -1.0;
        } else {
          returnInput.lsX = 0.0;
        }
        if (!(strncmp(cpu->actionState, "SPECIAL", 7) == 0)) { // :1513
          returnInput.lsY = 0.0; // :1515
          returnInput.b = true;
          return returnInput; // :1517
        }
      }
    } else { // :1520
      if (cpu->phys.cVel.y <= 0 &&
          ((closest.y - cpu->phys.pos.y > 10 &&
            (fabs(closest.x - cpu->phys.pos.x) > 25)) ||
           (closest.y - cpu->phys.pos.y > 25 &&
            (fabs(closest.x - cpu->phys.pos.x) <= 25)))) { // :1521 is falling
        AI_COV(H_RECOVER_JUMPARM);
        if (!cpu->phys.doubleJumped ||
            (cpu->phys.jumpsUsed < 5 && ai_multiJump(sim, p))) { // :1523
          double randomSeed = floor((ml_random() * 1000) + 1); // :1524
          if (randomSeed <= 300) { // :1526 will jump
            returnInput.x = true;
          } else if (randomSeed <= 301) { // :1528 will up-b
            if (sim->cS[p] != 1) { // :1529 not jigglypuff
              returnInput.lsX = 0.0;
              returnInput.lsY = 1.0;
              returnInput.b = true;
            }
          }
        } else {
          if (sim->cS[p] == 0) { // :1536 is marth
            if ((fabs(closest.x - cpu->phys.pos.x) <= 20 &&
                 closest.y - cpu->phys.pos.y > 30) ||
                closest.y - cpu->phys.pos.y > 60) { // :1537
              returnInput.lsY = 1.0;
              returnInput.b = true;
            } // else moves towards ledge
          }
          if (sim->cS[p] == 2) { // :1543 is fox
            if ((fabs(closest.y - cpu->phys.pos.y) <= 10) &&
                (fabs(closest.x - cpu->phys.pos.x) >= 30 &&
                 fabs(closest.x - cpu->phys.pos.x) <= 77)) { // :1544 side-b?
              const double randomSeed2 =
                  floor((ml_random() * 10) + 1); // :1546 (hoisted var reuse)
              if (randomSeed2 <= 4) { // :1547
                returnInput.lsY = 0.0;
                returnInput.lsX =
                    1 * js_sign(closest.x - cpu->phys.pos.x); // :1549
                returnInput.b = true;
                return returnInput; // :1551
              }
            }
            if (closest.y - cpu->phys.pos.y >= 40 ||
                fabs(closest.x - cpu->phys.pos.x) >= 50) { // :1559
              returnInput.lsX = 0.0;
              returnInput.lsY = 1.0;
              returnInput.b = true;
            }
          }
        }
      }
    }
    if (sim->cS[p] == 2 && returnInput.lsY == 1.0) { // :1568
      returnInput.lsX = 0.0;
    }
  } else if (sim->cS[p] == 0 &&
             strcmp(cpu->actionState, "UPSPECIAL") == 0) { // :1571
    returnInput.lsX = 0.35 * js_sign(closest.x - cpu->phys.pos.x); // :1572
  }
  return returnInput; // :1574
}

// --- forward decls of the character AIs -------------------------------------
static void ai_marthAI(MlAiSim *sim, int i);
static void ai_jiggsAI(MlAiSim *sim, int i);
static void ai_foxAI(MlAiSim *sim, int i);
static void ai_falcoAI(MlAiSim *sim, int i);
static void ai_falconAI(MlAiSim *sim, int i);

// --- generalAI (ai.js:32-450) ------------------------------------------------
static void ai_generalAI(MlAiSim *sim, int i) {
  MlPlayer *const pl = sim->player[i];
  MlAiInput *const bk0 = &sim->bank[i][0];

  bk0->lsX = aiv_num(0);       // :33
  bk0->lsY = aiv_num(0);       // :34
  bk0->x = aiv_bool(false);    // :35
  bk0->b = aiv_bool(false);    // :36
  bk0->l = aiv_num(0);         // :37 (NUMBER 0, not false — rule 16)
  bk0->lA = aiv_num(0);        // :38
  bk0->csX = aiv_num(0);       // :39
  bk0->csY = aiv_num(0);       // :40
  bk0->a = aiv_bool(false);    // :41
  // :42 `var willWalk = false` — declared, never used (dead)
  const char *const paction = pl->actionState; // :43
  const double px = pl->phys.pos.x;            // :44
  const double py = pl->phys.pos.y;            // :45
  // :46-47 pcyx/pcyy — declared, never used in this function (dead)
  const double pdiff = pl->difficulty; // :48
  // :49-52 aerialAttacks/idleActions/groundAttacks — declared, unused here
  const double ptimer = pl->timer;          // :53
  const bool pgrounded = pl->phys.grounded; // :54
  // :55-71 large commented-out block (dead upstream)

  if (strcmp(paction, "GRABRELEASE") == 0) { // :73
    AI_COV(GEN_GRABRELEASE);
    static const char *const L[] = {
        "WAIT", "OTTOTTOWAIT", "DAMAGEFALL", "FALL", "JUMPF", "LANDING",
        "JAB1", "ESCAPEF", "ESCAPEB", "FORWARDSMASH", "DOWNTILT"};
    if (ptimer >= 2 && ai_in(paction, L, 11)) { // :74 — dead: paction IS
      // "GRABRELEASE", never in the list. Carried verbatim.
      ai_strcpy(pl->currentAction, "NONE");    // :75
      ai_strcpy(pl->currentSubaction, "NONE"); // :76
    }
  }
  if (strcmp(paction, "CATCHWAIT") == 0) { // :79 filler AI
    AI_COV(GEN_CATCHWAIT);
    const double randomSeed = floor((ml_random() * 10) + 1); // :81
    if (randomSeed <= 2) {
      bk0->lsX = aiv_num(1.0); // :83
    } else if (randomSeed <= 4) {
      bk0->lsX = aiv_num(-1.0);
    } else if (randomSeed <= 6) {
      bk0->lsY = aiv_num(1.0);
    } else if (randomSeed <= 8) {
      bk0->lsY = aiv_num(-1.0);
    } else {
      bk0->a = aiv_num(1.0); // :91 NUMBER into a button field (rule 16)
    }
    return; // :93
  }
  if (strcmp(pl->currentAction, "DROPTHROUGHPLATFORM") == 0 &&
      strcmp(paction, "SQUAT") != 0) { // :95
    AI_COV(GEN_DROPTHRU_CLEAR);
    ai_strcpy(pl->currentAction, "NONE");
    ai_strcpy(pl->currentSubaction, "NONE");
  } else if (strcmp(pl->currentAction, "DROPTHROUGHPLATFORM") == 0) { // :98
    AI_COV(GEN_DROPTHRU_HOLD);
    bk0->lsY = aiv_num(-1.0); // :100
    return;                   // :103
  }
  if ((strcmp(pl->currentSubaction, "LEFT") == 0 ||
       strcmp(pl->currentSubaction, "RIGHT") == 0) &&
      strcmp(pl->currentAction, "NONE") == 0) { // :105
    AI_COV(GEN_SUB_LR_CLEAR);
    ai_strcpy(pl->currentSubaction, "NONE"); // :107
  }
  if (strcmp(pl->currentAction, "RUNOFFPLATFORM") == 0) { // :109
    AI_COV(GEN_RUNOFF);
    if (!(pgrounded) && ai_isOffstage(sim, pl)) { // :110
      ai_strcpy(pl->currentAction, "NONE");
      ai_strcpy(pl->currentSubaction, "NONE");
    }
    static const char *const L[] = {"FALL", "DASH",      "RUN",
                                    "SMASHTURN", "TURN", "WALK"};
    if (!ai_in(paction, L, 6)) { // :114
      ai_strcpy(pl->currentAction, "NONE");
      ai_strcpy(pl->currentSubaction, "NONE");
    } else {
      if (strcmp(paction, "WALK") == 0) { // :118
        AI_COV(GEN_RUNOFF_WALK);
        bk0->lsX = aiv_num(pl->phys.face * -1.0); // :120
      }
      if (strcmp(paction, "SMASHTURN") == 0) { // :122
        AI_COV(GEN_RUNOFF_SMASHTURN);
        if (ptimer < 2) {
          return; // :124
        }
      }
      if (strcmp(pl->currentSubaction, "LEFT") == 0) { // :127
        if (pgrounded) {
          bk0->lsX = aiv_num(-1.0); // :129
        } else {
          AI_COV(GEN_RUNOFF_AIR);
          bk0->lsX = aiv_num(-1.0); // :131
          if (ptimer == 2 && pl->phys.cVel.y <= 0) { // :132 fast fall
            bk0->lsY = aiv_num(-1.0);
          }
          return; // :135
        }
      } else {
        if (pgrounded) {
          bk0->lsX = aiv_num(1.0); // :139
        } else {
          AI_COV(GEN_RUNOFF_AIR);
          bk0->lsX = aiv_num(-1.0); // :141
          if (ptimer == 2 && pl->phys.cVel.y <= 0) { // :142 fast fall
            bk0->lsY = aiv_num(-1.0);
          }
          return; // :145
        }
      }
    }
  }
  if (strcmp(pl->currentSubaction, "UPTILT") == 0 &&
      strcmp(paction, "UPTILT") != 0) { // :150
    AI_COV(GEN_SUB_UPTILT_CLEAR);
    ai_strcpy(pl->currentSubaction, "NONE");
  }
  const int nearest = ai_NearestEnemy(sim, pl, i); // :153
  if (pdiff >= 2) { // :154
    if (strcmp(pl->currentAction, "NONE") == 0) { // :155
      static const char *const L[] = {"OTTOTTOWAIT", "WAIT", "SMASHTURN",
                                      "WALKF", "WALK", "SQUAT"};
      // :156-159 — the isAboveGround call only evaluates when the
      // indexOf clause passed (JS && short-circuit; the call is pure, so
      // the staged evaluation below is observation-identical):
      bool cond = false;
      if (ai_in(paction, L, 6)) {
        // :157 isAboveGround(pos.x, px + 1.0) — the y argument is px+1
        // (upstream typo q7, verbatim)
        const AiAboveGround ag =
            ai_isAboveGround(sim, pl->phys.pos.x, px + 1.0);
        cond = strcmp(ag.kind, "platform") == 0 && pgrounded &&
               py - sim->player[nearest]->phys.pos.y > 0 &&
               fabs(sim->player[nearest]->phys.pos.x - pl->phys.pos.x) <=
                   40; // :158-159
      }
      if (cond) { // is above platform
        AI_COV(GEN_PLATDROP);
        const double randomSeed = floor((ml_random() * 10) + 1); // :162
        if (randomSeed <= 3) { // :164
          bk0->lsY = aiv_num(-1.0);
          ai_strcpy(pl->currentAction, "DROPTHROUGHPLATFORM"); // :166
          return;
        } else if (randomSeed <= 5) { // :168
          ai_strcpy(pl->currentAction, "SHIELD"); // :169
          bk0->l = aiv_num(1.0); // :170 NUMBER into l (rule 16)
          if (aiv_truthy(bk0->l)) { // :171
            bk0->lA = aiv_num(1);
          }
          return; // :174
        } else if (randomSeed >= 6) { // :175
          ai_strcpy(pl->currentAction, "RUNOFFPLATFORM"); // :176
          const double randomSeed2 = floor((ml_random() * 2) + 1); // :177
          if (randomSeed2 == 1) {
            ai_strcpy(pl->currentSubaction, "LEFT"); // :179
            bk0->lsX = aiv_num(-1.0);
            return;
          } else {
            ai_strcpy(pl->currentSubaction, "RIGHT"); // :183
            bk0->lsX = aiv_num(1.0);
            return;
          }
        }
      }
    }
  }
  if (strcmp(pl->currentAction, "SHIELD") == 0) { // :191
    static const char *const L[] = {"GUARD", "GUARDON",     "WAIT",
                                    "DASH",  "OTTOTTOWAIT", "SMASHTURN"};
    if (!ai_in(paction, L, 6)) { // :192
      AI_COV(GEN_SHIELD_CLEAR);
      ai_strcpy(pl->currentAction, "NONE");
      ai_strcpy(pl->currentSubaction, "NONE");
    } else { // :195 is shielding
      AI_COV(GEN_SHIELD_DO);
      const AiShieldRet inputs = ai_CPUShield(sim, pl, i); // :197
      bk0->lsX = aiv_num(isnan(inputs.lsX) ? 0 : inputs.lsX); // :198
      bk0->lsY = aiv_num(isnan(inputs.lsY) ? 0 : inputs.lsY);
      bk0->x = aiv_bool(inputs.x);
      bk0->b = aiv_bool(inputs.b);
      bk0->l = aiv_bool(inputs.l);
      bk0->csX = aiv_num(isnan(inputs.csX) ? 0 : inputs.csX);
      bk0->csY = aiv_num(isnan(inputs.csY) ? 0 : inputs.csY);
      bk0->a = aiv_bool(inputs.a);
      if (aiv_truthy(bk0->l)) { // :206
        bk0->lA = aiv_num(1);
      }
      return; // :209
    }
  }
  if (strcmp(pl->currentAction, "LEDGESTALL") == 0) { // :212
    AI_COV(GEN_LEDGESTALL_CLEAR);
    if (strcmp(pl->currentSubaction, "FALL") == 0) { // :213
      static const char *const L[] = {
          "CLIFFWAIT", "JUMPF", "FALL", "FALLAERIAL", "JUMPAERIAL",
          "JUMPAERIALF", "JUMPAERIAL1", "JUMPAERIALB"};
      if (!ai_in(paction, L, 8)) { // :214
        ai_strcpy(pl->currentAction, "NONE");
        ai_strcpy(pl->currentSubaction, "NONE");
      }
    } else if (strcmp(pl->currentSubaction, "GRAB") == 0) { // :219 grab ledge
      static const char *const L[] = {
          "UPSPECIAL", "UPSPECIALCHARGE", "UPSPECIALLAUNCH", "JUMPAERIAL",
          "CLIFFWAIT", "FALL", "JUMPAERIAL1", "FALLAERIAL", "JUMPAERIALF",
          "JUMPAERIALB", "JUMPF"};
      if (!ai_in(pl->currentAction, L, 11)) { // :220-222 (checks
        // currentAction — "LEDGESTALL" is never in the list: always clears)
        ai_strcpy(pl->currentAction, "NONE");
        ai_strcpy(pl->currentSubaction, "NONE");
      }
    }
  }
  {
    static const char *const L[] = {"TOURNAMENTWINNER", "LEDGEGETUP",
                                    "LEDGEATTACK", "LEDGEROLL"};
    if (ai_in(pl->currentAction, L, 4)) { // :228
      if (!(strncmp(paction, "CLIFF", 5) == 0)) { // :229 substr(0,5)
        AI_COV(GEN_TW_CLEAR);
        ai_strcpy(pl->currentAction, "NONE"); // :230
      }
    }
  }
  if (strcmp(pl->currentAction, "LEDGEDASH") == 0) { // :233
    static const char *const L[] = {
        "CLIFFWAIT", "JUMPAERIALF", "JUMPAERIALB", "FALLAERIAL",
        "ESCAPEAIR", "FALL", "JUMPAERIAL1", "JUMPAERIAL"};
    if (!ai_in(paction, L, 8)) { // :234
      AI_COV(GEN_LEDGEDASH_CLEAR);
      ai_strcpy(pl->currentAction, "NONE"); // :236
    } else {
      AI_COV(GEN_LEDGEDASH_DO);
      const AiLedgeRet inputs = ai_CPULedge(sim, pl, i); // :238
      bk0->lsX = aiv_num(isnan(inputs.lsX) ? 0 : inputs.lsX); // :240
      bk0->lsY = aiv_num(isnan(inputs.lsY) ? 0 : inputs.lsY);
      bk0->x = aiv_bool(inputs.x);
      bk0->b = aiv_bool(inputs.b);
      bk0->l = aiv_bool(inputs.l);
      bk0->csX = aiv_num(isnan(inputs.csX) ? 0 : inputs.csX);
      bk0->csY = aiv_num(isnan(inputs.csY) ? 0 : inputs.csY);
      bk0->a = aiv_bool(inputs.a);
      if (aiv_truthy(bk0->l)) { // :248
        bk0->lA = aiv_num(1);
      }
      return; // :251
    }
  }
  if (pdiff == 4 && pl->hit.hitlag > 0 && ai_isOffstage(sim, pl) &&
      !(pgrounded)) { // :254 SDI
    AI_COV(GEN_SDI);
    const AiSDIRet inputs = ai_CPUSDItoStage(sim, pl, i); // :255
    bk0->lA = aiv_num(1);      // :256
    bk0->l = aiv_bool(true);   // :257
    bk0->lsX = aiv_num(isnan(inputs.lsX) ? 0 : inputs.lsX);
    bk0->lsY = aiv_num(isnan(inputs.lsY) ? 0 : inputs.lsY);
    return; // :260
  }
  // :262 `!(player[i].grounded)` — TOP-LEVEL grounded, a property no
  // playerObject has (q1; marshal-asserted undef): !undefined === true.
  if (ai_isOffstage(sim, pl) &&
      strcmp(pl->currentAction, "NONE") == 0) {
    AI_COV(GEN_RECOVER);
    const AiRecoverRet inputs = ai_CPUrecover(sim, pl, i); // :263
    bk0->lsX = aiv_num(isnan(inputs.lsX) ? 0 : inputs.lsX); // :265
    bk0->lsY = aiv_num(isnan(inputs.lsY) ? 0 : inputs.lsY);
    bk0->x = aiv_bool(inputs.x);
    bk0->b = aiv_bool(inputs.b);
  } // (no return — falls through, verbatim)
  if (strcmp(pl->currentAction, "REVERSEUPTILT") == 0) { // :270
    static const char *const L[] = {"SMASHTURN", "WAIT", "UPTILT",
                                    "LANDING", "OTTOTTOWAIT"};
    if (ai_in(paction, L, 5)) { // :271
      AI_COV(GEN_RUT_CLEAR);
      ai_strcpy(pl->currentAction, "NONE");
      ai_strcpy(pl->currentSubaction, "NONE");
    } else {
      if (strcmp(pl->currentSubaction, "REVERSE") == 0) { // :275 smash turn
        AI_COV(GEN_RUT_REVERSE);
        bk0->lsX = aiv_num(-1.0 * pl->phys.face); // :276
        ai_strcpy(pl->currentSubaction, "UPTILT"); // :277
        return;
      } else if (strcmp(pl->currentSubaction, "UPTILT") == 0 &&
                 ptimer > 1) { // :279
        AI_COV(GEN_RUT_UPTILT);
        bk0->lsX = aiv_num(0.0); // :280
        ai_strcpy(pl->currentAction, "NONE");
        ai_strcpy(pl->currentSubaction, "NONE");
        bk0->lsY = aiv_num(.50);   // :283
        bk0->a = aiv_bool(true);   // :284
        return;
      }
    }
  }
  if (strcmp(pl->currentAction, "MASHING") == 0 &&
      strcmp(paction, "WAIT") == 0 && ptimer > 2) { // :289
    AI_COV(GEN_MASHING_WAIT);
    ai_strcpy(pl->currentAction, "NONE"); // :290
  }
  if (strcmp(pl->currentAction, "SMASHTURN") == 0) { // :295
    if (strcmp(paction, "WAIT") == 0 || ptimer > 0) { // :296
      AI_COV(GEN_SMASHTURN_CLEAR);
      ai_strcpy(pl->currentAction, "NONE");
    }
  }
  if (strcmp(pl->currentAction, "TECH") == 0 ||
      strcmp(pl->currentAction, "MISSEDTECH") == 0) { // :300
    if (strcmp(paction, "CLIFFWAIT") == 0 || strcmp(paction, "FALLN") == 0 ||
        strcmp(paction, "WAIT") == 0) { // :301
      AI_COV(GEN_TECH_CLEAR);
      ai_strcpy(pl->currentAction, "NONE");
    }
  }
  if ((strcmp(paction, "DAMAGEFALL") == 0 ||
       strcmp(paction, "DAMAGEFLYN") == 0) &&
      (!ai_isOffstage(sim, pl)) && pdiff > 0) { // :305
    if (pl->hit.hitstun <= 0) { // :309
      AI_COV(GEN_DMGFALL_ACT);
      double extra = 0; // :310
      if (!pl->phys.doubleJumped ||
          (pl->phys.jumpsUsed < 5 && ai_multiJump(sim, i))) { // :311
        extra = 3; // :312
      }
      const double randomSeed = floor((ml_random() * (2 + extra)) + 1); // :314
      if (randomSeed == 1) { // :315 left
        bk0->lsX = aiv_num(-1.0);
      } else if (randomSeed == 2) { // :317 right
        bk0->lsX = aiv_num(1.0);
      } else { // :319 jump
        bk0->x = aiv_bool(true);
      }
      ai_strcpy(pl->currentAction, "NONE"); // :322
      return;
    }
    AI_COV(GEN_DMGFALL_TECH);
    ai_strcpy(pl->currentAction, "TECH"); // :326
    const AiTechRet inputs = ai_CPUTech(sim, pl, i); // :327
    bk0->lsX = aiv_num(isnan(inputs.lsX) ? 0 : inputs.lsX); // :328
    bk0->l = aiv_bool(inputs.l);
    if (aiv_truthy(bk0->l)) { // :330
      bk0->lA = aiv_num(1);
    }
    return; // :333
  }
  if (strcmp(paction, "DOWNWAIT") == 0) { // :340 missed tech options
    AI_COV(GEN_DOWNWAIT);
    ai_strcpy(pl->currentAction, "MISSEDTECH"); // :341
    const AiMissedTechRet inputs = ai_CPUMissedTech(sim, pl, i); // :342
    bk0->lsX = aiv_num(isnan(inputs.lsX) ? 0 : inputs.lsX);
    bk0->lsY = aiv_num(isnan(inputs.lsY) ? 0 : inputs.lsY);
    bk0->a = aiv_bool(inputs.a);
  }
  if (strcmp(paction, "DOWNWAIT") != 0) { // :347
    if (strncmp(paction, "CAPTURE", 7) == 0 && pdiff > 0 &&
        strcmp(paction, "CAPTURECUT") != 0) { // :349 break out of grabs
      AI_COV(GEN_CAPTURE_MASH);
      ai_strcpy(pl->currentAction, "MASHING"); // :351
      pl->lastMash += 1;                       // :352
      if (pl->lastMash > (8 - (2 * (pdiff)))) { // :353
        AI_COV(GEN_CAPTURE_BURST);
        pl->lastMash = 0;         // :354
        bk0->lsY = aiv_num(1.0);  // :355
        bk0->lA = aiv_num(1);     // :356
        if (!aiv_truthy(sim->bank[i][1].a)) { // :357
          bk0->a = aiv_bool(true);   // :358
          bk0->x = aiv_bool(true);   // :359
          bk0->lsX = aiv_num(-1.0);  // :360
          bk0->csX = aiv_num(-1.0);  // :361
        } else {
          bk0->y = aiv_bool(true);   // :364
          bk0->lsX = aiv_num(1.0);   // :365
          bk0->b = aiv_bool(true);   // :366
          bk0->csX = aiv_num(1.0);   // :367
        }
      }
    }
    if (strcmp(pl->currentAction, "WAVESHINEANY") == 0) { // :372
      AI_COV(GEN_WAVESHINE_DO);
      const AiWaveshineRet inputs = ai_CPUWaveshineAny(sim, pl, i); // :373
      bk0->lsX = aiv_num(isnan(inputs.lsX) ? 0 : inputs.lsX);
      bk0->lsY = aiv_num(isnan(inputs.lsY) ? 0 : inputs.lsY);
      bk0->x = aiv_bool(inputs.x);
      bk0->b = aiv_bool(inputs.b);
      bk0->l = aiv_bool(inputs.l);
      if (aiv_truthy(bk0->l)) { // :379
        bk0->lA = aiv_num(1);
      }
      return; // :382
    }
    if (strcmp(pl->currentAction, "WAVESHINEANY") != 0 &&
        (strcmp(paction, "CAPTURECUT") == 0 ||
         strcmp(pl->currentAction, "GRABRELEASE") == 0)) { // :384
      AI_COV(GEN_GRABREL_DO);
      ai_strcpy(pl->currentAction, "GRABRELEASE"); // :386
      const AiGrabRet inputs = ai_CPUGrabRelease(sim, pl, i); // :387
      bk0->lsX = aiv_num(isnan(inputs.lsX) ? 0 : inputs.lsX);
      bk0->lsY = aiv_num(isnan(inputs.lsY) ? 0 : inputs.lsY);
      bk0->x = aiv_bool(inputs.x);
      bk0->b = aiv_bool(inputs.b);
      bk0->l = aiv_bool(inputs.l);
      bk0->csX = aiv_num(isnan(inputs.csX) ? 0 : inputs.csX);
      bk0->csY = aiv_num(isnan(inputs.csY) ? 0 : inputs.csY);
      bk0->a = aiv_bool(inputs.a);
      if (aiv_truthy(bk0->l)) { // :396
        bk0->lA = aiv_num(1);
      }
      return; // :399
    }
    if (strcmp(pl->currentAction, "MASHING") == 0 &&
        !(strncmp(paction, "CAPTURE", 7) == 0)) { // :401
      AI_COV(GEN_MASHING_TYPOARM);
      // :402 `player[i].currentAction == "NONE";` — a COMPARISON used as
      // a statement (upstream typo q2): NO write happens. Verbatim.
      pl->lastMash = 0; // :403
    }
    if (pl->hit.hitstun > 0) { // :405 stops action if they get interrupted
      AI_COV(GEN_HITSTUN_CLEAR);
      ai_strcpy(pl->currentAction, "NONE"); // :406
    }
    if (strcmp(paction, "REBIRTHWAIT") == 0) { // :408
      AI_COV(GEN_REBIRTHWAIT);
      bk0->lsY = aiv_num(-1.0); // :409
    }
    if ((strcmp(pl->currentAction, "NONE") == 0 &&
         (strcmp(paction, "CLIFFWAIT") == 0)) ||
        (strcmp(pl->currentAction, "LEDGEDASH") == 0 ||
         strcmp(pl->currentAction, "LEDGEAIRATTACK2") == 0 ||
         strcmp(pl->currentAction, "LEDGEAIRATTACK") == 0 ||
         strcmp(pl->currentAction, "LEDGEGETUP") == 0 ||
         strcmp(pl->currentAction, "LEDGEATTACK") == 0 ||
         strcmp(pl->currentAction, "LEDGEJUMP") == 0 ||
         strcmp(pl->currentAction, "LEDGEROLL") == 0 ||
         strcmp(pl->currentAction, "LEDGEJUMP") == 0 || // duplicate, verbatim
         strcmp(pl->currentAction, "TOURNAMENTWINNER") == 0)) { // :411-415
      AI_COV(GEN_LEDGE_DO);
      const AiLedgeRet inputs = ai_CPULedge(sim, pl, i); // :416
      bk0->lsX = aiv_num(isnan(inputs.lsX) ? 0 : inputs.lsX); // :418
      bk0->lsY = aiv_num(isnan(inputs.lsY) ? 0 : inputs.lsY);
      bk0->x = aiv_bool(inputs.x);
      bk0->b = aiv_bool(inputs.b);
      bk0->l = aiv_bool(inputs.l);
      bk0->csX = aiv_num(isnan(inputs.csX) ? 0 : inputs.csX);
      bk0->csY = aiv_num(isnan(inputs.csY) ? 0 : inputs.csY);
      bk0->a = aiv_bool(inputs.a);
      if (aiv_truthy(bk0->l)) { // :426
        bk0->lA = aiv_num(1);
      }
      return; // :429
    }
  }
  if (aiv_truthy(bk0->l)) { // :432
    bk0->lA = aiv_num(1);
  }
  if (pdiff > 1) { // :435
    const double distx = pl->phys.pos.x -
                         sim->player[nearest]->phys.pos.x; // :436
    const double disty = py - sim->player[nearest]->phys.pos.y; // :437
    (void)disty; // declared+assigned, unused below (verbatim shape)
    if (strcmp(pl->currentAction, "NONE") == 0 &&
        strcmp(pl->currentSubaction, "NONE") == 0 &&
        (strcmp(paction, "WAIT") == 0 || strcmp(paction, "OTTOTTOWAIT") == 0 ||
         strcmp(paction, "WALK") == 0)) { // :438 walk towards enemy
      // :440-441 — grounded || isAboveGround(...)[0]; short-circuit
      // preserved (the call is pure):
      bool cond = false;
      if (fabs(distx) >= 23) {
        if (sim->player[nearest]->phys.grounded) {
          cond = true;
        } else {
          const AiAboveGround ag =
              ai_isAboveGround(sim, sim->player[nearest]->phys.pos.x,
                               sim->player[nearest]->phys.pos.y);
          cond = ag.above;
        }
      }
      if (cond) {
        AI_COV(GEN_WALK);
        bk0->lsX = aiv_num(0.75 * (-1.0 * (js_sign(distx)))); // :442
      }
    }
  }
  // :448 run character specific stuff — ais[cS[i]](i)
  const double c = sim->cS[i];
  if (c == 0) {
    ai_marthAI(sim, i);
  } else if (c == 1) {
    ai_jiggsAI(sim, i);
  } else if (c == 2) {
    ai_foxAI(sim, i);
  } else if (c == 3) {
    ai_falcoAI(sim, i);
  } else if (c == 4) {
    ai_falconAI(sim, i);
  } else {
    ml_ai_out_of_domain("ais[cS[i]]: character selection outside 0-4");
  }
}

// --- marthAI (ai.js:451-569) -------------------------------------------------
static void ai_marthAI(MlAiSim *sim, int i) {
  MlPlayer *const pl = sim->player[i];
  MlAiInput *const bk0 = &sim->bank[i][0];
  const char *const paction = pl->actionState; // :452
  const double px = pl->phys.pos.x;            // :453
  const double py = pl->phys.pos.y;            // :454
  // :455-461 pcyx/pcyy + the three lists — declared, unused (dead)
  const double pdiff = pl->difficulty;      // :457
  const double ptimer = pl->timer;          // :462
  const bool pgrounded = pl->phys.grounded; // :463
  if (strcmp(pl->currentAction, "LEDGESTALL") == 0) { // :464
    AI_COV(MARTH_LEDGESTALL);
    bk0->lsX = aiv_num(0.0); // :465
    if (strcmp(pl->currentSubaction, "FALL") == 0) { // :466
      if (ptimer == 7) { // :467
        bk0->lsY = aiv_num(-1.0); // :469
        bk0->x = aiv_num(1.0);    // :470 NUMBER into x (rule 16)
        ai_strcpy(pl->currentSubaction, "GRAB"); // :471
      } else {
        bk0->lsY = aiv_num(-1.0); // :473
      }
      return; // :475
    } else if (strcmp(pl->currentSubaction, "GRAB") == 0) { // :476
      bk0->lsX = aiv_num(0.0); // :477
      // :478 `paction.substr(0, 4) == "CLIFF"` — the 4-char prefix
      // "CLIF" can never equal "CLIFF": ALWAYS false upstream (dead
      // clear arm, carried verbatim).
      if (0 && strcmp(paction, "CLIFFCATCH") == 0) {
        ai_strcpy(pl->currentAction, "NONE");
        ai_strcpy(pl->currentSubaction, "NONE");
      }
      return; // :482
    }
  }
  const int nearest = ai_NearestEnemy(sim, pl, i); // :485
  // :486-489 `var distx/disty` — hoisted, assigned only when
  // currentAction == "NONE"; undefined -> NaN elsewhere (q6):
  double distx = ai_undef_num();
  double disty = ai_undef_num();
  if (strcmp(pl->currentAction, "NONE") == 0) { // :486
    distx = px - sim->player[nearest]->phys.pos.x;
    disty = py - sim->player[nearest]->phys.pos.y;
  }
  if (pdiff >= 2 && strcmp(pl->currentAction, "NONE") == 0) { // :490
    // :491-495 (marth parenthesization, transcribed verbatim)
    if ((pgrounded &&
         ((strcmp(paction, "WAIT") == 0 ||
           (pgrounded && sim->turbo && pl->hasHit &&
            (floor((ml_random() * 10) + 1) >= 8 - (2 * pdiff)))) &&
          fabs(distx) > 15)) ||
        ((pdiff > 0 && pl->hasHit && sim->turbo && pgrounded) ||
         strcmp(paction, "WAIT") == 0) ||
        (strcmp(paction, "LANDING") == 0 && ptimer > 3)) {
      // smash turn to face enemy
      if (!(pl->phys.face == -1.0 * (js_sign(distx)))) { // :496
        AI_COV(MARTH_TURN);
        ai_strcpy(pl->currentAction, "SMASHTURN"); // :497
        bk0->lsX = aiv_num(-1.0 * pl->phys.face);  // :498
        return;
      } else {
        static const char *const L[] = {"WAIT", "WALK", "OTTOTTOWAIT",
                                        "LANDING"};
        if (strcmp(pl->currentAction, "NONE") == 0 &&
            ai_in(paction, L, 4) &&
            sim->player[nearest]->phys.hurtBoxState == 0) { // :501
          if (fabs(distx) < 23 && fabs(disty) < 15) { // :502
            AI_COV(MARTH_TILT);
            const double randomSeed = floor((ml_random() * 100) + 1); // :503
            if (randomSeed <= 10) { // :504 grab
              bk0->z = aiv_bool(true); // :505
            } else if (randomSeed <= 25) { // :511 tilt
              const double randomSeed1 =
                  floor((ml_random() * 100) + 1); // :512
              if (randomSeed1 <= 25) { // :513 f-tilt
                bk0->lsX = aiv_num(0.50);
              } else if (randomSeed1 <= 50) { // :515 d-tilt
                bk0->lsY = aiv_num(-0.50);
              } else if (randomSeed1 <= 75) { // :517 up-tilt
                if (sim->cS[i] == 1 || sim->cS[i] == 2) { // :518 (dead in
                  // the marth dispatch — ais[0] only runs when cS[i]==0;
                  // copy-paste from the fox/jiggs bodies, verbatim)
                  if (!(1.0 * js_sign(distx) == pl->phys.face)) { // :519
                    ai_strcpy(pl->currentAction, "REVERSEUPTILT");
                    ai_strcpy(pl->currentSubaction, "REVERSE");
                    return; // :522
                  } else {
                    bk0->lsY = aiv_num(0.50);
                    bk0->a = aiv_bool(true);
                  }
                } else {
                  bk0->lsY = aiv_num(0.50); // :529
                  bk0->a = aiv_bool(true);
                }
              }
              bk0->a = aiv_bool(true); // :533
              return;                  // :534
            }
          }
        }
      }
    }
  }
  if (pdiff >= 3) { // :545
    if (sim->player[nearest]->phys.hurtBoxState == 0) { // :546
      static const char *const L[] = {"WAIT", "OTTOTTOWAIT", "WALK", "DASH",
                                      "RUN"};
      if (ai_in(paction, L, 5)) { // :547
        if (fabs(py - sim->player[nearest]->phys.pos.y) <= 3) { // :548
          if (pl->phys.face == -1.0 * (js_sign(distx))) { // :549 (extra parens dropped)
            AI_COV(MARTH_REACT);
            const double randomSeed = floor((ml_random() * 100) + 1); // :550
            if (randomSeed <= 40) { // :551
              if (ai_isEnemyApproaching(pl, sim->player[nearest]) ||
                  strncmp(sim->player[nearest]->actionState, "GUARD", 5) ==
                      0) { // :552
                if (fabs(px - sim->player[nearest]->phys.pos.x) <= 20) { // :553
                  bk0->l = aiv_bool(true);
                  bk0->lA = aiv_num(1.0);
                  bk0->a = aiv_bool(true);
                }
              } else {
                static const char *const LD[] = {"DOWNBOUND", "DOWNSTANDF",
                                                 "DOWNSTANDB", "DOWNSTANDN"};
                if (randomSeed <= 25 &&
                    fabs(px - sim->player[nearest]->phys.pos.x) < 12.5 &&
                    !ai_in(paction, LD, 4)) { // :558
                  bk0->l = aiv_bool(true);
                  bk0->lA = aiv_num(1.0);
                  bk0->a = aiv_bool(true);
                }
              }
            }
          }
        }
      }
    }
  }
}

// --- jiggsAI (ai.js:571-676) -------------------------------------------------
static void ai_jiggsAI(MlAiSim *sim, int i) {
  MlPlayer *const pl = sim->player[i];
  MlAiInput *const bk0 = &sim->bank[i][0];
  const char *const paction = pl->actionState; // :572
  const double px = pl->phys.pos.x;            // :573
  const double py = pl->phys.pos.y;            // :574
  const double pdiff = pl->difficulty;         // :577
  const double ptimer = pl->timer;             // :582
  const bool pgrounded = pl->phys.grounded;    // :583
  const int nearest = ai_NearestEnemy(sim, pl, i); // :584
  // :586-587 hoisted distx/disty (q6):
  double distx = ai_undef_num();
  double disty = ai_undef_num();
  if (strcmp(pl->currentAction, "NONE") == 0) { // :585
    distx = px - sim->player[nearest]->phys.pos.x;
    disty = py - sim->player[nearest]->phys.pos.y;

    if (pdiff >= 2 && strcmp(pl->currentAction, "NONE") == 0) { // :589
      // :590-594 (jiggs parenthesization, transcribed verbatim)
      if ((pgrounded &&
           ((strcmp(paction, "WAIT") == 0 ||
             (pgrounded && sim->turbo && pl->hasHit &&
              (floor((ml_random() * 10) + 1) >= 8 - (2 * pdiff)))) &&
            fabs(distx) > 15)) ||
          ((pdiff > 0 && pl->hasHit && sim->turbo && pgrounded) ||
           strcmp(paction, "WAIT") == 0) ||
          (strcmp(paction, "LANDING") == 0 && ptimer > 3)) {
        // smash turn to face enemy
        if (!(pl->phys.face == -1.0 * (js_sign(distx)))) { // :595
          AI_COV(JIGGS_TURN);
          ai_strcpy(pl->currentAction, "SMASHTURN"); // :596
          bk0->lsX = aiv_num(-1.0 * pl->phys.face);
          return;
        } else {
          if (sim->cS[i] == 2 && fabs(distx) > 80 &&
              fabs(disty) < 15) { // :600 is fox (dead in the jiggs
            // dispatch — ais[1] only runs when cS[i]==1; verbatim)
            const double randomSeed = floor((ml_random() * 10) + 1); // :601
            if (randomSeed == 1) {
              ai_strcpy(pl->currentAction, "SHDL"); // :603
              ai_strcpy(pl->currentSubaction, "LASER1");
            }
          }
          if (strcmp(pl->currentAction, "NONE") == 0) { // :607
            if (fabs(distx) < 23 && fabs(disty) < 15) { // :608
              AI_COV(JIGGS_TILT);
              const double randomSeed = floor((ml_random() * 100) + 1); // :609
              if (randomSeed <= 10) { // :610 grab
                bk0->z = aiv_bool(true); // :611
              } else if (randomSeed <= 25) { // :617 tilt
                const double randomSeed1 =
                    floor((ml_random() * 100) + 1); // :618
                if (randomSeed1 <= 25) { // f-tilt
                  bk0->lsX = aiv_num(0.50);
                } else if (randomSeed1 <= 50) { // d-tilt
                  bk0->lsY = aiv_num(-0.50);
                } else if (randomSeed1 <= 75) { // up-tilt
                  if (sim->cS[i] == 1 || sim->cS[i] == 2) { // :624
                    if (!(1.0 * js_sign(distx) == pl->phys.face)) { // :625
                      ai_strcpy(pl->currentAction, "REVERSEUPTILT");
                      ai_strcpy(pl->currentSubaction, "REVERSE");
                      return; // :628
                    } else {
                      bk0->lsY = aiv_num(0.50);
                      bk0->a = aiv_bool(true);
                    }
                  } else {
                    bk0->lsY = aiv_num(0.50); // :635
                    bk0->a = aiv_bool(true);
                  }
                }
                bk0->a = aiv_bool(true); // :639
                return;                  // :640
              }
            }
          }
        }
      }
    }
  }
  if (pdiff >= 3) { // :652
    if (sim->player[nearest]->phys.hurtBoxState == 0) { // :653
      static const char *const L[] = {"WAIT", "OTTOTTOWAIT", "WALK", "DASH",
                                      "RUN"};
      if (ai_in(paction, L, 5)) { // :654
        if (fabs(py - sim->player[nearest]->phys.pos.y) <= 3) { // :655
          if (pl->phys.face == -1.0 * (js_sign(distx))) { // :656 (extra parens dropped)
            AI_COV(JIGGS_REACT);
            const double randomSeed = floor((ml_random() * 100) + 1); // :657
            if (randomSeed <= 30) { // :658
              if (ai_isEnemyApproaching(pl, sim->player[nearest]) ||
                  strncmp(sim->player[nearest]->actionState, "GUARD", 5) ==
                      0) { // :659
                if (fabs(px - sim->player[nearest]->phys.pos.x) <= 13) { // :660
                  bk0->l = aiv_bool(true);
                  bk0->lA = aiv_num(1.0);
                  bk0->a = aiv_bool(true);
                }
              } else {
                static const char *const LD[] = {"DOWNBOUND", "DOWNSTANDF",
                                                 "DOWNSTANDB", "DOWNSTANDN"};
                if (randomSeed <= 20 &&
                    fabs(px - sim->player[nearest]->phys.pos.x) < 8 &&
                    !ai_in(paction, LD, 4)) { // :665
                  bk0->l = aiv_bool(true);
                  bk0->lA = aiv_num(1.0);
                  bk0->a = aiv_bool(true);
                }
              }
            }
          }
        }
      }
    }
  }
}

// --- foxAI (ai.js:678-865) ---------------------------------------------------
static void ai_foxAI(MlAiSim *sim, int i) {
  MlPlayer *const pl = sim->player[i];
  MlAiInput *const bk0 = &sim->bank[i][0];
  const char *const paction = pl->actionState; // :679
  const double px = pl->phys.pos.x;            // :680
  const double py = pl->phys.pos.y;            // :681
  const double pdiff = pl->difficulty;         // :684
  const double ptimer = pl->timer;             // :689
  const bool pgrounded = pl->phys.grounded;    // :690
  if (strcmp(pl->currentAction, "LEDGESTALL") == 0) { // :691
    AI_COV(FOX_LEDGESTALL);
    bk0->lsX = aiv_num(0.0); // :692
    if (strcmp(pl->currentSubaction, "FALL") == 0) { // :693
      if (ptimer == 1) { // :694
        bk0->lsY = aiv_num(1.0);  // :696
        bk0->b = aiv_bool(true);  // :697
        ai_strcpy(pl->currentSubaction, "GRAB"); // :698
      } else {
        bk0->lsY = aiv_num(-1.0); // :700
      }
      return; // :702
    } else if (strcmp(pl->currentSubaction, "GRAB") == 0) { // :703
      bk0->lsX = aiv_num(0.0); // :704
      // :705 `paction.substr(0, 4) == "CLIFF"` — "CLIF" never equals
      // "CLIFF": ALWAYS false upstream (dead clear arm, verbatim).
      if (0) {
        ai_strcpy(pl->currentAction, "NONE");
        ai_strcpy(pl->currentSubaction, "NONE");
      }
      return; // :709
    }
  }
  bool isDead = false; // :712
  // :713 `var deadDude = "NONE"` then `deadDude = aa` (string|number) —
  // declared+written, never read (dead; not modeled)
  for (int aa = 0; aa < 4; aa++) { // :714
    if (sim->playerType[aa] != -1 && !(i == aa)) { // :715
      if (strncmp(sim->player[aa]->actionState, "DEAD", 4) == 0 ||
          strncmp(sim->player[aa]->actionState, "REBIRTH", 7) == 0) { // :716
        isDead = true;
      }
    }
  }
  if (isDead) { // :724
    if (strcmp(pl->currentSubaction, "NONE") == 0 &&
        strcmp(pl->currentAction, "NONE") == 0 && pgrounded &&
        pdiff >= 3) { // :725 can do it
      AI_COV(FOX_RESPAWN_TRIGGER);
      ai_strcpy(pl->currentAction, "RESPAWNMULTISHINE"); // :726
      ai_strcpy(pl->currentSubaction, "SHINE");          // :727
      return; // :728
    }
  }
  if (strcmp(pl->currentAction, "SHIELDMULTISHINE") == 0) { // :731
    // :732 reads `nearest` BEFORE its `const` declaration (:778) — a TDZ
    // ReferenceError upstream (q3; nothing ever writes
    // "SHIELDMULTISHINE" — grep-measured). Trap, never guess.
    ml_ai_out_of_domain(
        "foxAI SHIELDMULTISHINE: TDZ read of `nearest` (upstream throws)");
  }
  if (strcmp(pl->currentAction, "RESPAWNMULTISHINE") == 0) { // :738
    AI_COV(FOX_RESPAWN_MACHINE);
    if (strcmp(pl->currentSubaction, "NONE") == 0) { // :739
      if (!(isDead)) { // :740 should finish multishining
        ai_strcpy(pl->currentAction, "NONE"); // :741
      } else {
        ai_strcpy(pl->currentSubaction, "JUMP"); // :743
      }
    }
    {
      static const char *const L[] = {
          "DOWNSPECIALGROUND", "DOWNSPECIALAIR", "KNEEBEND", "JUMPF",
          "JUMPB", "WAIT", "WALK", "WALKF", "OTTOTTOWAIT"};
      if (!ai_in(paction, L, 9)) { // :746 (`== -1.0` upstream)
        ai_strcpy(pl->currentAction, "NONE"); // :748
        ai_strcpy(pl->currentSubaction, "NONE");
      }
    }
    if (strcmp(pl->currentSubaction, "SHINE") == 0) { // :751
      bk0->lsY = aiv_num(-1.0); // :752
      bk0->b = aiv_bool(true);  // :753
      ai_strcpy(pl->currentSubaction, "JUMP"); // :754
    } else if (strcmp(pl->currentSubaction, "JUMP") == 0) { // :755
      bk0->b = aiv_bool(true); // :756
      if ((ptimer == 3 && strcmp(paction, "DOWNSPECIALGROUND") == 0) ||
          (ptimer == 6 && strcmp(paction, "DOWNSPECIALGROUND") == 0)) { // :757
        bk0->x = aiv_bool(true); // :759
        ai_strcpy(pl->currentSubaction, "SHINE2"); // :760
      }
    } else if (strcmp(pl->currentSubaction, "SHINE2") == 0) { // :762
      if (strcmp(paction, "KNEEBEND") == 0 && ptimer == 3) { // :763
        bk0->lsY = aiv_num(-1.0); // :764
        bk0->b = aiv_bool(true);
        ai_strcpy(pl->currentSubaction, "NONE"); // :766
      }
    }
    if (strcmp(pl->currentAction, "RESPAWNMULTISHINE") == 0) { // :769
      return; // :770
    }
    if (ai_in_array3(pl->currentSubaction)) { // :772 (q4 `in`-array)
      AI_COV(FOX_RESPAWN_INARR);
      if (pl->hit.hitstun >= 0) { // :773
        ai_strcpy(pl->currentSubaction, "NONE"); // :774
      }
    }
  }
  const int nearest = ai_NearestEnemy(sim, pl, i); // :778
  // :780-781 hoisted distx/disty (q6):
  double distx = ai_undef_num();
  double disty = ai_undef_num();
  if (strcmp(pl->currentAction, "NONE") == 0) { // :779
    distx = px - sim->player[nearest]->phys.pos.x;
    disty = py - sim->player[nearest]->phys.pos.y;
    if (pdiff >= 2 && strcmp(pl->currentAction, "NONE") == 0) { // :782
      // :783-787 (fox parenthesization, transcribed verbatim)
      if ((pgrounded &&
           ((strcmp(paction, "WAIT") == 0 ||
             (pgrounded && sim->turbo && pl->hasHit &&
              (floor((ml_random() * 10) + 1) >= 8 - (2 * pdiff)))) &&
            fabs(distx) > 15)) ||
          ((pdiff > 0 && pl->hasHit && sim->turbo && pgrounded) ||
           strcmp(paction, "WAIT") == 0) ||
          (strcmp(paction, "LANDING") == 0 && ptimer > 3)) {
        // smash turn to face enemy
        if (!(pl->phys.face == -1.0 * (js_sign(distx)))) { // :788
          AI_COV(FOX_TURN);
          ai_strcpy(pl->currentAction, "SMASHTURN"); // :789
          bk0->lsX = aiv_num(-1.0 * pl->phys.face);
          return;
        } else {
          if (sim->cS[i] == 2 && fabs(distx) > 80 &&
              fabs(disty) < 15) { // :793 is fox
            const double randomSeed = floor((ml_random() * 10) + 1); // :794
            if (randomSeed == 1) { // :795
              AI_COV(FOX_SHDL_TRIGGER);
              ai_strcpy(pl->currentAction, "SHDL"); // :796
              ai_strcpy(pl->currentSubaction, "LASER1");
            }
          }
          if (strcmp(pl->currentAction, "NONE") == 0) { // :800
            if (fabs(distx) < 23 && fabs(disty) < 15) { // :801
              AI_COV(FOX_TILT);
              const double randomSeed = floor((ml_random() * 100) + 1); // :802
              if (randomSeed <= 10) { // :803 grab
                bk0->z = aiv_bool(true); // :804
              } else if (randomSeed <= 25) { // :805 tilt
                const double randomSeed1 =
                    floor((ml_random() * 100) + 1); // :806
                if (randomSeed1 <= 25) { // f-tilt
                  bk0->lsX = aiv_num(0.50);
                } else if (randomSeed1 <= 50) { // d-tilt
                  bk0->lsY = aiv_num(-0.50);
                } else if (randomSeed1 <= 75) { // up-tilt
                  if (sim->cS[i] == 1 || sim->cS[i] == 2) { // :812
                    if (!(1.0 * js_sign(distx) == pl->phys.face)) { // :813
                      ai_strcpy(pl->currentAction, "REVERSEUPTILT");
                      ai_strcpy(pl->currentSubaction, "REVERSE");
                      return; // :816
                    } else {
                      bk0->lsY = aiv_num(0.50);
                      bk0->a = aiv_bool(true);
                    }
                  } else {
                    bk0->lsY = aiv_num(0.50); // :823
                    bk0->a = aiv_bool(true);
                  }
                }
                bk0->a = aiv_bool(true); // :827
                return;                  // :828
              }
            }
          }
        }
      }
    }
  }
  if (pdiff >= 3) { // :836
    if (sim->player[nearest]->phys.hurtBoxState == 0) { // :837
      static const char *const L[] = {"WAIT", "OTTOTTOWAIT", "WALK", "DASH",
                                      "RUN"};
      if (ai_in(paction, L, 5)) { // :838
        if (fabs(py - sim->player[nearest]->phys.pos.y) <= 3) { // :839
          if (pl->phys.face == -1.0 * (js_sign(distx))) { // :840 (extra parens dropped)
            AI_COV(FOX_REACT);
            const double randomSeed = floor((ml_random() * 100) + 1); // :841
            if (randomSeed <= 30) { // :842
              if (ai_isEnemyApproaching(pl, sim->player[nearest]) ||
                  strncmp(sim->player[nearest]->actionState, "GUARD", 5) ==
                      0) { // :843
                if (fabs(px - sim->player[nearest]->phys.pos.x) <= 12) { // :844
                  bk0->l = aiv_bool(true);
                  bk0->lA = aiv_num(1.0);
                  bk0->a = aiv_bool(true);
                }
              } else {
                static const char *const LD[] = {"DOWNBOUND", "DOWNSTANDF",
                                                 "DOWNSTANDB", "DOWNSTANDN"};
                if (randomSeed <= 20 &&
                    fabs(px - sim->player[nearest]->phys.pos.x) < 8 &&
                    !ai_in(paction, LD, 4)) { // :849
                  bk0->l = aiv_bool(true);
                  bk0->lA = aiv_num(1.0);
                  bk0->a = aiv_bool(true);
                }
              }
            }
          }
        }
      }
    }
  }
  if (strcmp(pl->currentAction, "SHDL") == 0) { // :860
    AI_COV(FOX_SHDL_DO);
    const AiSHDLRet inputs = ai_CPUSHDL(sim, pl, i); // :861
    bk0->x = aiv_bool(inputs.x); // :862
    bk0->b = aiv_bool(inputs.b); // :863
  }
}

// --- falcoAI / falconAI (ai.js:867-872) — empty upstream ---------------------
static void ai_falcoAI(MlAiSim *sim, int i) {
  (void)sim;
  (void)i;
}
static void ai_falconAI(MlAiSim *sim, int i) {
  (void)sim;
  (void)i;
}

// --- runAI (ai.js:874-878) ---------------------------------------------------
void ml_runAI(MlAiSim *sim, int i) {
  ai_generalAI(sim, i); // :875 calls general AI
}
