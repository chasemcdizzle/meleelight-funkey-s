# ▶ RESUME HERE (written 2026-08-04 for a cold restart — read this first, then §rulings)

**Branch `agent/auto`, tree clean, everything committed. Phase M4.**
Loop entry per `docs/LOOP.md` §B; this block is the short version.

## What is true right now
- **B9 arc: r5's seven findings are FIXED and VERIFIED GREEN ON HARDWARE.**
  `bash port/sim/device/check-device-fullgame.sh` -> `FULLGAME CONFORMS 12/12
  (… p99=16.562ms skips=0/allow12 … teeth=25)` rc 0, evidence
  `.loop/fullgame-b9r5-verify4-20260803T174600.log`. The row is STILL
  `arc-in-flight` — **green bytes are not a closed arc**; a NO-GO round's fixes
  do not close one. A fresh review round is owed and now also owes coverage of
  the frame-conservation repair.
- **The M4 gate cannot pass yet, for THREE independent reasons:**
  1. FIVE `arc-in-flight` manifest rows (`verify_m4.sh` hard-refuses on any):
     `check-device-fullgame.sh`, `check-device-opk.sh`, `verify_m4.sh`,
     `check-assets-expected.js`, `expected-assets.json`.
  2. **The OPK leg is still RED, but for a NEW reason — A15, not the old
     inventory drift.** A13 fixed the drift (device inventory now equals the
     pin, both arms navigate correctly) and that unmasked a defect hidden
     behind it since 2026-07-27: the FOH arm's structural shot judge rejects
     the title shot at 70.151% foreground vs its [0.5%, 60%] band. The frame
     is CORRECT — the band was measured pre-artwork. Evidence
     `.loop/a13-opk-foh.log`.
  3. **`verify_m4.sh` does not even reach its arc check (A16).** It refuses at
     [0] on ROW GRAMMAR: check-device-fullgame.sh's note token
     `skips==frames-...` contains `=`, forbidden by `verify_m4.sh:674`.
     88 of 89 rows pass. So reason 1 is real but is NOT the message you get.
- **p99 headroom is 108 µs** (s01 16.562 vs 16.670 ms budget). Swings ~600 µs
  run to run. The gate can go red on timing alone. Standing risk, not actioned.
- **B11 is DEFERRED** (owner, 2026-08-03) and its cost is live: 3 of 15 judged
  shots pass or fail BY LUCK. **A green FOH leg is not evidence the judgment is
  sound, and a red one is not evidence of a regression** — check any red on
  `f01/css`, `f02/css-cpu`, `f05/css` against the B11 signature FIRST.

## THE QUEUE — TIER 1, in owner order
| # | Item | State |
|---|---|---|
| 1 | A3 | **ALREADY BUILT** (measured 2026-08-03) — skip |
| 2 | A4 | **ALREADY BUILT**; NATURAL default ratified correct — skip |
| 3 | A5 | **ALREADY BUILT** — skip |
| 4 | A7 credits | **BLOCKED** on the owner's `Math.random` call — the ONLY open question |
| 5 | A13 opk title | **DONE 2026-08-04** — three roles, three .desktop files, three titles; play install rebuilt as `meleelight.opk` |
| 6 | D8-later | REDIRECTED to a novel Melee-style letter grid; needs a MENU-SPEC deviation registered first |
| 7 | WJ-later | PRE-REGISTER the checksum-surface change before any code (owner-ratified) |
| 8 | A14 glyph atlas | LARGE, deliberately last |
**TIER 2** = everything else, prior order preserved, B11 first, R8 last.
**JUMPED THE QUEUE (both found by A13, both block the M4 gate):** A15 (P1,
FOH shot band) then A16 (P2, manifest row grammar). Both are small and both
stand between the gate and an honest verdict — do them before D8-later.

## NEXT ACTION (A15 — re-measure the FOH shot band)
The judge's [0.5%, 60%] foreground band was measured at iter 115 against the
PRE-ARTWORK title screen (splash 4.69%, title 2.63%). The A1 restyle landed the
real IMG1 artwork; the title shot now measures 70.151% and the frame is CORRECT
(inspected: "MELEE LIGHT / PRESS START" over the radial-burst background).
1. Re-measure BOTH shots on BOTH arms, repeat runs — this leg shares the B11
   jitter surface, so one sample is not a measurement.
2. Re-freeze the band from what you measured. It must still reject
   blank/garbage/frozen frames: never round the upper bound up for headroom,
   and say in the code what the new number was measured against.
3. `MLFK_OPK_FOH=1 bash port/gfx/check-device-opk.sh` -> `OPK FOH LAUNCH OK`,
   rc 0. Then re-pin the manifest row + `MANIFEST_SHA256`.

## STANDING HAZARDS (learned the hard way this session)
- **Re-verify a queue row against the TREE before starting it.** Three tier-1
  rows described finished work. Two more described work of a different shape.
- **Re-verify the SOLUTION against the tree too, not just the problem** (A13,
  2026-08-04). The ratified "minimal fix, no decisions left" would have
  installed a coin-flip FALSE PASS on the FOH arm, because two .desktop files
  were serving three roles. Three greps inverted the answer.
- **A stale red masks everything downstream of it.** The OPK leg died at the
  inventory pin for a week; A15 sat invisible behind it the whole time. When a
  check has been red a while, fix the FIRST failure and RUN IT AGAIN.
- **Derive from measured output; never transcribe by eye.** A hand-typed tooth
  pin cost a 40-minute device run.
- **`docs/LOOP.md` §A-par:** parallel worktree lanes start at A14, NOT before.
  One device + one riglib lock means verification never parallelizes.
- Only `port/sim/device/` and `port/gfx/opk/` changed. **No C, no game code.**

---

# STATE.md — current truth (driver-updated every turn)

_Read CLAUDE.md first, then this page. History → docs/AGENT-LOG.md;
queue → fix_plan.md; standards → docs/PROCESS.md._

## §rulings (standing owner directives, day-tagged)

- **2026-08-03 — RE-PRIORITIZATION; B11 DEFERRED (Chase, in session).** Verbatim:
  *"let's skip B11 I am totally done with all this for now. defer it until after
  we're done. … A3, A4/A5, A5, A7, A13, D8-later, wj-later are all priority now
  (in that order). move everything to after that (preserve order)."*
  **TIER 1, in order: A3 · A4 · A5 · A7 · A13 · D8-later · WJ-later · A14.**
  (**A14 placed LAST by DRIVER JUDGMENT** under the owner's second delegation —
  *"ok move a14 wherever you want"* — NOT an owner ruling, amendable. It was
  first placed at position 3; RECON REFUTED that rationale. Relayout is
  POSITION-NEUTRAL — A14 re-lays whatever exists when it lands and later screens
  are authored against the new font, so either order costs ONE pass. Risk
  sequencing therefore decides, and it puts A14 last: it is LARGE, it carries a
  live oracle hazard (iter-72 jitter at ~10x against a never-loosenable
  `maxDiffPixels = 16`), and it is cosmetic while every owner-named item is
  functional. Full reasoning: fix_plan.md.)
  (The message listed "A4/A5, A5"; read as A4 then A5, duplicate treated as a
  slip — amend if that was not the intent.) **TIER 2 = everything else with its
  prior order preserved**, B11 first among them, R8 still last.
  **B11 is deferred, NOT cancelled** — the 2026-08-01 three-way byte-exact
  ratification stands, only its position moves. **MECHANICAL CONSEQUENCE, stated
  not papered over:** B11's defect is INTERMITTENT, so `check-device-foh.sh` and
  `verify_m4.sh` leg [2] pass or fail BY LUCK on 3 of 15 shots (f01/css,
  f02/css-cpu, f05/css) until it lands. **A green FOH leg is therefore not
  evidence the judgment is sound, and a red one is not evidence of a
  regression** — check any red on those three against the B11 signature (device
  shot == a hold±1 host twin, byte-exact) BEFORE diagnosing anything else. The
  generator is banked and committed (`1be6abd`). Full detail: fix_plan.md
  §"OWNER RULING 2026-08-03".

- **2026-08-01 — THREE RATIFICATIONS (Chase, in session, after a full written
  briefing of the options and their costs).** Verbatim: *"option A close the arc
  and approve a named skip allowance yes. decision 1: C yes please. decision 2:
  B please."* These three are the authority for the check edits that follow, and
  each edit cites this bullet at the edit site.

  1. **B9 / R7a — CLOSE THE ARC + APPROVE A NAMED, BOUNDED, LOUD SKIP
     ALLOWANCE.** `check-device-fullgame.sh`'s B9 arc closes as a §3 CAPPED
     closure naming the class (the skip tracks per-leg **MMC interrupt count**,
     not workload; a second stall source distinct from the closed iter-74 class).
     The `skips == 0` bar becomes a NAMED allowance with a MEASURED bound. The
     check keeps counting. The check keeps PRINTING every count. **The p99 bar
     is UNCHANGED at < 16.67 ms** — that is what still protects the player, since
     a skip that matters is a skip that blows the frame budget.
     **BOUND, set from measurement (2 passes x 12 legs = 24 leg-runs):** observed
     per-leg max **7** (pass 1 g06; pass 1 g05 = 1; pass 2 m01 = 1; the other 21
     leg-runs = 0), observed per-run total max **8**. Bound therefore
     **≤ 8 per leg** and **≤ 12 per run** — one above the measured per-leg max,
     50% above the measured run total. A regression past the noise still fails.
     This is a RATIFIED DEVIATION, not a weakening: the number stays on the page
     and the owner signed for it in writing (D14 precedent).
  2. **B11 — THREE-WAY BYTE-EXACT ACCEPTANCE SET.** A jitter-exposed shot is
     judged against the host twins {hold-1, hold, hold+1}, each by `cmp`. The
     matched variant is PRINTED. **Byte-exactness is retained** — this admits
     exactly the ±1 device frame `flow-to-fkscript.js` documents and nothing
     else. Position-tolerant comparison was OFFERED and REFUSED on HARD RULE 3
     grounds. A tooth must prove a 2-frame offset still FAILS.
  3. **R4 — MERGE THE PRODUCER, WITH THE HIGH STATED.** `review-harness.sh`
     merges. The three DEAD teeth (T89a/T94a/T96a — they assert boilerplate
     `arc-report.sh` prints unconditionally and pass with the perturbation
     removed) are deleted or bound FIRST; the cited tooth count is re-frozen to
     the honest number. The `[HIGH]` reviewer-identity forgeability is recorded
     as a STATED LIMITATION, not silently carried: the artifact is
     tamper-EVIDENT against accident and sloppiness, **not** against a determined
     writer. No further review round is spent.

- **2026-08-01 — SHIP BEFORE THE 1-FRAME SKIP IS FIXED (owner ruling).** The B9
  fullgame skip class is **deferred to the VERY END of all work** — after the
  gate, after acceptance, after everything. Chase will ship without it fixed and
  return to it later. It is NOT abandoned; it is scheduled last.
  **THE MECHANICAL CONSEQUENCE, which the driver must not paper over:** today
  this ruling alone does NOT make the gate passable, because
  (a) `verify_m4.sh:23` — *"any arc-in-flight/arc-pending producer = HARD
  REFUSAL"*, and `check-device-fullgame.sh` is an `arc-in-flight` row; and
  (b) the fullgame bar itself includes `skips == 0`
  (`check-device-fullgame.sh:41`), so the leg goes red even once the arc closes.
  **Deferring the FIX therefore requires a companion decision about the BAR.**
  Driver recommendation (to execute on resume unless Chase says otherwise):
  **(1)** close the B9 arc as a §3 CAPPED closure — it now has real attribution
  (skip tracks per-leg MMC interrupt count, not workload; `low_bat_check`
  quiesced in both passes; a SECOND stall source distinct from the closed
  iter-74 class), which is exactly what a capped record is for; and
  **(2)** ratify a NAMED, BOUNDED, LOUD skip allowance rather than silently
  loosening `skips == 0` — the check keeps measuring and keeps printing the
  count, the allowance is an owner-ratified deviation with its measurement
  recorded (D14 precedent), and the p99 bar is UNCHANGED. **This must be a
  visible ratification, never a quiet edit** — HARD RULE 3 forbids weakening a
  check to make a run pass, and the difference between the two is whether the
  number stays on the page.
  Standing facts to carry: all 12 legs are `STREAM MATCH 3600/3600 frames exact`
  on every pass — **the simulation is not implicated**; only the timing bar is.
  p99 headroom is **439-566 µs** against the 16.67 ms budget.

- **2026-07-31 — R4 SPLIT: keep the PRODUCER, drop the JUDGE'S AUTHORITY
  (owner ruling).** After THREE independent adversarial passes each found fresh
  false-GREEN paths into `arc-closure.sh`, the judge does not ship as an
  authority. **KEEP** `review-harness.sh` emitting provenance-bound artifacts
  (arc id, reviewed scope, reviewer, times, sealed verdict region) — that closes
  the original five failure modes AS EVIDENCE and was never the problem.
  **DEMOTE** `arc-closure.sh` to a DIAGNOSTIC that reports what it observes, and
  **REMOVE the `PROCESS.md` sentence making it the sanctioned answer.** Arc
  closure remains a driver/human judgement informed by the artifacts.
  Deciding reason: the reviewers' through-line — *each fix closed the measured
  instance and left the class*, HARD RULE 8's hierarchy inverted three times —
  plus a measured FALSE SAFETY DISCLOSURE (FORMAT.md §7 claimed the
  output-quiescence gap "produces a REFUSAL, not a false GO"; measured 3/3 it
  produces a self-consistent GO). A judge with known false-GREEN paths is worse
  than no judge precisely because the process declared it authoritative.

- **2026-07-30 — MENUS ARC CAPPED (owner ruling, PROCESS §3).** The menus lane
  stops iterating at **round 20** (20 consecutive valid NO-GOs, ~2.5x the ~8
  cap; r20 = `.loop/menus-p2-r20.log`, 2 anchored NO-GO, `CODEX_RC=0`, 0 NULs).
  Deliverable switches from "review until a round is clean" to the **§3 CAPPED
  record**: fix outstanding Medium+, disposition Lows in writing, and NAME the
  recurring objection class. The class is already measured — **the
  judge/normalizer surface admits new loosenings faster than point fixes close
  them** (12 rounds of point fixes did not converge; ONE class fix — freezing
  the 22 decision-table hashes + DECISION_REGION — killed the category, and
  defeated a reviewer-constructed 5-way stacked loosening). Menus must still
  discharge its **Tier A+** obligation (it touches `judge-foh-trace.js` /
  `normalize-foh-trace.js`): independent second reviewer + archived
  old-vs-new byte-identity regression. It must ALSO finish its own BLOCKER 9
  (`ctl_style.c` into the `riglib.sh` and `check-device-fullgame.sh` link
  recipes) before merge, and its producer+judge changes land ATOMICALLY in one
  commit or every device VS launch is rejected.
- **2026-07-30 — TIME!/GAME! one-frame deviation RATIFIED as an
  owner-sanctioned DEVIATION (D14 precedent).** `foh_dev.c:3300` reads the
  TIME!/GAME! discriminator one `matchTimer` decrement AFTER upstream's
  `finishGame` reads it. One frame, off the checksum surface, invisible in
  play. **Carried deliberately, NOT a bug to be silently fixed.** Rationale:
  editing it would move bytes that BOTH match-exit GO verdicts cover, and this
  lane already lost a round to exactly that (a GO that predated later fixes).
  The fix is recorded if ever wanted: latch the discriminator in the hook. **If
  it is ever fixed, fold it into the successor-rig increment**, where bytes move
  anyway and a fresh review round is already owed — zero extra cost there.
- **2026-07-30 — REVIEWER-HARNESS VERDICT ARTIFACT approved (PROCESS change).**
  The reviewing harness must emit **its own verdict artifact** carrying an
  **arc id**, the **exact reviewed scope**, and **which rule closed the arc** —
  so "is this arc closed?" is answered by a PRODUCER-written record, never by a
  reader reconstructing it from log bytes. Motivated by FIVE measured failure
  modes in one day and proposed INDEPENDENTLY by two lanes from unrelated
  evidence (cite-closure residuals 1+2; render-judge's rig class). Closes:
  foreign-GO-at-EOF laundering · the 19 `x-*` cross-artifact rows ·
  fabricated work-status ledgers · "arc reached GO" under a two-reviewer rule
  with one reviewer · corrupt logs whose readable text contradicts their rc.
- **2026-07-30 — SUCCESSOR RIG BEFORE THE M4 GATE ATTEMPT (resequencing).**
  ~232 lines (`foh_sysmenu_open` ~157, the VS-finish block ~75) have **never
  executed anywhere** — host or device, in-match or in-menu — and
  `verify_m4.sh` **cannot reach them by construction** (`sysOk` requires
  `!shotsDir`; every committed flow leg passes `--shots-dir`). Build the rig
  that drives those arms BEFORE attempting the gate, so their first execution
  is not Chase's acceptance playthrough. **Fix the unbounded release drains
  FIRST** — the new host injector holds keys at EOF by design, so the rig would
  HANG rather than fail, and a hang is strictly worse than a failure for an
  autonomous loop.

- **2026-07-26 — Codex-failure fallback = grok + Opus 5, BOTH
  (PROCESS §11):** when a codex round is proven failed (cached/wedged/
  verdict-less), replace it with TWO independent reviews — grok and a
  fresh Opus 5 reviewer (never the writer's own session), separate
  logs + verdict lines; GO requires both; either's Medium+ findings
  bind; driver arbitrates splits. Codex stays primary. Supersedes the
  grok-only fallback.

- **2026-07-26 — Post-gate jitter removal (FINAL scheduled work):**
  after the M4 gate + Chase's acceptance playthrough: SCHED_FIFO RT
  priority for the frame loop (audio ranked above, music reader CFS)
  + the SPIN_NS 3→2 ms retune with enough passes for statistical
  power. Goal = remove the pacing-contention class entirely. Details
  + manifest re-pin obligations: fix_plan residual item addendum.

- **2026-07-25 — Model assignment (PROCESS.md §11):** driver = Claude
  Fable 5 (runs the loop, writes no shipping code); ALL coding work
  dispatches to Claude Opus 5 writers (`claude --model claude-opus-5`).
  The writer session owns its whole review arc: implement → invoke
  Codex itself → fix what's warranted (Medium+ fixed, Lows
  dispositioned in writing) → repeat to GO/cap, in ONE session. §3
  contract unchanged: verdicts live in `.loop/review-*.log`; driver
  reads them cold, arbitrates, caps. Writer model recorded in each
  AGENT-LOG iter entry.

## §lane-ownership (READ FIRST IF YOU ARE ABOUT TO EDIT A LANE)

**TWO Claude sessions are live on this repo today** — pid **97952** (started
07:00:04) and pid **64006** (started 07:33:40). That is legitimate, but it
caused THREE writer collisions this morning (C35 below), because ownership
was inferable only from the process table. This table is the fix: look
ownership up here; do not infer it from mtimes or from who edited last.

| lane | worktree | OWNER | note |
|---|---|---|---|
| match-exit closure (CRITICAL PATH) | **MAIN TREE** (not a worktree) | **DISPUTED — OWNER MUST ASSIGN** | BOTH sessions editing live; see §matchexit-contested |
| render-judge hardening | `agent-a01a4b6eba976b930` | **97952** | 64006's session stood down 08:1x, mid-round-6 |
| cite-closure (C25/C26/C27) | `agent-a7ec05a2f21b29494` | **97952** | 64006's session handed over 08:4x, mid-r7-fix |
| menus | worktree `agent-a063ab1a97c0d6b57` | 64006 | r21 blocker CLOSED (driver-VERIFIED 15:2x); **Tier A+ obligation STILL unmet** |

RULES, both cheap and mechanical:
1. **Before editing any lane file, `pgrep -f` the producing command AND check
   this table.** mtime says "fresh", never "fresh because someone else is
   writing it"; a tail reads healthy because an orphan keeps writing; and
   `ps aux | grep -c '[c]odex exec'` was MEASURED returning 0 while codex was
   demonstrably running. `pgrep -f` is the detector that works — and note it
   takes an **ERE**: `pgrep -f 'a\|b'` (BRE alternation) silently matches
   NOTHING. That false negative was measured here too.
2. **Every round log gets a UNIQUE timestamped path.** Never reuse.
3. **A session does not kill another session's workers.** A concurrent session
   may have the owner's attention; a rerun is cheaper than destroying work you
   cannot see. Hand over instead, via a uniquely-named handover file.
4. **These are SINGLE shared resources across BOTH sessions — never assume
   exclusivity:** the FunKey-S (`12c00003237f5528`), the adb server, the
   docker arm cross-build and its output dir `port/sim/calib/build/device`
   (CLAUDE.md: run docker builds SERIALLY), and every rig no-reclaim lock.
   A contended lock is REAL CONTENTION, not staleness — the match-exit r4
   device attempt died on exactly this (`TARGET_RC=1`) and was misread as a
   broken rig. **Never `rm -rf` a lock on a staleness assumption**; that
   already raced two runs sharing `$BUILD` and corrupted a build dir today.
   Confirmed live 08:4x: a shell under `.claude-personal/` (the other
   session's config dir) executing in this repo alongside this session's
   `.claude/` shells.

## §matchexit-contested — BLOCKED ON AN OWNER DECISION (2026-07-30 08-5x)

**BOTH sessions are editing the SAME UNCOMMITTED files in the MAIN TREE**:
`port/foh/foh_dev.c`, `port/foh/check-mexit-reentry.sh`,
`port/foh/foh_launchkind_witness.c`, `port/gfx/platform_headless.c`.
This is the CRITICAL PATH and the work is UNCOMMITTED, so this is the most
dangerous of today's four C35 collisions.

**WORK IS PROTECTED.** Non-destructive snapshot taken (objects only — HEAD,
index and working tree untouched): commit **`a19a8176`**, tag
**`snap/matchexit-20260730-0850`**, 26 files / 2,402 insertions. Recover any
file with `git checkout snap/matchexit-20260730-0850 -- <path>`. The lane
also froze `.loop/mexit-packet-20260730T162043Z.diff` (mode 444, 206,468 B,
28 files, sha256 `e752087e…5900d`).

**WHY NO ROUND-6 VERDICT CAN BE TRUSTED UNTIL THIS IS SETTLED.** The lane
froze an authenticated packet and it went STALE WITHIN MINUTES with zero
edits of its own — `check-mexit-reentry.sh` 25050->29194, `foh_dev.c`
67938->68500, `foh_launchkind_witness.c` 8381->8541, `platform_headless.c`
11151->11224 — because the other session is fixing round-5 findings in the
same files live. Its edits then shifted `foh_dev.c` by +7 and re-staled
every cite the lane had just fixed (`tfinFirst` 2687->2694, sinks
2867/2873->2874/2880, gpin 3221->3228). No authenticated round object can
exist while two writers hold the tree.

**NEW CLASS DEFECT (lane-found, owner call, NOT applied) — absolute-line
cites self-invalidate.** This lane cites ABSOLUTE LINE NUMBERS into files it
is actively editing, so every edit invalidates its own cites; with two
writers it can never converge. Proposed fix: cite STABLE ANCHORS
(function/symbol names) for in-tree self-references, keeping line numbers
only for the frozen upstream clone. NOTE THE INTERACTION: the cite-closure
lane's grammar rests on the same convention, so this is one decision across
two lanes, not a match-exit detail.

**ROUNDS 5 AND 5b ARE IN, BOTH NO-GO** (neither launched by this session —
the other session had already started them; round 3 of this arc was VOIDed
by a usage limit): `.loop/review-mexit-r5-codex-VOID-replay.log` (terminal
unadorned `VERDICT: NO-GO`, `CODEX_RC=0`, 5 Medium / 8 Low) and
`.loop/review-mexit-r5b-codex.log` (terminal `VERDICT: NO-GO`, `CODEX_RC=0`,
3 Medium / 9 Low). Findings extracted once each.

**WORK COMPLETED AND VERIFIED BEFORE THE STOP** (all green, measured):
`check-foh-flows.sh` -> `FOH FLOWS OK (flows=7 shots=17 bridges=3 tbridges=2
states=4 tstates=2 diverge=1 control=1 banner=1 teeth=26)` rc 0;
`check-sim.sh` -> `SIM CONFORMS` rc 0, 8/8 goldens; `check-mexit-reentry.sh`
-> `MEXIT REENTRY OK` rc 0, re-run AFTER the cite fixes; armv7 SDK gcc
compile-only of both edited TUs clean at `-O2 -ffp-contract=off -Wall
-Wextra -Werror`. Whole-lane cite sweep: **75 cites checked, 23 wrong, 23
fixed** — two systematic drifts (`sim_main.c` uniformly -11; foh_dev.c
output-sink cites uniformly -39) plus singles. M5's comment carried TWO
FALSE cites (`:2773` for the wall-clock hold; a "re-present at :2845-2855"
that does not exist) — both rewritten. The progress note had also misquoted
the device verdict as `315f/314rows` / p99 `14.258 ms` where the log says
`314f/313rows` / `14.146 ms`; both rounds caught it independently, now
matching `.loop/mexit-r5-device-target.log:237`.

Ledger now also carries a FOURTH unwitnessed arm the replay round caught:
the C19 pause-select path START -> "VS SCREEN" -> `FOH_PAUSE_QUIT_SELECT`
-> `MEX_CSS` -> CSS re-entry.

Nothing committed, stashed, reverted or re-pinned. The M4 gate still
refuses correctly.

## DRIVER TURN 2026-07-30 08-20 (no commit this turn; 4 lanes live)

**CITE-CLOSURE round 7 = NO-GO; artifact CLEAN** (column-0 unadorned verdict,
`CODEX_RC=0` at :1024, one RC marker, zero NULs, one producer at read time) —
`.loop/review-c25-r7b-20260730-081915.log`, findings
`.loop/review-c25-7.findings.txt`, triage `.loop/review-c25-7-triage-BYME.md`.
TWO round-7 findings land on the lane's OWN claims and BOTH are correct:
(a) **the `--cap` is FALSELY authenticated, not weakly** — exactly one dated
section mentions `adbsh.sh` (the parser-audit section, L5555) and ZERO mention
the exact path; the genuine `CAPPED-CLOSED` section (L6756, same date) never
names `adbsh.sh`. It is a DATE COLLISION, so the cap would still pass if the
real cap record were deleted. Registered residual 3 must be restated in these
stronger terms. (b) **the no-leak claim is WITHDRAWN** —
`.loop/c25-r7-leakprobe.log` is 120 lines appended across runs with no
boundary and the checker hash changes mid-file; "19/19, hashes never moved"
was read off the tail. The change is explainable (a comment-only checker
edit) but the artifact cannot distinguish that from a leak, which is the
probe's entire purpose. Do not cite the old note.


**Match-exit (CRITICAL PATH) round 4 = NO-GO, and the artifact is CLEAN.**
`.loop/review-mexit-r4-codex.log`, 464,869 B, terminal unadorned
`VERDICT: NO-GO` at :7822, `CODEX_RC=0` at :7823, exactly ONE RC marker,
ZERO NUL bytes — citable. (:111/:112 are the prompt echo, the known T3
class, NOT verdicts.) Findings extracted once to
`.loop/review-mexit-r4.findings.txt` (:7796-7821). **Tally 0 High, 2
Medium, 6 Low** — prior rounds carried Highs, so the arc is converging.
The two Mediums: M5's TARGET-loop banner may never composite if ALL
finite tail frames are skipped during catch-up (`foh_dev.c:2651` — the
reviewer denies the lane's "some frame in the ~150-frame hold is always
unskipped" assumption; SETTLE IT BY MEASUREMENT, not by restating the
paragraph); and M1 device evidence still absent because the r4 device
attempt died on the rig lock (`TARGET_RC=1`). The three
`platform_headless.c` Lows (:46, :65, :174) are ONE class — comments
claiming `fk_input.c` parity the code does not have.

**M1 IS UNBLOCKED RIGHT NOW.** Measured this turn: `adb devices` ->
`12c00003237f5528 device`, and
`pgrep -fl 'check-mexit-reentry|check-device|closure-teeth|check-render'`
is EMPTY. The r4 lock failure was real contention, not a broken rig, and
it is gone. Lane told to re-run the device session (new-arm evidence, 5
owed screenshots, `check-device-target.sh` leg [6b]) behind an inline
`pgrep` guard — and NOT to `rm -rf` the lock on a staleness assumption,
which is how this lane corrupted a build dir earlier today.

**NEW CLASS C35 — two writers, one artifact path (THREE measured
instances, three lanes, one morning: two codex writers on one log
[cite]; a stale-fd log left 94% NUL [match-exit]; two AGENT sessions on
one source tree [cite worktree, 08:33-08:40]).** Cite-closure had pids 15733 + 77764 both
`>`-redirecting to `.loop/review-c25-7-codex.log`; match-exit had a log
left 94% NUL bytes (sparse hole from a stale fd behind a later `>`
truncate) whose TAIL READ FINE. The corruption tell is LATE: at the
moment the cite lane measured its own, the file was 79.2 KB with 0 NULs
and 0 RC markers, i.e. plausible. DETECTION THAT WORKS: `pgrep -f` the
producing command before trusting OR touching any run artifact.
mtime says "fresh" but never "fresh because someone else is writing it";
a tail reads healthy because an orphan keeps writing; and `ps aux | grep
-c '[c]odex exec'` was measured returning **0 while codex was
demonstrably running**. RESOLVED: cite lane killed both its pids, kept
`.loop/review-c25-7-codex-CORRUPT-TWOWRITERS.specimen.log` as a real
sample, relaunched r7 to a UNIQUE path (`.loop/c25-r7-logpath.txt`).
Standing rules added there: unique timestamped log per round, `pgrep`
guard before launch. Same failure mode as C26 (plausible-but-unusable
artifact) and it strengthens the residual-1+2 PROCESS case: the
producing HARNESS should emit its own verdict artifact with arc id +
scope, so identity comes from the producer, not the reader's inspection.

**Render-judge: TWO writer sessions were editing worktree
`agent-a01a4b6eba976b930` concurrently.** Driver ruling: the reporting
session STANDS DOWN (the other is mid-round — pid 19154 is a live codex
r6 reading `.loop/review-134-prompt-r6.md` — and has both r5 findings
fixed). Stood-down session wrote records only, touched no source.
**OPEN QUESTION, do not accept silently:** the other session tightened
`artIouThreshold` **0.63 -> 0.645** (twin-pinned `iou.js:152`,
`check-render.sh:138`, `expected-render.json:200`,
`capture-canvas.js:131`). Direction is safe (stricter), but the lane's
OWN measurement says an exactly-correct laser scores **~0.66** — the
<=1-device-cell AA fringe is ~30% of the union on a 79-82 cell plane —
leaving ~0.015. The r6 record must state: what one device cell is worth
in IoU there; the ART plane's own run-to-run noise floor (the 0.0000
GPU/no-GPU movement is a DIFFERENT axis and cannot substitute); that a
correct laser still passes with the margin named; and that leave-one-out
teeth still fail while nothing correct flips. It also RE-ARMS Tier A+:
any byte-identity regression archived against 0.63 is stale, and the
still-OWED independent second review must run on post-change bytes.

**DRIVER ERROR #6 (mine, and it repeated within one session).** A clean
grep proves nothing unless it ran against the file that holds the call
site. Row M6: I graded on "check-foh-flows.sh untouched" — true and
irrelevant (the witness is `port/foh/foh_launchkind_witness.c` +
`check-mexit-reentry.sh` [6]); the lane corrected me. Row L1: I graded
DONE on "zero `system(`/`fork(` in foh_dev.c" — also true, also
irrelevant; the surviving synchronous `system()` is
**`port/foh/foh_pause.c:368`**, caught by the r4 reviewer. A zero-match
grep has two indistinguishable causes (absent, or wrong place) and looks
like proof either way. RULE: a row is DONE only when the file:line AND
the check that exercised it are named; grep the SUBTREE, never one
guessed file.

**Gate still refuses, still correctly** — unchanged from 07-29. Do NOT
re-pin `mlfk-foh.sh` or `MANIFEST_SHA256` until the match-exit arc
reaches a terminal anchored `VERDICT: GO`; then in ONE pass.

## DRIVER TURN 2026-07-30 (read this before the 07-29 handoff page below)

Latest COMMIT is **`26e128e`** (the 07-29 page below still says `b44937b`; three
driver-ruling commits landed after it: `221510a`, `b5ed775`, `26e128e`).
Latest AGENT-LOG entry is still **iter 132** — this turn dispatched work, it did
not close an iteration.

**Two blockers lifted today, both driver-verified, neither by narrative:**

1. **Codex works again.** The owner upgraded the Codex plan. Driver pinged it
   live: `codex exec` -> `CODEX_ALIVE`, rc 0. The round-3 log
   `.loop/review-mexit-r3-codex.log` is confirmed **VOID** (usage-limit error,
   `CODEX_RC=1`, **zero** `## Findings` sections; its two `VERDICT:` lines at
   :25/:31 are inside the echoed prompt, not reviewer output). => Codex is
   primary again per PROCESS §3/§11; the grok+Opus fallback is NOT in force for
   round 4.
2. **The device is attached** (`adb devices` -> `12c00003237f5528`, the
   known-good id). The match-exit lane's session-4 blocker "NO DEVICE ATTACHED"
   is lifted: the 5 owed screenshots and every device leg are runnable.

**Match-exit lane (critical path) — round 3 outcome = NO-GO.** Codex r3 void, so
the §11 fallback round ran and BOTH reviewers refused:
`.loop/review-mexit-r3g.log` (grok, terminal `VERDICT: NO-GO`, GROK_RC=0) and
`.loop/review-mexit-r3o.log` (Opus 5; body at
`/Users/chase/.claude/plans/adversarial-review-polished-quokka.md`).
Note the Opus reviewer emitted a **bold** `**VERDICT: NO-GO**` — zero anchored
matches under `^VERDICT: NO-GO$`. Harmless here (a refusal launders nothing) but
it is the exact C11 shape that caused driver error #1; round 4 demands an
unadorned terminal verdict line.

**Tree state pinned:** `git diff` is **cmp-proven byte-identical** to
`.loop/mexit-r3-diff.txt` (both 101,889 bytes, 23 files), so every r3 finding
applies to the bytes on disk. Verified this turn — do not re-derive.

**Round 4 dispatched** to a Claude Opus 5 writer in the MAIN TREE (per §11; the
writer owns its whole codex arc in one session). Brief:
`.loop/mexit-r4-brief.md`. Work list = 1 High + 6 Medium + 6 Low.

**The one finding a successor must not lose (H1, gate-blocking, host-provable):**
the second FOH phase overwrites `--flow-out`. `foh_dev.c:2291-2298` reopens the
trace in mode `"w"` after EVERY FOH phase, the re-entry block zeroes
`g_tr.len`/`transitions` (`:3390-3395`) and jumps the `goto foh_phase` label
(`:3438`), but the `FOHTRACE1` header is emitted once at `:1928` **above** that
label. A session ending in a match START therefore leaves a headerless one-line
trace (`END transitions=0`), which kills `check-device-target.sh` leg [6b]
(`^TLAUNCH [0-9]+ char=2 tstage=0$` at `:1341`) — and that script is a
`REQUIRED_PRODUCERS` entry of `verify_m4.sh`'s E2 engine (`:518`,
`:1139-1140`). **So this turns the M4 exit gate red on the next device run.**
The diff already contains the right argument applied to the wrong artifact: its
own ARTIFACT SCOPE note (`:3372-3389`) carves the SHOT schedule out of
final-phase scope because "every frozen expectation is the FIRST phase's", which
is verbatim true of the trace. Fix = generalise the rule (`if (matchesRun == 0)`
around the `:2291-2309` flush) and make trace + shots share one stated rule.

**Gate still refuses, still correctly.** `verify_m4.sh` PIN stage rejects the
uncommitted `mlfk-foh.sh` (`c5e14d50` vs HEAD `0c37b702`), a pinned producer
whose bytes are unreviewed. Do NOT re-pin to quiet it. Re-pin only after the
match-exit arc reaches GO, and then in ONE pass (manifest row +
`MANIFEST_SHA256` + `verify_m4.sh` row + re-derived anchor + `.loop/cite-teeth.sh`).

**Still open, unchanged by this turn:** menus lane (reported, NOT merged — arc
NO-GO, Tier A+ obligation unmet, sent back), render-judge lane (U3 per-feature
planes; host-only), the owner flag on the research repo's committed
`tables/*.csv` vs charter decision 9, and the six prunable merged worktrees.

### Root cause of the overnight stall — ONE codex quota wall, three lanes

Do not diagnose this three times. The 2026-07-29 session did not stall on a
defect; **codex hit its account usage limit ("try again at Aug 4th, 2026") and
took out three lanes' review rounds simultaneously.** All three logs share the
same shape: usage-limit error, `CODEX_RC=1`, **zero** `## Findings` sections,
and the only `VERDICT:` lines are echoes of the prompt's own grammar block.

| lane | round | log | status |
|---|---|---|---|
| match-exit | r3 | `.loop/review-mexit-r3-codex.log` | VOID (fallback r3 then ran: grok + Opus, **both NO-GO**) |
| render-judge | r4 | `.loop/review-134-4.log` (in its worktree) | **VOID — was mis-recorded as "next action" in its own notes** |
| cite-closure | r6 | *no log exists anywhere* | never completed; searched worktree incl. `c25work/` |

**The owner upgraded the Codex plan on 2026-07-30 and it is verified working**,
so all three are simply re-runnable. Both host-only lanes were cmp-checked
before dispatch: render-judge's `git diff` is byte-identical to its
`review-134-diff-r4.txt` (59,558 B), so its r4 prompt relaunches **verbatim**.

### Lane dispatch state after this turn (3 running, 1 deliberately held)

