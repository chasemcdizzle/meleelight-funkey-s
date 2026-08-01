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

Reviewer: `codex` CLI (adversarial stance); when a codex round is PROVEN
FAILED the replacement is the §11 fallback — `grok` AND an Opus 5 reviewer,
BOTH (the older "fallback = grok" wording is superseded; see §11).
Contract: prompt file + full log on disk under `.loop/`, exact verdict line
required (`VERDICT: GO` / `VERDICT: NO-GO`), driver reads the verdict FROM
THE LOG FILE, never from an agent's summary. Since 2026-07-30 rounds are
launched through `port/review/review-harness.sh`, which writes that bundle
under `.loop/arc/<arc-id>/` (see below); the older ad-hoc
`.loop/review-<iter>-<n>.log` paths remain valid for reading archived arcs.

**Verdicts are emitted UNADORNED (binding; C11, iter 133).** The verdict
must occupy its own line, at column 0, with no decoration whatsoever:
`VERDICT: GO`. NOT `**VERDICT: GO**`, not `(VERDICT: GO — 0 Highs)`, not
indented, quoted or bulleted. The only grep any judge or driver may use
is the ANCHORED, full-line `^VERDICT: GO$` — and every reviewer prompt
must state this requirement to the reviewer. Rationale, paid for: at
iter-127 a manifest row was flipped to `reviewed-go` citing a log whose
sole verdict was the markdown-bold form, which has ZERO anchored
matches; the anchored grep was run, returned nothing, and the judgement
fell back to eyeballing a `tail`. An adorned verdict is NOT a verdict —
a reviewer that emits one has failed to deliver a verdict, and the round
is void (re-run it) rather than interpreted. This is now enforced
mechanically: `verify_m4.sh`'s cite verification requires every
`reviewed-go` row to cite at least one log whose **TERMINAL** anchored
verdict is `VERDICT: GO` (or, for a driver cap, a ledger entry bound to
that producer), so an adorned verdict can no longer be laundered into the
freeze manifest by a human reading. "Terminal" matters and is not
pedantry: a reviewer log echoes its own prompt, and the prompt states
this grammar — so an anchored `VERDICT: GO` appears inside rounds that
ended NO-GO. Taking any match, rather than the last, would have accepted
those. Known residual, registered rather than papered over: a verdict
quoted in a transcript appended AFTER a report is positionally
indistinguishable from a real one, so the durable fix is a structured
closure record binding producer path + pinned sha + verdict.

**That residual is now CLOSED: the reviewing harness writes its own verdict
artifact (owner ruling 2026-07-30; fix_plan R4; format
`port/review/FORMAT.md`).** Rounds are launched through
`port/review/review-harness.sh run` (`--arc --round --reviewer --role --tier
--prompt --scope`), which archives the log, prompt and scope under
`.loop/arc/<arc-id>/` and, when the reviewer exits, writes a sealed RVERDICT2
artifact binding **arc id · round · reviewer · role · the exact reviewed
scope BYTES (a per-file sha256 manifest) · the log's sha256/bytes/lines/NUL
count · the reviewer's rc and the deadline it ran under · the terminal
verdict and its line · which rule the round counts toward**.
**ARC CLOSURE IS A DRIVER/HUMAN JUDGEMENT, INFORMED BY THOSE ARTIFACTS
(owner ruling 2026-07-31 — see §3.1 below).** `bash port/review/arc-report.sh
--arc <id>` is a DIAGNOSTIC that reads them: it refuses evidence it cannot
read (bad seal, mutated log, cross-wired bundle, NULs, version skew,
incoherent VOID reason, artifacts that contradict each other) and otherwise
REPORTS what the artifacts say — every round's reviewer, role, verdict,
times, rc, deadline and scope; whether a §11 pair is complete; what a cap
claims; whether the reviewed bytes are still on disk. It decides nothing, and
neither its output nor its exit code authorizes anything. **It is not a
substitute for reading FORMAT.md §7, which is BINDING disclosure of what it
does not verify** — most importantly that SCOPE MEMBERSHIP (which paths the
arc covers) and PROMPT SEMANTICS (whether the reviewer was actually told to
be adversarial) are caller assertions, that `--tier` is an argument rather
than a derivation, and that a transcript-decided round's recorded verdict is
a reported field rather than a proven decision. The closure-rule vocabulary
is still recorded in every artifact and still means what §3/§11 say —
`process3-tier-a-go` (codex GO), `process11-fallback-dual-go` (§11 — BOTH
grok and opus5 GO; one alone does not close it), `process3-capped` (a
harness-written RVCAP1 naming the recurring class), plus BOTH Tier A+
obligations, the independent second reviewer AND the byte-identity
regression, the latter representable because `review-harness.sh regression`
RUNS the lane's regression command and seals its rc and output (RVREG1)
exactly as it does a review round. The report tells a driver which of those
ingredients are ON DISK; it does not tell anyone that they add up. Poll for
the ARTIFACT, never for a substring of the log. Five measured failure modes
closed, all from
2026-07-30: foreign-GO-at-EOF laundering · no machine-readable arc identity
(the 19 `x-*` cross-artifact rows) · fabricated work-status ledgers ·
"reached GO" under a two-reviewer rule with one reviewer · corrupt logs whose
readable text contradicts their rc. The mechanism against the first is
structural rather than a hash alone: the harness terminates every log with a
nonce-bearing `REVIEWER_RC <nonce> <rc>` + `HARNESS-EOR <nonce>` pair, the
verdict is read only from the region ahead of it, and `HARNESS-EOR` must be
the log's final line — so a transcript pasted into a finished log is out of
region, after the terminator, AND a hash mismatch.

