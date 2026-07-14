/*eslint-disable */
// =============================================================================
// FunKey-S control-mapping prototype (meleelight-funkey-s wayfinder ticket #9)
//
// Keyboard stands in for the FunKey-S's physical controls (all digital):
//   d-pad U/D/L/R, A, B, X, Y, L, R, Start, Fn.
// Virtual button state is routed through a data-driven digital->analog
// mapping layer (schemes S1/S2/S3 from docs/research/b0xx-mapping.md) that
// emits FINAL Melee-unit coordinates (1/80-quantized), injected at the
// pollInputs seam — deliberately BYPASSING pollKeyboardInputs' tasRescale
// (which would saturate modifier values back to 1.0).
//
// Scheme select: URL ?scheme=s1|s2|s3 (default s1), F8 cycles at runtime,
// or window.__funkey.setScheme("s2"). Disable layer entirely: ?funkey=off.
// F9 toggles the debug overlay. ?tapjump=on re-enables tap jump (default:
// forced off for the FunKey player, since digital up = 1.0 would tap-jump
// on every upward DI).
// =============================================================================

import {deaden, meleeRound} from "../meleeInputs";
import {gameSettings} from "../../settings";
import {startOverlay} from "./funkeyOverlay";

// -----------------------------------------------------------------------------
// KEYBOARD -> VIRTUAL FUNKEY BUTTON BINDING (edit freely; keycodes: keycode.info)
//
// Physical FunKey-S face diamond is X on top, Y left, A right, B bottom;
// the WASD cluster mirrors that spatially under the left hand, arrows = d-pad
// under the right hand, Q/E = the L/R shoulders flanking W.
// -----------------------------------------------------------------------------
export const KEYBINDS = {
  DU: 38,    // ArrowUp     -> d-pad up
  DD: 40,    // ArrowDown   -> d-pad down
  DL: 37,    // ArrowLeft   -> d-pad left
  DR: 39,    // ArrowRight  -> d-pad right
  X:  87,    // W  -> FunKey X (top of diamond)
  Y:  65,    // A  -> FunKey Y (left of diamond)
  A:  68,    // D  -> FunKey A (right of diamond)
  B:  83,    // S  -> FunKey B (bottom of diamond)
  L:  81,    // Q  -> FunKey L shoulder
  R:  69,    // E  -> FunKey R shoulder
  START: 13, // Enter -> FunKey Start
  FN: 70     // F  -> FunKey Fn/Menu (reserved by FunKey OS; no scheme uses it)
};

// Lab-only keys (NOT FunKey buttons):
var KEY_CYCLE_SCHEME  = 119; // F8
var KEY_TOGGLE_OVERLAY = 120; // F9

// -----------------------------------------------------------------------------
// Modifier coordinate tables (Melee units, all on the 1/80 grid).
// Values verbatim from docs/research/b0xx-mapping.md §2.2/§5 (HayBox
// Melee20Button.cpp lineage). Magnitudes only; signs come from d-pad state.
// -----------------------------------------------------------------------------
var COORD = {
  cardinal:            1.0,
  diagonal:            [0.7000, 0.7000],
  shieldDropDiagonal:  [0.7000, 0.6875], // shield + down-diagonal (y negated)
  modX: {
    horizontal:        0.6625,           // walk / f-tilt band
    vertical:          0.5375,           // u-tilt / d-tilt band
    diagonal:          [0.7375, 0.3125], // ~23 deg shallow angle
    shieldDiagonal:    [0.6375, 0.3750]  // ~30.5 deg wavedash/airdodge
  },
  modY: {
    horizontal:        0.3375,           // slowest walk
    vertical:          0.7375,
    diagonal:          [0.3125, 0.7375], // ~67 deg steep angle
    shieldDiagonalUp:  [0.4750, 0.8750], // ~61.5 deg upward airdodge
    shieldDiagonalDown:[0.5000, 0.8500]  // ~59.5 deg steep wavedash
  },
  clayerCardinal:      1.0,
  clayerDiagonal:      [0.7000, 0.7000]
};

// -----------------------------------------------------------------------------
// Schemes: virtual FunKey button -> function role. Data only — the resolver
// below is shared. Roles: attack special jump shield grab start
//                         mod (S1/S3 single mod == ModX family)
//                         modx / mody (S2)
//                         clayer (hold: d-pad drives C-stick, ls -> neutral)
// S2 has no clayer button: holding BOTH shoulders (modx+mody) is the C-layer.
// -----------------------------------------------------------------------------
export var SCHEMES = {
  s1: {
    name: "S1 One-Mod + C-layer",
    roles: { A: "attack", B: "special", X: "jump", Y: "clayer",
             L: "mod",    R: "shield",  START: "start", FN: null },
    clayerChord: null
  },
  s2: {
    name: "S2 Dual-Mod (B0XX-faithful angles)",
    roles: { A: "attack", B: "special", X: "jump", Y: "shield",
             L: "modx",   R: "mody",    START: "start", FN: null },
    clayerChord: ["L", "R"] // both shoulders held = C-layer
  },
  s3: {
    name: "S3 Minimal (dedicated grab, no C-stick)",
    roles: { A: "attack", B: "special", X: "jump", Y: "grab",
             L: "mod",    R: "shield",  START: "start", FN: null },
    clayerChord: null
  }
};

