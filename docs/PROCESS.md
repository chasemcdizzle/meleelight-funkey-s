# PROCESS.md — adopted loop standards (owner ruling, 2026-07-16)

Provenance: adapted from brawlback-lab `harness/PROCESS-EXPORT.md`
(Chase's ruling 2026-07-16: apply everything applicable; deltas and
rejections recorded in docs/AGENT-LOG.md the same day). This doc BINDS the
driver and every writer brief. It supplements — never overrides — CLAUDE.md
HARD RULES and docs/LOOP.md; where they overlap, the stricter rule wins.

## Already-law mapping (do not duplicate)

Measurement-over-belief = HARD RULES 1/5 + capture-FIRST (M2CAL rule 7).
ZOOM OUT / class registry = HARD RULE 8 + fix_plan's numbered rules (1-18+).
Done-check teeth / OFF controls = the negative-test discipline (nibble →
exactly 1; perturb → count → restore). Writer ≠ checker + driver cold
re-runs = HARD RULE 7. Never-carve-the-oracle = HARD RULE 3 (stricter than
the export's amendment path — kept stricter). DIGEST = docs/AGENT-LOG.md
(append-only). BACKLOG = fix_plan.md. Milestone contracts = CLAUDE.md
§Gates + PLAN §4. Honest-exposure reporting = the "honest coverage" notes.

## 1. STATE.md (docs/STATE.md)

The ONE current-truth page. Driver updates it EVERY driver turn (writer
completion, outage recovery, milestone events). Top section = live right
now (phase, in-flight task + writer state, device state, last commit,
gates green/red); §rulings = standing owner directives, day-tagged;
superseded bullets get marked, not deleted. A fresh context reads
CLAUDE.md then STATE.md and is oriented.

## 2. Pre-registration

Any measurement, divergence-hunt, or ledger session freezes BEFORE the
first run/edit: method, run count/cap, pass criteria, and REFUTATION
SHAPES (what result refutes the theory, and what happens then — default
"one bounded evidence round, then STOP and report"). Frozen in the writer
brief and echoed in the AGENT-LOG entry. If you can't state what would
refute your fix, you don't understand your fix. Refutations are recorded
permanently in the AGENT-LOG ledger with evidence and an explicit
"do NOT retry blind" line — the loop never re-litigates a dead theory
without new evidence.

## 3. Adversarial review arcs (Codex; tiered)

Reviewer: `codex` CLI (adversarial stance), fallback `grok` if Codex is
unavailable. Contract: prompt file + full log on disk under `.loop/`
(`.loop/review-<iter>-<n>.log`), exact verdict line required
(`VERDICT: GO` / `VERDICT: NO-GO`), driver reads the verdict FROM THE LOG
FILE, never from an agent's summary.

- **Tier A (full arc to VERDICT: GO)** — every non-checksummed shipping
  surface: check scripts, device rig/hygiene scripts, renderer, input
  layer, OPK/launcher, gate-assembly scripts. Bounded convergence: past
  ~8 rounds, cap — fix Medium+, disposition Lows in writing, record
  CAPPED + name the recurring objection class.
- **Tier A+ (tier-up)** — anything touching a judge/verify path
  (verify-stream wrappers, check-script comparison logic, canon/ser):
  independent second review (different reviewer) + byte-identity
  regression on archived results.
- **Tier B (1-2 rounds, driver's judgment to skip)** — sim TUs whose
  done-check is a bit-exact oracle replay (the frozen checksum stream is
  the stronger reviewer), and diagnostic-only instruments.

Review ≠ verification: GO never substitutes for the done-check, and a
clean done-check never substitutes for Tier A review of a non-checksummed
surface.

**Review bar for rig/check scripts (threat model; iter 40).** Check
scripts must fail CLOSED against accident, corruption, partial failure,
staleness, and self-deception — every plumbing edge (cache keys, pulls,
parses, locks, rc propagation) dies loudly rather than reading as clean.
Findings that require an adversary with repo-write access or hostile
crafted payloads are dispositioned in writing by default, not fixed: an
adversary who can write the repo can edit the check script itself, so no
in-script defense is coherent against that actor. EXCEPTION: a
hostile-input fix is still taken when it is ≤ trivial AND closes the
whole class (the nonce-marker / no-eval pattern) — cheap class closure
beats a disposition. Reviewers should be pointed at this bar so rounds
converge on the accident/corruption classes instead of re-raising
adversary-with-write-access scenarios.

## 4. Artifact identity pins (narrow form)

- Every device check script sha256-verifies the ON-DEVICE binary against
  the freshly built host artifact BEFORE running it (`adbsh` pull-hash or
  push-then-hash; never trust push/build exit codes — this adbd drops
  exit codes anyway).
- All device evidence is pulled and judged host-side by
  cmp/sha256/verify-stream.js (already law) — the device never
  self-reports.
- OPK contents verified by checksum after packaging; installed-artifact
  identity checked at launch evidence time.
- Build stamp-caches must hash INPUTS (TU list + flags + sources), so a
  half-refreshed build cannot masquerade as current (`MLFK_FORCE_ARM=1`
  escape hatch stays).

## 5. Ground-truth from disk (driver ritual)

Never trust a writer's status text. On every writer completion: verify
the commit exists, tree is clean, `.loop/` logs exist and contain the
claimed verdict lines, then re-run the done-check COLD (already law).
After ANY outage/death/529 storm: ground-truth FIRST — docker builds and
device runs often COMPLETE after their watcher died; check artifact
mtimes/hashes and build-stamp coherence before redoing or trusting
anything. Process checks go by command PATH, not name pattern.

## 6. Respawn-with-context

Dead writer with a healthy transcript → nudge/resume via message (cheap).
Poisoned or unresumable → fresh writer pointed at (1) the fix_plan task,
(2) the on-disk artifacts + `.loop/` logs, (3) docs/STATE.md, (4) an
explicit exactly-what-remains list. 529s are transient: resume; 3 in a
row → back off one heartbeat; persistent → notify once, keep heartbeating.

## 7. Failure-mode catalog (watch specifically)

1. Dead-parked waiters — writers park on background tasks and die.
   FOREGROUND polls only (chunked `sleep N; check` inside one shell call);
   driver heartbeat catches slips. (Bit this project 3+ times.)
2. Session/credit kills mid-task — state lives in notes/logs/commits on
   disk; §5/§6 recover it. Wakeups stall during outages too.
3. Build-system traps — a "successful" build that didn't refresh the
   artifact you run. Verify by checksum/symbol, never exit code. (§4.)
4. Reviewer churn — an arc recycling one objection class. Cap and name.
5. Confounded experiments — one variable per done-check; if confounded,
   run the disambiguation control before concluding.
6. Framing inherited as fact — periodically re-derive key claims from
   primary artifacts (frozen streams, captures), not prior summaries.
7. Tool lies — verify critical comparisons with checksums (this project's
   instance: zsh `time cmd | tee` masking mid-pipe failures — exit codes
   by direct invocation).
8. Backgrounded interactive CLIs — a `codex exec` migrated
   foreground→background dead-parks reading stdin; launch
   background-from-start with stdin `</dev/null`.

## 8. Instrument exposure

Every structural / non-bit-exact check (e.g. the renderer IoU silhouette
check) states its blind spots and detectable-exposure figure in its
AGENT-LOG entry; "0 findings" at low exposure is NOT "clean". Thresholds
are measured-then-frozen and never loosened (already law). When a masking
issue dies, re-adjudicate what it censored.

## 9. Run batching

Device/docker time is cheap, wall-clock and driver attention are not:
writers queue full run matrices per dispatch (all goldens, all sweeps)
with a pre-registered hard cap and early-stop clause — not one run per
handoff. Docker builds stay SERIAL.

## 10. Owner rulings

Recorded day-tagged in docs/AGENT-LOG.md, mirrored in STATE.md §rulings,
restated in the loop prompt so they survive context loss. Escalate only
true impossibilities; batch decision points; push-notify milestone-grade
events and genuine decisions, not progress noise.

## Explicitly NOT adopted (with reasons; reopen only with new evidence)

- **Parallel lanes / claim files / host-idle multi-lane dispatch** —
  conflicts with HARD RULES 4/6 (one branch, one atomic commit per
  iteration, clean tree between). Reopen condition: wall-clock becomes
  the binding constraint (M4-scale), and then via git worktrees, not
  claim files.
- **Full pin/parking ceremony** (candidate bundles, OFF-hatch binaries,
  streak counters, integration windows) — the frozen oracle streams
  already make unreviewed-code-producing-accepted-results structurally
  impossible for checksummed surfaces; deterministic builds + git are the
  revert levers. §4's narrow form is the kept residue.
- **Checker amendment via evidence package** — HARD RULE 3 is stricter
  (never weaken; spec change = version bump + re-freeze all goldens in
  one change). Kept stricter.
- **DIGEST newest-on-top reordering** — AGENT-LOG stays append-at-bottom;
  STATE.md covers fast orientation.
