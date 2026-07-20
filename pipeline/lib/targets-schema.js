"use strict";
// Pinned schema + executed-JS walk for the AUTHORED target-test stage
// extractor output (fix_plan §M4 task 11; format TTAB1,
// pipeline/FORMATS.md §6).
//
// SINGLE SOURCE OF TRUTH on the JS side for target-stage field
// typing/ordering. pipeline/lib/targets_check.c walks the COMPILED
// generated tables and must print the byte-identical canonical dump
// (dumpTargetStages below) — any schema/emission drift becomes a byte
// diff in check-targets.sh.
//
// SHAPE (measured at the pin, iter 94 — the INVERSE of STAB1's VS
// posture): box[] and target[] are PAYLOAD here (box = the drawn/authored
// AABBs, target = the breakable target centers targetplay.js collides
// against radius size+7); polygonMap/polygon, name, respawnPoints/
// respawnFace/startingFace, movingPlats/movingPlatforms and connected
// are ABSENT on every authored target stage — each pinned absent by the
// exact key set below (an appearance = hard-throw = format bump
// territory, never droppable). ledgePos is OPTIONAL: EXACTLY ONE authored
// stage carries it (targetstage9, 1 entry parallel to its 1 ledge —
// found by this schema's own hard-throw at the first executed walk; the
// space-before-colon in `ledgePos :` had defeated the static grep).
// It is AI-only upstream (ai.js:890) and target mode fields no CPU, but
// the data plane carries it VERBATIM (faithfulness > minimality — the
// fdest ledgePos-quirk precedent). damageType: ZERO authored stages (VS
// or target) carry a SurfaceProperties third element — asSurface
// hard-throws on one exactly like stages-schema.js.
//
// Typing is MEASURED-THEN-PINNED: int32-listed fields hard-throw on any
// non-integer executed value; geometry coords are IEEE-754 double bits.

const {
  f64bits, asF64, asI32, loadExtractor,
} = require("./tables-schema");

// tstageId order == upstream src/stages/activeStage.js
// targetStageMapping (0..9 -> targetstage1..targetstage10) == the
// setActiveStageTarget(val) selection domain (targetselect.js:143).
const TSTAGE_NAMES = [
  "targetstage1", "targetstage2", "targetstage3", "targetstage4",
  "targetstage5", "targetstage6", "targetstage7", "targetstage8",
  "targetstage9", "targetstage10",
];

// Surface kinds, pinned order (struct + dump + coverage order; the same
// order STAB1 pins — the C sim's stage builder consumes both).
const SURF_KINDS = ["ground", "platform", "ceiling", "wallL", "wallR"];

// Ledge surface types (upstream type Ledge = [ "ground"|"platform", i, side ]).
const LEDGE_TYPES = { ground: 0, platform: 1 };

// Exact key set per authored target stage object (measured at the pin
// over all 10 files; hard-throw on drift). ledgePos: only targetstage9.
const TSTAGE_KEYS = ["startingPoint", "box", "ground", "ceiling", "wallL",
  "wallR", "platform", "ledge", "target", "scale", "blastzone", "offset"];
const TSTAGE_KEYS_OPTIONAL = ["ledgePos"];

function loadTargetStages(distRoot) {
  const { win, srcSha256 } = loadExtractor(distRoot);
  const tstages = win.__targetStages;
  if (!tstages) {
    throw new Error("extractor bundle did not assign window.__targetStages");
  }
  return { tstages, srcSha256 };
}

function asVec2(where, p) {
  if (!p || typeof p.x !== "number" || typeof p.y !== "number") {
    throw new Error(`${where}: not a Vec2D`);
  }
  return { x: asF64(where + ".x", p.x), y: asF64(where + ".y", p.y) };
}

// Surface = [Vec2D, Vec2D]; the optional third SurfaceProperties element
// (damageType) exists on NO authored stage (measured iter 94) — its
// appearance hard-throws (format bump, never droppable).
function asSurface(where, s) {
  if (!Array.isArray(s) || s.length !== 2) {
    throw new Error(`${where}: expected [Vec2D, Vec2D] (got length ` +
      `${Array.isArray(s) ? s.length : typeof s}; a SurfaceProperties ` +
      `third element is a format change, never droppable)`);
  }
  return [asVec2(where + "[0]", s[0]), asVec2(where + "[1]", s[1])];
}

function asBox2(where, b) {
  if (!b || !b.min || !b.max) throw new Error(`${where}: not a Box2D`);
  return {
    min: asVec2(where + ".min", b.min),
    max: asVec2(where + ".max", b.max),
  };
}

