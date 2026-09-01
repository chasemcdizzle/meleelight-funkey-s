# 1. Persistence is a field table, not a per-screen save/load interface

Date: 2026-08-27
Status: Proposed

## Context

The port persists a **settings record**: about twenty named fields plus a
screen name. Each field is hand-written into a serialiser, a parser, a
migration arm and a version bump. The format is at `MLFKPERSIST7`; seven
versions in, each one paid for by hand.

That cost has a consequence nobody chose. **Screen-local state** — cursor
rows, hand position, which page a grid shows, port types, CPU difficulty,
the stock/endless mode — has never been persisted, because no single one of
those fields is individually worth a version bump and a migration arm. The
owner's report of hibernate "not working" was six of those fields in a
trench coat. The record's *shape* decided what got saved, not any judgement
about what should.

Two obvious alternatives:

1. **Keep adding fields by hand.** Cheapest per field. It is also what
   produced the current gap, and the gap grows with every screen added.
2. **A `save()`/`load()` pair on each screen.** The conventional answer,
   and the owner's first proposal. Twelve screens times two methods is
   twenty-four places a future author must remember to update, and nothing
   fails when someone forgets — the same silent omission that produced the
   gap, relocated from one place to twelve.

   It also models the domain wrongly. Per-screen save/load treats the
   *screen* as the unit that owns state, but the state is one flat struct
   every screen shares: the character-select hand position does not belong
   to that screen in any enforced sense. The interface would invent an
   ownership boundary the data does not have and then require it to be
   maintained by hand.

## Decision

Persistence is driven by a **declarative field table** — one row per
persisted field, naming its type, offset and size — that both the writer and
the reader walk.

Three consequences follow from the shape:

- Adding a field is one row. No migration arm, no parser edit.
- Unknown rows are skipped on read and missing rows take defaults, so the
  format is forward and backward compatible by construction. **Version bumps
  stop being a thing.**
- A `_Static_assert` on the size of the persisted struct fails the build
  whenever anyone adds a field. The author must then either add a row or
  raise the number with a comment saying the field is deliberately not
  persisted.

That last point is the reason for the decision rather than a detail of it.
It converts "someone must remember" into "the compiler will not let you
forget", which is the same mechanism this project already relies on for its
capacity limits and for the exhaustive resume-target map — a map whose
exhaustiveness caught a real gap when two lanes merged.

**One half of the rejected alternative is kept, because it was right.** Some
state should not be saved at all — it should be *recomputed*. Target select
must re-scan the slot files on entry, because the card may have changed while
the lid was shut; credits must place its reticle. That is genuinely
per-screen, genuinely a procedure, and a field table cannot express it. So a
screen may also carry a **resume hook**, called once after the fields land.
Not `save`/`load` — only the re-derivation. The builder's document resume
already has this shape, entering the screen first and restoring over the top.

That hook extends a mechanism the project already has rather than adding a
parallel one: the resume-target map is an exhaustive switch whose comment
states that a new screen must be a compile error there, "the one place that
must think about it", and its exhaustiveness has already caught a real gap
when two lanes merged. The hook lives with it.

Fields holding pointers are marked in the table as reconstructed rather than
copied. A raw byte image would restore stale pointers that happen to be
valid only while the binary is unchanged, which is a trap rather than a
feature.

## Consequences

**Good.** New state gets persisted by default rather than by exception.
The migration surface goes to zero. The failure mode for a forgotten field
moves from "silent data loss the owner reports months later" to "the build
does not compile".

**Bad.** One indirection between a field and its bytes, so a field's
persisted form is no longer readable from the struct alone. The table
becomes a thing that must itself be correct, and an offset table is exactly
the kind of thing that is wrong in a way tests must catch rather than
review.

**Accepted risk.** The static assertion is load-bearing. If someone raises
the size without reading the comment, the guard is gone and nothing says so.
That is the same hostage relationship this project already documents for its
deliberate perturbations, and it is accepted on the same terms: the guard
asserts an outcome, and a check must prove the guard still bites.

**Correction, 2026-08-27 (ticket #27).** The Decision above names two hook
cases: target select's slot scan and *"credits must place its reticle"*.
Only the first survived measurement. `foh_init` already places the credits
reticle at the identical home the entering transition writes, so a resume —
which arrives on a freshly initialised state — finds it placed, and a hook
that placed it again would be a **provable no-op whose tooth could not
bite**. A no-op hook is worse than no hook: it asserts a dependency that is
not there, and it dilutes the one mechanism this ADR kept. The credits
screen therefore resumes into itself with `FOH_RESUME_HOOK_NONE`, and the
two placements were collapsed into one macro (`FOH_CRED_HOME_X/Y`) so the
argument holds by construction rather than by two literals agreeing;
`port/foh/check-credits.sh` compares a resumed credits screen against an
entered one pixel for pixel and its T4 moves `foh_init`'s placement to prove
the comparison bites. The general rule the correction leaves behind: **a
hook earns its place only if removing it changes something a check can
see.** Target select's still does.
