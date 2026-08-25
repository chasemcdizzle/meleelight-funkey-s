#!/usr/bin/env node
// judge-foh-trace.js — FOHTRACE1 whitelist-grammar judge (fix_plan §M4
// task 9; PROCESS §3 whitelist-grammar rule). Decision-bearing parser:
// anchored FULL-LINE patterns measured from the producer (foh_app.c's
// exact fprintf grammars), binary outcome — resembles-but-doesn't-match
// is CORRUPTION, fail closed. Validated against the full genuine corpus
// (the 4 committed flow traces) with zero false rejections before
// shipping (check-foh-flows.sh leg [3]).
//
// STRUCTURE enforced beyond per-line grammar:
//   - header `FOHTRACE1 flow=<id>` with the EXPECTED id (argv);
//   - event frames non-decreasing; END is the last line, exactly once;
//   - T-chain continuity: first T departs `startup`, every T departs
//     the previous T's destination (a screen machine cannot teleport);
//   - every T edge is in the PINNED FLOW GRAPH (the faithful edge set,
//     upstream citations in port/foh/foh.h) — an off-graph transition
//     is corruption, never new behavior;
//   - S field/value domains pinned (chars 0-4, port types -1/0/1,
//     carry -1/0/1, difficulty
//     1-4 = the upstream slider domain, lcancel 0-2, tapjump/turbo 0/1,
//     refused entries = the 11 registered tokens);
//   - LAUNCH-or-TLAUNCH at most once TOTAL, required/forbidden per
//     argv, and IMMEDIATELY preceded by its own launch T line at the
//     same frame (`T <f> sss match launch` for LAUNCH, `T <f>
//     target-select target-match launch` for TLAUNCH — iter 99);
//   - END transitions= count == the number of T lines.
//
// Usage: node judge-foh-trace.js <trace.txt> <flow-id> <launch: 0|1>
"use strict";

const fs = require("fs");

function die(msg) {
  console.error("judge-foh-trace: CORRUPT: " + msg);
  process.exit(2);
}

if (process.argv.length !== 5) {
  console.error(
      "usage: node judge-foh-trace.js <trace.txt> <flow-id> <launch 0|1>");
  process.exit(1);
}
const [, , path, flowId, launchArg] = process.argv;
if (!/^[a-z0-9-]+$/.test(flowId)) die("flow id argv fails [a-z0-9-]+");
if (launchArg !== "0" && launchArg !== "1") die("launch argv must be 0|1");
const wantLaunch = launchArg === "1";

