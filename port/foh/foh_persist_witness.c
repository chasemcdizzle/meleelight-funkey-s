// port/foh/foh_persist_witness.c — the host witness for ticket #22 (the
// persist record becomes a declarative FIELD TABLE; ADR 0001).
//
// WHY A WITNESS AND NOT A SCREEN CHECK. The change under test is a
// SERIALISER REWRITE whose whole contract is "no observable difference":
// the same in-memory record must produce the same bytes, and the same bytes
// must produce the same record. Nothing on screen can see that, and the one
// thing that can — the bytes — is exactly what a byte comparison judges. So
// this witness is deliberately thin: it drives foh_persist_{defaults,load,
// save} and prints the loaded record in a flat, greppable grammar, and
// port/foh/check-persist-table.sh does every judgement on the OUTSIDE, in
// bytes, against fixtures it builds from the FORMAT rather than from the C.
//
// It links foh_persist.c and nothing else of the FOH: gfx_fatal is
// host-provided (raster.h:114), so it is defined here, exactly as
// port/gfx/ctl_input_witness.c and port/gfx/img1_check.c define it.
//
// Usage (MLFK_PERSIST_DIR selects the directory, as in the product):
//   foh_persist_witness seed        defaults + a deterministic NON-default
//                                   edit of EVERY persisted field, then save
//   foh_persist_witness roundtrip   load, then save (the byte-identity leg)
//   foh_persist_witness dump        load, then print the record
//   foh_persist_witness defaults    print foh_persist_defaults()'s record
// `dump` and `roundtrip` print the load status as `status=<token>` first, so
// a caller can tell a load from a reset without parsing stderr.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "foh_persist.h"

// host-provided (raster.h:114) — the witness IS the host here.
void gfx_fatal(const char *what) {
  fprintf(stderr, "%s\n", what);
  exit(2);
}

// The seeded record. EVERY persisted field is moved off its default, and
// every value stays inside its own domain so that a saved seed is a file
// the product (and check-device-persist.sh's independent positional
// whitelist) both accept. A field left at its default would be a dead
// tooth: a table row that silently dropped it would still round-trip.
static void seed(FohPersist *p) {
  foh_persist_defaults(p);
  p->turbo = 1;
  p->lCancelType = 2;
  p->tapJumpOff[0] = 1;
  p->tapJumpOff[1] = 0;
  p->tapJumpOff[2] = 1;
  p->tapJumpOff[3] = 1;
  p->ctlStyle = 1; // CTL_STYLE_BOX
  p->modOnR = 0;
  // records: a distinct finite value per slot, with slot (2,7) left at the
  // -1 no-record sentinel so BOTH halves of the rec domain are carried.
  for (int c = 0; c < FOH_PERSIST_CHARS; c++) {
    for (int s = 0; s < FOH_PERSIST_TSTAGES; s++) {
      p->targetRecords[c][s] =
          (c == 2 && s == 7) ? -1.0 : (double)(c * 10 + s) + 0.5;
    }
  }
  p->flashOnLCancel = 1;
  p->everyCharWallJump = 1;
  p->blastzoneWrapping = 0;
  p->dustLessPerfectWavedash = 1;
  p->phantomThreshold = 0.02;
  p->masterVolume[0] = 0.7;
  p->masterVolume[1] = 0.4;
  // a NON-identity permutation on port 0; the other three stay identity, so
  // the port-major progression is exercised with two distinct rows.
  const int perm[8] = {3, 1, 0, 2, 5, 4, 7, 6};
  for (int i = 0; i < (int)CTL_BTN_COUNT; i++) p->bind[0][i] = perm[i];
  p->selChar[0] = 1;
  p->selChar[1] = 2;
  p->selChar[2] = 3;
  p->selChar[3] = 4;
  p->resumeScreen = (int)FOH_TSS;
  // ticket #25's CSS machine plane. Every value is BOTH non-default AND at a
  // different point of its column than its neighbours, so a row that landed
  // on the wrong member cannot still compare equal: the four port types
  // cover all three of N/A/HMN/CPU, the four levels are all distinct, and
  // `carry` and `cpucarry` differ from each other so the two one-digit rows
  // that sit next to each other in the file cannot be swapped unnoticed.
  p->portType[0] = 0;
  p->portType[1] = 1;
  p->portType[2] = -1;
  p->portType[3] = 1;
  p->cpuDifficulty[0] = 3;
  p->cpuDifficulty[1] = 1;
  p->cpuDifficulty[2] = 4;
  p->cpuDifficulty[3] = 2;
  p->versusMode = 1;
  p->cssHand[0] = 33.25;
  p->cssHand[1] = 240.0; // the INCLUSIVE top of FP_DOM_SCREEN, on purpose
  for (int k = 0; k < FOH_CSS_PORTS; k++) p->cssSliderX[k] = 10.5 + 7.25 * k;
  // BOTH grabs are set, which the SCREEN can never produce — a held knob
  // pins the hand to the rail, so it can never also be in the roster band.
  // This record is a COLUMN test, not a screen state: the file's domain is
  // per row and the loader has no cross-field rule, so seeding both is what
  // proves the two adjacent one-digit rows cannot be swapped for each other.
  // (The reachable combinations are what check-hibernate.sh's round trip
  // drives, through the real screen.)
  p->cssCarry = 2;
  p->cssCpuCarry = 3;
  p->cssHandType = 2;
}

