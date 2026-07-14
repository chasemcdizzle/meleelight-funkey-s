# REPLAN contract (planning sub-agent)

Invoked by the loop in **step C-(b)** when the next actionable item's
`done-check:` is non-runnable (empty / `…` / missing / prose). Runs as a
sub-agent. It studies, then writes.

## Inputs

- `phase` — current phase id (M0 / M1 / M2-CAL / M2 / M3 / M4).
- Read: `PLAN.md` §4 (the phase's scope + EXIT definition — binding),
  `CLAUDE.md §Gates`/`§Commands`, `docs/AGENT-LOG.md` (what's been tried),
  the relevant evidence (`docs/research/*`, `spikes/*`, `prototypes/*`),
  and the upstream clone where needed.

## Duties

1. Decompose the phase into the **smallest verifiable steps** advancing
   its EXIT definition.
2. Give EACH step an **exact, runnable `done-check:` command** (no `…`,
   no prose) with a pass condition. Each step < ~400 line diff,
   independently verifiable.
3. **Concretize the phase EXIT gate:** turn this phase's *(REPLAN)* cell
   in `CLAUDE.md §Gates` into an exact command recorded under
   `CLAUDE.md §Commands`. The command must implement PLAN §4's EXIT
   definition faithfully — REPLAN may pick tools/paths, never weaken the
   contract (exact-equality checksums, full trace lengths, coverage
   counts).
4. Sort by dependency/priority.

## Output

Overwrite ONLY the current phase's section of `fix_plan.md`, each item:

```
N. <imperative task> — done-check: <exact command> → <pass condition>
```

Update `CLAUDE.md §Commands` with the concretized gate (and any build
commands the phase introduces). Do not touch other phases' sections. Do
not implement anything. Commit `fix_plan.md` + `CLAUDE.md`; the loop
iteration then ends.
