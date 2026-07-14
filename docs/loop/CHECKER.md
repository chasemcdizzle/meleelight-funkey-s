# CHECKER contract (independent verifier sub-agent)

Invoked by the loop (steps E and F-advance) as a **separate sub-agent**.
Writer ≠ checker. It **only reads and runs verification commands — never
edits, never commits.** It verifies the **uncommitted working tree** (the
loop has NOT committed yet).

## Inputs (the loop passes these)

- `phase` — current phase id (M0 / M1 / M2-CAL / M2 / M3 / M4).
- `mode` — `task` (verify ONE item's done-check) or `phase-advance`
  (verify the phase EXIT gate).

## Procedure (must actually execute, not infer)

1. **Placeholder check:** the command you're about to run must be exact
   and runnable — no `…`, no prose. If it isn't → `verified=false`, gap
   `"non-executable gate"`.
2. **Run, by mode:**
   - `task`: the in-progress `fix_plan.md` item's exact `done-check:`,
     output to `.loop/checker/`.
   - `phase-advance`: the phase's exit gate from `CLAUDE.md §Gates` (the
     concretized command in §Commands).
3. **Conformance guard:** if `CLAUDE.md §Commands` lists a concretized
   oracle-conformance command for the current phase (M2-CAL onward), run
   it — golden-trace checksum conformance must still pass.
4. Confirm required artifacts exist (build outputs, checksum-stream files,
   manifests, logs — per the check). Checksum comparisons are
   **exact-equality**: any epsilon, tolerance, frame-skip, or truncated
   comparison in the verification path is itself a FAIL.
5. **Tamper check:** `git diff --stat HEAD` (staged+unstaged). FAIL if it
   touches: `oracle/` or any committed golden checksum stream (except
   during M0, whose contract is building them), `spikes/determinism/`
   (frozen evidence), any test/fixture used by a gate, `docs/loop/*`,
   `docs/LOOP.md`, `CLAUDE.md` HARD RULES/§Gates, `LICENSE-meleelight`,
   or git hooks.

## Output — STRICT JSON only

```json
{ "phase": "M2", "mode": "task", "verified": true,
  "evidence": ["<cmd> → exit 0", "conformance guard → 3600/3600 frames identical"],
  "gaps": [], "tamper": false }
```

- `verified=true` ONLY if the command(s) passed, artifacts exist,
  conformance (if applicable) is exact, no placeholder, and
  `tamper=false`.
- `gaps`: only correctness/contract issues, not style. A missing required
  tool is a `gap` with `verified=false` — never "N/A".
