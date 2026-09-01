#!/usr/bin/env node
// port/gfx/opk/make-data-pack.js — assemble the SD-card data pack a release
// ships, from a FULL pipeline run.
//
// WHY THIS EXISTS. The first release's pack was PULLED OFF THE AUTHOR'S DEVICE.
// It worked, because it was the artifact that had been proven to work — but it
// was not reproducible from source, so nobody could rebuild or audit it. This
// script is the reproducible path: everything it emits comes from
// `node pipeline/run.js`, the tracked frozen render data, and the sound packer.
//
// THE ONE THING THAT IS NOT MECHANICAL, and the reason this is a script rather
// than a `cp`: the music manifest's TRACK TOKENS are the PORT's names
// (foh_dev.c kMusTok), and the sounds.json keys are UPSTREAM's Howl names.
// Four of the eight differ:
//
//     port token   upstream key   .pcm on the card
//     ystory       yStory         yStory.pcm
//     pstadium     pStadium       pStadium.pcm
//     fdest        finald         finald.pcm
//     fountain     fod            fod.pcm
//
// MEASURED, and it is not a nicety: foh_dev.c:969 resolves a track by matching
// the token against kMusTok, and a row it cannot match makes the app exit(2)
// with "music manifest has no '<tok>' row but the run needs it". A generator
// that used the upstream keys verbatim produces a pack that dies on four of
// the six stages. The first attempt at this did exactly that, and it was
// caught by byte-comparing the result against a device known to work — which
// is why the check below exists rather than trusting this table twice.
"use strict";
const fs = require("fs");
const path = require("path");

// port token -> upstream sounds.json key. Order is kMusTok's, so the emitted
// file reads in the same order the port indexes them, even though the loader
// matches by name and does not care.
const TRACKS = [
  ["menu", "menu"],
  ["battlefield", "battlefield"],
  ["ystory", "yStory"],
  ["pstadium", "pStadium"],
  ["dreamland", "dreamland"],
  ["fdest", "finald"],
  ["fountain", "fod"],
  ["targettest", "targettest"],
];

function die(m) { console.error("make-data-pack: " + m); process.exit(1); }

const src = process.argv[2];
const dst = process.argv[3];
if (!src || !dst) die("usage: make-data-pack.js <pipeline-out-dir> <pack-dir>");

const sounds = JSON.parse(fs.readFileSync(path.join(src, "sounds.json"), "utf8"));
const keys = Object.keys(sounds.music).sort();
const want = TRACKS.map((t) => t[1]).sort();
if (keys.join(",") !== want.join(",")) {
  die("sounds.json music keys " + keys.join(",") + " != the mapped set " +
      want.join(",") + " — upstream's track list moved and the TRACKS table " +
      "above has to move with it");
}

fs.mkdirSync(dst, { recursive: true });
const lines = [];
for (const [tok, key] of TRACKS) {
  const t = sounds.music[key];
  const base = path.basename(t.blob);
  fs.copyFileSync(path.join(src, t.blob), path.join(dst, base));
  lines.push(["track", tok, "/mnt/mlfk-data/" + base, t.volume.bits,
              t.sprite.start[0], t.sprite.start[1],
              t.sprite.loop[0], t.sprite.loop[1]].join(" "));
}
fs.writeFileSync(path.join(dst, "foh-music.txt"), lines.join("\n") + "\n");

for (const f of fs.readdirSync(src)) {
  if (/^anim_[0-9]+_[a-z]+\.bin$/.test(f)) {
    fs.copyFileSync(path.join(src, f), path.join(dst, f));
  }
}
fs.cpSync(path.join(src, "assets"), path.join(dst, "assets"), { recursive: true });

console.log("make-data-pack OK tracks=" + TRACKS.length +
            " (sndpack.bin and simdata.txt are produced by their own tools; " +
            "see the release recipe)");
