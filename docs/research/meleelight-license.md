# meleelight license research (wayfinder ticket #3)

Investigated 2026-07-13 against primary sources of
[schmooblidon/meleelight](https://github.com/schmooblidon/meleelight)
(default branch `master`, last pushed 2023-08-14).

## Verdict (short form)

**The code is MIT-licensed.** The repo's `LICENSE` file is the verbatim MIT
License text, copyright "(c) 2016 Will Blackett" (schmooblidon), with two
extra lines appended disclaiming the Nintendo characters. Those appended
lines are exactly why GitHub's licensee reports `Other / NOASSERTION` — the
file no longer byte-matches the stock MIT template, so automated detection
refuses to assert. The deviation does not change the legal substance of the
code grant; `package.json` independently declares `"license": "MIT"`.

**The bundled game assets are not MIT and cannot be** — the repo ships
ripped/recreated Melee audio and character art that is Nintendo/HAL
Laboratory IP, which the LICENSE rider acknowledges.

## 1. The LICENSE file (primary source, full relevant text)

`LICENSE` at repo root, added in commit `ad174686` ("LICENSE",
2016-10-04 — the day the repo was created, so the license has been in place
for the project's entire public life). Full text is the standard MIT
license, beginning:

> MIT License
>
> Copyright (c) 2016 Will Blackett
>
> Permission is hereby granted, free of charge, to any person obtaining a
> copy of this software and associated documentation files (the
> "Software"), to deal in the Software without restriction, including
> without limitation the rights to use, copy, modify, merge, publish,
> distribute, sublicense, and/or sell copies of the Software...

...continuing through the standard warranty disclaimer, then appending:

> Characters (c) Nintendo / HAL Laboratory
>
> All characters and concepts belong to their respective owners

Link: <https://github.com/schmooblidon/meleelight/blob/master/LICENSE>

### Why GitHub says NOASSERTION

GitHub's license detection (licensee) requires a near-exact match to a known
license template. The two appended Nintendo lines break the match, so the
API reports `{"key": "other", "spdx_id": "NOASSERTION"}`. This is a
detection artifact, not an indication of a custom or restrictive license.

## 2. Corroborating sources

- **`package.json`**: `"license": "MIT"`, `"author": "schmooblidon"`.
  <https://github.com/schmooblidon/meleelight/blob/master/package.json>
- **README.md**: no license statement at all; links the playable build at
  `http://ikneedata.com/meleelight` (still live as of 2026-07-13; page
  carries no copyright/license notice of its own).
- **contributing.md**: "Melee light is a fan project entirely built from
  the ground up in web tech. It was recently open sourced..." — i.e. the
  author asserts the engine code is original work, not decompiled/copied
  Nintendo code.
- **Code file headers**: spot-checked `src/index.js`, `src/main.js`,
  `src/main/main.js` — no per-file license or copyright headers anywhere;
  the root LICENSE is the sole grant.
- **Issues/PRs**: GitHub search for "license", "copyright", "nintendo",
  "takedown" in the repo's issues/PRs turns up nothing where the author
  discusses licensing intent (only PR #74, an unrelated controller PR).
  No DMCA/takedown history visible.
- **meleelight.com**: does not resolve (dead). The live deployment is
  `ikneedata.com/meleelight`.

## 3. Third-party code and data bundled in the repo

### JS dependencies (runtime)

Only two runtime deps, both permissive, both pulled from npm (not vendored
as copied source):

- `howler` ^2.0.1 — MIT
- `jquery` 1.11.3 — MIT

Dev dependencies (webpack/babel/electron/deepstream etc.) are build-time
only and not shipped. No vendored engine code, no `vendor/` or `lib/`
directory. CSS references only system fonts (Consolas, Ubuntu Mono
fallback stack); **no font files are bundled** (no ttf/otf/woff in the
tree).

### Game assets — the real third-party problem

The `dist/` tree bundles assets that are plainly Nintendo/HAL-derived and
are NOT covered by the MIT grant (the LICENSE rider exists precisely to
carve these out):

- **204 `.wav` sound effects** in `dist/sfx/` — many are unmistakably
  ripped Melee audio: announcer lines (`choose-your-character.wav`,
  `break-the-targets.wav`), character voice clips
  (`falconpunchshout1.wav`, `falcofirebird.wav`, `dolphin-slash.wav`,
  etc.), hit/clank/shine effects.
- **8 `.ogg` music tracks** in `dist/music/` named for Melee stages
  (`battlefield.ogg`, `dreamland.ogg`, `fod.ogg`, `pStadium.ogg`,
  `yStory.ogg`, `finald.ogg`, `menu.ogg`, `targettest.ogg`).
- **Character select portraits** in `dist/assets/css/` (`fox.png`,
  `falco.png`, `falcon.png`, `marth.png`, `puff.png`) and stage icons —
  Melee character/stage art or close recreations.
- In-game character animations are code-drawn (vector paths compiled by
  the `animations` build step), i.e. recreations rather than ripped
  sprites — but they recreate Nintendo character designs.

## 4. The Nintendo-IP dimension (separate from code license)

Two distinct layers, only one of which schmooblidon could license:

1. **The engine/code**: original work by the meleelight authors, validly
   MIT-licensed. Physics constants and frame data replicating Melee's
   mechanics are, under prevailing doctrine, uncopyrightable game
   mechanics/facts.
2. **The expressive Nintendo/HAL content**: character designs (Fox, Falco,
   Marth, Jigglypuff, Captain Falcon), stage designs, music, voice/SFX
   rips, names, and trade dress. schmooblidon has no rights to grant here,
   said so in the LICENSE, and MIT terms simply do not attach to this
   material. Nintendo's enforcement posture toward fan games is famously
   aggressive (AM2R, Pokémon Uranium, etc.) — relevant only to
   distribution, not possession.

## 5. What this means for our exact case (private, personal-use FunKey-S port)

- **Code**: MIT allows use, copying, and modification without restriction.
  A private derivative is unambiguously fine. Our only obligation —
  retaining the copyright + permission notice — technically triggers on
  distribution of copies, but we should carry the upstream LICENSE file in
  our port anyway as a matter of hygiene and provenance.
- **Assets**: private personal use of the bundled Nintendo-derived audio
  and art carries no practical exposure; there is no distribution, no
  public performance, and no plausible enforcement vector against a
  personal handheld build. Copyright infringement risk is a
  distribution-shaped problem we do not have.
- **Net**: nothing in the licensing picture constrains the private port.
  We derive from: MIT-licensed engine code (c) 2016 Will Blackett +
  Nintendo/HAL-owned assets used privately.

## 6. If publication were ever reconsidered (out of scope — noted only)

- The **code** could be republished/forked freely under MIT with notice
  retained.
- The **assets** (sfx, music, portraits, and arguably the recreated
  character animations/likenesses) could not be lawfully distributed;
  a publishable build would need all Nintendo-derived audio/art stripped
  or replaced, a renamed project (the "Melee" mark), and original
  character designs — at which point it is a generic platform fighter
  engine. Nintendo's DMCA history with fan projects makes this a
  when-not-if enforcement scenario for anything recognizably Melee.
- Note meleelight itself remains up after ~9 years, but survivorship of
  one repo is not a safe-harbor argument.

## Source index

- LICENSE: <https://github.com/schmooblidon/meleelight/blob/master/LICENSE>
- LICENSE commit: <https://github.com/schmooblidon/meleelight/commit/ad174686> (2016-10-04)
- package.json: <https://github.com/schmooblidon/meleelight/blob/master/package.json>
- README: <https://github.com/schmooblidon/meleelight/blob/master/README.md>
- contributing.md: <https://github.com/schmooblidon/meleelight/blob/master/contributing.md>
- Live build: <http://ikneedata.com/meleelight> (meleelight.com is dead)
- GitHub API license report: `"spdx_id": "NOASSERTION"` (detection artifact, see §1)
