"use strict";
// Stage "animations" — executed-JS serialization of the data plane's
// animation half (PLAN §4 M1; format: pipeline/FORMATS.md §2, ANIM1).
//
// EXECUTED-JS principle: this stage require()s the BUILT upstream bundle
// dist/js/animations.js (webpack output of src/animations.js, which
// assigns window.animations = [marth, puff, fox, falco, falcon]) under a
// window shim and walks the live objects. The bundle is pure Int16Array
// data construction — no DOM, no Math, no engine-dependent semantics —
// so plain node execution is byte-equivalent to the browser (PROVISIONAL
// call, logged AGENT-LOG iter 9). Nothing is ever hand-transcribed.

const fs = require("fs");
const path = require("path");
const { sha256 } = require("../lib/manifest");
const { encodeAnim, decodeAnim } = require("../lib/animbin");

const CHAR_NAMES = ["marth", "puff", "fox", "falco", "falcon"]; // upstream order

function loadAnimations(distRoot) {
  const srcPath = path.join(distRoot, "dist", "js", "animations.js");
  if (!fs.existsSync(srcPath)) {
    throw new Error(`missing ${srcPath} — run oracle/build-upstream.sh first`);
  }
  const hadWindow = "window" in global;
  const saved = global.window;
  global.window = {};
  try {
    delete require.cache[require.resolve(srcPath)];
    require(srcPath);
    const animations = global.window.animations;
    if (!Array.isArray(animations) || animations.length !== CHAR_NAMES.length) {
      throw new Error("window.animations missing or not 5 characters");
    }
    return { animations, srcPath, srcSha256: sha256(fs.readFileSync(srcPath)) };
  } finally {
    if (hadWindow) global.window = saved; else delete global.window;
  }
}

// Deep-compare executed source data against a decoded ANIM1 buffer.
function roundTripCheck(charName, statesMap, decoded) {
  const decNames = [...decoded.states.keys()].sort();
  const srcNames = [...statesMap.keys()].sort();
  if (decNames.length !== srcNames.length ||
      decNames.some((n, i) => n !== srcNames[i])) {
    throw new Error(`${charName}: state-name sets differ after decode`);
  }
  for (const name of srcNames) {
    const src = statesMap.get(name);
    const dec = decoded.states.get(name);
    if (src.length !== dec.length) {
      throw new Error(`${charName}/${name}: frameCount ${src.length} != ${dec.length}`);
    }
    for (let f = 0; f < src.length; f++) {
      const sf = src[f] === undefined ? null : src[f];
      const df = dec[f];
      if ((sf === null) !== (df === null)) {
        throw new Error(`${charName}/${name}[${f}]: absent-frame mismatch`);
      }
      if (sf === null) continue;
      if (sf.length !== df.length) {
        throw new Error(`${charName}/${name}[${f}]: pathCount mismatch`);
      }
      for (let j = 0; j < sf.length; j++) {
        const sp = sf[j], dp = df[j];
        if (sp.length !== dp.length) {
          throw new Error(`${charName}/${name}[${f}][${j}]: coordCount mismatch`);
        }
        for (let k = 0; k < sp.length; k++) {
          if (sp[k] !== dp[k]) {
            throw new Error(
              `${charName}/${name}[${f}][${j}][${k}]: ${sp[k]} != ${dp[k]}`);
          }
        }
      }
    }
  }
}

function run(ctx) {
  const { animations, srcSha256 } = loadAnimations(ctx.distRoot);

  const artifacts = [];
  const perChar = {};
  const cov = {
    chars: 0, states: 0, frames: 0, absentFrames: 0,
    paths: 0, coords: 0, irregularPaths: 0, nonInt16Paths: 0,
    maxCoordCount: 0, bytes: 0,
  };

  for (let charId = 0; charId < CHAR_NAMES.length; charId++) {
    const charName = CHAR_NAMES[charId];
    const charObj = animations[charId];
    if (!charObj || typeof charObj !== "object") {
      throw new Error(`animations[${charId}] (${charName}) is not an object`);
    }
    const statesMap = new Map();
    const c = { states: 0, frames: 0, absentFrames: 0, paths: 0, coords: 0 };
    for (const stateName of Object.keys(charObj).sort()) {
      const frames = charObj[stateName];
      if (!Array.isArray(frames)) {
        throw new Error(`${charName}/${stateName}: frames is ${typeof frames}, not array`);
      }
      // Normalize: sparse/undefined frame -> null; validate paths.
      const normFrames = [];
      for (let f = 0; f < frames.length; f++) {
        const frame = frames[f];
        if (frame === null || frame === undefined) {
          normFrames.push(null);
          c.absentFrames++; cov.absentFrames++;
          continue;
        }
        if (!Array.isArray(frame)) {
          throw new Error(`${charName}/${stateName}[${f}]: frame is not an array`);
        }
        for (const p of frame) {
          const isI16 = p instanceof Int16Array;
          if (!isI16) {
            cov.nonInt16Paths++;
            if (!Array.isArray(p)) {
              throw new Error(`${charName}/${stateName}[${f}]: path is neither Int16Array nor Array`);
            }
          }
          if (p.length < 2 || (p.length - 2) % 6 !== 0) cov.irregularPaths++;
          if (p.length > cov.maxCoordCount) cov.maxCoordCount = p.length;
          c.coords += p.length; cov.coords += p.length;
        }
        c.paths += frame.length; cov.paths += frame.length;
        normFrames.push(frame);
      }
      statesMap.set(stateName, normFrames);
      c.states++; cov.states++;
      c.frames += frames.length; cov.frames += frames.length;
    }

    const bin = encodeAnim(charId, statesMap);
    roundTripCheck(charName, statesMap, decodeAnim(bin)); // hard gate, in-run
    const fileName = `anim_${charId}_${charName}.bin`;
    fs.writeFileSync(path.join(ctx.outDir, fileName), bin);
    cov.chars++;
    cov.bytes += bin.length;
    perChar[charName] = { ...c, bytes: bin.length, stateNames: [...statesMap.keys()].sort() };
    artifacts.push({ path: fileName, sha256: sha256(bin), bytes: bin.length, charId });
    ctx.log(`  ${fileName}: ${c.states} states, ${c.frames} frames, ` +
      `${c.paths} paths, ${c.coords} coords, ${bin.length} bytes`);
  }

  artifacts.sort((a, b) => (a.path < b.path ? -1 : 1));
  return {
    format: "ANIM1",
    sources: [{ path: "dist/js/animations.js", sha256: srcSha256 }],
    coverage: cov,
    perChar,
    artifacts,
  };
}

module.exports = { name: "animations", run };
