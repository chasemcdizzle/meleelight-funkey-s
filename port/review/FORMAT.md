# port/review — the reviewer-harness verdict artifact (RVERDICT2 / RVCAP1)

Owner ruling 2026-07-30 (docs/STATE.md §rulings; fix_plan R4). The reviewing
harness emits **its own** verdict artifact carrying an **arc id**, the **exact
reviewed scope**, the reviewer, the times, and a sealed verdict region — so
the EVIDENCE for "is this arc closed?" is a record the PRODUCER wrote, never a
reader's reconstruction from log bytes after the fact.

**Owner ruling 2026-07-31 — KEEP THE PRODUCER, DROP THE JUDGE'S AUTHORITY.**
Three independent adversarial passes over the judge that used to live here
each found FRESH false-GREEN paths to a printed `ARC CLOSED` line over a
review that had not happened, and the through-line was that each fix closed
the measured instance and left the class (CLAUDE.md HARD RULE 8's hierarchy
inverted three times). So: the artifact producer stays, and
**arc closure is a driver/human judgement informed by these artifacts.**
`arc-report.sh` is a DIAGNOSTIC that reports what it observes. It decides
nothing, it has no "closed" output, and no exit code it can return authorizes
anything.

| file | role |
|---|---|
| `review-harness.sh` | PRODUCER. Launches a round, writes the log + the artifact. |
| `arc-report.sh` | DIAGNOSTIC READER. Reports evidence defects and observations. **Decides nothing** — see §5. |
| `reviewers.sh` | The ONE reviewer table both of the above read. |
| `check-review-artifact.sh` | The done-check: named teeth over both layers (see §6 for the measured coverage bound). |
| `specimens/` | The two live failure specimens, sha256-pinned, used as fixtures. |

## 1. Producing a round

```
nohup bash port/review/review-harness.sh run \
  --arc <arc-id> --round <n> --reviewer <codex|grok|opus5> \
  --role <primary|fallback|second-opinion> --tier <A|A+|B> \
  --prompt <prompt file> --scope <file listing the reviewed paths> \
  [--fallback-basis <basename>] [--timeout-sec N] [--reviewer-cmd <exe>] \
  > .loop/harness-<arc>-r<n>.out 2>&1 &
```

Poll for **the artifact**, not for a substring of the log:
`ls .loop/arc/<arc-id>/r00<n>-*.verdict`. That is the point of the change —
the artifact appears atomically (tmp + rename) and only when the round is
genuinely over.

Each round writes a bundle into `.loop/arc/<arc-id>/`: `<base>.log` ·
`<base>.prompt` · `<base>.scope` · `<base>.verdict`, plus `<base>.decision`
for reviewers that have a separate decision channel (codex today; grok and
opus5 rounds deliberately create no `.decision` file and record
`decision-source: transcript`), where
`<base> = r<NNN>-<reviewer>-<role>-<UTC stamp>-<8 nonce hex>`. The stamp has
one-second resolution, so the nonce is what makes the path unique under
simultaneous launches, and each path is reserved with `set -C` (noclobber) so
the create is atomic rather than check-then-write. The reader refuses an
INCOMPLETE bundle, which is how a killed or in-flight run announces itself
instead of letting an older GO stand.

`MLFK_ARC_DIR` overrides the root; it must be repo-relative. **In a lane
worktree the harness prints a §12.3(5) notice**: the arc directory is
self-contained and uses only repo-relative paths, so copying it into the main
tree's `.loop/` preserves every reference — but somebody has to do it before
the worktree is pruned, and re-running `arc-report.sh` there re-validates
everything including scope currency against the merged bytes.

`--reviewer-cmd` runs an executable instead of the built-in reviewer (used by
the teeth; argv is `<archived prompt> <decision-channel path>`). It is not a
back door: the artifact records a different `reviewer-cmd-sha256`, and the
reader REFUSES such an artifact unless it is run with `--synthetic-ok`.

Roles: `primary` is codex (PROCESS §3 Tier A). `fallback` is a member of the
§11 codex-failure pair — PROCESS §11 needs BOTH `grok` and `opus5`, each with
`role=fallback`, each naming the same `--fallback-basis`; the reader reports
whether that pair is complete and refuses an artifact whose basis is
incoherent, but it does not rule on what the pair licenses (§5).
`second-opinion` is the Tier A+ independent second reviewer; it is not a
review round on its own and is never a substitute for a member of the
fallback pair.

**The reviewer runs on the ARCHIVED prompt envelope, never on the caller's
mutable file.** The envelope is the caller's prompt plus a harness-generated
`## REVIEWED SCOPE` section carrying the full scope manifest and its digest,
so the prompt the reviewer saw and the scope the artifact binds cannot drift
apart — the reader re-greps the archived envelope for the recorded
`scope-sha256` — POSITIONALLY: the digest is the envelope's LAST line, so a
prompt that merely QUOTES an older scope block cannot satisfy the check, and
the arc/round/tier line is matched with `grep -xF` so a legal `.` in an arc id
cannot act as a regex wildcard. The envelope deliberately does NOT name the
reviewer or role, because §11 requires the fallback pair to see the same
prompt. The reader enforces prompt equality where it is a coherence
question — a `fallback` artifact against the codex round it names as its
basis — and otherwise reports `prompt-sha256` rather than adjudicating it. A
Tier A+ second opinion is deliberately exempt: §3 binds it by independence,
not by prompt identity.

Directory membership is EXACT, not suffix-shaped: every entry must be a
`r<NNN>-…` round file, a `cap-…` file or a `reg-…` file of the right family,
enumerated with `ls -A` so dotfiles and `..`-prefixed names are seen. An
orphan `foo.cmd` or a `cap-*.prompt` is a refusal, not ignored debris.

The round's bundle base is
`r<NNN>-<reviewer>-<role>-<UTC stamp>-<8 nonce hex>`, and every one of those
five components is RE-DERIVED by the reader from the record it names: the
stamp and `started-utc` are drawn from ONE clock reading, and the stem is the
first 8 hex of `eor-nonce`. So the enumeration the reader already performs is
evidence about round order, and a record cannot be re-labelled in place
without its path disagreeing with it.

`--timeout-sec` (default 5400) is RECORDED in the artifact, together with
`harness-kill` — whether the harness's own deadline loop signalled the
reviewer. RECORDING them is the whole of what this buys: a caller can still
shorten a deadline or kill a reviewer, and no mechanical rule here decides
whether the result is "a codex round PROVEN failed" (§5.3 — the rule that
tried covered two of eight shapes and was bypassable in zero seconds).
Both values are printed in the report for the driver who makes that call.

## 2. Three independent defences against verdict laundering

