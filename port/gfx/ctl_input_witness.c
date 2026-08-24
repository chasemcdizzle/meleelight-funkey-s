// port/gfx/ctl_input_witness.c — the L-SHOULDER witness (fix_plan A25b +
// A3 + A30(a)). Host-only; driven by port/gfx/check-ctl-input.sh.
//
// WHAT WENT WRONG, AND WHY A WITNESS EXISTS AT ALL. The keymap said the
// L shoulder emits keysym 'k'; the hardware emits 'm' (KEY_M, code 50 —
// decoded off /dev/input/event0 on 2026-08-24 while the owner held the
// button). One wrong token killed L at EVERY call site simultaneously:
// dead in target-select (A25b) and never shielding in a match (A3). No
// device check could see it, because every one of them injects through
// our OWN uinput node while the physical buttons come from fkgpiod,
// which the rig's quiesce bracket stops for the duration of a run.
//
// So this witness deliberately does NOT re-prove the injector chain the
// rig already covers. It proves the two things the rig is blind to and
// the one thing the fix is FOR:
//   [1] the DEVICE TRANSLATION ARM, run — not read. platform_sdl1.c's
//       platform_poll body is platform_keymap_translate(); that function
//       lives in the header precisely so a host with no SDL can execute
//       it against a synthetic keystate. Held 'm' must land on
//       PlatformInput.l and on nothing else; held 'k' must now land on
//       NOTHING, because 'k' is a dead keysym on this hardware.
//   [2] the SSOT — the compiled table re-emitted in the KEYMAP1 form
//       foh_dev's --dump-keymap uses, byte-compared against the frozen
//       port/foh/keymap-frozen.txt.
//   [3] OWNER-VISIBLE BEHAVIOUR, through the real functions:
//       (a) L SHIELDS in the fresh-install style — s1_input_row_style()
//           with only l held must emit r=true, rA=1.0 (A3);
//       (b) L CHAR-STEPS in target-select — the real foh_tick() on the
//           real FOH_TSS screen must wrap p1Char 0 -> 4 (A25b);
//       (c) the Mod shoulder default is the SWAPPED one, and BOX really
//           reads it: L shields and R mods (A30(a) / D30).
//
// Every leg is an assertion against a value produced by tree code. The
// check script's teeth are perturbed COPY builds, so each leg has to be
// able to fail on its own.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../foh/foh.h"
#include "ctl_style.h"
#include "platform.h"
#include "platform_keymap.h"
#include "s1_input.h"

// Every app TU defines this (foh_dev.c routes it to sim_fatal). The
// witness never links the sim, so it carries its own: loud, non-returning.
void gfx_fatal(const char *what) {
  fprintf(stderr, "ctl_input_witness: gfx_fatal: %s\n", what);
  exit(3);
}

static int g_fails;

static void want(int cond, const char *what) {
  if (!cond) {
    printf("FAIL %s\n", what);
    g_fails++;
  }
}

// The twelve button fields, in the table's own row order, read by the
// SAME offset accessor the arm writes through — so a row that lands on
// the wrong field is visible as the wrong field being set, not merely as
// "l is false".
static bool field_of(const PlatformInput *in, int idx) {
  PlatformInput t = *in;
  return *platform_keymap_field(&t, idx);
}

static int row_index(const char *logical) {
  for (int i = 0; i < PLATFORM_KEYMAP_ROWS; i++) {
    if (strcmp(kPlatformKeymap[i].logical, logical) == 0) return i;
  }
  return -1;
}

