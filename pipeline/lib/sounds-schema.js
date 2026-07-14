"use strict";
// Pinned schema + executed-JS walk for the sound map (fix_plan §M1 task 4;
// format SND1, pipeline/FORMATS.md §5).
//
// SINGLE SOURCE OF TRUTH on the JS side for the sound-map shape. Typing is
// MEASURED-THEN-PINNED (AGENT-LOG iter 12): every surprise hard-throws —
// silent coercion/dropping is the defect class this file prevents.
//
// Volume semantics (measured; FORMATS.md §5.3): each Howl's authored cfg
// volume is DEAD after module load — sfx.js's own load-time
// `changeVolume(sounds, 0.5, 0)` / `changeVolume(MusicManager, 0.3, 1)`
// pass overwrites every instance's `_volume` with
// masterDefault * (volumeOverwrites[name] || 1). We record BOTH: `volume`
// = the post-load effective value (what Howler would actually play at
// default settings; the C mixer's per-sound gain source) and `cfgVolume`
// = the authored constructor value (provenance). Runtime master-volume
// changes (audiomenu/main.js changeVolume calls) are M4 logic, not data.

const { f64bits, asI32, loadExtractor } = require("./tables-schema");

const CHAR_NAMES = ["marth", "puff", "fox", "falco", "falcon"]; // charId order

// The 8 MusicManager track Howls (bytewise-sorted; measured iter 12).
const TRACK_NAMES = ["battlefield", "dreamland", "finald", "fod", "menu",
  "pStadium", "targettest", "yStory"];

const SFX_CFG_KEYS = new Set(["src", "volume", "loop"]);
const MUSIC_CFG_KEYS = new Set(["src", "volume", "html5", "sprite", "onend"]);

function checkName(kind, name) {
  if (!/^[A-Za-z0-9_]+$/.test(name)) {
    throw new Error(`${kind} name ${JSON.stringify(name)} is not [A-Za-z0-9_]+`);
  }
}

function asVol(where, v) {
  if (typeof v !== "number" || !Number.isFinite(v) || v < 0 || v > 1) {
    throw new Error(`${where}: expected volume in [0,1], got ${v}`);
  }
  return { bits: f64bits(v), dec: String(v) };
}

function howlCfg(where, inst) {
  if (!inst || typeof inst !== "object" || !inst.__howlCfg) {
    throw new Error(`${where}: not a captured Howl instance`);
  }
  return inst.__howlCfg;
}

function srcFile(where, cfg, dirRe) {
  if (!Array.isArray(cfg.src) || cfg.src.length !== 1 ||
      typeof cfg.src[0] !== "string" || !dirRe.test(cfg.src[0])) {
    throw new Error(`${where}: src ${JSON.stringify(cfg.src)} !~ ${dirRe}`);
  }
  return cfg.src[0];
}

function loadSounds(distRoot) {
  const { win, srcSha256 } = loadExtractor(distRoot);
  if (!win.__sounds) throw new Error("extractor bundle did not assign window.__sounds");
  if (!win.__tables) throw new Error("extractor bundle did not assign window.__tables");
  return { sounds: win.__sounds, tables: win.__tables, srcSha256 };
}

