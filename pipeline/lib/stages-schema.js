"use strict";
// Pinned schema + executed-JS walk for the VS-stage geometry extractor
// output (fix_plan §M1 task 3; format STAB1, pipeline/FORMATS.md §4).
//
// SINGLE SOURCE OF TRUTH on the JS side for stage field typing/ordering.
// pipeline/lib/stages_check.c walks the COMPILED generated tables and must
// print the byte-identical canonical dump (dumpStages below) — any
// schema/emission drift becomes a byte diff in check-stages.sh.
//
// ONE-SOURCE-OF-TRUTH note (PLAN §4 M1): upstream renders stages from the
// SAME structures the collision engine consumes (stagerender.js draws
// polygon/ground/walls/ceiling/platform/blastzone/ledge; physics.js
// collides against ground/platform/ceiling/wallL/wallR/connected/ledge)
// — so these tables serve both the M2 sim and the M3 renderer. The
// movingPlatforms function bodies are sim LOGIC, ported with M2 (like
// setVelocities, FORMATS.md §3.4); their data (movingPlats indices,
// platform rest positions) is carried here.
//
// Typing is MEASURED-THEN-PINNED: int32-listed fields hard-throw on any
// non-integer executed value; geometry coords are IEEE-754 double bits.

const {
  f64bits, asF64, asI32, loadExtractor,
} = require("./tables-schema");

// stageId order == oracle harness --stage ids == upstream
// src/stages/activeStage.js stageMapping (0..5).
const STAGE_NAMES = ["battlefield", "ystory", "pstadium", "dreamland",
  "fdest", "fountain"];

// Surface kinds, pinned order (struct + dump + coverage order).
const SURF_KINDS = ["ground", "platform", "ceiling", "wallL", "wallR"];

// Ledge surface types (upstream type Ledge = [ "ground"|"platform", i, side ]).
const LEDGE_TYPES = { ground: 0, platform: 1 };

// Connected-surface label enum (full set per upstream
// src/stages/stage.js getSurfaceFromStage; only "g" occurs at the pin).
const CONN_TYPES = { g: 0, p: 1, c: 2, l: 3, r: 4 };
// Which surface list a connected label indexes into (bounds checking).
const CONN_LIST = { g: "ground", p: "platform", c: "ceiling", l: "wallL", r: "wallR" };

// Exact key set per stage object (measured at the pin; hard-throw on
// drift). box/target/background/polygonMap are target-stage machinery:
// box is pinned-empty for VS stages, the others pinned-absent.
const STAGE_KEYS_REQUIRED = ["name", "box", "polygon", "platform", "ground",
  "ceiling", "wallL", "wallR", "startingPoint", "startingFace",
  "respawnPoints", "respawnFace", "blastzone", "ledge", "ledgePos",
  "scale", "offset", "movingPlats", "movingPlatforms"];
const STAGE_KEYS_OPTIONAL = ["connected"];

const PLAYERS = 4; // engine indexes startingPoint/respawnPoints [0..3]

function loadStages(distRoot) {
  const { win, srcSha256 } = loadExtractor(distRoot);
  const stages = win.__stages;
  if (!stages) throw new Error("extractor bundle did not assign window.__stages");
  return { stages, srcSha256 };
}

function asVec2(where, p) {
  if (!p || typeof p.x !== "number" || typeof p.y !== "number") {
    throw new Error(`${where}: not a Vec2D`);
  }
  return { x: asF64(where + ".x", p.x), y: asF64(where + ".y", p.y) };
}

// Surface = [Vec2D, Vec2D] at the VS pin; the upstream type allows an
// optional third SurfaceProperties element (damageType) — none exists on
// any VS stage, and silently dropping one would be a faithfulness bug,
// so its appearance hard-throws (format bump territory).
function asSurface(where, s) {
  if (!Array.isArray(s) || s.length !== 2) {
    throw new Error(`${where}: expected [Vec2D, Vec2D] (got length ` +
      `${Array.isArray(s) ? s.length : typeof s}; a SurfaceProperties ` +
      `third element is a format change, never droppable)`);
  }
  return [asVec2(where + "[0]", s[0]), asVec2(where + "[1]", s[1])];
}

function asFace(where, v) {
  const n = asI32(where, v);
  if (n !== 1 && n !== -1) throw new Error(`${where}: face must be 1|-1, got ${n}`);
  return n;
}

