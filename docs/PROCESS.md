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

**The whitelist-grammar rule (owner ruling 2026-07-16, from brawlback-lab
— paid for there with ~14 review rounds).** Any tool whose OUTPUT is a
decision (verdict script, pass/fail checker, scorer, comparator) and whose
INPUT is parsed text (logs, headers, config, build/serial output) must not
use permissive parsing ("right prefix → scan for key=value → take what you
find"). Permissive parsers have an infinite hole class — truncated writes,
duplicate keys, coincidental token matches — and an adversarial reviewer
finds a new one every round, forever (the tell: non-monotone finding
counts, always the same input-trust category; this repo's own instances:
RC-marker rounds 1-2, manifest-eval rounds 1-3, timing-grammar task-4 r1).
The fix is by construction: (1) measure the producer's exact grammar
EMPIRICALLY from the full corpus of real files/logs, never docs or memory;
(2) parse only through anchored, full-line patterns matching that grammar
exactly; (3) binary outcome — exact match → parse; resembles-but-doesn't-
match → corruption → fail closed (drop the leniency, loud counter), no
partial parses, no silent skips; (4) validate the strict parser against
the entire real corpus before shipping — zero false rejections on genuine
data. Apply up front to anything decision-bearing; retroactively the
moment an arc raises the same input-trust objection twice.

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

## 11. Model assignment (owner ruling 2026-07-25)

- **Driver = Claude Fable 5** (`claude-fable-5`): runs the loop —
  orientation, dispatch, ground-truth ritual (§5), cold done-check
  re-runs, STATE/AGENT-LOG upkeep, arc arbitration, commits. The driver
  writes NO shipping code; "micro" iterations the driver used to take
  itself now dispatch to a writer like everything else.
- **Writer = Claude Opus 5** (`claude-opus-5`): ALL coding work. Pin
  the model at dispatch time (`claude --model claude-opus-5 -p <brief>`,
  or the harness agent's model field where dispatch goes through the
  Agent/Task tools) and record the model in the iteration's AGENT-LOG
  entry. **The writer session owns its whole review arc**: implement →
  invoke Codex itself (`codex exec`, backgrounded per §7.8) → fix what
  is warranted per the existing bar (Medium+ fixed; Lows dispositioned
  in writing) → repeat to VERDICT: GO or the §3 cap — all in ONE writer
  session, no driver round-trip per round.
- **Reviewer = Codex** (unchanged §3): every change still gets at
  least one review round — the §3 tiers already guarantee this (Tier B
  is never wholly skippable). §3's contract is unchanged by
  writer-invoked review: prompt + full log under
  `.loop/review-<iter>-<n>.log`, exact verdict line, and the DRIVER
  reads verdicts from the log files cold at ground-truth time (§5),
  never from the writer's summary. Driver arbitrates disputes and caps.
- **Codex-failure fallback (owner ruling 2026-07-26): grok AND an
  Opus 5 reviewer, BOTH.** When a codex round is proven failed (cached
  output cmp-proven, wedge, no verdict), the replacement round is TWO
  independent reviews — `grok` and a fresh `claude --model
  claude-opus-5` reviewer subagent (reviewer ≠ the writer's own
  session; same prompt, separate logs `.loop/review-<iter>-<n>{g,o}.log`,
  each with its own exact verdict line). Medium+ findings from EITHER
  are fixed/dispositioned; the round counts as GO only if BOTH end GO;
  disagreement → driver arbitrates. This supersedes the earlier
  grok-only fallback and the "no claude reviewers" rulings FOR THE
  FALLBACK PATH ONLY — codex remains the primary; Opus never reviews
  while codex is healthy.

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

## 12. Worktrees by default for feature lanes (owner ruling 2026-07-29)

**Every writer lane runs in its own git worktree.** The prior practice —
partitioning lanes by file so they could never touch the same source —
is retired. It bought conflict-avoidance at the cost of artificial
serialization (whole punch-list items waited on an unrelated lane
because they shared a file), and when conflicts did occur they were
trivial: the iter-125 merge was five instances of two lanes appending
their own TU to the same build list, resolved by union in seconds.
Git's 3-way merge is built for this; hand-partitioning is a worse
version of it.

**What worktrees do NOT solve — the reason lanes still need routing:**
1. **Semantic conflicts are invisible to git.** Two lanes can edit
   disjoint files and still break each other. Measured instances: the
   CSS lane changed flow scripts and the injector cadence while the
   device lane's `--foh-max` derivation depended on them (no textual
   conflict, real dependency); U1 changed background PIXELS, which
   invalidated device screenshot evidence held by a different lane.
   Routing exists for coupling, not for file collisions.
2. **The device is a singleton.** One lane drives the FunKey at a time.
   That is a hardware lock; worktrees are irrelevant to it.
3. **The evidence layer is global.** m4-freeze-manifest.txt rows and
   verify_m4.sh's MANIFEST_SHA256 anchor are single-writer by nature —
   which is an argument FOR this model: lanes report new shas, the
   driver re-pins centrally, and the anchor never becomes a merge
   hotspot.
