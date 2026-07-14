# meleelight → FunKey-S — Autonomous Build Loop (rev. 1, adapted from ssb64 rev. 6)

You run ONE bounded iteration, then stop. **State lives ON DISK. Trust the
files, not your memory.** ONE branch for the whole run: **`agent/auto`**.
Each iteration ends with a **clean tree** (one atomic commit, or none).

## AUTONOMOUS MANDATE (owner may be asleep — make the calls yourself)

**Maximize progress; do NOT stop for the human unless truly impossible.**
You have full authority over engineering AND product-default decisions:

- **Install/fetch tooling yourself** (brew/docker/npm/pip, SDKs) — never
  block waiting for a human install.
- **When blocked, route around it**: resequence/defer, pick another tool,
  choose the simplest sensible default (e.g. golden-trace char = the
  existing Fox/Marth/Battlefield trace first). Document decisions in
  `docs/AGENT-LOG.md`; tag genuinely new judgment calls
  `PROVISIONAL (auto-adopted)`.
- Reserve `LOOP STOP:` sentinels for things genuinely impossible without
  the owner: the **physical FunKey-S is required and absent from ADB**, a
  **Chase-ratification gate** (M3 playtest, M4 acceptance, M2-CAL no-go),
  or a **destructive/irreversible** action. Try alternatives and exhaust
  the retry budget first.
- Prefer shipping a working-but-imperfect step over halting — but never
  fake a gate; the oracle is exact-equality and stays that way.

**Two-level gating (important):**
- A **task `done-check:`** = the exact command proving ONE `fix_plan.md`
  item. Run **every iteration**.
- A **milestone exit gate** (`CLAUDE.md §Gates`) = the contract proving a
  whole phase. Run **only on a phase-advance iteration**, never per task.

## A. Guards (FATAL; never dirty a tracked file here)

1. **Branch:** HEAD must be `agent/auto`. Else → write `.loop/STOP.txt`
   (git-ignored), HALT. Touch no tracked file.
2. **Clean tree:** `git status --porcelain` empty. Dirty → prior crash:
   STOP to `.loop/STOP.txt` for a human; do not auto-discard.
3. **State files present:** `CLAUDE.md`, `PLAN.md`, `fix_plan.md`,
   `docs/AGENT-LOG.md`, `docs/loop/CHECKER.md`, `docs/loop/REPLAN.md`.
   If any missing → create minimal valid form, commit, HALT.

## B. Orient