**(a) The decision channel.** codex is invoked with `--output-last-message`,
a file only the reviewer's FINAL MESSAGE can reach. Tool output, quoted
transcripts and mid-stream text cannot write it. The reader REQUIRES
`decision-source: last-message` for every codex round, re-extracts the verdict
from that file, and refuses if it disagrees with the artifact. The harness
additionally VOIDs (`decision-mismatch`) any round where the transcript's
terminal verdict and the final message disagree — which is exactly the shape a
foreign or tool-emitted verdict produces. Reviewers without such a channel
record `decision-source: transcript`; the reader will not accept that for codex.

**The decision file is never repaired or rewritten** — not even to add a
missing trailing newline. Those are the reviewer's bytes, and "fixing" a
truncated write could turn a partial `VERDICT: G` into a valid line. The rule
is instead that the channel's LAST line (including a final line with no
trailing newline) must be EXACTLY `VERDICT: GO` or `VERDICT: NO-GO`. That
admits no content after the verdict and catches truncation, with the harness
touching nothing; both harness and reader apply it identically, and a violation
is `void-reason: decision-malformed`.

**(b) The nonce terminator.** The harness draws a 16-byte nonce at launch and,
when the reviewer exits, appends exactly two lines to the transcript.
**Terminator lines are counted NONCE-SCOPED, never by the bare token** — round
5 of this item's own arc VOIDed because the reviewer quoted the literal line
`HARNESS-EOR nonce` out of this file's code block below. A fixed token is
collidable by anyone who reads the docs; the nonce is precisely what a
reviewer, and a pasted foreign transcript, cannot know:

```
REVIEWER_RC <nonce> <rc>
HARNESS-EOR <nonce>
```

The transcript verdict is read ONLY from the region **before** `REVIEWER_RC`,
by the anchored full-line rule PROCESS §3/C11 fixes: the LAST line matching
`^VERDICT: GO$` or `^VERDICT: NO-GO$`. The reader then requires `HARNESS-EOR
<nonce>` to be the log's **final** line and the log's sha256 to match.

So a transcript pasted into a finished log is (a) outside the verdict region,
(b) after the terminator, and (c) a hash mismatch — three independent
refusals, on top of the decision channel. A reviewer cannot forge a terminator
either, because it never sees the nonce; if its own output somehow contains a
nonce-matching terminator line, the harness sees two and writes
`verdict: VOID`, `void-reason: duplicate-terminator`. A void round is loud and
re-run, never interpreted.

Two rules make "the last line" mean what it says, and they apply to EVERY
artifact and EVERY log — round transcripts, decision channels and regression
logs alike. First: the file must END WITH A NEWLINE — `wc -l` counts newlines,
so bytes appended after the final one are invisible to a line count — and the
last line is compared with `tail -n 1` rather than by index. Second: the
terminator pair, its nonce and its rc are verified for EVERY artifact, VOID
ones included, with the rc taken from the LOG MARKER rather than the
artifact's own field; otherwise a forged `reviewer-failed` VOID would be
accepted and could then license a §11 fallback. A `duplicate-terminator` VOID
relaxes ONLY the count-equals-one rule — never the pair's presence, since a
log with zero nonce-matching markers is not a duplicate but a fabrication.

Sidecar paths are DERIVED from each artifact's own basename rather than merely
checked for containment: `<base>.verdict` may name only `<base>.log`,
`<base>.prompt`, `<base>.scope` and `<base>.decision`, and the cap and
regression records likewise. Otherwise two bundles could be cross-wired —
round 7's artifact pointing at round 2's log — with every hash still agreeing.

**(c) The ambiguity rule (added 2026-07-31).** Defence (a) exists only for
codex, and defence (b) closes only a paste AFTER the run. That left failure
mode #1 — the reason this whole item exists — OPEN on the two paths that are
by definition never codex: PROCESS §11's fallback pair and the Tier A+ second
opinion. MEASURED: a reviewer that emits its real `NO-GO` and then QUOTES a
foreign log whose last line is an anchored GO is INSIDE the verdict region,
BEFORE the terminator, and hashes correctly — it decided GO, and closed an
arc. The tell was already being measured and written into every artifact
(`anchored-go` and `anchored-nogo`) and then never used for judgement.

So: for any round with `decision-source: transcript`, a region carrying BOTH
an anchored GO and an anchored NO-GO does not have "a terminal verdict"; it
has two candidates and a positional tie-break. The producer records
`verdict: VOID`, `void-reason: ambiguous-verdict`, and the reader re-derives
the same thing from the log bytes and refuses a non-VOID artifact of that
shape. A void round is loud and re-run, never interpreted.

Deliberately NOT applied to `last-message` rounds: a codex round is decided
by a channel no quoted text can reach, and real codex reviews DO quote
opposing verdicts — 2 of the 7 codex rounds of this item's own arc carry an
in-region anchored GO alongside their NO-GOs while being honest NO-GOs.
Voiding those would discard real reviews to close a hole already closed for
them.

**THE OVERCLAIM THIS PARAGRAPH USED TO END WITH IS RETRACTED.** It said the
ambiguity rule "is what stands between the §11 path and a laundered GO". It
is not, and a reader must not act on that sentence. Both the producer and the
reader fire only when the region carries BOTH an anchored GO and an anchored
NO-GO. MEASURED 2026-07-31: a transcript-decided reviewer that ran out of
context, **emitted no verdict of its own**, and was followed by a quoted
foreign block ending in an anchored GO is recorded `verdict: GO`,
`anchored-go: 1`, `anchored-nogo: 0` — no ambiguity, no VOID. Worse, the rule
makes DELETING the honest verdict the winning move: keeping it VOIDs the
round, dropping it lets the foreign GO stand. `void-reason: no-verdict` is
likewise unreachable whenever any anchored GO appears anywhere in the region.
The rule closes the shape it was built for (verified) and leaves the class.
The real fix is unchanged and undone: give grok and opus5 real decision
channels — `reviewers.sh`'s descriptor comment is where that flag would land.
Until then, a transcript-decided round's verdict is a REPORTED FIELD, not a
proven one, and PROCESS §3/C11's "end with the verdict line" is a discipline
a human checks by reading the log.

## 3. RVERDICT2 — exactly 37 lines

Line 1 is the magic; lines 2..35 are `key: value` in this EXACT order; line 36
is `END RVERDICT2`; line 37 is the seal. No blank lines, no CR, no NULs.

