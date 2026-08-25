"use strict";
// Synthetic input trace that drives player 1 off a stage edge and into a
// LEFT wall with the away-flick that upstream's walljump gate wants
// (physics.js:132-134, ported at port/sim/physics.c:450). Consumed by
// port/sim/check-walljump-d47.sh (MENU-SPEC DEVIATION D47).
//
// Emitted in the sim_host --trace text format (port/sim/sim/trace-to-txt.js):
// one line per frame, four slots separated by BARE pipes, each present slot
// 22 tokens — 12 bools then 10 IEEE-754 bit-pattern hex16 doubles, in the
// order a b x y z r l s du dr dd dl lsX lsY csX csY lA rA rawX rawY rawcsX
// rawcsY.
//
// THREE INSTRUMENT BUGS COST THE 2026-08-05 ATTEMPT (fix_plan "D20 MARTH
// WITNESS (#16)") and are structurally excluded here:
//   1. lsX set but rawX left 0 — interpretInputs derives the processed stick
//      from raw, so BOTH come from one `x` argument in slot() below.
//   2. slot separators written as "... | | " — a SPACE where slot 2 begins
//      made every run die "trace: short slot token list" and write an EMPTY
//      stream, and two empty files cmp as identical, so the sweep reported
//      "no divergence" while nothing had run. Rows are built as
//      slot0 + "|" + slot1 + "||" and asserted by the grammar check below.
//   3. the comparison was never sanity-checked against a known-good positive.
//      The consuming check proves a POSITIVE (fox, who walljumps upstream
//      with no house rule at all) BEFORE it trusts any null result.
//
// WHY THE STICK OSCILLATES rather than simply holding away from the wall:
// the gate reads BOTH the current input and the one from three frames ago —
// `sign*in[0].lsX >= 0.7 && sign*in[3].lsX <= 0` — so a walljump can only
// fire within three frames of flicking from into-the-wall to away-from-it.
// A steady away-hold satisfies in[0] and fails in[3] forever. The repeating
// (INTO xN, AWAY xM) cycle keeps a fresh flick within reach of whichever
// frame the wall contact lands on, and the into-frames also supply the
// `posDelta.x >= 0.5` that arms wallJumpTimer (physics.c:427).
//
// usage: node port/sim/walljump-trace.js <frames> [runOff] [flick] [jump]
//   runOff — first frame of the hold-left walk off the left edge
//   flick  — first frame of the into/away cycle
//   jump   — frame on which X is pressed (marth needs a jump to get airborne
//            against the wall; 0 = never)

function bits(d) {
  const b = Buffer.alloc(8);
  b.writeDoubleBE(d, 0);
  return b.toString("hex");
}

// One slot's 22 tokens. `x` drives lsX AND rawX together (bug 1 above).
function slot(x, y, buttons) {
  const bl = (n) => (buttons.indexOf(n) === -1 ? "0" : "1");
  return (
    [bl("a"), bl("b"), bl("x"), bl("y"), bl("z"), bl("r"), bl("l"), bl("s"),
     bl("du"), bl("dr"), bl("dd"), bl("dl")].join("") +
    [bits(x), bits(y), bits(0), bits(0), bits(0), bits(0),
     bits(x), bits(y), bits(0), bits(0)].map((h) => " " + h).join("")
  );
}

const NEUTRAL = slot(0, 0, []);
const CYCLE_INTO = 6;
const CYCLE_AWAY = 2;

function rowsFor(n, runOff, flick, jump) {
  const rows = [];
  for (let f = 1; f <= n; f++) {
    let x = 0;
    if (f >= flick) {
      // into the LEFT wall is +x (the player is outside it, to its left);
      // away is -x, which is what the gate wants in in[0].
      const k = (f - flick) % (CYCLE_INTO + CYCLE_AWAY);
      x = k < CYCLE_INTO ? 1 : -1;
    } else if (f >= runOff) {
      x = -1; // walk/run off the left edge
    }
    // a one-frame X press; jumpsquat then carries the character upward.
    const btn = jump && (f === jump || f === jump + 1) ? ["x"] : [];
    rows.push(slot(x, 0, btn) + "|" + NEUTRAL + "||");
  }
  return rows;
}

const n = parseInt(process.argv[2] || "600", 10);
const runOff = parseInt(process.argv[3] || "100", 10);
const flick = parseInt(process.argv[4] || "140", 10);
const jump = parseInt(process.argv[5] || "0", 10);
const rows = rowsFor(n, runOff, flick, jump);

// Row grammar self-check (bug 2 above): exactly three bare pipes, no space
// adjacent to any of them, 11 whitespace-separated tokens per present slot
// (the 12 bools are one token, then 10 hex16). A malformed trace must fail
// HERE, not silently produce an empty stream that cmp calls identical.
for (const r of rows) {
  if ((r.match(/\|/g) || []).length !== 3) throw new Error("row: pipe count");
  if (/ \|/.test(r) || /\| /.test(r)) throw new Error("row: space at a pipe");
  for (const s of r.split("|").slice(0, 2)) {
    if (s.trim().split(/\s+/).length !== 11) throw new Error("row: token count");
  }
}
process.stdout.write(rows.join("\n") + "\n");
