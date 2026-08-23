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

- **A30b (M-7) is owner-ratified but edits the byte-for-byte ratified BOX
  table** pinned by `s1_sweep.c` + 15 S1 checks. It must land AFTER A31
  (M-6), because arbitrary rebinding may make most of it unnecessary.
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
