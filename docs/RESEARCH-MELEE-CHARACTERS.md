# Research charter — Melee-faithful characters from doldecomp/melee

_Owner decisions from the grilling session, 2026-07-29. Status: CHARTER
ONLY — nothing dispatched. Sources: https://decomp.dev/doldecomp/melee ·
https://github.com/doldecomp/melee_

## The question

"What is the optimal way to add more characters that feel exactly how
they are in the real game?"

## Facts established before deciding

- **meleelight is a 5-character game** (falco, falcon, fox, marth,
  puff) — measured in the pinned clone. So new characters cannot come
  from upstream; an outside source is genuinely required.
- **A Melee character is not a portable unit.** Decomp characters are
  PowerPC code coupled to Melee's own engine (its collision, its action
  state machine, its 3D skeletal animation). meleelight is an
  independent 2D reimplementation. There is no seam where a character
  detaches and reattaches.
- **The wall is animation, not physics.** Per character meleelight
  carries ~149 action states, ~5,562 vector paths, and **~1,550,000
  int16 coordinates** (project pins: 744 states / 27,808 paths /
  7,747,148 coords across 5 chars). Fox's *idle* alone is a 113k-token
  source file. `npm run animations` is webpack bundling, NOT a
  generator — whatever produced that data is not in the repo, and is
  not in the decomp either (Melee ships 3D skeletal DATs; meleelight
  needs 2D vector paths).
- **"84.32% decompiled" is largely the wrong metric.** Melee's
  per-character frame data, hitboxes and attributes live in the
  character DAT files as subaction bytecode, not in the decompiled
  code. What the decomp supplies is the **interpretive map** — struct
  layouts and bytecode semantics — which is among the earliest, best-
  understood parts of any Melee decomp. The numbers come from an owned
  copy of the game.

## Decisions (owner-ratified)

1. **Scope = MECHANICALLY Melee.** Frame data, hitboxes, knockback,
   attributes exact. Animation is a separate, later, optional problem.
   (Rejected: "Melee including animation", which makes a 3D→2D DAT
   extraction pipeline the centre of the project and the decomp a side
   quest.)
2. **Acceptance test = TRANSLATION CONFORMANCE.** meleelight's five
   existing characters are a **Rosetta Stone**: five worked examples of
   "real Melee character → meleelight character". Diffing them against
   decomp-derived data recovers the translation function (units, scales,
   systematic simplifications). A new character is correct if it was
   produced by that same measured mapping. Chase's playthrough remains
   the final human gate. **Dolphin differential registered as the
   escalation** if the mapping proves non-systematic.
3. **Go/no-go = the FOX CALIBRATION SLICE**, modelled on M2-CAL: extract
   Melee's real Fox values, diff against meleelight's authored Fox. If
   the translation is systematic ⇒ agentic development works and a
   character becomes a pipeline. If it is ad hoc ⇒ conformance collapses
   and only the Dolphin differential remains. M2-CAL's rule binds: a
   blocker list is NOT a pass.
4. **CLONE CHARACTERS ONLY. Roy first, Ganondorf second.** Melee's
   clones share animations with their base, and meleelight already ships
   both bases: Roy→Marth, Ganondorf→Falcon. For these the 1.55M-coord
   animation wall does not exist. The alignment is exact: what makes Roy
   *Roy* is almost entirely DATA (reversed sweetspot — base of the blade
   instead of the tip — different knockback, weight, attributes), which
   is precisely what the decomp supplies. meleelight already contains a
   Melee clone pair (fox/falco), so the architecture tolerates it.
   Non-clone characters are OUT OF SCOPE — they reopen the animation
   problem and test nothing better.
5. **Oracle = a SEPARATE fork (option C).** Roy is authored in
   meleelight **JavaScript first**, then ported to C, so the existing
   checksum machinery applies unchanged. The pinned clone (27af171)
   stays BYTE-FROZEN for the 8 existing goldens; a separate Roy fork
   serves only as Roy's oracle. HARD RULE 3 exists to stop new work
   endangering frozen evidence — physical separation beats discipline.
   (Rejected: forking the pin in place; authoring straight in C, which
   deletes the oracle for that character.)
6. **FIRST EXPERIMENT = a throwaway stub.** Add a stub 6th character to
   a fork and re-run the M0 gate. If the 8 goldens still verify AND the
   boot-RNG pin (exactly 465 draws) holds, the approach is de-risked for
   an hour's work. Character-registry changes shifting the boot draw
   count is the named risk; that pin exists because such a shift
   silently corrupts every stream.

## Open (not yet decided)

- IP posture for decomp-derived material (recommendation: never copy
  decompiled code into the tree; use it as a reading map only, extract
  DATA at build time from an owned game copy, gitignored and never
  distributed — identical to the ratified audio-blob posture).
- Where the project lives, when it starts, and what it delivers.
