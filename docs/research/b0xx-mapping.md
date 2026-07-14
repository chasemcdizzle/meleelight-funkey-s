# B0XX-style digital→analog mapping for the FunKey-S (wayfinder #6)

Research ticket: how do B0XX / Frame1 / Smash Box-style digital controllers
express Melee's analog language, and which subset fits the FunKey-S button
budget (d-pad + A/B/X/Y + L/R + Start + Fn)?

**TL;DR.** Digital Melee controllers solve "analog on buttons" with exactly
three mechanisms: (1) dedicated direction buttons emitting full-deflection
coordinates, (2) one or two *modifier* buttons that scale those coordinates
into known-good sub-threshold values (tilt band, walk band, wavedash/firefox
angles, shield-drop band), and (3) an SOCD policy for opposite directions.
meleelight already contains a miniature version of this system in its
keyboard path (5 multiplicative stick modifiers). All five hard-goal
mechanics (dash/walk, tilt-vs-smash, short/full hop, wavedash + angles,
cardinal/diagonal DI) are achievable on the FunKey with **one** modifier
button; a second modifier only buys extra up-B/wavedash angle families.
Recommended scheme: **S1 "One-Mod + C-layer"** (§5.3).

---

## 1. Melee's analog coordinate model (what the mapping layer emits)