4. **A review GO covers BYTES, not intent.** Merging changes the bytes
   a lane's arc approved. The merge PRODUCT was reviewed by nobody.

**Therefore the merge ritual is binding (driver duty):**
- Driver remains sole merger; lanes never commit.
- After every merge, re-run the affected checks COLD in the merged tree
  — a lane's own green is evidence about its worktree, not about HEAD.
- **Verify a CONTENT FINGERPRINT of each merged file** (line count, a
  pinned token), never the patch tool's own success messages: at
  iter-131 `git apply --3way` printed "Applied cleanly" per file and
  then ROLLED BACK ATOMICALLY on a later conflict; the cold re-run
  reported the pre-merge ledger and was nearly waved through as a stale
  build. Only the disagreement between the writer's claimed count and
  the measured one caught it.
- Any pinned producer whose bytes moved in the merge goes back to
  `arc-in-flight` until a round covers the MERGED bytes, or the driver
  records why the existing GO still binds (mtime/diff proof, as at
  iter-132).

### 12.1 Reconciliation with the protected files (read this before "fixing" a rule)

CLAUDE.md HARD RULES and `docs/LOOP.md` are **protected by HARD RULE 3** —
the driver may not edit them. This section is a READING that reconciles
§12 with them, never an override (PROCESS supplements; stricter wins).

- **HARD RULE 4 "ONE branch `agent/auto`; ONE atomic commit per completed
  iteration; clean tree between."** Worktree lanes are COMPATIBLE, not in
  tension: lanes never commit, so `agent/auto` remains the only branch
  that receives commits and the one-atomic-commit-per-increment invariant
  is unchanged. Lane branches (`worktree-agent-*`) are ephemeral
  scaffolding for uncommitted work.
- **HARD RULE 4 "never delete branches."** Stricter reading adopted: a
  lane worktree/branch is pruned ONLY after its work is merged AND
  committed to `agent/auto`. Until then it is the only copy of that work
  — and it has already saved us once (iter-133: a lane's transcript was
  unrecoverable after a session-limit death while its worktree held 25
  modified files and four review rounds; a fresh writer resumed from the
  worktree with nothing lost).
- **LOOP.md §A.2 "clean tree or STOP."** Worktrees-by-default RESTORES
  this invariant rather than straining it: lanes-in-the-main-tree are
  what make it dirty. With every lane in a worktree, the main tree is
  clean between driver commits, which is exactly what the guard wants.
  If the owner ever wants HARD RULE 4's literal text amended to name
  worktrees, that is HIS edit to make; nothing requires it.

### 12.2 Operating procedure (measured against this week's actual costs)

1. **Base freshness.** Every lane worktree is created from CURRENT HEAD,
   the brief states that commit, and the lane VERIFIES it (`git rev-parse
   HEAD`) before its first edit. Measured cost of not doing this: one
   lane's worktree was created at an M0-era commit with no `pipeline/`
   directory at all and had to be fast-forwarded mid-arc.
2. **Rebase-before-handoff, not merge-after-report.** A finishing lane
   rebases onto current HEAD and re-runs its OWN done-check before
   reporting. Then the bytes it hands over are the bytes it tested. If
   the rebase moves bytes its review arc covered, it says so — the arc
   needs a delta round, or the driver records why the GO still binds
   (mtime/diff proof, iter-132 precedent).
3. **Batch merges that touch pinned producers.** Every manifest edit
   forces a MANIFEST_SHA256 recompute + full self-check; N merges = N
   cycles. Merge the pending lanes first, then do ONE re-pin pass. (Cost
   measured: five separate re-pin cycles in one day, each recompute +
   verify, all avoidable.)
4. **Re-run by consumption, not by ritual.** After a merge, re-run the
   checks whose INPUTS the merge touched — not everything, not a guess.
   This wants a DERIVED map (see C20), never a hand-maintained list:
   C13 already burned us on a hand-maintained enumeration going stale.
5. **Fingerprint, don't trust the tool.** Verify a content fingerprint of
   each merged file (line count / pinned token). `git apply` prints
   per-file success and can still roll back atomically (iter-131).
6. **Semantic coupling is the real merge risk — git cannot see it.**
   Registry of measured instances, to be named in briefs:
   flows/`.expect` ↔ the injector cadence and `--foh-max` derivations ·
   rendered pixels ↔ device screenshot evidence (U4) · any pinned
   producer ↔ the manifest row + anchor · the checksummed sim surface ↔
   anything under `port/sim/` (proof is always `SIM CONFORMS`).
7. **Singletons stay serialized regardless of worktrees:** the physical
   device (one lane at a time), the manifest/anchor (driver-only), and
   docker builds (SERIAL, CLAUDE.md note).
8. **Accept the cold-build cost.** A fresh worktree has no arm-build
   stamp and no `pipeline/build/` artifacts, so the first device-facing
   build in a lane is cold. That is minutes, and it is cheaper than the
   contamination risk of sharing build dirs across lanes.