// --- [1] the device translation arm, executed -------------------------------
static void leg_arm(void) {
  unsigned char ks[256];
  PlatformInput in;

  const int li = row_index("l");
  want(li >= 0, "[1] the keymap has an 'l' row at all");
  if (li < 0) return;

  // THE FIX, stated as behaviour: held 'm' reaches PlatformInput.l.
  memset(ks, 0, sizeof ks);
  ks['m'] = 1;
  memset(&in, 0, sizeof in);
  platform_keymap_translate(&in, ks);
  want(in.l, "[1] holding keysym 'm' sets PlatformInput.l");
  for (int i = 0; i < PLATFORM_KEYMAP_ROWS; i++) {
    if (i == li) continue;
    want(!field_of(&in, i), "[1] 'm' sets ONLY .l and no other button");
  }

  // ... and 'k' is now dead. This is the half that makes the leg bite in
  // both directions: a table carrying BOTH letters would pass the test
  // above and fail here.
  memset(ks, 0, sizeof ks);
  ks['k'] = 1;
  memset(&in, 0, sizeof in);
  platform_keymap_translate(&in, ks);
  for (int i = 0; i < PLATFORM_KEYMAP_ROWS; i++) {
    want(!field_of(&in, i), "[1] keysym 'k' is dead — it sets no button");
  }

  // Every row is one-to-one: its keysym sets its field and nothing else.
  // This is what catches a fix that lands 'm' on the wrong field, or a
  // second row quietly colliding on the same letter.
  for (int i = 0; i < PLATFORM_KEYMAP_ROWS; i++) {
    memset(ks, 0, sizeof ks);
    ks[(unsigned char)kPlatformKeymap[i].keysym] = 1;
    memset(&in, 0, sizeof in);
    platform_keymap_translate(&in, ks);
    for (int j = 0; j < PLATFORM_KEYMAP_ROWS; j++) {
      want(field_of(&in, j) == (i == j),
           "[1] each keysym drives exactly its own logical button");
    }
  }

  // A released key must CLEAR its field even when the struct arrives
  // dirty — the arm assigns rather than ORs, and the device's poll
  // relies on that (the memset there is for `quit`, not the buttons).
  memset(ks, 0, sizeof ks);
  memset(&in, 0xFF, sizeof in);
  platform_keymap_translate(&in, ks);
  for (int i = 0; i < PLATFORM_KEYMAP_ROWS; i++) {
    want(!field_of(&in, i), "[1] a released key clears its field");
  }
  printf("  [1] OK: the device translation arm maps 'm' -> .l, 'k' -> nothing\n");
}

// --- [2] the compiled table == the frozen KEYMAP1 file ----------------------
static void leg_ssot(const char *frozenPath) {
  char want_buf[512];
  size_t n = 0;
  int w = snprintf(want_buf, sizeof want_buf, "KEYMAP1\n");
  if (w < 0) { printf("FAIL [2] snprintf\n"); g_fails++; return; }
  n = (size_t)w;
  for (int i = 0; i < PLATFORM_KEYMAP_ROWS; i++) {
    // The EXACT form foh_dev.c:1500-1504's --dump-keymap emits.
    w = snprintf(want_buf + n, sizeof want_buf - n, "map %s %c %c\n",
                 kPlatformKeymap[i].logical, kPlatformKeymap[i].flowLetter,
                 kPlatformKeymap[i].keysym);
    if (w < 0 || (size_t)w >= sizeof want_buf - n) {
      printf("FAIL [2] dump overflow\n");
      g_fails++;
      return;
    }
    n += (size_t)w;
  }
  FILE *f = fopen(frozenPath, "rb");
  if (!f) {
    printf("FAIL [2] cannot open %s\n", frozenPath);
    g_fails++;
    return;
  }
  char got[512];
  const size_t got_n = fread(got, 1, sizeof got, f);
  const int truncated = !feof(f);
  fclose(f);
  want(!truncated, "[2] the frozen keymap fits the reader");
  want(got_n == n && memcmp(got, want_buf, n) == 0,
       "[2] the compiled table re-emits the frozen keymap-frozen.txt byte-exactly");
  printf("  [2] OK: compiled table == %s (%d rows)\n", frozenPath,
         PLATFORM_KEYMAP_ROWS);
}

// --- [3a] L shields, in the fresh-install style (A3) ------------------------
static void leg_shield(void) {
  PlatformInput p;
  memset(&p, 0, sizeof p);

  // Neutral first: r must be false, or the leg proves nothing.
  MlInput neutral = s1_input_row_style(&p, ctl_style_get(), ctl_mod_on_r_get());
  want(!neutral.r && neutral.rA == 0.0,
       "[3a] a neutral pad does not shield");

  p.l = true;
  MlInput held = s1_input_row_style(&p, ctl_style_get(), ctl_mod_on_r_get());
  want(held.r && held.rA == 1.0,
       "[3a] L alone shields in the fresh-install style (r=true, rA=1.0)");

  // And R still does, so the fix added a shield button rather than
  // moving one.
  memset(&p, 0, sizeof p);
  p.r = true;
  MlInput rHeld = s1_input_row_style(&p, ctl_style_get(), ctl_mod_on_r_get());
  want(rHeld.r && rHeld.rA == 1.0, "[3a] R still shields too");
  printf("  [3a] OK: L shields in style %s\n", ctl_style_name((int)ctl_style_get()));
}