- The GC stick reports 0–255 per axis; Melee re-centers at 128 and divides
  by 80, so the usable engine range is **−1.0 … +1.0 in steps of 1/80 =
  0.0125**, with the vector clamped to the unit circle. meleelight
  reproduces this exactly: `steps = 80`, `meleeRound(x) =
  round(80·x)/80`, `unitRetract` norm-clamp
  (`src/input/meleeInputs.js:109-186` in schmooblidon/meleelight).
  HayBox's GC output uses the same model: `ANALOG_STICK_MIN 48 / NEUTRAL
  128 / MAX 208`, i.e. ±80 counts, and writes coordinates as
  `128 + direction * N` where `N/80` is the Melee value
  (`src/modes/Melee20Button.cpp`). Community notation "7000" = 0.7000.
- **Deadzone:** meleelight zeroes any axis with |v| < 0.28
  (`deadzoneConst = 0.28`, `meleeInputs.js:110,210`). Synthesized
  values must clear this (this is why B0XX's smallest useful X value is
  0.2875, one step above the dead zone).
- Everything the mapping layer produces should be **quantized to 1/80**
  and injected at meleelight's `Input.lsX/.lsY/.csX/.csY` level
  (`src/input/input.js`), bypassing `tasRescale`/`scaleToMeleeAxes`
  (those exist to simulate imperfect physical sticks; we can emit final
  Melee-unit coordinates directly).

## 2. Mechanism catalog: how B0XX / Frame1 / Smash Box do it

### 2.1 Layouts

- **B0XX** (Hax$ / 20XX): ~20 buttons. Left hand: Left/Down/Right on the
  home row, Up on a thumb-adjacent key, L. Right hand: A, B, X, Y, Z, R,
  Light-shield, Mid-shield, Start, and 4 C-stick direction buttons.
  Thumbs: **Mod X** and **Mod Y**. (B0XX Instruction Manual, archive.org
  copy of the "B0XX Manual and Manifesto".)
- **Frame1**: same input language — "two modifier buttons: Mod X and Mod Y…
  first designed by Hax for the B0XX, and later adopted by DIY controllers,
  keyboard emulation, and more recently Frame1" (techhenzy box-controller
  FAQ); Frame1 adds on-board remapping and per-mode configurable SOCD
  (frame1.gg remapper docs).
- **Smash Box** (Hit Box): the original (2017) box controller; different
  philosophy — a bank of ~4 freely-programmable modifier buttons whose
  analog values are user-assigned per profile, rather than the two
  fixed-purpose Mod X/Mod Y of the B0XX school. The B0XX scheme won the
  community standard war; open firmwares (HayBox, GP2040-CE Melee modes,
  b0xx-ahk for keyboards) all implement the Mod X/Mod Y language.
- **Open-source reference implementation**: HayBox
  (github.com/JonnyHaystack/HayBox), `src/modes/Melee20Button.cpp` — the
  de-facto standard coordinate set, quoted below. `b0xx-ahk`
  (github.com/agirardeau/b0xx-ahk) is the same scheme for keyboards.

### 2.2 The modifier system and its coordinates

Unmodified direction buttons emit full deflection: cardinals ±1.0,
diagonals (±0.7000, ±0.7000) — a 45° vector chosen because it is above
every "smash" threshold on both axes. Holding a modifier rescales the held
direction(s) to a pre-baked coordinate. The two families (values in Melee
units; HayBox `Melee20Button.cpp` unless noted; B0XX v2.1 manual values in
parentheses where they differ):

| Chord | HayBox coordinate | Angle | Purpose |
|---|---|---|---|
| plain cardinal | ±1.0 | — | dash, smash, spotdodge, roll |
| plain diagonal | (0.7000, 0.7000) | 45° | diagonal DI, 45° firefox |
| shield + down-diagonal | (0.7000, −0.6875) | −44.5° | **vanilla shield drop**; still a legal wavedash angle |
| **Mod X** + horizontal | 0.6625 (B0XX v2: 0.7375) | — | **walk / f-tilt band**: above the 0.3 walk floor, below the 0.8 dash/smash threshold |
| **Mod X** + vertical | 0.5375 (B0XX v2: 0.6500) | — | **u-tilt / d-tilt band**: below the 0.6625 Y smash / tap-jump threshold, above 0.3; down value avoids crouch and fast-fall |
| **Mod X** + diagonal | (0.7375, 0.3125) | 22.96° | shallow firefox / drift |
| **Mod X** + diagonal + shield | (0.6375, 0.3750) | 30.47° | shallow **wavedash / airdodge** ("magic number" for ledgedashes per the B0XX manual) |
| **Mod X** + diagonal + C-direction | 5 stepped angles 22.96°–40.59° | | fine firefox angle ladder |
| **Mod X** + diagonal + B | extended (higher-magnitude) versions of the 5 angles | | extended up-B |
| **Mod Y** + horizontal | 0.3375 (B0XX v2: 0.2875) | — | slowest walk; just above the 0.28 deadzone |
| **Mod Y** + vertical | 0.7375 | — | tap-jump-capable but sub-1.0 vertical |
| **Mod Y** + diagonal | (0.3125, 0.7375) | 67.04° | steep firefox |
| **Mod Y** + diagonal + shield | (0.4750, 0.8750) up / (0.5000, 0.8500) down | 61.5° / 59.5° | steep wavedash & upward airdodge |
| Mod X + Mod Y together | — | — | layer shift: C-stick buttons become the D-pad (HayBox) |

Trigger side: B0XX/HayBox also give digital buttons for **light shield**
(analog trigger ≈ 49/140 counts) and mid-shield, and Mod X turns L into an
analog ("Z-light") press. C-stick diagonals get a fixed ASDI/slide-off
coordinate (0.5250, 0.8500).

### 2.3 SOCD (simultaneous opposite cardinal directions) policies

Three policies exist in the wild:

1. **Neutral** (Hit Box tradition, and meleelight's own keyboard code —
   opposite keys cancel to 0; `input.js:161-166`).
2. **Second-input priority (2IP)** — the newer direction wins while both
   are held.
3. **2IP-no-reactivation** — the B0XX policy: "overriding the previously
   held direction with the newly chosen direction… \[on releasing the
   newer one] the original direction will not reactivate" (B0XX manual).
   This is what HayBox's `InputMode::HandleSocd` implements.

One deliberate exception: while Left+Right are physically held, B0XX/HayBox
**refuse to apply modifiers to the X axis and pin X at 1.0**, because a
full-deflection X during ledgedash regrab maximizes jump trajectory (B0XX
manual §9.1; HayBox "Horizontal SOCD overrides X-axis modifiers (for
ledgedash maximum jump trajectory)"). On a FunKey **cross-shaped d-pad,
opposite cardinals are mechanically impossible**, so SOCD is nearly moot —
the mapping layer should still define it (recommend 2IP-no-reactivation for
B0XX parity; note meleelight's keyboard precedent is Neutral) but the
ledgedash SOCD trick is simply unavailable (harmless: our plain cardinal is
already 1.0).

### 2.4 Why those exact numbers (rationale from the B0XX manual)

- **0.8000 X / 0.6625 Y** — "the first values that activate Smash on
  either axis"; everything below is tilt territory. All modifier values
  are placed relative to these two thresholds.
- **0.7375 X** (B0XX v2 ModX-horizontal) — "the greatest X-value in the
  game that does not break Teeter" (edge-teeter preservation).
- **Shield drop** — "requires a very precise Y-axis input of −.6625,
  −.6750, or −.6875": below the −0.65-ish shield-drop floor but above the
  −0.7 spotdodge threshold. HayBox bakes it into the shield+down-diagonal
  coordinate (Y = −0.6875) so every down-diagonal while shielding shield
  drops.
- **Wavedash 30.5° (0.8500, −0.5000) and 59.5° (0.5000, −0.8500)** —
  "magic #'s" found by examining ledge-dash sequences of top characters.
- **0.2875** — smallest X surviving Melee's ±0.2750 dead zone (one 1/80
  step above it).
- Later firmware revisions (HayBox et al.) nerfed some values
  (ModX-horizontal 0.7375→0.6625, turnaround-B pinned to X=1.0) to comply
  with tournament controller rulesets — the coordinate table is a living
  legality document, not physics.

## 3. meleelight's input model — what the layer must synthesize

meleelight is the oracle; these thresholds were read from
schmooblidon/meleelight master (files under `src/`).

### 3.1 Engine thresholds (left stick unless noted)

| Mechanic | Condition | Source |
|---|---|---|
| Deadzone | axis zeroed if abs < 0.28 | `input/meleeInputs.js:110,210` |
| Walk | abs(lsX) > 0.3 in WAIT; walk speed ∝ lsX (`walkMaxV * lsX`) | `characters/shared/moves/WAIT.js:88`, `WALK.js:37` |
| **Dash** | abs(lsX) ≥ **0.79** AND lsX **two frames earlier** < 0.3 (flick detection) | `physics/actionStateShortcuts.js:489` |
| Smash turn | lsX·face < −0.79 with 2-frames-ago > −0.3 | `actionStateShortcuts.js:493` |
| **F-smash** | A press + abs(lsX) ≥ 0.79 + 2-frames-ago < 0.3 | `actionStateShortcuts.js:349` |
| **U/D-smash** | A press + abs(lsY) ≥ **0.66** + flick | `actionStateShortcuts.js:352-354` |
| C-stick smashes | csX ≥ 0.79 / csY ≥ 0.66, fresh | `actionStateShortcuts.js:359-364` |
| **F-tilt / u-tilt / d-tilt / jab** | A press + lsX·face > 0.3 / lsY > 0.3 / lsY < −0.3 / else jab (checked after smashes, so a *held* full deflection tilts) | `actionStateShortcuts.js:371-386` |
| Tap jump | lsY > 0.66 with 3-frames-ago < 0.2 (per-player `tapJumpOffp*` setting exists) | `actionStateShortcuts.js:507` |
| **Short vs full hop** | jump released before `jumpSquat` frames elapse → short hop (pure timing, digital-friendly) | `characters/shared/moves/KNEEBEND.js` |
| Crouch | lsY < −0.69 | `actionStateShortcuts.js:524` |
| Fast fall | lsY < −0.65 with 3-frames-ago > −0.1 | `actionStateShortcuts.js:271` |
| Aerial drift | ∝ lsX above 0.3 | `actionStateShortcuts.js:224-246` |
| **Airdodge/wavedash angle** | any nonzero stick → velocity 3.1·(cos θ, sin θ) of the exact stick angle (angle fully analog) | `characters/shared/moves/ESCAPEAIR.js:15-18` |
| Spotdodge (in shield) | lsY < −0.7 flick (4-frame lookback), or csY < −0.7 | `moves/GUARD.js:45` |
| Roll (in shield) | lsX·face beyond ±0.7 flick, or c-stick | `GUARD.js:50-55` |
| **Shield drop** | lsY < −0.65 (6-frame lookback > −0.3) on a platform — checked *after* spotdodge, so the usable band is **(−0.70, −0.65]**, mirroring real Melee's −0.6625…−0.6875 | `GUARD.js:60-62` |
| Grab | A while shielding (also Z = A + analog trigger 0.35 synthesized in the input layer) | `GUARD.js:40-43`, `input.js:221-227` |
| L-cancel | any trigger press on landing; `lCancelType` setting offers **Auto** | `settings.js` (`lCancelType`) |
| Up-B out of jumpsquat | B with lsY > 0.58 | `KNEEBEND.js` interrupt |

Key digital-mapping consequence: because a digital press goes 0→1.0 in one
frame, **every flick check auto-passes** — plain direction+A is always a
smash, plain sideways is always a dash. Tilts and walking therefore *need*
either a modifier value inside (0.3, 0.79)/(0.3, 0.66) or the authentic
hold-then-press timing (stick held ≥3 frames fails the flick check and
falls through to the tilt branch).

### 3.2 Prior art inside the codebase: meleelight's keyboard mapping

`pollKeyboardInputs` (`src/input/input.js:148-237`) is already a digital→
analog layer:

- WASD = stick, arrow keys = C-stick, with **opposite-key SOCD = neutral**.
- **Five stick modifier slots and five analog-trigger modifier slots**
  (`keyMap.lstick.modifiers`, `keyMap.shoulders.modifiers`,
  `src/settings.js:8-42`): each is `[keyCode, xMul, yMul]`, applied
  multiplicatively before rescaling — the same idea as Mod X/Mod Y.
- Default binding: Space = ×(0.7, 0.7); the four unbound slots default to
  ×(0.5, 0.5).
- **Quirk/bug worth knowing:** keyboard values go through `tasRescale`,
  which maps k → k·1.59375 before the unit-circle clamp
  (`meleeInputs.js:215-219`). So the shipped ×0.7 modifier saturates back
  to 1.0 on cardinals (0.7·1.59 = 1.11 → clamped) — it does nothing. Only
  multipliers ≤ ~0.627 survive; the 0.5 default lands at 0.7875, one
  quantum below the 0.79 dash/smash threshold — clearly the intended tilt/
  walk value. Lesson for the port: **synthesize final Melee-unit
  coordinates directly and skip `tasRescale`.**
- Keyboard Z is synthesized as A + lightest analog trigger (0.35), and
  digital L/R force analog 1.0 — precedents for chord-synthesis.

## 4. FunKey-S budget

Available: d-pad (4 digital directions, cross-shaped — opposite cardinals
unpressable), A, B, X, Y, L, R (digital, single-stage), Start. Fn/Menu is
reserved by the FunKey OS. That is **9 assignable inputs + 4 directions ≈
13 signals** vs the B0XX's ~20 buttons, and the d-pad must serve as the
only direction cluster (B0XX has 8 direction buttons: stick + C-stick).
Non-negotiable consumers: stick (d-pad), A, B, jump, shield, Start. That
leaves exactly **two free buttons** for modifiers / C-stick / grab.

Engine-settings assists (already in meleelight, zero engine changes):
set `tapJumpOffp1 = true` (up = 1.0 would otherwise tap-jump on every
upward DI) and optionally `lCancelType = 1` (Auto).

## 5. Candidate schemes

Common core for all three: d-pad = stick; A = attack; B = special;
Start = Start; SOCD 2IP-no-reactivation (moot on a cross pad); grab =
shield+A (native meleelight behavior); L-cancel = shield tap before
landing; shield+down-diagonal emits (0.7000, −0.6875) → shield drop;
shield+straight-down emits −1.0 → spotdodge. Mod coordinate set (Melee
units, 1/80-quantized): horizontal 0.6625, vertical ±0.5375, diagonal
(0.7375, 0.3125); Mod+shield+diagonal (0.6375, ±0.3750).

### S1 — "One-Mod + C-layer" (recommended)

X = jump, R = shield, **L = Mod** (tilt/walk/angle modifier),
**Y = C-stick layer** (while held, d-pad drives csX/csY instead of lsX/lsY).

| Mechanic | Performable? | How |
|---|---|---|
| Dash / dash-dance | yes | d-pad left/right (1.0 flick) |
| Walk | yes | L + left/right → 0.6625 |
| F/U/D-tilt vs smash | yes | tilt: L + dir + A (0.6625 / ±0.5375); smash: dir + A same-frame flick; walking f-tilt also works held+A |
| Jab | yes | A neutral |
| Short / full hop | yes | X tap vs hold (KNEEBEND timing, authentic) |
| Wavedash 45° | yes | X, then R + down-diagonal (0.7, −0.6875 — legal airdodge angle) |
| Shallow wavedash (~30°) | yes | X, then L + R + down-diagonal → (0.6375, −0.3750) |
| Steep wavedash (~60°) | **no** | needs a Mod-Y family — sacrificed |
| Wavedash in place | yes | X, then R + straight down |
| DI cardinal / diagonal | yes | d-pad 1.0 / (0.7, 0.7) |
| Firefox angles | partial | 45° native; ~23° via L+diagonal; steep 67° family sacrificed |
| C-stick aerials / ASDI | partial | hold Y + d-pad; **drift freezes while Y is held** (d-pad can't do both) |
| Charged smash | yes | dir + hold A |
| Shield drop | yes | R + down-diagonal (auto coordinate) |
| Spotdodge / rolls | yes | in shield: straight down / left / right (1.0 flicks) |
| Grab / JC grab | yes | R+A; X then R+A |
| Light shield / analog shield | **no** | single-stage triggers |
| Z-items, dedicated Z | **no** (grab covered) | |
| Ledgedash SOCD X=1.0 | n/a | cardinal already 1.0 |

Sacrifices: Mod-Y angle family (steep firefox/wavedash), light shield,
simultaneous drift+C-stick, dedicated Z.

### S2 — "Dual-Mod, B0XX-faithful angles"

X = jump, **Y = shield**, **L = Mod X, R = Mod Y**; C-stick layer on
L+R held together (HayBox precedent for chorded layers).

Gains over S1: full angle ladder — Mod Y horizontal 0.3375 (slow walk),
vertical 0.7375, steep diagonal (0.3125, 0.7375) = 67°, steep wavedash
(0.5000, −0.8500) = 59.5°, both B0XX airdodge families.
Costs: shield on a face button — wavedash becomes X→Y+d-pad (thumb roll),
shield+A grab and shield-drop chords all one-thumb; C-stick layer needs
both shoulders held, so effectively unusable during aerials. Sacrifices:
ergonomics (highest chord load), C-stick in practice, light shield.
Verdict: only worth it if angle variety proves match-relevant; angles are
explicitly best-effort in the goals.

### S3 — "Minimal, no layers"

X = jump, R = shield, L = Mod, **Y = grab (Z)**. No C-stick at all.

Same hard-goal row as S1 (all five pass — dash/walk, tilt/smash,
short/full hop, 45°+30° wavedash, DI). Sacrifices relative to S1: all
C-stick functions (aerials always via stick+A with drift coupling — a real
gameplay cost for retreating fairs and ASDI down; up/down aerials while
drifting require diagonal+A which risks u-smash… mitigated because aerial
state has no smash check, aerials read the 0.3 axis-dominance rule in
`checkForAerials`). Gains: dedicated grab button, zero layers/claw, the
simplest firmware. Good fallback if the Y-layer in S1 feels bad in play.

### Recommendation

Build the mapping layer around **S1**, keeping the coordinate table and
button-role assignment in data (a la HayBox `config`) so S2/S3 are config
swaps, not code. Every S1 coordinate above was checked against the
meleelight thresholds in §3.1 (walk 0.6625 ∈ (0.3, 0.79); tilt-Y 0.5375 ∈
(0.3, 0.66) and clear of crouch −0.69 / fastfall −0.65; shield-drop
−0.6875 ∈ (−0.70, −0.65]; all values ≥ deadzone 0.28 and on the 1/80
grid).

## 6. Sources

- B0XX Instruction Manual ("Manual and Manifesto"), full text:
  https://archive.org/stream/b-0-xx-manifesto-aziz-al-yami/B0XX%20Manual%20-%20Aziz%20Al-Yami_djvu.txt
  (coordinate tables §4–9, SOCD policy, rationale quotes); v2.1/v4 PDF
  mirrors: https://www.scribd.com/document/472160614/ and
  https://www.scribd.com/document/855470885/
- HayBox firmware (open-source reference implementation):
  https://github.com/JonnyHaystack/HayBox —
  `src/modes/Melee20Button.cpp` (all HayBox coordinates quoted here),
  `src/core/ControllerMode.cpp` (SOCD).
- b0xx-ahk keyboard implementation: https://github.com/agirardeau/b0xx-ahk
- Frame1 remapper/SOCD docs: https://frame1.gg/pages/remapper ;
  box-controller lineage FAQ: https://techhenzy.com/box-controllers-faq/
- Melee controller ruleset context:
  https://github.com/CarVac/MeleeConchRuleset/blob/main/ruleset.md
- meleelight source (oracle): https://github.com/schmooblidon/meleelight —
  `src/input/input.js`, `src/input/meleeInputs.js`, `src/settings.js`,
  `src/physics/actionStateShortcuts.js`, `src/characters/shared/moves/`
  (WAIT, WALK, KNEEBEND, GUARD, ESCAPEAIR).