// Walk + validate the live vs-stages registry into the canonical STAB1
// model. Hard-throws on any surprise.
function buildStageModel(stagesObj) {
  const gotNames = Object.keys(stagesObj).sort();
  const wantNames = [...STAGE_NAMES].sort();
  if (gotNames.join(",") !== wantNames.join(",")) {
    throw new Error(`stage key set drifted:\n got ${gotNames}\nwant ${wantNames}`);
  }

  const stages = [];
  for (let stageId = 0; stageId < STAGE_NAMES.length; stageId++) {
    const sn = STAGE_NAMES[stageId];
    const st = stagesObj[sn];
    const where = (s) => `${sn}/${s}`;

    // -- exact key set --
    const keys = Object.keys(st);
    for (const k of keys) {
      if (!STAGE_KEYS_REQUIRED.includes(k) && !STAGE_KEYS_OPTIONAL.includes(k)) {
        throw new Error(`${sn}: unexpected stage key ${k}`);
      }
    }
    for (const k of STAGE_KEYS_REQUIRED) {
      if (!keys.includes(k)) throw new Error(`${sn}: missing stage key ${k}`);
    }
    if (st.name !== sn) throw new Error(`${sn}: name field is ${st.name}`);
    if (!Array.isArray(st.box) || st.box.length !== 0) {
      throw new Error(`${sn}: box expected pinned-empty for VS stages`);
    }
    if (typeof st.movingPlatforms !== "function") {
      throw new Error(`${sn}: movingPlatforms is not a function`);
    }

    const model = { stageId, name: sn };

    // -- polygon (render+collision one-source vector rings) --
    if (!Array.isArray(st.polygon)) throw new Error(`${sn}: polygon not an array`);
    model.polygon = st.polygon.map((ring, r) => {
      if (!Array.isArray(ring) || ring.length < 3) {
        throw new Error(`${sn}: polygon[${r}] is not a ring`);
      }
      return ring.map((p, v) => asVec2(where(`polygon[${r}][${v}]`), p));
    });

    // -- surfaces --
    for (const kind of SURF_KINDS) {
      if (!Array.isArray(st[kind])) throw new Error(`${sn}: ${kind} not an array`);
      model[kind] = st[kind].map((s, i) => asSurface(where(`${kind}[${i}]`), s));
    }

    // -- ledges (parallel arrays ledge[i] / ledgePos[i]) --
    if (!Array.isArray(st.ledge) || !Array.isArray(st.ledgePos) ||
        st.ledge.length !== st.ledgePos.length) {
      throw new Error(`${sn}: ledge/ledgePos must be parallel arrays`);
    }
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
    model.ledgePos = st.ledgePos.map((p, i) => asVec2(where(`ledgePos[${i}]`), p));

    // -- spawn/respawn (pinned 4 = engine player slots) --
    for (const [pts, faces] of [["startingPoint", "startingFace"],
      ["respawnPoints", "respawnFace"]]) {
      if (!Array.isArray(st[pts]) || st[pts].length !== PLAYERS) {
        throw new Error(`${sn}: ${pts} must have ${PLAYERS} entries`);
      }
      if (!Array.isArray(st[faces]) || st[faces].length !== PLAYERS) {
        throw new Error(`${sn}: ${faces} must have ${PLAYERS} entries`);
      }
      model[pts] = st[pts].map((p, i) => asVec2(where(`${pts}[${i}]`), p));
      model[faces] = st[faces].map((f, i) => asFace(where(`${faces}[${i}]`), f));
    }

    // -- blastzone (Box2D min/max) --
    const bz = st.blastzone;
    if (!bz || !bz.min || !bz.max) throw new Error(`${sn}: blastzone not a Box2D`);
    model.blastzone = {
      min: asVec2(where("blastzone.min"), bz.min),
      max: asVec2(where("blastzone.max"), bz.max),
    };

    // -- scale (f64) + offset (int32[2] screen pixels) --
    model.scale = asF64(where("scale"), st.scale);
    if (!Array.isArray(st.offset) || st.offset.length !== 2) {
      throw new Error(`${sn}: offset is not [x,y]`);
    }
    model.offset = st.offset.map((v, i) => asI32(where(`offset[${i}]`), v));

    // -- connected (optional; physics indexes connected[0] by ground
    //    index and connected[1] by platform index — lengths pinned) --
    if (st.connected !== undefined) {
      const c = st.connected;
      if (!Array.isArray(c) || c.length !== 2) {
        throw new Error(`${sn}: connected is not [grounds, platforms]`);
      }
      const conn = [];
      [["ground", 0], ["platform", 1]].forEach(([listName, k]) => {
        if (!Array.isArray(c[k]) || c[k].length !== st[listName].length) {
          throw new Error(`${sn}: connected[${k}] length != ${listName} count`);
        }
        conn.push(c[k].map((pair, i) => {
          if (!Array.isArray(pair) || pair.length !== 2) {
            throw new Error(`${sn}: connected[${k}][${i}] is not [left,right]`);
          }
          return pair.map((lab, s) => {
            if (lab === null) return null;
            if (!Array.isArray(lab) || lab.length !== 2 || !(lab[0] in CONN_TYPES)) {
              throw new Error(`${sn}: connected[${k}][${i}][${s}] bad label`);
            }
            const idx = asI32(where(`connected[${k}][${i}][${s}].index`), lab[1]);
            if (idx < 0 || idx >= st[CONN_LIST[lab[0]]].length) {
              throw new Error(`${sn}: connected[${k}][${i}][${s}] index out of range`);
            }
            return { type: CONN_TYPES[lab[0]], index: idx };
          });
        }));
      });
      model.connected = conn;
    } else {
      model.connected = null;
    }

    // -- movingPlats (platform indices whose y/x is sim-animated) --
    if (!Array.isArray(st.movingPlats)) throw new Error(`${sn}: movingPlats not an array`);
    model.movingPlats = st.movingPlats.map((v, i) => {
      const idx = asI32(where(`movingPlats[${i}]`), v);
      if (idx < 0 || idx >= st.platform.length) {
        throw new Error(`${sn}: movingPlats[${i}] out of platform range`);
      }
      return idx;
    });

    stages.push(model);
  }
  return { formatVersion: 1, stages };
}