static const char *status_token(FohPersistStatus st) {
  switch (st) {
    case FOH_PERSIST_LOADED: return "loaded";
    case FOH_PERSIST_RESET_MISSING: return "reset-missing";
    case FOH_PERSIST_RESET_VERSION: return "reset-version";
    case FOH_PERSIST_RESET_CORRUPT: return "reset-corrupt";
  }
  return "?";
}

// One line per value, flat and greppable. Doubles print as their IEEE-754
// bit pattern for the same reason the file does: no strtod, no rounding,
// and an exact comparison is possible in a shell.
static void dump(const FohPersist *p) {
  unsigned long long b;
#define BITS(d)                                                                \
  (memcpy(&b, &(double){(d)}, 8), (unsigned long long)b)
  printf("turbo %d\n", p->turbo);
  printf("lcancel %d\n", p->lCancelType);
  for (int k = 0; k < 4; k++) printf("tapjump %d %d\n", k, p->tapJumpOff[k]);
  printf("ctlstyle %d\n", p->ctlStyle);
  printf("modonr %d\n", p->modOnR);
  for (int c = 0; c < FOH_PERSIST_CHARS; c++) {
    for (int s = 0; s < FOH_PERSIST_TSTAGES; s++) {
      printf("rec %d %d %016llx\n", c, s, BITS(p->targetRecords[c][s]));
    }
  }
  printf("flash %d\n", p->flashOnLCancel);
  printf("walljump %d\n", p->everyCharWallJump);
  printf("blastzone %d\n", p->blastzoneWrapping);
  printf("dustless %d\n", p->dustLessPerfectWavedash);
  printf("phantom %016llx\n", BITS(p->phantomThreshold));
  printf("soundslevel %016llx\n", BITS(p->masterVolume[0]));
  printf("musiclevel %016llx\n", BITS(p->masterVolume[1]));
  for (int k = 0; k < CTL_BIND_PORTS; k++) {
    for (int i = 0; i < (int)CTL_BTN_COUNT; i++) {
      printf("bind %d %d %d\n", k, i, p->bind[k][i]);
    }
  }
  for (int k = 0; k < FOH_CSS_PORTS; k++) printf("sel %d %d\n", k, p->selChar[k]);
  printf("resume %d\n", p->resumeScreen);
  // ticket #25. These print the FIELD's value, not the file's column — the
  // wire bias is the table's business and a dump that echoed the biased
  // digit would let a bias applied on only one side of the file pass.
  for (int k = 0; k < FOH_CSS_PORTS; k++) {
    printf("ptype %d %d\n", k, p->portType[k]);
  }
  for (int k = 0; k < FOH_CSS_PORTS; k++) {
    printf("cpudiff %d %d\n", k, p->cpuDifficulty[k]);
  }
  printf("vsmode %d\n", p->versusMode);
  printf("hand %016llx %016llx\n", BITS(p->cssHand[0]), BITS(p->cssHand[1]));
  for (int k = 0; k < FOH_CSS_PORTS; k++) {
    printf("slider %d %016llx\n", k, BITS(p->cssSliderX[k]));
  }
  printf("carry %d\n", p->cssCarry);
  printf("cpucarry %d\n", p->cssCpuCarry);
  printf("handtype %d\n", p->cssHandType);
#undef BITS
}

int main(int argc, char **argv) {
  if (argc != 2) {
    fprintf(stderr, "usage: foh_persist_witness "
                    "<seed|roundtrip|dump|defaults>\n");
    return 2;
  }
  FohPersist p;
  if (strcmp(argv[1], "seed") == 0) {
    seed(&p);
    foh_persist_save(&p);
    return 0;
  }
  if (strcmp(argv[1], "defaults") == 0) {
    foh_persist_defaults(&p);
    dump(&p);
    return 0;
  }
  if (strcmp(argv[1], "roundtrip") == 0) {
    const FohPersistStatus st = foh_persist_load(&p);
    printf("status=%s\n", status_token(st));
    foh_persist_save(&p);
    return 0;
  }
  if (strcmp(argv[1], "dump") == 0) {
    const FohPersistStatus st = foh_persist_load(&p);
    printf("status=%s\n", status_token(st));
    dump(&p);
    return 0;
  }
  fprintf(stderr, "foh_persist_witness: unknown command '%s'\n", argv[1]);
  return 2;
}
