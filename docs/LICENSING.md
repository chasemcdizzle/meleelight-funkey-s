# Licensing & provenance

Practical map, not legal advice. Full upstream analysis (primary sources,
commit archaeology, the NOASSERTION explanation):
[`docs/research/meleelight-license.md`](./research/meleelight-license.md).
Attributions: [`NOTICES`](../NOTICES) at the repo root.

## Verdict

- **meleelight code → MIT** ((c) 2016 Will Blackett). The upstream
  `LICENSE` is standard MIT plus a two-line Nintendo-IP rider; GitHub's
  `NOASSERTION` is a detection artifact of that rider, nothing more.
  A derivative port is unambiguously fine, private or public. We carry the
  upstream license **verbatim** as [`LICENSE-meleelight`](../LICENSE-meleelight)
  (rider included). That used to be hygiene ahead of an obligation that had
  not triggered; since the source repository became public (owner ruling,
  2026-08-31) it IS the obligation — MIT requires the notice to travel with
  substantial portions, and a derivative work published as source is exactly
  that. Carrying it from the start is why publishing needed no scramble.
- **meleelight's bundled assets → Nintendo/HAL IP, not MIT, cannot be.**
  Ripped/recreated Melee audio (204 wav SFX, 8 ogg tracks), CSS portraits,
  and the character designs the vector animations recreate. **None of it is
  in this repository and none of it is published.** The converted audio is
  gitignored build output, produced on the machine that runs it and never
  committed; publishing the SOURCE changes nothing about the assets, which
  are still personal use on one handheld. Distribution: never (rules below).
- **This repo's own work** (spec, harnesses, spikes, the future C port) is
  a derivative of meleelight where it translates/patches/serializes it —
  MIT obligations carried via `LICENSE-meleelight` + `NOTICES`.

## Rules (mirrored as a CLAUDE.md hard rule)

1. `LICENSE-meleelight` stays verbatim, rider included. Never edit it.
2. `NOTICES` gains an entry BEFORE any third-party code is copied in-tree.
3. **No distribution of anything from this repo** — that is a project goal
   (locked goal #4), and it is also what keeps the Nintendo-derived asset
   question moot. **The SOURCE is public and RELEASES are published**
   (`chasemcdizzle/meleelight-funkey-s`; owner rulings 2026-08-31 and
   2026-09-01) — code, docs, checks, the OPK, and the data pack.

   The previous position here was "no OPKs, no binaries, no converted audio",
   justified as "private personal use: no practical exposure". That
   justification stopped being true when the repo went public, and the
   remaining argument did not survive a check: **MEASURED 2026-09-01, upstream
   publishes 212 audio files itself**, including all 8 music tracks as ogg in
   `dist/music/`. This project was being more conservative about Nintendo's
   material than the project that material came from, while depending on that
   same project as its oracle.

   That does NOT make the audio ours to license — upstream cannot grant rights
   it does not hold, and the rider says so plainly. What it changes is the
   exposure argument, which was the actual basis for the rule. The owner has
   ruled on the residual risk; it is recorded here as a decision, not as a
   legal conclusion.

   Still binding, and unchanged: no fork of upstream, no PR or issue against
   it, no generated audio committed to this repo, and every release carries
   the attribution and `LICENSE-meleelight`.
4. Never push to, or open PRs against, `schmooblidon/meleelight` or any
   other upstream remote. Upstream clones live OUTSIDE the tree (scratch /
   `vendor/`, both untracked).
5. SDL 1.2 is LGPL-2.1: always dynamically linked from the FunKey OS/SDK
   sysroot, never statically linked or vendored. (Moot privately; kept as
   hygiene.) SDL2 (dev/CI) is zlib.
6. Vendored math (fdlibm via V8's `ieee754.cc`) and any lifted ssb64 port
   code retain their notices in-file and in `NOTICES`.

## If publication were ever reconsidered (out of scope, recorded once)

Code: republishable under MIT with notice. Assets: all Nintendo-derived
audio/art (and arguably the recreated character animations) would have to
be stripped/replaced and the project renamed — see
`docs/research/meleelight-license.md` §6. Not our problem: we don't ship.
