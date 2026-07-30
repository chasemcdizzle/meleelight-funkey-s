# STATE.md — current truth (driver-updated every turn)

_Read CLAUDE.md first, then this page. History → docs/AGENT-LOG.md;
queue → fix_plan.md; standards → docs/PROCESS.md._

## §rulings (standing owner directives, day-tagged)

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
