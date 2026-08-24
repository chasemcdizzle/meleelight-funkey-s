# CONTEXT — the port's ubiquitous language

A glossary, and nothing else. No implementation details, no plans, no status.
Spec and tickets live in `docs/FEATURES-SPEC.md`; queue in `fix_plan.md`;
current truth in `docs/STATE.md`.

**Why this file exists.** Almost every defect found in the 2026-08-23/24
sessions was a **naming failure**, not a logic failure: two different things
sharing one word, or one thing having two representations that drifted apart.
Each entry below is a term that has already cost real work. New terms earn a
row when they are resolved, not when they are invented.

---

## Input vocabulary

**Keysym** — the letter a physical button emits into SDL. A property of the
*hardware*, measured on the device, never inherited from a donor project.
*(Cost: the L shoulder emits `m`; the port asserted `k` for months because the
row came from a different handheld.)*

**Flow letter** — the letter an input *script* uses to name a button. It is a
label in a test corpus and is deliberately **decoupled from the keysym**: the
injector resolves flow letter → keysym through the keymap. Renaming a keysym
must not rename a flow letter.

**Style** — a named whole-controller scheme (`BOX`, `CLASSIC`, `NATURAL`).
Decides what each *role* is and which physical button carries it.

**Binding** — a per-player permutation of *physical* buttons, applied **before**
the style resolves roles. Because it is a permutation of physical inputs, a
binding can only reshuffle buttons that exist; **it can never create a role the
style does not emit.** *(Cost: "just rebind X to grab" was proposed twice and is
impossible by construction.)*

**Role** — what a button *does* in the engine (attack, special, jump, grab,
shield, Mod, C-layer). Distinct from both the physical button and the style.

**Mod shoulder** — an *orthogonal cell*, not a style. It names which shoulder
carries Mod within one style, and is inert in styles that have no Mod. Two
rulings about "R" can therefore both be true if they concern different styles.
*(Cost: a contradiction was filed between two rulings that never conflicted.)*

**`z`** — in **this engine**, an *alternate smash-attack button*. It is **not
grab.** In real Melee the Z button grabs, and that outside knowledge has now
produced one shipped defect and one misleading source comment.
*(Cost: X→grab shipped doing nothing.)*

**Grab (how it is reached)** — a *dispatched action state*, not a button. It is
reached by shield+A, or by the analog shoulder while dashing/running/in
jumpsquat. **A control scheme "has grab" only if it can reach one of those.**

**Tap jump off** — a stored setting whose name is a negation. Because the value
is also a boolean, `TAPJUMP OFF: ON` reads as a double negative and is
routinely misread as being about a different setting entirely. **Display and
storage may legitimately disagree in polarity**; the name must say which one it
is.

---

## Character-select vocabulary

**Selection plane** — which character each port has *chosen* (`p1Char`,
`p2Char`). This is what a match launches with. It survives match exit and
back-out.

**Token plane** — where each port's *pin sprite* physically sits on screen
(`cssChar`). It is a **view**, not the choice.
**These two planes are the single richest source of defects in the project.**
When they disagree, the player sees a character they did not pick. Any rule
that re-homes a token must re-home it from the **selection**, never from the
token's own pixel position, and never from the **port index**.
*(Cost: two separate shipped bugs — tokens parking on marth/puff after every
match, and backing out re-selecting whichever character sits nearest the BACK
button.)*

**Port** — a player slot (0–3). **Never an index into the character roster**,
though the two are the same shape and have been confused at least twice.

**Participant** — a port that is actually playing. The engine plane is four
ports wide throughout; a front-of-house that exposes fewer is a *presentation*
limit, not an engine one.

---

## Event-plane vocabulary

**vfx name** and **sound name** — different namespaces that read alike. A name
found by grep proves nothing about which plane it belongs to; the *call* has to
be read. *(Cost: one near-miss where a sound was almost reported as a missing
visual effect.)*

**Play id** — the handle identifying one *sounding voice*, so it can later be
stopped. It is meaningful only if **every plane that starts a sound advances the
same counter**. Two counters that "count the same events" is a *precondition*,
never a fact to be assumed. *(Cost: a charge sound that never stops, because
menu clicks advanced one counter and not the other.)*

**Emitted vs renderable** — the sim *emits* effect names; the renderer *knows*
templates. An effect only appears if both are true. The relationship should be
checked, not assumed.

---

## Verification vocabulary

**Tooth** — a deliberate perturbation asserting that a check can still fail.
A tooth is **hostage to the mechanism it asserts**: if unrelated work removes
that mechanism, the tooth silently stops biting and the check goes quietly
vacuous. *(Cost: two teeth found disarmed — one by a menu-font change, one by a
cursor rewrite.)* A tooth should assert **the outcome protected**, never an
error string or an implementation detail that happens to produce it today.

**Seam** — a boundary between two planes (resolver→sim, sim→renderer,
menu→mixer). **Defects concentrate here**, because each side's own check passes
while nothing asserts the crossing. *(Cost: X→grab, and the play-id drift —
both had green checks on both sides.)*

**Frozen** — an artifact whose bytes are pinned. Changing it is legitimate when
behaviour legitimately changed, and is then a *reviewed* change carrying its
citation. **Weakening a check to make a run pass is never that**, and the
difference is whether the number stays on the page.

**Measured / Cited / Assumed** — the three grades of a claim. Mixing them is how
a plausible reading of source becomes a false conclusion; ten filed premises
were falsified this session by running the code instead.