**THE MAGIC IS THE VERSION, and the line count is stated PER VERSION.**
RVERDICT1 shipped a 29-field grammar and then a 32-field one under one magic,
and the reader reported the skew as `expected 35 lines, measured 31 (truncated
or extended)` — i.e. it called its own older records CORRUPT, and the single
arc this item was built to answer became unanswerable by its own tool. THE
RULE: any change to the field list bumps the magic IN THE SAME EDIT, and
`read_artifact` refuses a known-family older magic with its own distinct
wording ("was written by an older format …") so version skew is never again
confused with damage. Older records are not migrated — the round is re-run.

| # | key | grammar |
|---|---|---|
| 1 | *(magic)* | `RVERDICT2` |
| 2 | `arc-id` | `[a-z0-9][a-z0-9._-]{0,63}` |
| 3 | `round` | 1..999 — a caller LABEL; see §5 (round ordering) and §7 |
| 4 | `reviewer` | `codex` \| `grok` \| `opus5` |
| 5 | `role` | `primary` \| `fallback` \| `second-opinion` |
| 6 | `tier` | `A` \| `A+` \| `B` |
| 7 | `closure-rule` | `process3-tier-a-go` \| `process11-fallback-dual-go` \| `process3-tier-aplus-second` (DERIVED from role) |
| 8 | `fallback-basis` | basename of the failed codex `.verdict` this §11 round rests on, else `-` |
| 9 | `reviewer-cmd-sha256` | sha256 of the reviewer descriptor (`reviewers.sh`) |
| 10 | `scope-count` | files in the scope manifest |
| 11 | `scope-sha256` | sha256 of the scope manifest |
| 12 | `scope-manifest` | repo-relative path, inside the arc directory |
| 13 | `prompt-sha256` | sha256 of the ARCHIVED prompt envelope that was executed |
| 14 | `prompt-path` | repo-relative path, inside the arc directory |
| 15 | `log-path` | repo-relative path, inside the arc directory |
| 16 | `log-sha256` | sha256 of the transcript AS WRITTEN (terminator included) |
| 17 | `log-bytes` | measured |
| 18 | `log-lines` | measured |
| 19 | `log-nul-count` | measured (a non-zero value forces VOID) |
| 20 | `decision-source` | `last-message` \| `transcript` |
| 21 | `decision-path` | the decision channel (its own file, or the log) |
| 22 | `decision-sha256` | sha256 of the decision channel |
| 23 | `eor-nonce` | 32 hex chars |
| 24 | `reviewer-rc` | 0..999 as exited |
| 25 | `rc-line` | line number of `REVIEWER_RC` (0 when VOID) |
| 26 | `verdict` | `GO` \| `NO-GO` \| `VOID` |
| 27 | `verdict-line` | line of the terminal in-region verdict (0 when VOID) |
| 28 | `anchored-go` | in-region count of `^VERDICT: GO$` (0 when VOID) |
| 29 | `anchored-nogo` | in-region count of `^VERDICT: NO-GO$` (0 when VOID) |
| 30 | `void-reason` | `-` \| `reviewer-failed` \| `nul-bytes` \| `no-verdict` \| `timeout` \| `duplicate-terminator` \| `decision-empty` \| `decision-malformed` \| `decision-mismatch` \| `ambiguous-verdict` |
| 31 | `timeout-sec` | the deadline this round ran under (1..999999; default 5400) |
| 32 | `harness-kill` | `none` \| `harness-timeout` — did the harness's own deadline loop signal the reviewer |
| 33 | `harness-sha256` | sha256 of the producing `review-harness.sh` (provenance) |
| 34 | `started-utc` | `YYYY-MM-DDTHH:MM:SSZ` — the SAME clock reading the path stamp carries |
| 35 | `ended-utc` | `YYYY-MM-DDTHH:MM:SSZ` |
| 36 | *(terminator)* | `END RVERDICT2` |
| 37 | `artifact-sha256` | sha256 of lines 1..36 |

Fields 2, 3, 4, 5 and 23 are additionally bound to the artifact's own PATH
(`r<NNN>-<reviewer>-<role>-<stamp>-<nonce8>`), and field 34 is the stamp in
that path: the reader re-derives all of them from the name it enumerates.
`void-reason: timeout` further requires `harness-kill: harness-timeout` and
`ended-utc − started-utc >= timeout-sec` — a round cannot claim to have hit a
deadline it never reached.