// -----------------------------------------------------------------------------
// State
// -----------------------------------------------------------------------------
var enabled = true;
var schemeId = "s1";
var overlayWanted = true;
var tapJumpForceOff = true;

if (typeof window !== "undefined") {
  try {
    var qs = window.location.search;
    var m = qs.match(/[?&]scheme=(s[123])/i);
    if (m) { schemeId = m[1].toLowerCase(); }
    if (/[?&]funkey=off/i.test(qs)) { enabled = false; }
    if (/[?&]overlay=off/i.test(qs)) { overlayWanted = false; }
    if (/[?&]tapjump=on/i.test(qs)) { tapJumpForceOff = false; }
  } catch (e) { /* no window/location: leave defaults */ }
}

// last resolved state, for the debug overlay
export var funkeyState = {
  scheme: schemeId,
  schemeName: SCHEMES[schemeId].name,
  buttons: {},   // virtual FunKey buttons currently held
  dx: 0, dy: 0,  // SOCD-resolved d-pad
  flags: { mod: false, modx: false, mody: false, clayer: false, shield: false },
  family: "",    // which coordinate family produced the stick value
  input: null    // the Input object handed to the sim this poll
};

export function funkeyActive () {
  return enabled;
}

export function setScheme (id) {
  if (SCHEMES[id]) {
    schemeId = id;
    funkeyState.scheme = id;
    funkeyState.schemeName = SCHEMES[id].name;
  }
  return schemeId;
}

function cycleScheme () {
  var order = ["s1", "s2", "s3"];
  setScheme(order[(order.indexOf(schemeId) + 1) % order.length]);
}

// -----------------------------------------------------------------------------
// Core resolver: virtual button state -> final Melee-unit Input fields
// -----------------------------------------------------------------------------
function readVirtualButtons (keys) {
  var b = {};
  for (var name in KEYBINDS) {
    b[name] = keys[KEYBINDS[name]] ? true : false;
  }
  return b;
}

function q (x) { return meleeRound(x); } // clamp to the 1/80 grid

export function resolveFunkey (b, scheme) {
  // SOCD: FunKey d-pad is a cross (opposites unpressable); on a keyboard we
  // resolve opposite cardinals to NEUTRAL (meleelight's own keyboard policy).
  var dx = (b.DR ? 1 : 0) - (b.DL ? 1 : 0);
  var dy = (b.DU ? 1 : 0) - (b.DD ? 1 : 0);
  if (b.DR && b.DL) { dx = 0; }
  if (b.DU && b.DD) { dy = 0; }

  var roles = scheme.roles;
  var has = function (role) {
    for (var name in roles) { if (roles[name] === role && b[name]) { return true; } }
    return false;
  };

  var shield = has("shield");
  var mod    = has("mod");   // S1/S3 single mod == ModX family
  var modx   = has("modx");
  var mody   = has("mody");
  var clayer = has("clayer");
  if (scheme.clayerChord && b[scheme.clayerChord[0]] && b[scheme.clayerChord[1]]) {
    clayer = true;           // S2: both shoulders held
    modx = false; mody = false;
  }

  var lsX = 0, lsY = 0, csX = 0, csY = 0;
  var family = "neutral";

  if (clayer) {
    // C-layer: d-pad drives the C-stick; left stick goes neutral (the d-pad
    // cannot serve both — documented S1 sacrifice: drift stops during C-layer).
    family = "c-layer";
    if (dx !== 0 && dy !== 0) {
      csX = dx * COORD.clayerDiagonal[0]; csY = dy * COORD.clayerDiagonal[1];
    } else {
      csX = dx * COORD.clayerCardinal;    csY = dy * COORD.clayerCardinal;
    }
  } else if (dx !== 0 || dy !== 0) {
    var mag;
    var useModX = mod || modx;
    if (dx !== 0 && dy !== 0) {                          // ---- diagonals ----
      if (useModX) {
        mag = shield ? COORD.modX.shieldDiagonal : COORD.modX.diagonal;
        family = shield ? "modX+shield diag " + fmtPair(mag) : "modX diag " + fmtPair(mag);
      } else if (mody) {
        if (shield) {
          mag = dy > 0 ? COORD.modY.shieldDiagonalUp : COORD.modY.shieldDiagonalDown;
          family = "modY+shield diag " + fmtPair(mag);
        } else {
          mag = COORD.modY.diagonal;
          family = "modY diag " + fmtPair(mag);
        }
      } else if (shield && dy < 0) {
        mag = COORD.shieldDropDiagonal;                  // shield-drop band
        family = "shield-drop (0.7000,-0.6875)";
      } else {
        mag = COORD.diagonal;
        family = "diagonal (0.7,0.7)";
      }
      lsX = dx * mag[0]; lsY = dy * mag[1];
    } else if (dx !== 0) {                               // ---- horizontal ----
      if (useModX)      { lsX = dx * COORD.modX.horizontal; family = "modX walk/f-tilt 0.6625"; }
      else if (mody)    { lsX = dx * COORD.modY.horizontal; family = "modY slow walk 0.3375"; }
      else              { lsX = dx * COORD.cardinal;        family = "cardinal 1.0 (dash/smash)"; }
    } else {                                             // ---- vertical ----
      if (useModX)      { lsY = dy * COORD.modX.vertical;   family = "modX u/d-tilt 0.5375"; }
      else if (mody)    { lsY = dy * COORD.modY.vertical;   family = "modY vertical 0.7375"; }
      else              { lsY = dy * COORD.cardinal;        family = "cardinal 1.0"; }
    }
  }

  return {
    dx: dx, dy: dy,
    lsX: q(lsX), lsY: q(lsY), csX: q(csX), csY: q(csY),
    a: has("attack"), bSpec: has("special"), jump: has("jump"),
    shield: shield, grab: has("grab"), start: has("start"),
    flags: { mod: mod, modx: modx, mody: mody, clayer: clayer, shield: shield },
    family: family
  };
}