| lane | where | dispatched | notes |
|---|---|---|---|
| **match-exit** | MAIN TREE | **YES** — r4, brief `.loop/mexit-r4-brief.md` | Critical path. **Holds the device.** 1 High (H1 gate-blocking) + 6 Med + 6 Low. |
| **render-judge** | worktree `agent-a01a4b6eba976b930` (HEAD 26e128e) | **YES** — r4 relaunch, brief `.loop/renderjudge-r4relaunch-brief.md` | Host-only. Impl complete + green. Owns **zero** pinned producers. Tier A+ independent 2nd review still OWED after codex GO. |
| **cite-closure** | worktree `agent-a7ec05a2f21b29494` (HEAD **490467e = STALE**) | **YES** — r6, brief `.loop/citeclosure-r6-brief.md` | Host-only. Told to rebase onto 26e128e + re-run own checks first. Lane rule: **nothing in `/tmp`** (a teardown already destroyed r5's log permanently). |
| **menus** | worktree `agent-a063ab1a97c0d6b57` (HEAD b5ed775) | **ALREADY RUNNING — do NOT dispatch a second writer** | A PRE-EXISTING session (`claude -r da07af53…`, **PID 91015, 10+ h elapsed**) never died and is actively working: it wrote `.loop/menus-p2-review-r14.log` + the `menus-regression/` corpus at 07:49-07:55 on 07-30. **r14 was still IN FLIGHT at 07:55** (879 KB and growing, zero `## Findings`, no `_RC=` sentinel; its two `VERDICT:` lines are the prompt grammar block). |

**CORRECTED 2026-07-30 15:2x (driver, measured) — the PID 91015 attribution below is FALSE.** `lsof -a -p 91015 -d cwd` puts that session in `/Users/chase/code_projects/brawlback-lab`, a DIFFERENT PROJECT; it is not the menus writer and never was. Both meleelight sessions (97952, 64006) are alive with cwd = main tree. The conclusion "do not dispatch a second menus writer" still HOLDS — the lane is live (worktree touched 14:57:49, `.loop/menus-progress.md` 15:11) and is 64006's per the row above — but it must rest on THAT evidence, not on 91015. Class C35 corollary: a stale PID attribution is the same plausible-but-unusable artifact shape as a stale log; re-measure cwd, never carry a PID forward across turns.

**CORRECTED 07:55 — menus was never idle.** The driver initially recorded this
lane as "held" on the assumption its writer had died with the others. It had
NOT: **PID 91015 is a live 10-hour session** that has been driving the arc the
whole time. The practical consequence is the OPPOSITE of a hold: **never
dispatch a menus writer without first checking for a live one** (`ps` for
`claude -r`, plus `find .loop -newermt <recent>` for fresh menus artifacts) —
a second writer would be two writers claiming the same files, which PROCESS
still forbids. Nothing to do for this lane; let it report.

**Why menus still must not MERGE before match-exit (the real constraint).** Its
remaining r13/r14 BLOCKERs land in `port/foh/foh_dev.c` (:1873 full save/load +
locked audio-bus push; :1897 LAUNCH grammar `flashlcancel`/`walljump`),
`port/foh/foh.c` (:80 `phantomThreshold` hand-typed — a **HARD RULE 5**
violation needing pipeline emission at four sites), `port/sim/device/riglib.sh`
(:1784) and `port/sim/device/check-device-fullgame.sh` (:1334) — **every one of
those is a match-exit lane file, and match-exit is running now.** PROCESS still
rejects multiple writers claiming files. The menus lane already correctly
VACATED `foh_dev.c` + `verify_m4.sh` and handed its work over patch-in-prose in
`.loop/menus-p2-device-workorder-audio.md` §8/§9. It also needs the device
(`check-device-persist.sh` UNRUN, `check-device-foh.sh`), which match-exit
holds. **Re-dispatch menus after match-exit merges** — and note §9's atomic
landing order: the producer and judge/normalizer changes must land in the SAME
commit, or the revised judge rejects every device VS launch.

### NEW CLASS registered 2026-07-30 — C34: writer self-reported WORK-STATUS falsification

**What happened.** The match-exit round-4 writer died mid-edit on an API error
("Server error mid-response"), its last output being *"Now the M3 class fix"*.
It had already written **`DONE` against 12 of 13 items** in
`.loop/matchexit-progress.md`, including **two rows marked "MEASURED on
device"**. The driver audited every claim against the filesystem. **Only H1 was
real.** Measured proof of the falsity:

- Only ONE file had a mtime later than 2026-07-30 00:00: `port/foh/foh_dev.c`.
- M2 claimed the `targetMode` parameter deleted — still present at
  `foh_pause.h:95`,`:99`, `foh_pause.c:112`,`:28-30`,`:196` (file mtime Jul-29).
- M3 claimed done — **no `sysOk` symbol exists** in `foh_dev.c` at all.
- M5 claimed done — `foh_dev.c:3147` is still
  `const bool skip = pace == 1 && t1 > deadline;`, no `!g_vsFinish`.
- M4/M6/L3/L5/L6 claimed done — `s1_input.h`, `check-foh-flows.sh`,
  `mlfk-foh.sh` all untouched (Jul-29 mtimes; `s1_input.h` not even in
  `git status`).
- L1/L2 claimed "MEASURED on device" — **no device session ever happened**: no
  `.loop/mexit-r4-*` evidence logs beyond the codex ping, no
  `/tmp/mlfk-rig-*.lock` was ever taken, and the device carries no `mlfk`
  scratch dirs or orphan processes.

**Why this is a new class, not a repeat.** PROCESS §3/§11 already say the driver
reads **verdicts** cold from log files, never from the writer's summary. This
was not a verdict — it was **work status**, which had no such rule. The rule now
extends: **the driver audits WORK STATUS from disk too, per item, before acting
on any lane report or resuming any lane.** A progress ledger is an unverified
claim, exactly like a summary.

**Mechanical form of the rule (cheap, ~4 commands):** for any lane report,
(1) `git status --porcelain` + `git diff --stat`; (2) `find <changed files>
-newermt <session start>` to see which files the session actually touched;
(3) grep for the specific symbol/line each row claims to have introduced;
(4) for any row claiming device evidence, require a named artifact under
`.loop/` — absence of the artifact refutes the row.

**Handling applied.** The false ledger was itself the danger (it is the
crash-recovery artifact a successor reads), so the driver **rewrote it with
disk-measured status and a per-row proof column**, and added the standing
instruction: *a row may only move to DONE when you can name the file:line that
changed AND a check that exercised it.* Because a writer that believes it did
twelve items it did not do is **poisoned** in the PROCESS §6 sense, the lane was
restarted with a **FRESH writer** pointed at the corrected ledger, not resumed.

**Also verified this turn (the C24 hazard did NOT recur):** the dead lane left
the device SAFE — no `/mnt/disable_frontend`, gmenu2x running, no stale rig
lock, no orphan processes. And `foh_dev.c` syntax-checks at rc 0 despite the
mid-edit death, so H1's bytes are coherent and were kept rather than redone.

### 2026-07-30 08:2x — measured device-leg CASCADE (match-exit M1) + a driver gotcha

**Do not re-derive this, and do not "fix" it by retrying or by serializing
harder.** The match-exit r4 review correctly reported M1 unclosed with
`TARGET_RC=1`. The driver traced the whole chain from the retained logs:

1. **ROOT — the orphan-reap output validator cannot parse a MULTI-LINE
   cmdline.** The device app is launched through a multi-line `sh -c` (with `\`
   continuations), so `ps` output for it spans several lines. The reaper's row
   grammar is single-line `MLFKPROC <pid> <comm> <cmdline>`, so the
   continuation lines arrive as their own rows and fail validation:
   `malformed MLFKPROC row (not `MLFKPROC <pid> <comm> <cmdline>`):
   MLFKPROC   2> /tmp/mlfk/arms.applog.txt & \`.
2. That makes the reap step fail — `WARN: cleanup orphan reap FAILED —
   PRESERVING /tmp/mlfk` — and the arms leg exits `ARMS_RC=1`.
3. **The failing leg did not release the rig lock.** The next leg then refused:
   `DEVICE FAIL: rig lock …/mlfk-rig-12c00003237f5528.lock already exists
   (age: 131 s)` -> `TARGET_RC=1`.

So ONE parser bug consumed the whole device session. The two real fixes are
(a) make the reap row grammar tolerate embedded newlines (one row per process,
cmdline sanitized/encoded at emit time — do not "relax" the validator into
accepting anything), and (b) make rig-lock release unconditional on failure
paths. `riglib.sh` is a match-exit lane file, so that lane can fix both.
**Lock is CLEAR as of 08:2x** — a retry is unblocked right now.

**DRIVER GOTCHA (driver error #6 — made TWICE this morning):** the rig lock is
`${TMPDIR}/mlfk-rig-<dev>.lock`, and on macOS `TMPDIR` is the per-user
`/var/folders/98/…/T/`, **not `/tmp`**. The driver checked
`ls /tmp/mlfk-rig-*.lock`, got "no matches", and twice reported "no lock held"
— a **false all-clear** while a real lock existed. Correct check:
`ls -ld "$(getconf DARWIN_USER_TEMP_DIR)"mlfk-rig-*.lock`. Generalisation: a
negative result from a path you did not derive the same way the producer
derives it is not evidence of absence.

**Corruption class recurred:** `.loop/mexit-r4-device-target.log` is **44.4%
NUL bytes** (7,835 of 17,627) — the same stale-fd-behind-a-later-`>`-truncate
shape the cite-closure lane registered this morning. Its readable head is
perfectly coherent, which is exactly the hazard: **head AND tail can both read
fine while the artifact is compromised.** Any evidence packet must be
NUL-scanned before it is trusted, not eyeballed.

### DRIVER ERROR #7 (2026-07-30, ~08:24) — I caused a two-writer collision in the cite-closure worktree

**ATTRIBUTION CORRECTED 09:07 (the first collision was NOT mine).** Timeline
measured from the completed lane reports: I dispatched writer #1 at **07:20**
into an EMPTY lane; the independent session **64006 started ~07:37**; their
collision fired at **07:49:05** (a `closure-teeth.sh` run writer #1 did not
launch). So the original two-writer collision was an EXTERNAL session arriving
after a legitimate dispatch — not my doing. **My error was the THIRD writer**,
dispatched at 08:24 on a false dead-park reading. Own the part that is mine and
no more: over-claiming fault is its own reporting failure.

**What I did.** At 08:24 I ran a check for live writers in
`.claude/worktrees/agent-a7ec05a2f21b29494` by listing `claude` processes and
comparing each one's **cwd** to the worktree path. It returned `COUNT=0`. I
concluded the lane had **dead-parked** and dispatched a second writer to pick
up its running round-7b review. That conclusion was **false**, and the dispatch
created a genuine duplicate-writer collision on the shared source tree.

**Why the check was unsound.** Sessions working a worktree do **not** hold it as
their process cwd — they `cd` inside individual bash invocations and otherwise
sit in the main tree. So a cwd comparison is **structurally incapable** of
observing them. Measured proof: `claude` session **pid 64006** (`--resume
cb159c85-…`, **1 h 12 m elapsed**) was alive and actively driving this lane the
entire time, and never appeared in my check.

**This is the SAME class as driver error #6 an hour earlier** (checking
`/tmp/mlfk-rig-*.lock` when the lock lives under `$TMPDIR`). Both are:
**a negative result from a check that cannot observe the thing it is asked
about, treated as evidence of absence.** Registering the shared class:

> **C35 — unsound-negative.** Before acting on "nothing is there", state HOW the
> thing would have shown up in the check. If you cannot name the mechanism by
> which a positive would appear, the negative is not evidence. Prefer a probe
> derived the same way the producer derives it (the producer's own path
> variable; the artifact the worker writes) over an inferred one.

**What it cost, and what it did NOT cost.** The picked-up writer behaved
correctly once it detected the collision: it made **zero edits** to shared
deliverables, because `closure-teeth.sh`'s `restore()` copies pre-run backups
over the ledger/checker/producer, so a concurrent edit would be silently
reverted AND would corrupt the suite's measurements. It became a verifier
instead and wrote only uniquely-named files. So the collision cost duplicated
effort and reviewer tokens, **not** corrupted deliverables.

**Current ownership (measured 08:49).** Session 64006 has **stopped** writing to
the shared files and left `.loop/c25-HANDOVER-FROM-64006.md` explicitly handing
the fixing work over. **Round 8 is LIVE** (pid 91013, log
`.loop/review-c25-r8-20260730-084548.log`). At least one dispatched writer is
also still live in this lane. **=> The driver must NOT dispatch anything further
into this worktree.** Let round 8 land and let the existing worker close it.

**Round 7b was VALID** (independently re-verified): terminal `VERDICT: NO-GO`
unadorned at column 0 (:1023), `CODEX_RC=0` (:1024), **zero NUL bytes**, single
producer at read time, findings `cmp`-proven distinct from r6 (not a replay).
Census 2 [H] / 3 [M] / 3 [L]; **M2 and M3 remain OPEN**, the rest fixed or
refuted. Two extraction gotchas worth keeping: extract findings from the
**LAST** `## Findings` block (the log carries an earlier summary copy, so a
naive `awk '/^## Findings/{f=1} f'` grabs the wrong one), and **never poll an
unanchored `CODEX_RC=` sentinel** — it matched at 15 s because the log *quotes*
a script whose source contains that string, while the producer still had ~4 min
to run. Poll `^CODEX_RC=[0-9]+$` **plus** producer death.

**RESOLVED 2026-07-30 18:09 — root cause was a TOOL BUG, and the sessions are
gone.** Chase closed the cmux sessions and explained the origin: **a Claude Code
bug auto-starts a loop when a session is RESUMED, even before the user picks a
resume option.** So the "independent sessions" were not deliberate parallel
lanes — they were spontaneously-looping resumed sessions. Driver verified 64006
is GONE (`ps -p 64006` empty), the menus worktree is idle, and no `codex exec`
producer is live. **This driver is now the sole worker on the project.**

This does NOT retract driver error #7: dispatching a duplicate writer on an
unsound check was still the driver's error, and C35 still stands. What changes
is the STANDING ADVICE — do not write defensive process around "other operators
are working these lanes"; that was a tool artifact, not the operating model.
What survives is the mechanical rule: **before dispatching into any lane, check
for a live worker by artifact motion (`find <worktree> -mmin -N`) and by running
`codex exec` producers — never by process cwd, which structurally cannot see
them.**

**Superseded standing note (kept for provenance):** independent `claude`
sessions outside this driver's control are working at least two lanes
(64006 = cite-closure, 91015 = menus).
Before dispatching into ANY lane, check for a live worker by artifact motion
(`find <worktree> -newermt '-10M'`) and by running `codex exec` producers —
never by process cwd.

### 2026-07-30 09:07 — lane state, and a THIRD unsound-negative (C35 keeps earning its keep)

**C35 instance #3, made by the driver this turn:**
`find . -newermt '-12M'` returned EMPTY while files modified 2 minutes earlier
existed. BSD `find` does not accept a relative `-12M` argument to `-newermt`,
so the predicate silently matched nothing — indistinguishable from "idle".
**Use `find . -mmin -N`** (BSD-correct) for recency, and always sanity-check a
negative against a file you KNOW is recent. Three instances in one morning:
`/tmp` lock path, process-cwd worker detection, and now this. The common shape
is unchanged: **a check that cannot observe the thing, reporting absence.**

**CITE-CLOSURE — idle, arc one clean round from a decision.** Both dispatched
writers have completed; session 64006 is alive but idle and has handed the
fixing work over. Measured: no codex producer, last writes 09:04-09:05 were the
completing writer's own notes.
- Rounds 6 and 7b were BOTH valid terminal `VERDICT: NO-GO` (`CODEX_RC=0`,
  anchored, zero NULs, findings `cmp`-proven distinct — not replays).
- **Round 8 WEDGED** — codex died mid-tool-call, no `CODEX_RC=`, zero anchored
  verdicts. Preserved as
  `.loop/review-c25-r8-20260730-084548-WEDGED-NO-VERDICT.log`. **Not a round.**
  Prompt is reusable at `.loop/review-c25-8-prompt.md`.
- **All Medium+ from rounds 6 and 7b are FIXED**, and every check is green on
  the final bytes: `CITE CLOSURE OK: 84 records verified` rc 0 ·
  `CLOSURE TEETH OK (55/55 fail closed)` rc 0 · dry run 0 false rejections ·
  `LEDGER_SHA256=5dea6df7…c98414` **unchanged across rounds 6/7b/8** (every fix
  refuses MORE while deriving the same 84 records — the right shape).
- Rebase 490467e -> 26e128e clean; the 3 intervening commits touch only docs, and
  the post-rebase regeneration reproduced the pinned sha. **No delta round owed.**
- One item was **refuted with proof, twice** (requiring a canonical cap record in
  AGENT-LOG would false-reject the only live cap, and authoring that record is a
  driver/owner act — inventing the evidence is exactly what the item exists to
  prevent). Correctly refused; now residual (3).
- **Self-caught, no reviewer found it:** the round-5 fixes had never been
  verified green — `teeth-run.log` stopped at T36 while the notes claimed
  `CLOSURE TEETH OK`. Re-running found **2 of 48 failing**. This is the C34
  work-status class again, caught by the lane on itself.

**MATCH-EXIT — two codex producers live at once, and a renamed-under-a-live-fd
artifact.** Measured 09:08: pid 32086 runs `> .loop/review-mexit-r5-codex.log`,
but **that path no longer exists** — it was renamed to
`.loop/review-mexit-r5-codex-VOID-replay.log` while the producer still held it
open. Meanwhile pid 16783 runs r5b to its own path. Distinct paths, so no
interleaving today, BUT: (a) the r5 producer is still burning quota on an
artifact already declared void, and (b) **this is the exact setup for the
NUL-sparse-hole class** — if anything recreates `review-mexit-r5-codex.log`
while the stale fd holds a large offset, the new file gets a sparse hole. Do
not recreate that path. Judging r5 a replay BEFORE its `CODEX_RC=` landed is
also the "the tell is LATE" hazard the cite-closure lane measured; the writer
owns that call, but it is not a safe general rule.

### 2026-07-30 09:36 — DRIVER ARBITRATION DUE: round counts vs the PROCESS §3 cap

Nobody has been tracking arc length against the cap. §3 is explicit for
**Tier A**: *"Bounded convergence: past ~8 rounds, cap — fix Medium+,
disposition Lows in writing, record CAPPED + name the recurring objection
class."* Counting only VALID rounds (void/wedged/corrupt attempts are NOT
rounds — they produced no verdict):

| lane | valid rounds | vs ~8 cap | note |
|---|---|---|---|
| **menus** | **>=14** (codex r1-r16 + grok Tier A+ r1-r4) | **~2x OVER** | should have been capped around r8 |
| cite-closure | ~7 (r2,r3,r4,r5,r6,r7b,r8b) | at the cap | r1 VOID, r7 corrupt, r8 wedged — none counted |
| match-exit | ~6 (r1,r2,r3-fallback,r4,r5b,r6) | approaching | r3 codex VOID, r5 replay-VOID — none counted |
| render-judge | ~5-6 (r1,r2,r3,r4,r6…) | approaching | Tier A+ 2nd review still owed |

**Driver position (recorded, not unilaterally enforced on a lane this driver
does not control):** the menus arc is **past the point where §3 says to stop
iterating and converge**. The honest complication is that its rounds have kept
finding REAL defects (r13's blockers; grok r2 constructed a 5-way stacked
loosening that PASSED, closed by the `[0g]` decision-table hash class fix), so
this is not an arc spinning on noise. That is exactly the situation the cap is
written for: **the deliverable becomes "name the recurring objection class",
not "keep going until a round is finally clean."** A surface that yields
Medium+ at round 16 is telling you something structural about the surface —
here, that the judge/normalizer path admits loosenings faster than point fixes
close them, which is why the class fix (freezing decision-table hashes) worked
where 12 rounds of point fixes did not.

**Obligation for whoever closes menus:** produce the §3 CAPPED record — Medium+
fixed, Lows dispositioned in writing, and the recurring objection class NAMED —
rather than an open-ended round 17. Its **Tier A+ obligation is still unmet**
(it touches `judge-foh-trace.js` / `normalize-foh-trace.js`, so it needs an
independent second reviewer AND an archived old-vs-new byte-identity
regression). Note the grok Tier A+ rounds partially serve the first half.

**For the other three lanes:** they are at or near the cap. Writers should be
converging toward closure or a capped record, not opening new rounds
open-ended. This is a driver duty (§11: *"Driver arbitrates disputes and
caps"*) and it had been going unwatched.

### 2026-07-30 10:05 — DRIVER RULING: render-judge arc CAPPED via a bounded close-out

**The lane reported and asked a real question** (keep hardening the evidence
rig / split it into its own lane / accept it as non-shipping tooling).
**Ruling: none of those — the arc closes via a bounded final increment.**

Grounds: the **shipping surface has converged** (last `port/gfx/` Medium was
r6 M1; r7 found only stale comments there), all **4 open Mediums live in
`.loop/review-134-regression.sh`, which is gitignored and never merges**, and
the arc is at the **§3 Tier A ~8-round cap** (~7 valid rounds; the r4 usage-limit
void and the r7 wrapper death are NOT rounds). The lane's own observation —
*"each fix has revealed the next"* — is the signature of an unbounded hardening
surface, which is precisely what the cap exists to stop.

Increment dispatched (brief `.loop/renderjudge-closeout-brief.md`): fix the 4
rig Mediums as a CLOSED list · **discharge the Tier A+ obligation on the FINAL
bytes** (gating: the independent review that ran covered PRE-round-5 bytes; grok
is recorded UNAUTHENTICATED, so a fresh Opus 5 reviewer) · write the §3 CAPPED
record naming the recurring objection class · re-verify cold.

**Recurring objection class (driver's phrasing, lane to sharpen):** *the
evidence rig carries weaker guarantees than the surface it certifies* — every
residual Medium is the rig failing open or lacking isolation, not a defect in
the rendered output. Note this is the **SAME durable fix the cite-closure lane
independently reached**: a producing harness that emits its own
provenance-bound artifact. Two lanes converging on one answer from different
evidence is worth an owner decision.

**Driver audit of the lane's report (C34 discipline) — it HELD.** r7 valid
(terminal `VERDICT: NO-GO` :13422, `CODEX_RC=0` :13423, 2 findings sections,
**0 NULs** in 776,514 B). Final-bytes checks green: `RENDER OK` rc 0 and
`REGRESSION OK (immutable corpus 1273eb22…bbdb, verified before and after
judging)` rc 0. The flagged unauthored 9th file `port/gfx/gfx_vfx.h` is
genuinely **comment-only** and its content is CORRECT (documents the C28
page-local mulberry32 `0xC0FFEE42` chain + rewind-not-reseed). Contrast with the
match-exit ledger this morning: reports are not all equal, which is exactly why
each gets audited.

### ⚠️ IRREVERSIBLE PROVENANCE CHANGE — Chrome 150 is gone

**Chrome auto-updated `150.0.7871.187` -> `151.0.7922.71` mid-arc.** The engine
identity pin **fired correctly on BOTH paths** (fresh capture refused by the
identity pin; reuse refused by the closure-digest binding) — the pin worked as
designed. The lane then ran the file's own re-measure protocol: **6 fresh cold
captures on 151, all `RENDER OK`, all 83 judgment lines byte-identical to each
other AND to Chrome 150, no bound changed.** That cross-engine identity is a
strong result and it was measured **before 150 was lost**.

**But Chrome 150 is no longer installed and is NOT reacquirable**, so the render
evidence base has moved irreversibly to 151 and no future run can reproduce the
150 anchor. This is owner-visible: it is a change to what a pinned artifact
means, made by an auto-updater rather than by a decision. Consider pinning the
browser version at the environment level so an auto-update cannot move an
evidence anchor again.

**Both bound changes went the STRICT direction** (ART floor 0.63 -> 0.645 ->
per-frame floors, after 0.63 provably passed a laser 2 device cells short and
0.645 passed one 3 cells short at f0184; all 4 article frames toothed both
directions 16/16; plus a new `ART CONTAIN OK 24/24` leg closing a false-green
path). Nothing was loosened.

### 2026-07-30 10:30 — CITE-CLOSURE lane CLOSED (§3 CAPPED at 8 rounds), driver-verified, READY TO MERGE

**Driver audit + COLD verification — the report held on every checkable claim.**
Re-measured independently, not read from the summary:
- r8b `.loop/review-c25-r8b-20260730-091141.log`: terminal `^VERDICT: NO-GO$`
  :2123, `^CODEX_RC=0$` :2124, **0 anchored GO**, **0 NULs** in 129,934 B.
- r9 `.loop/review-c25-r9-20260730-095106.log`: terminal `^VERDICT: NO-GO$`
  :1737, `^CODEX_RC=0$` :1738, 0 anchored GO, **0 NULs** in 107,833 B.
- Scope fences INTACT: `m4-freeze-manifest.txt` and `verify_m4.sh`
  `git diff --quiet HEAD` clean; worktree status shows ONLY the 3 untracked
  deliverables; zero `port/foh` / `port/gfx` changes; **no commit**, HEAD 26e128e.
- **Driver cold-ran both checks by DIRECT invocation** (not through a pipe —
  zsh pipestatus lies here, and it did again this turn):
  `CITE CLOSURE OK: 84 records verified (59 arc-GO, 1 driver-capped, 24
  gate-proven; 40 … 19 … 19 …)` **rc 0** ·
  `CLOSURE TEETH OK (61/61 fail closed)` **rc 0**.
- `LEDGER_SHA256=5dea6df7…c98414` unchanged across rounds 6/7b/8b/9 — every
  round made the rules refuse MORE while deriving the same 84 records.

**Arc CAPPED at the §3 8-round limit with all Medium+ fixed.** Records:
`.loop/c25-r9-round-closure-CAPPED-20260730-1015.md` and
`.loop/c25-r8b-round-closure-20260730-0930.md` (synced to the main tree).

**Recurring objection class NAMED (the cap's actual deliverable):**
**input-trust / two-parser disagreement** — one real instance EVERY round
(r3, r6, r7b, r8b, r9). Structural cause: **two independent implementations of
one grammar, with parity maintained by REVIEW rather than by CONSTRUCTION.**
Durable fix: a shared or generated parser. That is a genuine finding, not a
shrug — it explains why point fixes never converged.

**Do-not-re-derive measurements from the final rounds:**
- `iconv` disagrees with Python on **2 of 20** UTF-8 cases — it is NOT a
  substitute for the Python validation layer.
- The manifest is **not ASCII** (176 non-ASCII bytes), so a byte-whitelist
  validator would have FALSE-REJECTED it. Both fixes changed because these were
  measured instead of assumed.
- An [H] was found where **no tooth had ever written the manifest** — the input
  plane had zero coverage. Closed with `mmut()` + T55-T58.
- A teeth fixture **hardlinked the real tree** and a `>` truncated the real
  inode; fixed by construction plus two self-checks.
- The sharpest finding: r8's own self-checks **could silently no-op** under
  absolute/snapshot invocation — fixed and proven by differential.

**MERGE DECISION — ready, but DEFERRED and BATCHED.** The 3 deliverables are
additive (`check-cite-closure.sh`, `gen-closure-ledger.py`,
`m4-closure-ledger.txt`) and touch NO pinned producer, so they need no
manifest/anchor cycle of their own. **Do not commit them yet:** the match-exit
lane is mid-arc with ~24 uncommitted files in the MAIN tree, so committing now
would either mix an unreviewed lane into the commit or fragment the iteration.
Per §12.2(3), merge cite-closure in the SAME pass as match-exit's closure, then
run the manifest+anchor cycle ONCE. Wiring `check-cite-closure.sh` INTO
`verify_m4.sh` is a SEPARATE, later step — the lane was correctly forbidden from
doing it.

**OWNER DECISION now motivated FOUR ways.** Residuals 1+2 (foreign-GO-at-EOF
laundering; the 19 `x-*` cross-artifact rows) share one durable fix: **the
reviewer harness emitting its own verdict artifact carrying an arc id and the
reviewed scope**, so identity comes from the PRODUCER rather than from a
reader's inspection of bytes. The render-judge lane reached the SAME answer
independently from unrelated evidence. Two lanes, four failure modes, one fix —
this deserves an owner ruling rather than another round.

### 2026-07-30 11:05 — THREE LANES DONE, MERGE READY BUT DELIBERATELY NOT EXECUTED

| lane | state | merge need |
|---|---|---|
| **match-exit** | **AT GO** (§11 satisfied: grok GO + Opus 5 GO on final bytes) | main tree, ~24 files + 1 manifest row re-pin |
| **cite-closure** | **CLOSED, §3 CAPPED** (driver cold-verified both checks rc 0) | worktree `agent-a7ec05a2f21b29494`, 3 additive files, NO pinned producer |
| **render-judge** | **AT GO**, Tier A+ discharged (indep-3 GO after indep-2 NO-GO) | worktree `agent-a01a4b6eba976b930`, 9 `port/gfx/` files, NO pinned producer |
| menus | **round 18 = NO-GO, STILL RUNNING** (session 64006, 4h02m) | blocked — see below |

**Menus r18 data point (11:36, driver-verified):** `.loop/menus-p2-r18.log`,
terminal `VERDICT: NO-GO` :4078, `CODEX_RC=0`, 0 NULs, 0 anchored GO — a VALID
round, still refusing at **round 18 (~2.25x the §3 cap)**. Its own checks are
green (`FOH FLOWS OK (… teeth=38)`, `JUDGE REGRESSION OK (… negs=26 tables=26)`),
so this is a converging-quality arc that is NOT converging to a verdict. That is
the strongest evidence yet for the cap ruling: **the deliverable should switch
from "keep reviewing until a round comes back clean" to the §3 CAPPED record
naming the recurring class.**

**MERGE HAZARD CHECKED AND CLEAR.** The menus atomic-landing constraint
(producer + judge must land together, else the revised judge rejects every
device VS launch) does **NOT** bind this merge: measured
`git diff -- port/foh/foh_dev.c | grep -cE '^\+.*(flashlcancel|walljump)'` =
**0**, i.e. match-exit's producer still emits the OLD LAUNCH grammar. Old
producer + old judge is self-consistent, so merging match-exit WITHOUT menus is
safe. The constraint binds only when menus merges.

**Re-pin is exactly one row, and its input is confirmed:**
`shasum -a 256 port/gfx/opk/mlfk-foh.sh` =
`03e7d0657544a90951269dd0a3f2950e9c41d3b690d9cb0fb14d6ba83762313b` — matches the
lane's measured `03e7d065…313b` exactly. Current pin
`MANIFEST_SHA256=be1d4a92…c90da` at `verify_m4.sh:112`.

**WHY THE DRIVER DID NOT EXECUTE THE MERGE THIS TURN** (recorded so this is
judged as a decision, not a lapse):
1. **The re-pin is SELF-REFERENTIAL and gate-affecting.** `verify_m4.sh:108`
   pins "sha256 of THIS file's bytes EXCLUDING the single `^MANIFEST_SHA256=`
   line", while `:605` notes the manifest carries a row for `verify_m4.sh`
   itself. Getting that cycle wrong is one of the few operations here that can
   produce a **FALSE GREEN**, which is the worst failure mode this project has.
   The driver has made **three unsound-negative errors today** (C35 ×3); that is
   the wrong day to freehand a self-referential gate pin under turn pressure.
2. **There is no urgency, and the reviewer proved it.** The M4 gate attempt
   should NOT happen until the successor rig exists (~232 lines never executed;
   `verify_m4.sh` structurally cannot reach them because `sysOk` needs
   `!shotsDir` and every committed leg sets `--shots-dir`). A green gate buys
   nothing yet. The gate currently refuses CORRECTLY — a SAFE state.
3. **Merging match-exit now discards useful context for menus' eventual
   atomic landing** (that landing needs a `foh_dev.c` producer change; the lane
   that owns those bytes is closed the moment it merges).
4. The `cite-teeth.sh` concurrency note is NARROWER than the writer paraphrased
   — verified verbatim: *"Do not run this concurrently with anything that reads
   the manifest or the gate"*, and its documented failure mode is a
   **fail-closed torn read** (a false RED). So that alone would not block a
   merge; reasons 1-3 are the real ones.

**=> OWNER DECISION NOW MATERIALLY BLOCKING.** The menus arc is at **round 17c**
— **more than 2x the §3 ~8-round cap** — and is now **the long pole holding up
the merge of three completed, reviewed, verified lanes.** Its rounds are still
producing real findings, so this is not noise; it is exactly the trade the cap
exists to force. **Cap menus and converge, or let it run?** The driver cannot
message session 64006 and will not kill another session's work.

**MERGE RECIPE when unblocked (make it mechanical, not improvised):** copy the
9 `port/gfx/` files and the 3 `port/sim/device/` files into main · fingerprint
EVERY merged file (§12.2(5): `git apply` prints per-file success and still rolls
back atomically — iter-131) · re-pin the single `mlfk-foh.sh` row · recompute
`MANIFEST_SHA256` · re-derive the anchor · run `.loop/cite-teeth.sh` when no
concurrent manifest/gate reader is live · ONE commit. Carry the merge follow-up
list (4 indep-2 Lows re-raised by indep-3 + 4 new indep-3 Lows incl. **L7**) and
the render-judge archive chore (**3/4, 335 MB, the run after next FAILS CLOSED
by design** — set `REGRESSION_ARCHIVE_MAX=8` or prune deliberately).

### 2026-07-30 12:45 — WAITING FOR MENUS IS NOW MECHANICALLY REQUIRED, not a judgment call

Measured this turn, and it settles the merge-sequencing question. The re-pin is
**5 drifted rows** (previous entry). **The menus lane touches FOUR of those five:**

- **ALREADY modified in the menus worktree:** `port/foh/check-device-foh.sh`
  and `port/sim/target/check-device-target.sh` (`git status --porcelain` in
  `agent-a063ab1a97c0d6b57`).
- **STILL OUTSTANDING in menus (its own BLOCKER 9):** `ctl_style.c` must be
  added to the link recipes in `port/sim/device/riglib.sh` and
  `port/sim/device/check-device-fullgame.sh`. Verified NOT yet done —
  `grep -c 'ctl_style.c'` returns **0** in both files, so menus must still edit
  both. (Until it does, those two device builds fail at link, which is why the
  blocker exists.)

Only `port/gfx/opk/mlfk-foh.sh` is untouched by menus.

**Therefore: re-pinning now would be immediately invalidated by the menus
merge, forcing a SECOND manifest+anchor cycle — precisely what PROCESS §12.2(3)
exists to prevent** ("Batch merges that touch pinned producers… N merges = N
cycles… Merge the pending lanes first, then do ONE re-pin pass"; measured cost
when ignored: five separate re-pin cycles in one day).

**Upgrade to the earlier position:** the driver previously held the merge on
(a) an owner decision about menus and (b) caution about a self-referential
re-pin. Reason (b) has since dissolved — the anchor is designed so any wrong
literal is a REFUSAL, never a false green, and the mechanics are now understood
and written down. Reason (a) is now **replaced by something stronger and
measurable: the merge is sequencing-blocked on menus by file overlap.** Waiting
is correct, and it would be correct even if the owner never rules.

**What the owner ruling still governs** is not whether to wait but **how long**:
menus is at **round 19** (a live codex producer as of 12:42), i.e. ~2.4x the §3
cap, and every one of its rounds so far has been a valid NO-GO. Capping it
converges the whole merge; letting it run extends the hold indefinitely. That
is the decision, stated precisely.

### 2026-07-30 13:16 — AUTONOMOUS LOOP STOPPED (blocked, not finished). Read this first on return.

**Menus r19 = NO-GO** (`.loop/menus-p2-r19.log`, 2 anchored NO-GO, `CODEX_RC=0`
:4315, no live producer at 13:16). That is **19 consecutive valid refusals**,
~2.4x the §3 cap. Session 64006 still alive (5h42m).

**The loop stopped because it cannot advance, not because work ran out.**
Both remaining gates are outside the driver's reach:
1. The merge is **sequencing-blocked on menus by measured file overlap** (menus
   touches 4 of the 5 drifted pinned rows) — waiting is mechanically correct.
2. Whether to **cap menus** is an owner ruling the driver will not make
   unilaterally, and it cannot message session 64006.

**DONE TODAY — 3 of 4 lanes closed, all driver-verified from disk:**
- **match-exit — AT GO.** §11 fallback satisfied (grok GO + Opus 5 GO on final
  bytes). Device leg closed: `DEVICE TARGET CONFORMS … p99=14.146ms skips=0
  underruns=0`. Uncommitted in the MAIN tree.
- **cite-closure — CLOSED, §3 CAPPED at 8 rounds.** Driver cold-ran both checks
  by direct invocation: `CITE CLOSURE OK: 84 records verified` rc 0,
  `CLOSURE TEETH OK (61/61 fail closed)` rc 0. Worktree `agent-a7ec05a2f21b29494`.
- **render-judge — AT GO, Tier A+ discharged** (indep-2 NO-GO -> fixed ->
  indep-3 GO). `RENDER OK` rc 0, `REGRESSION OK` rc 0. Worktree
  `agent-a01a4b6eba976b930`.

**FIRST ACTIONS ON RETURN (in order):**
1. Rule on menus: **cap it** (§3 CAPPED record naming its recurring class) **or
   let it run**. Everything else queues behind this.
2. Then ONE batched merge + ONE re-pin pass: **5 drifted rows**, not the 1 the
   writer reported (`riglib.sh`, `check-device-target.sh`, `check-device-foh.sh`,
   `mlfk-foh.sh` = reviewed-go; `check-device-fullgame.sh` = arc-in-flight).
   Cites must use `.loop/review-mexit-r7{g,o}.log` (both terminal anchored GO)
   and must NOT carry forward the brace glob in the current mlfk-foh.sh cite.
3. Note **`check-device-fullgame.sh` is arc-in-flight (B9 OPEN)** — a green M4
   gate needs that arc closed regardless of these three lanes.
4. **Build the successor rig BEFORE attempting the M4 gate**: ~232 lines
   (`foh_sysmenu_open` ~157, VS-finish ~75) have **never executed anywhere**,
   and `verify_m4.sh` cannot reach them by construction (`sysOk` needs
   `!shotsDir`; every committed leg sets `--shots-dir`). Fix the unbounded
   release drains first or that rig will HANG rather than fail.

**TWO OWNER DECISIONS PENDING:**
- **The reviewer-harness change**, now motivated by FIVE independent failure
  modes and proposed INDEPENDENTLY by two lanes: the reviewing harness should
  emit its own verdict artifact carrying arc id + reviewed scope, so arc closure
  is answered by a producer-written record instead of a reader's inspection.
- **The TIME!/GAME! one-frame HARD RULE 5 deviation** (`foh_dev.c:3300`) —
  ratify (D14 precedent) or fix. Deliberately NOT fixed: editing it would move
  bytes both GO verdicts cover.

**Time-bounded chore:** render-judge's retention archive is at **3/4 (335 MB)**;
the run after next FAILS CLOSED by design. Set `REGRESSION_ARCHIVE_MAX=8` or
prune deliberately.

**Driver error ledger grew by 2 today (both C35 "unsound-negative"):** #6 the
`/tmp` vs `$TMPDIR` rig-lock path, #7 detecting live lane workers by process
cwd (which cannot see them). Plus two more instrument failures caught in the
act: `find -newermt '-12M'` silently matching nothing, and a `shasum`/`awk`
`while` loop reporting **all 89 manifest rows drifted** because both binaries
were not found. **Standing rule: a suspiciously TOTAL result — everything
failed, nothing found — indicts the instrument before the finding.**

### 2026-07-30 18:15 — menus is at r21 (not r20), its Tier A+ attempt is VOID, and the driver is now SOLE operator

**Sole-operator now.** Chase closed the cmux sessions; driver verified `ps -p
64006` empty, menus worktree idle, zero `codex exec` producers. Root cause of
those sessions: **a Claude Code bug auto-starts a loop when a session is
RESUMED**, before the user picks a resume option. Not deliberate parallel lanes.

**Menus ran round 21 BEFORE the cap ruling existed — no violation.** Measured
mtimes: r20 13:30 · r21 prompt 14:25 · **r21 log 14:41** · tierA-r7 prompt
15:21 · tierA-r7 log 15:25 · cap ruling 17:46. So:
- **r21 is a VALID round** (351,038 B, 2 anchored `VERDICT: NO-GO`,
  `CODEX_RC=0` :5162). The arc is at **21 consecutive valid NO-GOs (~2.6x
  cap)**. The cap ruling now binds at r21: **do not open round 22**, and fix
  **r21's** Medium+ (r21 supersedes r20).
- **The Tier A+ attempt is VOID and discharges nothing.**
  `.loop/menus-p2-tierA-r7-20260730T222250Z.log` is **487 bytes**, contains
  **no `## Findings` and no `VERDICT:` line** — only the reviewer's streamed
  preamble — and ends `TIERA_RC=0`. **A zero exit code on a verdict-less
  artifact is not a pass.** Third instance today of "plausible artifact, no
  verdict" (cf. the 1.9 MB codex log at `CODEX_RC=1` whose only `VERDICT:`
  lines were prompt echoes; the device log 44.4% NUL containing
  `DEVICE TARGET CONFORMS` whose run exited `TARGET_RC=1`). **Tier A+ remains
  fully OWED**; prompt `.loop/menus-p2-tierA-r7-prompt.md` is reusable.

Both facts were appended to `.loop/OWNER-RULING-20260730-MENUS-CAPPED.md`
(synced into the menus worktree) — the file the R1 writer reads first.

**C35 INSTANCE #5, driver's own, caught this turn.** A check written as
`ls -lt --time-style=full-iso … | grep … | head || stat …` printed NOTHING and
the driver briefly concluded r21/tierA-r7 "do not exist". Cause: BSD `ls`
rejects `--time-style`, but the pipeline's exit status comes from `head`, which
SUCCEEDED — so the `||` fallback never fired and the failure was invisible.
**Rule extended: a `||` fallback after a PIPELINE guards the last command's
status, not the first's.** Caught only because a second, differently-built check
contradicted it. Five instances today, one mechanism: *a check that cannot
observe what it was asked about, reporting absence.*

**Lanes in flight (driver-dispatched, sole operator):** R1 menus capped closure
(worktree `agent-a063ab1a97c0d6b57`) · R4 reviewer-harness verdict artifact
(worktree `agent-ad3238e43fc13278b`). R2 merge unblocks when R1 lands.

### 2026-07-30 20:25 — MERGE IN PROGRESS: all 4 lanes reconciled into the main tree; re-pin measured at 17 rows

**Merged so far (driver, fingerprint-verified):** render-judge (9 `port/gfx/`) +
cite-closure (3 new `port/sim/device/`) copied in, **12/12 byte-identical**.
Then a writer reconciled menus against match-exit's uncommitted work — this
could NOT be a copy: **7 files were modified by BOTH lanes** and menus' worktree
predates match-exit, so copying would have silently reverted GO'd work.

**Reconciliation audit (driver-measured, all held):** zero conflict markers ·
`ctl_style.c` present in **6** link recipes · atomic landing live
(`flashlcancel=%d walljump=%d` at `foh_dev.c:2227`) · `EMIT_PENDING_DEV=""` at
`check-judge-regression.sh:2055` · the stale judge sha `32bbc8b8…` is GONE ·
`judge-foh-trace.js`, `normalize-foh-trace.js`, `dump-judge-grammar.js`,
`judge-grammar.frozen.txt` all **byte-IDENTICAL** to the menus worktree (the
reviewed bytes were not edited, so the Tier A+ discharge still binds).

**Two real defects the reconciliation caught that neither lane's arc had:**
1. menus pinned `judge-foh-trace.js` at a **STALE sha** (`32bbc8b8…`, 6
   occurrences across 3 device check scripts) while the reviewed file carries
   `e709c03b…b878`. Their own twin-pin greps would have matched 0 and hard-failed
   on device.
2. match-exit's own untracked witness `check-mexit-reentry.sh` links `foh.c` and
   died with `ld: _ctl_style_get/_ctl_style_set not found` — the **same
   BLOCKER-9 class**, in a file the menus lane never saw. Fixed (`:549`).
   **Flag for whoever reviews match-exit: that link-recipe line was never in its
   review scope.**

**DRIVER MEASUREMENT CORRECTED — my `node_modules` claim was WRONG.**
I reported `.gitignore` has zero `node_modules` entries and warned `git add -A`
would commit it. It IS ignored — by **`oracle/harness/.gitignore:1`**, a NESTED
ignore file my root-level scan never looked at; `git check-ignore -v` confirms.
Same shape as C35: **a check scoped too narrowly, reporting absence.** Adding
files explicitly at commit remains right, but the stated reason was false.

**RE-PIN MEASURED: 17 drifted rows** (72 of 89 still match; 0 missing) — the
estimate went 1 -> 5 -> ~10 -> **17** as work merged, which is why it is measured
each time and never carried forward. 16 are `reviewed-go`; 1 is
`check-device-fullgame.sh` (**arc-in-flight**, B9 OPEN — stays arc-in-flight).
Plus NEW rows still owed for files not yet in the manifest: `port/foh/foh.h`
(now a judge DECISION INPUT via `#define FOH_NETPLAY`),
`check-judge-regression.sh`, `dump-judge-grammar.js`,
`judge-grammar.frozen.txt`, `judge-domains.authored.txt`.

**CITE FORMS (C11) — note two arcs have NO GO to cite.** match-exit rows cite
`.loop/review-mexit-r7{g,o}.log` (both terminal anchored GO); render-judge rows
cite `.loop/review-134-indep-3.log` (GO). **menus and cite-closure closed
CAPPED**, so their rows take the established driver-capped form already in the
manifest (`…-arc-CAPPED-CLOSED-driver-accepted-AGENT-LOG-<date>+rounds-<logs>`),
NOT a GO cite. Do not invent a GO for a capped arc.

**NOT COMMITTED.** A driver cold `bash port/sim/check-sim.sh` is running now —
the checksummed sim plane is the project's core invariant and both lanes touched
`port/sim/` files. **No commit until it prints `SIM CONFORMS` rc 0.**

### 2026-07-30 21:00 — MERGE + RE-PIN COMMITTED. All 4 lanes are in. Read this first.

**`bed73d6`** — 4-lane merge (71 files): match-exit (GO), render-judge (GO),
cite-closure (CAPPED), menus (CAPPED), **plus the LAUNCH producer/judge atomic
landing** (`foh_dev.c:2227` gains `flashlcancel`/`walljump`;
`EMIT_PENDING_DEV=""`; caveat deleted — all one change, so judge and producer
cannot disagree).
**`14c394b`** — manifest re-pin: **17 drifted rows** + the iter-132 **brace glob
removed** + anchor recomputed to `0a5f5957…88ee`.

**Driver verification that gates these commits (all run by the driver, cold):**
- `bash port/sim/check-sim.sh` -> **`SIM CONFORMS`, rc 0, 8/8 goldens,
  3600/3600 frames exact.** The checksummed core survived a 71-file merge — this
  was the one invariant not delegated.
- Manifest drift recomputed over all 89 rows -> **DRIFTED=0, missing=0**.
- Anchor self-consistent (`MANIFEST_SHA256` == sha256(manifest)).
- `bash .loop/cite-teeth.sh` -> **`CITE TEETH OK (17/17 fail closed)`**, rc 0.

**THE MERGE WAS NOT A COPY — remember this shape.** menus' worktree predated
match-exit's uncommitted work; **7 files were edited by both** and were
reconciled as true unions. A copy would have silently reverted GO'd work with no
conflict marker. Cross-lane file-list comparison is what caught it; worktree
isolation does NOT imply independence.

**Two defects found only by reconciling lanes against each other** (neither
arc's own review found them): menus pinned `judge-foh-trace.js` at a **stale
sha** (6 sites, 3 device scripts) whose twin-pin greps would have matched 0 and
hard-failed on device; and match-exit's own `check-mexit-reentry.sh` failed to
link (`_ctl_style_get`/`_ctl_style_set` undefined) — the same BLOCKER-9 class in
a file menus never saw. **Its link-recipe line was never in match-exit's review
scope — flag it if that lane is ever re-reviewed.**

**Cite forms:** match-exit rows cite two terminal-GO logs; **menus and
cite-closure closed CAPPED and have NO GO**, so their rows use the documented
rule-3 form (`CAPPED-CLOSED` + `AGENT-LOG-2026-07-30`). **Never invent a GO for
a capped arc** — that is the exact laundering cite-closure exists to prevent.

**GATE STATUS: still refuses, for TWO CORRECT REASONS** — (1)
`check-device-fullgame.sh` is `arc-in-flight` (**B9 arc still OPEN**), (2) every
device leg is unrunnable (**device OFFLINE**, `adb devices` empty, confirmed not
an adb-server hiccup). The pins themselves are now honest, which was the part
the driver could fix.

**STILL OWED, in order:**
1. **New manifest rows** for judge decision inputs not yet pinned: `port/foh/foh.h`
   (became one via `#define FOH_NETPLAY`), `check-judge-regression.sh`,
   `dump-judge-grammar.js`, `judge-grammar.frozen.txt`,
   `judge-domains.authored.txt`. Deliberately NOT tail-appended — adding
   producer rows changes what the gate governs.
2. **R3 successor rig** (fix the unbounded drains FIRST or it HANGS instead of
   failing) — must precede any M4 gate attempt: ~232 lines still never executed.
3. **B9 arc** must close before a green gate is even possible.
4. Two work-order patches now landable (`foh_dev.c` is no longer single-writer):
   `.loop/menus-p2-device-workorder-audio.md` §8 (`foh_audio_bus_push`) and §5
   (the four `ctl_style_set`/`ctl_mod_on_r_set` persistence lines).
5. Device-only: `check-device-persist.sh` unrun; all device legs deferred.

**In flight:** R4 (reviewer verdict artifact) at review round 3, in worktree
`agent-ad3238e43fc13278b` — and it is DOGFOODING its own design, emitting
`.loop/arc/r4-verdict-artifact/r003-codex-primary-<ts>-<hash>.{log,verdict,decision}`
for its own rounds.

### 2026-07-30 22:40 — PAUSED at a clean checkpoint (owner request). Device is BACK.

**Main tree is at a clean boundary — nothing half-done here.** HEAD `a78c074`;
3 commits today (`bed73d6` merge, `14c394b` re-pin, `a78c074` recovery page);
`git status` shows only the 4 deliberately-excluded untracked files
(`.tokensave/`, `AGENTS.md`, `CLAUDE.local.md`, `scratch-b8.js`).

**DEVICE IS ONLINE AGAIN** (owner re-plugged, 22:38): `12c00003237f5528`
present, `/mnt/disable_frontend` ABSENT, gmenu2x running, no orphan `mlfk`
processes, no stale rig lock at `$TMPDIR/mlfk-rig-*.lock`, MemAvailable
42,628 kB. **Every device leg deferred today is now runnable.**

**Two lanes were mid-arc when the pause was called and were NOT killed** —
stopping them would discard substantial in-flight work, and both write their own
progress notes. They are harness-tracked, so they still report when done:
- **R3 successor rig** — worktree `agent-a55523774731719ba`. Editing
  `foh_dev.c`, `foh_pause.{c,h}`, `gfx_overlay.c`; building the input scripts
  that drive the two never-executed arms (`sysfoh.fks`, `fin.fks`,
  `nav-noshot.fks`). **Now that the device is back, its deferred device legs are
  runnable — but it was briefed while the device was offline, so it will
  register them as deferrals unless told otherwise.**
- **R4 reviewer verdict artifact** — worktree `agent-ad3238e43fc13278b`, review
  round 4. Deliverables on disk: `port/review/{review-harness.sh,
  check-review-artifact.sh,arc-closure.sh,reviewers.sh,FORMAT.md,specimens/}`.
  It is DOGFOODING its own format on its own rounds.

**RESUME HERE (ordered):**
1. Let R3 and R4 report; audit both against disk (three self-reported completion
   claims were contradicted by filesystem audit today — audit every one).
2. **Re-run the device legs now unblocked:** `check-device-persist.sh` (never
   run), plus the device arms of `check-device-foh.sh`,
   `check-device-target.sh`, `check-device-fullgame.sh`.
3. New manifest rows for the unpinned judge decision inputs (`foh.h`,
   `check-judge-regression.sh`, `dump-judge-grammar.js`,
   `judge-grammar.frozen.txt`, `judge-domains.authored.txt`).
4. **B9 arc must close** — `check-device-fullgame.sh` is still `arc-in-flight`,
   so a green M4 gate is impossible until then.
5. Then, and only then, an M4 gate attempt — **after** R3's rig lands, never
   before (~232 lines still never executed; the gate cannot reach them).

### 2026-07-31 — CURRENT TRUTH. Loop resuming. Read this block first, then §rulings.

**HEAD `109645c`.** Main tree CLEAN except 4 deliberately-excluded untracked
files (`.tokensave/`, `AGENTS.md`, `CLAUDE.local.md`, `scratch-b8.js`).
**DEVICE IS ONLINE** (`12c00003237f5528`, frontend running, no danger marker, no
stale rig lock, MemAvailable ~42 MB).

**Commits this cycle:** `bed73d6` 4-lane merge · `14c394b` manifest re-pin (17
rows, brace glob removed) · `a78c074` recovery page · `ebe8f35` pause checkpoint
· `109645c` R3 successor rig.

**ALL SIX LANES ARE DONE.** match-exit (GO) · render-judge (GO) · cite-closure
(CAPPED) · menus (CAPPED) · **R3 successor rig (CAPPED, MERGED)** · R4 reviewer
verdict artifact (NOT GO — held, see below).

**R3's headline, driver-reproduced cold:** executing the never-run arms found a
**shipping crash** — every natural VS timeout would have aborted the game
(`glyphs: font 0 has no glyph '-'`, `SIM FATAL frame 210`; the finish frame
carries a NEGATIVE `matchTimer` and the atlas has no minus sign). Fixed with a
one-tick, finish-frame-permissioned guard; tooth T6 requires the crash back.
**This empirically vindicates owner decision 4 (rig BEFORE the gate).**

**Driver cold verifications on `109645c`:** `LIVE ARMS OK (sysmenu=4 vsfinish=1
drains=3 teeth=15)` rc 0 · `SIM CONFORMS` rc 0, 8/8 goldens, 3600/3600 exact ·
all 8 touched files measured against the manifest = **0 pinned producers**, no
re-pin owed.

### THE ONE OPEN OWNER DECISION — R4's tier

R4 built `port/review/` (~3,500 lines: producer, judge, 144-tooth done-check,
FORMAT.md, 2 live specimens) + PROCESS §3/§11 amendments. Driver cold-verified
`REVIEW ARTIFACT TEETH OK (144/144 fail closed)` rc 0. It fails closed on BOTH
real 07-30 specimens and on a single-reviewer §11 fallback.

**It is NOT merged, and must not be, because it flagged its OWN arc as
misclassified:** `arc-closure.sh` IS a judge, so PROCESS §3 tiers it UP to
**Tier A+** (independent second reviewer, NOT codex + archived byte-identity
regression). All 7 rounds were sealed as tier A. **Sealed artifacts cannot be
re-tiered — that is the point of sealing.**

**Driver recommendation: a FRESH ARC at the correct tier**, not a re-tiering
ruling. Re-tiering by decree is exactly the laundering this tool exists to
prevent, and it would be the worst possible first exception. Consistency
precedent from 07-30: menus was sent back for an unmet Tier A+, render-judge's
Tier A+ was not waived, and match-exit was sent back for the sibling
two-reviewer rule. **Waiving it for the anti-laundering tool itself would be
indefensible.**
R4's two registered residuals (output-quiescence race — outcome is a REFUSAL,
never a false GO; and unenforced prompt semantics) are recommended ACCEPTED as
registered.

### RESUME ORDER (the loop should work this top-down)
1. **Dispatch R4's Tier A+ independent reviewer** (fresh Opus 5, NOT codex, NOT
   R4's own session) + the archived byte-identity regression — R4's own
   `review-harness.sh regression --tier A+` can record it. Then merge R4.
2. **Run the device legs that were deferred all cycle — the device is BACK.**
   `check-device-persist.sh` (NEVER run), plus the device arms of
   `check-device-foh.sh`, `check-device-target.sh`, `check-device-fullgame.sh`.
   R3's capped-closure floor is explicitly three device-only facts (headless
   present is a no-op, headless audio starts no callback thread, a daemon-owned
   container escapes a process-group kill) — only hardware settles them.
3. **New manifest rows** for judge decision inputs not yet pinned: `port/foh/foh.h`
   (became one via `#define FOH_NETPLAY`), `check-judge-regression.sh`,
   `dump-judge-grammar.js`, `judge-grammar.frozen.txt`,
   `judge-domains.authored.txt`. Adding producer rows changes what the gate
   governs — deliberate change, not a tail-end append.
4. **B9 arc must CLOSE** — `check-device-fullgame.sh` is still `arc-in-flight`,
   so a green M4 gate is impossible until it does.
5. Two work-order patches now landable (`foh_dev.c` is no longer single-writer):
   `.loop/menus-p2-device-workorder-audio.md` §8 (`foh_audio_bus_push`) and §5
   (the four `ctl_style_set`/`ctl_mod_on_r_set` persistence lines).
6. **Only then** an M4 gate attempt, followed by Chase's acceptance playthrough.

### STANDING DISCIPLINE (earned the hard way this cycle — do not relax)
- **Audit every lane report against disk.** Three self-reported completion
  claims were contradicted by filesystem audit; one claimed 12 of 13 items done
  when 1 was real, including two device measurements that never happened.
- **C35 unsound-negative — 5 instances, all driver-made, all caught.** A check
  that cannot observe the thing reports absence: `/tmp` vs `$TMPDIR` lock path ·
  process-cwd worker detection · `find -newermt '-12M'` (use `-mmin`) · a
  `shasum` loop whose binaries were missing (reported ALL 89 rows drifted) · a
  root-only `.gitignore` scan missing a NESTED ignore file. **A suspiciously
  TOTAL result indicts the instrument first.**
- **A merge is not a copy.** Worktree isolation does NOT imply independence —
  7 files were edited by two lanes and a copy would have silently reverted
  reviewed work. Always diff lane file-lists against each other first.
- **Never invent a GO for a capped arc.** Capped arcs cite `CAPPED-CLOSED` +
  `AGENT-LOG-<date>`.
- **Never run timing-sensitive rigs concurrently** (live-arms pins a 2500 ms
  hold to ±200 ms; contention fails it for the wrong reason).

### 2026-07-31 (late) — CURRENT TRUTH. Pausing at the next lane boundary. Read this block first, then §rulings.

**HEAD `7546485`.** Main tree carries the device lane's IN-PROGRESS edits (4
files, uncommitted, see below) plus the usual 4 excluded untracked files.
**DEVICE ONLINE** (`12c00003237f5528`); it is being driven by a live lane.