Read `CLAUDE.md` (rules, §Gates, §Commands), `PLAN.md` §4 (the current
milestone's contract), `tail -40 docs/AGENT-LOG.md`, then `fix_plan.md` —
note `Current phase:` and its items. Phases: **M0 → M1 → M2-CAL → M2 →
M3 → M4** (contracts in PLAN §4).

## C. Decide the iteration type

- An **exact `done-check:`** = a single runnable shell command with a pass
  condition — NOT prose, not empty, not `…`.
- **(a) ≥1 item WITH a runnable `done-check:`** → TASK iteration: pick the
  single highest-priority such item. Go to D.
- **(b) items exist but the next actionable one's `done-check:` is
  non-runnable** → run **REPLAN** (`docs/loop/REPLAN.md`): rewrites this
  phase's items with runnable `done-check:`s AND concretizes this phase's
  exit gate into `CLAUDE.md §Commands`. Commit, HALT (one iteration).
- **(c) NO items left for the phase** → PHASE-ADVANCE iteration: go to
  F-advance.
- **Device check (M3/M4 only):** if the chosen task needs the physical
  device, verify it is on ADB (`adb devices`). Absent → do NOT burn the
  iteration: pick the highest-priority device-free task instead; if none
  exists, append an `ESCALATION:` entry ending with the `m3-device`
  sentinel (§H), commit, HALT.

## D. Implement a small diff (< ~400 lines, working tree only — do NOT commit yet)

Search before creating. HARD RULES (violation → `git restore .` then G):
behavior > compiles; no stubs/placeholders/TODO-as-done; never touch the
oracle/goldens/gates/`CLAUDE.md` rules/this file/`docs/loop/*`; doubles
only, vendored fdlibm, `-ffp-contract=off`; never force-push or touch any
upstream remote. ZOOM OUT before and after (CLAUDE.md rule 8).

## E. Verify the WORKING TREE (TASK iteration) — artifacts, never self-report

1. Run the task's exact `done-check` to `.loop/*.log`; record exit code.
2. **Conformance guard:** if `CLAUDE.md §Commands` lists a concretized
   oracle-conformance command for the current phase (M2-CAL onward: the C
   sim replay of golden traces), run it too — checksum conformance must
   still pass after every change.
3. **CHECKER** (`docs/loop/CHECKER.md`, mode=`task`): independently
   re-runs the done-check + conformance guard against the uncommitted
   tree, tamper-checks `git diff HEAD`, returns STRICT JSON
   `{verified,evidence,gaps,tamper}`. Proceed to F only if
   `verified && !tamper`; else G.

## F. Commit the iteration atomically (only after CHECKER passes)

All edits BEFORE the single commit (tree clean after):
1. Remove the done item from `fix_plan.md`. Append reusable
   commands/gotchas to `CLAUDE.md §Commands/§Notes`.
2. Append the artifact manifest to `docs/AGENT-LOG.md`: `iter, phase,
   task, done-check cmd+exit, .loop log paths, artifact hashes, CHECKER
   evidence, zoom-out note, next`.
3. **Stage explicitly** (no blind `git add -A`): only source/docs/config
   you changed; `git status --porcelain` must show nothing under `.loop/`
   or unexpected binaries. ONE `git commit`. No edits after.

### F-advance (PHASE-ADVANCE iteration, type C-c)

1. **CHECKER** (mode=`phase-advance`): runs this phase's exit gate
   (`CLAUDE.md §Gates`, the now-concrete command) + tamper. Not verified →
   REPLAN adds the tasks needed to satisfy the gate (commit, HALT).
2. If verified: set `fix_plan.md` `Current phase:` to the next phase;
   append `MILESTONE PASS: <phase>` to `docs/AGENT-LOG.md`; commit.
3. **If the next step is a human gate** — M2-CAL returned NO-GO, M3's
   ratification playtest, or M4 acceptance — make the commit's AGENT-LOG
   last line the exact sentinel (§H) and HALT.

## G. Repair / escalate

- Gate/CHECKER failed → fix THIS iteration (back to D). **Retry budget: 3
  on the SAME error**, then `git restore . && rm -rf .loop/checker`,
  append a `BLOCKER:` entry (failing tail ~20 lines + tries) ending with
  the blocked sentinel (§H), commit that single change, HALT.
- Cascading/contradictory errors or a wrong guessed fact → suspect a
  broken FOUNDATION; backtrack. Divergence hunting: structure parallelism
  means binary-search by module and by frame — localize before patching
  (and ZOOM OUT: one divergence is usually a CLASS of mistranslation).

## H. Sentinels (the driver watches for a line starting `LOOP STOP:`)

LAST line of `docs/AGENT-LOG.md`, exactly one:
- `LOOP STOP: m2-entry-no-go — <metrics + reason>` (calibration slice
  says the grind is untenable; Chase decides)
- `LOOP STOP: m3-device — <needed: device on ADB | Chase S1 ratification playtest>`
- `LOOP STOP: m4-complete — awaiting Chase acceptance playthrough`
- `LOOP STOP: blocked — <one-line reason>`

## Loop reach (honest)

Autonomous: **M0 → M1 → M2-CAL → M2** entirely on the host. **M3/M4** are
autonomous over ADB while the device is plugged in; escalate when it isn't
and for the two human gates (S1 ratification inside M3's gate, acceptance
inside M4's). Arming instructions: README §Running the loop.

## Caps (never modify)

Configured max-iterations / wall-clock / budget in `scripts/loop.sh`. One
task per iteration. Output to `.loop/*.log`. Branch `agent/auto` only.
