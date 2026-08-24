# FEATURES-SPEC — the feature backlog as a task graph

**OWNER RULING 2026-08-23 (Chase, in session), verbatim:** *"i want to skip
m4 for now and just do all my features please."*

M4's exit gate (`port/sim/device/verify_m4.sh`) is therefore **DEFERRED, not
cancelled**. Its 8 arc-in-flight rows stay exactly as they are; nothing in
this document may flip a manifest status, close an arc, or weaken a check.
When the owner returns to M4, the blocker is unchanged: one combined review
round over the 8 rows (B9 is the keystone — it taints 5 of them).

**This file is the SPEC + TICKET GRAPH.** Row detail lives in `fix_plan.md`
(§"OWNER PLAYTHROUGH #3" and §"ROUND 2"); this file adds only what fix_plan
does not carry: **lane assignment, blocking relationships, and the frontier.**

---

## 1. Binding constraints (read before scheduling anything)

These are not style preferences. `docs/LOOP.md` is a HARD RULE 3 never-edit
file and §A-par is owner-authorized.

1. **§A-par.3 — SPLIT BY PLANE, NEVER BY ITEM COUNT.** Lanes equal the number
   of *file-disjoint planes*, not the number of open tickets.
2. **§A-par.3 — the menus/text plane is SERIAL WITHIN THE LANE.** `foh.c`,
   `foh.h`, `foh_render.c` and the glyph atlas are shared by almost every
   ticket. They cannot be worked concurrently. This is why the graph below is
   mostly a chain, not a fan.
3. **§A-par.2 — never two writers in one working tree.** Every lane gets its
   own git worktree and its own branch. The precedent is
   `§matchexit-contested` (2026-07-30): a frozen review packet went stale
   *within minutes* with zero edits of its own.
4. **§A-par.4 — VERIFICATION NEVER PARALLELIZES.** One physical FunKey, one
   `riglib.sh` no-reclaim lock, 30-40 min per device leg. Lanes buy parallel
   EDITING only. **The device is currently disconnected, so every ticket
   below is scoped HOST-ONLY until it returns.**
5. **§A-par.5 — one batched re-pin pass, driver-only, at the end.**
   `MANIFEST_SHA256` is a single anchor; concurrent re-pinning collides by
   construction. **No lane touches the freeze manifest.**
6. **§A-par.7 / HARD RULE 4** — `agent/auto` is the autonomous run. Lanes are
   *supervised side lanes* on their own branches, driver-merged.

## 2. Lanes

| Lane | Plane | Files it owns | Tickets |
|---|---|---|---|
| **M** | menus/text | `foh.c` `foh.h` `foh_render.c` `foh_ctl_labels.h` glyph atlas | A29, A23+A27, A24+A4, A32, A25a, A31, A30b, A25c, B4, A7, A14, D8-later |
| **A** | audio | `platform_audio_sdl.h`, mixer TUs | A28 |
| **P** | platform/power | `foh_pause.c`, `platform_sdl1.c`, `foh_persist.*`, launcher | A34, A26 |
| **I** | input | keymap, `s1_input.h`, `ctl_style.c` | A25b, A30a |
| **R** | research | `docs/research/` ONLY — writes no code | A33 |

**Lane M carries ~80% of the cost and is a strict chain.** That is the
honest shape of this backlog; it is not a defect in the plan.

## 3. The ticket graph

Arrows mean "blocks". Anything with no unmet blocker is on the **frontier**.

```
LANE M (serial chain — each ticket blocks the next by file contention)
  M-1  A29   CSS picks bug (P0)          [FRONTIER]
   -> M-2  A23+A27  CSS header widgets: BACK hold-bar + mode ribbon
   -> M-3  A24+A4   renames (CONTROLS / HANDHELD; BOX-CLASSIC-NATURAL)
   -> M-4  A32      "TAPJUMP OFF" -> "TAP JUMP", value inverted
   -> M-5  A25a     TSS selection highlight (1px border is invisible)
   -> M-6  A31      Controls screen: real rebinding, kill mod row + rebind:N/A
   -> M-7  A30b     BOX/CLASSIC/NATURAL face-button remap  (owner-ratified)
   -> M-8  A25c     foh_hand extraction; CSS byte-identical FIRST, then TSS
   -> M-9  B4       FOH_TMATCH has no exit transition
   -> M-10 A7       credits screen
   -> M-11 A14      glyph-atlas swap (relayout; position-neutral per STATE.md)
   -> M-12 D8-later tag widgets + D18 letter grid        [blocked by A14]

LANE A   A-1  A28   audio buzz                            [FRONTIER]
LANE P   P-1  A34   POWER OFF dead control  -> P-2  A26  hibernate/resume
LANE I   I-1  A25b  L does nothing (check A3 zoom-out)    [FRONTIER]
         I-2  A30a  modOnR default flip                   [blocked by M-3]
LANE R   R-1  A33   GC adapter spike                      [FRONTIER]
```