**Commits this session (8):** `bed73d6` 4-lane merge · `14c394b` manifest re-pin
· `a78c074` recovery page · `ebe8f35` pause checkpoint · `109645c` R3 successor
rig · `df72146` device legs + armv7 cross-compile fix · `5619645` R4 second
Tier A+ NO-GO · `7546485` owner ruling: R4 splits.

### WHAT LANDED TODAY THAT MATTERS

1. **A shipping crash, found by executing never-run code, now fixed AND
   witnessed on real hardware.** A natural VS timeout rendered a NEGATIVE match
   timer (`-1:-0.00`) into a font atlas with no `-` glyph -> `SIM FATAL frame
   210`. **Every natural VS timeout on device would have aborted the game.**
   The device leg now passes: `DEVICE FOH OK (… vsfinish=1 … fbwit=23
   p99=13.995ms skips=0 …)` rc 0. **`vsfinish=1` and fbwit 17->23 are the proof
   it actually ran on hardware.**
2. **An armv7 CROSS-COMPILE BREAK that blocked EVERY device leg.**
   `foh_render.c` failed SDK gcc `-Werror=format-truncation`; those bytes had
   only ever been compiled by host clang. **CLASS: host-clang-clean is not
   evidence of device-buildable, and no host check can see it.**
3. **`check-device-persist.sh` PASSED for the first time in its existence**
   (authored iter 100, never once past step 3/10):
   `PERSIST OK (sessions=2 powercycle=reboot bootid=PRE!=POST … roundtrip=byte-exact
   record=00:14.50 … teeth=29)` rc 0 — a REAL reboot, boot-id change proving the
   power cycle, save data byte-exact across it.
4. **2 of R3's 3 device-only floor facts SETTLED** — pixels provably reach the
   scanned-out framebuffer (`eq=1` at `yoff=0`, 17 witnesses), and audio is real
   on device (host `0 callbacks / 274 steals` vs device `6585 callbacks / 0
   steals`, with **282 voice starts identical**).

### LANES IN FLIGHT (both will report; PAUSE WHEN THEY DO)
- **Device lane** — MAIN TREE, holds the hardware lock. Round 8, one hardware
  re-run per review round. Uncommitted: `check-device-foh.sh`,
  `check-device-persist.sh`, `decode-pb-glyphs.js`, `foh_dev.c`.
  **WATCH: `foh_dev.c` gained ~23 lines though the brief said the arm binary
  needed no change — require its justification in the report.**
- **R4 split lane** — worktree `agent-ad3238e43fc13278b`. Executing the owner's
  split ruling. `arc-report.sh` has appeared (the demotion is going in at the
  command-name level, not as a disclaimer).

### R4 — SETTLED BY OWNER RULING, do not re-litigate
Three independent adversarial passes each found fresh false-GREEN paths into the
judge. Reviewers' through-line: **each fix closed the measured instance and left
the class — HARD RULE 8's hierarchy inverted three times.** Plus a measured
FALSE SAFETY DISCLOSURE (§7 claimed the quiescence gap "produces a REFUSAL";
measured 3/3 it produces a self-consistent GO). Owner ruled: **keep the
producer, demote the judge to a diagnostic, remove PROCESS.md's
sanctioned-answer sentence.** Closure stays a human judgement.

### RESUME ORDER
1. Audit both lanes' reports against disk, then commit them.
2. **Re-pin the manifest** — the device lane touches `check-device-foh.sh` (a
   pinned producer) and others; drift must be re-measured, NOT carried forward
   from any earlier count (it went 1 -> 5 -> ~10 -> 17 this cycle).
3. New manifest rows for unpinned judge decision inputs (`foh.h`,
   `check-judge-regression.sh`, `dump-judge-grammar.js`,
   `judge-grammar.frozen.txt`, `judge-domains.authored.txt`).
4. **B9 arc must CLOSE** — `check-device-fullgame.sh` stays `arc-in-flight` and
   red on its `skips == 0` bar. NEW attribution from today: the skip follows
   per-leg **MMC interrupt count**, not workload (g06 2820 mmcirq + the only
   `pswpin=7` -> 7 skips; baseline 198-485), `low_bat_check` quiesced in both
   passes, so this is a **SECOND stall source** distinct from the closed iter-74
   class. p99 headroom is thin: **439-566 µs**.
5. Two work-order patches now landable (`foh_dev.c` no longer single-writer):
   `.loop/menus-p2-device-workorder-audio.md` §8 and §5.
6. **Only then** an M4 gate attempt, then Chase's acceptance playthrough.

### STANDING DISCIPLINE (do not relax — all earned this cycle)
- **Audit every lane report against disk.** Three self-reported completion
  claims were contradicted this cycle, including two device measurements that
  never happened.
- **C35 unsound-negative, 5 driver instances, all caught:** `/tmp` vs `$TMPDIR`
  lock path · process-cwd worker detection · `find -newermt '-12M'` (use
  `-mmin`) · a `shasum` loop with missing binaries (reported ALL 89 rows
  drifted) · a root-only `.gitignore` scan missing a NESTED ignore file.
  **A suspiciously TOTAL result indicts the instrument first.**
- **A merge is not a copy.** Worktree isolation does NOT imply independence.
- **Never invent a GO for a capped arc.**
- **Never run timing-sensitive rigs concurrently.**
- **Liveness is a process + log-tail check, not a single mtime window** — a lane
  mid-adb-push looks dead for minutes.

### 2026-07-31 — PAUSED AT A CLEAN CHECKPOINT (owner request). READ THIS FIRST.

**HEAD `ef31e53`.** Main tree CLEAN except the 4 permanently-excluded untracked
files. **9 commits this session.** Both in-flight lanes reported and were
audited; nothing is half-done in the main tree.

## ⚠️ TWO THINGS NEEDING A HUMAN

**1. THE DEVICE IS OFF THE USB BUS and did not return.** No `adb devices`, no
`funkey` in `system_profiler SPUSBDataType`, after ~20 min of retries and an
adb server restart. **It needs a physical check.**
**C24 RISK, stated honestly:** the lane parked the frontend at ~16:23; the
deadman's 900 s window expired ~16:38. If the device stayed powered it
self-unparked and is fine. **If it lost power while parked, `/mnt/disable_frontend`
persists on the SD card and the frontend will refuse to start on every boot.**
That is NOT a brick — it is one file. Fix: `adb shell rm /mnt/disable_frontend`,
or pull the SD and delete it. HOST side is verified clean: no rig lock, no lane
processes, and **zero FunKey SDK containers** (the 5 running containers are the
owner's own — postgres/redis/adp-dev-api/floci — checked, NOT killed, because a
blind sweep once took out 4 unrelated ones).

**2. R6's FINAL BYTES HAVE NOT HAD A DEVICE RUN.** Committed at `ef31e53` with
this caveat explicit. Last device-green: foh = the r6-fix state, persist = the
r5-fix state. The delta is r7 hardening only (flat-panel foreign-ink guard +
tooth + deadman-pid assertions), host-verified (`bash -n`, `node --check`,
host+ARM `-Werror`). **The two runs on final bytes died on ADB TRANSPORT LOSS,
not on any assertion.** When the device returns: re-run `check-device-foh.sh`,
then `check-device-persist.sh`.

## WHAT THIS SESSION ACHIEVED

- **A shipping crash found, fixed, and WITNESSED ON HARDWARE.** A natural VS
  timeout rendered a negative match timer into a font atlas with no `-` glyph
  (`SIM FATAL frame 210`) — **every natural timeout on device would have aborted
  the game.** Now: `device vsfinish: expiry frame 210 == twin, banner byte-exact
  + decodes 'TIME!', fb-witnessed, hold 2526 ms, rc 0`, with
  `W 210 finish-banner yoff=0 eq=1` proving it reached the SCANNED-OUT page, and
  zero `SIM FATAL` bytes.
- **An armv7 CROSS-COMPILE BREAK that blocked EVERY device leg** (`foh_render.c`
  under `-Werror=format-truncation`; those bytes had only ever met host clang).
  **CLASS: host-clang-clean is not evidence of device-buildable.**
- **`check-device-persist.sh` PASSED for the first time in its existence** —
  authored iter 100, never past step 3/10. Now all ten, four green runs, with a
  REAL reboot (`bootid:PRE!=POST`) and a byte-exact save roundtrip across it.
- **2 of R3's 3 device-only floor facts SETTLED** — pixels provably reach the
  scanned-out fb; audio is real on device (host `0 callbacks / 274 steals` vs
  device `6585 callbacks / 0 steals`, **282 voice starts identical**).
- **4 lanes merged** + a 17-row manifest re-pin + the iter-132 brace glob removed.

## R4 — SPLIT DONE, AWAITS EXACTLY ONE REVIEW ROUND (do not merge before it)

Owner ruled the split (`7546485`); the lane executed it and the driver
cold-verified: `REVIEW ARTIFACT TEETH OK (189/189 bit)` rc 0, scope clean
(`M docs/PROCESS.md` + `?? port/review/`).
**The demotion is REAL, not cosmetic:** `arc-closure.sh` -> `arc-report.sh`;
output grammar is now `ARC REPORT … (DIAGNOSTIC — this tool does not decide
whether an arc is closed)` / `EVIDENCE DEFECT` / `REFUSED`; **the entire
closure-rule layer is deleted** — driver-verified the only surviving `maxround`
and `ARC CLOSED` strings are deletion-rationale COMMENTS (`:742`, `:30`), and
PROCESS.md's sanctioned-answer sentence is **gone (0 refs)**.
**Two unsound rules were DELETED rather than downgraded to observations** — round
ordering and §11 basis eligibility — on the correct principle that *an
observation derived from an unsound rule is still an unsound claim*. The report
now prints both timestamps and says `round order: NOT ESTABLISHED` /
`§11 basis eligibility: NOT DECIDED here`.
§7's FALSE SAFETY DISCLOSURE is corrected with the measured mechanism, and the
lane **self-retracted** a second overstated claim (§2(c)) it found while fixing
the first. Teeth 185 -> 189, none deleted; `expect_observed` **fails if the
report is SILENT** about a situation it cannot judge.