// Walk + validate the live tstages registry into the canonical TTAB1
// model. Hard-throws on any surprise.
function buildTargetStageModel(tstagesObj) {
  const gotNames = Object.keys(tstagesObj).sort();
  const wantNames = [...TSTAGE_NAMES].sort();
  if (gotNames.join(",") !== wantNames.join(",")) {
    throw new Error(`target stage key set drifted:\n got ${gotNames}\nwant ${wantNames}`);
  }

  const stages = [];
  for (let tstageId = 0; tstageId < TSTAGE_NAMES.length; tstageId++) {
    const sn = TSTAGE_NAMES[tstageId];
    const st = tstagesObj[sn];
    const where = (s) => `${sn}/${s}`;

    // -- exact key set (absence pins for name/polygon/respawn*/
    //    startingFace/ledgePos/movingPlats/movingPlatforms/connected
    //    are implied: any unexpected key throws, all 12 required) --
    const keys = Object.keys(st);
    for (const k of keys) {
      if (!TSTAGE_KEYS.includes(k) && !TSTAGE_KEYS_OPTIONAL.includes(k)) {
        throw new Error(`${sn}: unexpected target-stage key ${k}`);
      }
    }
    for (const k of TSTAGE_KEYS) {
      if (!keys.includes(k)) throw new Error(`${sn}: missing target-stage key ${k}`);
    }

    const model = { tstageId, name: sn };

    // -- startingPoint (pinned length 1: startTargetGame reads ONLY
    //    startingPoint[0], targetplay.js:196; initializePlayers(p,true)
    //    entrance vfx likewise) --
    if (!Array.isArray(st.startingPoint) || st.startingPoint.length !== 1) {
      throw new Error(`${sn}: startingPoint must have exactly 1 entry`);
    }
    model.startingPoint = [asVec2(where("startingPoint[0]"), st.startingPoint[0])];

    // -- box (authored AABBs; PAYLOAD here, unlike VS stages) --
    if (!Array.isArray(st.box)) throw new Error(`${sn}: box not an array`);
    model.box = st.box.map((b, i) => asBox2(where(`box[${i}]`), b));

    // -- surfaces --
    for (const kind of SURF_KINDS) {
      if (!Array.isArray(st[kind])) throw new Error(`${sn}: ${kind} not an array`);
      model[kind] = st[kind].map((s, i) => asSurface(where(`${kind}[${i}]`), s));
    }

    // -- ledges (NO ledgePos on target stages — pinned absent above) --
    if (!Array.isArray(st.ledge)) throw new Error(`${sn}: ledge not an array`);
    model.ledge = st.ledge.map((l, i) => {
      if (!Array.isArray(l) || l.length !== 3) {
        throw new Error(`${sn}: ledge[${i}] is not [type,index,side]`);
      }
      const [type, index, side] = l;
      if (!(type in LEDGE_TYPES)) throw new Error(`${sn}: ledge[${i}] type ${type}`);
      const idx = asI32(where(`ledge[${i}].index`), index);
      if (idx < 0 || idx >= st[type].length) {
        throw new Error(`${sn}: ledge[${i}] index ${idx} out of ${type} range`);
      }
      if (side !== 0 && side !== 1) throw new Error(`${sn}: ledge[${i}] side ${side}`);
      return { type: LEDGE_TYPES[type], index: idx, side };
    });

    // -- ledgePos (OPTIONAL; measured present on targetstage9 ONLY,
    //    parallel-length to ledge — carried verbatim, AI-only upstream) --
    if (st.ledgePos !== undefined) {
      if (!Array.isArray(st.ledgePos) || st.ledgePos.length !== st.ledge.length) {
        throw new Error(`${sn}: ledgePos present but not parallel to ledge`);
      }
      model.ledgePos = st.ledgePos.map((p, i) => asVec2(where(`ledgePos[${i}]`), p));
    } else {
      model.ledgePos = null;
    }

    // -- targets (breakable centers; radius is the CODE literal 7 in
    //    targetplay.js, not data) --
    if (!Array.isArray(st.target) || st.target.length < 1) {
      throw new Error(`${sn}: target must be a nonempty array`);
    }
    model.target = st.target.map((p, i) => asVec2(where(`target[${i}]`), p));

    // -- scale (f64) + blastzone (Box2D) + offset (int32[2]) --
    model.scale = asF64(where("scale"), st.scale);
    model.blastzone = asBox2(where("blastzone"), st.blastzone);
    if (!Array.isArray(st.offset) || st.offset.length !== 2) {
      throw new Error(`${sn}: offset is not [x,y]`);
    }
    model.offset = st.offset.map((v, i) => asI32(where(`offset[${i}]`), v));

    stages.push(model);
  }
  return { formatVersion: 1, stages };
}

// Canonical leaf dump (FORMATS.md §6): one line per leaf value, the
// EXACT byte stream pipeline/lib/targets_check.c prints from the
// compiled C tables. Doubles as 16 lowercase hex digits of their
// IEEE-754 bit pattern; ints as decimal.
function dumpTargetStages(model, emit) {
  const v2 = (p) => `${p.x.bits},${p.y.bits}`;
  for (const st of model.stages) {
    const sn = st.name;
    st.startingPoint.forEach((p, i) =>
      emit(`tstage/${sn}/startingPoint[${i}]=${v2(p)}`));
    st.box.forEach((b, i) =>
      emit(`tstage/${sn}/box[${i}]=${v2(b.min)},${v2(b.max)}`));
    for (const kind of SURF_KINDS) {
      st[kind].forEach((s, i) =>
        emit(`tstage/${sn}/${kind}[${i}]=${v2(s[0])},${v2(s[1])}`));
    }
    st.ledge.forEach((l, i) =>
      emit(`tstage/${sn}/ledge[${i}]=${l.type},${l.index},${l.side}`));
    emit(`tstage/${sn}/hasLedgePos=${st.ledgePos ? 1 : 0}`);
    if (st.ledgePos) {
      st.ledgePos.forEach((p, i) => emit(`tstage/${sn}/ledgePos[${i}]=${v2(p)}`));
    }
    st.target.forEach((p, i) => emit(`tstage/${sn}/target[${i}]=${v2(p)}`));
    emit(`tstage/${sn}/scale=${st.scale.bits}`);
    emit(`tstage/${sn}/blastzone=${v2(st.blastzone.min)},${v2(st.blastzone.max)}`);
    emit(`tstage/${sn}/offset=${st.offset[0]},${st.offset[1]}`);
  }
}

module.exports = {
  TSTAGE_NAMES, SURF_KINDS, LEDGE_TYPES,
  loadTargetStages, buildTargetStageModel, dumpTargetStages, f64bits,
};
