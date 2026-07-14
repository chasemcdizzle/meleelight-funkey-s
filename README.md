# meleelight-funkey-s

Faithful port of [meleelight](https://github.com/schmooblidon/meleelight)
(browser JS Melee remake, upstream pin `27af171`) to the FunKey-S handheld.
Private, personal-use hobby project — no distribution.

**Status: spec locked, build loop ready** (wayfinder map completed
2026-07-14 — issue #1 and closed tickets #2–#13 hold the full decision
trail). The repo is self-sufficient: an agent landing here with no other
context starts at [`PLAN.md`](./PLAN.md) and [`CLAUDE.md`](./CLAUDE.md).

## Index

| Surface | File |
|---|---|
| **The spec** — locked strategy (S1 C rewrite), verification spine, M0–M4 milestone ladder, input/renderer/audio sections, risks | [`PLAN.md`](./PLAN.md) |
| **Hard rules** (always loaded) + gate table + verified commands | [`CLAUDE.md`](./CLAUDE.md) |
| **Loop protocol** (one bounded iteration; sentinels) | [`docs/LOOP.md`](./docs/LOOP.md) |
| Independent verifier / planner contracts | [`docs/loop/CHECKER.md`](./docs/loop/CHECKER.md) · [`docs/loop/REPLAN.md`](./docs/loop/REPLAN.md) |
| Task queue (phase-scoped, REPLAN-owned) | [`fix_plan.md`](./fix_plan.md) |
| Iteration ledger + LOOP STOP sentinels | [`docs/AGENT-LOG.md`](./docs/AGENT-LOG.md) |
| Licensing (upstream MIT carried verbatim + provenance) | [`LICENSE-meleelight`](./LICENSE-meleelight) · [`NOTICES`](./NOTICES) · [`docs/LICENSING.md`](./docs/LICENSING.md) |

Evidence appendix (research + measured spikes, all merged onto main):

| | |
|---|---|
| Codebase anatomy (#2) | [`docs/research/meleelight-anatomy.md`](./docs/research/meleelight-anatomy.md) |
| License/provenance (#3) | [`docs/research/meleelight-license.md`](./docs/research/meleelight-license.md) |
| Porting-strategy survey (#4) | [`docs/research/porting-strategies.md`](./docs/research/porting-strategies.md) |
| FunKey envelope + ssb64 reusables (#5) | [`docs/research/funkey-envelope.md`](./docs/research/funkey-envelope.md) |
| B0XX digital→analog mapping (#6) | [`docs/research/b0xx-mapping.md`](./docs/research/b0xx-mapping.md) |
| Determinism/oracle spike (#7) | [`docs/research/determinism-spike.md`](./docs/research/determinism-spike.md) + [`spikes/determinism/`](./spikes/determinism/) |
| On-device feasibility spike (#8) | [`docs/research/device-feasibility-spike.md`](./docs/research/device-feasibility-spike.md) + [`spikes/device-feasibility/`](./spikes/device-feasibility/) |
| Control-mapping prototype (#9) | [`prototypes/control-mapping/`](./prototypes/control-mapping/) |
| Device audio spike (#12) | [`docs/research/audio-spike.md`](./docs/research/audio-spike.md) + [`spikes/device-audio/`](./spikes/device-audio/) |

## Locked goals / restrictions

1. **Faithful port** — meleelight's gameplay verbatim; deviations are bugs.
   The browser original is the oracle (deterministic input-replay +
   per-frame state checksums — proven real in #7, not aspirational).
2. **Input** — B0XX-style digital→analog mapping layer (scheme S1
   "One-Mod + C-layer", PLAN §6). Core techniques guaranteed; engine
   untouched. Chase ratifies the scheme hands-on at the M3 gate.
3. **Performance** — hard 60 fps simulation AND render at 240×240
   (renderer measured at 2.54 ms worst-case on real hardware, #8).
4. **Private, personal use** — publication out of scope (`docs/LICENSING.md`).
5. **Solo parity is the playable bar** — meleelight has a CPU opponent
   (`ai.js`), so solo = menus, VS-vs-CPU, target test; the AI ports in M4.

## Running the loop

```sh
git switch -c agent/auto          # the loop refuses any other branch
bash scripts/loop.sh              # interactive approvals by default
# unattended (owner opt-in, git-guardrails hook installed):
# PERM_MODE=bypassPermissions MAX_ITERS=300 bash scripts/loop.sh
```

One fresh-context iteration per `claude -p` invocation, protocol in
`docs/LOOP.md`. Halts on `LOOP STOP:` sentinels (device needed, M2-CAL
no-go, Chase ratification/acceptance gates) or `.loop/STOP`. First
iteration is a REPLAN that concretizes M0's seeded tasks.

Sibling project / prior art: `~/code_projects/ssb64-funkey-s` (toolchain,
platform-layer pattern, loop discipline — adapted here, see NOTICES).