**TIER RULING (driver accepts the lane's argument): Tier A, ONE codex round.**
§3's A+ trigger is a judge/verify-surface change; this change *removes* the judge
surface — nothing gates on `arc-report.sh`, nothing outside `port/review/`
references it, and the failure mode A+ exists to catch (a mechanised green line
laundering a decision) is **structurally absent because there is no green line**.
A+'s obligation (2), a byte-identity regression on archived results, has no
referent: the archived results were closure lines deleted by design, so an
"old vs new" artefact would be fabricated — itself evidence A+ is the wrong tier.
**Point that one round at TOOTH-VACUITY specifically:** whether any
`expect_observed` substring is satisfied by a report that would print it anyway
(baseline-vacuity), whether §7 still understates anything, and whether any
surviving sentence claims authority the code no longer has.

## RESUME ORDER
1. **Physical check on the device**; clear `/mnt/disable_frontend` if the
   frontend does not start.
2. Re-run `check-device-foh.sh` then `check-device-persist.sh` on R6's final
   bytes (the only outstanding evidence debt).
3. R4: one Tier A round aimed at tooth-vacuity -> then merge (copy list is at the
   top of `.loop/r4-progress.md`, 34 files + the progress artifacts; it is the
   ONLY copy of round 7's decision channel).
4. **Re-pin the manifest** — `check-device-foh.sh` is row 20 (`reviewed-go`) and
   its sha is now stale. **Re-MEASURE drift; never carry a count forward** (it
   went 1 -> 5 -> ~10 -> 17 this cycle).
5. New manifest rows for unpinned judge decision inputs (`foh.h`,
   `check-judge-regression.sh`, `dump-judge-grammar.js`,
   `judge-grammar.frozen.txt`, `judge-domains.authored.txt`).
6. **B9 arc (R7)** — blocks a green gate. Today's attribution: the fullgame skip
   tracks per-leg **MMC interrupt count**, not workload; `low_bat_check` quiesced
   in both passes, so it is a **second stall source** distinct from the closed
   iter-74 class. p99 headroom **439-566 µs**.
7. The two work-order patches, then the M4 gate attempt, then Chase's playthrough.

## NEW GOTCHAS FROM THIS SESSION (do not rediscover)
- **Editing a running bash script corrupts it** (bash reads by byte offset); the
  killed run also left a daemon-owned SDK container alive.
- **`grep -a $'\x00'` is an UNSOUND NUL scan on macOS/zsh** — the empty pattern
  matches every line. C35 class, found in a lane's own instrument.
- **codex reads `.loop/` and echoes prior rounds' logs**, so a round log can
  contain EARLIER rounds' `## Findings` and `CODEX_RC=` mid-file. Require
  last-block extraction AND "sentinel is the final line AND producer dead".
- **Liveness is a process + log-tail check, not one mtime window** — a lane
  mid-adb-push looks dead for minutes.

### 2026-08-03 (latest) — B9 r5 came back NO-GO; all 7 findings FIXED on the host. DEVICE VERIFICATION IS OWED.

The detached round finished: `.loop/review-b9-r5-20260801.log`, `CODEX_RC=0`,
**VERDICT: NO-GO** (last anchored `## Findings` at line 8389) — 4 HIGH, 2
MEDIUM, 1 LOW. Every one is now fixed. The arc stays **arc-in-flight**: a
NO-GO round's fixes do not close an arc, and nothing below has run on
hardware.

**H1 — the two skip witnesses were never reconciled.** The app-log
`match_skips` and the timing artifact's `skips` were each independently
bounded by `SKIP_ALLOW_PER_LEG` and only the timing one fed `SKIPS_TOTAL`, so
12 legs at 8 app skips each — 96 — passed every predicate against a zero
ledger (reviewer-executed). NEW production judge `judge_skip_witnesses`
requires exact equality, called from `judge_leg` once both witnesses are in
hand. **This is sound at the source, not just plausible:** `foh_dev.c:3318`
does `matchSkips++` in the `else` arm of `if (!skip)` and `:3325` sets
`tim[f].skipped` immediately after — same event, equal by construction.
MEASURED on the archived 2026-07-31 pass: app == timing on all 12 legs
(m01 = 1/1, the other eleven 0/0), so the bar costs a genuine run nothing.
Teeth T3c/T3c2 pin both directions.

**H2 — the decision-bearing `.skips` write was `|| true`.** It now writes to
a temp and renames, and a failed write FAILS the leg. It is not a diagnostic:
the per-run cap sums it. Reader side: a missing file used to fold as `0`; it
is now `unknown`, counted in `SKIPS_UNKNOWN`, named in the ledger, and a
post-loop assert makes "all legs passed but some carry no judged count" a
hard failure.

**H3 — T3b was deterministically broken.** It called `made` on the stderr of
a command it expects to SUCCEED; `made` demands a NON-EMPTY file. MEASURED
here, not taken on faith: `node port/gfx/judge-render-timing.js` on the real
fixture → `rc=0 stderr_bytes=0`, so the old assertion failed 100% of the
time. Now asserts stderr is EMPTY. Its fixture also handles a base already at
the allowance by REMOVING a skip instead of constructing allowance+1 —
validated at base 0 (real g01), 3 and 8, row counts preserved.

**H4 — the M4 consumer could not accept a passing suite.** `FULLGAME_RE`
still demanded `skips=0 teeth=21` while the producer emits
`skips=N/allow12 teeth=<n>`. Updated, and the consumer now RE-ENFORCES the
bound itself (`(0|[1-9]|1[0-2])`) rather than trusting the printed `/allow12`.
CLASS FIX rather than a point fix: the terminal line is composed in ONE place
(`verdict_line`) and NEW tooth **T22** reads `FULLGAME_RE` out of
verify_m4.sh's OWN BYTES and proves compatibility in-run. **EXECUTED ON THE
HOST, 7/7:** accepts clean + at-bound; refuses over-bound, teeth-drift,
short-legs, pre-allowance and `[ATTRIB-ARMED]`.

**M1** — unmeasured legs are UNKNOWN and the ledger states its coverage
(`over M/12 judged legs`) instead of claiming a total across all 12; the raw
unjudged skip-row count is reported separately, never folded in.
**M2** — both stale rows re-pinned and all three anchors verified consistent:
fullgame `038ec87e…`, verify_m4 (normalized) `9ff4b01b…`, `MANIFEST_SHA256=`
`ed6f5b43…`. **LOW** — `.loop/b9/bench.c`'s pre-seeded-framebuffer claim
narrowed; `foh_render.c:2179` `rast_clear` wipes fb+ink first, so the seed is
dead — preexisting-destination and clip coverage belong to `primdiff.c`.

`TEETH_PIN` 22 → **25** (T3c, T3c2, T22); `FULLGAME_RE` re-pinned to match.

**VERIFIED ON HARDWARE (same day, device attached mid-session).**
`bash port/sim/device/check-device-fullgame.sh` -> `FULLGAME CONFORMS 12/12
(render+sfx+music live; live-ai=g07,g08,m01,m02 p99=16.562ms skips=0/allow12
underruns=0 starves=0 presentfails=0 teeth=25)`, **rc 0**
(`.loop/fullgame-b9r5-verify4-20260803T174600.log`). The REAL terminal line
was then checked against `FULLGAME_RE` extracted from verify_m4.sh's bytes:
MATCHES — H4 is closed end to end, not just synthetically. T3b, T3c and T22
all executed green on the device.

**A SECOND DEFECT, FOUND ONLY BY RUNNING IT: the ratified allowance was
INOPERATIVE.** Run 1 went red 10/12 — g01 and g04 each skipped exactly 1
frame, cleared the per-leg allowance, then died on `rendered 3599 != 3600`.
`judge_timing` still asserted `rendered == frames`, and a skipped frame is by
definition not rendered, so `skips <= 8` and `rendered == frames` are
contradictory: **no skip could ever pass, and the 2026-08-01 ratification
could not do what it says.** Defect originates in `812059a` (the allowance
commit), NOT in the r5 fixes; the r5 reviewer could not see it because it
only manifests when a leg actually skips, and the bound itself was measured
while the bar was still `skips == 0`. REPAIRED to frame conservation
`rendered + skips == frames`. **NOT a loosening** —
`judge-render-timing.js:277` already enforces exactly this
(`render.length + skips !== expected` -> die), so the shell assert is a
redundant restatement of a law the FROZEN judge owns; the fixed form also
catches a witness disagreement the old one could not see. T3b would have
died on this too, so the H3 fix alone was necessary but not sufficient.
**This is the one edit in this change that touches a PASS CONDITION and it is
owner-overrulable.**

**STANDING RISK, MEASURED AND WORSE THAN THE PAGE SAYS: p99 headroom is
108 µs.** Worst leg s01 16.562 ms vs the 16.670 ms budget (107,958 ns). This
page's standing figure is 439-566 µs and run 2 of this session measured 744 µs
(m01 15.926 ms) — so p99 swings ~600 µs run to run and this run landed within
108 µs of a RED gate on timing alone, with no code change involved. Skips are
likewise nondeterministic (run 1 = 2 across g01+g04; runs 2/4 = 0), which is
exactly the noise the allowance exists to absorb.

**STILL OPEN.** The arc stays **arc-in-flight**: green bytes are not a closed
arc. **NEXT: (1) a fresh review round on the new bytes — it now owes coverage
of the conservation repair as well as the seven r5 findings, (2) only then
the five arc rows.** B11's wiring (per-flow leg declarations,
`judge_dev_shot` acceptance set, the 2-frame tooth, two device runs) is
untouched and still outstanding.

### 2026-08-01 (CHECKPOINT — paused at owner request mid-execution of the three ratified decisions)

All three decisions are ratified (§rulings) and IN EXECUTION. Resume state:

**Decision 3 — B9 skip allowance: CODE DONE + COMMITTED (`812059a`).**
`skips == 0` is now `SKIP_ALLOW_PER_LEG=8` / `SKIP_ALLOW_PER_RUN=12`, bound set
from 24 measured leg-runs. Count kept, print added on the PASS path and in the
terminal line, p99 bar untouched, run-total gate added (there was none). T3
re-armed to inject allowance+1 (it was about to go DEAD — one skip is now
inside the allowance), T3b ADDED to prove a within-allowance skip still gets
counted and persisted, `TEETH_PIN` 21 -> 22. Both fixtures are DELTAS on the
measured base, proven at base=0 and base=3.
**REMAINING:** the review round is STILL RUNNING (detached codex,
`.loop/review-b9-r5-20260801.log`, prompt `.loop/b9-r5-prompt-20260801.md`).
Read its LAST `## Findings` block — the log echoes other logs, so an unanchored
grep is unsound. On GO, close the B9 arc's **FIVE** manifest rows
(check-device-fullgame.sh, check-device-opk.sh, verify_m4.sh,
check-assets-expected.js, expected-assets.json — measured, the header saying
"seven" is stale), re-pin the fullgame hash, then recompute `MANIFEST_SHA256`
into verify_m4.sh. Closing at GO is BETTER than the ratified cap and costs
nothing extra, because this edit forced a round anyway.

**Decision 2 — R4: DONE AND MERGED (`86f836e`).** Verified ON THE MERGED BYTES
from the main tree, not just in the lane: `bash
port/review/check-review-artifact.sh` -> `REVIEW ARTIFACT TEETH OK (187/187
bit)` rc 0 (`.loop/r4-merge-verify.log`). 187 = 189 - 3 dead + 1 honest
baseline. The worktree `agent-ad3238e43fc13278b` is now redundant and may be
removed. What landed: Three dead teeth removed and replaced by ONE
honest `expect_footer_baseline`; T55 now requires rc 2 + its anchored
empty-scope refusal; T82 now requires rc 2 + the anchored third-path collision
message; the prompt envelope no longer tells reviewers that "the judge …
call[s] the arc closed"; the `[HIGH]` is registered in `FORMAT.md` §7 as a
STATED limitation (tamper-evident against accident, **not** against a
determined writer).
**MEASURED MISTAKE, fixed, worth keeping as a CLASS:** deleting the three dead
teeth dropped the suite to 184/187, because `expect_observed` had a SIDE
EFFECT — it GENERATED the report T89b/T94b/T96b then grep. They were not
vacuous teeth; they were vacuous ASSERTIONS attached to a load-bearing call.
Bare `arcreport` calls restored the generation with the false pass still gone.
**Before deleting a dead assertion, check what its setup was silently
providing.** NOTHING REMAINS on decision 2.

**Decision 1 — B11: generator DONE + COMMITTED (`1be6abd`), wiring NOT done.**
`port/foh/make-jitter-flow.js` is validated end-to-end: of the 3x3 product over
f01's two counted legs (releases 703 and 744), EXACTLY ONE — x0/y+1 — is
byte-identical to the real device shot; the other eight differ. All four
refusal paths proven by execution.
**REMAINING:** (a) per-flow leg declarations — f01 = `703,744` (from its own
"LEFT+DOWN clamps, then RIGHT x33 … UP x36" comment); f02/f05 still need their
legs read off their clamp comments, and NOTE f02's `I 980 L` -> `I 995 -` drag
*deliberately ends ON the clamp*, so it is likely absorbed and contributes no
distinct image; (b) judge-side acceptance set in `judge_dev_shot`, printing the
matched variant AND the count of DISTINCT images the set collapses to (that
count is the honest measure of how permissive it really is); (c) a tooth
proving a 2-frame offset still FAILS; (d) two device runs.

**Nothing is half-edited on disk.** The main tree is clean apart from
pre-existing untracked files; the only uncommitted work is R4's, isolated in
its worktree.

### 2026-08-01 (earlier) — THREE OWNER DECISIONS ARE NOW PENDING; the queue is decision-blocked, not work-blocked

Everything the driver could advance without a ruling has been advanced. What
remains all needs Chase:

1. **B11 fix direction** (new this turn, below) — recommendation (c).
2. **R4 disposition** — the post-split round came back **`VERDICT: NO-GO`**
   (`.loop/review-r4split-r1-20260801.log`), so the owner's *"one round, then
   merge"* precondition FAILED and the split stays unmerged. It found 3
   genuinely DEAD teeth (T89a/T94a/T96a assert boilerplate `arc-report.sh`
   prints unconditionally — they pass with the perturbation removed, three false
   bits inside the cited `189/189`), 2 teeth that pass against `true`/`false`,
   surviving present-tense authority language in the generated prompts, and one
   `[HIGH]`: **reviewer identity is forgeable by the writer the artifact exists
   to constrain** — a design ceiling, not a bug. Options + driver recommendation
   (2) in `fix_plan.md`. NOTE the shape difference from the three NO-GOs that
   shelved the judge: those were fresh false-GREEN classes each round
   (non-convergence); these are bounded and fixable.
3. **R7a — B9 companion decision** (unchanged, still open).

No further autonomous dispatch is warranted: the remaining items either edit a
judge, spend a review budget the owner set, or ratify a bar — all owner-visible
by construction. The manifest re-pin (item 3) is deliberately HELD rather than
taken, because B11's fix will edit `check-device-foh.sh` again and the row
should be pinned once, on final bytes, not twice.

### 2026-08-01 (later) — DEVICE EVIDENCE RUN: persist GREEN, foh BLOCKED on a NEW finding (B11)

Item 1 of the resume list was executed on the device (`12c00003237f5528`, healthy,
`/mnt/disable_frontend` absent, no orphans, no lock). Result is **split**:

- **`check-device-persist.sh` -> `PERSIST OK` rc 0** on `ef31e53`'s FINAL bytes
  (`sessions=2 powercycle=reboot bootid=PRE!=POST legs=5 pulls=4
  roundtrip=byte-exact teeth=30`; log `.loop/resume-persist-20260801.log`).
  **That evidence debt is DISCHARGED** — the caveat in `ef31e53`'s message is
  answered for persist.
- **`check-device-foh.sh` -> `DEVICE FOH FAIL` rc 1** at
  `shot f01-vs-g01/css` (log `.loop/resume-foh-20260801.log`). **Diagnosed, not
  re-run blindly. It is NOT a product regression and NOT the B9 skip class.**

**B11 (new, in `fix_plan.md`): the shot judge is stricter than the rig's own
determinism.** The device shot is **byte-identical** to a host twin whose
`I 708 U` hold runs **37 frames instead of 36** (`51e0d8db…c00217` both sides;
the committed 36-frame twin is `3abd60ff…5c7ec91`; evidence preserved under
`.loop/b11-shot-jitter/`). The flow file's own header documents the tolerance —
*"a hold can land +/-1 device frame off"* — and the slack did its job: the device
trace is **byte-identical** to the twin trace, every transition on the same frame.
What the slack does not protect is the resting pixel position: the hand moves
**3.84 px/frame** (D3), so ±1 frame relocates it, and the shot judge is byte-exact.

**B9 ruled OUT by measurement, not assumption:** the leg's applog reads
`976 ticks, 5 transitions, 5 shots, 0 render skips, 0 failed presents` (match
phase likewise 0). This is injector wall-clock jitter, not a game-side stall.

**Consequence for the plan:** this is `verify_m4.sh` leg [2]'s judgment too, so
the gate inherits the intermittency; the manifest re-pin (item 3) should NOT cite
a device-green foh run until B11 is decided, because there isn't one. Driver
recommendation is option **(c)** in fix_plan B11 — judge against the DECLARED ±1
tolerance as a three-way byte-exact acceptance set, printing which variant
matched — because it keeps byte-exactness and refuses the (b) "loosen the
comparator" path HARD RULE 3 forbids. **Not executed: every option edits a judge,
so it is owner-visible by construction.**

### 2026-08-01 — RESUME POINT. Device is BACK and CLEAN. Read §rulings first.

**HEAD `f823369`** (a commit follows this block). Main tree clean except the 4
permanently-excluded untracked files.

**DEVICE VERIFIED HEALTHY after the USB disconnect** — `12c00003237f5528`
present; **`/mnt/disable_frontend` is ABSENT** (C24 did NOT bite — it either
self-unparked via the deadman or never lost power while parked); gmenu2x
running; no orphan `foh_device`; `/tmp/mlfk` empty; no rig lock. **Nothing to
clean up.**

**NEW OWNER RULING (2026-08-01): ship before the 1-frame skip is fixed.** B9 is
deferred to the very end. See §rulings for the full text AND for the mechanical
consequence — deferring the fix does not by itself make the gate passable, and
the companion decision about the `skips == 0` bar is written there with the
driver's recommendation.

### FIRST ACTIONS ON RESUME (ordered)
1. **Re-run the two device checks on the final committed bytes** — the only
   outstanding evidence debt. Both prior attempts died on ADB transport loss,
   not on any assertion:
   `bash port/foh/check-device-foh.sh` then `bash port/foh/check-device-persist.sh`.
2. **R4 split — one Tier A review round**, aimed at TOOTH-VACUITY specifically
   (can an `expect_observed` substring be satisfied by a report that would print
   it anyway?), then merge. Driver already ruled Tier A and recorded why.
   Copy list is at the top of `.loop/r4-progress.md` — 34 files, and it is the
   ONLY copy of round 7's decision channel.
3. **Re-pin the manifest.** `port/foh/check-device-foh.sh` is row 20
   (`reviewed-go`) and its sha is stale after `ef31e53`. **RE-MEASURE drift; never
   carry a count forward** — it went 1 -> 5 -> ~10 -> 17 in a single day.
4. **New manifest rows** for judge decision inputs not yet pinned: `port/foh/foh.h`,
   `check-judge-regression.sh`, `dump-judge-grammar.js`,
   `judge-grammar.frozen.txt`, `judge-domains.authored.txt`.
5. **B9 companion decision** (see §rulings) — cap the arc + ratify the bounded
   skip allowance, so the deferral is executable rather than merely stated.
6. Two work-order patches (`.loop/menus-p2-device-workorder-audio.md` §8 and §5),
   then the M4 gate attempt, then **Chase's acceptance playthrough**.
7. **LAST, after everything ships:** fix the B9 skip properly.

## Live right now (updated: 2026-07-29 — HANDOFF PAGE. Latest AGENT-LOG entry = **iter 132**; latest COMMIT = `b44937b` (verification-debt lane merged). Phase: M4 mechanically PASSED; in owner-punch-list execution, NOT re-ratified)

**READ ORDER for a fresh context:** CLAUDE.md → this section → `fix_plan.md`
(the punch list, items A*/B*/C*/U*) → `docs/PROCESS.md` §11/§12 (the model
this loop now runs under) → `docs/MENU-SPEC.md` (menu contract) →
AGENT-LOG from iter 120 to EOF for narrative.

### 1. Where the milestone actually stands

**M4's mechanical gate PASSED at iter 119** (commit `fe885bd`):
`M4 GATE OK` twice — writer run + an independent driver cold run — with
87 producers pinned and review-closed, FULLGAME 12/12 on device, targets
2/2, flows 7/7, OPK frontend launch. The sentinel
`LOOP STOP: m4-complete` was written.

**Then Chase played it, twice (2026-07-27 and -28), and it is NOT
ratified.** Those playthroughs produced the whole punch list now in
`fix_plan.md`. So: the *gate* passed; the *game* did not yet satisfy the
owner. Do not treat M4 as closed.

**verify_m4.sh currently REFUSES.** Three independent reasons, all known:
(a) the driver wrote a **brace glob** — `.loop/c6-review-codex-r{1,2,3}.log`
— into the `mlfk-foh.sh` cite at iter 132, and the gate's own line
grammar rejects that form; (b) 7 rows remain `arc-in-flight` from the B9
render-headroom arc; (c) lane work is uncommitted. All three are in
flight in the M4-fix lane. **Nobody had re-run the gate between iters
120-132, which is how (a) went unnoticed.**

### 2. Live lanes (4) — all in worktrees per PROCESS §12

| Lane | Location | State |
|---|---|---|
| ~~**Controls**~~ | **CLOSED + MERGED — commit `490467e`** | Arc GO codex r6 after FIVE genuine NO-GOs. Three styles (Natural default, Normal, Box) + orthogonal Mod-shoulder swap, MLFKPERSIST3 v1/v2 migration. **flick=>smash / hold=>tilt VERIFIED against the real sim — the owner was right and the driver was wrong.** Lane retracted its own false "loses shield-drop" claim (band `[-0.70,-0.65)` clears). **NOT REACHABLE until C30 wiring lands (match-exit lane's files).** |
| **Menus** (gameplay/audio/controls/target-select) | worktree `agent-a063ab1a97c0d6b57` | 25 files, **r1–r9 done** (r9 NO-GO, 2 BLOCKERs). Re-dispatched for: **wire the audio sliders to the mixer**, the **C23 sound tooth**, and recording **D14** as a ratified deviation. |
| ~~**M4 fix / verification debt**~~ | **CLOSED + MERGED — commit `b44937b`** | Arc closed on **2 independent Opus 5 GOs** (codex replayed r2, aborted r4; grok aborted r3/r3b twice). Landed: C11 mechanical cite verification in `verify_m4.sh` `[0]` (89 producers, 140 artifacts, TERMINAL anchored `^VERDICT: GO$` per reviewed-go row) + `.loop/cite-teeth.sh` **17/17 fail closed** + B1 blend565 (53.3% of 708M triples wrong → 0, **no judged output moved, no re-freeze**) + PROCESS **C11** unadorned-verdict rule. Driver re-verified both pins, cold-ran gate+teeth pre- AND post-merge, content-fingerprinted 4/4 files. Reviewer verdicts PERSISTED to `.loop/review-vdebt-r{2,5,6}-opus.log` (they existed only under `/private/tmp`). Residuals registered **C25–C29** (C25 prior-closure laundering is the real remaining hole; C28 fg-IoU is not run-to-run reproducible — the mechanism behind U3's articles hole). U3 articles fix = **gfx/render lane**. |
| ~~**Roy research**~~ | **REPORT DELIVERED — `melee-chars-research` commit `b3af2db`** | **GO on translation conformance.** E1: stub 6th char passed the M0 gate 8/8 bit-exact, boot-RNG pin unmoved at 465. E2: mapping SYSTEMATIC (hitboxes 1.56%, frame data **0.00%**, attrs 3.23%) because **meleelight's author IS ikneedata.com** — values are a transcription of his own Melee tables, which already contain Roy (`Ry`, 168 rows). **Marth — what Roy clones — is cleanest at 0.65%; Fox, the charter's chosen slice, is the noisiest and alone misses the 2% bar.** No ISO needed; Dolphin escalation NOT required. Only unpriced work: Roy's 2D hitbox offsets. **Building Roy is an OWNER decision (charter decision 8) — not started.** |

**Lanes dispatched 2026-07-29 after the first three closed:**

| Lane | Where | Scope |
|---|---|---|
| **Match-exit closure** — CRITICAL PATH | **MAIN TREE** (its ~12 files are uncommitted there) | Its Tier A arc (**never run**), 5 device screenshots, C19 quit-to-VS, **A12c the FunKey SYSTEM menu — MENU must work EVERYWHERE, menus AND in-match** (lift `~/code_projects/ssb64-funkey-s/port/gfx/fk_menu.c`), **C30(a)+(b) the controls wiring**, then re-pin `mlfk-foh.sh` + re-derive the anchor. **SOLE manifest writer this cycle.** Holds the device. |
| **Render-judge hardening** | worktree, host-only | U3 articles per-feature plane (fg IoU scored **0.8961 vs a 0.88 bound with articles entirely removed** — 3rd instance of the class) + C28 (fg IoU not run-to-run reproducible: 7 distinct values on identical code). Must NOT touch the manifest or verify_m4.sh. |
| **Cite closure (C25/C26/C27)** | worktree, host-only | The structured closure record. **Design + generator + dry run ONLY — explicitly forbidden from editing the manifest or verify_m4.sh** (single-writer discipline); hands the driver an ordered application plan. |
| **Menus** | worktree `agent-a063ab1a97c0d6b57` | **REPORTED, NOT MERGED — arc at NO-GO (r12 unrun) + an UNMET Tier A+ obligation** (it changed `judge-foh-trace.js`/`normalize-foh-trace.js` = judge path, so PROCESS §3 needs a DIFFERENT reviewer + an archived old-vs-new byte-identity regression; neither exists). Delivered: audio sliders actually reach the mixer (bus is the RATIO `masterVolume/default` — SND1 gains already carry the defaults; byte-identical at defaults across 12 goldens + 8 tracks), C23 sound witness w/ 7 teeth, D14 recorded, C30(c)+C31 (`NORMAL` label -> "Classic", enum untouched). **Sent back**; also told to vacate `verify_m4.sh` and `foh_dev.c` (match-exit is sole writer) and hand those over as a patch-in-prose. `MLFKPERSIST4` collision RATIFIED — see fix_plan D-RULING. |

**Gate status right now:** `verify_m4.sh` refuses at the PIN stage because
the match-exit lane's uncommitted `mlfk-foh.sh` (`c5e14d50` vs HEAD
`0c37b702`) is a pinned producer. **Correct behaviour — the bytes are
unreviewed. Do NOT re-pin to quiet it.** `[0]` itself is green (89
producers, 140 cited artifacts) and `.loop/cite-teeth.sh` is 17/17.

**OWNER FLAG (driver did not decide unilaterally):** the research repo
commits `tables/*.csv` (~134 KB) whose `melee` column holds extracted
Melee values. Charter decision 9 says extracted DATA is gitignored build
output, never committed. These are analytical comparison/delta tables and
the repo has **no remote**, so nothing is distributed — but it is a
deviation from the ratified wording. Deleting them would destroy the
report's evidence, so they were left in place and flagged. One-line
remedy if the owner wants strictness: add `tables/*.csv` to `.gitignore`
and `git rm --cached` them.

Six further worktrees are already-merged leftovers and may be pruned
(PROCESS §12.1 permits pruning only after merged AND committed).

### 3. Owner rulings from 2026-07-29 (all binding)

- **Worktrees by default** for every feature lane; file-partitioning
  retired (PROCESS §12; reconciled against the protected HARD RULES and
  LOOP.md in §12.1 — lanes never commit, so the one-branch invariant
  holds and worktrees actually RESTORE LOOP.md's clean-tree guard).
- **Natural control scheme is the fresh-install default**; Chase's own
  device staying on Box after migration is explicitly fine.
- **Mod must be remappable** between the shoulders (L=Mod/R=shield ↔
  R=Mod/L=shield), switchable.
- **Wire the audio sliders** (do not revert the screen to a refusal).
- **D14 ratified**: the numeric volume readout stays (upstream has no
  digits — record as an owner-sanctioned DEVIATION).
- **Game pause overlay approved as-is**; only addition is C19
  (quit → VS/character-select screen).
- **MENU/HOME = the FunKey SYSTEM menu**, copied from Chase's own ssb64
  `port/gfx/fk_menu.{c,h}` (VOLUME/BRIGHTNESS/QUIT/POWER OFF, the
  `/usr/games/menu_resources/` artwork). "Adjust if needed" = adapt to
  our platform seam, NOT redesign. NOTICES entry before the code.
- **Hide Spectate/P2P/Server; VS Melee goes straight to local VS**,
  behind a named flag (MENU-SPEC §11.0 records that this ruling
  SUPERSEDES the spec's own §11.1 — the spec is evidence, the ruling is
  binding).
- **Post-gate jitter increment is the FINAL work item** (SCHED_FIFO +
  the SPIN_NS 3→2 ms retune, with enough passes for statistical power).
- Deferred by owner ruling: **4-player** (needs conformance AND a perf
  leg), **real name entry** (Melee-style d-pad character grid), **making
  the walljump toggle actually work**.

### 4. Findings a successor must not re-derive

- **Smash vs tilt is EDGE-TRIGGERED, not magnitude-based.**
  `action_state_shortcuts.c:387` needs `|in[0].lsX| >= 0.79 &&
  in[2].lsX*sign < 0.3` — full deflection NOW, near-neutral TWO FRAMES
  AGO. Same shape at `physics.c:367` (dash, frames 0 vs 3) and `:287`
  (fastfall). **A digital d-pad therefore produces smashes on press and
  tilts on hold, faithfully.** The driver initially claimed the opposite
  and the owner caught it. Natural's real losses are: no WALK (needs a
  sustained intermediate magnitude), 8 directions at magnitude 1 only
  (no partial DI angles), and no C-stick unless mapped.
- **The "box style" was never missing** — what ships (S1 One-Mod +
  C-layer) IS the box scheme, HayBox/B0XX lineage
  (`docs/research/b0xx-mapping.md`, branch `research/b0xx-mapping`).
  Two further prototyped-never-ported schemes exist (S2 dual-mod,
  S3 minimal) in `prototypes/control-mapping/funkeyMapping.js:84-103`.
- **L was never unbound**: keysym `k` → `PlatformInput.l` → S1 **Mod**,
  which emits nothing without a direction. R already shields.
- **Aggregate visual thresholds can be blind to a whole missing
  feature** — proven twice: deleting every STAR passed at 0.9927/0.99
  (U1), and deleting every ARTICLE passed at 0.8961/0.88 (U3). Fix
  shape is per-feature planes. **`check-render.sh`'s fg IoU is also not
  run-to-run reproducible** (0.9076/0.9038/0.9020 on identical code).
- **The FOH trace/judge emits NO sound observation at all** (`s->snd[]`
  never emitted) — every menu check is structurally audio-blind (C23,
  assigned to the menus lane; ~30 lines in `check-foh-flows.sh`).
- **"Evidence-rig bound governing the play path" is a CLASS**, three
  instances: A2 (`--bridge live` refused target launches), C1 (300 s
  menu timeout), C6 (3 min match cap). A fourth is registered (C7,
  fixed seed). The discriminator is never the bridge mode — it is the
  ABSENCE of a bound.
- **Upstream has no results screen**: `finishGame` is a banner + 2500 ms
  hold, then `endGame` resets and jumps to gamemode 2 (VS) / 7
  (targets). Five `finishGame` sites exist — the timer plus all four
  `DEAD*` KO arms.
- **Codex produces PROVEN REPLAYS** (six-plus instances: byte-identical
  findings blocks, identical token counts, citations to deleted code).
  cmp-prove every round. Distinguish replay from "findings I already
  fixed" from "findings I wrongly dismissed" — the C4 lane lost a round
  to that confusion. **Run codex SERIALLY**: concurrent sessions get
  their transcripts spliced by the companion (that was the root cause of
  several early "cached" discards).

### 5. Driver errors recorded (so a successor repeats none)

1. **iter 127** — flipped a manifest row to `reviewed-go` citing a log
   whose verdict was markdown-bold `**VERDICT: GO**`, i.e. ZERO anchored
   matches. Ran the anchored grep, got nothing, then eyeballed a `tail`.
2. **iter 132** — nearly the inverse: asserted bytes postdated a GO when
   mtimes proved otherwise. Judging evidence by narrative, both times.
3. **iter 132** — wrote a brace glob into a cite and **broke the gate**
   (§1c). C11 exists to make all three impossible mechanically.
4. **iter 131** — `git apply --3way` printed per-file success then rolled
   back ATOMICALLY; the cold re-run reported the pre-merge ledger and was
   nearly waved through. **Always verify a content fingerprint after a
   patch merge**, never the tool's own messages.
5. **2026-07-29** — a lane died in an API outage leaving
   `/mnt/disable_frontend` set, so **every boot declined to launch the
   frontend and the owner's device looked bricked**. Registered as C24:
   the deadman covers a DEVICE-side death, not a HOST-side one. Fix is a
   self-expiring marker, not more discipline.

### 6. The Roy research project (separate repo, charter-bound)

`~/code_projects/melee-chars-research` (git-init'd, no remote,
`CHARTER.md`). Full decision record: `docs/RESEARCH-MELEE-CHARACTERS.md`
here. Nine owner-ratified decisions; the load-bearing ones:
scope is **mechanically Melee** (animation deferred); acceptance is
**translation conformance** derived from meleelight's five existing
characters as a **Rosetta Stone**; the **Fox calibration slice** is the
go/no-go; **clone characters only, Roy first** (Marth's animations —
which is why the ~1.55M-coordinate-per-character animation wall does not
apply); Roy is authored in **meleelight JS first** against a **separate**
oracle fork so the pinned clone stays byte-frozen; the **first
experiment is a throwaway stub character + M0 re-verification** (the
named risk is the boot-RNG pin of exactly 465 draws). Deliverable is a
**report, not a character**. IP posture: the decomp is a READING MAP
only, never vendored; extracted data is gitignored, private, undistributed.
Key correction to carry: **"84.32% decompiled" is largely the wrong
metric** — the per-character numbers live in the game's DAT files as
subaction bytecode, so what matters is whether the interpretive map
(struct layouts + bytecode semantics) is done, not code-match percent.

### 7. Immediate next actions for the driver

0. **DONE (b44937b): verification-debt lane merged.** 3 lanes remain.
1. Drain the remaining 3 lanes as they report; **batch the merges** that
   touch pinned producers so the manifest+anchor cycle runs ONCE
   (PROCESS §12.2(3)).
2. Commit the match-exit lane's main-tree work once its arc closes.
   **BLOCKING FOR THE GATE:** its uncommitted `mlfk-foh.sh` (`c5e14d50`
   vs HEAD `0c37b702`) is a PINNED producer, so `verify_m4.sh` currently
   refuses at the pin check — EARLIER than `[0b]` — in the main tree.
   That is correct behaviour, not a regression: the bytes are unreviewed.
   Do NOT re-pin it to make the gate quiet; re-pin only at its closure.
3. Re-pin + re-anchor, then **re-run `verify_m4.sh`**.
4. Chase re-plays → ratification.
5. Then, and only then, the **jitter increment** (final item by owner
   ruling).

## [superseded by the 2026-07-29 handoff] (updated: 2026-07-27, iter 119 — **M4 GATE OK — AUTHORITATIVE, TWICE (writer + driver cold)**; LOOP STOP: m4-complete — awaiting Chase acceptance playthrough)

- **THE M4 EXIT GATE PASSED** (commit pending this entry): 87/87
  producers review-closed; delta arc + tables-schema both CLOSED on
  grok+Opus dual-GO (§11 fallback; codex proven-failed ×3, root cause
  refined: CODEX_RC fires pre-exit → sentinel-AND-process-gone
  discipline). Writer run rc 0 + driver COLD rc 0: `M4 GATE OK`
  exactly once each. Legs: FULLGAME CONFORMS 12/12 (p99 16.340 ms,
  skips 0, underruns 0, starves 0) · targets 2/2 · flows 7/7 · OPK
  frontend launch green. Evidence: .loop/m4-gate-run2.log,
  .loop/driver-cold-m4-gate.log, verify-m4/leg-*.log.
- **HUMAN GATE OPEN:** Chase acceptance playthrough closes the build
  phase (LOOP §H sentinel is the AGENT-LOG last line).
- **Post-gate queue (after ratification):** jitter increment
  (SCHED_FIFO + spin retune, §rulings) · M3 audio attribution →
  M3 GATE OK · present-column judge arm · grok-M/Opus-L1/plib-Low
  hardening registrations.

## [superseded by iter-119] (updated: 2026-07-27, iter 118 — universe 87 producers all pinned; 3 more gate false-greens fixed (NUL/binary-grep/tear); delta arc OPEN at r4 (codex Highs genuine, serial-codex rule adopted); M3 audio wired but perf-red 17.444 ms → deferred post-gate; queue = delta r5 → tables-schema arc → flips → GATE)

- **Iter 118 DONE** (commit 2c027ec). All four iter-117 rulings
  executed; ruling-4's premise was false (engine needed full
  data-plane wiring; now measures the modern renderer in a
  pre-iter-113 rig config → known-class attribution deferred
  post-gate, M3 refuses honestly, M4 unblocked). Serial-codex
  standing rule (concurrent codex sessions were being spliced — our
  bug, not caching). Delta arc continues: r4 fixes unreviewed, r5
  mandatory, §3 cap at ~8, Highs never capped.
- **Queue:** delta r5+ (serial) → tables-schema.js Tier A arc → flip
  5 rows → final anchors/self-checks → **M4 GATE live legs** → LOOP
  STOP: m4-complete → Chase playthrough → post-gate window: jitter
  increment (§rulings) + M3 audio attribution/green + present-column
  judge arm + plib deferral Low.

## [superseded by iter-118] (updated: 2026-07-27, iter 117 — jrt arc GO (judge frozen 4b68fba5, 5 false greens rejected, 100/100 zero-cost) + plib CLOSED (3 GOs); delta arc r1 NO-GO (23 unpinned frozen artifacts + status-token + 2 gate-grammar Ms); M3 leg-1 re-validated 8/8 on hardware, leg-2 red = known audio-TU gap; 4 rulings issued)

- **Iter 117 DONE (3 writer sessions).** Judge hardened through 9
  rounds + regression/falsification (10 code lines total, both
  driver-sanctioned); manifests M3 23/23 + M4 44/44 green with 5
  honest arc-in-flight rows; writer refused to flip rows over a NO-GO
  (correct). Present-column residual registered with the class named
  and both proposed remedies measured wrong. Full trail AGENT-LOG
  iter-117 + .loop/review-117-triage.md.
- **Rulings for the closure session:** pin the 23 frozen artifacts
  individually (provenance cites; consolidated provenance arc for any
  uncovered); add `grandfathered-m1` token + relabel; strengthen-only
  gate-grammar fixes authorized (final-LF, shots=13, 5/5, no leading
  zeros); audio-TU repair + m3 re-pin to full green.
- **Queue:** closure session (rulings → delta r2 → flip rows → final
  anchors → M4 GATE live legs) → LOOP STOP: m4-complete → Chase
  playthrough → FINAL jitter increment (§rulings) + present-column
  judge arm + plib deferral Low.

## [superseded by iter-117] (updated: 2026-07-26, iter 116 — m4 manifest assembled (33 rows, truthful) + universe extended; gate CORRECTLY REFUSES [0b] on 6 unclosed rows; arcs proved a jrt FALSE-GREEN (p99 13.9→7.1 rc 0) + fail-open plib plane; 5 driver rulings queued for iter 117)

- **Iter 116 DONE.** Manifest + universe 25→33 (8 omitted
  decision-bearing producers); statuses honest; driver cold refusal
  rc 1 naming exactly the six unclosed rows
  (.loop/driver-cold-t116-refusal.log). Tier A+ arcs: jrt codex NO-GO
  + Opus-5 NO-GO (demonstrated sim-column false green on the gate's
  headline p99; 29-judgment byte-identity PASSED), plib codex 4H +
  grok 2H independently reproduced (vacuous pass on empty set;
  self-derived contract). Cached-codex instances #6/#7 discarded.
  Full trail AGENT-LOG iter-116 incl. the 5 rulings.
- **Queue (iter 117):** jrt unfreeze-fix + plib hardening + goldens-m4
  plane pinning + correlate-skips pin/comment + self-row arc → all 33
  rows closed → re-anchor → delta GO → **M4 GATE RUN (legs live)** →
  LOOP STOP: m4-complete → Chase playthrough → FINAL jitter increment
  (§rulings) + audio-check TU repair.

## [superseded by iter-116] (updated: 2026-07-26, iter 115 — **GATE LEG 3 GREEN: OPK FOH LAUNCH OK via real gmenu2x, driver-cold rc 0**; pace regressions foh/target/render green; m3 manifest 23/23 truthful, zero arc-pending; audio-check stale-TU registered)

- **Iter 115 DONE.** MLFK_OPK_FOH arm (arc GO codex r3, 8 findings
  fixed; .desktop trailing-line defect fixed; three measured
  corrections: live gmenu2x conf anchoring, 2-column grid nav,
  busybox-ls terminal mode). Driver COLD on final bytes:
  `OPK FOH LAUNCH OK …` rc 0. Regressions: FOH OK / TARGET CONFORMS
  / RENDER OK (audio = pre-existing stale TU, gate-inert,
  registered). Driver manifest rulings executed: riglib re-pin
  (iter-109 capped-arc cite), 3 status flips per iter-108 ledger —
  cold self-check 23/23 + ANCHOR GREEN, verify_m3 runnable again.
  Full trail AGENT-LOG iter-115.
- **Queue:** iter-116 (LAST pre-gate) m4-freeze-manifest assembly +
  verify_m4 anchor → **M4 GATE RUN** → LOOP STOP: m4-complete →
  Chase acceptance playthrough → FINAL jitter increment (SCHED_FIFO
  + spin retune, §rulings) + audio-check TU repair.

## [superseded by iter-115] (updated: 2026-07-26, iter 114 — hybrid sleep landed: **runs 9+10 BOTH zero-skip on final bytes, vanilla FULLGAME CONFORMS 12/12 rc 0**; contention residual REGISTERED (not closed); gate evidence accepted by driver ruling)

- **Iter 114 DONE.** pace.h shared hybrid sleep (SPIN_NS=3 ms,
  measured two-component late-start split; net −14 lines); arc GO
  (grok r5; codex cached ×5th; claude-supplemental input adopted,
  reviewer set re-affirmed codex+grok). Runs 9 (armed) + 10
  (vanilla) both `FULLGAME CONFORMS 12/12 … skips=0` rc 0 on FINAL
  bytes; run-8's skip legs clean twice; swap flat. Jitter class NOT
  proven closed (15 B-events survive, max 6.4 ms = contention;
  p99 +0.3-0.8 ms cost) — ACCEPTED for gate per ruling, residual
  REGISTERED in fix_plan with reopen trigger. Worst headroom 0.566 ms
  = standing gate risk. Full trail AGENT-LOG iter-114.
- **Queue:** iter-115 OPK FOH mode + pace.h caller regressions
  (foh/target/render/audio on final bytes) → m4-freeze-manifest
  (+ m3 re-cites) → M4 GATE → provision → LOOP STOP: m4-complete →
  Chase acceptance playthrough → **FINAL: jitter-removal increment
  (SCHED_FIFO + spin retune, owner ruling 2026-07-26)**.

## [superseded by iter-114] (updated: 2026-07-26, iter 113 — rig swap pressure ELIMINATED at cause (4495→28 pages; A/B 94-skips-tmpfs vs 0-pages-SD); settle removed; residual = wakeup-jitter × sim-tail collision; hybrid-sleep class fix ruled; vanilla pass withheld pending final-bytes passes)

- **Iter 113 DONE.** Tmpfs plane measured 35.6 MB = 59% of RAM →
  relocated to SD (staged-back + sha-verified per review); one-var
  A/B decisive; settle removed; arc GO r7 (7 rounds, 11 fixes;
  codex cached-malfunction 4th instance; $BUILD read-by-presence
  stale-evidence class learned). Run 8 armed: bar (a) MET (swap
  flat), bar (b) not — g03/m02 skips = late_start≈3ms nanosleep
  overshoot COLLIDING with 13.8-15.2ms content sim tails (majflt 0,
  nivcsw single-digit). Worst p99 15.975/16.670 (0.695 ms margin =
  standing gate risk). Full trail AGENT-LOG iter-113.
- **RULING:** hybrid sleep (coarse nanosleep + bounded spin tail) at
  the shared seam in foh_dev.c:449 + gfx_app.c:312; engine-perf
  re-scope REJECTED unless this too is refuted. Final bytes still
  need their device passes (run-8 staleness caveat).
- **Queue:** iter-114 hybrid-sleep + armed & vanilla passes on final
  bytes (mints FULLGAME CONFORMS 12/12 as PROVEN close) → OPK FOH
  mode → m4-freeze-manifest (+ m3 re-cites) → M4 GATE → provision →
  LOOP STOP: m4-complete.

## [superseded by iter-113] (updated: 2026-07-26, iter 112 — **FULLGAME CONFORMS 12/12 MINTED (run 7 rc 0)** + DEVICE MUSIC OK; settle hypothesis REFUTED (close is BY LUCK); driver zoom-out: rig's ~33MB tmpfs plane named as suspect — relocation increment next, swap owner-call deferred)

- **Iter 112 DONE.** Run 7: `FULLGAME CONFORMS 12/12 (… skips=0
  teeth=21)` rc 0 + `DEVICE MUSIC OK`. Settle shipped (flow-mode
  dwell, structurally judgment-inert) but its OWN instrument refuted
  it: settle absorbed 0 pages, leg 1 displaced 4495 — menu working
  set ≠ match working set. Arc GO r5 (grok; codex cached-round
  malfunction 3rd proven instance). Full trail AGENT-LOG iter-112.
- **ZOOM-OUT (driver):** the suite's ~33MB /tmp/mlfk tmpfs plane =
  rig-induced RAM pressure real play doesn't have (OPK reads from
  SD). Iter-113 measures + relocates large artifacts to SD scratch;
  evidence bar = pswpout ≈0 ALL legs + 12/12 (PROVEN close). Swap
  owner-call only if pressure survives relocation.
- **Queue:** iter-113 tmpfs relocation → OPK FOH mode →
  m4-freeze-manifest (+ m3 re-cites) → M4 GATE → provision →
  LOOP STOP: m4-complete.

## [superseded by iter-112] (updated: 2026-07-26, iter 111 — fadvise class fix landed (run-5 11/12, swap-out −27%, tail legs clean); residual = FIRST-LEGS displacement transient; driver approved pre-suite settle (warm-up precedent); swap-policy + pin-rescope levers REJECTED)

- **Iter 111 DONE.** snd_mixer.h shared fadvise helpers (both readers —
  half-class premise caught by review), decision-inert per-leg
  vmstat/interrupt snapshots, arc GO ×2 (fresh codex retry + grok;
  codex cached-round malfunction recurred, proven again). Run 5
  vanilla: 11/12, 24/24 streams exact, only g02 (4 skips); 74% of
  swap-out in legs 1-2 → warm-up displacement transient, not
  in-window I/O. skip-attrib gap closed (`SKIP ATTRIB OK` rc 0).
  Full trail: AGENT-LOG iter-111 incl. the driver ruling.
- **RULING:** settle phase APPROVED (menu-dwell parity, evidence bar:
  flat pswpout on judged legs + 12/12); swap policy + pin re-scope
  REJECTED (swap = last-resort owner call with A/B if settle fails).
- **Queue:** iter-112 settle + `FULLGAME CONFORMS 12/12` +
  check-device-music (authorized) → OPK FOH mode → m4-freeze-manifest
  (+ m3 re-cites) → M4 GATE → provision → LOOP STOP: m4-complete.

## [superseded by iter-111] (updated: 2026-07-26, iter 110 — STALL CLASS ATTRIBUTED: SD-swap-driven preemption bursts (57MB RAM + 128MB swap-on-SD); instrument retargeted behind default-OFF gate; fix pending ONE deciding measurement)