// Canonical leaf dump (FORMATS.md §4.5): one line per leaf value, the
// EXACT byte stream pipeline/lib/stages_check.c prints from the compiled
// C tables. Doubles as 16 lowercase hex digits of their IEEE-754 bit
// pattern; ints as decimal.
function dumpStages(model, emit) {
  const v2 = (p) => `${p.x.bits},${p.y.bits}`;
  for (const st of model.stages) {
    const sn = st.name;
    st.polygon.forEach((ring, r) =>
      ring.forEach((p, v) => emit(`stage/${sn}/polygon/${r}/${v}=${v2(p)}`)));
    for (const kind of SURF_KINDS) {
      st[kind].forEach((s, i) =>
        emit(`stage/${sn}/${kind}[${i}]=${v2(s[0])},${v2(s[1])}`));
    }
    st.ledge.forEach((l, i) =>
      emit(`stage/${sn}/ledge[${i}]=${l.type},${l.index},${l.side}`));
    st.ledgePos.forEach((p, i) => emit(`stage/${sn}/ledgePos[${i}]=${v2(p)}`));
    st.startingPoint.forEach((p, i) =>
      emit(`stage/${sn}/startingPoint[${i}]=${v2(p)}`));
    st.startingFace.forEach((f, i) => emit(`stage/${sn}/startingFace[${i}]=${f}`));
    st.respawnPoints.forEach((p, i) =>
      emit(`stage/${sn}/respawnPoints[${i}]=${v2(p)}`));
    st.respawnFace.forEach((f, i) => emit(`stage/${sn}/respawnFace[${i}]=${f}`));
    emit(`stage/${sn}/blastzone=${v2(st.blastzone.min)},${v2(st.blastzone.max)}`);
    emit(`stage/${sn}/scale=${st.scale.bits}`);
    emit(`stage/${sn}/offset=${st.offset[0]},${st.offset[1]}`);
    emit(`stage/${sn}/hasConnected=${st.connected ? 1 : 0}`);
    if (st.connected) {
      ["g", "p"].forEach((tag, k) => {
        st.connected[k].forEach((pair, i) => {
          pair.forEach((lab, s) => {
            emit(`stage/${sn}/connected/${tag}[${i}][${s}]=` +
              (lab === null ? "-" : `${lab.type},${lab.index}`));
          });
        });
      });
    }
    st.movingPlats.forEach((p, i) => emit(`stage/${sn}/movingPlats[${i}]=${p}`));
  }
}

module.exports = {
  STAGE_NAMES, SURF_KINDS, LEDGE_TYPES, CONN_TYPES, PLAYERS,
  loadStages, buildStageModel, dumpStages, f64bits,
};