### §3.1 Why there is no arc-closure JUDGE (owner ruling 2026-07-31)

An earlier version of this section declared `arc-closure.sh` "the ONLY
sanctioned answer to 'is this arc closed?'" and listed three shapes it had
just closed. **That sentence is withdrawn, and two of the three "closures"
did not hold.** THREE independent adversarial passes over the judge — the
arc's own round 7, then two independent Tier A+ reviewers — each produced a
green `ARC CLOSED` line over a review that had not happened, each time
through a shape the previous fix had not anticipated:

- The **ambiguity rule** (a transcript carrying BOTH anchored verdicts is
  VOID) closes the shape it was built for, and leaves the class: a
  transcript that emits **no verdict of its own** and ends with a quoted
  foreign anchored GO is recorded GO with no ambiguity at all — so the rule
  makes DELETING the honest verdict the winning move.
- The **round-order rule** (labels must agree with `started-utc`) never read
  `ended-utc`: a GO that started one second later and finished six seconds
  earlier than an adverse round still closed the arc.
- The **§11 basis-eligibility rule** was a two-item blacklist over eight VOID
  reasons; two fresh eligible "proven codex failures" were manufactured in
  zero seconds with no repo write (codex off PATH → rc 127; one sentence of
  prompt → rc 0 `decision-malformed`).
- And FORMAT.md §7's own disclosure that the open output-quiescence gap
  "produces a REFUSAL, not a false GO" was measured FALSE 3/3: it produces a
  self-consistent `verdict: GO` written by a background process, and closed
  arcs.

The through-line is the finding: **every fix closed the measured instance and
left the class** — CLAUDE.md HARD RULE 8's hierarchy (instrument > class fix >
registered one-off) inverted three times. A judge with known false-GREEN paths
is worse than no judge precisely because the process had declared it
authoritative. So the owner's ruling: **KEEP the producer, DROP the judge's
authority.** `review-harness.sh`'s provenance-bound artifacts stay — they
close the original five failure modes AS EVIDENCE and were never the problem.
`arc-closure.sh` is demoted to `arc-report.sh`, a diagnostic that reports what
it observes and decides nothing; the round-order and basis-eligibility rules
are DELETED rather than reported, because an observation derived from an
unsound rule is still an unsound claim. **Arc closure remains a driver/human
judgement informed by the artifacts.** Do not re-add a judge without arguing
the class fix and having it reviewed as one.

Teeth: `bash port/review/check-review-artifact.sh` →
`REVIEW ARTIFACT TEETH OK`, which refuses the real laundering specimen and the
real 44.4%-NUL specimen (both kept as fixtures under
`port/review/specimens/`), requires the report to DISCLOSE every situation a
deleted closure rule used to refuse on, and whose measured single-check OFF
controls — with their stated bounds, including that OFF controls say nothing
about checks that were never written — are tabulated in FORMAT.md §6. NOTE
the deliberate scope: this is the review layer's own record. `verify_m4.sh`
and `m4-freeze-manifest.txt` are untouched by it, and nothing in the freeze
manifest consults `arc-report.sh` — a diagnostic is not a cite verifier, and
the manifest is single-writer by nature (§12(3)).