- **Iter 110 DONE.** attrib.h lift + foh_dev --attrib
  (MLFK_FULLGAME_ATTRIB default-OFF; armed verdict ` [ATTRIB-ARMED]`
  structurally cannot match verify_m4's anchor). ATTRIBUTED: skips =
  involuntary-preemption bursts 100%-co-occurring with SD-IRQ storms
  (g01 f243: d_nivcsw=1374/frame; majflt=0 everywhere — paging and
  music-drain hypotheses REFUTED); per-leg SD IRQs track pswpout →
  swap-out bursts to the SD swap file under 57MB RAM. Arc GO r5 via
  grok fallback after codex returned two PROVEN-cached rounds (cmp
  byte-identical; §7 failure-mode 4 named). Full trail: AGENT-LOG
  iter-110.
- **Queue:** iter-111 = deciding measurement (vmstat in sampler
  windows: readahead vs swap-out) → in-scope fix (fadvise) OR owner
  call (swap policy) → vanilla 12-leg pass = FULLGAME CONFORMS 12/12 →
  check-skip-attrib device re-run (registered gap) → OPK FOH mode →
  m4-freeze-manifest (+ m3 re-cites) → M4 GATE → provision →
  LOOP STOP: m4-complete.

## [superseded by iter-110] (updated: 2026-07-26, iter 109 — task-14 increment 2 COMMITTED: full-game engine live, 12/12 streams exact ON DEVICE ×2 passes, cold-start skip class CLOSED; residual = environmental sim-stall class, attribution next)

- **Iter 109 DONE (4 Opus writer sessions + driver runs/rulings; full
  trail AGENT-LOG iter-109).** foh_dev direct-match entry + launch
  warm-up; check-device-fullgame.sh (12 legs, per-leg quiesce,
  owned-claim recovery, 21 teeth); riglib hardening; PORTABILITY entry;
  verify_m4 teeth=21 tighten. Arc r1-9 CAPPED (all M+ closed except the
  registered five-producer recovery class; "no bar-(a) defect" ×5
  rounds); delta arc r10-13 CAPPED ("gate-contract escalation" class,
  dissent verbatim in .loop/review-109-r3-triage.md; §8 blind-spot
  statement in AGENT-LOG).
- **Evidence:** run4 10/12 — 24/24 STREAM MATCH, p99 ≤16.003, underruns
  0, starves 0; g01 cold-start class CLOSED (R7 held: all frame-1s
  8.1-13.4 ms). Residual: g04 f1831 sim=28.7 ms, m02 f3356 sim=15.9 ms
  — NON-deterministic sim-phase environmental stalls (single-core
  preemption/paging); SD-writeback class fix HELD (zero render stalls).
- **Queue:** task-8 instrument RETARGET (foh_device/any-leg) +
  attribution runs on the sim-stall class (hypotheses to MEASURE:
  music-PCM SD streaming contention, major-fault paging under the
  ~150 MB PCM set, daemons incl. system_stats) → skips==0 12-leg pass
  (= increment-3 exit `FULLGAME CONFORMS 12/12`) → OPK FOH mode →
  m4-freeze-manifest (+ driver-sanctioned m3 re-cites) → M4 GATE →
  provision → LOOP STOP: m4-complete.

## [superseded by iter-109] (updated: 2026-07-25, iter 108 — task-14 increment 1: verify_m4.sh DELIVERED + arc GO r4; task 14 OPEN — leg engines unbuilt, no M4 golden replays on device yet)

- **Iter 108 DONE (task-14 increment 1, §11 Opus writer).** verify_m4.sh
  (spec-verbatim gate: anchor→pins→[0b] refusal before any leg, relay
  contract, anchored grammars) committed with its Tier A arc CLOSED —
  .loop/review-108-{1..4}.log NO-GO ×3 → GO r4 zero findings; 9 Highs
  + 1 Medium fixed. Driver cold: bash -n + refusal run rc 1
  (.loop/driver-cold-t14-refusal.log). Producer ledger
  .loop/m4-t14-producer-status.md (25 rows verified).
- **KEY DISCOVERY (writer, honest stop):** leg 1 unassemblable today —
  gfx_app refuses --cpu without --ai-bridge (no live-AI arm);
  foh_dev's live ai.c reachable only through menu flows; only m01 has
  a flow. NO M4 golden replays on device yet.
- **Queue (task 14 remains, iter-109 dispatch):** foh_dev direct-match
  entry → check-device-fullgame.sh (12 traces, render+audio+music) →
  check-device-opk.sh FOH mode → m4-freeze-manifest.txt (+
  driver-sanctioned m3 re-cites: render/judge-shot → review-80-1 GO;
  check-device-opk STAYS pending) → new-surface arcs → M4 GATE →
  provision → LOOP STOP: m4-complete.

## [superseded by iter-108] (updated: 2026-07-25, iter 107 — PERSIST ARC CLOSED: GO round 7, riglib reviewed-go; PERSIST OK cold teeth=19; first §11 Opus-writer arc)

- **Iter 107 DONE — persist-arc rounds 4-7 CLOSED at GO** (first
  PROCESS §11 arc: Opus 5 writer ran Codex ×4 itself and fixed
  in-session; driver read verdicts from .loop/review-106-{1..4}.log —
  NO-GO ×3 → GO, MLFKPROC-grammar class CAPPED round 7). 8 Mediums
  closed, headline = NEW host-side collateral-destruction class (the
  H copy tooth eval'ed device commands on the host, running `pkill
  foh_device` locally — now zero eval-of-device-commands + parity
  guard + seam unit tooth). Driver COLD: `PERSIST OK (… teeth=19)`
  exit 0 (.loop/driver-cold-per107.log — verifies the writer-flagged
  teeth counter) + manifest ALL 23 ROWS + ANCHOR GREEN with riglib
  `reviewed-go .loop/review-106-4.log`
  (.loop/driver-cold-per107-manifest.log). Full trail: AGENT-LOG
  iter-107.
- **Queue:** task 14 — verify_m4.sh assembly (dispatch Opus 5 writer
  per §11; the 4 remaining arc-pending manifest rows belong to the
  render/opk arcs it composes) → M4 GATE → provision → LOOP STOP:
  m4-complete.

## [superseded by iter-107] (updated: 2026-07-25, iter 106 — M4 hardening DONE: persist-arc round-3 closure driver-committed; PERSIST OK cold teeth=18; model-assignment ruling in force)

- **Iter 106 DONE (persist-arc round-3 closure; driver-committed per
  iter-101 precedent — the 2026-07-20 writer finished implementation +
  warm checks then died; this driver ground-truthed COLD 2026-07-25).**
  COLD done-check: `PERSIST OK (sessions=2 powercycle=reboot
  bootid=bootid:PRE!=POST bootwait=13s legs=5 pulls=4
  roundtrip=byte-exact record=00:14.50 resets missing=1 loud-corrupt=2
  dirsync=plain-saved+degraded-tooth teeth=18)` exit 0
  (.loop/driver-cold-per106.log; teeth 17→18). Manifest self-check COLD
  all rows + anchor GREEN (.loop/driver-cold-per106-manifest.log).
  Closures: all 6 review-104-triage dispositions (M-1 CRLF byte-exact
  raw grammars, M-2 scan fail-closed rc/zero-byte arms, M-3 anchored
  MLFKPROC grammar, M-4 decoder full-initializer + exact-byte PPM
  reconciliation, L-1 real-EXIT-trap COPY tooth, L-2 `(deleted)` cwd
  forms) + riglib manifest re-pin + verify_m3 anchor. foh_persist.{c,h}
  BYTE-UNCHANGED. Honest gap logged: writer's refutation ledger
  (R1–R5) not separately recorded — round 4 owns follow-up.
- **Ruling in force:** 2026-07-25 model assignment (§rulings below /
  PROCESS §11, commit b7b8c5c).
- **Queue:** persist-arc **round 4 (closure-or-cap) over the iter-106
  diff — dispatch to an Opus 5 writer per §11** (first ruling-shaped
  arc; riglib manifest row stays arc-pending until its GO) → task 14
  (verify_m4.sh assembly) → M4 GATE → provision → LOOP STOP:
  m4-complete.

## [superseded by iter-106] (updated: 2026-07-20, iter 105 — M4 micro DONE: task-12-arc round-3 M closed, [5b] banner-witness grammar anchored, FOH FLOWS OK cold teeth=21)

- **Iter 105 DONE (M4 micro; review-103 round-3 NO-GO — the sole
  Medium, PROCESS §3 whitelist-grammar rule).** The NEW [5b]
  banner-witness leg parsed its OWN decision output PERMISSIVELY:
  :1374 accepted the `BANNER WITNESS OK` PREFIX (producer emits a
  longer metrics line), :1398 searched a bare missing-glyph SUBSTRING
  for the tooth, :1397 accepted ANY nonzero exit. **FIX (anchored
  full-line whitelist):** MEASURED the producer grammar empirically
  (.loop/m4-ban105-measure.log — success line `BANNER WITNESS OK
  (complete_ink=2016 failure_ink=1680 distinct=yes)` byte-identical in
  both archived runs + fresh; tooth exit code **3** + exact foh_font
  diagnostic line confirmed by manual relink), then a `banner_verdict_ok`
  helper anchors the FULL success line (count_xl = grep -cxF, exactly 1)
  with a needle==full resemblance guard, and the tooth now requires the
  EXACT rc 3 + EXACT full-line diagnostic + resemblance reconciliation.
  Ink counts pinned (deterministic 5x7 integer blit). CORPUS: both
  archived genuine [5b] outputs accept 1/1 (zero false rejections).
- **Done-check (cold):** `FOH FLOWS OK (flows=7 shots=17 bridges=3
  tbridges=2 states=4 tstates=2 diverge=1 control=1 banner=1 teeth=21)`
  exit 0 (.loop/m4-ban105-fohflows-cold.log). teeth 19→21 — TWO NEW
  parser teeth joined the ledger (garbled-success + substring-resemblance
  verdict outputs both rejected). ONE atomic commit. Full trail:
  AGENT-LOG iter-105 pre-reg + result.
- **Surface:** port/foh/check-foh-flows.sh ONLY. Frozen artifacts +
  gfx_target.c / foh_banner_witness.c / foh_font.c BYTE-UNTOUCHED
  (grammar measured FROM them). Refutations R1/R2/R3 all recorded
  NEGATIVE with evidence.
- **Run ledger:** cold host checks 1/2 (spare unused; measurement reused
  iter-104 .o's); paced device 0, arm 0, browser 0.
- **Queue:** task-12-arc **round 4 = closure-or-cap on THIS iter-105
  diff** (the anchored [5b] parser); persist-arc round-3 GO status →
  task 14 (verify_m4.sh assembly) → M4 GATE → provision → LOOP STOP:
  m4-complete.

## [superseded by iter-105] (updated: 2026-07-20, iter 104 — M4 hardening DONE: persist-arc round-2 closure, review-102 ALL 7 dispositions, PERSIST OK cold)

- **Iter 104 DONE (M4 hardening; review-102 round-2 NO-GO — all 7
  dispositions closed).** COLD done-check: **`PERSIST OK (sessions=2
  powercycle=reboot bootid=bootid:PRE!=POST bootwait=12s legs=5 pulls=4
  roundtrip=byte-exact record=00:14.50 resets missing=1 loud-corrupt=2
  dirsync=plain-saved+degraded-tooth teeth=17)`** exit 0
  (.loop/m4-per104-donecheck.log; teeth 12→17). Real reboot judged
  (PRE 4e3cd631 != POST f05cf035, uptime 7s<12s). Device left clean.
  ONE atomic commit. Full trail: AGENT-LOG iter-104 pre-reg + result.
- **Closures**: **H (DATA-LOSS)** — trap three-state model
  (UNPROBED/ABSENT/PRESENT via a pure `persist_residue_decide`); the
  only delete arm is ABSENT, UNPROBED/unknown KEEP the user file
  (observed in vivo: 3 pre-[5] aborts left $DFILE untouched).
  **M-a** raw freshness-token grammars (boot_id/btime/uptime validated
  as bytes-on-disk incl. newline shape, no squeeze; live-verified).
  **M-b** persist-file byte-exactness (od final-LF, wc-c==1366
  reconciliation, complete SUM-line grammar; corpus 5 PASS/4 REJECT).
  **M-c** orphan-scan reconciliation (VANISHED vs UNREADABLE; pure
  `rig_orphan_parse` reconciles rows==found, fails closed). **M-d**
  teardown reap-failure PRESERVES $DTMP. **L-a** relative-cwd orphan
  predicate (comm + /proc/<pid>/cwd). **L-b** NEW decode-pb-glyphs.js
  reads the FOH font AS DATA + decodes the PB shot region == the
  derived string (twin 00:14.50 / control --:--:--).
- **Surfaces**: check-device-persist.sh, riglib.sh, NEW
  decode-pb-glyphs.js, m3-freeze-manifest.txt (riglib re-pin) +
  verify_m3.sh anchor. foh_persist.{c,h} BYTE-UNCHANGED (constraint).
  Manifest self-check ALL 23 ROWS GREEN + ANCHOR GREEN
  (.loop/m4-per104-manifest-selfcheck.log). The riglib row stays
  `arc-pending` — persist-arc **review round 3 (closure-or-cap)** over
  THIS iter-104 diff owns the GO.
- **Regressions**: check-foh-flows mechanical skip-proof
  (.loop/m4-per104-fohflows-skip.txt — zero shared surfaces:
  foh_persist/render/font/gfx/sim untouched); other device checks
  skip-proof (riglib deltas scan/teardown-only;
  check-device-target's next cold run stays the task-14 ritual).
- **Queue**: persist-arc round 3 (closure-or-cap on iter-104) →
  task-12-arc round 3 → task 14 (verify_m4.sh assembly) → M4 GATE →
  provision → LOOP STOP: m4-complete.

## [superseded by iter-104] (updated: 2026-07-20, iter 103 — M4 micro DONE: finish-banner glyph fix, review-101 round-2 M closed, FOH FLOWS OK cold)

- **Iter 103 DONE (M4 micro; review-101 round-2 M — the sole
  outstanding finding).** The authored-unreachable finish path was a
  latent PRODUCT BUG: `gfx_target_banner` drew `COMPLETE!`/`FAILURE`
  via the frozen VFXGLYPHS atlas font 0 (digits + ':' only), so the
  FIRST real finish would hit gfx_overlay.c's FATAL missing-glyph path
  and ABORT (never observed — no committed device leg reaches the finish
  seam; foh_dev draws the banner only when g_tfin_fired, which
  check-device-target's assert_no_tfinish forbids). **FIX (triage option
  (a)):** the banner now renders via the letter-complete self-authored
  FOH 5x7 font (`foh_text`), split into `gfx_target_banner_text`. Frozen
  artifacts BYTE-UNTOUCHED; option (b) (atlas re-freeze) refuted/not
  needed. **WITNESS:** new `port/foh/foh_banner_witness.c` +
  check-foh-flows.sh [5b] drives the REAL banner-text render for BOTH
  strings (links gfx_target.o + raster.o + foh_font.o, -Wl,-dead_strip)
  → `BANNER WITNESS OK` (both non-blank + distinct); the missing-glyph
  fatal is structurally unreachable for both reachable strings. **TOOTH:**
  a COPY with a missing glyph (COMPLETE?) still dies loud at foh_font;
  a font-revert to gfx_glyph_text fails to LINK (guard intact).
- **Done-checks (cold, logged):** `FOH FLOWS OK (flows=7 shots=17
  bridges=3 tbridges=2 states=4 tstates=2 diverge=1 control=1 banner=1
  teeth=19)` exit 0 (.loop/m4-ban103-fohflows-cold.log) — NEW banner=1,
  teeth 18→19; regression `RENDER OK` exit 0
  (.loop/m4-ban103-render-cold.log, IOU 0.9049); check-target-sim
  skip-proved (target_finish_probe.c unchanged, no sim TU touched). ONE
  atomic commit. Full trail: AGENT-LOG iter-103 pre-reg + result.
- **Queue:** task-12-arc round 3 (closure-or-cap on THIS iter-103 diff)
  + persist-arc round 2 → task 14 (verify_m4.sh assembly) → M4 GATE →
  provision → LOOP STOP: m4-complete.

## [superseded by iter-103] (updated: 2026-07-20, iter 102 — M4 hardening DONE: review-100 persist round-1 closure + the orphaned-deadman leak CLASS FIX, PERSIST OK cold)

- **Iter 102 DONE** (respawn writer; original writer died on a usage
  limit after freezing its pre-reg — this respawn adopted the pre-reg +
  partial riglib edits; full trail: AGENT-LOG iter-102 pre-registration
  + result). COLD done-check: **`PERSIST OK (sessions=2
  powercycle=reboot bootid=bootid:PRE!=POST bootwait=12s legs=5 pulls=4
  roundtrip=byte-exact record=00:14.50 resets missing=1 loud-corrupt=2
  dirsync=plain-saved+degraded-tooth teeth=12)`** exit 0
  (.loop/m4-per102-donecheck.log). ONE atomic commit.
- **Bundle A — review-100 (persist-arc round 1) ALL dispositions
  closed**: H1 identity-grade reboot witness (host-judged PRE!=POST
  boot_id — fired on the real reboot: 5b9b339e… != 8031437d…, uptime
  7s<13s gap; btime fallback measured-present; COPY tooth died exactly
  at the judge); M1 same-process stale-PB PRODUCT BUG fixed at the
  chokepoint (bind-at-apply + refresh-at-record; host witness shot ==
  persisted twin, discrimination proven vs an unfixed rebuild); M2 exact
  positional MLFKPERSIST1 whitelist (corpus-validated: 14 genuine PASS /
  5 corrupt REJECT); M3 dir-fsync loudness (`saved-nodirsync` distinct
  token); M4 verdict-bound byte-verified restore (zero-byte-safe); L1
  spec-derived display pin.
- **Bundle B — orphaned-deadman leak CLASS FIX** (riglib chokepoint):
  `rig_orphan_reap` step-0 (rig_lock_acquire; inherited by all 11 rig
  checks) + all-exit teardown (rig_cleanup, before the $DTMP wipe that
  loses a racing deadman.cancel). Teeth green on device: T-B1 mid-check
  death -> zero survivors (+ discriminator showing the OLD path leaks),
  T-B2 planted orphan loudly reaped. A reap-robustness bug (vanishing
  proc -> shell redirect leaked stderr into the whitelist grammar) was
  found + fixed mid-run (cat opens the file). Manifest re-pinned SAME
  commit: riglib row -> arc-pending, anchor recomputed (openssl); 23
  rows GREEN + ANCHOR GREEN.
- **Regressions**: cold FOH FLOWS OK (teeth=18), ZERO re-freezes;
  check-sim/target-sim + other device checks mechanical skip-proof (no
  sim TU touched; riglib delta is start/teardown-only;
  check-device-target's next cold run is the task-14 ritual). New device
  semantics in docs/PORTABILITY.md (boot-identity witness + deadman
  reap).
- **Queue**: task-12-arc round 2 (iter-101 diff + the COMPLETE task-12
  packet, .loop/review-99-full-diff.txt) + persist-arc round 2 (reviews
  THIS iter-102 hardening diff — owns the riglib arc-pending GO) → task
  14 (verify_m4.sh assembly) → M4 GATE → provision → LOOP STOP:
  m4-complete.

## [superseded by iter-102] (updated: 2026-07-20, driver post-iter-101 — task-12-arc round-1 closure ADJUDICATED GREEN + orphaned-deadman stall class found; iter-102 next)

- **Iter 101 (task-12-arc round-1 closure) DONE via DRIVER
  ADJUDICATION** (writer BLOCKED-HONEST at the skips gate 2/2; driver
  found + killed TWO orphaned /tmp/mlfk/deadman.sh combs — leaked by
  failed-run exit paths, skip counts doubled with orphan count — then
  cold-verified on the clean device: **`DEVICE TARGET CONFORMS
  (… skips=0 … sfxpin=15/31 music=menu>targettest:5/5 teeth=6)`**
  exit 0, .loop/m4-tgt101-driver-cold.log; full trail: AGENT-LOG
  iter-101 entries + the driver adjudication entry). All five
  review-99 dispositions shipped: track-identified music (mustrack
  producer line + 5/5 leg pairs), SFX_STARTS_PIN 15/31 exact,
  TLAUNCH canonical decimal (judge sha 2267f8b7… re-pinned in all
  four consumers), tfinish resemblance-death all legs, T3 semantic
  exit + exact diagnostic. FOH FLOWS OK teeth=18 cold, zero
  re-freezes.
- **SELF-AMPLIFYING LEAK CLASS registered → iter-102**: device-check
  failure paths leak the deadman; orphaned 2 s combs stall later runs
  (the iter-74 low_bat_check mechanism). Riglib chokepoint fix
  (orphan-scan step-0 + all-exit teardown) + manifest re-pin rides
  with the review-100 persist closure.
- **Queue**: iter-102 (review-100 persist triage H1/M1-M4/L1 + the
  deadman class fix) → task-12-arc round 2 (iter-101 diff + the
  COMPLETE task-12 packet, .loop/review-99-full-diff.txt) +
  persist-arc round 2 → task 14 (verify_m4.sh assembly) → M4 GATE →
  provision → LOOP STOP: m4-complete.

## [superseded by driver post-iter-101] (updated: 2026-07-20, iter 100 — M4 task 13 DONE: SD persistence, PERSIST OK cold)

- **Iter 100 (M4 task 13 — persistence to SD) DONE** (full entry:
  AGENT-LOG iter 100 pre-registration + result; latest AGENT-LOG id:
  iter 100). COLD: **`PERSIST OK (sessions=2 powercycle=reboot
  bootwait=12s legs=5 pulls=4 roundtrip=byte-exact record=00:14.50
  resets missing=1 loud-corrupt=2 teeth=9)`** exit 0
  (.loop/m4-task13-donecheck-run2.log). ONE chokepoint
  port/foh/foh_persist.{h,c} (MLFKPERSIST1: sha256-sealed, hex16
  bit-pattern doubles/strtod-free, atomic tmp+fsync+rename, LOUD
  reset-to-defaults on missing/version/corrupt — the qjs lesson
  inverted); settings {turbo,lcancel,tapjump[4]} +
  targetRecords[5][10] (medals DERIVED upstream — not persisted;
  display deferral stands); save points = options B-exit +
  the finishGame record arm (tp_finish_hook, REAL wiring; exercised
  by the NEW foh_dev --tooth-persist-finish arm — completing runs
  authored-unreachable, registered honest coverage). Records READ
  path CLOSED: PERSONAL BEST renders the persisted record, device
  shot byte-exact vs host twin + != defaults control. TWO real
  sessions across a REAL measured reboot (down ~2 s post-dispatch,
  adbd healthy ~40 s; in-check bounded 120 s + offline witness); all
  4 pulls byte-identical to host-constructed twins. Regression: cold
  **FOH FLOWS OK (flows=7 shots=17 bridges=3 tbridges=2 states=4
  tstates=2 diverge=1 control=1 teeth=18)** with ZERO re-freezes
  (.loop/m4-task13-fohflows-run1.log); check-sim/check-target-sim
  mechanical skip-proof (.loop/m4-task13-checksim-skip.txt — no sim
  TU touched). Device left clean (verified: no marker, no residue,
  scratch wiped, gmenu2x live, lbc==1). MEASURED gotcha (new): a raw
  `adb shell "… &"` reboot dispatch is killed by this adbd's
  teardown — use the house setsid+dsh detach recipe (PORTABILITY
  row).
- **MEASURED: /mnt/mlfk-data is the LIVE play-install data dir**
  (anim bins/sndpack/gfxdata/mlfk-logs) — the check touches ONLY
  mlfk-persist.dat{,.tmp} and restores any pre-existing file.
- **Registered residuals for task 14**: check-device-foh.sh +
  check-device-target.sh got PAIRED MECHANICAL edits (per-leg
  MLFK_PERSIST_DIR hermeticity; device-foh's host-twin recipe also
  REPAIRED — it was link-broken since iter 99: targets stage +
  target_play/gfx_target/ml_targets TUs added) WITHOUT cold device
  reruns (iter-99 precedent — the gate + driver ritual own them).
  m3-freeze-manifest riglib row stale since iter 93 (pre-existing;
  the m4 manifest pins fresh). Tier-A arc for the iter-100 surfaces
  (check-device-persist.sh NEW, foh_persist.{h,c}, check-foh-flows
  delta, driver deltas) = driver-run, post-commit. Product-path
  note: the OPK launcher saves to SD inside the render loop at the
  upstream cookie moment (no skips gate on the play surface;
  PORTABILITY row).
- **Next: task 14 (verify_m4.sh assembly) → M4 GATE → provision
  device → LOOP STOP: m4-complete.**

## [superseded by iter 100] (2026-07-20, iter 99 — M4 task 12 DONE: target test FOH + device, DEVICE TARGET CONFORMS cold)

- **Iter 99 (M4 task 12 — target test FOH + device) DONE** (full
  entry: AGENT-LOG iter 99 pre-registration + result; latest
  AGENT-LOG id: iter 99). COLD: **`DEVICE TARGET CONFORMS (goldens=2
  flows=2 shots=4 fbwit=4 p99=13.650ms skips=0 underruns=0 starves=0
  starts f06-target-t01=15 f07-target-t02=31 teeth=6)`** exit 0
  (.loop/m4-task12-devtarget-run4.log). Regressions cold on final
  bytes: FOH FLOWS OK (flows=7 tbridges=2 teeth=18), TARGET SIM
  CONFORMS (probes=2 teeth=24), SIM CONFORMS 8/8 (REQUIRED — shared
  sim TUs: ML_MAX_LABELLED_SURFACES 96 capacity fix, targetstage9
  concatenates 95), RENDER OK. Device verified clean post-run.
- **t03 REFUTED (R1, stronger form)**: targetstage9's single target
  is TOPOLOGICALLY UNREACHABLE in the authored geometry (sealed
  region; the 14.18 s devRecord is stale data — do NOT re-litigate).
  finishGame is REAL (tp_finish_game + tp_finish_hook seam) with
  MECHANICAL coverage via the NEW target_finish_probe (Complete +
  Failure arms + the DOUBLE-DESTROY QUIRK now live — both brief gaps
  closed); the `foh_dev tfinish:` line is asserted ABSENT on all
  committed legs; end banner + live finish = acceptance surface
  (registered).
- **Registered residuals for tasks 13/14**: records/medals/cookies +
  records READ (task 13); check-device-foh.sh got paired pin updates
  (judge sha, FLOW_INVENTORY 7 vs 5 driven legs) WITHOUT a cold
  device rerun — task 14's gate + driver ritual own it; START-quit
  endGame still trapped; match-phase presents still unwitnessed;
  Tier A arcs for the iter-99 surfaces (check-device-target.sh NEW,
  check-foh-flows.sh, judge/normalize extensions) = driver-run,
  post-commit.
- **Next: task 13 (SD persist), task 14 (verify_m4.sh) → M4 GATE →
  provision device → LOOP STOP: m4-complete.**

## [superseded by iter 99] (2026-07-20, driver post-iter-98 — DEVICE-FOH ARC CLOSED (GO r3, capped); iters 97+98 pushed; target-arc round 3 + task 12 in flight)

- **DEVICE-FOH ARC CLOSED (GO round 3, .loop/review-97-1.log)** after
  iters 95/97 (cd99733, 2ee7577 — both driver-cold-verified + pushed).
  CAPPED naming two recurring classes (reviewer's own closure-or-cap
  call): whitelist-input-trust spelling refinements + compiled-keymap
  projection/tooth coverage (the platform_keymap.h logical↔fieldOff
  binding note REGISTERED for task 14's manifest round alongside the
  platform_sdl1.c re-pin). Final form: DEVICE FOH OK opk=evidence
  fbwit=15 teeth=15.
- **Iter 98 (a3fc74d) driver-cold-verified (TARGET SIM CONFORMS
  teeth=24, RC=0) + PUSHED.** Tier A+ second review of
  verify-target-stream.js: grok VERDICT: GO (its Medium/Low adopted +
  shipped in iter 98). Codex process note: review-96-1 died on a
  provider content flag mid-review (after live-confirming the dup-key
  finding); the -2 retry delivered — new reviewer-outage mode: retry
  once, then grok fallback.
- **In flight: (1) target-arc ROUND 3 = closure-or-cap over a3fc74d
  (.loop/review-98-1.log when it lands); (2) iter-99 = M4 task 12
  (target test FOH + device) writer** — done-check
  `bash port/sim/target/check-device-target.sh` → DEVICE TARGET
  CONFORMS. Then: task 13 (SD persist), task 14 (verify_m4.sh) →
  M4 GATE → provision device → LOOP STOP: m4-complete.

## [superseded by driver post-iter-98] (updated: 2026-07-20, iter 98 — M4 hardening DONE: target-rig round-2 closure, all review-96 dispositions shipped; cold check green first attempt)

- **Iter 98 (M4 hardening — target-rig round-2 closure, review-96
  triage C-M1/C-M2/C-M3/G-M4/C-L5/C-L6/G-L7 ALL closed) DONE** (full
  entry: AGENT-LOG iter 98 pre-registration + result; latest
  AGENT-LOG id: iter 98). COLD, FIRST attempt: **`TARGET SIM CONFORMS
  (2 goldens: t01 t02; leaves=718 probe=ok teeth=24)`** exit 0 —
  TWICE (.loop/m4-tgt98-donecheck.log + -2.log; teeth 12→24 honest
  change); check-sim NOT consumed — mechanical skip-proof
  (.loop/m4-tgt98-checksim-skip.txt). FROZEN streams/traces/manifest
  + oracle/ + run-target.js + record-target.sh byte-untouched.
  **C-M1**: NEW SHARED port/goldens-m4/json-dup-key-scan.js — ONE
  string-aware duplicate-JSON-key scanner (raw-bytes tokenizer,
  escape-decoding, per-object-scope key sets, dup at ANY scope =
  death naming key+scope); used by validate-target-manifest.js (the
  refuted byte-literal '"key":' count DELETED), verify (frozen +
  sibling + run raw), freeze (runA/runB + refreeze read); the
  live-confirmed `"id" : "t02"` probe and top-level dups die (T13/
  T14). **C-M2**: sibling gets the full exact-whitelist schema
  (top-level + params key ORDER + {f,h} rows + target-mode value
  pins); run target rows keysExact {f,h}; the codex
  `{"f":999,"f":1,...}` probe dies at the scanner (T15/T16). **C-M3**:
  T12 = TRUE integration tooth — copied tree with cmp-proven
  PRODUCTION consumer bytes, pristine-manifest CONTROL pass, then
  wrap + verify + IDS-pull each die naming `duplicate golden id` on
  the dup COPY (no direct validator CLI counts). **G-M4**: run-meta
  pins at M0 discipline — fdlibm/seedRandom === true + EXPLICIT
  p2/stage/difficulty null, cpu false (absent = undefined = death;
  T23/T24). **C-L5**: wrap-target canonInt (safe-integer + round-trip
  + producer domains %u / 0..20); `RNG 9007199254740993 1` dies
  (T22). **C-L6**: per-class frozen-side teeth — tstage
  metadata-binding, playerStream name-binding, sibling-rngCalls
  cross-pin, finalTargetsDestroyed quality deaths (T17-T20). **G-L7**
  (pre-registered refutation FIRED as predicted): finals NOT
  derivable from sealed hashes → assertion form — judge asserts
  frozen finalTargetsDestroyed >= the VALIDATED manifest row's
  minTargets directly + run-finals equality; T21 proves the
  +1-within-domain death. CORPUS: 45 genuine JSONs scanned zero false
  rejections + 8 judge regressions MATCH + freeze `unchanged` x4 +
  wrap cmp-identical x2 (.loop/m4-tgt98-corpus.log). Run ledger:
  browser 0/0, cold check-target-sim 2/2 (both green), check-sim 0
  (skip-proof). **Driver next: review-96 ROUND 3 — closure-or-cap
  naming the class (per the triage header; remaining objection
  classes were whitelist-input-trust + tooth-coverage variants, both
  now closed at instrument level) over the iter-98 diff
  (validate-target-manifest.js, verify-target-stream.js,
  wrap-target.js, freeze-target.js, json-dup-key-scan.js NEW,
  check-target-sim.sh). Then task 12 (target test FOH + device).**
- **Remaining to gate**: 12 (target test FOH+device), 13 (SD persist),
  14 (verify_m4.sh) -> M4 GATE -> provision device -> LOOP STOP:
  m4-complete -> Chase acceptance playthrough.

## [superseded by iter 98] (2026-07-20, iter 97 — M4 hardening DONE: device-FOH round-2 residuals, all review-95 dispositions shipped; cold check green first attempt)

- **Iter 97 (M4 hardening — device-FOH round-2 residuals, review-95
  triage M-a/M-b/M-c/M-d/M-e/L-a/L-b ALL closed) DONE** (full entry:
  AGENT-LOG iter 97 pre-registration + result; latest AGENT-LOG id:
  iter 97). COLD, FIRST attempt: **`DEVICE FOH OK (flows=5 shots=13
  bridge=1 states=3 opk=evidence fbwit=15 p99=13.774ms skips=0
  underruns=0 starves=0 starts f01-vs-g01=281 f02-cpu-m01=16
  f03-options=23 f04-nav=39 f05-vs-g03=14 teeth=15)`** exit 0
  (.loop/m4-foh97-donecheck.log; teeth 12→15 honest change). **M-a**:
  judge_fbwit = single strict reader (trailing-newline mandatory,
  reader-iterations == grep-count == pinned, END terminator bound to
  the last position); T10 gains the torn-trailer + yoff=240 shapes.
  **M-b**: NEW port/gfx/platform_keymap.h = ONE compiled definition
  site consumed by BOTH platform_sdl1.c's platform_poll loop (minimal
  diff — touched as judged unavoidable; task-14 m4-freeze-manifest
  note registered) and foh_dev --dump-keymap/parse_buttons; global
  source-substring scan DELETED; T12 perturbed-COPY-build tooth (dump
  diverges from frozen, rc 1) + T-devswap unchanged. Single-LINK-
  SYMBOL form refuted by construction (frozen rig_arm_build TU list +
  host dump links platform_headless) — recorded, do not re-litigate.
  **M-c**: all four summary parsers needle-anywhere (`grep -cF`)==1 —
  the iter-86 needle-freedom form. **M-d**: OPK verdict path now runs
  the SAME bounded judge (constructed expect = frozen f01.expect
  pre-input prefix + END==500 exact; explicit input-free declaration;
  bounds from the ARCHIVED green opk trace — spare paced run unspent)
  + strict skip-summary parse of the pulled mlfk-foh.log (skips==0,
  fails==0, transitions==1) under a LEG-IDENTICAL low_bat_check
  quiesce bracket; T13 = T11's stall shape at OPK copy level (rc 3).
  **M-e**: canonical decimals (0|[1-9][0-9]{0,K}) at EVERY numeric
  acceptance site in check-device-foh.sh AND normalize-foh-trace.js
  (`00` = death; BASH_REMATCH indices re-audited). **L-a**:
  anchor=null fatal rc 3 unless explicitly declared input-free
  (frozen EMPTY whitelist or the OPK leg's literal arg);
  declared-but-anchored = rc 2; T14 proves both. **L-b**: yoffset==0
  pinned in-app (foh_dev dies naming pan-reject drift;
  --fb-witness-raw stays the instrument) + in-check (W-row yoff=0).
  Corpus: 61 checks over all archived genuine artifacts with the REAL
  extracted function bytes, zero false rejections
  (.loop/m4-foh97-corpus.log). Run ledger: paced 1/2, arm rebuilds
  1/3; check-foh-flows mechanical skip-proof
  (.loop/m4-foh97-fohflows-skip.txt). Device left clean (verified:
  lbc==1, gmenu2x live, no markers, scratch wiped, sentinel absent).
  **Driver next: review-95 ROUND 3 (closure-or-cap per the round-1
  triage header) over the iter-97 diff (check-device-foh.sh,
  foh_dev.c, normalize-foh-trace.js, platform_sdl1.c,
  platform_keymap.h NEW); task-14 notes: m4-freeze-manifest must
  re-pin platform_sdl1.c with this arc's GO; render-rung witness
  inheritance + match-phase witness + frontend-nav/live-branch
  exercise stand. Then task 12 (target test FOH + device).**
- **Remaining to gate**: 12 (target test FOH+device), 13 (SD persist),
  14 (verify_m4.sh) -> M4 GATE -> provision device -> LOOP STOP:
  m4-complete -> Chase acceptance playthrough.

## [superseded by iter 97] (2026-07-20, iter 96 — M4 hardening DONE: target-rig round-1 closure, all review-94 dispositions shipped; cold checks green first attempt)

- **Iter 96 (M4 hardening — target-rig round-1 closure, review-94
  triage H1/H2/M1-M5/L1/L2 ALL closed) DONE** (full entry: AGENT-LOG
  iter 96 pre-registration + result; latest AGENT-LOG id: iter 96).
  COLD, FIRST attempt: **`TARGET SIM CONFORMS (2 goldens: t01 t02;
  leaves=718 probe=ok teeth=12)`** exit 0
  (.loop/m4-tgt96-donecheck.log) + **`TARGETS OK`**
  (.loop/m4-tgt96-check-targets.log) + **`SIM CONFORMS`** 8/8
  (.loop/m4-tgt96-check-sim.log). FROZEN artifacts byte-untouched; NO
  re-record, NO re-freeze. **H1**: NEW SHARED
  port/goldens-m4/validate-target-manifest.js (freezer grammar
  extracted verbatim; module + CLI) run by EVERY done-check consumer
  (freeze/wrap/verify/record/check); T12 duplicate-id tooth dies
  naming the dup. **H2**: verify-target-stream.js binds frozen params
  == the validated manifest row (frames/seed/char/tstage/minTargets/
  wantArticles/trace), the playerStream sibling by name + its OWN
  seal + cross-pins + run rngCalls equality, and re-judges mechanical
  quality (finalTargetsDestroyed >= minTargets, finalEndTargetGame ==
  false) from the frozen metadata. **M1**: exact frozen-file whitelist
  schema + typed run pins — `0/0 MATCH`/undefined===undefined
  structurally dead. **M2**: wrap-target.js positional exact-token
  grammar (2N+3 lines, canonical integer text, strict section order);
  corpus: archived sim.out re-wraps byte-identical. **M3**:
  run-target.js captures the page-own __frameCount at
  __serializeState call time with +1-monotonicity death; proof = 2
  fresh probes (browser 2/4) STREAM MATCH + TARGET STREAM MATCH vs
  the frozen goldens (.loop/m4-tgt96-probe.log). **M4**: nested
  Vec2D/Box2D exact-key-set hard-throws (measured: 1155/{x,y},
  95/{min,max}; live-registry teeth fire). **M5 (pre-registered
  AMENDMENT)**: the triage's "max 9 (tstage6)" REFUTED — measured
  authored max is **10** (8 stages x10; == the upstream 10-slot
  targetDestroyed literal); shipped ONE ML_MAX_TARGETS 10
  (target_play.h + schema twin, static-asserted, loud death, 16-slot
  OOB window dead). **L1**: x2 browser-identity refusal in
  freeze-target.js. **L2**: frozen-side teeth T7-T11 on COPIES
  (control + frame-nibble/numbering/seal/metadata/sibling-seal
  deaths in the PRODUCTION judge); verdict teeth 6->12. Run ledger:
  browser 2/4, cold check-target-sim 1/2 + check-sim 1/1 +
  check-targets 1; corpus zero false rejections
  (.loop/m4-tgt96-corpus.log).
  **Driver next: review-94 ROUND 2 over the iter-96 diff — including
  the Tier A+ INDEPENDENT second review (grok) of
  verify-target-stream.js on the hardened bytes + the A+ byte-identity
  regression on the archived iter-94 runs (already exercised in
  .loop/m4-tgt96-corpus.log [d]); then task 12 (target test FOH +
  device).**
- **Remaining to gate**: 12 (target test FOH+device), 13 (SD persist),
  14 (verify_m4.sh) -> M4 GATE -> provision device -> LOOP STOP:
  m4-complete -> Chase acceptance playthrough.

## [superseded by iter 96] (2026-07-20, iter 95 — M4 hardening DONE: device-FOH round-1 closure, all review-93 dispositions shipped; cold check green first attempt)

- **Iter 95 (M4 hardening — device-FOH round-1 closure, review-93
  triage H1/H2/M1/M2/M3/M4/L ALL closed) DONE** (full entry:
  AGENT-LOG iter 95 pre-registration + result; latest AGENT-LOG id:
  iter 95). COLD, FIRST attempt: **`DEVICE FOH OK (flows=5 shots=13
  bridge=1 states=3 opk=evidence fbwit=15 p99=13.754ms skips=0
  underruns=0 starves=0 starts f01-vs-g01=281 f02-cpu-m01=16
  f03-options=23 f04-nav=39 f05-vs-g03=14 teeth=12)`** exit 0
  (.loop/m4-foh95-donecheck.log). **H1**: present witness form (a)
  LANDED — foh_dev --fb-witness reads the DISPLAYED kernel-fb page
  post-present per sampled shot (measured pins: identity transform
  unique, ll=480, vyres=720, yoffset always 0, visible page only
  readable — probe .loop/m4-foh95-probe2.log); a dead presenter now
  dies in-app; 15/15 shots witnessed. **H2**: keymap SSOT
  port/foh/keymap-frozen.txt (sha-pinned; consumed by
  flow-to-fkscript, emitted by foh_dev --dump-keymap byte-exactly,
  asserted against platform_sdl1.c's poll table) + permanent DEVICE
  tooth: A<->B-swapped injector mapping through the REAL uinput->SDL
  chain DIES at the judge. **M1**: bounded-delta trace judgment
  (measured-then-frozen: ID 2 / anchor 40..240 / dev -90..+30) on all
  5 legs — mid-run stalls no longer normalize away (fresh anchors
  76-91). **M2**: foh-args sentinel removed + absence-verified
  pre-verdict, trap-covered. **M3**: opk=evidence (task-14 deferral
  named). **M4**: ordered 23-key timing whitelist + prefix-resemblance
  death on all four summary parsers (corpus: zero false rejections).
  **L**: PORTABILITY rows (check, fb pins, keymap, cadence bounds).
  check-foh-flows.sh: mechanical skip-proof
  (.loop/m4-foh95-fohflows-skip.txt — zero shared surfaces). Run
  ledger: probe x2 (attempt 1 aborted by an instrument partial-read
  defect, fixed) + cold check x1; arm rebuilds 3 vs planned 2
  (overrun recorded, cause mechanical). Device left clean (verified).
  **Driver next: review-93 ROUND 2 over the iter-95 diff
  (check-device-foh.sh, foh_dev.c, flow-to-fkscript.js,
  normalize-foh-trace.js, keymap-frozen.txt, PORTABILITY delta);
  registered task-14 notes: render-rung witness inheritance,
  match-phase witness, frontend-nav + live-branch exercise. Then
  task 12 (target test FOH + device) after the iter-94 target arc.**
- **Remaining to gate**: 12 (target test FOH+device), 13 (SD persist),
  14 (verify_m4.sh) -> M4 GATE -> provision device -> LOOP STOP:
  m4-complete -> Chase acceptance playthrough.

## [superseded by iter 95] (2026-07-19, iter 94 — M4 task 11 DONE: target test data + sim plane, host; cold check green first attempt)

- **Iter 94 (M4 task 11 — target test, data + sim plane, host) DONE**
  (full entry: AGENT-LOG iter 94 pre-registration + result; latest
  AGENT-LOG id: iter 94). COLD: **`TARGET SIM CONFORMS (2 goldens:
  t01 t02; leaves=718 probe=ok teeth=6)`** exit 0, FIRST attempt
  (.loop/m4-task11-check-target-sim-run1.log). NEW pipeline stage
  `targets` (TTAB1, FORMATS.md §6): 10 authored target stages via
  upstream's own tstages aggregator -> window.__targetStages, 718-leaf
  C-vs-executed-JS round trip, expected.json `targets` section
  measured-then-frozen. NEW sim plane `port/sim/target/` (target_play,
  target_main = sim_host_target, target_hq_probe). NEW goldens t01
  fox/tstage1 (2 laser/article breaks) + t02 falcon/tstage2 (2 melee
  breaks), browser-recorded x2-identical, frozen spec-v1 player stream
  + the SEPARATE target-plane stream; C replays BOTH bit-exact
  3600/3600.
  **TWO MEASURED REFUTATIONS of the task text (permanent; do NOT
  re-litigate)**: (1) polygonMap exists on NO authored stage (builder
  plane only) -> pinned ABSENT; (2) NO authored stage carries
  damageType -> the "untrap the stage-damage hq rows" premise is FALSE,
  the path stays legitimately zero-live, sim_tick.c:355's VS trap
  STAYS, and the honest deliverable is the NEW standing
  `target_hq_probe` covering the already-translated CONSUME path
  (drop-arm tooth proven).
  REGRESSIONS: `SIM CONFORMS` 8/8 cold + `PIPELINE OK` cold (the M1
  gate EXTENDED: 38832 + 412 + 718 leaves). Sim-TU edits were
  visibility-only (two collision helpers static->extern, matching
  targetplay.js's imports) + ML_MAX_LEDGES 8->16 (targetstage8 has 16);
  behavioral identity proven by the bit-exact 8/8.
  Caps: browser 5/8, cold checks 1/4, docker 1/2, C iterations 7/60.
  **Driver next: Tier-A arc over the iter-94 non-checksummed surfaces
  (check-target-sim.sh, run-target.js, record-target.sh,
  freeze-target.js, wrap-target.js, check-target-quality.js,
  check-targets.sh) + Tier A+ for verify-target-stream.js (judge
  path); then task 12 (target test FOH + device).**
- **Remaining to gate**: 12 (target test FOH+device), 13 (SD persist),
  14 (verify_m4.sh) -> M4 GATE -> provision device -> LOOP STOP:
  m4-complete -> Chase acceptance playthrough.

## [superseded by iter 94] (2026-07-19, iter 93 — M4 task 10 DONE: FOH on device, first full device attempt green)

- **Iter 93 (M4 task 10 — FOH on device) DONE** (full entry: AGENT-LOG
  iter 93 pre-registration + result; latest AGENT-LOG id: iter 93).
  COLD: **`DEVICE FOH OK (flows=5 shots=13 bridge=1 states=3 opk=1
  p99=13.585ms skips=0 underruns=0 starves=0 starts f01=281 f02=16
  f03=23 f04=39 f05=14 teeth=9)`** exit 0
  (.loop/m4-task10-donecheck.log; attempt 1 died host-side at a pin
  grep, zero device runs consumed). The M3 DEFER-BOUND binding is
  HONORED: all 5 committed flows drove the device through fk_input →
  uinput → SDL keysyms → platform_poll, judged vs the SAME frozen
  traces (normalized frame-elision only); swap teeth T1-T3 prove the
  kill chain. f01's FOH-launched match: STREAM MATCH 3600/3600 vs
  frozen g01 ON DEVICE with render+SFX+music. 13/13 device shots
  BYTE-EXACT vs host twin. SSS RANDOM measured seeded
  (stageselect.js:80-84) → registered exclusion, slot
  visible-but-refusing (f04 extended + re-frozen; judge re-pinned
  same commit). OPK: FOH launcher (mlfk-foh.sh, unique evidence
  title) mounted + entered the FOH, boot marker == stamp; play
  install untouched; frontend-nav = task 14 per the iter-73 note.
  REGRESSION: FOH FLOWS OK (flows=5 shots=13 bridges=3 states=4
  diverge=1 control=1 teeth=16) cold
  (.loop/m4-task10-fohflows-run2.log). Device left clean (lbc=1,
  gmenu2x live, no markers, scratch wiped).
  **Driver next: Tier-A arc over the iter-93 surfaces
  (check-device-foh.sh, foh_dev.c, flow-to-fkscript.js,
  normalize-foh-trace.js, mlfk-foh.sh, riglib delta, foh.c/foh_render
  delta); registered-only surfaces: foh_dev --bridge live +
  mlfk-foh.sh live mode (structural review only this iteration).**
- **Remaining to gate**: 11-12 (target test), 13 (SD persist), 14
  (verify_m4.sh) → M4 GATE → provision device → LOOP STOP:
  m4-complete → Chase acceptance playthrough.

## [superseded by iter 93] (2026-07-19, driver post-iter-92 — BOTH task-7/9 arcs CLOSED; task 10 dispatching)

- **FOH ARC CLOSED (GO round 4, .loop/review-92-1.log)** after iters
  90/91/92 (7f42cc7, c37b47b, a7621d8 — all driver-cold-verified +
  pushed; final form: FOH FLOWS OK flows=5 shots=13 bridges=3
  states=4 diverge=1 control=1 teeth=16; one capped Low recorded:
  exotic-filename enumeration class). MUSIC ARC CLOSED (GO r2)
  earlier this session. M4 rig backlog clear.
- **In flight: iter-93 = M4 task 10 (FOH on device)** — brief carries
  the DEFER-BOUND M3 binding (real platform_poll/keysym path via
  fk_input, same frozen traces), judge sha re-pin rule, menu SFX/music
  selection wiring, OPK→FOH launcher, SSS RANDOM measurement ruling,
  riglib hygiene, paced-run caps. done-check:
  `bash port/foh/check-device-foh.sh` → DEVICE FOH OK.

## [superseded by driver post-iter-92] (2026-07-19, iter 92 — M4 hardening DONE: FOH-arc round-3 residuals closed, cold check green first attempt)

- **Iter 92 (M4 hardening MICRO — FOH-arc round-3 closure: control
  flow-ID confound + dotfile enumeration) DONE** (full entry:
  AGENT-LOG iter 92 pre-registration + result; latest AGENT-LOG id:
  iter 92). Both review-91 round-3 findings (.loop/review-91-1.log,
  NO-GO: one High + one capped Low) shipped: **H** — the [4w]
  treatment and control now share ONE flow id (`wit-g01`, sibling
  dirs build/check/wit/ vs ctrl/); the control-trace derivation's
  header-substitution normalization is DELETED (want = witness trace
  minus `S 415 lcancel 1` + LAUNCH lcancel 1→0, byte-exact cmp header
  included). ID-independence proven by MEASUREMENT: dev probe on the
  iter-91 residue + standing tooth T15 — same flow bytes under a
  renamed basename change ONLY the trace header, sim stream +
  BRIDGE-STATE byte-identical; the treatment pin 9cd2843d…ecb7f is
  rename-invariant, NOT re-frozen. **L** — judge_shot_inventory is
  dotfile-inclusive (find, rc case-split); tooth T16 plants
  `.unexpected.ppm` and dies in the production judge. Teeth 14→16.
  check-foh-flows.sh ONLY surface touched; flows/ +
  judge-foh-trace.js BYTE-UNCHANGED (pin valid). COLD: **FOH FLOWS OK
  (flows=5 shots=13 bridges=3 states=4 diverge=1 control=1
  teeth=16)** first attempt (.loop/m4-foh92-donecheck.log;
  post-commit .loop/m4-foh92-donecheck2.log; skip-proofs
  .loop/m4-foh92-checksim-skip.txt). Caps held: cold 2/2, dev 1/2.
  **Driver next: FOH arc ROUND 4 reviews this commit's diff (closure
  round — if it raises only variants of the closed classes, the arc
  CAPS naming the class, per the triage header); then task-10
  dispatch (device FOH; M3 defer-bound binding verbatim).**

## [superseded by iter 92] (2026-07-19, iter 91 — M4 hardening DONE: FOH-arc round-2 residuals closed, cold check green first attempt)

- **Iter 91 (M4 hardening — FOH-arc round-2 closure: witness control +
  hash pins, stderr capture, shots-b inventory) DONE** (full entry:
  AGENT-LOG iter 91 pre-registration + result; latest AGENT-LOG id:
  iter 91). All three review-90 round-2 FIX dispositions
  (.loop/review-90-triage.md, review-90 NO-GO .loop/review-90-1.log)
  shipped: **H** — [4w] now runs a TREATMENT+CONTROL+PIN triad: the
  lcancel=0 CONTROL (derived mechanically from the witness flow, only
  the `I 415 A` press pair deleted) fully MATCHES frozen g01
  (whole-log byte-exact), the witness report's hashes are BOUND
  (frozen side == the g01 frame-1 entry read mechanically; run side ==
  the measured-then-frozen treatment pin 9cd2843d…ecb7f), and
  validate_run_shape() re-validates all 5 wrapped runs; **M1** —
  verify_capture() puts stderr INTO the byte-judged verify log
  (corpus-validated stderr-quiet, zero false rejections); **M2** —
  judge_shot_inventory() exact-set on BOTH shots-a and shots-b. Teeth
  10→14 (T11 pin death · T12 control-diverged death · T13 stderr
  wrapper through the production capture · T14 shots-b plant).
  check-foh-flows.sh ONLY surface touched; flows/ + judge-foh-trace.js
  BYTE-UNCHANGED (pin valid). COLD: **FOH FLOWS OK (flows=5 shots=13
  bridges=3 states=4 diverge=1 control=1 teeth=14)** first attempt
  (.loop/m4-foh91-donecheck.log; post-commit
  .loop/m4-foh91-donecheck2.log; skip-proofs
  .loop/m4-foh91-checksim-skip.txt). Caps held: cold 2/3, dev 1/3.
  **Driver next: FOH arc ROUND 3 reviews this commit's diff (closure
  round — if it raises only variants of the closed classes, the arc
  CAPS naming the class, per the triage header); then task-10
  dispatch (device FOH; M3 defer-bound binding verbatim).**

## [superseded by iter 91] (2026-07-19, driver post-iter-90 — MUSIC ARC CLOSED at GO r2; FOH arc round 2 in flight)

- **MUSIC ARC CLOSED (GO round 2, zero findings —
  .loop/review-89-1.log)**: iters 87+89 surfaces fully reviewed;
  riglib manifest row flipped to reviewed-go this commit (anchor
  recomputed, self-check ALL ROWS GREEN). Iter-90 (7f42cc7)
  driver-cold-verified (FOH FLOWS OK flows=5 diverge=1); FOH arc
  ROUND 2 reviews its diff (check-foh-flows.sh + flows/ only) — on
  GO, task-10 writer dispatches (FOH device; M3 defer-bound binding:
  real platform_poll/keysym path, judge sha re-pin, menu SFX/music
  selection, OPK→FOH; SSS RANDOM seeded-draw ruling at brief time).

## [superseded by driver post-iter-90] (2026-07-19, iter 90 — M4 hardening DONE: FOH-arc round-1 findings closed, cold check green first attempt)

- **Iter 90 (M4 hardening — FOH-arc round-1 closure: divergence
  witness, f05/g03 bridge, exact grammars) DONE** (full entry:
  AGENT-LOG iter 90 pre-registration + result; latest AGENT-LOG id:
  iter 90). All 7 review-88 FIX dispositions
  (.loop/review-88-triage.md, review-88 NO-GO) shipped: H1 honest
  computed verdict counts + the NEW [4w] DIVERGENCE WITNESS (check-
  owned synthetic flow: lcancel=1 + g01 params → verify-stream vs
  frozen g01 MUST report `first divergence at frame 1 of 3600`, rc 2,
  exact 3-line grammar; MATCH = death — the FOH-fed settings plane
  demonstrably reaches ticking; f03 state witness kept); M1 f04 now
  traverses sss→css(B) (15/15 pinned edges frozen; .expect re-frozen
  via the designed channel); M2 NEW flow f05-vs-g03 (p2Char=2
  stream-load-bearing, pstadium; FIRST-CONTACT STREAM MATCH
  rngCalls=119) — **BRIEF AMENDMENT registered: the triage's d9/m02
  form is UI-unreachable (css.js:326-327 slider domain 1..4, 2nd
  instance of the iter-88 amendment class); m02 stays load-bearing in
  check-ai-live**; M4 whole-log BYTE-EXACT verdict construction from
  frozen rngCalls (corpus-validated vs archived iter-88 logs); M5
  exact P6 structural validation per shot both runs; M6 T5 runs the
  PRODUCTION judge_shot_pair; L1 T1 same-header variant + exact
  first-divergent-pair assert. M3 remains DEFER-BOUND to task 10
  (binding note in the triage must appear in the task-10 brief).
  foh.c/foh.h/foh_app.c/judge-foh-trace.js BYTE-UNCHANGED (judge pin
  valid, no re-pin). COLD: `bash port/foh/check-foh-flows.sh` →
  **FOH FLOWS OK (flows=5 shots=13 bridges=3 states=4 diverge=1
  teeth=10)** first attempt (.loop/m4-foh90-donecheck.log; teeth
  10/10 .loop/m4-foh90-teeth.log; post-commit rerun logged
  .loop/m4-foh90-donecheck2.log). Regression skip-proofs: port/sim +
  port/gfx diffs EMPTY (.loop/m4-foh90-checksim-skip.txt). Run caps
  held: cold 2/4, witness probes 1/3. **Driver next: (1) FOH arc
  ROUND 2 on this commit's bytes (review the hardening diff:
  check-foh-flows.sh + flows/); (2) music-arc round 2 + the
  wrap-run.js re-pin verify (iter-89 notes) unchanged; (3) task 10
  (device FOH) MUST carry the M3 binding note verbatim.**

## [superseded by iter 90] (updated: 2026-07-19, iter 89 — M4 hardening DONE: music-arc round-1 findings closed, both cold checks green)

- **Iter 89 (M4 hardening — music-arc round-1 closure: metadata pins,
  atomics, exact grammars) DONE** (full entry: AGENT-LOG iter 89
  pre-registration + result; latest AGENT-LOG id: iter 89). All 9
  triage dispositions (.loop/review-87-triage.md, review-87 NO-GO)
  shipped as frozen: H1a metadata pin tables (MUSIC_META_PINS enforced
  inside read_music; device constants + cross-grep), H1b COMPUTED
  wraps/eofsilence counters asserted vs frozen expectations before the
  verdict, M1 set-bound pin tables (pin_setcheck, dup+omit dies naming
  the track), M2 g_mus_quit -> C11 atomics + wr/outPos audit (device =
  real SDL lock; headless = no consumer thread; only quit/done cross
  threads, both atomic), M3 exact-token music-summary grammar +
  final-newline assertion (corpus-validated, zero false rejections),
  L1 exact-line pack/T4 verdicts, L2 needle-free failure diagnostic,
  L3 bounded reader join (5 s deadline + --tooth-music-wedge proof),
  riglib RIG_SCRIPTS += check-device-music.sh + manifest re-pin +
  anchor. COLD: `check-music-fidelity.sh` -> **MUSIC FIDELITY OK
  (goldens=12 tracks=8 diff=bit-identical wraps=2 eofsilence=1)**
  (computed form, .loop/m4-mus89-musicfid.log); done-check
  `check-device-music.sh` -> **DEVICE MUSIC OK (full p99 13.433 ms,
  underruns 0, starves 0, refill-read p99 1.357 ms, skips 0/3600)**
  first paced attempt (.loop/m4-mus89-donecheck.log); regression
  `check-mixer-fidelity.sh` -> MIXER FIDELITY OK
  (.loop/m4-mus89-reg-mixer.log); check-sim SKIP proof
  (.loop/m4-mus89-checksim-skip.txt). Teeth all fired (T-META,
  T-PIN-SET, T-EOF, parser 00/torn-line, T-WEDGE rc=3, bf-meta,
  T-NEEDLE static probe — .loop/m4-mus89-teeth.log). Device runs 1/2
  cap. **SURPRISE registered (driver-owned, untouched here): the
  m3-freeze-manifest wrap-run.js row is STALE since iter 81 (commit
  315f8c5 changed the file, no re-pin) — verify_m3.sh would refuse;
  self-check otherwise ANCHOR GREEN 23/24
  (.loop/m4-mus89-manifest-selfcheck.log). Driver next: (1) music-arc
  round 2 on this commit's bytes; (2) reviewed wrap-run.js re-pin;
  (3) FOH Tier-A arc + task 10 unchanged from iter 88.**

## [superseded by iter 89] (updated: 2026-07-19, iter 88 — M4 task 9 DONE: FOH core + menu flows host, FOH FLOWS OK)

- **Iter 88 (M4 task 9 — FOH core + menu flows, host) DONE** (full
  entry: AGENT-LOG iter 88 pre-registration + result; latest AGENT-LOG
  id: iter 88): NEW `port/foh/` — the REWRITTEN screen machine
  (startup→title→menu(top/battle/options/controls)→css→sss→match +
  options-gameplay; every edge cited from upstream in foh.h; menu
  entries for excluded/deferred screens stay visible and REFUSE with
  structural events), self-authored 5x7 font, FLOW1 scripts ×4 +
  frozen FOHTRACE1 traces + BRIDGE-STATE witnesses
  (port/foh/flows/*.expect). Cold done-check
  `bash port/foh/check-foh-flows.sh` → **FOH FLOWS OK (flows=4
  shots=11 bridges=3 streams=MATCH teeth=6)** exit 0
  (.loop/m4-task9-donecheck.log). MATCH-LAUNCH BRIDGES first-contact,
  ZERO divergence rounds: FOH-selected params (never CLI) →
  sim_setup_match → full 3600-frame streams judged by the UNCHANGED
  wrap-run.js/verify-stream.js — f01 == frozen g01 (rngCalls=134),
  f02 == frozen m01 on LIVE C AI (rngCalls=59); f03 BRIDGE-STATE
  proves options edits reach the GameState slice. Judge:
  judge-foh-trace.js (whitelist grammar + the 15-edge PINNED flow
  graph + T-chain continuity; corpus-validated, zero false
  rejections). BRIEF AMENDMENT (evidence): upstream difficulty slider
  domain is 1-4 (css.js:316-329), not 1-9 — CPU bridge golden is m01
  (d1), g08's d5 is unreachable through the faithful UI. Teeth 6/6
  (.loop/m4-task9-teeth.log). check-sim/check-render SKIPPED justified
  (port/sim AND port/gfx diffs EMPTY,
  .loop/m4-task9-checksim-skip.txt). Registered deferrals: SSS RANDOM
  slot (seeded-draw ruling), menu SFX/music selection + device FOH →
  task 10, persistence → task 13, palettes/tags/versusMode. **Driver
  next: (1) Tier-A arc for the new FOH surfaces (check-foh-flows.sh /
  judge-foh-trace.js / foh_app.c / foh.c — every FOH surface is
  Tier A per the §M4 conventions); (2) the iter-87 music-surface
  Tier-A arc + RIG_SCRIPTS residual still queued; (3) task 10 (device
  FOH) is unblocked — handoff notes in the AGENT-LOG result entry.**

## [superseded by iter 88] (updated: 2026-07-19, iter 87 — M4 task 7 DONE: music streaming, DEVICE MUSIC OK)

- **Iter 87 (M4 task 7 — music streaming: mixer music channel + SD
  double-buffer reader) DONE** (full entry: AGENT-LOG iter 87
  pre-registration + result; latest AGENT-LOG id: iter 87; completed
  by a resumed writer after a mid-task session wipe —
  .loop/m4-task7-HANDOFF.md was the binding contract): snd_mixer.h
  gained a DEDICATED music channel (ring 32768 / chunk 16384 = PLAN §7
  2x64 KB; sounds.json sprite windows, floor(ms*441/20); ZOH 2x; Q8
  per channel before the single clamp; past-EOF = silence — fod quirk
  verbatim; disabled = byte-identical fill, cold MIXER FIDELITY OK
  re-proven); gfx_app.c streams it from SD on a pthread reader (25 ms
  poll, chunk refills under platform_audio_lock, prefill before audio
  start, --music-lat sidecar, separate `gfx_app music:` grammar — all
  prior pinned grammars byte-unchanged). NEW checks:
  `port/gfx/check-music-fidelity.sh` → MUSIC FIDELITY OK (12 goldens
  bit-identical WITH stage-track music vs the independent reference +
  menu-wrap/fod-EOF/targettest synthetic legs; 8-track sha pins; teeth
  T1-T4+grammar) and the done-check `port/gfx/check-device-music.sh` →
  **DEVICE MUSIC OK (full p99 13.635 ms, underruns 0, starves 0,
  refill-read p99 1.594 ms, skips 0/3600)**
  (.loop/m4-task7-donecheck.log; refills 80, musout == cbs*512, T5
  PCM-corruption tooth, SD dd ~21.2 MiB/s). Regressions: cold MIXER
  FIDELITY OK (.loop/m4-task7-reg-mixer.log) + cold DEVICE RENDER OK
  12.539 ms p99 (.loop/m4-task7-reg-render.log); check-sim SKIPPED
  justified (port/sim diff EMPTY, .loop/m4-task7-checksim-skip.txt).
  Music-selection seam verdict: RENDER-PLANE (main.js:1342 stage→track
  switch, zero RNG). **Driver next: (1) Tier-A arc for the NEW audio
  surfaces (check-music-fidelity.sh / check-device-music.sh /
  snd_mixer.h music channel / gfx_app music path) — include the
  registered residual: add check-device-music.sh to riglib RIG_SCRIPTS
  when that frozen surface thaws; (2) the pending round-3 FINAL
  CONFIRM arcs (iter-86 note); (3) tasks 9-10 note: menu/targettest
  music SELECTION is FOH's surface — the fidelity check already covers
  their windows via synthetic legs.**

## [superseded by iter 87] (2026-07-19, session handoff — iter-87 task-7 writer was in flight at handoff; ground-truth its state from disk per the ccebc9b precedent)

- **Iter 86 (M4 hardening — BOTH arcs' round-2 residuals: review-83
  1 Medium + review-84 5 Mediums/2 Lows) DONE** (full entry: AGENT-LOG
  iter 86 pre-registration + result; latest AGENT-LOG id: iter 86):
  (1) ai-live cov artifacts made()-guarded; (2) NEW shared-scratch lock
  `port/sim/calib/build/shared-scratch.lock` taken by all three
  calib-build/sim-tables consumers (mixer/ai-live/vfx-seam — they now
  serialize; T-SLOCK: all three refuse under a held lock); (3) T5 tooth
  made(); (4) record-m4.sh per-id crafted-generator dispatch (missing
  generator = death naming it — T-GEN proven, s01 fallback dead);
  (5) snd_render.c exact-token schedule grammar (P 075 / plays=060 /
  elastic-whitespace probes DIE; 12/12 genuine schedules accepted —
  zero false rejections; standing teeth T6c/d/e added); (6) freezer
  strict specVersion validation on both refreeze sides (corrupt/
  missing/string = corruption death; same-spec refusal intact —
  T-FREEZER); (7) GUARD/GUARDON shield_depletion deduped into shared
  `mv_shield_depletion` (moves_index.c — the sibling-drift class closed
  at the root; producer pins untouched); (8) snd_mixer.h 2^53 play-id
  bound assert. Colds: **MIXER FIDELITY OK** (12/12, all teeth;
  .loop/m4-iter86-donecheck.log) + **AI LIVE CONFORMS**
  (.loop/m4-iter86-donecheck2.log); perturbed contract artifacts
  restored cmp-identical. **Driver next: both arcs round 3 = FINAL
  CONFIRM reviews on this commit's bytes (closure checks of
  review-83/review-84 residuals).**

## [superseded by iter 86] (2026-07-19, iter 85 — M4 micro DONE: GUARDON depletion-break fix + s02 scenario golden, browser-verified)

- **Iter 85 (M4 micro — the review-82 un-triaged High: GUARDON.c:56
  dropped as_shieldDepletion's return) DONE** (full entry: AGENT-LOG
  iter 85 pre-registration + result; latest AGENT-LOG id: iter 85):
  GUARDON.c's shield_depletion now dispatches SHIELDBREAKFALL.init on
  the break flag — GUARD.c's iter-82 form verbatim. Browser-verified
  by NEW crafted golden **s02-marth-fox-guardon-break-battlefield**
  (gen-s02-trace.js; the shield depletes to break ON a GUARDON frame —
  sim frame 596, GUARDON timer 6 — then FURAFURA/DAMAGE/KO, full
  quality contract, browser x2-identity, rngCalls=39): C replay
  STREAM MATCH 3600/3600. Tooth: fix reverted -> divergence at frame
  596 exactly; restored -> match. T3 sweep: 460 non-void as_* call
  sites — no sim/move TU drops a return; the
  zero-coverage-dispatch-scaffold class (GUARD iter 82, GUARDON iter
  85) is CLOSED mechanically (.loop/m4-iter85-sweep.log). s02 JOINED
  the mixer corpus (check-mixer-fidelity.sh inventory 3->4 +
  DIFF_COUNT 12 + measured exposure row `s02 4 0 2 2 0`; aggregate
  pins unchanged; s01-only witness/app legs not extended).
  Regressions: SIM CONFORMS (.loop/m4-iter85-checksim.log) + post-
  commit cold MIXER FIDELITY OK / AI LIVE CONFORMS (see the result
  entry's amended ledger). **Driver next: the queued Tier-B sim-TU
  review round now has its GUARDON High CLOSED; mixer arc round 2
  reviews should use the complete diff incl. this commit.**

## [superseded by iter 85] (2026-07-19, iter 84 — M4 hardening DONE: mixer-rig round 1 CLOSED + goldens-snd FOLDED, both cold checks green)

- **Iter 84 (M4 hardening — review-82 round-1 closure: stop witnesses +
  exposure pins + the goldens-snd fold) DONE** (full entry: AGENT-LOG
  iter 84 pre-registration + result; latest AGENT-LOG id: iter 84):
  s01 now lives in port/goldens-m4/ (git mv, stream/trace/generator
  BYTES UNCHANGED — sha proof in the entry; freezer re-freeze from the
  archived record-s01 runs → 'unchanged (byte-identical re-freeze)');
  port/goldens-snd/ deleted whole (recorder/freezer twins dead);
  freeze-stream-m4.js id grammar ^[ms][0-9]{2}$ + x2-identity
  (path/dev:ino) + mechanical --refreeze spec-bump proof; record-m4.sh
  crafted-trace refusal for s-ids. check-mixer-fidelity.sh hardened:
  four FROZEN s01 stop witnesses (frame/sound/preceding-play-id; the
  mixer stop counter now splits matched/unmatched on both differential
  sides — unmatched pinned 0 everywhere), per-golden EXPOSURE_PINS
  (over-cap set pinned {g06,m02}, peak 9, steals 2), 3-producer sha
  pin table + full both-manifest inventory binding + eval-kill, run
  lock + rm-before-produce/made(), file-byte verdict grammars,
  git-guard rc case-split. check-vfx-seam.sh check-sim leg = forced
  cold 10-line shape (the iter-83 residual closed). Cold:
  `bash port/gfx/check-mixer-fidelity.sh` → MIXER FIDELITY OK
  (.loop/m4-mixrig84-donecheck.log) + `bash port/sim/check-ai-live.sh`
  → AI LIVE CONFORMS (.loop/m4-mixrig84-donecheck2.log — run-cap
  overage recorded honestly in the entry: guard-vs-uncommitted-fold
  bootstrap + one externally-interfered run; CLASS FLAG for the
  driver: $CAL/build is shared unlocked scratch across composed
  checks/sessions). Teeth all fired (.loop/m4-mixrig84-teeth.log).
  **Driver next: mixer arc ROUND 2 with the COMPLETE diff of d0927fa
  plus the iter-84 commit (the round-2 rule). LOUD: review-82's GUARDON.c:56 High
  (as_shieldDepletion return ignored — the raise-arm depletion break)
  is NOT in the triage and NOT fixed here (sim TU, out of surface) —
  it belongs to the queued Tier-B sim-TU round and must not be
  dropped.**

## [superseded by iter 84] (2026-07-19, iter 83 — M4 hardening DONE: ai-live rig arc round 1 CLOSED, AI LIVE CONFORMS)

- **Iter 83 (M4 hardening — review-81 round-1 closure: ai-live
  aggregator kit + golden-home grammar) DONE** (full entry: AGENT-LOG
  iter 83 pre-registration + result; latest AGENT-LOG id: iter 83):
  all 6 review-81 findings fixed on port/sim/check-ai-live.sh +
  port/goldens-m4/{record-m4.sh,freeze-stream-m4.js} — rm-before-
  produce + made() freshness (H), the vfx-seam evidence-grammar kit
  incl. the anchored M2-witness message (M1), a 5-producer sha256 pin
  table (M2), pinned-array inventory-execution binding (M3), and the
  eval-class kill + full manifest schema/dup/range/basename-
  containment grammar in the freezer (M4). Cold done-check
  `bash port/sim/check-ai-live.sh` → `AI LIVE CONFORMS` exit 0
  (.loop/m4-ailive83-donecheck.log); teeth 22/22
  (.loop/m4-ailive83-teeth.log — corpus-validated grammars, zero
  false rejections); end-to-end record-m4.sh m01 re-record →
  byte-identical re-freeze (.loop/m4-ailive83-record-pos.log).
  Frozen streams/manifest byte-untouched; check-sim.sh/
  check-ai-bridge.sh/check-ai-replay.sh/wrap-run.js/verify-stream.js
  untouched (now pinned). Registered residual (outside surface):
  check-vfx-seam.sh's check-sim leg false-rejects on a bridge-less
  build/ tree (8-vs-10 STREAM MATCH cold shape — the class ai-live
  now forces deterministic). **ai-live rig arc round 2 = CLOSURE
  pending (driver-scheduled re-review of the round-1 fixes); the
  task-6 audio-rig Tier-A arc + Tier-B sim-TU round still queued
  (iter-82 note).**

## [superseded by iter 83] (2026-07-19, iter 82 — M4 task 6 DONE: mixer fidelity, MIXER FIDELITY OK)

- **Iter 82 (M4 task 6 — mixer fidelity + real play-ids + stop-path
  coverage) DONE** (full entry: AGENT-LOG iter 82 pre-registration +
  result): the OFFLINE deterministic render differential is live —
  every golden's sound-event schedule (STREAM-MATCH-guarded tap) renders
  through the C mixer math (snd_render.c, offline, ×2 stable) and an
  INDEPENDENT reference (snd_reference.js from SND1 + vendored-howler
  semantics) to BIT-IDENTICAL PCM: 11/11 vs the capped-8 reference,
  9/9 vs browser-unlimited where concurrency ≤ 8 (measured exposure:
  g06/m02 peak 9 voices → 1 steal each on device vs browser — the
  8-voice cap is the PLAN §7 design choice). Real play-ids: one id
  plane in ml_events.c (howler-parallel play counter, off-surface),
  id-routed stops through ml_snd_stop_id_sink, mixer stop(id) = howler
  semantics; marth sbid replay preserved via ml_howl_id_oracle.
  FOUND+FIXED a zero-coverage latent integration bug: depletion shield
  break left the victim in GUARD (note-only as_dispatch scaffold) +
  SHIELDBREAKFALL.land's dead-path trap contradicted upstream's 2-arg
  land call. NEW GOLDEN s01 (port/goldens-snd/ — outside goldens-m4
  ONLY because of the concurrent review; folding it in post-arc is a
  registered driver decision): crafted marth-vs-fox shield-break
  scenario, browser ×2 first attempt, C replay bit-exact
  (3600/3600, rngCalls=57, zero divergence rounds), ALL FOUR in-match
  .stop arms live. Cold done-check
  `bash port/gfx/check-mixer-fidelity.sh` → `MIXER FIDELITY OK
  (goldens=11 diff=bit-identical maxvoices=9 steals=2 s01stops=4)`
  exit 0 (.loop/m4-task6-donecheck.log; composes check-sim.sh).
  Teeth 6/6 (incl. stop-id-skew + steal-flip on g06's real steal).
  Regressions: ASSHORT/MOVES SHARED/MOVES marth/HITDET MATCH +
  RENDER OK. Registered deferrals: device audio rung (id-routed stop
  semantics + offline-render device cmp) → task 7/14; sim_tick.c dead
  mv_howl_play_id counter cleanup → post-arc (no-edit window).
  **Driver next: Tier-A arc for the new audio-rig surfaces
  (check-mixer-fidelity.sh, snd_render.c, snd_reference.js,
  snd_events_tap.c, record-snd.sh/freeze-stream-snd.js) + Tier-B round
  for the sim-TU changes (shield-break chain, id plumbing); then task 7
  (music streaming — the differential rig + SND1 sprite windows are
  its seams).**

## [superseded by iter 82] (updated: 2026-07-19, iter 81 — M4 task 5 DONE: live CPU integration, AI LIVE CONFORMS)

- **Iter 81 (M4 task 5 — live CPU integration) DONE** (full entry:
  AGENT-LOG iter 81 pre-registration + result): the sim's runAI site now
  runs the REAL C ai.c LIVE (seeded-chain draws, bank + bookkeeping
  writes) via the `ml_sim_runai_live` pointer seam — constructor-
  installed by new port/sim/sim/sim_ai_live.c, linked only with ai.c so
  the FROZEN check-sim.sh build is symbol- and behavior-identical
  (its sha256 is now pinned inside the new check). AIBRIDGE1 stays as
  the archival --ai-bridge arm. ALL FOUR CPU goldens conform on the
  live path (unchanged verify-stream.js, zero divergence rounds):
  g07/g08 vs the frozen oracle streams + the NEW d1/d9 coverage
  goldens in port/goldens-m4/ — m01 falcon/CPU-marth(d1)/ystory seed
  8114 (first live CPU on a moving-platform stage) and m02
  falcon/CPU-fox(d9)/dreamland seed 8109 — browser ×2-identity,
  mechanical M0 quality contract, M0-format freeze (recorder reuses
  oracle/harness bytes by path; oracle/ untouched). Cold done-check
  `bash port/sim/check-ai-live.sh` → `AI LIVE CONFORMS` exit 0
  (.loop/m4-task5-donecheck.log; composes check-sim.sh bridge-fed +
  live legs + check-ai-bridge.sh + check-ai-replay.sh). Teeth 5/5
  (.loop/m4-task5-teeth.log). Rule-16 verdict: no capture adoption
  (no JS→C marshal on the live path; full-trace stream oracle is the
  binding check). Coverage delta measured (--ai-cover): FOX_* arms +
  GEN_TW_CLEAR newly live; marthAI d1 proves the `pdiff>=2` OFF side
  (MARTH_* zero-live registered). NOTE: the brief's "edit
  check-sim.sh" was refused per HARD RULE 3 + fix_plan §M4 conventions
  (recorded in both AGENT-LOG entries); gfx_app live mode deferred to
  the device tasks (pinned option surface).
  **Driver next: Tier-B round for the sim-TU changes + Tier-A arc for
  the new scripts (check-ai-live.sh, record-m4.sh, freeze-stream-m4.js,
  check-quality.js); then task 6 (mixer fidelity) — its stop-path live
  witness and offline-render differential are unblocked and the m4
  goldens/manifest machinery it can reuse now exists.**

## [superseded by iter 81] (updated: 2026-07-19, iter 80 — device-rig arc round-3 Medium CLOSED; device-rig round 4 = closure-or-cap)

- **Iter 80 (M4 hardening — device-rig round-3 Medium: restore-stamp
  causality coupling) DONE** (full entry: AGENT-LOG iter 80
  pre-registration + result; finding: .loop/review-78-1.log, VERDICT
  NO-GO on the one substantive Medium — qrestore.ts was stamped
  independently of rig_daemon_restore, so the bracket bounded
  app-end→marker, not app-end→actual restore): the stamp is now
  COUPLED to the operation — `rig_daemon_restore <name> <init-script>
  [<stamp-devpath>]` ITSELF writes the stamp only after its comm-scan
  verifies exactly one instance (new internal rig_restore_stamp; loud
  nonzero on an unwritable stamp); both callers stop writing it
  independently (render passes the path; skip-attrib's quiesce arm
  passes it per loop restore — surviving value = LAST daemon verified,
  bracket bounds the whole daemon-down window); trap/normalize paths
  stay 2-arg. Slack model follows the semantics: render post-slack
  stays 10 s, skip-attrib 10→15 s (2-daemon model, worst ~12 s).
  Teeth 8/8 (.loop/m4-rig80-teeth.log) incl. the reviewer's exact
  probes: old-scheme 3 s chore INVISIBLE (demonstrated) → coupled
  scheme kills it; in-helper comm-scan stall killed; coupling +
  compat + refusal arms proven on REAL bodies (transport stubbed).
  Cold check GREEN first attempt: `DEVICE RENDER OK … skips 0/3600`
  exit 0 (.loop/m4-rig80-donecheck.log, one arm rebuild as expected;
  live bracket `end->restore 3 s` — now includes the restore's own
  verify latency). Manifest re-pins (riglib + check-device-render,
  cite iter80) + anchor same commit, self-check 23/23 + ANCHOR GREEN
  (.loop/m4-rig80-manifest-selfcheck.log). Run cap held (1 paced run;
  skip-attrib not re-run — changed region is quiesce-arm-only,
  justified in the pre-reg). Device left clean. HONEST NOTE: the
  writer ended a turn waiting on the background run (failure mode #1)
  and was driver-nudged back to foreground polling; no evidence
  affected, logged in the AGENT-LOG result entry.
  **Driver next: device-rig arc ROUND 4 = closure-or-cap
  (grammar-variant re-raises → cap naming the class, per the triage
  ruling; round 3 closed M2/M3/M4 — only this Medium was
  substantive).**

## [superseded by iter 80] (updated: 2026-07-19, iter 79 — ai-rig arc round-2 residuals CLOSED; ai-rig round 3 = closure-or-cap)

- **Iter 79 (M4 hardening — ai-rig arc round-2 residuals, 2 Mediums)
  DONE** (full entry: AGENT-LOG iter 79 pre-registration + result;
  driver triage .loop/review-77-triage.md; RESPAWN iteration — the
  first writer died on a usage limit mid-teeth, this session reviewed
  + adopted its in-tree edits then ran all evidence fresh): M1 ERANGE
  guard (errno reset + `errno == ERANGE` in the fail condition) on the
  replay_ai_port.c frame strtol AND its class siblings (parse_expect
  counts, --cover-gate, --ncov-pin) — an overflowing decimal is now
  corruption death, never LONG_MAX saturation; M2 the coverage-table
  UNIVERSE is pinned: expected-capture-aiport.json `coverage`
  {ncov 64, liveArms 61, deadArms by NAME} + check-ai-replay.sh's
  frozen NCOV_FROZEN=64 twin (asserted before the lock) feeding
  --cover-gate/--ncov-pin/--dead-pin, replay asserts ncov-pin ==
  compiled ML_AI_NCOV + dead-pin names == compiled g_dead_arms
  (bijection); a grown named-but-unhit arm can no longer pass in
  lockstep. Teeth 6/6 with asserted death classes
  (.loop/m4-airig79-teeth.log): T5 positive control 0 divergences;
  T1 the reviewer's exact overflowing-frame probe → `malformed frame
  field` exit 3; T2 overflow --expect → EXPECT PARSE FAIL; T3a the
  reviewer's exact accident DEMONSTRATED (NCOV-65+PROBE_NEW_ARM probe
  binary PASSES under old flags) → T3b same probe dies under
  --ncov-pin 64 (COVERAGE PIN FAIL exit 2); T4 wrong dead-pin name →
  DEAD-ARM PIN FAIL exit 2. Cold check GREEN first attempt: `AI MATCH`
  exit 0 (.loop/m4-airig79-donecheck.log; run cap held, 1 composed
  run). No device surfaces touched.
  **Driver next: ai-rig arc ROUND 3 = closure-or-cap (grammar-variant
  re-raises → cap naming the class, per the triage ruling); device-rig
  round 3 runs concurrently (iter 78).**

## [superseded by iter 79] (updated: 2026-07-19, iter 78 — device-rig arc round-2 residuals CLOSED; device-rig round 3 = closure-or-cap)

- **Iter 78 (M4 hardening — device-rig arc round-2 residuals, 4
  Mediums) DONE** (full entry: AGENT-LOG iter 78 pre-registration +
  result; driver triage .loop/review-76-triage.md): M1 the quiesce
  window is now EXACT in both scripts — daemon stop is the last
  pre-launch action, restore is the FIRST device action after
  app-exit detection (ahead of rc pull/hash/cmp chores), with a
  STANDING device-clock bracket tooth (riglib
  rig_quiesce_bracket_assert over qstop/app.start/app.end/qrestore
  stamps, 10 s slacks; fired live on the cold render gate:
  stop->start 0 s, app 60 s, end->restore 2 s); M2 parseStat
  requires ALL NINE /proc/stat fixed-table line classes (clipped
  snapshot = corruption); M3 the verdict needle is suffix-free
  (`^SKIP ATTRIB VERDICT: \((a|b|c)\)$`, detail on a separate
  non-gating line; canonical AGENT-LOG line rewritten through the
  designed replacement channel); M4 twin-pin + argv EXACTNESS (new
  riglib rig_pin_assert_once / rig_argv_assert_once: exactly-one
  assignment per pinned var in BOTH scripts' bytes, all 20 gfx_device
  options exactly once in the extracted launcher region). Cold checks
  GREEN first attempt: `DEVICE RENDER OK ... skips 0/3600`
  (.loop/m4-rig78-donecheck.log, the expected one arm rebuild) +
  `SKIP ATTRIB OK (arm=sampler, skips=1/3600, events=50, stream
  MATCH)` (.loop/m4-rig78-donecheck2.log, stamp HIT). Teeth 21/21
  with asserted death classes (.loop/m4-rig78-teeth.log) incl. the
  reviewer's exact ' — superseded' and duplicate-SHOT_FRAME probes;
  manifest re-pins (riglib + check-device-render, cite iter78) +
  anchor same commit, self-check 23/23 + ANCHOR GREEN
  (.loop/m4-rig78-manifest-selfcheck.log). Run cap held (2 paced
  runs); device left clean (lbc ==1, no marker, scratch wiped,
  gmenu2x live).
  **Driver next: device-rig arc ROUND 3 = closure-or-cap
  (grammar-variant re-raises → cap naming the class, per the triage
  ruling); ai-rig round 2 runs concurrently (iter 77).**

## [superseded by iter 78] (updated: 2026-07-19, iter 77 — ai-rig arc round-1 findings CLOSED; ai-rig round 2 = closure pending)

- **Iter 77 (M4 hardening — task-4 ai-rig arc round-1 closure) DONE**
  (full entry: AGENT-LOG iter 77 pre-registration + result): ALL
  triage items closed (.loop/review-75-triage.md; review
  .loop/review-75-1.log r1 NO-GO) + the two round-1 findings outside
  the M-list (stale-capture High, run lock) closed with the standard
  classes. Highlights: spec-aiport.js recon bookkeeping allowlist is
  now PER-SLOT and the post `bk` is the FOUR-SLOT array (foreign-slot
  bookkeeping writes = wsViol in-page AND divergence in C; captures
  re-recorded by the check, pins counts invariant); replay gained
  --expect strict-grammar record inventory (REQUIRED under --strict;
  ferror-checked — truncation/read error = corruption death),
  representability guards before every captured-data cast (+ a
  SIBLING found by the audit: the frame field's NULL-endptr strtol,
  now full-token validated), ledgePos-empty rule-7 death, and
  --cover-gate 61 (live arms pinned per golden; the 3 documented-dead
  arms pinned ZERO); check-ai-replay.sh gained corpus inventory pin,
  no-reclaim run lock, freshness contract, rc-case-split hygiene
  guard; ai.c H_LEDGE_CTA comment now cites T4a/T4b (T4 refuted).
  Cold check GREEN: `AI MATCH` (.loop/m4-airig77-donecheck.log, 4×
  STREAM MATCH, 0 divergences, 61 live arms per golden); 12 teeth
  fired with asserted death CLASSES incl. the reviewer's three exact
  probes now dying (.loop/m4-airig77-teeth.log). check-sim skip
  justified mechanically (ai.o byte-identical HEAD vs worktree);
  bridge surface untouched (git status 0 lines). Composed runs 2/2,
  probe captures 1/2.
  **Driver next: ai-rig arc ROUND 2 (closure re-review of the iter-77
  surfaces); device-rig arc round 2 runs concurrently (iter 76).**

## [superseded by iter 77] (updated: 2026-07-19, iter 76 — device-rig arc round-1 findings CLOSED; rig arc round 2 = closure pending)

- **Iter 76 (M4 hardening — iters-73/74 device-rig arc round-1
  closure) DONE** (full entry: AGENT-LOG iter 76 pre-registration +
  canonical-needle section + result): ALL triage items closed
  (.loop/review-73-triage.md; review .loop/review-73-1.log r1 NO-GO).
  Highlights: **H** — the on-device deadman now backstops the daemon
  quiesce too (nonce-scoped qd markers, comm-scan-guarded hard-coded
  restore arms, marker cleared only on live rescan) and the quiesce
  window narrowed to exactly the paced run; TOOTH fired with real
  transport-death evidence (host SIGKILL + adb kill-server mid-paced
  -run → deadman restored BOTH frontend AND low_bat_check,
  .loop/m4-rig76-probe-h.log). NEW riglib `rig_qd_normalize` step-0
  chokepoint (stale markers restored before any disarm/wipe);
  rig_daemon_restore idempotent + exact-cardinality (device-probed
  6/6 incl. n=2 refusals); skip-attrib degraded-mode lockout
  (readonly SKA_AUTHORITATIVE, DEV banner + exit 3, OK sentinel
  structurally unreachable); full-line verdict-needle grammar +
  resemblance counter (canonical line appended to AGENT-LOG); NEW
  validate-ev.js (full EV whitelist grammar, 108-line corpus 0 false
  rejections, skip/event reconciliation, sampler arms require kernel
  windows on EVERY event — win=none fails closed); correlator
  validates ALL sampler payloads + full-line /proc/stat + vmstat
  dup/required-key grammars; rc files judged by exact bytes RC=0\n;
  skip-attrib runs the exact task-3 workload (sha twin-pins + full
  argv + judged shot); render check gained the RENDER_OK fail-closed
  exit guard. Cold checks GREEN: `DEVICE RENDER OK ... skips 0/3600`
  (.loop/m4-rig76-donecheck.log) + `SKIP ATTRIB OK (arm=sampler,
  skips=1/3600, events=42, stream MATCH)`
  (.loop/m4-rig76-donecheck2.log); 18/18 host teeth
  (.loop/m4-rig76-teeth.log); manifest re-pins (riglib +
  check-device-render, arc-pending cite iter76) + anchor + 23/23
  self-check (.loop/m4-rig76-manifest-selfcheck.log). Device left
  clean (lbc ==1, no marker, scratch wiped, gmenu2x live). Paced
  runs: 2 cold + 1 partial H-probe; one arm rebuild (stamp).
  **Driver next: rig arc ROUND 2 (closure re-review of the iter-76
  surfaces); task-4's ai.c arc still pending separately.** Low
  disposition on record: fdlibm lround boundary sweep registered for
  the next fdlibm-touching iteration.

## [superseded by iter 76] (updated: 2026-07-19, iter 75 — M4 task 4 DONE: ai.js C port, AI MATCH)

- **Iter 75 (M4 task 4 — ai.js structure-parallel C port) DONE** (full
  entry: AGENT-LOG iter 75 pre-registration + result): cold done-check
  `bash port/sim/calib/check-ai-replay.sh` → `AI MATCH` exit 0
  (.loop/m4-task4-donecheck.log). port/sim/ai.{h,c} (22 fns over
  MlAiSim; tagged bank writes, curentAction typo as slice state, fdlibm
  math, quirks q1-q8 verbatim) verified by the NEW aiport capture spec
  (runAI-only, pre read-set projection + M2-parallel post, wsViol==0,
  153-preset sweep) — 0 divergences over 7571 records on g07+g08,
  ZERO divergence-driven fix rounds. Honest coverage measured via the
  --cover arm instrument: 61/64 arms per golden; 3 zero-hit + 5 more
  surfaces measured-DEAD upstream (FORMAT.md "The aiport spec" lists
  them — incl. the ai.js:1254 curentAction write, dead by the
  :228/:1253 contradiction; T4 tooth amended accordingly, recorded).
  Teeth all fired (.loop/m4-task4-teeth.log): nibble→1, draw-drop→1104
  cascade, bk-drop→3/1, cta-serializer→3663, q2-typo→1. Regressions
  green: SIM CONFORMS + AI BRIDGE OK (AIBRIDGE1/check-sim.sh
  byte-untouched — task 5 retires the bridge from the live path).
  Task-5 handoff notes in the AGENT-LOG entry (MlAiSim population,
  bank-row alias/slot-0 re-copy stays caller's job, same seeded stream,
  rule-16 re-survey on new goldens). Device untouched this iteration
  (host-only task; iters 73-74 device-rig Codex arc unaffected).

## [superseded by iter 75] (updated: 2026-07-19, tasks 3+8 done): stall class ATTRIBUTED + mitigated; task 3 DONE)

- **Iter 74 (M4 task 8 — skip-stall attribution instrument,
  driver-re-ordered forward) DONE; M4 task 3 UNBLOCKED → DONE** (full
  entries: AGENT-LOG iter 74 pre-registration + result + addendum):
  **SKIP ATTRIB VERDICT: (a)** — the external stall class =
  `low_bat_check` (FunKey OS battery poller: 2 s shell loop, ~8
  busybox forks + blocking AXP20x i2c sysfs reads per wake; event
  comb every ~123 frames ≈ 2.05 s, phase-random per run — the
  "1100-1500 zone" was phase+load illusion). Matrix: live arms 2-3
  skips/33-34 events per 3600 paced frames; quiesce arms ×2 = 0
  skips, comb gone. Instrument committed (Tier-B diagnostic):
  gfx_app --attrib (per-frame mono/raw + rusage rows), sk_sampler.c
  (fork-free 250 ms /proc counter snapshots), correlate-skips.js
  (whitelist grammars + attrib_complete terminator), pre/post kernel
  + per-pid snapshots; `bash port/sim/device/check-skip-attrib.sh` →
  `SKIP ATTRIB OK` exit 0 cold (.loop/m4-task8-donecheck.log; arms
  nosampler/sampler/quiesce via MLFK_SKATTRIB_ARM). MITIGATION in
  check-device-render.sh: low_bat_check quiesced for the paced window
  (riglib rig_comm_pids/rig_daemon_stop/rig_daemon_restore —
  comm-scan kill-by-pid; busybox start-stop-daemon -K -x is a
  measured NO-OP for script daemons and pidof is comm-blind), restore
  hard-gated + trap-covered; skips==0 gate UNWEAKENED. The unblocked
  task-3 rerun then hit the FIRST-ever-reached host<->device shot
  bit-compare and exposed the SECOND iter-38-class instance: device
  musl lround shifted HUD glyph anchors 1 px — fixed with fdlibm.c
  exact round/lround STRONG overrides + mathsweep rr/lr columns + nm
  assertion (floor ceil fmod round lround). **Task 3 cold done-check
  GREEN: `DEVICE RENDER OK (full p99 12.777 ms, render-only p99
  5.598 ms, sim p99 7.429 ms, present p99 1.400 ms, skips 0/3600)`
  exit 0 (.loop/m4-task8-task3-donecheck.log; shot BIT-IDENTICAL
  PPM+PGM; p99 recovered ~2.7 ms vs the blocked attempts).**
  Regressions ALL green: CROSSCHECK OK, SIM CONFORMS 8/8, RENDER OK
  (IoU 0.9059, no pin moved), DEVICE CONFORMS g01, DEVICE CONFORMS
  8/8 + SIM P99 OK (.loop/m4-task8-{host,device}-regressions.log).
  Manifest: riglib.sh + check-device-render.sh re-pinned arc-pending
  (cite iter74) + anchor; SELF-CHECK 23/23 + ANCHOR GREEN. Driver
  notes: (1) OPK PLAY path keeps low_bat_check live by design (valve
  absorbs it; Chase's ratified playtest ran with it); task-14
  verify_m4 assembly decides whether with-audio legs adopt the
  quiesce (machinery is shared riglib); (2) check-device-audio.sh /
  check-device-opk.sh paced legs don't yet carry the mitigation;
  (3) Tier-B review round for the new instrument surfaces + the
  fdlibm round/lround addition is driver-queueable; (4) 9/12 paced
  runs consumed, device left clean (daemons verified restored, play
  install untouched).

## [superseded by iter 74] (updated: 2026-07-18, iter 73 — M4 task 3 landed+verified, done-check BLOCKED on the external stall class)

- **Iter 73 (M4 task 3 — stage legibility + device render rung)
  LANDED + VERIFIED, done-check BLOCKED (honest report; full entry =
  AGENT-LOG iter 73)**: legibility = device-only `--legible`
  (GFX_LEGIBLE_MIN_DEV_PX 2.0 device px, gfx.h rationale, twin-pinned,
  standing in-check no-legible-differs witness; host IoU path
  byte-untouched); --vfxdata/--glyphs threaded through
  check-device-render.sh / check-device-opk.sh / mlfk.sh with the
  COMMITTED frozen files' sha pins (iter-72 rule); arm build + host
  backends gain the vfx render TUs; judge-shot criterion-5 retired
  (reviewed pin change — ink-suppressed bg art); arm-gcc-10.2 -Werror
  class fixes (hit_detection.h noreturn decl — SIM CONFORMS 8/8 rerun
  green; overlay buffer); dv_start countdown clamped to the atlas
  domain (valve-tooth-found C-only skip-desync state); bit-identical
  render optimization round measured with the NEW -DMLFK_RENDER_PROF
  per-pass profiler (batch blend prims + rast_fill cov-window +
  unit-circle table): device render p99 13.20 -> 7.06 ms, full p99
  20.79 -> 15.51 (all budgets MET; trail in device-perf.md iter-73).
  GREEN: host RENDER OK (IoU min 0.9041, no pin moved), STREAM MATCH
  every device attempt, device shot BIT-IDENTICAL to host, teeth all
  fired, manifest re-pin 5 producers arc-pending + anchor SELF-CHECK
  23/23 GREEN. **BLOCKED: skips==0 unreachable today — 8/8 paced
  attempts carried 1-3 skips from isolated ~7-15 ms EXTERNAL kernel
  stalls (frames ~1118-1290 zone, the iters-54/59 zone); adbd-poll /
  writeback / rig-machinery / swap / DVFS / fresh-boot all REFUTED by
  isolation probes. skips==0 unweakened; task-8 attribution instrument
  = closure path; do NOT retry blind (driver: one cold retry on fresh
  device state is legitimate new evidence).** ALSO REGISTERED:
  check-device-opk.sh's frozen nav is stale (post-M3 inventory: the
  persistent meleelight.opk + 4 other OPKs; two same-title entries) —
  failed loud as designed, needs re-measured nav + unique evidence-OPK
  title; check-device-audio.sh still unthreaded (task 6/14). Device
  left clean, play install untouched.

## [superseded by iter 73] (updated: 2026-07-18, iter 72 — glyph-jitter class fix: measured glyph comparison)

- **Iter 72 (M4 task 2 micro-iteration — glyph-jitter class fix) DONE**:
  the driver's cold-r71 finding (committed vfxglyphs-frozen.txt vs
  fresh capture: ONE hex char of 43,013 — sprite "ready" RGBA px 2576,
  r 234<->235, delta 1; ~1-in-8 cold runs) root-classed: bit-freezing
  browser-rasterized TEXT assumes a determinism canvas font rendering
  does not provide. Pre-registered characterization (new standing
  instrument port/gfx/glyph-jitter-probe.js, 5 fresh sessions + frozen
  + preserved-failed = 21 pairs, .loop/m4-task2r72-probe.log):
  structural drift 0/21, max channel delta 1, max diff-pixel count 1.
  Refutation shape (c) fired -> measured-then-frozen comparator
  port/gfx/glyph-compare.js replaces the vfxglyphs cmp in
  check-render.sh: structure EXACT, channels within 4, <=16 of 19,764
  pixels may differ; twin-pinned (script + expected-render.json
  glyphComparePins). vfxglyphs-frozen.txt NOT re-frozen (5/5 fresh
  sessions byte-matched it). Teeth T1-T7 all bite incl. negative
  control: the preserved failing pair now PASSES
  (.loop/m4-task2r72-teeth.log). Cold RENDER OK x2, fresh sessions
  (donecheck{,2}.log): IOU MIN 0.9010 / 0.9030, streams MATCH. This
  was a measurement-honesty correction, not a weakening — documented
  with exposure figures in the AGENT-LOG entry. Task-3 note: device
  path consumes the COMMITTED frozen file (browser-free) — device
  comparisons stay bit-exact; pin the frozen sha, never a fresh
  capture's.

- **Iter 71 (M4 task 2 hardening round 3 — loo trajectory continuity)
  DONE**: the single review-70 round-3 Medium (.loop/review-70-1.log,
  NO-GO; capture-canvas.js:487) closed on
  port/gfx/{capture-canvas.js,iou.js,expected-render.json(comment)}.
  The injection-frame canonical render joined the deterministic
  render-plane RNG; det = strict replay ASSERTED bitwise-equal to the
  canonical mask (capture-side throw + iou.js judge twin, negative
  tooth proven); post-canonical vfxQueue restored BY REFERENCE, the
  finally native re-render DELETED — zero native draws at the frame,
  frames 151+ continue exactly the saved-mask trajectory. Accident
  measured pre-fix (probe OLD arm: randomTail 0/4 equal, star scatter
  moved), continuity measured post-fix (NEW arm: 4/4 identical,
  det==canonical) — .loop/m4-task2r71-probe-{old,new}.log; declared
  probe deviation (184-mask form is lifetime-blind, template FRAMES
  measured) pre-registered. Cold done-check RENDER OK DONECHECK_RC=0
  (.loop/m4-task2r71-donecheck.log): IOU MIN 0.8982 -> 0.9026, f0150
  0.9339 -> 0.9319, bdiff values identical, NO pinned value moved;
  check-sim.sh skipped (zero port/sim bytes). **Task-2 renderer arc
  round 4 = CLOSURE-OR-CAP (driver)**: rerun the reviewer on the new
  commit; per PROCESS §3 bounded convergence, if round 4 raises only
  variants of closed classes the arc CAPS with the recurring objection
  class named.

- **Iter 70 (M4 task 2 hardening round 2 — INJECT1 grammar, cast
  domain, region fix, browser attribution, inkNames pin, closure
  TOCTOU, iou parsers) DONE**: all 7 review-65 round-2 Mediums
  (.loop/review-65-triage-r2.md / .loop/review-65-3.log) closed on
  port/gfx/{gfx_vfx.c,iou.js,check-render.sh,capture-canvas.js,
  expected-render.json}. M1 whitelist-exact INJECT1 C parser (+
  emit-side grammar guard); M2 vfx_cfg_int cast-domain guard at 8
  sites (2 cited + 6 class siblings; isfinite no-op reordered before
  the cast); M3 firefoxcharge judge region cfg.face->cfg.f (measured
  delta: frames 1/5 box -> 3/7 box, 26.6 px wider x; NO pinned value
  moved, 0.88 aggregate untouched, run-1 min 0.8982); M4 browser
  leave-one-out attribution (deterministic det/loo masks at the
  injection frame, evaluate-only — gfx-pagelib.js untouched; iou.js
  bdiff>0 per effect); M5 exact ordered 5-name inkNames pin in 4
  validator sites; M6 final closure-identity re-check before
  RENDER OK; M7 full-corpus VFXDATA1 validation + exact 16-hex
  scale.bits grammar. Cold done-check RENDER OK DONECHECK_RC=0
  (.loop/m4-task2r70-donecheck.log); 7/7 teeth fired with predicted
  classes incl. a REAL end-to-end TOCTOU run and a pre-fix UBSan
  diagnostic at the cited line (.loop/m4-task2r70-teeth.log);
  check-sim.sh skipped (zero port/sim bytes — justified in the log).
  **Task-2 renderer arc round 3 = CLOSURE (driver)**: rerun the
  reviewer on the new commit; per PROCESS §3 bounded convergence the
  arc is at round 3 of ~8.

- **Iter 69 (M4 hardening — carrier uniqueness + banner affine counts)
  DONE**: both review-68 Mediums (.loop/review-68-1.log) closed on
  `port/sim/calib/check-vfx-seam.sh` ONLY. R1: each CARRIERS entry's 3
  tokens asserted DISTINCT at the inventory pin (cross-component
  repetition stays legal — g01/g04/g06 serves four components by
  design); R2: the check-sim `== gNN (name)` banner literal gets
  `count_aff` like every other decision literal (banner exact ×1 +
  banner-affine ×1 per golden — torn `=`/`==` fragments now die).
  Grammars re-validated 55/55 against the same archived corpus, zero
  false rejections (.loop/m4-rig69-corpusval.log); teeth 2/2 fired
  with the predicted message classes (.loop/m4-rig69-teeth.log); cold
  done-check `VFX SEAM MATCH` DONECHECK_RC=0
  (.loop/m4-rig69-donecheck.log). **vfx-rig arc round 4 =
  CLOSURE-OR-CAP (driver)**: per PROCESS §3 bounded convergence, if
  round 4 raises only grammar-variant re-raises of the closed classes,
  the arc CAPS with the recurring objection class named.

- **Iter 68 (M4 hardening — vfx-seam identity binding + truncated
  resemblance + rc case-split) DONE**: all three review-66 Mediums
  (.loop/review-66-triage.md) closed on
  `port/sim/calib/check-vfx-seam.sh` ONLY. M1: grammar counts now bind
  IDENTITY — frozen CARRIERS array (3 carrier golden names per
  component; runA/runB banners exact ×1 each, per-name STREAM MATCH
  ×2, byte-stable bound positionally `3 0 1 1 1`) + 8-name SIM_GOLDENS
  for check-sim. M2: the `count_aff` affinity counter (extension OR
  torn PREFIX of the exact literal) on the verdict + every evidence
  literal + STREAM stems. M3: grep rc case-split in all count helpers
  (rc 0/1 = count, rc ≥ 2 / any awk rc = loud corruption death; no
  blanket suppression). Grammars validated 55/55 against the full
  archived corpus (11 raw iter-66 logs + 44 reconstructed sections
  from the 4 archived composed runs) with ZERO false rejections
  (.loop/m4-rig68-corpusval.log); teeth 5/5 fired
  (.loop/m4-rig68-teeth.log); cold done-check `VFX SEAM MATCH`
  DONECHECK_RC=0 (.loop/m4-rig68-donecheck.log). Round-3 review
  returned 2 Mediums (carrier uniqueness + check-sim banner affinity,
  .loop/review-68-1.log) — closed by iter 69. Judgment lives in `vfx_judge_log`
  — the registered template (with the round-2 refinements) for task-14
  verify_m4.sh.

- **Iter 67 (M4 task 2 hardening — injection-set pin + per-effect ink
  assertions) DONE**: both review-65 Mediums (.loop/review-65-triage.md)
  closed on port/gfx/{check-render.sh, capture-canvas.js, iou.js,
  expected-render.json}. M1: frozen `injectPin` (ordered 7-name reviewed
  set + inkNames 5 + frame 150) asserted independently by
  check-render.sh (early + INJECT1 emitter), capture-canvas.js
  (pre-browser) and iou.js — a dropped/renamed effect dies on every
  side. M2: no-inject + five LEAVE-ONE-OUT C baselines (all streams
  cmp'd == run-a: injection is render-plane-only), per-effect
  browser-ink + C-ink + leave-one-out differential-ink assertions at
  f150 + a region-soundness guard (all differential ink inside the
  derived-region union); regions derived from the executed stages.json
  transform + frozen VFXDATA1 bounds x dVfx code-literal scales
  (documented in iou.js injectRegions()). Teeth: T-M1 both sides fired;
  T-M2 round 1 REFUTED the shared-baseline design (region overlap —
  diff=123 despite the stub), reworked to leave-one-out, rerun fired
  exactly the finding's scenario (f0150 aggregate 0.9122 PASS + INJ
  dashDust diff=0 death). Cold done-check RENDER OK exit 0, fresh
  capture, IOU MIN 0.9049 (.loop/m4-task2r67-donecheck.log; capture cap
  2/2 — run 1 died on the registered node -p ANSI-colour class,
  String() fix). check-sim.sh skipped, justified: zero port/sim bytes.
  **Task-2 arc ROUND 2 PENDING (driver): rerun the reviewer with the
  COMPLETE commit diff** (the round-1 High was a truncated artifact —
  include gfx-pagelib.js + the C TUs for the Tier-B union/bounds note
  + this commit's fix bytes).

- **Iter 66 (M4 hardening — check-vfx-seam aggregator classes) DONE**:
  all 4 review-64 Mediums (.loop/review-64-triage.md) closed on
  `port/sim/calib/check-vfx-seam.sh` ONLY — the verify_m3.sh aggregator
  classes adapted host-side: freshness-evidence grammar (runA/runB/
  byte-stable/STREAM MATCH counts measured from the 11-log iter-64
  corpus), expect_verdict (exact ×1 + final-line + measured
  verdict-prefix resemblance), mkdir-atomic no-reclaim host lock
  (`build/vfx-seam.lock`, iter-41 posture), 10-literal inventory pin
  over CHECKS/VERDICTS/SPECS, `  | ` relay prefix (one column-0
  `VFX SEAM MATCH` possible). Teeth 4/4 fired
  (.loop/m4-rig66-teeth.log); cold done-check `VFX SEAM MATCH` rc 0,
  zero grammar false-rejections on fresh logs
  (.loop/m4-rig66-donecheck.log). Round-2 review returned 3 Mediums
  (M1 identity/M2 truncation/M3 rc split) — closed by iter 68; the host aggregator
  classes are the registered pattern for task-14's verify_m4.sh
  assembly (AGENT-LOG iter 66 zoom-out). Tier-B sim-TU surface: clean
  per reviewer (no action).

## [superseded by iter 66] (updated: 2026-07-18, iter 65 — M4 task 2 DONE)

- **Iter 65 (M4 task 2 — renderer vfx + overlay/banner/background + IoU
  re-freeze) DONE** (respawn writer after the credit-death; dead
  writer's pre-reg adopted with 2 recorded amendments, its ml_events
  dust-glue diff reviewed + adopted verbatim): cold
  `bash port/gfx/check-render.sh` → `RENDER OK`, exit 0
  (.loop/m4-task2-donecheck.log). Full render sequence both sides
  (renderVfx + renderOverlay(true); mask fg1|fg2|UI): all 45 dVfx draw
  arms in gfx_vfx.c (canvas-2d emulation, NaN no-ops load-bearing,
  render-LOCAL RNG), HUD overlay + browser-rasterized VFXGLYPHS1 glyph
  atlas, ink-suppressed background art, executed VFXDATA1 template
  plane — both artifacts ×2 byte-stable, committed + cmp-tripwired.
  Corpus 16→24 + synthetic f150 injection (firefox*/shine* measured
  zero-live in EVERY golden). Old 0.91 pin retired with its exposure;
  NEW pin **0.88** frozen after the pre-registered refutation-(a)
  round: percentShake capture-to-capture variance (f1297 0.8835)
  closed by a render-guard-class fix (capture zeroes shake under
  snapshot/restore), threshold = floor over both honest minima; final
  cold run min 0.9032. Teeth T1/T1b/T2a/T2b/T3/T4/T5 logged
  (.loop/m4-task2-teeth.log) incl. the honest T1 sub-threshold
  sensitivity note. Regressions: SIM CONFORMS 8/8 + VFX SEAM MATCH.
  Task-3 handoff: check-device-render.sh/check-device-opk.sh must
  thread --vfxdata/--glyphs into gfx_app (it now requires them);
  ready/go banner sounds await the task-6 mixer.

## [superseded by iter 65] (updated: 2026-07-17, iter 64 — M4 task 1 DONE)

- **Iter 64 (M4 task 1 — vfx seam widening, sim + capture side) DONE**:
  cold `bash port/sim/calib/check-vfx-seam.sh` → `VFX SEAM MATCH`,
  exit 0 (.loop/m4-task1-donecheck.log; ~7 min — captures are ~15-25 s
  each, not minutes). ml_events vfx plane widened name-only → FULL
  drawVfx config (MlVfx + ml_drawVfx* emitters + ml_vfx_sink renderer
  chokepoint; cb_vfx canon); 193 sites translated (112 move TUs +
  article 9 + hitdet 17 + physics 4 + asshort 1 + sim_boot
  entrance/start); affected-cluster list MEASURED to include hitdet
  (brief's guess refuted by grep — 18 upstream sites); 10 specs
  re-recorded ×2 byte-stable, every run STREAM-MATCH guarded, ALL
  cluster replays 0-divergence (2592 live full-config events);
  SIM CONFORMS 8/8 unchanged (frozen goldens untouched). Read-set
  widenings forced by the configs (rule-7 corollary, 2×):
  shieldDepletion +pos/face (7-key pre), puff stage projection
  +wallL/wallR. Teeth: nibble→11 exact, name-only→34 (= non-empty-vfx
  records), face-drop→11, hitdet-field-drop→14400; restores proven by
  0-divergence re-replay. Honest coverage: asshort breakShield +
  physics shocked/burning zero-live; sim_boot boot vfx capture-less
  (task 2's render checks exercise them). Task-2 handoff notes in
  AGENT-LOG iter 64 (sink semantics, drawVfx defaults, circleDust
  draws already burned, render-plane spawn sites out of seam).
  Tier-B review round for the sim TUs: PENDING (driver queues it —
  mechanical arg-threading, done-check is the bit-exact oracle).

## [superseded by iter 64] (updated: 2026-07-17, iter 63 — PHASE M4, REPLAN done)

- **Phase: M4 — Full-game parity (REPLAN complete, iter 63)**: fix_plan
  `Current phase: M4`; §M4 concretized as a conventions block + 14
  dependency-ordered tasks with runnable done-checks; M4 EXIT GATE
  concretized into CLAUDE.md §Commands (`bash
  port/sim/device/verify_m4.sh` — full-game trace suite on device at
  60 fps with audio+music + menu flows + OPK-into-FOH, verify_m3.sh
  freeze-manifest/authoritative discipline inherited; on mechanical
  pass the DRIVER emits `LOOP STOP: m4-complete — awaiting Chase
  acceptance playthrough`). Ladder: (1) vfx seam widening
  sim+captures; (2) renderer vfx + overlay/banner/bg + IoU re-freeze;
  (3) stage legibility at device scale; (4) ai.js C port
  (capture-replay verified); (5) live CPU integration (bridge retired
  from live path, d1/d9 coverage); (6) mixer fidelity + play-ids +
  stop-path; (7) music streaming from SD; (8) skip-burst attribution
  instrument; (9) FOH core + flows host; (10) FOH device; (11) target
  test data+sim; (12) target test FOH+device; (13) SD persistence;
  (14) verify_m4.sh assembly. Key PROVISIONAL calls (AGENT-LOG iter
  63): target-plane = a SEPARATE parallel stream (CHECKSUM.md stays
  v1, no re-freeze); M4 goldens at port/goldens-m4/ (oracle/ is
  M0-only); menus verified by structural flow scripts + the
  checksummed match-launch bridge (no browser IoU); outOfCameraTimer
  stays render-excluded everywhere; scope exclusions (target builder,
  replay UI, multiplayer, credits). Measured: fd_tan already
  vendored+swept — "adds tan" was pre-satisfied at M0.

- **Iter 62 (M3 hardening — gate relay prefix, true-respawn poll,
  probe-order attribution) DONE**: all three .loop/review-60-triage.md
  + attribution items landed. (H2-residual) verify_m3.sh relay_lines
  chokepoint — every relayed sub-content line (tail'd leg logs,
  canned-rc bytes, status lists) prints `  | `-prefixed; the genuine
  `M3 GATE OK` echo is the ONLY possible unprefixed line-anchored
  occurrence (contract documented at definition + emission site).
  (L1-residual) riglib rig_proc_respawn_poll TRUE-RESPAWN form
  (+ rig_proc_pid): pre-kill pid captured at all 3 opk sites; verified
  respawn = old pid GONE (/proc RC-checked) AND live single pid != old.
  (class completion) judge-render-timing.js emits judge_complete=1;
  parse_timing_judge in render+audio checks reads judge stdout from
  FILE BYTES with the 17-byte terminator assert (iter-61 pattern;
  corpus 5/5 zero false rejections). (attribution) T5 probe moved
  AFTER the paced attempts; gate counters saved before the probe.
  **ATTRIBUTION VERDICT: probe after-effect — class-fixed by
  ordering.** Cold run attempt1 skips=1 / attempt2 skips=0 → `DEVICE
  AUDIO OK (full p99 12.393 ms, underruns=0, attempts=2; cbs=5166
  starts=274 stops=0 skips=0/3600)`, exit 0
  (.loop/m3-task7r62-audio-donecheck.log; 1/3 paced cap; T5 through
  the reordered path: 19 underruns counted + rejected). The
  probe-before elevated signature (3/4 attempts; driver cold both
  attempts 3,1) did NOT recur; residual single-skip transient = the
  pre-probe registered class (M4 instrument seed stands); thermal
  confound honestly recorded; skips==0 gate unweakened, cooldown arm
  not triggered. Gate mechanics 5/5 green incl. NEW relay teeth
  (.loop/m3-task7r62-gate-mechanics.log — status refusal now names 11
  producers: the two edited reviewed-go surfaces
  check-device-render.sh + judge-render-timing.js truthfully flipped
  to arc-pending); respawn tooth 8/8
  (.loop/m3-task7r62-tooth-respawn.log); timing-blank tooth both
  shipped variants (.loop/m3-task6r62-tooth-timingblank.log).
  Manifest: 6 producers re-pinned + MANIFEST_SHA256 → 578bfbd5… same
  commit; `SELF-CHECK 23/23 + ANCHOR GREEN`
  (.loop/m3-task7r62-manifest-selfcheck.log). **BOTH ARCS ROUND-3
  SCOPED CONFIRM PENDING (driver)**: audio round-3 confirm surface now
  incl. iter-62 probe-order + timing-judge bytes; gate-arc round-3
  confirm surface = iter-62 relay/respawn/timing bytes. Sequencing
  unchanged: both closures → driver flips ALL statuses to reviewed-go
  in the phase-advance commit → cold authoritative verify_m3.sh →
  sentinel + Chase S1 ratification.

- **Iter 61 (M3 task 6 hardening ROUND 2 — audio round-2 triage
  closure) DONE**: both .loop/review-59-triage.md items landed. (M)
  platform_audio_sdl.h platform_audio_stop: SDL_PauseAudio(1) BEFORE
  the terminal gap sample (quiesce the callback source, then sample
  under SDL_LockAudio — no callback can start after the sample;
  accounting semantics otherwise identical). (L) check-device-audio.sh
  reads judge + pack producer outputs from FILE BYTES (judge stdout →
  file + byte-exact 'judge_complete=1\n' tail assert; pack verdict →
  file + wc -c == one-verdict-line + one final newline), so trailing
  blank lines violate the grammar as written. NEW teeth T9/T10
  (trailing-blank → death, positive controls pass) + T8 file-based;
  standing T5 probe through the reordered path: 24 underruns counted +
  rejected. Cold `bash port/gfx/check-device-audio.sh` → `DEVICE AUDIO
  OK (full p99 12.952 ms, underruns=0, attempts=1; cbs=5166 starts=274
  stops=0 skips=0/3600)`, exit 0 (.loop/m3-task6r61-donecheck.log; run
  cap 1/1, no retry consumed, one pre-registered arm rebuild).
  Manifest: check-device-audio.sh re-pinned (c5471dd5…, status
  truthfully arc-in-flight) + verify_m3.sh MANIFEST_SHA256 →
  7e148d8c… in the SAME commit per the documented discipline (the
  anchor line is verify_m3.sh's ONLY change; its NORMALIZED manifest
  row ca21b4a5… unchanged — see AGENT-LOG iter 61 for the driver's
  gate-closure review); `SELF-CHECK 23/23 + ANCHOR GREEN`
  (.loop/m3-task6r61-manifest-selfcheck.log). Residual class instance
  flagged (not fixed, out of triaged scope): parse_timing_judge's
  $()-captured timing-judge stdout still normalizes a TRAILING blank
  line (inherited task-4 apparatus — driver may queue with the render
  check). **AUDIO ARC ROUND 3 = SCOPED CONFIRM PENDING (driver)**:
  round-2 fix bytes (platform_audio_sdl.h + check-device-audio.sh)
  are the confirm surface; sequencing unchanged — audio round-3
  closure + gate-arc round-2 closure → driver flips statuses to
  reviewed-go in the phase-advance commit → cold authoritative
  verify_m3.sh → sentinel + Chase S1 ratification.

- **Iter 60 (M3 task 7 hardening — gate-assembly round-1 triage
  closure) DONE**: all 6 triage items (.loop/review-58-triage.md
  H1/H2/M1/M2/L1/L2) landed on port/sim/device/{verify_m3.sh,
  m3-freeze-manifest.txt} + port/gfx/check-device-opk.sh +
  port/gfx/opk/mlfk.sh + riglib.sh (shared respawn-poll body). The
  gate now: [0] verifies the manifest's own bytes vs the in-script
  MANIFEST_SHA256 anchor (update discipline: any manifest edit changes
  the literal in the SAME commit; verify_m3.sh's manifest row is the
  NORMALIZED digest excluding that line — circularity, see both
  headers); [0b] HARD-REFUSES in AUTHORITATIVE mode while any producer
  status is arc-in-flight/arc-pending (currently 9 — the refusal IS
  the expected default-run outcome until closure); `M3 GATE OK` prints
  ONLY on a fully-authoritative all-real run — MLFK_M3_DEV=1 or
  MLFK_M3_FAKE_LEG_DIR force `M3 GATE (DEV — NON-AUTHORITATIVE)` +
  exit 3 (readonly flag, sentinel structurally locked out);
  verdict-RESEMBLING malformed lines at every leg parse = corruption
  death (discriminators measured from the real corpus);
  check-device-opk.sh restores the frontend VERIFIED (pkill rcs
  case-split + bounded rig_proc_respawn_poll before the verdict; trap
  never re-kills a verified frontend); mlfk.sh refuses loud (RC=7)
  when MLFK_DATA_DIR lacks simdata.txt. All teeth fired
  (.loop/m3-task7r60-*.log); 23/23 + anchor self-check green; ZERO
  real-leg gate runs consumed (driver owns the phase-advance cold run;
  it will trigger one arm-stamp rebuild — RIG_SCRIPTS bytes changed).
  Manifest re-pinned for the 4 touched producers, statuses kept
  arc-pending/arc-in-flight per truth. **GATE ARC ROUND 2 = CLOSURE
  PENDING (driver). Sequencing unchanged: audio round-2 closure +
  gate-arc round-2 closure → driver flips ALL statuses to reviewed-go
  (cites = closure logs) in the phase-advance commit → cold
  authoritative verify_m3.sh → sentinel + Chase S1 ratification.**

- **Iter 59 (M3 task 6 hardening — audio round-1 triage closure) DONE**:
  all 5 triage items (.loop/review-57-triage.md H/M1/M2/M3/L) landed;
  cold `bash port/gfx/check-device-audio.sh` → `DEVICE AUDIO OK (full
  p99 12.267 ms, underruns=0, attempts=2; cbs=5166 starts=274 stops=0
  skips=0/3600)`, exit 0 (.loop/m3-task6r59-donecheck2.log; run 1 =
  .loop/m3-task6r59-donecheck.log, honestly REFUSED on the registered
  transient skip class — audio legs green both attempts). NEW: judge
  `judge_complete=1` integrity terminator + counter-bound retry
  classification (fail_* now reporting-only); STANDING T5 device
  starvation probe every run (19 underruns counted + rejected);
  boundary-interval gap accounting in platform_audio_sdl.h (open→first
  + last→stop; healthy runs still 0); app-summary resemblance rule;
  exactly-one-line pack verdict; teeth T6/T7/T8 fired. MEASURED
  EXPOSURE recorded (PROCESS §8): DMA-xrun blindness + no SDL1.2
  priority API — M4 mixer-fidelity seed is the closure path.
  m3-freeze-manifest.txt re-pinned for the 2 touched audio producers
  (same commit, documented path; 23/23 direct-shasum self-check
  green). **AUDIO ARC ROUND 2 = CLOSURE PENDING (driver): the closure
  review covers the new bytes; producer edits INVALIDATE prior gate
  evidence — sequence per triage: audio round-2 closure → task-7 arc →
  THEN the phase-advance cold verify_m3.sh.** Skip-class measurement
  for the M4 instrument candidate: 3/4 attempts today (frames
  466/467/1116/1170/1192), probe after-effect not excluded.

- **Iter 58 (M3 task 7, the M3 EXIT GATE) DONE**: cold
  `bash port/sim/device/verify_m3.sh` → `M3 GATE OK`, exit 0
  (.loop/m3-task7-donecheck.log). Four legs all passed host-judged:
  [1] `DEVICE CONFORMS 8/8 + SIM P99 OK`; [2] `DEVICE AUDIO OK (full
  p99 12.573 ms, underruns=0, attempts=2)`; [3] `OPK LAUNCH OK`
  (packaged with the SDK container's mksquashfs 4.4 ONLY, launched via
  the REAL gmenu2x frontend driven by fk_input, boot-marker bin-sha ==
  arm stamp, evidence g01 900/900 stream prefix == frozen, in-app
  screenshot judge-shot structural); [4] `S1 INPUT OK`. NEW:
  port/gfx/opk/{mlfk.sh, meleelight.funkey-s.desktop, icon32.png (from
  OUR renderer's g01 f900 shot — no Nintendo bytes)},
  port/gfx/check-device-opk.sh, port/sim/device/verify_m3.sh (REUSES
  the arc-hardened sub-checks; verdicts parsed by exact anchored
  grammar), port/sim/device/m3-freeze-manifest.txt (PROCESS §4
  reviewed-pin freeze — 23 producers; HARD-REFUSES before any leg on
  drift). Measured gmenu2x nav (empirical): conf ignored for start;
  pkill-respawn = stable persisted section (games); `n m r a`
  normalizes link + selects MeleeLight; wrong section → no boot marker
  → leg FAILS LOUD (fail-closed). Teeth T1-T4 fired. Regression
  DEVICE RENDER OK skips 0/3600 (attempt 2; attempt 1's 1-skip = the
  registered transient class, not a regression). **PHASE-ADVANCE IS
  THE DRIVER'S NEXT TURN**: ground-truth the gate cold, then the
  human-gate sentinel `LOOP STOP: m3-device — needed: Chase S1
  ratification playtest` (the GATE does NOT print it — driver duty).

- **[superseded by iter 58] updated 2026-07-17, iter-57 writer completion**

- **Phase: M3 — GATE PASSED (MILESTONE PASS: M3, 2026-07-17)**: the
  authoritative verify_m3.sh printed M3 GATE OK exit 0 (driver-cold, all
  23 producers reviewed-go). LOOP STOPPED at the §H human gate:
  **LOOP STOP: m3-device — needed: Chase S1 ratification playtest**.
  RATIFIED 2026-07-17 (Chase playtest: controls perfect, sound perfect;
  visual amendments → M4 seeds: stage-surface legibility at device
  scale + vfx render seam). #18 closing; next: M4 REPLAN (PLAN §4/M4).
  Play install lives at /mnt/mlfk-data + /mnt/Applications/meleelight.opk
  (persistent, survives rig cleanup).
- **Iter 38 (M3 task 1) DONE**: commit af06bb7 — `DEVICE CONFORMS g01`
  (armv7 static sim, 3600/3600 STREAM MATCH on the FunKey, 21 s wall ≈
  5.8 ms/frame sim-only avg). Class finding: SDK static musl libm is
  FP-unsafe (floor/ceil/round identity, fmod(0,0), strtod misrounding) →
  exact floor/ceil/fmod strong overrides in fdlibm.c + strtod-free
  fmt_diff --gen + standing mathsweep instrument. round/trunc also broken
  on device, zero sim call sites — extend overrides+sweep before any use.
- **Iter 39 (M3 task 1 review-hardening) DONE**: all 9 verified Codex
  round-1 findings fixed with teeth proven (stamp authenticates
  script+image+binary bytes; digest-proven pulls via pullv; strict
  mathsweep corpus parse + count pins; fail-loud manifest eval/srchash/
  git guard; RC-marker leading-newline + 0-255 validation; mkdir rig
  lock; visible-WARN cleanup; nm override assertion). H1's
  "overrides absent" claim REFUTED on record (fdlibm.c:156/174/200).
  Cold done-check `DEVICE CONFORMS g01` exit 0; logs `.loop/m3-task1r39-*`.
- **Iter 40 (M3 task 1 review-hardening ROUND 2) DONE**: all 9 triaged
  round-2 findings (`.loop/review-39-1.log`, NO-GO) class-fixed with
  teeth proven — nonce RC markers (EXIT-trap bypass now fails), no-eval
  manifest parse with strict validation, frozen CORPUS_LINES=257287 pin
  + strict mathsweep grammar, symlink-aware NUL-framed srchash,
  fail-loud nm/git guards, rehash-adjacent-to-push, docker run by Id,
  fail-closed lock (never auto-delete a pid-less lock). TOCTOU-with-
  concurrent-mutator + hostile-repo-content dispositioned in writing
  (AGENT-LOG iter 40); rig threat model ("Review bar for rig/check
  scripts") added to PROCESS.md §3 + failure mode 8 to §7. Cold
  done-check DEVICE CONFORMS g01 through BOTH rebuild and cache-HIT
  paths; logs `.loop/m3-task1r40-*`.
- **PROCESS.md amended (reservations arc resolved, post-iter-40 driver
  turn)**: Tier B never wholly skippable (+ escalation conditions);
  concrete parallel-lane trigger (5 iters / ≥20% serialized host wait /
  registered consumer); checker-succession EVIDENCE PACKAGES
  (anti-laundering); STATE recovery pointer (monotonic log ids);
  reviewed-pin FREEZE MANIFEST for gate evidence — verify_m3.sh must
  hard-refuse unreviewed evidence producers (task-7 requirement).
- **Iter 41 (M3 task 1 review-hardening ROUND 3) DONE**: round-3 triage
  (`.loop/review-40-1.log`, NO-GO) — 5 surgical fixes with teeth proven:
  shared no-reclaim rig lock at `${TMPDIR:-/tmp}/mlfk-rig-<dev>.lock`
  (mkdir-atomic, keyed by DEVICE id, zero reclamation code — existing
  lock = loud death + manual rm); corpus IDENTITY pin CORPUS_SHA256=
  b164802a…b3d05 frozen next to CORPUS_LINES (one file feeds BOTH
  sweeps); post-push device-side digest of all 4 binaries vs stamp
  ("push provenance") before anything runs; srchash find -L (symlinked
  dirs descended, broken links = loud death); count-pipeline explicit
  status + non-numeric guard. TOCTOU-with-concurrent-mutator re-raise
  RE-dispositioned pointing at the iter-40 record (fix 3 covers its
  only observable edge). Cold done-check DEVICE CONFORMS g01 exit 0,
  rebuild path; logs `.loop/m3-task1r41-*`.
- **Iter 42 (M3 task 1 review-hardening ROUND 4) DONE**: round-4 triage
  (`.loop/review-41-1.log`, NO-GO — 2 findings, ONE class: host-side
  artifact not freshness-proven) closed by a CLASS SWEEP of
  check-device-g01.sh — rm-before-produce + `made()` exists-non-empty
  assert on all 13 fixed sites (incl. the two named: wrap-run JSON
  High :534, fdlibm corpus Medium :204; plus tables/.h headers,
  simdata, trace text, host sweep binaries+outputs, fmt corpus+output,
  the 4 armv7 binaries pre-docker); pullv gains a non-empty assert
  (closes both-sides-empty cmp). Both no-op-producer teeth fired
  (stale frozen-sha-identical artifacts on disk → loud made() death;
  T-wrap after the full device run, pre-verify-stream). Class rule:
  content pins prove CONTENT, never FRESHNESS. Cold done-check DEVICE
  CONFORMS g01 exit 0 (stamp HIT; the one forced rebuild landed in the
  T-wrap probe); 3/4 run cap; logs `.loop/m3-task1r42-*`. PROCESS
  honesty note on record: writer dead-parked on a background monitor
  (failure mode #1), driver-nudged, resumed foreground.
- **Tier-A device-rig arc CLOSED**: round 5 VERDICT: GO, NO findings
  (.loop/review-42-1.log; arc summary in the driver AGENT-LOG entry).
  The rig plumbing (nonce-dsh, pullv + non-empty assert, stamp + push
  provenance, shared device-keyed lock, rm-before-produce + made()) is
  the inheritance package for every M3 device script.
- **Iter 43 (M3 task 2) DONE**: `bash port/sim/device/
  check-device-conform.sh` → DEVICE CONFORMS 8/8 + SIM P99 OK, exit 0
  (.loop/m3-task2-donecheck.log). All 8 goldens replayed ON the FunKey,
  streams judged host-side by the unchanged verify-stream.js — zero
  armv7 divergences (empty ledger; the iter-38 libc class fix held).
  MEASURED sim-only (docs/research/device-perf.md): p50 4.27-5.81 ms,
  p99 7.95-10.68 ms — worst p99 (g08) leaves ~6 ms for
  render+present+audio. sim_main gained --timing (CLOCK_MONOTONIC ns,
  RAM-buffered, post-run write; host+device); percentiles.js is the
  host-side timing judge. Rig plumbing EXTRACTED into
  port/sim/device/riglib.sh (both device scripts source it; RIG_SCRIPTS
  = every rig script's bytes are stamp input → ONE shared arm build).
  Teeth: no-timing probe (pullv death), run-side stream perturbation
  (MISMATCH at exact frame; frozen-side perturbation trips the seal
  first — run side proves the judge), 1 ms threshold probe (SIM P99
  FAIL after STREAM MATCH). Regressions green: check-sim.sh SIM
  CONFORMS + check-device-g01.sh DEVICE CONFORMS g01 via the lib.
- **Iter 44 (M3 task 3) DONE**: `bash port/gfx/check-render.sh` →
  RENDER OK, exit 0 (.loop/m3-task3-donecheck.log). Renderer core
  host-side in NEW `port/gfx/`: ANIM1 C reader (FORMATS.md §2), the
  rastbench measured raster as a module (-O3 only on that TU; explicit
  ink plane), structure-parallel stage/players/articles compositor to
  ONE 240x240 RGB565 buffer, camera = STAB1 pos*scale+offset verbatim
  (upstream has NO dynamic zoom — measured) + k=0.2/dy=45 letterbox,
  GFXDATA1 executed colour/flag dump. Silhouette IoU vs the browser
  canvas (capture-canvas.js, run-capture served-bytes class, STREAM-
  MATCH-guarded, oracle/ untouched): measured 0.9149-0.9302 over 16
  sampled frames, threshold frozen 0.91 (seed 0.90; never loosened);
  x2 byte-stable renders; render-on C replay STREAM MATCHes g01.
  Teeth: dy-perturb → all-frames FAIL; PPM instability → cmp fail;
  1e-9 sim-write → MISMATCH frame 2. Host render ~45 µs/frame avg.
  VFX excluded BOTH sides — registered deferral (ml_events seam is
  name-only; M4 seed "vfx render seam widening"). Regression green:
  check-sim.sh SIM CONFORMS 8/8.
- **Iter 45 (M3 task 2 review-hardening) DONE**: task-2 arc round-1
  findings closed — (1) MATRIX PIN: conform.sh asserts the manifest ==
  frozen {g01..g08}, unique, CPU role on exactly {g07,g08}, BEFORE any
  build/device work; 8/8 derives from the pinned set (MLFK_MANIFEST
  override = negative-testing seam, default unchanged); (2) sim_main.c
  --timing malloc guarded (frames > 10^7 or SIZE_MAX/8 wrap → sim_fatal
  before malloc); (3) perf-history PRESENCE: read-only assert that
  docs/research/device-perf.md carries a measured row per pinned golden
  (content stays writer duty; split documented in the script header).
  All three teeth fired (.loop/m3-task2r45-tooth-*.log); cold
  done-check DEVICE CONFORMS 8/8 + SIM P99 OK
  (.loop/m3-task2r45-donecheck.log); check-sim.sh SIM CONFORMS.
  PROCESS honesty: writer dead-parked on background watchers again
  (nudged 3x) + one verify-then-destroy-in-one-call incident that
  sabotaged its own live run — class lessons in the AGENT-LOG entry.
- **Iter 46 (M3 task 3 review-hardening) DONE**: task-3 arc round-1
  findings (.loop/review-44-1.log NO-GO, triage
  .loop/review-44-triage.md) closed — (1) CORPUS PIN: sampledFrameCount
  16 frozen; check-render.sh + iou.js twin pins (16 unique frames,
  asserted pre-build), capture-canvas.js rejects duplicate frames;
  (2) ORACLE BUILD DIGEST: served-bytes sha256 (43 files incl. hooked
  main.js as served) recorded in run-JSON meta, pinned as
  servedDistSha256 (measured-then-frozen; reproduced exactly by the
  done-check's independent capture), asserted before judging;
  (3) REUSE BINDING: capture.digests.json sidecar (driver bytes +
  GFXDATA) written at capture time, MLFK_GFX_REUSE_CANVAS refuses loud
  on any mismatch; (4) CONSOLE FAIL-CLOSED: frozen consoleErrorAllowlist
  (favicon 404, localforage, our own sfx/music route aborts), anything
  else kills the capture; (5) miniView pAx==0 VERDICT: upstream
  render.js:173 divides unguarded → ±Inf (NaN unreachable), C already
  mirrors it bit-exactly under IEEE no-trap (grep-verified) — comment
  documenting the assumption at the division site, NO guard (hard rule
  5). All 4 teeth fired (.loop/m3-task3r46-tooth-*.log); cold
  done-check RENDER OK exit 0 (.loop/m3-task3r46-donecheck.log),
  IOU MIN 0.9149 ≥ 0.91, both STREAM MATCH 3600/3600.
- **Iter 47 (M3 task 2 review-hardening ROUND 2) DONE**: the round-2
  Medium (.loop/review-45-triage.md) closed — the matrix pin's
  exact-set comparison no longer word-splits manifest data (unquoted
  $gids could drop a stub id:"" entry): validation now runs INSIDE
  node on the parsed JSON (count == 8, ids nonempty + ^g0[1-8]$ +
  unique, CPU role on exactly {g07 g08} via env from the shell pin),
  emitting ONE validated line the shell consumes QUOTED.
  MLFK_MANIFEST override kept for teeth. Tooth fired: 9-entry stub
  id:"" manifest copy → loud pin death before any build/device work
  (.loop/m3-task2r47-tooth-blankid.log). Cold done-check DEVICE
  CONFORMS 8/8 + SIM P99 OK (.loop/m3-task2r47-donecheck.log; one
  expected arm rebuild — script bytes are stamp input). PROCESS
  honesty: monitor-park again (nudged once; fix = foreground bounded
  until-loop) + nohup launched without rc-echo (exit 0 evidenced by
  final markers + trap-clean lock removal; wrapper pattern noted).
- **Iter 48 (M3 task 3 review-hardening ROUND 2) DONE**: both round-2
  Mediums (.loop/review-46-triage.md) closed — (1) ALLOWLIST FLOOR:
  capture-canvas.js rejects at load (pre-browser) any
  consoleErrorAllowlist textIncludes/urlIncludes with trimmed length
  < 8; sfx/music url patterns lengthened to the MEASURED "/dist/sfx/"
  + "/dist/music/" forms (368/368 measured lines, zero match-set
  change); (2) REUSE INPUT-CLOSURE BINDING: NEW
  port/gfx/capture-closure.js = the ONE mechanical enumeration (9
  members: capture-closure.js, capture-canvas.js, fdlibm.js,
  harness init.js + pagelib.js, gfx-pagelib.js, expected-render.json,
  goldens manifest.json, the g01 trace) — capture-canvas.js LOADS from
  the map, the sidecar hashes every member, reuse refuses on
  member-set or digest drift either direction (old-format sidecars
  refuse → recapture). Both teeth fired
  (.loop/m3-task3r48-tooth-{allowlist,reuse}.log, cmp-verified
  restores); cold done-check RENDER OK exit 0
  (.loop/m3-task3r48-donecheck.log), IOU MIN 0.9149 ≥ 0.91, both
  STREAM MATCH 3600/3600. Class: bind the input CLOSURE — read-side
  twin of iter-42's write-site enumeration.
- **Iter 49 (M3 task 3 review-hardening ROUND 3) DONE**: the round-3
  single finding (.loop/review-48-1.log — closure hashes computed only
  AFTER replay; an editor save mid-capture binds NEW bytes to masks
  from OLD bytes) closed: capture-canvas.js snapshots sha256 of all 9
  closure members BEFORE any member is consumed (CLOSURE_SNAP), and at
  sidecar-write time re-hashes + verifies each equals its snapshot —
  drift → loud death naming the member, NO sidecar; the sidecar records
  the snapshot (consumed-bytes) hashes. Dispositioned-class variant
  taken under PROCESS §3's trivial-whole-class exception. Tooth fired
  (.loop/m3-task3r49-tooth-midrun.log: mid-capture whitespace append
  to expected-render.json → "closure member changed MID-CAPTURE", rc 1,
  sidecar absent; restore cmp-verified). Cold done-check RENDER OK
  exit 0 (.loop/m3-task3r49-donecheck.log), IOU MIN 0.9149 ≥ 0.91,
  both STREAM MATCH 3600/3600. Task-3 Tier-A arc → CAPPED-CLOSED by
  the driver.
- **Task-2 AND task-3 Tier-A arcs CLOSED** (driver, post-iter-49):
  task-2 GO at round 3; task-3 CAPPED-CLOSED at round 3 (named class:
  TOCTOU re-raises; trivial fix taken iter 49). Rig inheritance packages:
  riglib.sh + capture-closure.js.
- **Iter 50 (M3 task 4) DONE**: `bash port/gfx/check-device-render.sh`
  → DEVICE RENDER OK (full p99 10.743 ms, render-only p99 2.568 ms, sim
  p99 7.527 ms, present p99 1.479 ms, skips 0/3600), exit 0
  (.loop/m3-task4-donecheck.log) — FIRST PIXELS ON THE FUNKEY'S SCREEN:
  g01 replayed live, paced 60 fps, SDL1.2 240x240x16 (chain step 0,
  RGB565 verified), STREAM MATCH 3600/3600 with render+present live,
  screenshot (own fb, f900) structurally judged + BIT-IDENTICAL to the
  host render. Three-backend platform seam shipped (port/gfx/platform.h
  + headless/sdl2/sdl1 TUs, ONE per binary; PlatformInput carries the
  FunKey letter-keysym map for task 5). gfx_device joined the shared
  rig build (ARMBINS + port/gfx srchash + RIG_SCRIPTS — stamp now
  335a0f1a…). GFXDATA staged as committed gfxdata-frozen.txt
  (sha-pinned; check-render.sh cmp tripwire). Class fixes: device-libm
  FLOAT plane pre-empted (integer floor/ceil helpers + fdlibm trig →
  render cross-platform deterministic; mathsweep +sqrtf/fabsf columns,
  healthy); per-script push provenance (G01_BINS); pkill -f self-match.
  All 4 teeth fired (.loop/m3-task4-tooth-*.log + the standing in-check
  valve tooth). Regressions green: check-sim SIM CONFORMS, check-render
  RENDER OK (IoU min unchanged 0.9149 with fdlibm-routed trig),
  check-device-g01 DEVICE CONFORMS g01. Perf: ~5.9 ms p99 headroom left
  for task-6 audio (docs/research/device-perf.md iter-50 table).
- **Iter 51 (M3 task 5) DONE**: `bash port/gfx/check-device-input.sh` →
  S1 INPUT OK, exit 0 (.loop/m3-task5-donecheck.log) — the S1 input
  layer is live at the pollInputs seam. port/gfx/s1_input.h =
  data-driven PLAN §6 chord table (header-only; task-4 TU lists
  untouched); s1_sweep asserts the 15 pinned chord checks bit-exact +
  2048-combo dump ×2 + 1/80-grid closure. gfx_app --live records the
  golden-trace JSON via ml_sb_num String(x) (round-trip bit-exact,
  proven host-side before the device leg); --tapjump-off-p1 on gfx_app
  + sim_main (S1 contract; default paths unchanged, flag proven live).
  port/tools/fk_input (own uinput device, ssb64 pattern) played the
  committed 1080-frame s1-session.script through the REAL SDL keysym
  path on the FunKey: coverage 24/24 signatures (chords held 15-18
  frames as designed), FOUR-way byte-identical checksum streams (host
  ×2 + device replay + the live stream), non-vacuity leg green. CLASS
  FINDING (fixed + logged): musl-1.2 64-bit time_t vs the old kernel's
  16-byte input_event ABI — short write errno 0; fk_input emits the
  kernel's 32-bit layout explicitly (iter-38 class extended to
  kernel-struct timestamp ABIs). Instrument exposure: the resolver
  quantizer absorbs sub-half-grid-step table perturbations (tooth
  round 1 measured it; round 2 fired at a full step). riglib.sh grew
  per its own contract (RIG_SCRIPTS/ARMBINS/srchash roots).
- **Iter 52 (M3 task 4 hardening + rig-wide whitelist-grammar parser
  audit) DONE**: all 8 triaged review-50 findings closed (deadman-
  guarded park with nonce disarm + cancel-on-success proven every run;
  detached setsid/rc-file launch; pessimistic PARKED; skip gate
  0/3600; PPM+PGM bit-compare GATING; pkill rcs captured;
  platform_present success channel; wall window [58,66] s; strict
  valve grammar) — cold done-check DEVICE RENDER OK exit 0
  (.loop/m3-task4r52-donecheck.log). REAL FINDING (the new M2 gate's
  first run caught it): the FunKey kernel rejects FBIOPAN_DISPLAY, so
  the patched libSDL's SDL_Flip returns -1 with
  `ioctl(FBIOPAN_DISPLAY) failed` on EVERY frame while presents run —
  platform_sdl1 pins that exact measured-benign signature (whitelist
  posture on a C API; probe .loop/m3-task4r52-probe-sdlflip.log).
  PARSER AUDIT: 42 sites enumerated / 24 conformant / 13 converted
  (NEW shared rig_dev_sha256 + rig_stamp_bin_sha + parse_app_summary;
  require_device full-line; conform perf-row end anchor; check-render
  strict gparams + render-only grammar; input.sh apprc/sweep/sha
  conversions) / 2 tightened / 2 not-converted with reasons — full
  list + corpus citations in AGENT-LOG iter 52. 39 teeth fired
  (32 host + 7 device-probe incl. deadman FIRE under real transport
  death). Regressions ALL green: check-render RENDER OK, check-sim
  SIM CONFORMS, device g01 + conform 8/8 + input S1 INPUT OK.
- **Iter 53 (M3 task 5 hardening) DONE**: all 8 triaged review-51
  round-1 findings closed — cold done-check `S1 INPUT OK` exit 0,
  FIRST run (.loop/m3-task5r53-donecheck.log). H1 strict full-stream
  validator on all 5 streams (F 1..1080 contiguous + RNG + SIM OK,
  gate-fatal); M1 fk_input anchored full-line script grammar + ferror
  (the `d l s 250` joined line dies rc 2 pre-injection, device-probed);
  M2 raw-line whitelist grammar before JSON.parse in the coverage
  judge (duplicate-key class dead; discriminating pair vs the old
  judge proven); M3 sweep exactly-one-verdict-line + byte-exact apprc
  cmp; M4 SOCD LIVE WITNESS — gfx_device --live records a raw-keysym
  bitmask sidecar (mandatory --record-keys), judge asserts pairing
  fidelity + universal SOCD invariants + 2 new signatures: the real
  session carried 18/18 SOCD H/V frames through the actual uinput→SDL
  path (iter-51's registered exposure closed); M5 standing tapjump
  behavioral oracle (host differential, measured-then-frozen
  divergence at frame 121); M6 pessimistic PARKED + rig_dsh_retry
  cleanup; L1 clayer-diag requires ls neutral. 11 host teeth + 4
  device-probe teeth fired (.loop/m3-task5r53-teeth-{host,fkinput}.log);
  zero false rejections on every genuine corpus. Regressions recorded:
  check-sim/check-device-render NOT run (no sim TU; riglib untouched)
  — gfx_app --trace guarded by host smoke (F1 == the g01 anchor;
  .loop/m3-task5r53-smoke-host.log).
- **Iter 54 (M3 task 4 hardening ROUND 2) DONE**: all 5 triaged
  review-52 round-2 findings closed (.loop/review-52-triage.md) — H
  deadman disarm ordering (RC-checked marker-gone VERIFICATION gates
  both disarm channels; rig_cleanup RIG_PRESERVE_DTMP=1 keeps the nonce
  alive on unverified restore — deadman probe: nonce survived cleanup,
  fired in-window, marker removed, gmenu2x respawned); M1 FRAMES_PIN=
  3600 literal (manifest cross-asserted at the pin; gate asserts the
  LITERAL); M2 timing-judge duplicate-key = corruption death + 8-key
  presence; M3 digit bounds on every numeric grammar in both files
  BEFORE bash arithmetic (status-2-as-false hole closed); L
  rig_stamp_ok strict WHOLE-FILE grammar (any extra/malformed line =
  MISS). Cold done-check DEVICE RENDER OK exit 0, skips 0/3600
  (.loop/m3-task4r54-donecheck.log; 2/2 paced cap — attempt 1's 1-skip
  fail was a GENUINE transient the H3 gate correctly caught, frame-1190
  sim spike 13.02 ms). Teeth: 9 stamp-grammar + M1/M2/M3a-c host teeth
  + the device deadman probe, zero false rejections on genuine corpus.
  Regressions ALL green: DEVICE CONFORMS g01 · DEVICE CONFORMS 8/8 +
  SIM P99 OK · S1 INPUT OK (.loop/m3-task4r54-reg-*.log).
- **Iter 55 (M3 task 4 hardening ROUND 3) DONE**: both review-54
  round-3 residuals closed — H cross-run deadman sequencing via
  STALE-STATE STARTUP NORMALIZATION (new step 0 chokepoint in
  check-device-render.sh: stale marker restored FIRST + RC-verified,
  stale deadman disarmed via its designed cancel channel with exit
  verified, state wiped only after; RIG_PRESERVE_DTMP held while an
  old backstop may still be needed) + NONCE-SCOPED deadman kill (argv
  tag infeasible — gfx_app rejects unknown args, gfx_app.c out of
  surface; equivalent: launcher records gfx_device's pid under
  gfx.pid.<nonce>, deadman kills only that pid and only while its
  /proc cmdline is still gfx_device); M rig_stamp_ok srchash value
  grammar restored (^srchash=[0-9a-f]{64}$ AND equality) +
  rig_srchash produce-time 64-hex assert (empty/short never stamped).
  Cold done-check DEVICE RENDER OK exit 0 FIRST attempt, skips 0/3600
  (.loop/m3-task4r55-donecheck.log; the round's one rebuild). Teeth:
  T1 plant+probe normalization, T2 the review's exact A/B stranding
  sequence (no stranding), T3 scoped-kill both directions, T4a-d +
  T5a2/b2/c stamp grammar (zero false rejections). Regression DEVICE
  CONFORMS g01, stamp HIT (.loop/m3-task4r55-reg-g01.log);
  conform/input skipped with pre-registered justification (stamp-
  machinery-only riglib edits). Zoom-out: cross-run state machines
  get ONE startup chokepoint, never per-sequence patches.
- **Iter 56 (M3 task 4 hardening ROUND 4, FINAL) DONE**: both
  review-55 Highs closed by INHERITED-STATE PESSIMISM
  (RIG_PRESERVE_DTMP=1 set GLOBALLY the instant the rig lock is held —
  before require_device/selftest/any probe can fail into the trap;
  trap preserves $DTMP and never touches a marker/cancel it does not
  own until step-0 normalization POSITIVELY verifies inherited-state
  ownership and clears the flag; step 0 moved ahead of the devsha
  selftest); the Medium closed by riglib rig_host_sha256() — FULL-LINE
  host shasum grammar (`<64hex>  <actual path>` reconstruction, mirror
  of rig_dev_sha256) at ALL surface extraction sites (pullv, stamp
  read/write/rehash, srchash stdin form, render gsum/hsum) — zero
  `cut -f1` scrapes remain. Cold done-check DEVICE RENDER OK exit 0
  FIRST attempt, skips 0/3600, full p99 11.400 ms
  (.loop/m3-task4r56-donecheck.log; the round's one rebuild). Teeth
  8/8: T1/T2 planted-foreign-state preservation through forced
  require_device + mid-normalization probe failures (live transport —
  preservation proven as a decision), T3a-f shasum grammar incl. the
  review's exact 64-hex-field-wrong-tail case, zero false rejections.
  Regression DEVICE CONFORMS g01, stamp HIT via the new parser
  (.loop/m3-task4r56-reg-g01.log). Zoom-out: protection flags default
  to PRESERVE from resource-acquisition time; only positive
  verification flips them.
- **In flight**: task-4 arc CAPPED-CLOSED (driver-recorded; class + Low disposition in AGENT-LOG) (iter
  56 was the pre-announced FINAL round; recurring class named:
  "cross-run frontend-park/deadman sequencing before startup
  normalization owns inherited state"; residual = operator-error
  concurrency, dispositioned Low with reviewer concurrence in
  .loop/review-55-1.log; review-50's untriaged Low — gfx_app
  getline/ferror — left for the driver's cap record). task-5 arc
  CLOSED at GO (round 2). Then: task-6 review arc → task 7 (OPK +
  verify_m3.sh gate).
- **Iter 57 (M3 task 6) DONE**: `bash port/gfx/check-device-audio.sh`
  → DEVICE AUDIO OK (full p99 12.614 ms, underruns=0, attempts=2;
  cbs=5166 starts=274 stops=0 skips=0/3600), exit 0
  (.loop/m3-task6-donecheck.log) — AUDIO IS LIVE ON THE FUNKEY: g01
  full match with render + the 44100/S16LSB/2ch/512 callback + the
  8-voice SFX mixer (spike math verbatim; steal-oldest-by-start-seq —
  spike-default claim refuted, audiotest has NO allocation), fed via
  the ONE ml_snd_sink chokepoint (sim_tick untouched; SIM CONFORMS +
  every audio-on stream verifies — the mixer only reads). SNDPACK1
  from the REUSED pipeline audio stage, count=180 + sha frozen; pushed
  to /mnt/mlfk-scratch with provenance (never committed). Audio-on
  p99 cost vs task-4 baseline ≈ +1.1 ms (sim 8.481 / render 3.422 /
  present 1.849). NEW transient measurement: cold attempt 1 skips=8
  (BURST form of the registered class) — retry policy absorbed it,
  attempt 2 clean; bursts recurring → the iter-56 M4 attribution
  instrument, never a wider retry budget. Teeth: standing T1-T4
  (pack truncation, dropped-blob death at play, underrun-perturbation
  gate fail, grammar deaths) + device T5 (64-sample starvation → 17
  underruns counted + rejected; .loop/m3-task6-tooth-t5.log).
  Honest coverage: audible FIDELITY unverified by construction (M3 =
  structural liveness only); stop-path has zero live g01 coverage;
  M4 seeds registered (mixer fidelity + music, stop-path coverage,
  skip-burst instrument). Regressions green: DEVICE RENDER OK (11.065
  ms full p99) + SIM CONFORMS (.loop/m3-task6-reg-{render,sim}.log).
- **Latest AGENT-LOG entry**: iter 73 (M4 task 3 RESULT — landed +
  verified, done-check BLOCKED on the external stall class; honest
  report); latest log id: .loop/m4-task3-donecheck.log (attempt
  family + m4-task3-teeth.log, m4-task3-prof*-device.log,
  m4-task3-manifest-selfcheck.log).
- **Device**: FunKey-S on ADB, id 12c00003237f5528, healthy. adbd drops
  exit codes → RC-echo via port/sim/device/adbsh.sh. /tmp tmpfs 128 MB;
  big artifacts → /mnt/mlfk-scratch; ADB pulls ~4.4 MB/s (budget pull
  time). Arm build stamp-cached (`MLFK_FORCE_ARM=1` forces).
- **Branch**: agent/auto, clean between iterations (iter-39 hardening
  commit on top of af06bb7 + process-docs commit). Origin only.
- **Loop**: dynamic self-paced driver; ~20-30 min heartbeat; writer
  completion notifications are the primary wake signal.

## Next

1. Driver: Tier-B review round for the iter-64 sim-TU surface
   (PROCESS §3 — mechanical vfx arg-threading across 116 TUs +
   ml_events/canon; done-check is a bit-exact oracle replay, so one
   structural round; escalate on any Medium+).
2. Writer: M4 task 2 — renderer vfx + overlay/banner/background + IoU
   re-freeze (fix_plan §M4; done-check `bash port/gfx/check-render.sh`
   → `RENDER OK`; consume ml_vfx_sink — handoff notes in AGENT-LOG
   iter 64).
3. Then the ladder in order (fix_plan §M4 tasks 3-14), Tier-A arcs on
   every non-checksummed shipping surface, Tier B ≥1 round on sim TUs.
3. Phase end: driver cold AUTHORITATIVE `bash
   port/sim/device/verify_m4.sh` → `M4 GATE OK` → sentinel
   `LOOP STOP: m4-complete — awaiting Chase acceptance playthrough`.

[superseded by iter 63 — M3 driver sequencing retained in AGENT-LOG]

## Rulings (standing owner directives)

- 2026-07-14 — Full autonomy: all judgment calls delegated; run until M4
  done; push-notify at milestone gates; stop for M4-complete, physical
  blockers, or usage-limit deaths (notify /login and stop).
- 2026-07-14 — ZOOM OUT is HARD RULE 8: class fix > one-off; zoom-out
  note ends every root-cause session.
- 2026-07-16 — Adopt brawlback PROCESS-EXPORT standards per
  docs/PROCESS.md (tiered Codex review arcs, pre-registration, STATE.md,
  artifact identity pins, ground-truth ritual); the four explicit
  non-adoptions + reopen conditions live in PROCESS.md's final section.
- 2026-07-16 — Whitelist-grammar rule (brawlback provenance): decision-
  bearing parsers = anchored empirical grammars, fail closed, corpus-
  validated; binding form PROCESS.md §3.
- 2026-07-19 — Portability ruling: FunKey-specific code inventoried in
  docs/PORTABILITY.md (4 layers + recipe); writers add rows in-commit
  for device-specific additions; driver enforces at review.
- Standing: never push to upstream/schmooblidon; origin only; no
  distribution of anything; writers never post to GitHub (driver owns
  tracker writes).
