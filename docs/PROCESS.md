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
**Recovery pointer (amended 2026-07-16)**: AGENT-LOG entries carry
monotonic ids (iter N / driver day-tags); STATE.md records the latest
entry id, both updated in the same commit. Recovery reads STATE, then
AGENT-LOG from that id to EOF; a mismatch means STATE is stale — 
reconcile before doing any work.

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
- **Tier B (light, never wholly skippable — amended per the reviewed
  reservations arc, 2026-07-16)** — sim TUs whose done-check is a
  bit-exact oracle replay, and diagnostic-only instruments. The stream
  proves behavioral equality ONLY inside the tested corpus — it cannot
  see UB, out-of-bounds writes later overwritten, or branches no golden
  exercises. Every shipping sim-TU change therefore gets at least ONE
  structural/coverage review round; skipping entirely is reserved for
  non-shipping diagnostics. Any Medium+ finding, or any change touching
  ABI/linkage/layout/lifetimes/bounds/pointers/overflow/build flags/
  error paths, escalates to a full Tier A arc.

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
- **Reviewed-pin freeze manifest for GATE evidence (adopted from the
  reservations arc, 2026-07-16 — R3's mechanical core)**: hashes prove
  identity, not approval. Every phase EXIT-GATE script (verify_m3.sh
  onward) carries a freeze manifest recording sha256 + review-GO status
  for each of its evidence PRODUCERS (check scripts, adbsh, judge tools,
  build recipe/image) and HARD-REFUSES to run when installed bytes don't
  match a reviewed pin. Any producer change invalidates prior gate
  evidence — the gate reruns with the new pins after their arc reaches
  GO. Per-task done-checks stay covered by the driver's ground-truth
  ritual; the manifest is a GATE-layer guarantee.

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

1. Dead-parked waiters — writers park on background tasks/monitors and
   end their turn. FOREGROUND polls only. The EXACT sanctioned pattern
   for runs longer than the shell-call cap (amended after 3 writer
   instances in one day, iters 42/45/47): start the run detached ONCE
   (`nohup <cmd> > <log> 2>&1 &` — capture the pid; wrap the cmd so the
   log's LAST LINE carries an explicit rc marker), then issue REPEATED
   foreground shell calls — each a BOUNDED until-loop (bare `sleep` may
   be blocked by the harness; use `for i in $(seq 1 8); do sleep 15;
   kill -0 <pid> || break; done; tail -3 <log>`) — read the result,
   issue the next, and NEVER end the turn while the run is live. Ending
   a turn "to wait for a notification" IS the failure mode.
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
  rejection scope NARROWED 2026-07-16 (reservations arc): what stays
  rejected is multiple WRITERS and claim files (conflict with HARD RULES
  4/6 — one branch, one atomic commit per iteration, clean tree between).
  A read-only pinned host lane overlapping ONE offline lane (worktrees,
  driver as sole merger) becomes eligible when its pre-registered trigger
  fires: across 5 completed iterations, serialized host waiting ≥ 20% of
  critical-path wall-clock while a consumer-registered offline task sat
  ready.
- **Full pin/parking ceremony** — rejection NARROWED 2026-07-16
  (reservations arc, R3 judged UNSOUND as originally stated): the
  mechanical core IS adopted — §4's reviewed-pin freeze manifest with
  gate-level hard refusal ("hashes prove identity, not approval").
  Still rejected: parked candidate bundles, byte-exact OFF-hatch
  binaries, and integration windows — artifacts here are
  deterministically rebuilt from a pinned commit with per-run recorded
  hashes, so git + the stamp already provide byte-exact restoration and
  attribution controls.
- **Checker amendment via evidence package** — AMENDED 2026-07-16 (the
  reservations arc found the laundering hole): HARD RULE 3's never-weaken
  stays, and version bumps alone prove CONSISTENCY, not LEGITIMACY. Any
  successor checksum-spec/checker version additionally requires an
  EVIDENCE PACKAGE: the demonstrated miss/defect, a minimal old-vs-new
  discriminating case pair, an anti-gaming argument in writing, an
  independent review, and a regression proving archived verdicts
  unchanged under the old checker — and the lane seeking relief from a
  verdict may not approve its own package.
- **DIGEST newest-on-top reordering** — AGENT-LOG stays append-at-bottom;
  STATE.md + the §1 recovery pointer cover fast, non-stale orientation.
