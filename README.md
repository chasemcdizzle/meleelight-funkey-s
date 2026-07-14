# meleelight-funkey-s

Faithful port of [meleelight](https://github.com/schmooblidon/meleelight)
(browser JS Melee remake) to the FunKey-S handheld. Private, personal-use
hobby project — no distribution.

**Status: wayfinding.** The plan is being charted as a wayfinder map on this
repo's issue tracker (issue labelled `wayfinder:map`). The destination of the
map: a locked, research-backed spec ready for an ssb64-style autonomous loop
to execute. Until that spec lands, this repo is scaffold + research findings
(`docs/research/`, written on `research/*` branches).

## Locked goals / restrictions

1. **Faithful port** — meleelight's gameplay verbatim; deviations are bugs.
   The browser original is the oracle (Slippi/brawlback-style deterministic
   input-replay + per-frame state checksums).
2. **Input** — B0XX-style digital→analog mapping layer for the FunKey's
   d-pad + buttons. Core techniques (dash/walk, tilt vs smash, short/full
   hop, wavedash, cardinal/diagonal DI) guaranteed; engine untouched.
3. **Performance** — hard 60 fps simulation AND render at 240×240; a
   kill-criterion for candidate porting strategies.
4. **Private, personal use** — publication out of scope.
5. **Solo parity** — meleelight's own solo experience is the playable bar;
   a CPU opponent is a future effort, out of scope here.

Sibling project / prior art: `~/code_projects/ssb64-funkey-s` (toolchain,
platform layer, loop discipline).