// THE BUILD PROFILE (review-r1 BLOCKER). `FOH_NETPLAY` (port/foh/foh.h) is
// a COMPILE-TIME switch: at 0 the battle submenu is unreachable and
// `VS. Melee` goes straight to the CSS; at 1 upstream's page 2 is back.
// The two graphs are MUTUALLY EXCLUSIVE, so registering both at once would
// accept a `menu-top>menu-battle>a` transition the shipped build can never
// emit — a false green. The flag is therefore parsed LIVE out of the
// header, exactly as verify-stream.js parses specVersion out of
// CHECKSUM.md: flipping the switch without re-freezing the traces fails
// mechanically instead of passing quietly.
const HDR = fs.readFileSync(require("path").join(__dirname, "foh.h"), "utf8");
// EXACTLY ONE definition, or die. A second `#define FOH_NETPLAY` (even a
// dead one inside an #if) would let the compiler take one profile while
// this judge took the other — a first-match parse is not fail-closed
// against duplicate input (review-r2 MAJOR).
const NET_DEFS = HDR.match(/^[ \t]*#[ \t]*define[ \t]+FOH_NETPLAY\b.*$/mg) || [];
if (NET_DEFS.length !== 1) {
  die("foh.h carries " + NET_DEFS.length + " FOH_NETPLAY definitions, want " +
      "exactly 1 (the build profile must be unambiguous)");
}
const M_NET = /^#define FOH_NETPLAY ([01])$/.exec(NET_DEFS[0]);
if (M_NET === null) {
  die("the FOH_NETPLAY definition is not exactly '#define FOH_NETPLAY [01]': '" +
      NET_DEFS[0] + "'");
}
const NETPLAY = M_NET[1] === "1";

// THE SECOND BUILD PROFILE — `FOH_CTL_CHOOSER` (DEVIATION D27, owner ruling
// 2026-08-23). Same kind of switch, same treatment, for the same reason: at
// 0 the Controls chooser is unreachable and the Options `CONTROLS` row opens
// the HANDHELD screen directly; at 1 upstream's menuMode-3 page is back.
// Registering both graphs at once would accept a `menu-options>
// menu-controls>a` the shipped build can never emit — the false green
// review-r1 blocked for FOH_NETPLAY. Parsed LIVE, and EXACTLY ONE
// definition or die, for the identical fail-closed reasons spelled out
// above.
const CTL_DEFS =
    HDR.match(/^[ \t]*#[ \t]*define[ \t]+FOH_CTL_CHOOSER\b.*$/mg) || [];
if (CTL_DEFS.length !== 1) {
  die("foh.h carries " + CTL_DEFS.length + " FOH_CTL_CHOOSER definitions, " +
      "want exactly 1 (the build profile must be unambiguous)");
}
const M_CTL = /^#define FOH_CTL_CHOOSER ([01])$/.exec(CTL_DEFS[0]);
if (M_CTL === null) {
  die("the FOH_CTL_CHOOSER definition is not exactly '#define " +
      "FOH_CTL_CHOOSER [01]': '" + CTL_DEFS[0] + "'");
}
const CHOOSER = M_CTL[1] === "1";

// The pinned faithful edge set (foh.h flow graph; upstream citations
// there). ANY other (from,to,cause) triple is corruption.
const EDGES = new Set([
  "startup>title>timer",
  "title>menu-top>start",
  "menu-top>menu-options>a",
  "menu-options>options-gameplay>a",
  "menu-options>menu-top>b",
  "options-gameplay>menu-options>b",
  "css>sss>start",
  // MENU-SPEC §4 — the audio options screen (gameMode 10, menu.js:130 in,
  // audiomenu.js:26 out with menuMode/menuSelected untouched).
  "menu-options>options-audio>a",
  "options-audio>menu-options>b",
  "sss>css>b",
  "sss>match>launch",
  // iter 99 (M4 task 12) — the target-test screen (upstream citations
  // in foh.h): menu.js:77-84 / targetselect.js:76-81 / :131-146.
  "menu-top>target-select>a",
  "target-select>menu-top>b",
  "target-select>target-match>launch",
  // A7 (MENU-SPEC §8) — the credits screen (gameMode 13). IN: menu.js:145-149
  // `setCreditsPlayer(i); changeGamemode(13)`. OUT: BOTH of credits.js's
  // exits go to gameMode 1 with menuMode/menuSelected untouched, so both land
  // back on Options with the cursor still on CREDITS — `b` is the manual exit
  // (:236-245) and `timer` is the unconditional 2500-frame one (:226-235,
  // cScrollingPos >= cScrollingMax), which is why the screen has two OUT
  // edges where every other screen has one.
  "menu-options>credits>a",
  "credits>menu-options>b",
  "credits>menu-options>timer",
]);
// PROFILE-DEPENDENT edges. Exactly one of these two blocks is legal in any
// given build, and which one is decided by the header above — never by
// whichever the trace happens to contain.
for (const e of NETPLAY
         ? ["menu-top>menu-battle>a", "menu-battle>css>a",
            "menu-battle>menu-top>b", "css>menu-battle>bhold"]
         // C5 (owner ruling 2026-07-28): `VS. Melee` runs menu.js:105's
         // Local VS action itself and the CSS's B-hold returns to the page
         // the player actually came from.
         : ["menu-top>css>a", "css>menu-top>bhold"]) {
  EDGES.add(e);
}
// The CONTROLS profile (DEVIATION D27, foh.h's FOH_CTL_CHOOSER). MENU-SPEC
// §9 — the CONTROLS page's two destinations (menu.js:155-157 / :159-161).
// At CHOOSER 1 the Options row opens the chooser and both destinations B
// back to it; at 0 the chooser is skipped in BOTH directions and the
// controller destination has no edge at all, because nothing on this device
// can reach it (A33: no USB host mode in the shipped OS image).
for (const e of CHOOSER
         ? ["menu-options>menu-controls>a", "menu-controls>menu-options>b",
            "menu-controls>controls-controller>a",
            "menu-controls>controls-keyboard>a",
            "controls-controller>menu-controls>b",
            "controls-keyboard>menu-controls>b"]
         : ["menu-options>controls-keyboard>a",
            "controls-keyboard>menu-options>b"]) {
  EDGES.add(e);
}
// Registered refusal tokens, each bound to the screen that emits it
// (review-r1 BLOCKER: a token registered globally passed on ANY screen).
// `audio`, `controller`, `keyboard` and — since A7 — `credits` are GONE, not
// merely unused: they are real screens now in every build (MENU-SPEC
// §4/§8/§9), so a trace carrying them is corruption, not history. The
// netplay three are legal only in the FOH_NETPLAY 1 build, where their page
// exists.
const REFUSED = new Map([
  ["targetbuilder", ["menu-top"]],   // conventions scope exclusion
  // iter 93 (M4 task 10): the SSS RANDOM slot — visible but refusing
  // (registered exclusion; upstream's arm draws from the SEEDED stream,
  // stageselect.js:80-84 — measured, AGENT-LOG iter 93).
  ["random", ["sss"]],
  // iter 99 (M4 task 12): `targettest` RETIRED (target-select is real);
  // the target-select "+ Add Code" slot refuses (builder/share-code
  // plane, scope-excluded — foh.h note).
  ["addcode", ["target-select"]],
  // CSS mechanics arc (MENU-SPEC items 2/3/4): the CSS can now reach port
  // configurations the LAUNCH plane cannot honour — a CPU port 0 (togglePort
  // has no port-0 special case, main.js:504-520) and the one-frame N/A race
  // that upstream's draw-pass readyToFight allows (css.js:1167-1181 vs
  // :446-451). sim_setup_match pins a human port 0, so START refuses loudly
  // instead of booting a different match than the record claims.
  ["portconfig", ["css"]],
]);
if (NETPLAY) {
  for (const t of ["spectate", "p2p", "server"]) REFUSED.set(t, ["menu-battle"]);
}

// FRAME/COUNT FIELDS ARE CANONICAL DECIMALS, FILE-WIDE (review-r14 MAJOR).
// Every numeric frame or count below is `(0|[1-9][0-9]*)`, never `[0-9]+`.
// The `%ld` producer cannot emit a leading zero and the normalizer already
// rejects one, so accepting `S 0400 turbo 1` here was a judge-side widening
// with nothing behind it. It is a TIGHTENING: no conforming producer output
// changes verdict, and leg [5b] carries a leading-zero negative for both of
// the forms this arc revised. (Generalized from the iter-101 review-99 L1
// ruling on TLAUNCH, which had this right for one line form only.)
const RE_T = /^T (0|[1-9][0-9]*) ([a-z-]+) ([a-z-]+) (timer|start|a|b|bhold|launch)$/;
// MENU-SPEC items 2/3/4 (CSS mechanics): `carry` is whichTokenGrabbed[0]
// (css.js:68) — the token-gesture state; the type fields gained N/A (-1) from
// DEVIATION D5's 3-cycle; P1 gained its own type + CPU level because
// togglePort has no port-0 special case (main.js:504-520).
// The value alphabet gained `10` for the two volume fields (tenths of the
// audiomenu step, MENU-SPEC §4). It is NOT a loosening of the existing
// fields: every field's real domain is pinned per-name in SVAL_DOM below
// and checked on every line, so `S 400 turbo 10` still dies.
// A44: the CSS is four ports wide, so the char and type planes gained
// p3char/p4char and p3type/p4type. The port number in the NAME is what
// carries the port, which is the point — foh.c indexes kCssCharField /
// kCssTypeField by the port it wrote, so a widened plane cannot report a
// write under the wrong port's field name. Domains are still pinned
// per-name in SVAL_DOM, and p3type/p4type's is NARROWER than p1/p2's.
const RE_S_NUM =
    /^S (0|[1-9][0-9]*) (p1char|p2char|p3char|p4char|p1type|p2type|p3type|p4type|p1difficulty|difficulty|p3difficulty|p4difficulty|carry|turbo|lcancel|flashlcancel|walljump|tapjump[1-4]|soundsvol|musicvol) (-1|10|[0-9])$/;
const RE_S_REF = /^S (0|[1-9][0-9]*) refused ([a-z0-9]+)$/;
const RE_SHOT = /^SHOT (0|[1-9][0-9]*) ([a-z0-9-]{1,32})$/;
// p1type/p1difficulty are real machine state and are traced as S events, but
// they are still NOT on this line: guard condition (1) at foh.c's launch arm
// pins a launched port 0 to HMN, so the record can never need those columns.
//
// A44 APPENDED p3/p4 and their types, and appended is the operative word —
// the fifteen fields up to and including `versus=` keep their names, their
// order and their domains, so the only change to a frozen pre-A44 line is
// the fixed suffix an absent pair emits. THE JUDGE IS NOT LOOSENED BY THIS:
// p3type/p4type are pinned to `(-1|0)`, which REFUSES the CPU value the
// p2type column accepts, because DEVIATION D40(b) keeps CPU off ports 2/3
// and the sim would have no AI replay for it (A46 OPEN/OWED). A trace
// claiming `p3type=1` is corruption here, not a configuration.
// A27: `versus` was the literal 0 until the mode ribbon became clickable,
// because the field could not be anything else. It is now the CSS ribbon's
// binary versusMode (css.js:393), so the domain widens to [01]. The frozen
// port/foh/flows/*.expect files still carry the exact byte 0 and are still
// cmp'd byte-for-byte by check-foh-flows.sh, whose own LAUNCH_RE reads only
// those frozen files and therefore still pins 0 exactly — the tightness
// moved to where the value really is fixed rather than being given up.
//
// A49 does two things to this line and neither loosens it by accident.
// (1) p3type/p4type widen from (-1|0) to (-1|[01]): the owner retired
//     DEVIATION D40(b), so CPU is a reachable type on ports 2/3. They are
//     now exactly p2type's domain, which is what "four ports, one rule"
//     means; the tightness that is really gone is a refusal, not a check.
// (2) p3difficulty/p4difficulty are APPENDED, never inserted, and carry
//     p2's [1-4] slider domain. Without them two matches that differ only
//     in a CPU level on port 2/3 would emit identical LAUNCH lines.
const RE_LAUNCH =
    /^LAUNCH (0|[1-9][0-9]*) p1=([0-4]) p2=([0-4]) p2type=(-1|[01]) difficulty=([1-4]) stage=([0-5]) turbo=([01]) lcancel=([012]) flashlcancel=([01]) walljump=([01]) tapjump=([01]),([01]),([01]),([01]) versus=([01]) p3=([0-4]) p4=([0-4]) p3type=(-1|[01]) p4type=(-1|[01]) p3difficulty=([1-4]) p4difficulty=([1-4])$/;
// iter 99 (M4 task 12): the target-mode launch record (foh.h TLAUNCH
// note; char domain 0-4, tstage domain 0-9 == targetStageMapping).
// iter 101 (review-99 L1): frame field is a CANONICAL decimal in the
// judging layer itself — `TLAUNCH 0405` is corruption here, never the
// normalizer backstop's problem. As of review-r14 that rule is file-wide
// (see the note above RE_T); this line form was simply first.
const RE_TLAUNCH = /^TLAUNCH (0|[1-9][0-9]*) char=([0-4]) tstage=([0-9])$/;
const RE_END = /^END (0|[1-9][0-9]*) transitions=(0|[1-9][0-9]*)$/;

const raw = fs.readFileSync(path, "utf8");
if (raw.length === 0) die("empty trace");
if (!raw.endsWith("\n")) die("missing trailing newline (torn write)");
const lines = raw.slice(0, -1).split("\n");
if (lines[0] !== "FOHTRACE1 flow=" + flowId) {
  die("header line is not exactly 'FOHTRACE1 flow=" + flowId + "': '" +
      lines[0] + "'");
}

let lastFrame = 0;
let lastScreen = "startup";
let tCount = 0;
let sawEnd = false;
let launches = 0;
let prevWasLaunchT = false;
let prevLaunchScreen = "";
let prevLaunchFrame = -1;
const shotNames = new Set();

const SVAL_DOM = {
  p1char: [0, 4], p2char: [0, 4], p3char: [0, 4], p4char: [0, 4],
  // -1 N/A, 0 HMN, 1 CPU (main.js:504-520 with DEVIATION D5's NET dropped).
  // A49: ONE domain for all four ports. A44's D40(b) stopped ports 2/3 at
  // HMN; the owner retired it, so CPU is reachable everywhere and the four
  // rows really are copies now.
  p1type: [-1, 1], p2type: [-1, 1], p3type: [-1, 1], p4type: [-1, 1],
  // whichTokenGrabbed[0]: -1 = empty-handed, else the port whose token is
  // held. A44/D40 lets the one hand hold any of the four.
  carry: [-1, 3],
  // Four CPU levels, upstream's own width (cpuDifficulty = [3,3,3,3],
  // main.js:109). Port 1's keeps the bare name `difficulty` every frozen
  // trace already means by it; A49 APPENDED ports 2/3 rather than renaming.
  p1difficulty: [1, 4], difficulty: [1, 4],
  p3difficulty: [1, 4], p4difficulty: [1, 4],
  turbo: [0, 1], lcancel: [0, 2],
  // MENU-SPEC §3.1's completed row list: rows 2 and 3 (gameplaymenu.js:
  // 50/:53, both `^= true` so integer 0/1).
  flashlcancel: [0, 1], walljump: [0, 1],
  tapjump1: [0, 1], tapjump2: [0, 1], tapjump3: [0, 1], tapjump4: [0, 1],
  // MENU-SPEC §4: masterVolume in TENTHS. The machine keeps the raw
  // unrounded double (audiomenu.js:103/:109 never round); the structural
  // plane carries round(v*10), clamped by the same [0,1] the menu clamps.
  soundsvol: [0, 10], musicvol: [0, 10],
};

// Which screen may emit each field (review-r1 BLOCKER: the domain pin said
// WHAT a value may be but never WHERE it may appear, so `S 5 turbo 1`
// during the startup timer passed). The machine's emission sites are the
// authority; `lastScreen` is already tracked for the T-chain.
const SFIELD_SCREENS = {
  // the CSS token gesture writes both planes at the hover site
  // (css.js:222-226); target-select's shoulder arms write the SHARED
  // characterSelections[0] (targetselect.js:60-74).
  p1char: ["css", "target-select"], p2char: ["css"],
  p3char: ["css"], p4char: ["css"],
  p1type: ["css"], p2type: ["css"], p3type: ["css"], p4type: ["css"],
  p1difficulty: ["css"], difficulty: ["css"],
  p3difficulty: ["css"], p4difficulty: ["css"], carry: ["css"],
  turbo: ["options-gameplay"], lcancel: ["options-gameplay"],
  flashlcancel: ["options-gameplay"], walljump: ["options-gameplay"],
  tapjump1: ["options-gameplay"], tapjump2: ["options-gameplay"],
  tapjump3: ["options-gameplay"], tapjump4: ["options-gameplay"],
  soundsvol: ["options-audio"], musicvol: ["options-audio"],
};

for (let k = 1; k < lines.length; k++) {
  const ln = lines[k];
  if (sawEnd) die("content after END at line " + (k + 1));
  let m;
  if ((m = RE_T.exec(ln)) !== null) {
    const f = Number(m[1]);
    if (f < lastFrame) die("T frame regressed at line " + (k + 1));
    lastFrame = f;
    if (m[2] !== lastScreen) {
      die("T-chain break at line " + (k + 1) + ": departs '" + m[2] +
          "' but the machine is on '" + lastScreen + "'");
    }
    const edge = m[2] + ">" + m[3] + ">" + m[4];
    if (!EDGES.has(edge)) die("off-graph transition '" + edge + "' at line " + (k + 1));
    lastScreen = m[3];
    tCount++;
    prevWasLaunchT = m[4] === "launch";
    prevLaunchScreen = m[3];
    prevLaunchFrame = f;
    continue;
  }
  if ((m = RE_LAUNCH.exec(ln)) !== null) {
    const f = Number(m[1]);
    if (!prevWasLaunchT || prevLaunchScreen !== "match" ||
        f !== prevLaunchFrame) {
      die("LAUNCH at line " + (k + 1) +
          " is not immediately preceded by its 'T <f> sss match launch'");
    }
    if (f < lastFrame) die("LAUNCH frame regressed at line " + (k + 1));
    lastFrame = f;
    launches++;
    if (launches > 1) die("multiple LAUNCH/TLAUNCH lines");
    prevWasLaunchT = false;
    continue;
  }
  if ((m = RE_TLAUNCH.exec(ln)) !== null) {
    const f = Number(m[1]);
    if (!prevWasLaunchT || prevLaunchScreen !== "target-match" ||
        f !== prevLaunchFrame) {
      die("TLAUNCH at line " + (k + 1) + " is not immediately preceded " +
          "by its 'T <f> target-select target-match launch'");
    }
    if (f < lastFrame) die("TLAUNCH frame regressed at line " + (k + 1));
    lastFrame = f;
    launches++;
    if (launches > 1) die("multiple LAUNCH/TLAUNCH lines");
    prevWasLaunchT = false;
    continue;
  }
  prevWasLaunchT = false;
  if ((m = RE_S_NUM.exec(ln)) !== null) {
    const f = Number(m[1]);
    const v = Number(m[3]);
    if (f < lastFrame) die("S frame regressed at line " + (k + 1));
    lastFrame = f;
    const dom = SVAL_DOM[m[2]];
    if (v < dom[0] || v > dom[1]) {
      die("S value " + v + " outside the pinned domain of " + m[2] +
          " at line " + (k + 1));
    }
    const scr = SFIELD_SCREENS[m[2]];
    if (scr.indexOf(lastScreen) === -1) {
      die("S field '" + m[2] + "' at line " + (k + 1) + " is emitted on '" +
          lastScreen + "', which cannot write it (legal: " + scr.join(",") +
          ")");
    }
    continue;
  }
  if ((m = RE_S_REF.exec(ln)) !== null) {
    const f = Number(m[1]);
    if (f < lastFrame) die("S frame regressed at line " + (k + 1));
    lastFrame = f;
    if (!REFUSED.has(m[2])) {
      die("unregistered refused entry '" + m[2] + "' at line " + (k + 1) +
          " (FOH_NETPLAY=" + (NETPLAY ? "1" : "0") + ")");
    }
    if (REFUSED.get(m[2]).indexOf(lastScreen) === -1) {
      die("refusal '" + m[2] + "' at line " + (k + 1) + " is emitted on '" +
          lastScreen + "', which cannot refuse it (legal: " +
          REFUSED.get(m[2]).join(",") + ")");
    }
    continue;
  }
  if ((m = RE_SHOT.exec(ln)) !== null) {
    const f = Number(m[1]);
    if (f < lastFrame) die("SHOT frame regressed at line " + (k + 1));
    lastFrame = f;
    if (shotNames.has(m[2])) die("duplicate SHOT name at line " + (k + 1));
    shotNames.add(m[2]);
    continue;
  }
  if ((m = RE_END.exec(ln)) !== null) {
    const f = Number(m[1]);
    if (f < lastFrame) die("END frame below the last event frame");
    if (Number(m[2]) !== tCount) {
      die("END transitions=" + m[2] + " but " + tCount + " T lines counted");
    }
    sawEnd = true;
    continue;
  }
  // Resemblance = same leading keyword, failed full grammar: corruption
  // by construction (no partial parses, no silent skips).
  die("line " + (k + 1) + " matches no FOHTRACE1 form: '" + ln + "'");
}
if (!sawEnd) die("no END line (truncated trace)");
// Tier A+ round-6 MINOR-1: this is an equality on the COUNT, deliberately not
// the boolean identity `wantLaunch !== (launches === 1)`. That form is only
// sound while the earlier "more than one LAUNCH" rule fires first; delete that
// rule and it silently accepts launches=2 for wantLaunch=0 (and prints its own
// OK line reading launch=2). This form states the requirement without
// depending on another check having already fired.
if (launches !== (wantLaunch ? 1 : 0)) {
  die("launch expectation: want " + (wantLaunch ? 1 : 0) + ", saw " + launches);
}
console.log("FOH TRACE GRAMMAR OK " + flowId + " (transitions=" + tCount +
            ", shots=" + shotNames.size + ", launch=" + launches + ")");