// --- [3b] L char-steps in target-select (A25b) ------------------------------
static void leg_tss(void) {
  FohState s;
  foh_init(&s);
  s.screen = FOH_TSS;
  s.p1Char = 0;

  PlatformInput none, lHeld;
  memset(&none, 0, sizeof none);
  memset(&lHeld, 0, sizeof lHeld);
  lHeld.l = true;

  // The arm is a RISING EDGE, so a settled frame comes first.
  foh_tick(&s, &none);
  const int before = s.p1Char;
  foh_tick(&s, &lHeld);
  want(before == 0 && s.p1Char == 4,
       "[3b] L wraps p1Char 0 -> 4 in target-select (targetselect.js:62-66)");
  // Held, not re-pressed: the edge must not repeat.
  foh_tick(&s, &lHeld);
  want(s.p1Char == 4, "[3b] a HELD L does not keep stepping");
  // ... and the same gesture arriving as keysym 'm' through the real arm
  // produces the same step, which is the whole chain in one assertion.
  unsigned char ks[256];
  memset(ks, 0, sizeof ks);
  PlatformInput fromKeys;
  memset(&fromKeys, 0, sizeof fromKeys);
  platform_keymap_translate(&fromKeys, ks); // release
  foh_tick(&s, &fromKeys);
  ks['m'] = 1;
  memset(&fromKeys, 0, sizeof fromKeys);
  platform_keymap_translate(&fromKeys, ks);
  foh_tick(&s, &fromKeys);
  want(s.p1Char == 3, "[3b] keysym 'm' itself steps p1Char 4 -> 3");
  printf("  [3b] OK: L char-steps in target-select, driven from keysym 'm'\n");
}

// --- [3c] the Mod shoulder default (A30(a) / D30) ---------------------------
static void leg_mod(void) {
  want(ctl_mod_on_r_get(),
       "[3c] the Mod shoulder cell defaults to R (owner 2026-08-24)");

  // BOX is the only style the cell changes, and ctl_roles is where it is
  // read — assert the ROLES, not the cell.
  PlatformInput p;
  bool clayer, mod, shield;
  memset(&p, 0, sizeof p);
  p.l = true;
  ctl_roles(CTL_STYLE_BOX, ctl_mod_on_r_get(), &p, &clayer, &mod, &shield);
  want(shield && !mod, "[3c] BOX: L is SHIELD under the default arrangement");
  memset(&p, 0, sizeof p);
  p.r = true;
  ctl_roles(CTL_STYLE_BOX, ctl_mod_on_r_get(), &p, &clayer, &mod, &shield);
  want(mod && !shield, "[3c] BOX: R is MOD under the default arrangement");

  // The ratified TABLE is untouched by the swap — the same chord row
  // still answers, only the shoulder that reaches it changed. This is
  // the claim ctl_style.h:63-70 makes; here it is as an assertion.
  PlatformInput lMod, rMod;
  memset(&lMod, 0, sizeof lMod);
  memset(&rMod, 0, sizeof rMod);
  lMod.right = true;
  rMod.right = true;
  lMod.l = true; // Mod under the RATIFIED arrangement
  rMod.r = true; // Mod under the SWAPPED one
  const S1Resolved a = s1_resolve_style(&lMod, CTL_STYLE_BOX, false);
  const S1Resolved b = s1_resolve_style(&rMod, CTL_STYLE_BOX, true);
  want(strcmp(a.row, b.row) == 0 && a.lsX == b.lsX && a.lsY == b.lsY,
       "[3c] the swap is a pure relabeling — same row, same coordinates");
  want(strcmp(a.row, "L-horizontal-walk") == 0,
       "[3c] and that row is still the ratified Mod walk");
  printf("  [3c] OK: Mod on R / shield on L, ratified table untouched\n");
}

int main(int argc, char **argv) {
  if (argc != 2) {
    fprintf(stderr, "usage: ctl_input_witness <keymap-frozen.txt>\n");
    return 2;
  }
  leg_arm();
  leg_ssot(argv[1]);
  leg_shield();
  leg_tss();
  leg_mod();
  if (g_fails != 0) {
    printf("CTL INPUT WITNESS FAILED (%d)\n", g_fails);
    return 1;
  }
  printf("CTL INPUT OK\n");
  return 0;
}