// Walk + validate the captured Howls and the actionSounds registry into
// the canonical SND1 model. `blob` paths point at the converted PCM
// artifacts the audio stage emits alongside this model.
function buildSoundModel(sounds, tables) {
  // -- sfx: the 180 named Howls of src/main/sfx.js --
  const sfx = {};
  for (const name of Object.keys(sounds.sfx).sort()) {
    checkName("sfx sound", name);
    const where = `sfx.${name}`;
    const inst = sounds.sfx[name];
    const cfg = howlCfg(where, inst);
    for (const k of Object.keys(cfg)) {
      if (!SFX_CFG_KEYS.has(k)) throw new Error(`${where}: unexpected Howl cfg key "${k}"`);
    }
    const file = srcFile(where, cfg, /^sfx\/[A-Za-z0-9-]+\.wav$/);
    if (cfg.loop !== undefined && cfg.loop !== true) {
      throw new Error(`${where}: loop must be true or absent, got ${cfg.loop}`);
    }
    sfx[name] = {
      file,
      blob: "audio/" + file.replace(/\.wav$/, ".pcm"),
      cfgVolume: asVol(where + ".cfgVolume", cfg.volume === undefined ? 1 : cfg.volume),
      loop: cfg.loop === true ? 1 : 0,
      volume: asVol(where + "._volume", inst._volume),
    };
  }

  // -- music: the 8 MusicManager static track Howls --
  const gotTracks = Object.keys(sounds.music)
    .filter((k) => sounds.music[k] && sounds.music[k].__howlCfg).sort();
  if (gotTracks.join(",") !== TRACK_NAMES.join(",")) {
    throw new Error(`music track set drifted:\n got ${gotTracks}\nwant ${TRACK_NAMES}`);
  }
  const music = {};
  for (const name of TRACK_NAMES) {
    const where = `music.${name}`;
    const inst = sounds.music[name];
    const cfg = howlCfg(where, inst);
    for (const k of Object.keys(cfg)) {
      if (!MUSIC_CFG_KEYS.has(k)) throw new Error(`${where}: unexpected Howl cfg key "${k}"`);
    }
    const file = srcFile(where, cfg, /^music\/[A-Za-z]+\.ogg$/);
    if (cfg.html5 !== true) throw new Error(`${where}: html5 !== true (streaming hint drifted)`);
    if (typeof cfg.onend !== "function") throw new Error(`${where}: onend loop handler missing`);
    // Sprite windows [offsetMs, durationMs]: exactly <name>Start + <name>Loop
    // (the onend handlers chain Start -> Loop; that logic is M4, the windows
    // are the data).
    const spriteKeys = Object.keys(cfg.sprite || {}).sort();
    const want = [`${name}Loop`, `${name}Start`].sort();
    if (spriteKeys.join(",") !== want.join(",")) {
      throw new Error(`${where}: sprite keys ${spriteKeys} != ${want}`);
    }
    const win2 = (k) => {
      const v = cfg.sprite[k];
      if (!Array.isArray(v) || v.length !== 2) throw new Error(`${where}.sprite.${k}: not [offset,duration]`);
      return v.map((n, i) => {
        const iv = asI32(`${where}.sprite.${k}[${i}]`, n);
        if (iv < 0) throw new Error(`${where}.sprite.${k}[${i}]: negative`);
        return iv;
      });
    };
    music[name] = {
      file,
      blob: "audio/" + file.replace(/\.ogg$/, ".pcm"),
      cfgVolume: asVol(where + ".cfgVolume", cfg.volume),
      volume: asVol(where + "._volume", inst._volume),
      sprite: { start: win2(`${name}Start`), loop: win2(`${name}Loop`) },
    };
  }

  // -- actionSounds: per-char state -> [[frame, sfxName], ...] schedule
  // (registered by the attributes modules; played by actionStateShortcuts
  // at player[p].timer == frame — that caller is M2/M4 logic, this is the
  // data). Event-list order kept verbatim; names must resolve into sfx. --
  const as = tables.actionSounds;
  if (!Array.isArray(as) || as.length !== CHAR_NAMES.length) {
    throw new Error(`actionSounds: expected ${CHAR_NAMES.length} chars, got ${as && as.length}`);
  }
  const actionSounds = {};
  for (let charId = 0; charId < CHAR_NAMES.length; charId++) {
    const cn = CHAR_NAMES[charId];
    const perState = {};
    for (const state of Object.keys(as[charId]).sort()) {
      checkName("actionSounds state", state);
      const list = as[charId][state];
      if (!Array.isArray(list)) throw new Error(`${cn}/${state}: not an array`);
      perState[state] = list.map((ev, i) => {
        const where = `${cn}/${state}[${i}]`;
        if (!Array.isArray(ev) || ev.length !== 2) throw new Error(`${where}: not [frame,name]`);
        const frame = asI32(where + ".frame", ev[0]);
        if (frame < 0) throw new Error(`${where}: negative frame`);
        if (typeof ev[1] !== "string" || !sfx[ev[1]]) {
          throw new Error(`${where}: sound ${JSON.stringify(ev[1])} not in the sfx map`);
        }
        return [frame, ev[1]];
      });
    }
    actionSounds[cn] = perState;
  }

  return { formatVersion: 1, sfx, music, actionSounds };
}

module.exports = { CHAR_NAMES, TRACK_NAMES, loadSounds, buildSoundModel };
