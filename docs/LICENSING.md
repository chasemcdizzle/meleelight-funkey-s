# Licensing & provenance

Practical map, not legal advice. Full upstream analysis (primary sources,
commit archaeology, the NOASSERTION explanation):
[`docs/research/meleelight-license.md`](./research/meleelight-license.md).
Attributions: [`NOTICES`](../NOTICES) at the repo root.

## Verdict

- **meleelight code → MIT** ((c) 2016 Will Blackett). The upstream
  `LICENSE` is standard MIT plus a two-line Nintendo-IP rider; GitHub's
  `NOASSERTION` is a detection artifact of that rider, nothing more.
  A private derivative port is unambiguously fine. We carry the upstream
  license **verbatim** as [`LICENSE-meleelight`](../LICENSE-meleelight)
  (rider included) even though the notice obligation only technically
  triggers on distribution — hygiene and provenance.
- **meleelight's bundled assets → Nintendo/HAL IP, not MIT, cannot be.**
  Ripped/recreated Melee audio (204 wav SFX, 8 ogg tracks), CSS portraits,
  and the character designs the vector animations recreate. Private
  personal use on one handheld: no practical exposure. Distribution:
  never (see rules below).
- **This repo's own work** (spec, harnesses, spikes, the future C port) is
  a derivative of meleelight where it translates/patches/serializes it —
  MIT obligations carried via `LICENSE-meleelight` + `NOTICES`.

## Rules (mirrored as a CLAUDE.md hard rule)

1. `LICENSE-meleelight` stays verbatim, rider included. Never edit it.
2. `NOTICES` gains an entry BEFORE any third-party code is copied in-tree.
3. **No distribution of anything from this repo** — that is a project goal
   (locked goal #4), and it is also what keeps the Nintendo-derived asset
   question moot. No public forks, no published OPKs, no uploaded videos of
   asset rips. The GitHub remote (`chasemcdizzle/meleelight-funkey-s`) is
   the private working copy.
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
