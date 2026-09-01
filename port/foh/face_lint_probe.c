// port/foh/face_lint_probe.c — the FACE-DOMAIN PROBE for ticket #24.
//
// Two jobs, and neither of them is "restate a list of characters".
//
// [1] IT HANDS OUT THE DOMAIN. `foh_face_domain` is foh_font.c's own
//     enumeration of its two glyph tables, so the lint that consumes this
//     dump is reading the FONT, not a copy of it. Restating a constant
//     instead of reading it is how four separate device checks went stale in
//     one week in this project (spec #20), and a face domain restated in a
//     lint would go stale the first time a glyph is added.
//
// [2] IT ENUMERATES THE NAME TABLES THAT ARE NOT ARRAYS. port/foh/face-lint.js
//     scans committed source for array initializers, and three drawn name
//     sets are not one:
//       * foh_ctl_labels()  (port/foh/foh_ctl_labels.h) — nine Controls-screen
//         row labels ASSIGNED one by one, and two of the nine are chosen by a
//         conditional, so the strings only all exist for (style, modOnR)
//         combinations. All six are walked here.
//       * ctl_style_name()  (port/gfx/ctl_style.c) — a switch, including its
//         out-of-domain arm, which returns a string too.
//     These reach the renderer through foh_render.c's Controls screen, which
//     folds them to upper case at the draw site (foh_upper) because they are
//     authored mixed-case. The dump therefore names the transform, and the
//     lint applies it; check-face-lint.sh pins foh_upper's body so the two
//     cannot drift apart.
//
// This is a CHECK build, so foh_font.c's loud arm is live — which is why the
// probe never DRAWS anything. It asks (`foh_face_domain`) rather than tries.
//
// Output grammar (consumed by port/foh/face-lint.js; the trailing END is what
// makes a truncated dump fail instead of looking like a small domain):
//
//   DOMAIN <face> <count> <hex> <hex> ...
//   RUNTIME <file> <symbol> face1|face2 raw|upper <index> <string>
//   END
//
// Built and run by `bash port/foh/check-face-lint.sh`.

#include "../gfx/ctl_style.h"
#include "../gfx/raster.h"
#include "foh.h"
#include "foh_ctl_labels.h"

#include <stdio.h>
#include <stdlib.h>

// The probe never draws, so this is only here to satisfy the link (foh_font.c
// and raster.c both reference it). If it ever fires, something drew.
void gfx_fatal(const char *what) {
  printf("PROBE FATAL %s\n", what);
  exit(7);
}

static void dump_domain(int face) {
  char dom[256];
  const int n = foh_face_domain(face, dom, (int)sizeof dom);
  if (n <= 0) {
    fprintf(stderr, "face_lint_probe: foh_face_domain(%d) returned %d\n", face,
            n);
    exit(1);
  }
  printf("DOMAIN %d %d", face, n);
  for (int i = 0; i < n; i++) printf(" %02x", (unsigned char)dom[i]);
  printf("\n");
}

// One RUNTIME row. `s` must not contain a newline — nothing in these tables
// does, and a check that silently split a row would be worse than one that
// says so.
static void emit(const char *file, const char *sym, const char *face,
                 const char *xform, int idx, const char *s) {
  if (!s) {
    fprintf(stderr, "face_lint_probe: %s %s[%d] is NULL\n", file, sym, idx);
    exit(1);
  }
  for (const char *p = s; *p; p++) {
    if (*p == '\n' || *p == '\r') {
      fprintf(stderr, "face_lint_probe: %s %s[%d] contains a newline\n", file,
              sym, idx);
      exit(1);
    }
  }
  printf("RUNTIME %s %s %s %s %d %s\n", file, sym, face, xform, idx, s);
}

int main(void) {
  dump_domain(1);
  dump_domain(2);

  // foh_ctl_labels: 3 styles x Mod-on-L/R. Every row of every combination,
  // because two of the nine rows are conditional on exactly those two inputs
  // and a single call would leave four strings unvisited.
  {
    const CtlStyle styles[3] = {CTL_STYLE_NORMAL, CTL_STYLE_NATURAL,
                                CTL_STYLE_BOX};
    int idx = 0;
    for (int si = 0; si < 3; si++) {
      for (int mod = 0; mod < 2; mod++) {
        const char *rows[FOH_CTL_LABEL_ROWS];
        foh_ctl_labels(styles[si], mod != 0, rows);
        for (int r = 0; r < FOH_CTL_LABEL_ROWS; r++) {
          // face 1: render_ctrl_key draws these with foh_text.
          emit("port/foh/foh_ctl_labels.h", "foh_ctl_labels", "face1", "raw",
               idx++, rows[r]);
        }
      }
    }
  }

  // ctl_style_name: authored mixed-case, uppercased at the draw site
  // (foh_render.c:2121-2122). It takes an INT, not the enum, precisely so an
  // out-of-domain value can be passed — so the OUT-OF-DOMAIN ARM is walked
  // here too. It returns a string like any other arm, and this lint is the
  // only thing in the tree that ever looks at it. (It found one: the arm
  // used to return "?", which face 1 cannot draw. See ctl_style.c.)
  //
  // ctl_mod_shoulder_name and ctl_btn_name are NOT walked: measured, neither
  // reaches the renderer — ctl_btn_name appears only in a witness fprintf and
  // ctl_mod_shoulder_name has no caller at all. If either ever gains a draw
  // site it belongs in the loop above.
  {
    const int styles[4] = {CTL_STYLE_NORMAL, CTL_STYLE_NATURAL, CTL_STYLE_BOX,
                           (int)CTL_STYLE_COUNT};
    for (int i = 0; i < 4; i++) {
      emit("port/gfx/ctl_style.c", "ctl_style_name", "face1", "upper", i,
           ctl_style_name(styles[i]));
    }
  }

  printf("END\n");
  return 0;
}
