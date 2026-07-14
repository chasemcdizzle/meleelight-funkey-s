/* pipeline/extractor/extractor.entry.js — webpack entry for the engine-table
 * extractor bundle (fix_plan §M1 task 2; EXECUTED-JS principle, PLAN §2).
 *
 * Imports ONLY the real upstream data modules (per-character attributes +
 * ECB files, which register themselves into the src/main/characters.js
 * registries) and exposes the LIVE registry objects as window.__tables.
 * Deliberately avoids the characters/<char>/index.js aggregators: those
 * pull in moves/actionStateShortcuts and the main.js god-module circle
 * (anatomy §3); the data surface below has no such tentacles (verified:
 * these files import only main/characters, Vec2D, createHitbox[Object]).
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