**§12.3(5) is now ENFORCED for arc-claiming rows.** "Evidence must outlive
the worktree" has a mechanical check: for every `reviewed-go` /
`arc-in-flight` / `arc-pending` row, `verify_m4.sh`'s cite verification
resolves each `.loop/` artifact the cite names — including every
alternative of the `r{1,2,3}` multi-round brace form — and REFUSES if any
is missing, empty, or a symlink. Scope stated precisely on purpose:
`oracle-frozen` and `grandfathered-m{1,2}` rows are proven by a NAMED
EXIT GATE rather than by an arc, their `.loop/` mentions are provenance
prose, and their references are NOT opened — do not read this as
evidence-survival being enforced everywhere. Found live the first time it
ran — the
`pipeline/expected-assets.json` row cited a `DUAL-GO` closure whose two
logs had never left the C4 lane's worktree (the GOs were real; the
evidence was invisible to the gate). Copy an arc's logs into the
repo-root `.loop/` as part of closing it; a cite the gate cannot open is
not a cite.

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
  Rounds launched through `port/review/review-harness.sh` (§3) satisfy
  this contract and additionally leave the sealed verdict artifact the
  driver reads with `arc-report.sh` instead of re-reading log bytes — a
  DIAGNOSTIC read, not a delegated decision (§3.1).
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
  while codex is healthy. **Mechanically enforced since 2026-07-30:**
  a fallback round is recorded with `--role fallback` plus a
  `--fallback-basis` naming the RECORDED failed codex round it rests on.
  `arc-report.sh` refuses a record whose basis is incoherent (absent, not a
  VOID codex `primary` of that round, or on other prompt/scope bytes) and
  REPORTS whether the pair is complete and what the basis actually was —
  void-reason, rc, deadline, harness-kill, elapsed. **Whether a codex round
  is PROVEN failed, and whether a complete pair closes the round, stay the
  driver's call** (§3.1: the mechanical eligibility rule was measured
  bypassable in zero seconds and was deleted). Teeth:
  `port/review/check-review-artifact.sh` T21-T26, T53, T54, T94, T96.

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

### 12.3 Shared scarce resources — device, docker, browser (assessed 2026-07-29)

**What is genuinely scarce, and what is not.** Most of this project's
evidence is HOST-side by design (host twins, headless backend, the
oracle harness), and that separation is the main reason lanes can run in
parallel at all. Hardware is required for exactly: real frame timing /
p99 + skips, the real SDL input path, real audio, OPK/frontend launch,
and framebuffer screenshots. Everything else already runs host-side and
does not contend.

**1. The device is a hard singleton — batch it, don't queue-jump it.**
`riglib.sh` already carries a shared no-reclaim lock (`rig_lock_acquire`
/ `rig_lock_release`, ownership-checked, EXIT-trap covered) plus a
deadman that restores device state if a lane dies. That is correct and
needs no change. What DOES change under parallel lanes: N host lanes
each finishing with a few minutes of device work is the worst possible
shape — N lock acquisitions, N frontend park/restore cycles, N cold arm
builds. **Adopt device WORK ORDERS:** a host lane that needs hardware
does not take the device; it emits a work order under `.loop/` (what to
run, what evidence to pull, what verdict lines it expects) and the
single device-owning lane DRAINS the queue in one session. This is
PROCESS §9's run-batching applied across lanes instead of within one.

**2. Docker/arm builds stay SERIAL (CLAUDE.md), but should be far rarer.**
The image is amd64 under emulation; parallel builds thrash. The real
waste is that every worktree starts with a cold `arm-build.stamp` and
rebuilds everything even when its sources are byte-identical to a build
that already exists in another lane. **The fix already exists in the
codebase and only needs relocating:** `rig_srchash` computes a content
hash over all `port/{sim,gfx,tools,foh,fdlibm,ryu}` + `oracle/qjs`
sources, the generated `ml_tables.c`/`ml_stages.c`, every rig script,
AND the resolved docker image id. That key is exactly a content address.
A **content-addressed shared build cache** (`~/.cache/mlfk-arm/<srchash>/`)
therefore cannot serve the wrong bytes by construction: a lane that
changed any input gets a different key and builds cold; a lane that
changed nothing gets a hit. **Non-negotiable safety conditions** — this
is evidence machinery, so it fails closed: re-hash every artifact on
READ before use (`rig_stamp_rehash` already does this adjacent to push),
never populate the cache from a build that did not verify, and keep
`MLFK_FORCE_ARM=1` as the escape hatch. Registered as C21; it touches a
pinned producer, so it needs its own arc.

**3. Pipeline artifacts are byte-stable BY PROOF — cache them the same
way.** The M1 gate's whole contract is that two fresh runs produce
byte-identical manifests, so sharing pipeline outputs across lanes
changes no result. Same content-addressed discipline (key on the
pipeline inputs + stage code), same verify-on-read. Note the live
counter-example that proves the key must be on INPUTS, not paths: the
C4 lane legitimately changed `img1.js` and its `menu.img1` SHOULD differ
— a path-keyed cache would have silently served it stale bytes.

**4. Browser/oracle runs are host-side and parallel-safe** (separate
Chrome instances, separate served dirs), but they are heavy; two lanes
capturing simultaneously is fine, four is not worth it.

**5. Evidence must outlive the worktree.** Review logs and measurement
artifacts written to a lane's `.loop/` vanish when the worktree is
pruned — while manifest cites point AT them. Either the lane writes them
to the main repo's `.loop/`, or the driver copies them at merge time
(done manually at iter-132 for the CSS arc logs). This is the C8 lesson
generalized: evidence must live where the driver reads it, permanently.