### Cross-lane dependencies (the ones that actually bite)

- **A30b (M-7) — LANDED as A41 (DEVIATIONS D31/D32/D33, 2026-08-24).** It
  did edit the byte-for-byte ratified BOX table's SURROUNDINGS, under an
  explicit owner re-ratification, and it landed after A31 (M-6) as this
  row required — the rebinder did NOT make it unnecessary, and the reason
  is worth keeping: `ctl_bind_apply` PERMUTES physical buttons before the
  style resolver, so it can never conjure an `in.z` that no style emits.
  Grab on BOX was a table change or nothing. The BOX chord ROWS are still
  byte-untouched; what moved is the button plane and the C-layer's holding
  button, and the 15 pinned S1 checks moved with them, deliberately.
- **A31's per-player question is gated on A33.** Design the binding table
  per-port; ship the UI editing port 0 only until R-1 says whether a second
  physical controller is real.
- **A25b (I-1) may be A3's defect at a second call site.** If L never reaches
  the app, fix it ONCE at the input layer — do not write a TSS-local
  workaround. Check this BEFORE writing code.
- **A23 and A27 are ONE ticket.** `foh_render.c:222` names the BACK wedge and
  the mode ribbon as a single not-hit-testable gap.

## 4. Frontier as of 2026-08-23

Host-doable, file-disjoint, no owner decision outstanding:

| Ticket | Lane | Why it is ready |
|---|---|---|
| **M-1 / A29** | M | P0 bug; mechanism already identified; pure host |
| **A-1 / A28** | A | P0 bug; code-inspection diagnosis is host-side |
| **I-1 / A25b** | I | code-inspection + A3 zoom-out is host-side |
| **R-1 / A33** | R | desk research; writes only `docs/research/` |

**P-1 (A34) and P-2 (A26) are NOT on the frontier** — both need the physical
device to observe the failure at all.

## 5. Rules every lane agent inherits

1. **CLAUDE.md HARD RULES 1-8 bind in full.** Especially: no stubs or
   placeholders (deferrals go to `docs/AGENT-LOG.md` BLOCKERS, never into
   code); never edit, delete or weaken any test, oracle, gate, check or
   frozen artifact; faithfulness to upstream is the default and every
   deviation is registered.
2. **Stay inside your lane's files.** A lane that needs a file another lane
   owns STOPS and reports — it does not reach across.
3. **No device runs. No `riglib.sh`. No freeze-manifest edits. No arc
   closures.** The device is disconnected and re-pinning is driver-only.
4. **Measure, do not recall.** Every claim cites file:line read this session.
   The 2026-08-04 B1 probe is the standing lesson: it grepped a symbol and a
   comment, concluded "not fixed", and was wrong — exercising the behaviour
   took two minutes and was decisive.
5. **Commit on your own branch only.** The driver merges.

---
---

# PART 2 — THE REMAINING WORK (spec'd 2026-08-24, owner: *"do everything that's left"*)

Vocabulary is pinned in **`/CONTEXT.md`** — read it first. Every term below in
`code font` is defined there, and most of them are defined there *because they
already caused a defect*.

## 6. What is actually left

| # | Ticket | One line |
|---|---|---|
| **A42** | X→grab does nothing | `z` is not grab in this engine |
| **A43** | CSS back-out re-selects falcon | token re-homed from pixels, not `selection plane` |
| **A40** | Shieldbreaker charge sound never stops | two `play id` counters drift |
| **A44** | No P3/P4 at the CSS | FOH models 2 ports; the sim already does 4 |
| **A7** | Credits screen | ~422-line transliteration |
| **A45** | Target builder | 58.5 KB upstream editor; the plane `+ ADD CODE` refuses |
| **A14** | Glyph-atlas swap | 41 call sites; unblocks D8 |
| **D8** | Tag widgets + letter grid | blocked by A14 |
| **A26** | Hibernate/resume | needs a signal probe on device |
| **A34** | POWER OFF dead | diagnosis; needs device |

## 7. The through-line: this project's defects live at `seam`s

A42 and A40 are the same *shape*: **both planes' own checks were green, and
nothing asserted the crossing.** A43 and A29 are the same shape too — the
`selection plane` and the `token plane` each self-consistent, disagreeing with
each other.