The scope manifest is `<sha256>  <path>` per file, sorted byte-wise by path,
no duplicate paths, no symlinks, no `..`. **The manifest IS the reviewed
scope** — the reader re-hashes every listed path in the CURRENT tree and
REPORTS, per distinct reviewed byte-set, whether those bytes are still on
disk or have DRIFTED (PROCESS §12(4): a GO covers BYTES, not intent — a
drifted scope means the artifacts describe something else now, and what that
costs is the driver's call, §5.2).

## 4. RVCAP1 — the PROCESS §3 cap record (14 lines)

`RVCAP1` · `arc-id` · `tier` · `closure-rule: process3-capped` ·
`rounds-completed` · `authorized-by` · `recurring-class` (24..400 printable
chars — §3 requires a cap to NAME its class) · `scope-count` ·
`scope-sha256` · `scope-manifest` · `harness-sha256` · `written-utc` ·
`END RVCAP1` · `artifact-sha256`.

`rounds-completed` is the cap's CLAIM about how much work was done. The
reader prints it beside the number of COUNTABLE non-VOID rounds it measured
on disk (§5.2), so a cap claiming work that was never done is visible in one
line — it is not adjudicated here, and neither is the fact that a cap does
NOT discharge a Tier A+ tier-up (the live precedent is the menus arc, capped
at round 20 and still owing its independent second review).

## 4b. RVREG1 — the Tier A+ byte-identity regression (19 lines)

PROCESS §3's Tier A+ tier-up is TWO obligations: an independent second
reviewer AND a byte-identity regression on archived results. The second used
to be unrepresented here, which meant an A+ arc could be declared closed with
half the tier-up unrecorded. It is representable by the same trick as a review
round — the harness RUNS the lane's regression command and seals the result:

```
bash port/review/review-harness.sh regression \
  --arc <id> --tier A+ --scope <file> --cmd <executable>
```

`RVREG1` · `arc-id` · `tier` · `scope-count` · `scope-sha256` ·
`scope-manifest` · `cmd-sha256` · `cmd-path` · `log-path` · `log-sha256` ·
`log-bytes` · `log-lines` · `log-nul-count` · `eor-nonce` · `rc` ·
`harness-sha256` · `run-utc` · `END RVREG1` · `artifact-sha256`.

The bundle is `<base>.reg` + `.scope` + `.log` + `.cmd`. **The command bytes
are ARCHIVED AND EXECUTED**, not merely hashed: a `cmd-sha256` naming a file
that has since moved or changed is a claim, not evidence, so the harness
copies the executable into the bundle, RUNS THE COPY (running the caller's
mutable path would certify bytes that may never have executed), and the reader
re-hashes THAT. The regression log
carries the same nonce terminator as a review transcript and is re-measured
the same way.

PROCESS §3's tier-up needs EXACTLY ONE such record with `rc: 0` over the
bytes the arc reviewed. **Whether an arc has it is now REPORTED, not
adjudicated** (§5.2): the reader validates every `.reg` present to the same
standard as a round (grammar, seal, sidecar derivation, NULs, terminator,
archived command re-hashed, log re-measured) and REFUSES a malformed one —
then states, as observations, how many records exist, each one's `rc`, and
which rounds' scope bytes each ran over. A regression that ran over other
bytes, or a nonzero rc, is named in the report; the conclusion is the
driver's. **Outside Tier A+ the record is still refused BY THE PRODUCER**
(`--tier` must be `A+`), and a `.reg` sitting in a tier-A or tier-B arc is
reported as an observation so it cannot sit unexamined.

## 5. Reading the artifacts (the DIAGNOSTIC — it decides nothing)

```
bash port/review/arc-report.sh --arc <id> [--tier A|A+|B] [--synthetic-ok]
  ARC REPORT <id> (DIAGNOSTIC …) …observations…   exit 0  a report was produced
  ARC REPORT <id>: EVIDENCE DEFECT — <reason>     exit 1  bytes unreadable as evidence
  ARC REPORT REFUSED: <reason>                    exit 2  cannot even start
```

**Owner ruling 2026-07-31: arc closure is a DRIVER/HUMAN judgement.** This
tool is not an authority and must not be wired into a gate, a `&&` chain, or
a manifest cite as a pass condition. `exit 0` means "a report was produced" —
it is returned for a clean arc and for an arc whose every round is VOID
alike, and the report says which. There is deliberately no output line that
can be quoted as an authorization; the report's own footer says so.

Why the judge that used to live here was withdrawn rather than fixed a fourth
time: three independent adversarial passes (`.loop/review-r4-tierA-opus-…`,
`.loop/review-r4-tierA2-opus-…`, and the arc's own round 7) each produced a
green `ARC CLOSED` line over a review that had not happened, each time
through a shape the previous fix had not anticipated. The reviewers'
through-line — *every fix closed the measured instance and left the class* —
is the finding, not any one hole. A judge with known false-GREEN paths is
worse than no judge precisely because the process had declared it the
sanctioned answer.

### 5.1 What the report REFUSES (evidence defects — fail-closed, unchanged)

A defect means **"these bytes cannot be read as evidence"**, never "this arc
is not closed". Nothing in this layer rules on closure, and every accept in
it is the end of a chain of exact, anchored, full-line matches over
re-derived values:

* artifact grammar, exact line count, magic/version, `END` line and self-seal;
* trailing-newline and `tail -n 1` rules on every artifact and every log;
* NULs and CRs, with producer/reader NUL accounting compared for equality;
* the log's sha256, byte count, line count and nonce-scoped terminator pair,
  with the rc read from the LOG MARKER rather than the artifact's own field;
* sidecar paths DERIVED from each artifact's own basename (`<base>.verdict`
  may name only `<base>.log`/`.prompt`/`.scope`/`.decision`), so two bundles
  cannot be cross-wired with every hash still agreeing;
* the path binding: round, reviewer, role, `started-utc` stamp and the 8-hex
  nonce stem in the filename must all equal the recorded fields, so nothing
  can be re-labelled in place;
* arc-id and tier agreement across the arc; the archived prompt envelope must
  carry this artifact's arc-id/round/tier and end with the recorded
  `scope-sha256`, re-grepped from the bytes the reviewer actually read;
* the VOID reason, re-derived by **replaying the producer's precedence chain
  in the harness's own order** — rc → log NULs → terminator count →
  transcript verdict → decision-empty → decision NULs → decision-malformed →
  decision-mismatch → ambiguous-verdict — and compared for EQUALITY. Per-reason
  spot checks were not enough: they accepted overlapping witnesses and so
  could confirm a MISLABELLED reason. Only `timeout` has no independent
  witness (§7);
* a VOID's zeroed derived fields; a non-VOID artifact with a nonzero rc; a
  `harness-kill` that contradicts its void-reason; a claimed `timeout` whose
  elapsed time never reached its own recorded deadline;
* the in-region anchored GO/NO-GO counts, re-measured from the log;
* the scope manifest's pinned grammar (`^[0-9a-f]{64}  <path>$`, byte-wise
  sorted, unique, no `..`), because it is decision-bearing parsed text;
* **arc identity**: every round of one arc reviewed one scope PATH SET, and
  every artifact of one round reviewed one set of scope BYTES. This is what
  the 19 `x-*` cross-artifact rows lacked;
* **referential integrity of `fallback-basis`**: it must name an artifact IN
  THIS ARC that is `codex`, `primary`, of the same round, VOID, and on the
  same prompt and scope bytes. An artifact whose basis is absent or
  contradictory is an incoherent record;
* one non-VOID artifact per (round, reviewer) — two disagreeing records for
  one round is the two-writer class and must not be arbitrated by whichever
  the reader happened to open;
* the arc directory's membership whitelist (`ls -A`, exact family matching)
  and complete bundles only — a `.log` with no `.verdict` is a killed or
  in-flight run;
* `reviewer-cmd-sha256` must be the built-in invocation unless
  `--synthetic-ok` is passed;
* structural validation of RVCAP1 and RVREG1 records to the same standard,
  including the archived regression command's own sha256.

### 5.2 What the report OBSERVES (facts, with no conclusion drawn)

Printed for every arc that survives 5.1: per artifact — round, reviewer,
role, verdict, void-reason, `reviewer-rc`, `timeout-sec`, `harness-kill`,
`started-utc`, `ended-utc`, decision source, in-region GO/NO-GO counts, scope
sha and `fallback-basis`. Then, as named observations:

* scope currency per distinct reviewed byte-set: current, DRIFTED (with a
  file count), or a scope path missing from the tree;
* per round: whether a §11 fallback pair is COMPLETE or INCOMPLETE, whether a
  round carries both a primary and fallback artifacts, and whether the round
  is countable at all (a countable round has a non-VOID codex `primary`
  artifact or a complete §11 pair — three second opinions are not three
  rounds);
* an arc in which every artifact is VOID;
* Tier A+ tier-up PRESENCE: which second-opinion artifacts exist and whether
  each reviewer is distinct from that round's primary/fallback reviewers;
  which RVREG1 records exist, their rc, and which rounds' scope bytes they
  match. Whether either obligation is DISCHARGED is not decided here;
