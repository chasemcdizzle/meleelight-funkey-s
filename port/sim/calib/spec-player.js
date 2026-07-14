// spec-player.js — M2 task 2 capture spec: per-frame post-update(i)
// player[i] snapshots (the player/game-state VALUE MODEL surface).
// Registers window.__capSpecs.player. Injected AFTER capturelib.js.
//
// Boundary: physics.js's exported `physics(i, inputBuffers)`. Upstream's
// update(i) (main/main.js:894) is called by gameTick through the LOCAL
// module binding (same module — babel CJS does not route intra-module
// calls through the exports object), so update itself is not wrappable at
// the namespace; `physics(i, inputBuffers)` is update's TAIL STATEMENT
// and is dereferenced cross-module at call time ((0,_physics.physics)(i,…)
// in the built bundle), so post-physics state == post-update(i) state.
//
// Mutation-capture (rig upgrade, this task): physics mutates player[i]
// in place and returns undefined — the value is in the POST-STATE field
// (5th tab field): canon of the projected player[i] AFTER the call.
//
// args projection (FORMAT.md "player spec projections"): [i] only — the
// inputBuffers argument is the input cluster's surface (tasks 3/5 capture
// it at their own boundaries; this task prices the player VALUE MODEL).
//
// player snapshot projection (documented exclusions, all three verified):
// - charAttributes, charHitboxes: immutable per-character table data
//   (M1 ml_tables/CTAB1 owns them; zero in-match writes upstream — the
//   only writers are menu-time css.js:150/171). The C model reaches them
//   through the generated tables, never through the snapshot.
// - percentShake: written OFF-TICK by wall-clock setTimeout callbacks
//   drawing from the stashed NATIVE RNG (oracle/CHECKSUM.md §7 — the one
//   player field excluded from the checksum surface for exactly this
//   reason). Including it would make capture byte-stability
//   timing-dependent BY CONSTRUCTION.
(() => {
  const EXCLUDE = ["charAttributes", "charHitboxes", "percentShake"];

  function projectPlayer(p) {
    const out = {};
    for (const k of Object.keys(p)) {
      if (EXCLUDE.indexOf(k) === -1) out[k] = p[k];
    }
    return out;
  }

  window.__capSpecs.player = {
    expectWrapped: 1,
    install(ctx) {
      const moduleIds = {};
      const physicsMod = ctx.findModule(ctx.cache, (ex) =>
        typeof ex.physics === "function" &&
        typeof ex.land === "function", "physics");
      moduleIds.physics = physicsMod.id;
      ctx.wrapExport(physicsMod.exports, "physics", "physics",
        (args) => [args[0]], // args projection: slot index only
        (args) => {
          const players = window.__harness.getPlayers();
          return ctx.canon(projectPlayer(players[args[0]]));
        });
      return { moduleIds: moduleIds };
    },
  };
})();