**So the highest-value work is not any single ticket. It is one instrument that
crosses the resolver→sim seam** (spec'd as A42's class fix below). It retires a
whole family, and without it every future remap ships equally broken.

## 8. Tickets, spec'd

### A42 — X must reach a real grab. **Class fix mandatory.**
**Do not** wire `X` to `z` and call it grab; that is the shipped defect.
`z` is the alternate smash-attack button (measured: 3 readers, 0 grabs).
**Reach a real trigger instead** — shield+A, or the analog shoulder. Preferred:
synthesise what Melee's Z *is* (A + lightshield), which reaches
`GUARD.c:75`/`KNEEBEND.c:66`.
**THE DELIVERABLE IS NOT THE BUTTON — IT IS THE SEAM CHECK.** Press a physical
button → real resolver → **real sim tick** → assert the resulting *action
state*. Cover every role D33 moved (A=jump, B=attack, Y=special, X=grab), so
the next remap cannot ship broken. *Fold in* the emitted-vs-renderable vfx
comparison (39 vs 43, currently a hand-run one-liner) — same instrument rung.

### A43 — re-home the token from the `selection plane`
Same family as D21. On back-out and re-entry the token must return to the
**stored character**, never to the nearest cell by pixel position and never to
the port index. The selection survives — A29 measured that. Tooth: pick a
non-default character, back out, re-enter, assert both planes agree.

### A40 — one `play id` counter, or a menu play that does not consume one
Root cause measured: the mixer advances on **every** event, the sim plane only
on sound plays, and the menu chokepoint calls the mixer directly. Recommended
shape: `snd_event_menu()` beside `snd_event()`, and point the menu caller at
it. **Function and caller must land together** (an uncalled static trips
`-Werror`). Tooth: play N menu sounds, then assert a sim-started voice still
stops — `stopsUnmatched` must stay 0.

### A44 — widen the FOH to four `port`s
The sim is already 4-wide (player plane, `tapJumpOff[4]`, goldens, AI). This is
presentation only: 2 more panels, 2 more token lanes, type/CPU boxes, and every
CSS check's expectations. `css_ready` already loops 0..3. **Scope check first:**
4 panels on a 240×240 screen may not fit at the current panel size — if not,
report the layout options rather than silently shrinking.

### A7 — credits
Unblocked; owner already delegated the RNG choice to a FOH-local stream (D19's
reasoning). ~422-line zero-DOM transliteration. A6's audio-options screen is the
finished template to copy.

### A45 — target builder
`src/target/targetbuilder.js`, 58.5 KB, in the pinned clone. It is a
**rewrite**, not a transliteration (jQuery+DOM editor → 240×240 C), exactly as
the FOH was. It is the plane `+ ADD CODE` currently refuses. **Its own arc.**
Sequence after A7.

### A14 → D8
Glyph-atlas swap: `foh_text2` → `gfx_glyphs_load`, 41 call sites. Position-
neutral (later screens author against the new font either way). **Cheaper than
long quoted** — there are no committed screenshot bytes to re-freeze; the cost
is device time to re-run legs. D8's tag widgets + letter grid follow it.

### A26 / A34 — device-blocked
A26: hibernate kills the app (owner-measured). Next step is a *signal probe* —
install a handler that logs to tmpfs, hibernate, reopen, read the log. That
decides handler-vs-checkpoint. A34: POWER OFF is wired at `foh_pause.c:570`;
diagnose why the arm does not take, comparing against QUIT which works.

## 9. Lanes and order

```
LANE M (menus/text — SERIAL, §A-par.3)
  A43 → A44 → A7 → A14 → D8 → A45
LANE I (input)     A42  ← includes the seam instrument
LANE A (audio)     A40
LANE P (platform)  A26, A34   [device-blocked]
```
**Frontier now: A42, A43, A40** — three planes, genuinely parallel.
A44 queues behind A43 (same files). A45 last: it is the largest and it wants
the credits template to exist first.

## 10. Standing rules for every lane

Inherit §5. Additionally, learned the hard way this session:
- **A comment is not evidence.** `z` was documented as grab and is not.
- **A name is not evidence about its plane.** Sounds and vfx read alike.
- **Verify the premise before building.** Ten filed premises were falsified by
  running the code; five were the driver's own.
- **Deviation numbers are DRIVER-ALLOCATED.** Two lanes collided on D29.
- **The device is the owner's daily driver.** No device runs; name the legs you
  make owed. Never leave a persistent marker that outlives the test.