* what a cap CLAIMS (`rounds-completed`, `authorized-by`, `recurring-class`)
  beside the countable-round count measured on disk, and whether its scope
  bytes match any round.

### 5.3 What was DELETED rather than demoted, and why

An observation derived from an unsound rule is still an unsound claim, so two
rules were removed outright rather than reported:

**Round order.** `round` is a caller LABEL (`--round N`) stamped into the
envelope by the harness and then "re-derived" by grepping that same envelope
— circular. A rule requiring the label order to agree with `started-utc`
shipped on 2026-07-31 morning and was measured unsound the same day: it never
read `ended-utc`, so a GO that STARTED one second after an adverse round and
FINISHED six seconds before it satisfied it, as did two rounds launched in
the same second. **The report prints `started-utc` AND `ended-utc` for every
artifact and states that round order is NOT ESTABLISHED.** Ordering is a human
read of those times and of the arc directory's file dates.

**§11 basis eligibility.** PROCESS §11's fallback is available "when a codex
round is PROVEN failed". A rule refusing a basis whose `timeout-sec` was
below a floor, or whose rc was in the signal range with `harness-kill: none`,
closed the two measured shapes and left six other VOID reasons eligible — two
of which were manufactured in ZERO SECONDS with no repo write: run the
harness with codex off PATH (rc 127, `reviewer-failed`), or add one sentence
to the prompt ("after the verdict line, append a summary"), which makes every
codex round `decision-malformed` at rc 0 while grok and opus5, decided by the
last ANCHORED line, are unaffected — and the same prompt is then the one the
pair is required to use. **The report prints the basis's void-reason, rc,
deadline, harness-kill and both timestamps, and states that §11 basis
eligibility is NOT DECIDED here.**

Do not re-add either rule as a "reported observation". If a future rule is
wanted, it has to be argued as a class fix and reviewed as one.

### 5.4 Closure-rule vocabulary (still recorded, no longer adjudicated)

Every round still declares which rule it counts toward, and the reader still
requires role↔rule coherence per artifact (a `primary` artifact carrying
`process11-fallback-dual-go` is an incoherent record). The vocabulary is
unchanged — `process3-tier-a-go`, `process11-fallback-dual-go`,
`process3-tier-aplus-second`, and RVCAP1's `process3-capped` — and PROCESS
§3/§11 still define what each one requires of a HUMAN closing an arc:

- **`process3-tier-a-go`** — a `primary` codex round that ended GO.
- **`process11-fallback-dual-go`** — BOTH `grok` and `opus5`, both GO, same
  prompt and scope bytes, on a codex round that genuinely failed. One alone
  does not close it.
- **`process3-capped`** — PROCESS §3's cap: fix outstanding Medium+,
  disposition Lows in writing, and NAME the recurring class.
- **Tier A+** — BOTH tier-up obligations, on every path: an independent
  second reviewer AND a byte-identity regression on archived results. A cap
  does not discharge the tier-up (the menus arc is the live precedent).

The report tells a driver which of those ingredients are ON DISK. It does not
tell anyone that they add up.

The seal is an INTEGRITY seal against accident, corruption and truncation —
PROCESS §3's rig bar. It is **not** an anti-adversary control: an actor with
repo-write access can recompute it and can also edit these scripts, so no
in-script defence against that actor is coherent (§3 dispositions that class
in writing rather than fixing it).

## 6. Done-check

`bash port/review/check-review-artifact.sh` → `REVIEW ARTIFACT TEETH OK
(N/N bit)`, exit 0. Named teeth plus their restore controls; each tooth is ONE
perturbation from a green baseline and is followed by a re-assert that the
baseline is green again.

**RE-POINTED 2026-07-31 with the demotion.** The teeth used to assert the
JUDGE'S AUTHORITY ("this cannot be talked into closed"). They now assert the
DIAGNOSTIC'S BEHAVIOUR, in three kinds:

| assertion | what it requires |
|---|---|
| `expect_report` | the baseline evidence is readable: a report is produced (rc 0), anchored, self-labelled `(DIAGNOSTIC`, and carrying the `NOT A CLOSURE DECISION.` footer |
| `expect_defect` | the perturbed bytes are refused with a nonzero exit **and the exact intended reason** — fail-closed, unchanged |
| `expect_observed` | the report DISCLOSES a named fact. Used wherever a closure rule was deleted: the rule is gone, but the situation it refused on must stay visible to the human who now makes that call. A tooth satisfiable by silence would be no tooth. |

