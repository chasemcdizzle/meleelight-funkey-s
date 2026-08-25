#ifndef FOH_LAUNCH_H
#define FOH_LAUNCH_H
// port/foh/foh_launch.h — the ONE place a FohState becomes a match config.
//
// WHY IT EXISTS (A44). Two bridges launch a VS match from the menu — the host
// harness (foh_app.c) and the device/dev app (foh_dev.c) — and until A44 each
// spelled the same five arguments out at its own call site. Five scalars is
// the kind of duplication that stays correct by luck; FOUR PORTS of three
// fields each is not, and CONTEXT.md's "Seam" entry says exactly where the
// defects then land: each side's own check passes and nothing asserts the
// crossing. So the mapping lives here, once, and both bridges plus the
// witness that proves the ticket read the SAME function.
//
// It is a header rather than a foh.c body on purpose: foh.c does not include
// the sim headers, and dragging port/sim/sim/sim.h onto every TU that
// includes foh.h (foh_render.c, foh_persist.c, nine witnesses) to place one
// six-line loop would be a worse trade than a header three files include.
//
// EQUIVALENCE WITH THE PRE-A44 CALL, which is the compatibility argument and
// is mechanically checkable rather than asserted: sim_setup_match's own
// 2-port wrapper (port/sim/sim/sim_boot.c, A46) builds
//   {0, p1, -1} {p2type, p2, p2type == 1 ? difficulty : -1} {-1,0,-1} x2
// and for any FohState the launch guard admits with ports 2/3 at N/A this
// function builds exactly that: port 0's type is pinned 0 by guard condition
// (1), a non-CPU port carries difficulty -1 (patch:84's "undefined -> 3"),
// and an absent port's character/difficulty are unread by the else arm.
// port/foh/foh_p34_witness.c proves that field for field before it goes
// anywhere near a 4-port config.
#include "../sim/sim/sim.h"
#include "foh.h"

// FohState -> harnessSetupMatch's cfg.players (oracle/meleelight-harness.
// patch:76-92). Indexed by PORT throughout — never by roster index, never by
// participation order (CONTEXT.md "Port").
static inline void foh_launch_ports(const FohState *s, SimPortCfg ports[4]) {
  for (int k = 0; k < FOH_CSS_PORTS; k++) {
    ports[k].type = s->portType[k];
    ports[k].character = s->selChar[k];
    // The patch reads cfg.players[i].difficulty and takes 3 when it is
    // undefined; the harness caller (oracle/harness/run.js:154-156) only
    // ever sets it for a CPU. -1 is this port's spelling of undefined.
    ports[k].difficulty = s->portType[k] == 1 ? foh_css_port_diff(s, k) : -1;
  }
}

#endif // FOH_LAUNCH_H
