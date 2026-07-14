/* pipeline/extractor/extractor.entry.js — webpack entry for the engine-table
 * + stage-geometry extractor bundle (fix_plan §M1 tasks 2-3; EXECUTED-JS
 * principle, PLAN §2).
 *
 * Imports ONLY the real upstream data modules (per-character attributes +
 * ECB files, which register themselves into the src/main/characters.js
 * registries) and exposes the LIVE registry objects as window.__tables.
 * Deliberately avoids the characters/<char>/index.js aggregators: those
 * pull in moves/actionStateShortcuts and the main.js god-module circle
 * (anatomy §3); the data surface below has no such tentacles (verified:
 * these files import only main/characters, Vec2D, createHitbox[Object]).
 *
 * Task 3 adds the six VS stage modules via upstream's own aggregator
 * stages/vs-stages/vs-stages.js -> window.__stages. ystory/fountain
 * top-level-import main/main, stages/activeStage and
 * physics/environmentalCollision but reference them ONLY inside their
 * movingPlatforms/updatePlatform function bodies (M2 sim-logic territory,
 * never called during extraction); extractor.config.js externals-stub
 * those exact request strings so the god-module never enters the bundle
 * (build-extractor.sh hard-fails on any DOM-access leak).
 *
 * Task 4 adds the sound map via main/sfx (the 180 named Howls incl.
 * upstream's own load-time changeVolume pass) and main/music (the 8
 * MusicManager track Howls with their sprite loop windows) ->
 * window.__sounds. Howl is a browser global in those modules; the
 * pipeline loader (tables-schema.js loadExtractor) provides a capture
 * shim that records each constructor cfg verbatim. sfx.js imports only
 * ./music; music.js imports nothing — no god-module tentacles.
 *
 * Built by pipeline/extractor/build-extractor.sh with the upstream clone's
 * OWN docker node:8 webpack toolchain (babel query mirrors the game
 * build's happypack loader: presets es2015, plugins
 * transform-flow-strip-types + transform-class-properties) so the executed
 * code is transformed exactly like the shipped game bundle.
 */

import "characters/marth/marthAttributes";
import "characters/marth/ecbmarth";
import "characters/puff/puffAttributes";
import "characters/puff/ecbpuff";
import "characters/fox/attributes";
import "characters/fox/ecb";
import "characters/falco/attributes";
import "characters/falco/ecb";
import "characters/falcon/attributes";
import "characters/falcon/ecb";
import {
  CHARIDS,
  charAttributes,
  intangibility,
  framesData,
  actionSounds,
  offsets,
  hitboxes,
  ecb,
} from "main/characters";

import vsstages from "stages/vs-stages/vs-stages";

import { sounds } from "main/sfx";
import { MusicManager } from "main/music";

window.__tables = {
  charIds: CHARIDS,
  charAttributes: charAttributes,
  intangibility: intangibility,
  framesData: framesData,
  actionSounds: actionSounds,
  offsets: offsets,
  hitboxes: hitboxes,
  ecb: ecb,
};

window.__stages = vsstages;

window.__sounds = {
  sfx: sounds,
  music: MusicManager,
};