No tooth was deleted in the demotion. Every tooth whose subject was a closure
RULE was converted to `expect_observed` over the same situation — a §11 pair
with one reviewer, a Tier A+ arc with no second opinion or no regression, a
cap claiming more rounds than exist, a drifted or missing scope, a failed
regression, a stray regression in a tier-A arc, an arc in which every round is
VOID, a round mixing a primary GO with a §11 fallback. The two teeth whose
subject was a rule DELETED AS UNSOUND (T89 round order, T94/T96 basis
eligibility) each became two: one requiring the tool's own "NOT ESTABLISHED" /
"NOT DECIDED here" statement, one requiring the raw evidence (both timestamps
of both rounds; the basis's deadline, rc and `harness-kill`) to be on the page.

**WHAT THE OFF CONTROLS DO AND DO NOT ESTABLISH.** An earlier version of this
section said "eighty-nine named teeth, each proven to bite". That is the
property this section exists to DISPROVE, and it did not hold: an independent
Tier A+ reviewer applied ten fresh single-check controls of its own and NINE
left the suite green, two of them demonstrably fail-open. The controls below
are a MEASURED SAMPLE with a stated bound, not a coverage claim:

> **BOUND.** The tables cover 62 named single-check weakenings of the scripts
> AS THEY STOOD ON 2026-07-31 MORNING. `arc-report.sh` and `review-harness.sh`
> contain more decision points than that. A check with no row here has NOT
> been measured, and the honest reading of an unmeasured check is "unknown",
> not "covered".
>
> **SECOND BOUND, added with the demotion (the second reviewer's M2).** Every
> control in both passes DELETES A SHIPPED CHECK, so the method can only find
> teeth missing for checks that EXIST. It says NOTHING about the completeness
> of the check set. All five HIGHs across the two adversarial reviews were
> checks that had never been written — an eligibility whitelist, a last-line
> rule, a quiescence barrier — and no OFF control could have surfaced any of
> them. The completeness of the check set has only ever been established by
> adversarial review. Do not read "62 controls" as an assurance number.
>
> **THIRD BOUND: these tables were measured against the JUDGE that has since
> been WITHDRAWN.** Rows naming a closure rule (`no-round-monotonic`,
> `no-basis-eligibility`, `no-cap-round-count`, `no-aplus-*`, `no-mixed-rule`,
> `no-stray-reg`, `no-cap-countable`, `no-scope-currency`, `no-fallback-anyround`)
> describe checks that no longer exist as REFUSALS; their teeth were
> re-pointed to `expect_observed` and are listed above. The rows are kept as
> measured history, not as a claim about the shipped bytes. Rows in the
> evidence-defect layer (seals, hashes, NULs, terminators, path binding,
> sidecar derivation, VOID-reason derivation, grammar, arc identity, basis
> referential integrity) still describe shipped refusals.

The two fixtures under `specimens/` are the real artifacts of the two failure
modes, copied out of `.loop/` so they outlive it, sha256-pinned in
`specimens/SHA256SUMS`, and additionally pinned structurally by the check
(line counts, verdict line numbers, byte count, NUL count, and the absence of
harness terminators) so they cannot rot into vacuity:

- `foreign-verdict-at-eof.log` = `.loop/review-c25-1-codex-VOID.log`. Its real
  terminal verdict is `VERDICT: NO-GO` at line 4063 (`CODEX_RC=0` at 4064);
  the `VERDICT: GO` at 4094 belongs to a QUOTED FOREIGN log pasted after the
  run. Tooth T12 refuses it as-is; T13 refuses it with a harness terminator
  spliced in at the true end of the run and an artifact whose hashes, counts
  and seal are all internally perfect — the refusal there is structural, not a
  broken hash. T14 shows the decision channel catching the same shape
  independently.
- `nul-corrupt-rc1.log` = `.loop/mexit-r4-device-target.log`. 7,835 of 17,627
  bytes are NUL (44.4%); it contains `DEVICE TARGET CONFORMS` while the run
  exited `TARGET_RC=1`. Teeth T16 and T17 refuse it both when the artifact
  lies about the NUL count and when it declares it honestly; T18/T19 refuse
  the nonzero-rc shape on its own.

**Measured OFF controls, first pass (2026-07-30/31; 39 applied, 39 bit).**
Single-check weakenings of `arc-closure.sh` (as `arc-report.sh` was then named) were applied ONE AT A TIME; each
turned the suite RED and was attributed to the intended tooth, and the file
was byte-restored afterwards with its sha256 re-verified every time. READ
THIS TABLE WITH THE BOUND ABOVE: 39 controls that bit is a statement about
those 39 lines, and an independent reviewer's ten FRESH controls then found
two fail-opens outside them.

| weakened check | tooth that went red | shape |
|---|---|---|
| `no-bundle-check` | T39 | FAIL OPEN |
| `no-prompt-scope-grep` | T31 | FAIL OPEN |
| `no-prompt-identity` | T66 | FAIL OPEN |
| `no-decision-agree` | T15 | FAIL OPEN |
| `no-duplicate-check` | T40, T41, T42 | FAIL OPEN |
| `no-basis-role` | T53 | FAIL OPEN |
| `no-basis-void` | T62 | FAIL OPEN |
| `no-scope-currency` | T29 | FAIL OPEN |
| `no-reg-scope-bind` | T59 | FAIL OPEN |
| `no-reg-rc-check` | T58 | FAIL OPEN |
| `no-cap-scope-bytes` | T50 | FAIL OPEN |
| `no-cap-round-count` | T19, T34 | FAIL OPEN |
| `no-aplus-cap-second` | T28 | FAIL OPEN |
| `no-aplus-cap-regression` | T60 | FAIL OPEN |
| `no-aplus-go` | T27 | FAIL OPEN |
| `no-aplus-go-regression` | T57, T58, T59 | FAIL OPEN |
| `no-reg-cmd-bind` | T73 | FAIL OPEN |
| `no-stray-reg` | T69 | FAIL OPEN |
| `no-dir-membership` | T38, T79 | FAIL OPEN |
| `no-magic-check` | T83 | FAIL OPEN |
| `no-self-seal-check` | T07 | caught by the tier-agreement layer |
| `no-manifest-grammar` | T63, T64, T65, T74 | caught by the sha/count layer |
| `no-reg-nul` | T71 | caught by the count-agreement layer |
| `no-reg-eor` | T72 | caught by the log-sha layer |
| `no-cr-check` | T78 | caught by the self-seal layer |
| `no-art-newline` | T75 | caught by the self-seal layer |
| `no-log-newline` | T77 | caught by the terminator layer |
| `no-void-rcmarker` | T80 | caught by the void-reason derivation |
| `no-sidecar-derive` | T86 | caught by the log-sha layer |
| `no-reg-log-newline` | T84 | caught by the terminator layer |
| `no-log-sha-check` | T10 | caught by the byte-count layer |
| `no-nul-agreement` | T16 | caught by the NUL-zero layer |
| `no-voidreason-derive` | T67 | caught by the zeroed-fields layer |
| `no-rc-count-check` | T12 | caught by the EOR-count layer |
| `no-eor-last-check` | T11, T13 | caught by the rc-line layer |
| `no-scope-bytes-agree` | T25 | caught by the prompt-agreement layer |
| `no-prompt-agree` | T26 | caught by the pair-agreement layer |
| `no-fallback-anyround` | T21, T22, T62 | partly FAIL OPEN (T62) |
| `no-cap-countable` | T49 | caught by the round-count layer |

Twenty weakenings FAIL OPEN outright. The rest are caught by a second,
independent layer — and the teeth still detect the weakening, because
`expect_refused` demands the EXACT refusal reason rather than merely a nonzero
exit. That is the property that keeps defence-in-depth from hiding a deleted
check.

**T56 exists because a control did NOT bite.** In an earlier pass,
`no-basis-prompt` left the suite green — the fallback-basis prompt check had
no tooth at all. The gap was closed by adding T56 and re-running the control,
which then failed open as it should. That is the point of running the controls
rather than asserting that the teeth are sufficient.

**Measured OFF controls, second pass (2026-07-31; 23 applied, 22 bit, 1
FAIL-OPEN found and closed).** Run after the Tier A+ independent review, in
23 throwaway git repos each seeded with a byte-copy of `port/review/`, one
check deleted per repo, nothing else touched. It covers every check added in
that response PLUS the ones the reviewer's own sweep found untoothed. Each
RED row was attributed by name: `expect_refused` demands the EXACT refusal
string, so a RED is the intended tooth firing, not merely a nonzero exit.

| weakened check | script | tooth that went red | shape |
|---|---|---|---|
| `no-round-monotonic` | arc-closure | T89 | **FAIL OPEN** (H1) |
| `no-path-prefix` | arc-closure | T91 | FAIL OPEN |
| `no-path-stamp` | arc-closure | T90 | FAIL OPEN |
| `no-path-nonce` | arc-closure | *(none — see below)* | **GREEN: no tooth** |
| `no-ambiguity-producer` | review-harness | T92 | **FAIL OPEN** (H2) |
| `no-ambiguity-judge` | arc-closure | T93 | **FAIL OPEN** (H2) |
| `no-basis-eligibility` | arc-closure | T94, T96 | **FAIL OPEN** (H3) |
| `no-timeout-floor` | arc-closure | T95 | FAIL OPEN |
| `no-hkill-coherence` | arc-closure | T97 | FAIL OPEN |
| `no-rule-coherence-primary` | arc-closure | T98 | **FAIL OPEN** (M1) |
| `no-rule-coherence-fallback` | arc-closure | T99 | FAIL OPEN |
| `no-rule-coherence-second` | arc-closure | T100 | FAIL OPEN |
| `no-mixed-rule` | arc-closure | T101 | **FAIL OPEN** (M1) |
| `no-inregion-agree` | arc-closure | T102 | FAIL OPEN |
| `no-tier-agreement` | arc-closure | T103 | FAIL OPEN |
| `no-primary-role-reviewer` | arc-closure | T104 | FAIL OPEN |
| `no-scope-count` | arc-closure | T105 | FAIL OPEN |
| `no-decision-nul` | arc-closure | T106 | FAIL OPEN |
| `no-verdict-line-agree` | arc-closure | T107 | FAIL OPEN |
| `no-cap-singleton` | arc-closure | T108 | FAIL OPEN |
| `no-version-discriminator` | arc-closure | T109 | caught by the magic layer, wrong reason |
| `no-path-dotdot-field` | arc-closure | T110 | caught by the sidecar-derivation layer, wrong reason |
| `no-manifest-dotdot` | arc-closure | T87 | caught by the sort layer, wrong reason (the row round 7's [L] asked for) |

**T111 exists because a control did NOT bite — again.** `no-path-nonce` left
the suite green on this pass: the third component of the path binding (the
8-hex nonce stem) had no tooth. T111 was added and the control re-run; it
then failed open as it should. Two passes, two undetected gaps found by
running controls instead of asserting sufficiency. That ratio is the reason
the BOUND above is stated the way it is.

## 7. Honest exposure (what this does NOT do)

- **The exhaustive per-format list of TRUSTED (recorded but not re-derived)
  values.** Everything not listed here is re-derived from the arc's own
  evidence and compared.

  | format | trusted field | why it cannot be verified |
  |---|---|---|
  | all three | `harness-sha256` | provenance; the producing script's bytes may legitimately have moved since |
  | RVERDICT2 | `started-utc`, `ended-utc` | no clock witness in the evidence |
  | RVERDICT2 | `reviewer`, `role` | checked for mutual coherence with `closure-rule`, and the descriptor must match the built-in invocation — but nothing in the evidence proves the process that wrote a log was `grok` rather than `opus5` |
  | RVERDICT2 | `void-reason: timeout` | a killed reviewer also exits nonzero, so it is indistinguishable from `reviewer-failed` |
  | RVERDICT2 | `verdict` (transcript-decided rounds) | it is the region's terminal anchored line, re-derived from the log — but §2(c) and the quiescence bullet below both show bytes reaching that region from something other than the reviewer's decision. A reported field, not a proven decision |
  | RVERDICT2 | `round`, `tier` | re-derived from the archived envelope the reviewer read AND from the artifact's own path — but BOTH are written by the harness from the caller's flag, so the VALUE is a caller label, and **the ORDER it implies is NOT verified either**: the monotonicity rule that used to check it read only `started-utc` and was measured unsound (§5.3), so it was deleted. Round order is a human read of the recorded times. `tier` has no backstop either (see the tier-classification bullet below) |
  | RVERDICT2 | `timeout-sec`, `harness-kill` | caller-influenced parameters, RECORDED rather than invisible — which is the part worth keeping. **They are no longer used to rule on anything**: the §11 basis-eligibility rule that floored them covered 2 of 8 VOID shapes and was measured bypassable in zero seconds (§5.3), so it was deleted and the values are simply printed. The one cross-check that remains is a coherence one: a claimed `timeout` whose elapsed time never reached its own recorded deadline is an incoherent record |
  | RVCAP1 | `authorized-by`, `recurring-class`, `written-utc` | a cap is a human/driver act; the reader enforces only that the claim is well-formed; the round count it asserts is REPORTED beside the measured one, not adjudicated |
  | RVREG1 | `run-utc` | as above |

  `arc-id` is NOT on this list: it is re-derived from the archived prompt
  envelope the reviewer actually read. Neither is RVREG1's `cmd-sha256` — the
  command bytes are archived in the bundle and re-hashed. `started-utc` /
  `ended-utc` remain trusted as CLOCK READINGS (nothing in the evidence
  witnesses a clock) but are no longer free-floating: `started-utc` is bound
  byte-for-byte to the artifact's path stamp, so it cannot be re-labelled in
  place. Nothing mechanical reads them for ORDER any more (§5.3); they are
  printed for the human who does.
- **REVIEWER IDENTITY IS NOT BOUND AGAINST THE WRITER. This is a design
  ceiling, stated rather than implied (measured 2026-08-01,
  `.loop/review-r4split-r1-20260801.log` [HIGH]).** The artifact records a
  `reviewer-cmd-sha256` for the built-in path, and §5.1 checks it — but the
  built-in path executes the bare names `codex`, `grok` and `claude`, so an
  exported shell function, a `BASH_ENV` hook or a `PATH` shadow can run
  something else entirely **while the recorded descriptor hash stays genuine**.
  The teeth cover the explicit `--reviewer-cmd` route only. Therefore:

  > **The guarantee this producer actually offers is tamper-EVIDENCE against
  > accident, drift and sloppiness — NOT against a writer who wants to forge a
  > review.** A writer invokes this harness, and the harness invokes the
  > reviewer, so no arrangement of checks inside it can bind the reviewer's
  > identity against that same writer.

  Closing it needs something structurally outside the writer's reach: a
  driver-owned launcher, or an external signature over the reviewer's output.
  Until one exists, do not cite an artifact as proof that a *particular*
  reviewer ran; cite it as proof that a review was recorded, with its evidence
  re-openable.

- **SCOPE MEMBERSHIP IS A CALLER ASSERTION.** The manifest binds the BYTES of
  the files the caller listed; nothing cross-checks that list against the
  change. MEASURED 2026-07-31: an arc was recorded over a one-file scope; a
  sibling tracked file was then rewritten and a brand-new file added and
  committed; the (then) judge still printed a green closure line, because
  scope currency can only detect drift in paths the manifest names. The
  diagnostic reports the same blind spot the same way — it reports currency
  for the listed paths and knows nothing about any other file. This is the same class as the 19 `x-*`
  rows resting on an unstated scope claim, moved one layer down: the arc
  identity is machine-readable, but WHAT it identifies is still asserted.
  Recording the launch commit plus the tracked-file delta and refusing when a
  changed tracked file is absent from the manifest is the real fix; it is not
  done here, because in this repo the working tree is routinely dirty in ways
  unrelated to any arc and a tool that refuses on those is a tool nobody can
  run. Until it is done, "the exact reviewed scope" means the exact BYTES OF
  THE LISTED PATHS, and reviewing the path list is the driver's job.
- It does not defend against an actor with repo-write access (§3, above).
- **Tier A+'s byte-identity regression is now represented (§4b), but what is
  verified is bounded:** the harness proves that THIS command, over THESE
  scope bytes, exited 0, and seals its output. It does not and cannot know
  whether the command is a real byte-identity regression rather than
  `exit 0` — the `cmd-sha256` records which executable ran, and reviewing that
  executable is the arc's job like any other shipping check.
- **OUTPUT QUIESCENCE IS NOT PROVEN (round 7 [H], OPEN) — AND THE PREVIOUS
  WORDING OF THIS BULLET WAS MEASURED FALSE. CORRECTED 2026-07-31.**

  This bullet used to say: *"What that produces is a REFUSAL, not a false GO —
  the artifact seals the log's sha256 and the judge re-hashes it, so a late
  write invalidates the round — but there is a narrow race in which the judge
  runs inside the window and sees a consistent pair."* **That is false, and
  false in the direction that matters.** The reasoning only holds for writes
  that land AFTER the terminator is appended.

  What actually happens: the harness waits on the reviewer's DIRECT pid with a
  0.25 s poll loop and appends the terminator only after that wait returns. So
  there is a window — bounded below by the poll interval, not by anything the
  harness controls — in which a background descendant that inherited the
  reviewer's stdout writes **INSIDE THE VERDICT REGION**. The harness then
  appends the terminator after those bytes and seals a perfectly
  self-consistent artifact. MEASURED 2026-07-31, 3 trials, 3/3 identical: a
  reviewer that printed prose, **decided nothing**, and left
  `( sleep 0.05; echo <anchored GO verdict line> ) &` behind produced
  `verdict: GO`, `anchored-go: 1`, `void-reason: -`, validated by the reader,
  and closed an arc on the §11 path. The verdict came from a background
  process; no reviewer emitted it.

  Note the interaction with §2(c): the ambiguity rule saves the case where the
  reviewer had an honest NO-GO (both verdicts → VOID, verified), which means
  the false GO is available exactly when the reviewer decided nothing.

  The two shapes the old wording DID describe correctly are real and were
  re-measured: a descendant writing at +1 s, or writing continuously, produces
  a `duplicate-terminator` VOID and/or a refusal on the log sha. The
  disclosure was right about the LATE window and wrong about the EARLY one,
  and the early one is the one that yields a green line.

  Consequence for a reader: **a round's recorded verdict is evidence that
  those bytes were in the region, not proof that the reviewer decided them.**
  The fix (run the reviewer in its own process group, capture through a FIFO,
  publish only after bounded EOF; or, as an interim, hash the log immediately
  before appending the terminator and again after `wait` and VOID on any
  change) is registered, NOT DONE. Same for the regression runner.

  A binding disclosure that understates its own exposure is worse than the bug
  it describes, because §7 is what PROCESS.md points readers at. This
  correction is the reason the whole item was re-opened.
- **`review-harness.sh regression` HAS NO DEADLINE** (both reviews' L, still
  open). `cmd_run` enforces `--timeout-sec`; `cmd_regression` does not, so a
  wedged regression command hangs the harness forever and writes no `.reg`.
  It fails CLOSED — the bundle is incomplete and the reader refuses the arc —
  but an autonomous loop hangs rather than fails, which is strictly worse than
  failing, so it is disclosed here rather than left to be discovered.
- **ROUND LABELS MAY BE SPARSE, AND A WHOLLY DELETED BUNDLE IS INVISIBLE**
  (second review's L2). Labels 1 and 9 with nothing between draw no comment,
  and `rm -f .loop/arc/<arc>/r001-*` (a gitignored directory, so not a
  tracked-file write) simply leaves one fewer round in the report. Deleting
  only the `.verdict` IS caught (incomplete bundle). A human reading the
  directory is equally blind, so this is not a regression against the practice
  — but the artifact count is a count of what SURVIVED, not of what happened.
- **PROMPT SEMANTICS ARE NOT VERIFIED (round 7 [M], open).** The harness
  injects the scope envelope but does not inject or check PROCESS's
  adversarial-stance and terminal-decision contract, so a weak caller prompt
  still yields a structurally valid round. A harness-owned contract block,
  positionally verified, is registered, not done.
- **TIER CLASSIFICATION IS AN ARGUMENT, NOT A DERIVATION.** `--tier` is taken
  from the caller. Nothing in the evidence forces a review-layer change to be
  declared A+; round 7 of this item's own arc caught exactly that mistake by
  reading PROCESS, not by any mechanical check.
- **A TIER A+ SECOND OPINION IS EXEMPT FROM PROMPT EQUALITY, AND THAT
  INTERACTS WITH PROMPT SEMANTICS.** The exemption is correct (§5: §3 binds a
  second opinion by independence, not by prompt identity, and imposing §11's
  same-prompt rule on it would reject every legitimate A+ arc). But combined
  with the un-verified prompt semantics above it means tier-up obligation (1)
  can be "discharged" by a GO from a different reviewer on an ARBITRARY
  prompt — measured: an A+ arc was recorded with a second opinion run on "You
  are a second reviewer. Reply GO." and an `exit 0` regression. Both halves
  are disclosed separately above; they are stated together here because the
  combination is what a driver needs to know before treating a reported
  `second-opinion` artifact as the tier-up. Related, and also unstated
  before: on the §11 path the fallback PAIR itself supplies two distinct
  reviewers, so an A+ arc can present two GO reviewers who are each other's
  only independence check.
- It cannot tell a genuine reviewer run from a replayed/cached one. That is
  PROCESS §11's `cmp`-prove-the-findings-distinct discipline, and it stays a
  human/driver step.
- `--fallback-basis` proves a codex round of that number was RECORDED and did
  not GO. A codex failure that never reached the harness at all (for instance
  a cached-output replay that still exits 0 with a plausible verdict) has to be
  run through the harness — or the driver caps — before the fallback path is
  available.