function fmtPair (p) { return "(" + p[0] + "," + p[1] + ")"; }

// -----------------------------------------------------------------------------
// pollInputs seam entry point. Returns a complete meleelight Input object with
// FINAL Melee-unit coordinates (no tasRescale anywhere in this path).
// -----------------------------------------------------------------------------
export function funkeyPoll (gameMode, frameByFrame, playerSlot, keys) {
  var scheme = SCHEMES[schemeId];
  var b = readVirtualButtons(keys);
  var r = resolveFunkey(b, scheme);

  // Engine-settings assist (b0xx-mapping.md §4): digital up = 1.0 would
  // tap-jump on every upward DI, so force tap jump off for this player.
  if (tapJumpForceOff && typeof playerSlot === "number") {
    gameSettings["tapJumpOffp" + (playerSlot + 1)] = 1;
  }

  var input = {
    a: r.a, b: r.bSpec, x: r.jump, y: false, z: false,
    l: false, r: false, s: r.start,
    du: false, dl: false, dr: false, dd: false,
    lsX: deaden(r.lsX), lsY: deaden(r.lsY),
    csX: deaden(r.csX), csY: deaden(r.csY),
    lA: 0, rA: 0,
    rawX: r.lsX, rawY: r.lsY, rawcsX: r.csX, rawcsY: r.csY
  };

  if (r.shield) {           // digital shield: full analog + digital press
    input.r = true;
    input.rA = 1;
  }
  if (r.grab) {             // S3 dedicated grab: Z = A + lightest analog press
    input.z = true;
    if (!frameByFrame && gameMode !== 4 && gameMode !== 14) {
      input.a = true;
      if (input.lA < 0.35) { input.lA = 0.35; }
    }
  }

  // stash for overlay
  funkeyState.debug = { schemeId: schemeId, rolesY: scheme.roles.Y, bY: b.Y,
                        rGrab: r.grab, gm: gameMode, slot: playerSlot };
  funkeyState.buttons = b;
  funkeyState.dx = r.dx; funkeyState.dy = r.dy;
  funkeyState.flags = r.flags;
  funkeyState.family = r.family;
  funkeyState.input = input;

  ensureOverlay();
  return input;
}

// -----------------------------------------------------------------------------
// Debug overlay + lab hotkeys (lazy init on first poll)
// -----------------------------------------------------------------------------
var overlayStarted = false;
function ensureOverlay () {
  if (overlayStarted || typeof window === "undefined") { return; }
  overlayStarted = true;
  window.addEventListener("keydown", function (e) {
    if (e.keyCode === KEY_CYCLE_SCHEME) { cycleScheme(); }
    if (e.keyCode === KEY_TOGGLE_OVERLAY) {
      overlayWanted = !overlayWanted;
      var el = document.getElementById("funkeyOverlay");
      if (el) { el.style.display = overlayWanted ? "block" : "none"; }
    }
  });
  startOverlay(funkeyState, function () { return overlayWanted; });
}

if (typeof window !== "undefined") {
  window.__funkey = {
    setScheme: setScheme,
    getScheme: function () { return schemeId; },
    state: funkeyState,
    keybinds: KEYBINDS,
    schemes: SCHEMES,
    setEnabled: function (v) { enabled = !!v; },
    isEnabled: funkeyActive,
    // direct resolver access for tests/debugging (virtual buttons in, coords out)
    resolve: function (b, id) { return resolveFunkey(b, SCHEMES[id || schemeId]); }
  };
}
