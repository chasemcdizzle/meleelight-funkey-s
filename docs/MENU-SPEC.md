# MENU-SPEC — faithful front-of-house behaviour

Status: SPEC (behavioural, implementable). Owner ruling 2026-07-27: the
restyled menu **designs are approved and are KEPT**; the **functionality is
not faithful** and must be re-derived from upstream. This document is the
binding behavioural contract for `port/foh/*`.

Method: every behavioural claim below is measured from upstream's own source
in `${MELEELIGHT_CLONE:-$HOME/.cache/meleelight-funkey-s/upstream}` at pin
`27af171`, cited `file.js:NN`. Nothing here is recalled from our port. Where
the FunKey-S hardware (240×240, d-pad, no mouse, no analog stick, no
keyboard, no gamepad) makes a verbatim port impossible, the departure is
marked **DEVIATION** and carries its justification — each DEVIATION is a
decision the owner may veto.

Gap column vocabulary, used per screen:

- **MATCHES** — our port already does what this spec says.
- **DIFFERS** — our port does something, but not this.
- **MISSING** — our port does not implement it at all.

---

## §0. Sources

| Screen | Upstream file | Lines | gameMode |
|---|---|---|---|
| Splash | `src/menus/startup.js` | 75 | 20 |
| Title | `src/menus/startscreen.js` | 192 | 0 |
| Main menu + 3 submenus | `src/menus/menu.js` | 534 | 1 |
| CSS | `src/menus/css.js` | 1263 | 2 |
| SSS | `src/menus/stageselect.js` | 208 | 6 |
| Target select | `src/stages/targetselect.js` | — | 7 |
| Audio options | `src/menus/audiomenu.js` | 223 | 10 |
| Gameplay options | `src/menus/gameplaymenu.js` | 256 | 11 |
| Keyboard rebinder | `src/menus/keyboardmenu.js` | 611 | 12 |
| Credits | `src/menus/credits.js` | 422 | 13 |
| Controller calibration | `src/menus/controllermenu.js` | 492 | 14 |
| Settings plane | `src/settings.js` | 59 | — |
| Globals / dispatch | `src/main/main.js` | — | — |

gameMode map is upstream's own comment block, `main.js:537-552`.
`keytest.js` is **not a screen** — it is a 256-entry keycode→label lookup
table (`keytest.js:1-260`) imported only by `keyboardmenu.js:10`.

---

## §1. Input model — 240×240, d-pad, and how it maps

### 1.1 Upstream's input surface

Menus read a per-port `Input` record with, relevantly:

- `lsX`, `lsY` — left analog stick, floats in `[-1, 1]`.
- `rawX`, `rawY` — undeadened stick, used only by credits (`credits.js:170-173`).
- `a`, `b`, `x`, `y`, `l`, `r`, `s` — buttons (booleans).
- `du`, `dd`, `dl`, `dr` — d-pad, **separate from the stick**.
- `pause[i][0]` / `pause[i][1]` — the START edge pair (`main.js:165`,
  `main.js:732/735`), which is what CSS launch reads.

Menus use two distinct paradigms:

1. **Discrete list menus** (`menu.js`, `gameplaymenu.js`, `audiomenu.js`,
   `keyboardmenu.js`) — stick past a hard `±0.7` threshold steps a cursor,
   with hold-to-repeat.
2. **Free pointer menus** (`css.js`, `stageselect.js`, `credits.js`) — the
   stick integrates a continuous 2D cursor position every frame, and every
   widget is a point-in-rect hit test. **There is no discrete row model at
   all on these screens.**

The owner's headline complaint is exactly this: our CSS was built as (1)
when upstream is (2).

### 1.2 The FunKey-S transport

Physical buttons (per `CLAUDE.md` "Device access"): d-pad `u/d/l/r`, face
`a/b/x/y`, `s` START, `k`/`n` = L/R shoulders, `q` MENU.

**DEVIATION D1 — d-pad drives the analog axes.** There is no analog stick.
The d-pad supplies `lsX/lsY ∈ {-1, 0, +1}` (full deflection while held, 0
released). Justification: every free-pointer screen integrates
`lsX/lsY * k` per frame (`css.js:195-196`, `stageselect.js:58-59`), so a
±1 d-pad reproduces upstream's *maximum-deflection* cursor motion exactly;
only intermediate speeds are lost, and no menu logic reads a magnitude
between 0 and 1. Diagonals are the two-button press, matching a stick held
at a corner.

**DEVIATION D2 — `du/dd/dl/dr` are unbindable.** Upstream reads the d-pad
as an input *independent* of the stick. On this device the d-pad IS the
stick, so the four d-pad-only behaviours cannot coexist with cursor motion.
Every affected site is listed with its disposition:

| Site | Upstream behaviour | Disposition |
|---|---|---|
| `css.js:452-455` | d-pad UP launches to SSS **ignoring `readyToFight`** | **DROPPED.** Unreachable; START launch (`css.js:446-451`) is the faithful path. |
| `css.js:456-459` | d-pad RIGHT force-sets your character to Falco (3) | **DROPPED.** An undocumented developer shortcut. |
| `keyboardmenu.js:69-72` | d-pad rows are rebindable targets | Out of scope, see §9. |

**DEVIATION D3 — cursor speed is expressed as a fraction of the screen, and
gains a calibration knob.** Upstream moves the CSS hand `12 px/frame` on a
1200×750 canvas (`css.js:195-196`) = 1.00% of width, 1.60% of height per
frame; the SSS ring moves `15 px/frame` (`stageselect.js:58-59`) = 1.25% /
2.00%. On 240×240 that is 2.40 / 3.84 px/frame (CSS) and 3.00 / 4.80
px/frame (SSS). Cursor position is held in **doubles**, never integers, and
rounded only at draw time. Full-width traversal stays 100 frames (CSS) /
80 frames (SSS), i.e. the same feel as upstream.

> `ponytail:` expose one knob `FOH_CURSOR_SPEED` (default `1.0`,
> multiplies both axes). Hardware feel cannot be judged from source; the
> owner tunes one number rather than us guessing a new curve.

**DEVIATION D4 — hit regions are our layout's rectangles, not upstream's
pixel coordinates.** The designs are approved and already re-laid-out for
240×240 in `port/foh/foh_render.c`. Carrying upstream's literal rects
(e.g. the marth cell at x∈[398,493], y∈[240,335], `css.js:219-221`) would
put hit regions where nothing is drawn. Therefore: **the semantic widget
table in each section below is normative; the rectangle for each widget is
whatever our renderer draws for it.** Every widget must be hit-testable at
its drawn extent, and there must be no hit region without a drawn widget
(and vice versa).

### 1.3 Faithfulness goal, stated precisely

Semantics are the contract. Transport necessarily differs. Concretely, for
every screen: the set of reachable states, the set of legal actions in each
state, the guard on each transition, and the value domain of every setting
must match upstream. Pixel geometry, framebuffer layout and the physical
button that carries an action may differ, and where they do it is recorded
as a DEVIATION above.

---

## §2. CSS — character select (gameMode 2) — DEEP SECTION

This is the owner's headline item. Everything below is `src/menus/css.js`,
control function `cssControls(i, input)` at `css.js:182-461`, called once
per **connected port** per frame from `main.js:956-960`:

```js
for (var i = 0; i < 4; i++) {
  if (i < ports) {
    input[i] = interpretInputs(i, true, playerType[i], oldInputBuffers[i]);
    cssControls(i, input);
  }
  actionStates[characterSelections[i]][player[i].actionState].main(i, input);
}
```

Note `i < ports` — a port gets a hand iff a *device* joined, independent of
its `playerType`. A port set to CPU still has a live hand.

### 2.1 Complete state the CSS owns

| Variable | Decl | Initial | Meaning |
|---|---|---|---|
| `handType[4]` | `css.js:63` | `[0,0,0,0]` | Hand sprite: `0`=`handPoint`, `1`=`handOpen`, `2`=`handGrab` (draw switch `css.js:1135-1143`) |
| `handPos[4]` | `css.js:64` | `(140,700) (365,700) (590,700) (815,700)` | Free cursor per port, **doubles** |
| `tokenPos[4]` | `css.js:65` | `(421,268) (461,268) (421,308) (461,308)` | Token position per port |
| `chosenChar[4]` | `css.js:66` | `[0,0,0,0]` | Character index per port (0..4) |
| `tokenGrabbed[4]` | `css.js:67` | `[false×4]` | Is port k's token currently held by someone |
| `whichTokenGrabbed[4]` | `css.js:68` | `[-1×4]` | Which port's token hand `i` is carrying, `-1` = none |
| `occupiedToken[4]` | `css.js:69` | `[false×4]` | Token k is claimed (blocks a second grab) |
| `bHold[4]` | `css.js:70` | `[0,0,0,0]` | B-held frame counter per port |
| `cpuSlider[4]` | `css.js:72` | x = `283 + 225k`, y = `595` | CPU-level knob position per port |
| `cpuGrabbed[4]` | `css.js:74` | `[false×4]` | Is hand `i` dragging a CPU knob |
| `whichCpuGrabbed[4]` | `css.js:75` | `[-1×4]` | Which port's knob hand `i` drags |
| `occupiedCpu[4]` | `css.js:76` | `[false×4]` | Knob k is claimed |
| `readyToFight` | `css.js:78` | `false` | Recomputed **in the draw function**, `css.js:1167-1181` |
| `rtfFlash`, `rtfFlashD` | `css.js:80-81` | `25`, `1` | READY-TO-FIGHT banner pulse |
| `choosingTag` | `css.js:57` | `-1` | Port currently typing a name tag, `-1` = none |

Shared globals it mutates: `playerType[4]` (`main.js:107`, init `[-1,-1,-1,-1]`),
`cpuDifficulty[4]` (`main.js:109`, init `[3,3,3,3]`), `pPal[4]`
(`main.js:159`, init `[0,1,2,3]`), `versusMode` (`main.js:140`, init `0`),
`hasTag[4]` / `tagText[4]` (`main.js:153`), `characterSelections` via
`setCS`.

### 2.2 The hand-cursor model

Motion, unconditional every frame for every connected port
(`css.js:195-206`):

```js
handPos[i].x += input[i][0].lsX * 12;
handPos[i].y += -input[i][0].lsY * 12;
if (handPos[i].x > 1200) { handPos[i].x = 1200; } else if (handPos[i].x < 0) { handPos[i].x = 0; }
if (handPos[i].y > 750)  { handPos[i].y = 750;  } else if (handPos[i].y < 0) { handPos[i].y = 0; }
```

Normative properties:

1. **Free 2D motion, clamped to the screen.** Not rows. Not snapping. Not
   nearest-widget. The cursor goes anywhere.
2. Speed is `12 px/frame` at full deflection (§1.2 D3 rescales this).
3. Clamp is to the full canvas, so the cursor can always be recovered.
4. Position **persists** across CSS entry/exit — `handPos` is module scope
   and is never re-initialised on entry.
5. Four independent cursors, one per connected port; they do not interact
   except through the `occupied*` claim flags.
6. The hand sprite is drawn at `(x-40, y-30)` sized `101×133`
   (`css.js:1137/1140/1143`) — the *logical* hit point is `handPos` itself,
   near the sprite's fingertip, not the sprite centre. Our renderer must
   preserve "the pointed fingertip is the hot spot".

### 2.3 The three screen bands

The whole `cssControls` body is a three-way branch on cursor position and
carry state:

| Band | Guard | Line |
|---|---|---|
| **Roster band** | `handPos[i].y < 400 && handPos[i].y > 160` | `css.js:207` |
| **CPU-knob drag** | `cpuGrabbed[i]` | `css.js:316` |
| **Everything else** | else | `css.js:297`, `css.js:336` |

Inside the roster band the hand is `handOpen` (`css.js:208`); outside it is
`handPoint` (`css.js:332`/`css.js:336`); carrying anything makes it
`handGrab` (`css.js:210/217/304/406`).

### 2.4 Token model — grab

There are exactly **two** ways to pick up a token.

**(a) B, on your own token, from anywhere in the roster band** —
`css.js:209-215`:

```js
if (input[i][0].b && !input[i][1].b && playerType[i] == 0 && whichTokenGrabbed[i] == -1) {
  handType[i] = 2;
  setTokenPosValue(i, new Vec2D(handPos[i].x, handPos[i].y));
  tokenGrabbed[i] = true;
  whichTokenGrabbed[i] = i;
  occupiedToken[i] = true;
}
```

This is the owner's "**B retrieves your token back to your hand**". Note:
it does **not** require the cursor to be over the token — anywhere in the
roster band works, the token teleports to the hand. Guards: rising edge on
B; the port must be `playerType == 0` (HMN); you must not already be
carrying something.

**(b) A, with the cursor on top of a grabbable token** — `css.js:298-312`:

```js
for (var j = 0; j < 4; j++) {
  if (!occupiedToken[j] && (playerType[j] == 1 || i == j)) {
    if (handPos[i].y > tokenPos[j].y - 20 && handPos[i].y < tokenPos[j].y + 20 &&
        handPos[i].x > tokenPos[j].x - 20 && handPos[i].x < tokenPos[j].x + 20) {
      if (input[i][0].a && !input[i][1].a) {
        handType[i] = 2;
        whichTokenGrabbed[i] = j;
        setTokenPosValue(whichTokenGrabbed[i], new Vec2D(handPos[i].x, handPos[i].y));
        tokenGrabbed[whichTokenGrabbed[i]] = true;
        occupiedToken[whichTokenGrabbed[i]] = true;
        break;
      }
    }
  }
}
```

Ownership rule, verbatim from the guard `playerType[j] == 1 || i == j`:
**you may grab your own token, or the token of any port set to CPU.** You
may not grab another human's token. This is the owner's "**the CPU token
gets picked up too**" — it is how you set a CPU opponent's character.

Hit region is a `±20` square around the token centre (a 40×40 box), tested
against the cursor point.

### 2.5 Token model — carry, hover-select, drop

While `tokenGrabbed[whichTokenGrabbed[i]]` (`css.js:216-296`):

```js
handType[i] = 2;
setTokenPosValue(whichTokenGrabbed[i], new Vec2D(handPos[i].x, handPos[i].y));
if (handPos[i].y > 240 && handPos[i].y < 335) {
  if (handPos[i].x > 452 - o && handPos[i].x < 547 - o) {          // o = 54, css.js:184
    if (chosenChar[whichTokenGrabbed[i]] != 0) {
      chosenChar[whichTokenGrabbed[i]] = 0;
      changeCharacter(whichTokenGrabbed[i], 0);
      sounds.menuSelect.play();
    }
    if (input[i][0].a && !input[i][1].a) {
      tokenGrabbed[whichTokenGrabbed[i]] = false;
      occupiedToken[whichTokenGrabbed[i]] = false;
      setTokenPosValue(whichTokenGrabbed[i], new Vec2D(473 - o + (whichTokenGrabbed[i] % 2) * 40,
                                                       268 + (whichTokenGrabbed[i] > 1 ? 40 : 0)));
      whichTokenGrabbed[i] = -1;
      sounds.marth.play();
    }
  } else if (/* puff, fox, falco, falcon — identical shape */) { … }
}
```

Normative semantics:

1. **The token follows the cursor exactly** — one `Vec2D` per frame at the
   hand position.
2. **Hovering a character cell selects that character LIVE** — not on
   press. `chosenChar` and `changeCharacter()` fire the instant the cursor
   enters the cell, guarded by `!= c` so the sound plays once per change
   (`css.js:222-226`, `236-240`, `251-254`, `266-269`, `281-285`).
   `changeCharacter` (`css.js:160-167`) calls `setCS`, resets the CSS
   preview doll to `WAIT`/timer 0, and re-points `charAttributes` /
   `charHitboxes`.
3. **A drops the token into the hovered cell.** The token snaps to a
   canonical resting slot, `whichTokenGrabbed` clears, and the
   **character's announcer sound** plays (`sounds.marth`, `.jigglypuff`,
   `.fox`, `.falco`, `.falcon` at `css.js:233/248/263/278/293`).
4. Five cells, contiguous, 95 px wide, in roster order
   **marth(0) · puff(1) · fox(2) · falco(3) · falcon(4)**, all inside
   `y ∈ (240, 335)`.
5. Drop slot per (character c, port k) is
   `x = 419 + 95c + (k % 2) * 40`, `y = 268 + (k > 1 ? 40 : 0)` — the four
   ports stack 2×2 within one cell so all four can share a character.

**Answering "right now there is no way to select your own character":**
upstream's ONLY character-selection gesture is grab-token → hover-cell →
drop. There is no left/right value stepper anywhere in `css.js`. A port
that never grabs its token keeps `chosenChar = 0` (marth) forever. So a
faithful CSS *requires* the token gesture — it is not a decoration.

### 2.6 Token model — invalid drop / leaving the band

If the cursor leaves the roster band while carrying, control falls to the
`else` at `css.js:336-347`:

```js
handType[i] = 0;
setTokenPosValue(whichTokenGrabbed[i], new Vec2D(518 + (whichTokenGrabbed[i] % 2) * 40 +
                 chosenChar[whichTokenGrabbed[i]] * 93, 268 + (whichTokenGrabbed[i] > 1 ? 40 : 0)));
if (whichTokenGrabbed[i] > -1 && tokenGrabbed[whichTokenGrabbed[i]] == true) {
  tokenGrabbed[whichTokenGrabbed[i]] = false;
  occupiedToken[whichTokenGrabbed[i]] = false;
}
whichTokenGrabbed[i] = -1;
```

Semantics: **leaving the roster band silently commits the currently hovered
character and returns the token to rest.** There is no "invalid drop"
rejection and no snap-back-to-origin. Carry it out of the band and whatever
you last hovered is your pick.

**~~Quirk Q1 (carried verbatim)~~ — HALF RETIRED 2026-08-24, DEVIATION D46.**
The rest position here uses base `518` and pitch `93`, while the A-drop path
(§2.5) uses base `419` and pitch `95`. This spec called that "a few px";
**MEASURED it is 99 px on a 95 px cell pitch — one WHOLE CELL**, so a token
dropped by leaving the band lands on the NEXT character's cell, not slightly
off. The magnitude claim above was wrong, and the owner filed the
consequence: *"whenever the pin is let go of (going off) it should go back
to the character you selected."* The leave-band drop now homes on the
SELECTION like the other two rest paths (D21, D35), and `foh_css_token_pos`
states that rule ONCE for all three rather than as three arms that agree.
The two constants stay in `foh.h` as the measurement; nothing computes a
position from them. See D46 in §12.1.

**Quirk Q2 (carried verbatim):** `setTokenPosValue` is called
*unconditionally* at `css.js:338`, including when `whichTokenGrabbed[i] ===
-1`. In JS this writes the property `tokenPos[-1]`, and
`chosenChar[-1]` is `undefined` so the computed `x` is `NaN`. It is
harmless because nothing ever reads index `-1`. In C this must be an
explicit no-op guard — reproducing a JS negative-index write is neither
possible nor observable.

### 2.7 Port type cycling — HMN ↔ CPU ↔ NET ↔ N/A

Hit region and action, `css.js:348-357` (inside the out-of-band branch):

```js
for (var j = 0; j < 4; j++) {
  if (handPos[i].y > 430 && handPos[i].y < 485 &&
      handPos[i].x > 109 + j * 225 && handPos[i].x < 207 + j * 225) {
    if (input[i][0].a && !input[i][1].a) {
      togglePort(j);
      hasTag[j] = false;
      sounds.menuSelect.play();
    }
  }
}
```

So: **the HMN/CPU label is a clickable widget** — cursor over port j's type
box, press A, the type advances. Any port's hand may toggle any port's box.
Toggling also clears that port's name tag.

`togglePort`, `main.js:510-526`:

```js
export function togglePort (i){
  playerType[i]++;
  if (playerType[i] == 3) { playerType[i] = -1; … }
  if (playerType[i] == 0 && ports <= i) { playerType[i] = 1; … }
}
```

Cycle: `-1 (N/A) → 0 (HMN) → 1 (CPU) → 2 (NET) → -1`. The second guard
skips HMN for ports with no attached device (`ports <= i`).

**Answering "P1 can be set to CPU":** yes. For port 0 with at least one
device attached, `ports <= 0` is false, so no arm intercepts, and
`playerType[0]` cycles through `1` (CPU) like any other port. A CPU'd P1
still gets a hand (`main.js:957` gates on `i < ports`, not on type), and
its token becomes grabbable by anyone via the `playerType[j] == 1` arm of
§2.4(b) — while the B-retrieve gesture stops working for it, since that
arm requires `playerType[i] == 0` (`css.js:209`).

Labels drawn from the same enum, `css.js:860-879`:

| `playerType` | Label | Colour |
|---|---|---|
| `-1` | `"N/A"` | `rgb(82,81,81)` |
| `0` | `"HMN"` | `rgb(201,178,20)` |
| `1` | `"CPU"` | `rgb(161,161,161)` |
| `2` | `"NET"` | `rgb(66,241,244)` |

**DEVIATION D5 — NET is removed from the cycle.** Cycle becomes
`N/A → HMN → CPU → N/A` (3 states). Justification: NET requires the
netplay/spectate stack, which is out of scope per §11 and gated behind a
named flag; leaving a reachable NET state would produce a port that can
never launch. A future netplay ARC restores the 4-cycle verbatim. Note it
is not gated on `FOH_NETPLAY`: that flag covers the battle page only and
this cycle is DEVIATION D5, unconditional at either flag value (§11.1).

**~~DEVIATION D6 — ports 3 and 4 stay N/A.~~ RETIRED 2026-08-24 (A44).**
It said their panels render but their type boxes do not toggle, because a
3-/4-participant launch was unverified against the oracle. **A46 verified
one** — `sim_setup_match_ports` is upstream's own four-port
`harnessSetupMatch` loop and the four-port golden `q01` replays
`STREAM MATCH 3600/3600 frames exact` through the UNCHANGED
`verify-stream.js`. The condition D6 named for its own deletion was met, so
it is deleted. What replaces it is **DEVIATION D40**, which is narrower: the
ports are live, and what remains restricted is the token-ownership guard.
D40's second half — no CPU on ports 2/3 — was **RETIRED by owner ruling
(A49, 2026-08-24)**; see D40 in §12.1.

**DEVIATION D40 — one hand owns every token.** *(Half of this deviation,
(b), is RETIRED as of A49; it is kept below because the retirement is only
legible beside what it replaced.)*
(a) Upstream's grab guard is `playerType[j] == 1 || i == j` (`css.js:300`) —
hand *i* takes its own token or any CPU port's, never another human's,
because upstream has FOUR hands and each human works its own. This device
has ONE input device and therefore one hand (§2.1's port model), so that
guard protects nobody here and instead removes the only way to give a HUMAN
port a character. For P2 that was survivable through the CPU detour §2.7
describes; for P3/P4 it is not, because (b) leaves them no CPU state to
route through — a port that can be switched on and never given a character
is the stub HARD RULE 2 forbids. (a) STANDS.
Consequence, stated because a player will see it: a HMN port with no
physical controller stands still — already true of P2 today, and closed by
a second controller (the A33 spike), not by this screen.

**~~(b) Ports 2/3 cycle N/A -> HMN -> N/A~~ — RETIRED 2026-08-24 (A49,
owner ruling: *"yeah enable the CPu please"*).** All four ports now cycle
N/A -> HMN -> CPU -> N/A, which is upstream's own `togglePort` with only
D5's NET step dropped, and all four CPU-level knobs are live (upstream's
`cpuSlider` was always a four-element array, `css.js:72`, and its grab loop
always ran `s < 4`, `css.js:397`).
**D40(b)'s STATED GROUND WAS WRONG and the correction is the durable part:**
AIBRIDGE1 is the RECORDED stream used to REPLAY a CPU golden — it is not
what makes the AI run. The play path links the LIVE C `ai.c` through
`ml_sim_runai_live` (`check-ai-live.sh` -> `AI LIVE CONFORMS`), so a CPU on
port 2 or 3 genuinely plays. What AIBRIDGE1's single slot limits is
**verification coverage, not capability**, and that is a scope call the
owner owns and has made.
**ACCEPTED CONSEQUENCE, carried at the site (`foh.c`'s launch guard,
`foh_launch_witness.c`'s verdict table, and `judge-domains.authored.txt`):
a 3- or 4-player match with a CPU on port 2 or 3 is PLAYABLE but NOT
checksum-verified**, because no golden can replay more than one CPU slot.
fix_plan **A48** tracks widening the bridge.

**DEVIATION D41 — the token 2x2.** Four tokens on one 44 px cell need
upstream's own 2x2 stack (`css.js`, and the note this port's constant block
has always carried) rather than the single row two ports read as. `r` 9 ->
7, column and row pitch 20 -> 14, `DX` 12 -> 15, `y` `CELL_Y+11` ->
`CELL_Y+9`; the four inequalities that fix them are at `FOH_CSS_TOKEN_R` in
`port/foh/foh.h`. The shape did not change — the numbers did — and the
alternative was measured: four r=9 tokens in a row span 78 px of a 44 px
cell, and the clamp would have parked port 0's token a whole cell LEFT of
the character it chose, re-creating D21's defect in the one configuration
A44 exists to add.

**DEVIATION D17 — a CPU in port 1 cannot LAUNCH.** The menu plane is
faithful and unchanged: `togglePort` has no port-0 special case
(`main.js:510-526`), port 1's type box cycles to CPU like any other port
(§2.7), a CPU'd P1 still gets a hand (`main.js:957` gates on `i < ports`,
not on type), and READY TO FIGHT still appears under §2.5's unmodified
rule — two non-N/A ports, no token held. What refuses is the LAUNCH:
`foh.c:671-685` emits `refused portconfig` for any configuration other
than (P1 HMN, P2 HMN|CPU). Justification, and the reason this is a
deviation rather than a bug: `sim_setup_match` takes no P1 type at all
(`port/sim/sim/sim.h:98-100`) and pins `types[0] = 0`
(`sim_boot.c:374-383`), so `slotIsAi[0]` can never be true — launching a
CPU'd P1 would boot a HUMAN P1 and the LAUNCH record would describe a
match that did not run (HARD RULE 2). Like D6 this is a *launch-plane*
limitation, not a menu-plane one: it is the same class as D6 (the CSS can
reach a configuration the launch plane cannot honour) but a strictly
different scope, so it is registered separately rather than folded into
D6's ports-3/4 wording. Closing it means giving `sim_setup_match` both
ports' type and level and re-proving conformance — a sim-surface change,
outside the menus lane. Until then the refusal is LOUD (`deny` + a
`refused portconfig` trace event), never silent.
**A44 amendment (2026-08-24):** the *quoted mechanism* above is stale —
`sim_setup_match_ports` (A46) does take every port's type, so the sim could
represent a CPU port 0. **D17 itself stands, on a different and narrower
ground:** port 0 is the port the physical controller drives and a CPU there
has no input source on the launch path, so it would still boot a match the
LAUNCH record misdescribes. The launch guard now spells TWO conditions —
port 0 HMN, and at least two participants (A49 deleted the third, "no CPU
above port 1", with D40(b)) — and `port/foh/foh_launch_witness.c` judges all
81 cells of the four-port type grid against an authored verdict table:
26 launch, 55 refuse.

### 2.8 CPU-level widget

Grab, `css.js:396-408`, only when not already dragging:

```js
if (!cpuGrabbed[i]) {
  for (var s = 0; s < 4; s++) {
    if (playerType[s] == 1) {
      if (!occupiedCpu[s]) {
        if (handPos[i].y >= cpuSlider[s].y - 25 && handPos[i].y <= cpuSlider[s].y + 25 &&
            handPos[i].x >= cpuSlider[s].x - 25 && handPos[i].x <= cpuSlider[s].x + 25) {
          if (input[i][0].a && !input[i][1].a && allowRegrab) { … handType[i] = 2; … }
```

Drag, `css.js:316-334`:

```js
handPos[i].y = cpuSlider[whichCpuGrabbed[i]].y + 15;
if (handPos[i].x < 152 + 15 + whichCpuGrabbed[i] * 225) { handPos[i].x = 152 + 15 + whichCpuGrabbed[i] * 225; }
if (handPos[i].x > 152 + 15 + 166 + whichCpuGrabbed[i] * 225) { handPos[i].x = 152 + 15 + 166 + whichCpuGrabbed[i] * 225; }
cpuSlider[whichCpuGrabbed[i]].x = handPos[i].x;
cpuDifficulty[whichCpuGrabbed[i]] =
  Math.round((cpuSlider[whichCpuGrabbed[i]].x - whichCpuGrabbed[i] * 225 - 152 - 15) * 3 / 166) + 1;
```

Normative semantics:

1. It is a **grabbable knob**, not a left/right stepper. A grabs it, the
   hand is then *locked to the slider rail* (y forced, x clamped to the
   166-px track), A releases (`css.js:328-333`).
2. **The value domain is 1..4**, measured: the track parameter `t ∈ [0,1]`
   maps `round(t*3)+1 ∈ {1,2,3,4}`. Not 1..9. (The oracle harness's
   `--difficulty 1-9` is a programmatic surface, not the CSS's.)
3. Default is `3` (`main.js:109`).
4. The knob only exists for ports with `playerType == 1`; the whole CPU
   panel is drawn under that guard (`css.js:937-969`).
5. `allowRegrab` (`css.js:183`, cleared at `css.js:333`) prevents
   re-grabbing on the same frame you released.
6. `player[k].difficulty` is written through on every drag frame
   (`css.js:327` region) so the preview doll reacts immediately.

**DEVIATION D7 — the knob is grab-drag, and the d-pad drives it at the
cursor speed of §1.2.** No change in semantics; noted only because a
166-px track at 2.4 px/frame is ~69 frames end to end, which the
`FOH_CURSOR_SPEED` knob covers.

### 2.9 Other CSS widgets

| Widget | Guard | Line | Action |
|---|---|---|---|
| BACK chevron | `y < 160 && x > 920`, A edge | `css.js:358-363` | `menuBack`, `changeGamemode(1)` → main menu |
| BACK hold bar | `bestHold > 0` (max `bHold[]`) | `css.js:735-746` | red quad, x 1020 → 1020 + 6·`bestHold`, y 119..125 — the wedge's bottom lip filling left to right; full at `bestHold == 30`, the frame the B-hold fires |
| Mode ribbon | `y ∈ (100,160)`, `x ∈ (380,910)`, A edge | `css.js:390-395` | `setVersusMode(1 - versusMode)` — a BINARY toggle, `main.js:140/237`. `0` = "4-man survival test!" (stocks), `1` = "An endless KO fest!" (`css.js:717-721`) |
| Palette next | X edge, anywhere | `css.js:366-377` | `pPal[tok]++`, wrap `>6 → 0`; `tok` = carried token's port else self |
| Palette prev | Y edge, anywhere | `css.js:378-389` | `pPal[tok]--`, wrap `<0 → 6` |
| Tag: random | `y ∈ (640,680)`, `x < 154 + i*225` | `css.js:421-426` | `hasTag[i]=true`, random from `randomTags` (`main.js:142`) |
| Tag: clear | `y ∈ (640,680)`, `x > 286 + i*225` | `css.js:427-431` | `hasTag[i] = false` |
| Tag: type | `y ∈ (640,680)`, middle third | `css.js:432-439` | `choosingTag = i`, shows an **HTML `<input>`** overlay |

`versusMode` is **on the sim surface**, not cosmetic: read at
`actionStateShortcuts.js:155` and `physics.js:980` (`if (player[i].stocks
=== 0 && versusMode)`) — `0` = stock match ("4-man survival test!"), `1` =
infinite respawn ("An endless KO fest!"), blurb at `css.js:717-721`.

The palette range is **0..6** (7 palettes), defaults `[0,1,2,3]`
(`main.js:159`). X/Y exist on the FunKey-S, so this maps directly with no
deviation.

**DEVIATION D8 — name tags are cut.** `choosingTag` opens a jQuery-shown
HTML text input (`css.js:438`: `$("#pTagEdit" + i).show().select()`) and
commits on `keys[13]` (Enter) or A (`css.js:443-444`). There is no DOM, no
keyboard and no text-entry surface on the device. Justification: the
mechanism is browser-only. **Partial recovery is cheap and recommended:**
keep the *random tag* and *clear tag* sub-widgets (`css.js:421-431`) —
they need only `randomTags` (34 strings, `main.js:142`) and a boolean, and
they are what makes the port panels show something other than the character
name (`css.js:1006-1008`). Drop only the free-text third.

**DEVIATION D18 — free-text name entry RETURNS as a NOVEL letter-grid screen
modelled on real Melee. SUPERSEDES D8's "drop the free-text third".**
Owner ruling 2026-08-03, verbatim: *"why not match the real melee name select?
novel screen (doesn't match the original melee light). that'd be awesome"*.

*Why this is a DEVIATION and not a port.* Every other DEVIATION in this file
departs from an upstream behaviour because the hardware cannot carry it. This
one is different in kind and must not be filed as if it were the same: **there
is no upstream behaviour to be faithful to.** Upstream's name entry is a DOM
`<textarea>` shown by jQuery (`css.js:438`) and read back on commit
(`css.js:176`); there is no canvas code, no layout, no cursor, no glyph
handling — nothing a port could reproduce or diverge from. So this is the
FIRST INVENTED SCREEN in the port, distinct from the invented *values* the
restyle already carries. It is a knowing departure, taken because a letter
grid on a d-pad handheld is better than a text box that cannot exist.

*What is MEASURED from upstream and therefore BINDING.*
- Commit is the A rising edge or Enter (`css.js:443-444`); Enter has no device
  key, so A is the commit and the grid needs its own visible confirm affordance.
- Entry is MODAL: while `choosingTag > -1` the entire per-slot CSS input block
  is skipped (`css.js:185`), so grabbing, port cycling, palette and CPU-level
  input are all dead during entry. The letter grid inherits this exactly.
- Maximum length is **10** — `maxlength="10"` on every `#pTagEdit<i>`
  (`dist/meleelight.html:227`). Not 4. Real Melee's 4-character tag is NOT the
  bound here; upstream's own data would not fit it.
- The committed string replaces the character name in the port panel, gated by
  `hasTag[i]` (`css.js:1006-1008`).
- The grid alphabet MUST span every character reachable by the random-tag
  widget, or "random" can produce a name the grid cannot display or edit.
  Measured over the 34 `randomTags` (`main.js:142`; 33 unique, `"S2J"` appears
  twice): **54 distinct characters** — 23 uppercase, 22 lowercase, digits `0`
  and `2`, and the 7 symbols `! $ ( ) . [ ]`. Mixed case is therefore
  mandatory (`Panda`, `Mang0`, `aMSa`, `HungryBox`), and a naive A-Z-only grid
  is measurably wrong.

*What is MODELLED on real Melee — SHAPE ONLY, and NOT measured.* This repo has
no Melee build to measure and none is obtainable here, so every claim in this
paragraph is a design intent, not a fact, and carries no citation by
construction. Modelled: a fixed grid of glyphs; the d-pad moves a cursor cell
by cell; A appends the highlighted glyph; B deletes the last character; the
name renders as it is typed. NOT modelled, and deliberately: Melee's
4-character limit (upstream's 10 wins, above), its exact grid dimensions,
glyph order, page/shift mechanism, sound set, and any animation. Where a
detail is ours rather than Melee's, it is ours — do not later "correct" the
port toward half-remembered Melee behaviour and call it faithfulness.

*BLOCKING CONSTRAINT — the font cannot draw the data (measured 2026-08-04).*
Both halves of D8 are blocked on the A14 glyph-atlas swap, and the attempt that
proved it is why this paragraph exists. Coverage read directly from
`port/foh/foh_font.c`: face 1 = 49 glyphs (space, `!'()+,-./`, `0-9`, `:<>`,
`A-Z`), face 2 = 51 (same plus `&?`). **Neither face has a single lowercase
letter.** Against the 54 characters `randomTags` spans, **25 are unrenderable**
— all 22 lowercase plus `$`, `[`, `]`. A tag widget built today `gfx_fatal`s on
the first `Panda` (reproduced: `SIM FATAL frame 0: foh_font: no glyph for
requested character`). Hand-authoring the missing glyphs is explicitly
forbidden — `foh_font.c:71` ("face 1's coverage is deliberately NOT widened")
and punch-list A8-F2. So the order is fixed: **A14 first, and A14's capture
must include a-z and `$[]`, not just the A-Z its plan named.**

*Scope split (owner-stated, and the two halves are independent).*
(i) **random-tag + clear-tag** (`css.js:421-431`) are a REAL upstream
transliteration and may land alone, with no grid;
(ii) **the letter grid** is the novel work and needs this deviation registered
first — which this section is.

*Open, to settle at implementation time, not now:* cursor wrap at grid edges,
whether B on an empty name exits entry or is a no-op, and the confirm
affordance's label. Each is invented; each gets recorded here when chosen.

**D18.1 — THE TAG ROW: measured upstream shape, re-laid-out per DEVIATION D4.**
Upstream's tag widget is ONE clickable row per port, `css.js:415-439`: hit
region `handPos.y` in (640, 680) and `handPos.x` in (130 + i·225, 310 + i·225)
— 180 px wide — split by x into three zones that share one A-press arm:

| zone | upstream x | width | share | action (`css.js`) |
|---|---|---|---|---|
| random | `< 154 + i·225` | 24 | 13.3% | `menuSelect`; `hasTag[i]=true`; `tagText[i]=randomTags[…]` (`:423-426`) |
| set | middle | 132 | 73.3% | `menuSelect`; `hasTag[i]=true`; `choosingTag=i` — the D18 grid (`:432-438`) |
| remove | `> 286 + i·225` | 24 | 13.3% | `menuSelect`; `hasTag[i]=false` (`:428-430`) |

Our port panel is 58 px wide at `foh_css_panel_x(k)`, `FOH_CSS_PANEL_Y` 96,
`FOH_CSS_PANEL_H` 120, so it ends at y=216 and the 240×240 screen leaves a free
strip below it (measured: READY TO FIGHT is at y=68, the ghost port letter at
y=194, the CPU slider at y=174-200 — nothing occupies 216-240). The tag row is
therefore **y 217..229** (12 px, the same height as the name plate at
`foh_render.c:1495`), full panel width, with the three zones carried at
upstream's PROPORTIONS rather than its pixels: **random x..x+8 · set x+8..x+50
· remove x+50..x+58** (58 × 24/180 = 7.73 → 8; 58 × 132/180 = 42.5 → 42;
8+42+8 = 58 exactly). Two 8 px targets are small for a d-pad cursor; that is a
consequence of honouring the proportions, and if play shows it is unusable the
fix is a recorded re-proportioning here, not a silent nudge in the C.

**The committed tag REPLACES the character name in the panel**, gated by
`hasTag[i]` (`css.js:1006-1008`) — in our renderer that is the single
`kCharShort[nameChr]` argument at `foh_render.c:1496`, same plate, same box.

**Port-type cycling CLEARS the tag**: `togglePort(j)` is immediately followed by
`hasTag[j] = false` (`css.js:353`). Our cycle site already notes this at
`port/foh/foh.c:634` ("Upstream also clears the port's name tag; tags are
DEVIATION D8 and not in this build") — that comment is the hook.

**DEVIATION D19 — the random-tag draw uses a FOH-LOCAL stream and CANNOT shift
the match. Registered before any code (owner's standing rule for
checksum-surface changes).**

*The first draft of this section said the opposite and was wrong.* It claimed
we would mirror upstream's coupling. Measuring the port refuted it, and the
refutation is the interesting part, so it is recorded rather than quietly
replaced.

MEASURED 2026-08-04:
- The FOH draws **no RNG at all** today — `port/foh/` contains no `ml_random`
  call, and `foh_app.c:328`'s `mulberry_inv` only COUNTS draws.
- Upstream `css.js:425` draws from the SAME global stream the simulation
  consumes, so upstream a player who presses random-tag before a match shifts
  that match's RNG.
- **Our port cannot reproduce that, by construction.** The browser's entire
  pre-match menu RNG consumption is modelled as a CONSTANT: the launch path
  seeds the sim and burns exactly **465 boot draws** (`foh_dev.c:51`,
  `sim_boot.c:8` — "465 boot draws (menu plane)"), a figure pinned by the qjs
  boot guard (CLAUDE.md M0 task 6) because "mulberry32 state is never re-seeded
  at setupMatch, so boot draw count misalignment silently shifts the in-match
  stream". The FOH runs BEFORE `sim_setup_match`; there is no sim stream in
  existence for it to draw from.

So the choice is not "mirror or deviate" — mirroring is unavailable, and
faking it by making the FOH draw into the sim's stream would corrupt the 465
pin and silently shift every in-match stream. **The random-tag draw therefore
comes from a FOH-LOCAL mulberry32 (`port/sim/ml_rng.h` is header-only inline,
so this links nothing new), and pressing random-tag has NO effect on the match
that follows.** The port is strictly MORE deterministic than upstream here:
same seed, same match, whatever the player did to the tags.

Consequences, stated rather than discovered later:
- No frozen golden is affected, and no flow becomes a golden re-freeze. A flow
  MAY press random-tag freely — the opposite of what the first draft concluded.
- The FOH-local stream's seed is a new invented value; record it at the
  implementation site.
- The draw EXPRESSION is still carried verbatim, quirk included:
  `Math.round((randomTags.length - 1) * Math.random())` with 34 tags is
  `js_round(33 · r)`, yielding 0..33 with both endpoints at half weight. Use
  `js_round` (ECMAScript ties-toward-+Inf), never `floor(x+0.5)`. Faithfulness
  survives in the expression even where it cannot survive in the plumbing.

### 2.10 Ready to fight, and launch

`readyToFight` is computed **in the draw function**, `css.js:1167-1181`:

```js
var readyPlayers = 0;
for (var k = 0; k < 4; k++) {
  if (playerType[k] > -1) {
    readyPlayers++;
    if (readyPlayers >= 2) { readyToFight = true; } else { readyToFight = false; }
    if (occupiedToken[k]) { readyToFight = false; break; }
  }
}
```

Normative rule: **READY TO FIGHT appears iff at least two ports are not
N/A, and no participating port's token is currently held.** Picking up any
token immediately un-readies the screen. Because it lives in the draw pass
it lags the control pass by one frame — reproduce that ordering.

Launch, `css.js:446-460`:

```js
if (readyToFight && choosingTag == -1) {
  if (pause[i][0] && !pause[i][1]) {          // START, rising edge
    sounds.menuForward.play();
    changeGamemode(6);                        // → stage select
    syncGameMode(6);
  }
} else if (choosingTag == -1 && input[i][0].du && !input[i][1].du) {
  sounds.menuForward.play();
  changeGamemode(6);
  syncGameMode(6);
} else if (choosingTag == -1 && input[i][0].dr && !input[i][1].dr) {
  chosenChar[i] = 3; changeCharacter(i, 3); sounds.menuSelect.play();
}
```

**Quirk Q3 (the pre-registered `readyToFight` START-vs-d-pad-up branch).**
The three arms are `else if`-chained on `readyToFight`:

- When **ready**: START launches; d-pad UP and d-pad RIGHT are dead.
- When **not ready**: START does nothing, but **d-pad UP launches anyway**,
  bypassing the ready check entirely — you can start a 1-participant match.
  And **d-pad RIGHT force-selects Falco.**

Both d-pad arms are removed by DEVIATION D2. The consequence is that the
not-ready state becomes inert, which is the sane reading of upstream's
intent (the arms are plainly developer shortcuts, and the banner explicitly
says `"PRESS START"`, `css.js:1247`).

Banner: red ribbon, `"READY"` / `"TO"` / `"FIGHT"` in flashing
`hsl(52, 85%, rtfFlash%)` pulsing 25→50 at `0.5`/frame (`css.js:1228-1242`),
with `"PRESS START"` beneath (`css.js:1247`).

### 2.11 B-hold exit

`css.js:186-194`:

```js
if (input[i][0].b) {
  bHold[i]++;
  if (bHold[i] == 30) { sounds.menuBack.play(); changeGamemode(1); }
} else { bHold[i] = 0; }
```

**Hold B for 30 frames (0.5 s) → main menu.** Exact equality on 30, so it
fires once. Note the deliberate overlap with §2.4(a): a B press inside the
roster band grabs your token on frame 1 *and* starts the exit counter; if
you keep holding, you exit at frame 30 while still nominally carrying. **That
last clause is the whole of A43, and it sat here unread:** nothing upstream
releases the grab on the way out (`changeGamemode` case 2 is `drawCSSInit()`
alone, `main.js:571`), so the token is still on the hand at re-entry and the
very next hand movement re-selects through §2.4's live hover — which, with
D22's wedge sitting above roster cell 4, means falcon. **DEVIATION D35**
releases it and re-homes both tokens on their chosen cells, so "nominally" is
no longer nominal here. The
counter is per-port and resets on release. The CSS also has the BACK
chevron (§2.9) as an A-clickable exit to the same place. A max-of-all-ports
`bHold` drives an on-screen exit indicator (`css.js:736-740`).

### 2.12 CSS gap table

| # | Behaviour | Spec | Our port AT ORIGINAL AUDIT | Gap |
|---|---|---|---|---|
| 1 | Cursor model | Free 2D hand, 12 px/frame, screen-clamped (`css.js:195-206`) | Discrete 4-row list cursor (`foh.c:270-277`); hand sprite is decoration slaved to the row | **DIFFERS** |
| 2 | Grab own token with B | B edge in roster band (`css.js:209-215`) | No grab gesture at all (`foh.c:74-77` registers its absence) | **MISSING** |
| 3 | Grab a token with A | A on token, own or any CPU's (`css.js:298-312`) | — | **MISSING** |
| 4 | Grab a **CPU** port's token | `playerType[j] == 1` arm (`css.js:300`) | — | **MISSING** |
| 5 | Carry + live hover-select | Token follows hand, char changes on cell entry (`css.js:216-296`) | — | **MISSING** |
| 6 | Drop with A + announcer sound | `css.js:227-233` etc. | — | **MISSING** |
| 7 | Leave-band silent commit | `css.js:336-347` | — | **MISSING** |
| 8 | Select your own character | Only via the token gesture | Left/Right stepper on rows 0/1, clamped 0..4 (`foh.c:278-287`) | **DIFFERS** (owner: "no way to select own character" — our stepper exists but is not the upstream gesture and does not read as one) |
| 9 | Port type is a clickable widget | Cursor + A on the type box (`css.js:348-357`) | A on row 2 toggles P2 only (`foh.c:304-309`) | **DIFFERS** |
| 10 | Type domain | `-1/0/1/2` = N/A/HMN/CPU/NET (`css.js:860-877`) | `{0,1}` only, no N/A, no NET | **DIFFERS** |
| 11 | **P1 may be CPU** | Yes (`main.js:510-526`, no guard for port 0) | No `p1Type` field exists | **MISSING** |
| 12 | CPU-level widget | Grabbable knob, domain 1..4, default 3 (`css.js:316-334`) | Left/Right stepper, domain 1..4, default 3 (`foh.c:294-302`) | **DIFFERS** (domain **MATCHES**) |
| 13 | READY TO FIGHT rule | ≥2 non-N/A ports AND no token held (`css.js:1167-1181`) | Banner drawn unconditionally (`foh_render.c:1526-1546`) | **DIFFERS** |
| 14 | START launches | Only when ready (`css.js:446-451`) | Always (`foh.c:260-264`) | **DIFFERS** |
| 15 | B-hold 30 → main menu | `css.js:186-194` | Present, 30 frames, → menu-battle (`foh.c:244-256`) | **MATCHES** (destination differs: upstream goes to gameMode 1 = main menu, we go to menu-battle) → **DIFFERS** on destination |
| 16 | BACK chevron clickable | `css.js:358-363` | Hit-tested at `FOH_CSS_BACK_X0` (`foh.c`'s counter arm), but HELD not clicked — **DEVIATION D22**, A23 | **DIFFERS** on trigger (present, owner-ratified) |
| 16a | BACK hold bar | `css.js:735-746` | Present, upstream's shape and rate (`foh_render.c` `css_header`); A23 | **MATCHES** |
| 17 | Palette cycle X/Y, 0..6 | `css.js:366-389` | Not implemented | **MISSING** |
| 18 | Mode ribbon toggles `versusMode` | `css.js:389-394`, sim-visible | Present (A27): upstream's own trigger — an A rising edge inside the widget's rect calls `setVersusMode(1 - versusMode)` — hit-tested at `FOH_CSS_MODE_*`, which IS the plate's drawn extent (D4); the value reaches `G.sim.versusMode` immediately BEFORE `sim_setup_match` at both launch bridges, because `startGame` reads it (`main.js:1334`). Proven end-to-end by `port/foh/check-css-mode.sh` | **MATCHES** on trigger and on effect (label: D28) |
| 19 | Name tags | random / clear / type (`css.js:415-439`) | Not implemented | **MISSING** (type-tag cut by D8) |
| 18a | Mode ribbon LABEL | `versusMode ? "An endless KO fest!" : "4-man survival test!"` (`css.js:715-721`) | `ENDLESS`/`KO FEST!` on two rows, or `VS. MELEE` in stock mode (`foh_render.c` `css_header`) — **DEVIATION D28** | **DIFFERS** on wording/width (present) |
| 20 | d-pad UP launches unready | `css.js:452-455` | Not implemented | **MISSING — intentional (D2)** |
| 21 | d-pad RIGHT → Falco | `css.js:456-459` | Not implemented | **MISSING — intentional (D2)** |

---

## §3. Gameplay options (gameMode 11) — the complete row list

Owner noticed "Everyone Walljumps" is missing. It is not the only one.

### 3.1 The rows, in order, exactly

**The row list is NOT derived from `gameSettings`.** `Object.keys(gameSettings)`
appears only on the cookie paths (`gameplaymenu.js:15` load,
`gameplaymenu.js:29` save). The visible rows are hard-coded **three times**
and hand-synchronised: labels (`gameplaymenu.js:178-182`), A-actions
(`gameplaymenu.js:39-59`), value strings (`gameplaymenu.js:228-245`). Row
count comes from `menuVOptions = 4` (a **max index**, so 5 rows) and
`menuHOptions = [0,0,0,0,3]` (max column index per row)
(`gameplaymenu.js:11-12`).

| Row | Label (line) | Setting key | Domain | Default | Step on A | Cells |
|---|---|---|---|---|---|---|
| 0 | `"Turbo Mode"` (`:178`) | `turbo` | `0/1` | `0` | `turbo ^= true` (`:41`) | 1 |
| 1 | `"L-Cancel"` (`:179`) | `lCancelType` | `0/1/2` | `0` | `++`, wrap `>2 → 0` (`:44-47`) | 1 |
| 2 | `"Flash on L-Cancel"` (`:180`) | `flashOnLCancel` | `0/1` | `0` | `^= true` (`:50`) | 1 |
| 3 | **`"Everyone Walljumps"`** (`:181`) | `everyCharWallJump` | `0/1` | `0` | `^= true` (`:53`) | 1 |
| 4 | `"Tapjump off"` (`:182`) | `tapJumpOffp1..4` | `0/1` ×4 | `0` | `gameSettings["tapJumpOffp" + (menuIndex[1]+1)] ^= true` (`:56`) | **4** (one per port) |

Value strings rendered: `"On"/"Off"` for rows 0, 2, 3 (`:230`, `:236`,
`:239`); `"Normal"/"Auto"/"Smash 64"` for row 1 (`:233`); per-column
`"On"/"Off"` for row 4 (`:242`). Defaults are all `0` in
`settings.js:47-55`. `x ^= true` coerces and XORs with 1, so values stay
integer `0`/`1`.

Row 4 is the one place the port does not render what upstream renders:
`"Tapjump off": On` is a double negative, and on playthrough #3 the owner
read it as an L-cancel defaults bug (A32). **DEVIATION D23** relabels it
`TAP JUMP` and shows the ENABLED state. The `tapJumpOffp1..4` keys above are
untouched — the deviation is in the pixels only.

**AT AUDIT TIME we were missing rows 2 and 3, and our row order was
wrong**: the port rendered `TURBO`, `L-CANCEL`, `TAP JUMP OFF` — three of
five, with Tapjump promoted from index 4 to index 2. **CLOSED 2026-07-29**:
all five upstream rows are present in upstream's order with upstream's own
value strings, and Tapjump is back at index 4 with its four columns
(`foh_render.c`; §3.1's gap table).

### 3.2 Settings with no UI row (persisted, never shown)

`gameSettings` (`settings.js:47-58`) has 11 keys; only 8 are reachable
through the 5 rows. The other three are persisted by the save loop
(`gameplaymenu.js:29-31` writes **every** key) but have no widget — their
label entries are the empty string `""` in the CSS label tables
(`css.js:85`, `:87`, and `phantomThreshold` likewise), which is exactly how
upstream marks "not displayable":

| Key | Default | UI row? | Consumed by |
|---|---|---|---|
| `blastzoneWrapping` | `0` | no | **nothing** — zero readers in `src/` |
| `dustLessPerfectWavedash` | `0` | no | **nothing** — zero readers in `src/` |
| `phantomThreshold` | **`0.01`** | no | `hitDetection.js:335,337,348`; `physics.js:1039-1040` |

`phantomThreshold` is the one that matters: it is **on the checksum
surface**, its default is `0.01` (not `0`), and the pre-existing qjs gotcha
(`CLAUDE.md`, M0 task 6) is precisely that a missing storage plane
`Number("")`-zeroes it and silently flips physics. Our persistence layer
must round-trip it as a double at `0.01` and never expose it.

### 3.3 What each setting actually does

Measured consumers, so the implementation knows which plane each row
touches:

| Setting | Plane | Sites |
|---|---|---|
| `turbo` | **sim** (checksum) | `physics.js:595`; `ai.js:492,494,591,593,784,786` |
| `lCancelType` | **sim** (checksum) | `physics.js:673,696,705` |
| `flashOnLCancel` | **render only** | `render.js:125` — a visual flash on `LANDINGATT*` |
| **`everyCharWallJump`** | **NONE — dead setting** | zero MECHANICS/GAMEPLAY consumers. Two DISPLAY-ONLY readers exist and are not a mechanic: `gameplaymenu.js:239` renders its own row's On/Off, and the `inServerMode` block at `css.js:1183-1191` walks `Object.keys(gameSettings)` and prints any key with a non-empty label. Nothing in the physics/sim plane reads it. |
| `tapJumpOffp1..4` | **sim** (checksum) | `actionStateShortcuts.js:507,516,520`; `fox/moves/DOWNSPECIALAIR.js:138` |
| `phantomThreshold` | **sim** (checksum) | `hitDetection.js:335,337,348`; `physics.js:1039-1040` |

**"Everyone Walljumps" is a dead toggle upstream.** It is written by the
menu and displayed, and NO MECHANIC ever reads it (the only readers are
display-only: `gameplaymenu.js:239` draws its own row, and
`css.js:1183-1191` prints it in `inServerMode`). Implementing it faithfully
means: add the row, persist the 0/1, and wire it to nothing. That is the
correct and cheapest outcome — the owner noticed its absence because it is
*visible*, not because it changes play. Do not invent a walljump rule to
give it meaning; that would be a faithfulness violation.

**DEVIATION D20 — the owner asked for it anyway, knowingly (2026-08-04).**
Verbatim: *"i want real everyone-walljumps. deliberate deviation."* Presented
with the measurement above — that the toggle is dead upstream and that giving
it meaning is a departure, not a fix — the owner chose the departure. **This is
a HOUSE RULE, and the paragraph above stays exactly as written**: it remains
the correct description of UPSTREAM, and of what a faithfulness argument
concludes on its own. D20 is what overrides it, on the owner's authority, and
nothing else in this port may cite "everyone walljumps" as precedent for
inventing mechanics.

**DEVIATION D47 — and D20 meant "everyone except puff" for twenty days
(2026-08-24).** D20 guarded itself on DATA rather than on a character list,
which was right, but the guard then EXCLUDED puff and nothing said so out
loud: all five characters carry `framesData` `WALLJUMP: 40`, yet only marth,
fox, falco and falcon carry the ECB, and puff carries neither the ECB nor a
WALLJUMP animation. Upstream never authored boxes for a state puff cannot
enter, so without the guard D20 died `ecb: unknown action state` one frame
after puff's walljump fired. The owner ratified **option 1** — reuse an
existing puff ECB rather than author new geometry, because authoring 40 frames
of invented collision data would have been this port's first unverifiable
engine number. D47 reuses **WALLTECHJUMP**, on the measurement that for all
FOUR characters that have both, the WALLTECHJUMP ECB is byte-identical to the
WALLJUMP ECB. The owner's own suggestion, FALL, is rejected: 8 ECB frames
against a frame index clamped to 40. Full statement in the deviation table.

**The lesson is about the guard, not the walljump.** A data guard that
silently narrows a house rule reads exactly like a data guard that correctly
scopes one, and only a POSITIVE witness can tell them apart. D20 shipped with
a regression proof and a crash proof and no positive; it took twenty days and
`port/sim/check-walljump-d47.sh` for either the puff path OR the marth path to
execute even once.

*The change, exactly.* One conjunct at the per-character ABILITY gate —
`port/sim/physics.c:368`, the mirror of `physics.js:132-134`:

```
    if (sign * in[0].lsX >= 0.7 && sign * in[3].lsX <= 0 &&
        (ATTR(S, i)->walljump || S->everyCharWallJump)) {
```

Placement is load-bearing and was corrected during implementation: it goes at
the per-character attribute (`charAttributes.walljump`), NOT at the
per-action-state `wallJumpAble` flag (`physics.c:777`). The setting means
"every CHARACTER can walljump", not "every STATE can be walljumped out of" —
putting it at the state flag would let characters walljump out of states
upstream never allows, which is a different and much larger rule.

*Safety, and why no golden moves.* `everyCharWallJump` **defaults false and
must stay false forever** (`settings.js:51` is `0`; `sim_boot.c` now writes the
default EXPLICITLY rather than inheriting a memset, because the flag has real
mechanical effect). Every frozen golden was recorded at the settings defaults,
so flag-off is bit-identical to the faithful port — VERIFIED: `bash
port/sim/check-sim.sh` -> `SIM CONFORMS`, all 8 goldens exact, after the change.

*WHO IT ACTUALLY AFFECTS — and the guard that had to be added.* "Everyone"
is aspirational; the data decides. Measured from the compiled CTAB1 table:

| char | `walljump` attr | ECB for WALLJUMP | framesData | D20 grants it? |
|---|---|---|---|---|
| marth | 0 | **yes** | yes | **YES** |
| puff | 0 | **NO** | yes | no — excluded |
| fox / falco / falcon | 1 | yes | yes | already had it |

Puff has no ECB data for WALLJUMP because upstream never authored boxes for a
state puff cannot enter. Granting the ability alone therefore drove puff into a
state with no collision data: **witnessed** — a one-token edit to g02 (frame
1887, puff's `lsX`) made `--walljump-all` die with `SIM FATAL frame 1888: ecb:
unknown action state`, stream truncated. Authoring the missing boxes would mean
inventing Nintendo-derived animation data, which this project does not do.

So the arm is **guarded on DATA, not on a character list**
(`has_ecb_state(S, i, "WALLJUMP")`): D20 grants walljump to any character whose
data can represent the state. Puff is excluded silently and correctly — with
the guard, that same trace runs to completion byte-identical to flag-off.
Evidence: `.loop/wj/D20-TOOTH.txt`.

*Reachability.* The FOH LAUNCH line has always carried `walljump=%d`
(`foh_dev.c:2245`, `foh_app.c:538`) while nothing consumed it; the three
`G.sim` apply sites now do (`foh_app.c:704`, `foh_dev.c` ×2, the target-live
one guarded by `tgtLive` exactly like its siblings so recorded target goldens
keep the settings they were recorded against). `sim_host --walljump-all`
exposes the same flag to the harness so the deviation is testable.

### 3.4 Navigation and persistence

`gameplayMenuControls(i, input)` (`gameplaymenu.js:23-164`), one `else if`
chain: **B → A → up → down → right → left → neutral**.

- **A is the only value-changing input.** Left/right move the *column*
  (only meaningful on row 4), they do not step values — unlike audiomenu.
  A plays `sounds.menuSelect` (`:38`), not `menuForward`.
- Up/down step `menuIndex[0]`, clamping the column against the new row
  first, then wrapping `<0 → 4` and `>4 → 0` (`:150-163`). Columns wrap
  against the post-wrap row.
- **Repeat cadence** (shared with every list menu): first frame past the
  `±0.7` threshold steps immediately, then one step per **10 frames**
  (`stickHold % 10 == 0`). The counter `stickHold` is a **single global**
  in `menu.js:65`, imported and mutated by gameplaymenu, audiomenu and
  keyboardmenu; it is reset only when *no* port holds a direction, and only
  by the last port (`i == ports - 1`, `:137`).
- **B exits** (`:25-36`): sets `input[i][1].b = true` to swallow the edge,
  writes **every** `gameSettings` key via `setCookie(key, value, 36500)`,
  then `changeGamemode(1)` back to the main menu with `menuMode`/
  `menuSelected` untouched (so you land on Options → Gameplay).
  Save is gated on `meHost`; the non-host branch pops a blocking `alert()`
  (`:32`) — netplay-only, out of scope.

**Quirk Q4:** the diagonal-rejection guards are malformed in all four
directional branches (`:60`, `:79`, `:98`, `:117`):
`!(Math.abs(input[i][0].lsX >= 0.7))` applies `Math.abs` to the *comparison
result*, so it reduces to `lsX < 0.7`. Up-left and down-left are accepted;
up-right and down-right are rejected. **Do not reproduce** — with a d-pad
the diagonal is a deliberate two-button press and this asymmetry is plainly
a typo, not a design. Marked **DEVIATION D9**: treat diagonals uniformly
(vertical wins, matching audiomenu's `lsY`-first ordering).

**Quirk Q5:** the left branch omits `stickHoldEach[i] = true` (`:118` vs
`:99`), so holding left resets the global counter every frame and repeats
at **60 Hz** while right repeats at 6 Hz. **Do not reproduce** — same
DEVIATION D9. Both quirks are unobservable in the checksum plane (menus are
not on the oracle surface) and reproducing them is user-hostile.

### 3.5 Gameplay-options gap table

| # | Behaviour | Our port today | Gap |
|---|---|---|---|
| 1 | Row 0 `Turbo Mode`, 0/1 | `TURBO`, 0/1 (`foh_render.c:1688-1708`) | **MATCHES** |
| 2 | Row 1 `L-Cancel`, Normal/Auto/Smash 64 | `L-CANCEL`, `NORMAL/AUTO/SMASH64` | **MATCHES** |
| 3 | Row 2 `Flash on L-Cancel`, 0/1 | present, and wired: `foh_dev.c` writes it over the GFXDATA1 copy at match boot on both live paths | **CLOSED 2026-07-29** |
| 4 | Row 3 **`Everyone Walljumps`**, 0/1 | present, faithfully DEAD (row + persisted bit, zero MECHANICS consumers; only display-only readers exist) | **CLOSED 2026-07-29** |
| 5 | Row 4 `Tapjump off`, 4 columns | present at index 4, columns WRAP as upstream's do | **CLOSED 2026-07-29** |
| 6 | A changes value; L/R move column only | matches | **MATCHES** |
| 7 | Up/down wrap | wraps -1->4 / 5->0 with upstream's pre-wrap column clamp | **CLOSED 2026-07-29** |
| 7b | Held direction repeats 1-then-every-10 | single step per rising edge | **DIFFERS — DEVIATION D16** (whole-FOH input model, §5.5 row 6) |
| 8 | `phantomThreshold` persisted at `0.01`, hidden | in `FohPersist` v4 as a hex16 double, pushed to `G.sim` at launch | **CLOSED 2026-07-29** |
| 9 | `blastzoneWrapping`, `dustLessPerfectWavedash` persisted at `0`, hidden | in `FohPersist` v4, no widget | **CLOSED 2026-07-29** |
| 10 | B saves all keys then → main menu | B → menu-options (`foh_app.c:559-563`) | **MATCHES** |

---

## §4. Audio options (gameMode 10)

**HOST-PROVEN, DEVICE-PENDING 2026-07-29 (menu-fidelity arc).** NOT "closed":
the mixer side is complete and proven host-side, and the SHIPPING TARGET still
has no caller, so on the device the sliders remain inaudible until the
`foh_dev.c` patch in the work order lands. Stated this way because the earlier
"CLOSED / THE SLIDERS ARE AUDIBLE" heading contradicted its own pending note
three paragraphs later (review-r13 MAJOR) — a screen is not closed because its
host half is.
> **THE SLIDERS ARE AUDIBLE ON THE HOST PATH; THE DEVICE CALL SITE IS PENDING.** Owner ruling 2026-07-29 ("yep wire") retired
> the earlier hand-off: file-partitioning is gone (PROCESS §12), so the
> engine push landed with the screen instead of behind another lane.
> The wire, end to end: `port/foh/foh.c` edits/clamps the two doubles →
> `port/foh/foh_persist.c` saves/loads them → the app pushes them under the
> audio lock (at boot from the persisted plane, and on every change) →
> `snd_bus_set` in `port/gfx/snd_mixer.h` converts each level to a Q12 bus
> gain applied at the two accumulate sites (sfx voices, music channel).
>
> **ONE LINK IS A PENDING PATCH, NOT LANDED CODE — read this before
> believing the device is done.** The app-side push (`foh_audio_bus_push` in
> `port/foh/foh_dev.c`) is written and was green in this lane, but
> `foh_dev.c` is SINGLE-WRITER this cycle (the match-exit closure lane holds
> it), so the menus lane reverted its copy rather than be a second writer on
> a file that feeds a pinned gate. The exact patch is handed over as prose in
> `.loop/menus-p2-device-workorder-audio.md` §8 and applies after that lane
> merges. Until it does, the levels are edited, persisted and correctly
> converted, and the DEVICE build has no caller — the host proof below stands,
> the device behaviour does not yet.
>
> **THE ONE SUBTLETY, and it is the whole correctness argument:** SND1's
> packed `gainQ8` values ALREADY carry the 0.5/0.3 master defaults, because
> `sfx.js:623-624` runs `changeVolume(sounds, 0.5)` /
> `changeVolume(MusicManager, 0.3)` at load and the pipeline records the
> POST-changeVolume `_volume` as the mixer's gain source
> (`pipeline/lib/sounds-schema.js:12-15`). So the bus is the RATIO
> `masterVolume / default`, never the raw level — pushing the level would
> apply the default twice (0.5 x 0.5 at rest). At the defaults the ratio is
> exactly 1.0, so the fill is BYTE-IDENTICAL to the pre-wire mixer and the
> frozen `check-mixer-fidelity.sh` / `check-music-fidelity.sh` streams are
> untouched. Above-unity gain (up to 2.0 sfx, 3.33 music) is faithful:
> upstream at `masterVolume` 1.0 really is twice the default loudness.
>
> **PRECISION — measured, with the residual REGISTERED (review-r10).** The
> bus is Q12 with round-half-up, applied PER VOICE. An earlier Q8 bus applied
> to the summed accumulator was measurably worse and its code comment claimed
> otherwise; both the code and the claim were corrected. Fixed: a no-overwrite
> sample of 1000 at level 0.1 emitted 99 where exact linear is 100 (chained
> truncation), and sum-level scaling diverged from per-voice by up to seven
> output LSBs on polyphonic frames. **Remaining, and PRE-EXISTING:** SNDPACK1
> stores each gain as Q8, so the authored product `default x overwrite` is
> already quantized in the pack before any bus touches it. Worst case is the
> smallest overwrite (`dash`, 0.3): the packed gain `round(0.5*0.3*256) = 38`
> encodes 0.1484 rather than 0.15, so at level 1.0 a full-scale sample emits
> 9728 where exact linear is 9830.1 — **1.04% of amplitude (~0.09 dB) at the
> loudest rail end** (re-measured after `snd_gain_apply`; the earlier 9726 was
> the pre-combined-multiply figure, review-r13). To be precise about WHY the frozen gates never saw it
> (review-r11 MINOR corrected an earlier "invisible at the default level"):
> the error is ALREADY THERE at the default — full-scale `dash` emits 4863
> where the authored value is 4915, the same 1.06% — it is simply UNCHANGED by
> this wire, so the old-C-versus-new-C default-level comparison the gates
> perform cannot reveal it. Removing it means widening SNDPACK1's gain field, which
> re-freezes a PINNED producer (the SNDPACK1 sha256, both fidelity gates and
> the device pin) — out of this lane's scope, reported to the driver as
> follow-up rather than silently accepted.
>
> **PROOF, and its honest boundary.** Host-side: `check-foh-flows.sh` leg
> [5c] (`port/foh/foh_snd_witness.c`) drives the REAL `snd_mix_fill` and
> asserts the levels reach the output samples — byte-identity at the
> defaults against an independent reference formula, exact silence at 0.0,
> authored full scale at 1.0, strict monotonicity across the rail, and the
> `snd_bus_q12` ratio against hand-computed pins, and that the SFX bus is
> SNAPSHOTTED per voice at play time while music applies live (howler's own
> split — see the AUDIO BUS note); eight teeth (T27-T34) prove it bites,
> including the literal "wired but inert" regression. NOT claimed
> host-side: real SDL audio through a real speaker on the device. That is
> the work order `.loop/menus-p2-device-workorder-audio.md` (PROCESS
> §12.3), to be drained by the device lane.

Was entirely missing — row 0
emitted `deny` + `refused audio`. Now `FOH_OPT_AUDIO`: two rows, +/-0.1
unrounded steps, [0,1] clamp, defaults 0.5/0.3, no A handler, B saves both
levels through the persist chokepoint, and the engine push described above.
The wedges + growing knob are carried.

**State:** `masterVolume = [0.5, 0.3]` (`audiomenu.js:13`) — `[0]` sounds,
`[1]` music; `audioMenuSelected = 0` (`:15`). Two rows only, labels
hard-coded `"Sounds"` (225,275) and `"Music"` (225,525) at
`audiomenu.js:140-141` (the `audioMenuNames` array at `:14` is dead, never
referenced).

**Input** — `audioMenuControls(i, input)` (`:16-121`), chain
**B → up → down → right → left → neutral**. **There is no A handler at
all** — A does nothing on this screen.

| Input | Effect | Line |
|---|---|---|
| Up / Down | move `audioMenuSelected`, wrap `-1 → 1`, `2 → 0` | `:27-54`, `:96-99` |
| Right | `masterVolume[sel] += 0.1`, clamp `≤ 1` | `:55-66`, `:100-105` |
| Left | `masterVolume[sel] -= 0.1`, clamp `≥ 0` | `:67-78`, `:106-111` |
| B | `setCookie("soundsLevel"/"musicLevel", …, 36500)`, `changeGamemode(1)` | `:20-26` |

Normative details: step is a fixed **±0.1** with **no rounding**, so
repeated steps accumulate float dust (`0.7999999999999999`) and that raw
double is what gets persisted (`:24-25`). `menuSelect` plays on every
accepted step including clamped no-ops. Volume is pushed to the engine only
on change (`:114-120`), via the global `changeVolume` installed by
`sfx.js:609`. Because `lsY` is tested before `lsX`, a diagonal moves the
cursor and never adjusts volume. Unlike gameplaymenu there is **no `meHost`
gate** — audio always saves.

Load: `getAudioCookies()` (`:212-223`) reads both keys, guards
`!= null && != undefined && != "null"`, `Number()`-converts, applies each.

Rendering is two slider wedges with a knob whose radius grows with volume
(`:168-207`) — our own 240×240 design substitutes here per D4, but the
**two rows, ±0.1 step, [0,1] clamp, defaults 0.5/0.3** are normative.

**DEVIATION D14 — numeric readout on the audio screen.** Upstream renders NO
number on this screen: the wedge's fill and the knob's radius ARE the
readout. The port adds a `0.0`-`1.0` tenths readout beside each knob,
because at 240 px the wedge is 160 px of travel for ten steps and the
difference between 0.6 and 0.7 is 16 px of a thin triangle. Same class as
D3/D4 — a 240x240 legibility adaptation of an upstream widget, not a new
widget.

> **RATIFIED BY THE OWNER 2026-07-29.** Presented as a choice (keep the
> digits, or strip them for strict fidelity) with the recommendation to
> keep; the owner accepted the recommendation. Rationale of record: a
> 240x240 wedge is 160 px of travel for ten steps, so the difference
> between 0.6 and 0.7 is 16 px of a thin triangle and is not readable
> precisely — the digits are what make the value legible at this size.
> This is therefore an OWNER-SANCTIONED DEVIATION, not an unremarked
> addition: upstream renders wedges and a growing knob and NO number
> (`audiomenu.js:168-207`), and we render both the wedge/knob AND a
> `0.0`-`1.0` tenths readout. Reverting is still one block in
> `render_opt_audio` (`foh_render.c`, the `lvl[]` snprintf) and nothing
> else depends on it.

> `ponytail:` this is the cheapest whole screen in the spec — two doubles,
> four inputs, one persist pair. It should not be deferred behind anything.

---

## §5. Main menu and its three submenus (gameMode 1)

One file, one state machine: `menu.js`, handler `menuMove(i, input)`
(`menu.js:66-255`), called per port per frame from `main.js:927`.

### 5.1 State

`menuSelected` (`:17`) row index, shared across pages, explicitly reset on
each page change; `menuMode` (`:39`) which page; `menuCount = [4,4,4,2]`
(`:31`); `menuTitle = ["Main Menu","Options","Battle Mode","Controls"]`
(`:32`). Page constants `TOPLEVEL=0, SECONDLEVELOPTIONS=1, MPMENU=2,
CONTROLLERCALIB=3` (`:44-47`).

### 5.2 The tree, verbatim (`menu.js:19-30`)

**Page 0 — "Main Menu"**

| Row | Label | Blurb | Action |
|---|---|---|---|
| 0 | `VS. Melee` | `Multiplayer Battles!` | `menuMode = MPMENU`, `menuSelected = LOCALVS` (`:74-75`) — a *page*, not a launch; and it does **not** set `menuMove`, so no `menuSelect` sound |
| 1 | `Target Test` | `Smash ten targets!` | `setTargetPlayer(i)`, `setTargetPointerPos([178.5,137])`, stop music, `playTargetTestLoop()`, `changeGamemode(7)` (`:78-84`) |
| 2 | `Target Builder` | `Build target test stages!` | `setEditingStage(-1)`, `setTargetBuilder(i)`, `changeGamemode(4)` (`:87-90`) |
| 3 | `Options` | `Game setup.` | `menuMode = SECONDLEVELOPTIONS`, `menuSelected = AUDIOOPTIONS` (`:95-97`) |

**Page 1 — "Options"**

| Row | Label | Blurb | Action |
|---|---|---|---|
| 0 | `Audio` | `Select audio levels.` | `changeGamemode(10)` (`:130`) |
| 1 | `Gameplay` | `Change gameplay settings.` | `changeGamemode(11)` (`:135`) |
| 2 | `Keyboard Controls` | `Customize & calibrate controls.` | **not the rebinder** — `menuMode = CONTROLLERCALIB`, `menuSelected = 0` (`:139-141`) |
| 3 | `Credits` | `Who did this?` | `setCreditsPlayer(i)`, `changeGamemode(13)` (`:146-147`) |

**Page 2 — "Battle Mode"**

| Row | Label | Blurb | Action |
|---|---|---|---|
| 0 | `Local VS` | `One box this screen.` | `changeGamemode(2)`, `positionPlayersInCSS()` (`:105-106`) |
| 1 | `Spectate` | `Ranked Mode` | `connectAsSpectator()`, then CSS (`:109-111`) |
| 2 | `P2P` | `Hostless Muliplayer` (sic) | **dead row** — body entirely commented out (`:114-116`); A still plays `menuForward` and nothing happens |
| 3 | `Server` | `Hosted Multiplayer` | `connectToMPServer()`, then CSS (`:119-121`) |

**Page 3 — "Controls"** (2 rows)

| Row | Label | Blurb | Action |
|---|---|---|---|
| 0 | `Controller` | `Customize & calibrate controller.` | `setCalibrationPlayer(i)`, `changeGamemode(14)`, `runCalibration(i)` (`:155-157`) |
| 1 | `Keyboard` | `Customize keyboard controls.` | `changeGamemode(12)`, `setKeyBinding(false)` (`:159-161`) — reached by `else`, so *any* non-zero index lands here |

Note the topology the owner half-remembered: there is **no top-level
Controls item**. The path is `Main Menu → Options → Keyboard Controls →
{Controller, Keyboard}`.

### 5.3 Input

`else if` chain, one branch per port per frame, priority
**A → B → up → down → neutral** (`:69`, `:164`, `:192`, `:205`, `:218`).

- A plays `sounds.menuForward` **unconditionally at the top** (`:70`),
  before any dispatch — so even the dead P2P row clicks.
- **Left/right are never read.** `lsX` does not appear in `menu.js`.
  START, triggers, C-stick, mouse and DOM key events: also absent.
- Up/down: same immediate-then-every-10-frames cadence as §3.4.
- Wrap on exact equality: `-1 → count-1`, `count → 0` (`:237-242`).
- Every accepted move plays `sounds.menuSelect` (`:236`) — so A-into-Options
  and every B transition play **two** sounds.

**B** (`:164-191`), rising edge, no hold:

| From | To | Cursor lands on |
|---|---|---|
| `CONTROLLERCALIB` | `SECONDLEVELOPTIONS` | `AUDIOOPTIONS` (0) — **not** the row you came from |
| `SECONDLEVELOPTIONS` | `TOPLEVEL` | `OPTIONS` (3) |
| `MPMENU` | `TOPLEVEL` | `VSMODE` (0) |
| `TOPLEVEL` | — | **no branch matches; B does nothing and plays no sound. There is no route back to the title screen.** |

`menu.js:181-190` are two identical unreachable duplicate branches (dead,
`MPMENU === 2` is already caught at `:176`).

### 5.4 Gating

**There is none.** No `inServerMode`, no netplay flag, no debug conditional
anywhere in `menu.js` (the only match for `spectat|netplay|debug` is the
import at `:11`). `menuCount[MPMENU] = 4` unconditionally, so Spectate,
P2P and Server always render and always highlight. P2P is disabled *by
commenting out its body*, not by a guard.

### 5.5 Main-menu gap table

| # | Behaviour | Our port today | Gap |
|---|---|---|---|
| 1 | 4 pages, exact labels/blurbs/counts | Present, exact strings incl. the `HOSTLESS MULIPLAYER` typo (`foh_render.c:33-39,69-79`) | **MATCHES** |
| 2 | Up/down wrap, `menuSelect` on move | `foh.c:230-238` | **MATCHES** |
| 3 | A dispatch table | Present; unimplemented targets emit `deny` + `refused` | **DIFFERS** (honest refusals — acceptable, see §11) |
| 4 | B back-targets and cursor landings | `foh.c:210-228`: controls→options(0), options→top(3), battle→top(0), top→nothing | **MATCHES** |
| 5 | Left/right, START inert on menus | inert | **MATCHES** |
| 6 | Repeat cadence 1-then-every-10 | not implemented (single-step per rising edge) | **DIFFERS — DEVIATION D16**, and it applies to EVERY front-of-house screen, including the gameplay (§3) and audio (§4) menus |
| 7 | `menuForward` on every A incl. dead rows | we play `deny` on refused rows | **DIFFERS** (deliberate: `deny` is more honest than a forward-click that does nothing) |

---

## §6. SSS — stage select (gameMode 6)

**The SSS is a free-pointer screen too**, the same paradigm as the CSS.
Our port built it as a discrete 3×2 grid. `stageselect.js`, handler
`sssControls(i, input)` (`:57-89`), called for every connected port from
`main.js:972-979` — **all players share one cursor and any player may
confirm.**

### 6.1 State

| Variable | Line | Initial | Note |
|---|---|---|---|
| `stageSelected` | `:50` | `6` | = `smallBoxStageNames.length`, i.e. **RANDOM**; persists across visits |
| `stagePointerPos` | `:53` | `[600, 635]` | dead centre of the RANDOM box; persists across visits, does **not** re-centre on entry |
| `stageSelectTimer` | `:51` | `0` | blink; incremented in *both* `drawSSSInit` (`:104`) and `drawSSS` (`:166`), so it double-ticks on the entry frame |
| `xRowOffset` | `:54` | `175` | cell pitch |

### 6.2 Layout — 6 + 1 cells, no grid

Six stage cells in **one horizontal row**, `fillRect(87.5 + i*175, 450, 150, 90)`
(`:62`, `:106`, `:177`), plus one RANDOM cell below at
`fillRect(525, 590, 150, 90)` (`:70-71`, `:189`). No 2D grid, no scrolling,
no disabled or hidden cells.

| id | short name (`:13-20`) | long name (`:22-29`) |
|---|---|---|
| 0 | `BATTLEFIELD` | `Battlefield` |
| 1 | `Y-STORY` | `Yoshi's Story` |
| 2 | `P-STADIUM` | `Pokemon Stadium` |
| 3 | `DREAMLAND` | `Dreamland` |
| 4 | `F-DEST` | `Final Destination` |
| 5 | `FOUNTAIN` | `Fountain Of Dreams` |
| 6 | `RANDOM` | — |

These ids are the oracle's `--stage` ids.

### 6.3 Cursor

```js
stagePointerPos[0] += input[i][0].lsX * 15;
stagePointerPos[1] += input[i][0].lsY * -15;
```

(`stageselect.js:58-59`) — 15 px/frame, **and no clamping at all.**
Rendered as a hollow white ring of radius 40 (`:203-207`).

**DEVIATION D10 — clamp the SSS cursor to the screen.** Upstream's
unclamped pointer is recoverable with an analog stick (you feel the
direction and the snap-back centres nothing, but a stick lets you sweep
back fast). With a d-pad at a fixed rate, driving off-canvas is a soft-lock
that looks like a frozen menu. Clamp exactly as the CSS already does
(`css.js:197-206`). This is the only behavioural change; hit tests are
untouched.

### 6.4 Hover and confirm

Hover is a plain point-in-rect test on the cursor centre, `else if`-chained
stage-row-then-RANDOM (`:60-76`). Three normative consequences:

1. **Selection is sticky** — moving into empty space does not clear
   `stageSelected`; the last hovered value stands. The 25-px gutters
   between cells therefore leave the selection unchanged.
2. `sounds.menuSelect` fires **only on change** (`:64-66`, `:72-74`).
3. The blink: selected cell alternates outline colour on
   `stageSelectTimer % 8 > 4` (`:179-186`).

**B** (`:77-79`): `menuBack`, `changeGamemode(2)` → **back to the CSS**. No
guard, no confirmation.

**A** (`:80-88`):

```js
sounds.menuForward.play();
if (stageSelected == smallBoxStageNames.length) {
  stageSelected = Math.floor(Math.random() * (smallBoxStageNames.length - 0.01));
}
setStageSelect(stageSelected);
syncStartGame(stageSelected);
startGame();
```

Order matters: **RANDOM is resolved into `stageSelected` before use**, so
after a random pick the module state holds the concrete stage and the next
SSS entry hovers *that*, not RANDOM. There is no guard against A before
anything was hovered — the default `stageSelected = 6` just means RANDOM.

**No d-pad handling** (`du/dd/dl/dr` never appear), **no START handling**
(`.s` never appears), no discrete stepping, no repeat. B and A share one
`if/else if`, so a same-frame A+B resolves as B.

### 6.5 RANDOM and the seeded stream

Our port currently refuses RANDOM (`foh.c:321-339`, registered exclusion in
`foh.h:69-76`) because upstream's `Math.random()` draw at
`stageselect.js:83` would consume from the seeded stream and desync a
live run against a replay.

**DEVIATION D11 — RANDOM resolves from a non-seeded source.** Keep the
widget and keep it selectable; draw the stage index from a source outside
the mulberry32 chain (e.g. the boot clock), *before* the match seeds. The
draw happens strictly before `startGame()` (`:82-84`), i.e. before any
match RNG exists, so this is sound: the seeded stream is untouched and
replay determinism is preserved by the trace recording the *resolved*
stage id, exactly as `setStageSelect` publishes it. Refusing a
first-class upstream widget is a worse outcome than sourcing one integer
off-chain.

### 6.6 SSS gap table

| # | Behaviour | Our port today | Gap |
|---|---|---|---|
| 1 | Free pointer, 15 px/frame | Discrete 3×2 grid + slot 6 (`foh.c:340-363`) | **DIFFERS** |
| 2 | 6 stages in one row + RANDOM below | 3×2 grid + RANDOM (design choice, D4) | **MATCHES** (semantically: 7 cells, same ids) |
| 3 | Hover selects live, sticky, sound on change | Cursor move selects; clamped, no sticky-gutter concept | **DIFFERS** |
| 4 | Cursor unclamped | clamped | **DIFFERS — intentional (D10)** |
| 5 | Cursor + selection persist across visits | reset on entry | **DIFFERS** |
| 6 | A confirms, resolves RANDOM, launches | A launches; RANDOM refused | **DIFFERS** (D11 closes it) |
| 7 | B → CSS | `foh.c:316-320` → CSS | **MATCHES** |
| 8 | START inert | inert | **MATCHES** |

---

## §7. Target select (gameMode 7)

Lives in `src/stages/targetselect.js`, dispatched at `main.js:592-594`
(init) and `main.js:980-983` (per-frame). **`stageselect.js` contains no
target-mode branch whatsoever** — the two screens are unrelated files.

Our port implements a 2×5 grid + an `ADD CODE` slot with an L/R character
selector and a personal-best line (`foh.c:368-434`,
`foh_render.c:1722-1777`). That is a re-authored screen, not a measured
port. The evidence boundary is now split (§7.1): the RENDER half of
`targetselect.js` WAS measured this pass — `drawTSSInit` (`:183-242`, whole function) and the FIRST PART of `drawTSS`
(`:244`-~`:420` of a function that actually runs to **`:540`** — the next
export is `getTargetStageCookies` at `:542`) were read and
the port's look re-authored from them. The render TAIL (`:421-540`: custom-stage
controls, character presentation, pointer and prompt) was NOT read, and the
SEMANTICS (`tssControls`, `:43-173` — the whole function, not the `:57-146`
this spec used to cite) remain unmeasured, so §7
stays UNVERIFIED as a whole.

**Registered as an open item**, not as a gap: before target-select is
called faithful, `src/stages/targetselect.js` must get the same treatment
the screens above got. Until then our implementation stands as-is and is
marked **UNVERIFIED**.

### 7.1 The LOOK, measured (menu-fidelity arc, 2026-07-29)

The open item above is now **partly closed**: `drawTSSInit` (`:183-242`, whole function) and the FIRST PART of `drawTSS`
(`:244`-~`:420` of a function that actually runs to **`:540`** — the next
export is `getTargetStageCookies` at `:542`) were read and
the port's look re-authored from them. The render tail `:421-540` was NOT
read and the SEMANTICS (`tssControls`, `:43-173`) remain unmeasured, so §7
stays
UNVERIFIED as a whole.

Measured, and it corrects a guess that had been circulating as "a brown and
orange target grid":

| Element | Upstream | Line |
|---|---|---|
| Backdrop | linear ramp `rgb(66,42,6)` -> `rgb(26,2,2)` (dark brown) | `:184-187` |
| Lattice | the same white 30 px grid every other menu draws | `:257-265` |
| Slot body | **black** `fillRect`, 250x50 at `(50 + floor(i/5)*260, 110 + (i%5)*60)` | `:196-198` |
| Slot label | `"Target " + (i+1)`, white 0.6 | `:200-203` |
| Slot border | `strokeRect`; idle `rgb(166,166,166)`, hovered FLASHES `rgb(251,116,155)` / `rgb(255,182,204)` on an 8-frame cycle | `:270-284` (the `strokeRect` itself is at `:284`) |
| Info panel | black 0.5 fill + white 0.5 stroke, `(200,450,800,200)` | `:206-208` |
| Character plate | rounded tile, ramp `rgb(41,47,68)` -> `rgb(85,95,128)`, chevron above and below | `:210-241` |
| Personal Best | label + `--:--:--` or `0M:SS.CC` | `:404-419` |

**The brown/orange is NOT the grid.** The grid is grey with a PINK hover
flash — the same flash the SSS uses. Brown/orange is the BRONZE MEDAL
gradient `rgb(180,123,65)` -> `rgb(236,179,120)` (`:325-327`), beside silver
`rgb(161,161,161)` -> `rgb(246,246,246)` and gold `rgb(255,221,42)` ->
`rgb(255,237,140)`.

**REGISTERED REWRITE DELTA (chevron axis).** Upstream stacks the character
plate's two chevrons VERTICALLY, above and below the tile (`:227-241`),
while its own control is the SHOULDER pair (`input.l` / `input.r`,
`:60-74`) — the arrows point along an axis nothing drives. The port draws
them LEFT and RIGHT, on the axis the control actually uses, and caps them
`L` and `R`. This is DEVIATION D4's rule applied to an arrow: a widget must
point at the gesture that works it.

**Medal discs, medal times and the Developer Record row are NOT portable
yet, and that is a DATA deferral, not a styling one.** They read
`medalTimes[][][]`, `devRecords[][]` and `medalsEarned[][][]` — authored
upstream data the M1 pipeline does not emit (measured: no
medalTimes/devRecords stage exists). HARD RULE 5 forbids retyping engine
data by hand, and drawing empty medal outlines would assert "not earned"
for a player who has earned them. The pipeline extension stays registered
(`port/foh/foh.h`, iter 99).

---

## §8. Credits (gameMode 13)

The owner reports "credits doesn't work". Measured at audit time: it was not
implemented at all (the Options CREDITS row emitted `deny` + `refused
credits`), and upstream's version has a real dependency problem on this
hardware.

**BUILT 2026-08-24 (punch-list A7).** The screen is a transliteration of
`credits.js` living in `foh.c`'s `step_credits` and `foh_render.c`'s
`render_credits`, with DEVIATION **D12** (the relative reticle, §8.3) and
DEVIATION **D38** (the FOH-LOCAL random stream, §12.1) as its only two
departures. Quirk Q6 is carried verbatim. The refusal token `credits` is
RETIRED from the judge the way `audio` and `keyboard` were — a refusal
registration promises that an affordance does nothing, and this one now
opens a screen. Proved by `port/foh/check-credits.sh` -> `CREDITS CHECK OK`.

### 8.1 What it is

A Star Fox-style shooting gallery: 14 names scroll up over a starfield and
you shoot them with a twin-laser reticle. Entered via `changeGamemode(13)`
(`menu.js:147`); per-frame logic `credits(creditsPlayer, input)`
(`main.js:944-946`) — **only one port is polled**, `creditsPlayer`,
default `0` (`main.js:65`).

Names (`credits.js:115-132`): Schmoo, Tatatat0, bites, shf, Nehgromancer,
BonesMalones, TJohnW, WwwWario, Mrjhrock2010, zircon, Buoy, Tom Mauritzon,
Rozen, Zack Parrish.

### 8.2 Input

| Input | Effect | Line |
|---|---|---|
| Left stick `rawX`/`rawY` | **absolute** reticle position | `:169-186` |
| A (edge) | fire, `shoot_cooldown = 8`, one-frame-deep buffer | `:188-203` |
| X / Y (edge) | cycle laser colour fwd/back (4 colours) | `:104-111` |
| START **or L or R**, level (held, not edge) | fast-forward: names `-3` px/frame instead of `-2`, reticle spins faster | `:146-153` |
| B (edge) | **exit** → `changeGamemode(1)` | `:238-246` |

**No d-pad handling.** No mouse. No keyboard.

Reticle positioning, verbatim (`:169-173`):

```js
cPlayerXPos = Math.round(((cBoundX / 2) + ((input[p][0].rawX) * (cBoundX / 2))) - ((cBoundX - cXSize) / 2));
cPlayerYPos = Math.round(((cBoundY / 2) + ((-1 * input[p][0].rawY) * (cBoundY / 2))) - ((cBoundY - cYSize) / 2));
```

then clamped to `[0,1200] × [0,750]` (`:175-186`). This is a **direct map
from stick deflection to screen position** — neutral stick parks the
reticle dead centre and you cannot leave it anywhere else.

### 8.3 Why it "doesn't work" and the fix

`rawX`/`rawY` come only from the analog stick
(`input/input.js:205-206` keyboard path, `:294-295` gamepad path). With a
d-pad supplying `±1`, the absolute map yields exactly **9 reachable reticle
positions** (the corners, edge midpoints and centre). The game runs, is
escapable with B, and is unplayable — it always ends on `sounds.failure`
(`:230`).

**DEVIATION D12 — drive the credits reticle as a relative cursor.**
Integrate `lsX/lsY * k` per frame and clamp, exactly like the CSS
(`css.js:195-206`), instead of upstream's absolute `rawX/rawY` map.
Justification: the absolute map is meaningless without an analog stick;
this is the minimum change that makes an upstream screen reachable rather
than nominally-present-but-dead, and it changes no other credits
semantics.

### 8.4 Exits and timing

Two exits, both to gameMode 1 (`:226-246`):

- **Timer**: `cScrollingPos` grows `+2`/frame from `0` to
  `cScrollingMax = 5000` → exactly **2500 frames (~41.7 s at 60 fps)**.
  Plays `sounds.complete` if all 14 names were shot, else
  `sounds.failure`.
- **B**: the only manual exit.

Both poke `input[p][1].b = true` (`:233`, `:241`) so the press cannot
re-trigger as a fresh edge on the main menu next frame. Both set
`initc = true`, which re-zeroes `cScore` and rebuilds `creditNames` on the
next entry (`:134`).

**Quirk Q6:** `cScrollingPos -= cScrollingSpeed` at `:144` is
**unconditional**, so the exit timer always runs at `+2`/frame regardless
of the START fast-forward — holding START desynchronises the names from
the end-of-credits trigger and they run off the top before the mode ends.
Carried verbatim.

### 8.5 Credits gap table

| # | Behaviour | Our port | Gap |
|---|---|---|---|
| 1 | Screen exists at all | `FOH_CREDITS`, `step_credits` / `render_credits` | **MATCHES** |
| 2 | 14 scrolling names, shootable | `foh_credits[]`, extracted from the clone and re-checked against it every run | **MATCHES** |
| 3 | Reticle | ring r 7 + 4 rotating spokes, red while hovering an unshot name | **DIFFERS** (registered: D12 drives it relatively) |
| 4 | A fires, 8-frame cooldown + 1-deep buffer | verbatim, incl. the twin bottom-corner bolts and the life-15 hit frame | **MATCHES** |
| 5 | X/Y laser colour | verbatim, 4 colours, wrapping both ways | **MATCHES** |
| 6 | START/L/R fast-forward | verbatim (`yDif` -2 -> -3, spin 3 -> 4.5) | **MATCHES** |
| 7 | B exits; 2500-frame auto-exit | verbatim, both to Options with the cursor still on CREDITS; `complete`/`failure` by score | **MATCHES** |
| 8 | Star field | 100 stars, upstream's spawn/respawn arithmetic; **no motion trails** — upstream smears them by painting `rgba(0,0,0,0.4)` over the PREVIOUS frame (`:316`) and this renderer is a pure function of state | **DIFFERS** (structural: shot byte-stability depends on the purity) |
| 9 | Star/laser motion runs in the DRAW loop | folded into the tick — upstream's draw loop is a separate rAF that its own 30 fps mode skips entirely (`main.js:1163`) | **DIFFERS** (structural, same reason) |
| 10 | Panel layout | name and role STACKED full width, not side by side; the blurb word-wraps to 2 lines | **DIFFERS** (240 px layout adaptation; no authored word is altered) |

---

## §9. Controls — device chooser, keyboard rebinder, controller calibration

The owner reports "controls can't select controller/keyboard". Measured
topology and disposition:

### 9.1 The device chooser IS implementable and WAS missing (now built)

Page 3 of `menu.js` (`menuMode = CONTROLLERCALIB`) titled `"Controls"`,
rows `["Controller", "Keyboard"]` (`menu.js:22-23,29,31-32`). It is not a
separate module — it is drawn by the same generic `drawMainMenu` and
operated by the same `menuMove` (A confirm, B back, up/down cycle). Our
port already renders exactly this page (`foh_render.c:37`). **At audit time
both rows refused** (`foh.c:202-207`); since 2026-07-29 row 0 opens the
no-controller state (§9.2, exited via D15) and row 1 opens the READ-ONLY
keyboard view (§9.3) — the D13 rebinder itself is still not built.

So the *chooser itself* — the thing the owner named — is a plain 2-row
menu page we already draw, and only its two destinations are hard.

**DEVIATION D25 — the chooser's labels and its ROW ORDER (owner-requested,
2026-08-23).** What we draw is no longer upstream's page 3 verbatim. The
Options row that opens this page reads `CONTROLS`, not `Keyboard Controls`;
the second entry reads `HANDHELD`, not `Keyboard` (there is no keyboard on a
FunKey-S — that destination is this device's own buttons); and `HANDHELD` is
row **0**, because the `CONTROLLER` destination has no hardware behind it
today. The row order is not a paint change: the page is index-selected, so
`foh.c`'s step_menu ternary swapped with the labels. Everything identity-shaped
stayed put — `FOH_CTRL_KEY` / `FOH_CTRL_PAD`, the screen tokens, and upstream's
gameModes 12 / 14 — because the judge grammar, the save-on-exit arm and the
frozen flow expects all key on those. Proved both ways by
`port/foh/check-controls-labels.sh`, whose T2 reverts the routing ternary
alone — labels untouched — and must fail.

**DEVIATION D27 — the chooser is COLLAPSED (owner ruling, same day).** D25
left the CONTROLLER row reachable and said "collapsing the submenu is a
separate decision". That decision was then taken: *"collapse now - make
easily revertable though please."* At the shipped `FOH_CTL_CHOOSER 0`
(`port/foh/foh.h`) the Options `CONTROLS` row runs `changeGamemode(12)`
directly and this whole page is unreachable, because A33 measured that the
CONTROLLER side can never exist on the shipped FunKey-OS image (no USB host
mode compiled; `docs/research/gc-adapter.md` §1.4/§2 — the port itself is
physically present, and §9.2's screen no longer claims otherwise). At
`FOH_CTL_CHOOSER 1` everything above returns exactly as written, which is
what "revertable" is required to mean: `check-controls-labels.sh` builds its
witness at BOTH values, T2 included.

### 9.2 Destination 1: Controller calibration — OUT OF SCOPE

`controllermenu.js` is **mouse-only**. Three DOM listeners
(`mousemove`/`mousedown`/`click`) are re-registered at the end of
`drawControllerMenu` every rendered frame (`:236-242`), and gameMode 14
polls input but calls **no** control function at all
(`main.js:947-949`) — nothing on this screen is reachable from a gamepad
or keyboard. The second click surface is the SVG controller diagram
(`gamepadCalibration.js:53-68`).

It also hard-requires a real pad: `getGamepad(j)` wraps
`navigator.getGamepads` (`input/gamepad/gamepad.js:9-11`), and with no pad
the "start" branch dead-ends at `errorText = ["Error: no controller
detected"]` (`gamepadCalibration.js:71`, `:161`) **without rescheduling
`preCalibrationLoop`** — the menu becomes permanently unresponsive, Quit
included, with no way out but a page reload.

**Excluded.** No mouse, no `navigator.getGamepads`, no gamepad on the
FunKey-S. Every calibration primitive (`scanForStick`, `scanForDPad`,
`scanForTrigger`, `getMinAndMax`, the min/max range extraction, the
four-point snapshot capture) is dead by construction.

**Required behaviour instead:** selecting `Controller` must show upstream's
own honest state — `"Error: no controller detected"` — and return on B. Not
a `deny` beep.

> **DEVIATION D15 — added B exit on the no-controller screen.** Stated
> plainly because it *is* a departure, and it is NOT the D9 class (D9 is
> only about declining to reproduce gameplay-menu input bugs): upstream's
> gameMode 14 has **no B handler at all**. It polls input and calls no
> control function (`main.js:947-949`), and its only exit is a mouse click
> on the `Quit` box (`gamepadCalibration.js:165-169` →
> `controllermenu.js:37-45`) — which is why the no-pad branch dead-ends
> unrecoverably. Our `B: BACK` is therefore an ADDED exit, not a ported
> one. It is the minimum that keeps
> the screen escapable on a device with no mouse; carrying upstream's
> dead-end verbatim would strand the user with no way out but a reboot.
>
> Two claims, kept apart because they have opposite signs: what the screen
> SHOWS is faithful — `Error: no controller detected` is upstream's own
> literal string, printed in upstream's own condition
> (`gamepadCalibration.js:71`). How the screen is LEFT is not faithful —
> the B exit is D15, an addition with no upstream counterpart. Together they
> answer the owner's complaint without building a calibrator for hardware
> that does not exist.

### 9.7 C30(c) — the control-style + Mod-shoulder rows (driver, 2026-07-29)

**CLOSED 2026-07-29.** The controls lane shipped three control styles and an
orthogonal Mod-shoulder swap as two process cells in `port/gfx/ctl_style.h`
(`CtlStyle {NORMAL=0, BOX=1, NATURAL=2}`, `CTL_STYLE_DEFAULT = NATURAL`, plus
`ctl_mod_on_r_{get,set}`) — and **nothing anywhere called them**
(grep-measured: zero `ctl_style_set`/`ctl_style_get` callers before this
change). A feature no user can reach is not shipped, so the driver assigned
the UI here.

**Where, and why there.** On **Controls > Keyboard** (`FOH_CTRL_KEY`), not on
the Controls menu page. Adding rows to the page would break upstream's pinned
`menuCount = [4,4,4,2]` (`menu.js:31`) — the exact class of drift this arc
exists to remove. The Keyboard screen is already OURS rather than a port
(upstream's `keyboardmenu.js` is a 56-item rebinder we did not build — D13),
so two settable rows on it is a rewrite of a rewritten screen, not a
deviation from a faithful one. The Controller row keeps upstream's verbatim
no-controller error (§9.2) and gains nothing.

**Interaction** follows the audio screen's idiom exactly, so the whole FOH
stays one model: up/down picks the row (one `else if` chain, up before down —
a simultaneous up+down runs UP ONLY), left/right cycles the value, every
accepted step plays `menuSelect`, B exits. Style cycles all three values with
wrap; the Mod row is a two-state swap so either direction flips it.

**No second copy of the value.** The rows write straight through to
`ctl_style.c`'s cells; `FohState` gains only `ctlRow`, the cursor. The values
are read by the sim-side input path in a different TU, and a mirror in
`FohState` would be a live desync — which is exactly why `ctl_style.c` is a
TU and not a header (its own note).

**Persistence** uses the two chokepoint calls `foh_persist.h` specifies,
deliberately NOT inside `foh_persist_apply/collect`: after load
`ctl_style_set(p.ctlStyle)` / `ctl_mod_on_r_set(p.modOnR != 0)`, and before
save `p.ctlStyle = ctl_style_get()` / `p.modOnR = ctl_mod_on_r_get()`. Wired
in `port/foh/foh_app.c` (the host twin). **OUTSTANDING:** `port/foh/foh_dev.c`
needs the same four lines and is the match-exit lane's file this iteration —
recorded in `.loop/menus-p2-device-workorder-audio.md` §5 rather than edited
across a live lane.

**C31 — NORMAL's display label is now "Classic".** "Normal" and "Natural"
share a prefix and a length and are near-indistinguishable in the 240x240 5x7
font, and the owner is about to choose between exactly those two. Only the
STRING changed (`ctl_style_name`). The `CtlStyle` enum values are a FROZEN
WIRE FORMAT stored verbatim in `FohPersist.ctlStyle`; renumbering would
silently remap every save on disk, so nothing was renumbered or removed.

**Font gotcha, measured not guessed:** `foh_font.c`'s face 1 has 49 glyphs
(A-Z, 0-9, and `- + . : / ' > < ! , ( )`) and **no lowercase at all**, and an
unknown glyph is a hard `gfx_fatal`, not a blank. `ctl_style_name` returns
mixed case, so the render site folds to uppercase before `foh_text`. The
f04-nav flow found this immediately as a frame-0 fatal — the guard works.

---

### 9.3 Destination 2: Keyboard rebinder — SHAPE-FAITHFUL, CONTENT REDUCED

`keyboardmenu.js` is a genuinely reachable, stick-navigated screen
(`main.js:940-943`), so unlike the controller menu its *interaction model*
ports cleanly:

- **56 bindable items** (`keyboardmenu.js:16-72`) in a 2D graph — each
  `KeymapItem` carries `above/toRight/below/toLeft` neighbour names
  (`:122-134`) and navigation is pure graph traversal (`:390-406`), with
  wrap-around edges.
- Three item types: `0` plain key, `1` modifier `[keycode, xScale, yScale]`,
  `2` range (a float).
- **Enter** enters listening mode (`:249-272`); the captured keycode comes
  from the global latch `keyBind`/`keyBinding` set by
  `overrideKeyboardEvent` (`main.js:255-268`).
- **Hold Enter 61 frames** clears a binding (`:149-199`), except 12
  protected primary slots which play `sounds.deny` + `"Cannot clear"`.
- Binding Enter itself to 12 movement/A/B slots is rejected with
  `"Not a good idea"` (`:211-228`).
- Ranges and modifier scales step **±0.01 clamped [0, 2]** on left/right
  (`:352-389`), with `menuScrollSpeed` dropping 10 → 5 while editing.
- **No conflict/duplicate detection exists** anywhere (`:136-418`) —
  binding one key to two actions is silently accepted.
- Persisted on B-exit only (`:273-279` → `setKeyboardCookie()`), one
  storage key per item name (56 keys), modifiers serialised
  `"keycode-xScale-yScale"`. Navigating away any other way discards edits.
- Defaults are the literal `keyMap` in `settings.js:8-45`. There is **no
  restore-defaults action** anywhere.
- Exit goes to gameMode **1** carrying `menuMode === CONTROLLERCALIB`
  unchanged, so what renders is the **Controls chooser**, not the top-level
  main menu. `menuMode` is private to `menu.js` (its only 26 references
  live there; no setter is exported) and `changeGamemode` never resets it
  (`main.js:554-569`); `drawMainMenu` indexes title, rows, count and
  blurb by `menuMode` (`menu.js:437,440,445,485`), so `menuMode` 3 draws
  title `"Controls"` over rows `["Controller", "Keyboard"]`
  (`menu.js:19-32`). `menuSelected` is not reset either, so the chooser
  comes back with the row you left on. Note the order at `:273-279`:
  `changeGamemode(1)` runs *before* `setKeyboardCookie()`.

**DEVIATION D13 — rebind the 12 physical FunKey-S buttons, not 56 keyboard
slots.** The device has no keyboard: there is no keycode space, no Enter
key, no primary/alternate distinction worth having, and `keytest.js`'s
256-entry label table is meaningless. Port the *shape*: a 2D graph of
bindable cells, a listening mode, a clear gesture, protected primaries, no
conflict detection, save-on-B-exit only, no restore-defaults. Bind the 12
melee actions (`a b x y z l r s` + `lstick up/right/left/down`) to the 12
physical buttons. Drop the modifier and range item types (types 1 and 2)
entirely — they scale an analog stick that does not exist; `lstick.ranges`
and `cstick.ranges` are already unreachable from upstream's own UI
(`settings.js:22` — the c-stick ranges have no `keymapItems` entry at all).

Enter → **A** (listening), hold-A 61 frames → clear, B → save and exit.
The frozen mapping in `port/foh/keymap-frozen.txt` becomes the default
row.

> `ponytail:` this is the largest single item in the spec and the least
> load-bearing — nobody is remapping a 12-button handheld mid-session. It
> is sized L and ordered last in §12 for that reason. The chooser (§9.1)
> plus the honest no-controller state (§9.2) is 90% of the owner's
> complaint at 10% of the cost.

**DEVIATION D26 — the rebinder that SHIPPED is a PERMUTATION on L/R, not
D13's listening mode (owner-requested, 2026-08-23; fix_plan A31).** The
sketch above went unbuilt for a year and the owner filed the consequence:
*"you should be able to rebind any of the 'active mappings'. currently you
can't even go to any of those rows, you can only change between 'style:' and
'mod'. changing 'mod' changes the controls. we don't want that. get rid of
'mod' altogether as an option here ... what is 'rebind: N/A'? why do we even
have that section? also, can we have a 'reset to defaults' button please."*
What shipped answers all four, and it is smaller than D13's sketch in every
dimension:

- **The rows on this screen are PHYSICAL BUTTONS**, not actions, because
  that is what the screen has always been ("what do my buttons do"). So a
  listening mode would have to mean *"press the button you want to swap
  with"*, which reads backwards. Instead **up/down picks the row and L/R
  cycles which action that row performs** — the audio screen's idiom, the
  one every other FOH row already uses, and the one this screen's own footer
  had promised since it was written. No new interaction model, and the
  caption `L/R: CHANGE   REBIND: N/A` becomes `L/R: CHANGE   A: RESET`.
- **Each step SWAPS**, so the table is always a permutation of the eight
  buttons. That is what retires D13's *"protected primaries"* clause
  outright: no action can be left on no button, so PAUSE and PAUSE MENU are
  reachable at every moment with no protection rule to get wrong. It also
  retires the *"no conflict detection"* clause — a permutation cannot have
  a conflict — and D13's **hold-A clear** gesture, since there is nothing
  to clear to.
- **`RESET TO DEFAULTS` is a real row**, which upstream does NOT have
  (`keyboardmenu.js` has no restore action anywhere) and which D13 copied
  that absence from. Owner-requested; it restores the identity binding, the
  default style and the ratified Mod arrangement in one press.
- **The `mod` row is REMOVED FROM THE SCREEN, not from the model.** The
  cell survives in `port/gfx/ctl_style.c` — the BOX label table reads it,
  the persisted record carries it, A30(a) wants it — but nothing on this
  screen writes it except the reset row. What a player wanted it for
  (Mod and shield the other way round) is now an ordinary rebind of the L
  and R rows, which is the owner's point.
- **The d-pad row is selectable and NOT bindable.** It drives the control
  STICK, not one of the eight buttons; L/R on it emits `deny`.
- **`rebind: N/A` was not vestigial** — it was this screen saying out loud
  that D13's rebinder did not exist, which was the honest caption for a
  read-only view. It is deleted because the thing it denied now exists.
- **The mechanism is ONE PERMUTATION APPLIED AT THE POLL SEAM.**
  `ctl_bind_apply()` rewrites the polled `PlatformInput` before anything
  reads it, so `s1_input.h`, the three chord tables, `ctl_roles()` and every
  frozen S1 sweep are untouched by the feature — a rebind never changes what
  a LOGICAL button does, only which physical one drives it. Under the
  fresh-install identity binding it is a struct copy, so every recorded
  session and frozen stream is unaffected by construction. The FOH MENU loop
  deliberately keeps the raw poll: a player who has moved A elsewhere must
  still be able to reach this screen and move it back.
- **The binding table is PER-PORT in the model and in the format**
  (`MLFKPERSIST6`, four `bind` rows), while the UI edits port 0 only. The
  port dimension is the expensive half to retrofit and the A33 spike has not
  closed on whether a second physical controller is possible, so it is
  carried now and exposed later.

Proved by `port/foh/check-rebind.sh`, whose T2 and T3 are the pair that
matters: T2 makes the SCREEN ignore the binding (labels lie, buttons are
right) and T3 makes `ctl_bind_apply` an identity copy (labels are right,
buttons lie). Each must fail, and each must fail ONLY on its own half — so a
build in which the screen and the buttons disagree cannot pass in either
direction.

### 9.4 `keytest.js` — not a screen

256-entry keycode→label array (`keytest.js:1-260`), imported only by
`keyboardmenu.js:10` and consumed at `:488,559,583,600`. There is no
gamemode for it and nothing in `menu.js` reaches it. Nothing to implement.

### 9.5 How the input device is *actually* chosen upstream

Not by any UI. `mType[4]` (`main.js:90-96`, values `null` / `"keyboard"` /
a `GamepadInfo` / `99` for network) is assigned at **join time** in
`findPlayers()` — whichever device presses START on the title screen or A
on the CSS claims the next port (`main.js:381-401`, `:403-411`,
`addPlayer` at `:492-503`). `keyboardOccupied` (`:381`) enforces at most
one keyboard player. `pollInputs` branches on `mType` (`input/input.js:122-145`).

So "select controller vs keyboard" is *not* a settable option upstream at
all — the Controls page only opens **configurators** for the two device
kinds. On a single-device handheld this distinction collapses entirely, and
§9.2's honest error state is the correct rendering of it.

### 9.6 Controls gap table

| # | Behaviour | Our port | Gap |
|---|---|---|---|
| 1 | 2-row chooser page, exact labels + blurbs | rendered (`foh_render.c:37`) | **MATCHES** |
| 2 | Row 0 → controller configurator | shows upstream's own `Error: no controller detected` (`gamepadCalibration.js:71`); B returns | **CLOSED 2026-07-29** (the B return is **DEVIATION D15**, §9.2 — upstream has no B handler at all and its mouse-only Quit arm `:165-169` is unreachable here, so it soft-locks) |
| 3 | Row 1 → keyboard rebinder | READ-ONLY S1 mapping screen | **PARTIAL 2026-07-29** — D13's rebinder (listening mode, hold-A clear, protected primaries) is NOT built; the screen says so rather than offering a dead control |
| 4 | B → Options with cursor on Audio | `foh.c:210-228` → menu-options, cursor 0 | **MATCHES** |
| 5 | Device kind is chosen at join, not in a menu | n/a (single device) | **MATCHES** by construction |

---

## §10. Splash and title (gameMode 20, 0)

### 10.1 Splash — `startup.js`

A purely frame-counted logo sequence. Only state is
`startUpTimer` (`:5`), incremented once per frame (`:14`), never reset.

- Frames 1–200: the Melee Light vector path (`LOGO`, `:54-66`) used as a
  clip mask with `schmoologo` scrolling through it; fade in 0–20, fade out
  180–200 (`:25-37`).
- Frames 201+: `"WITH MUSIC FROM"` + `hohlogo`; fade in 200–220, fade out
  350–370 (`:38-47`).
- **Transition at exactly frame 370** (`==`, not `>=`) → gameMode 0
  (`:49-51`).

There is **no skip input in the file**. The early-out is START, accepted by
`findPlayers` during gameMode 20 (`main.js:382`, `:446`), which jumps to
gameMode **1** (main menu) and **skips the title screen entirely**.

### 10.2 Title — `startscreen.js`

**Draw-only: the token `input` does not occur in the file.** Dismissal
lives in `main.js`.

- Keyboard path (`main.js:381-394`): `keys[13]` (Enter) **or either bound
  START key** — not "any key".
- Gamepad path (`main.js:446-465`): the **START button only**.
- Destination: gameMode **1** (main menu), not the CSS.
- **No timing gate.** `findPlayers` runs from the very first tick; START is
  live immediately.
- The guard is `gameMode < 2 || gameMode == 20`, so START also *adds
  players* from the main menu; past that it is **A** that adds a player
  (`main.js:395-400`, `:466-479`).

Visual state, all `export let` and persisting forever: `angB`/`angR`
pinwheel angles at `+0.001`/frame (`:74`, `:107`), `mlVel`/`mlPos`/`mlDir`
bobbing the `"LIGHT"` wordmark with the direction flipping at
`|mlVel| > 0.8` (`:141-150`), 6 drifting rings (`:53-68`), 20 rising dust
motes respawning below `y < 410` with alpha decaying `-0.01`/frame
(`:13-16`, `:153-157`). Prompt: `"PRESS START"`, `900 40px monospace`,
`#f0c900`, stroked black, at (600,600) (`:176-181`).

### 10.3 Splash/title gap table

| # | Behaviour | Our port | Gap |
|---|---|---|---|
| 1 | Splash advances at exactly frame 370 | `foh.c:492-496`, `startupTimer == 370` | **MATCHES** |
| 2 | START during splash skips **to the main menu** | all inputs ignored during splash | **DIFFERS** |
| 3 | Title dismissed by START only | `foh.c:497-505`, START edge → menu-top | **MATCHES** |
| 4 | Title → main menu | → menu-top | **MATCHES** |
| 5 | No timing gate on title input | none | **MATCHES** |
| 6 | Splash content: two logo phases | progress bar + wordmark (`foh_render.c:864-876`) | **DIFFERS** (our own design, approved) |
| 7 | Title dust motes / drifting rings | registered as not carried (`foh_render.c:872-881`) | **MISSING — accepted** |

---

## §11. Explicitly out of scope

Each exclusion names what it excludes and why. Anything not on this list is
in scope.

| # | Excluded | Reason |
|---|---|---|
| 1 | **Controller calibration** (`controllermenu.js`, `gamepadCalibration.js`) | Mouse-only DOM/SVG interaction (`controllermenu.js:236-242`); gameMode 14 runs **no** control function (`main.js:947-949`); hard-requires `navigator.getGamepads` (`gamepad.js:9-11`). No mouse and no gamepad exist on the device. Replaced by upstream's own `"Error: no controller detected"` state, exited via **DEVIATION D15**'s added B — §9.2. |
| 2 | **Keyboard rebinder's modifier and range item types** | Types 1 and 2 scale an analog stick that does not exist; upstream's own UI already leaves `cstick.ranges` unreachable (`settings.js:22`, no `keymapItems` entry). §9.3 / D13. |
| 3 | **Target builder** (gameMode 4, `menu.js:87-90`) | A level *editor*. No text entry, no mouse, no file surface on the device. Row stays visible and refuses, matching upstream's own P2P-style dead row. |
| 4 | **Netplay: Spectate, P2P, Server** (`menu.js:108-121`) | Requires the `streamclient` stack, a server, and `inServerMode`. Owner ruling: **hide behind a named flag.** See §11.1. Note P2P is *already* dead upstream — its body is commented out (`menu.js:114-116`) — so P2P specifically is faithful when it does nothing. |
| 5 | **CSS free-text name tags** | jQuery HTML `<input>` overlay (`css.js:438`) committed by `keys[13]`. No DOM, no keyboard. Random-tag and clear-tag survive — D8. |
| 6 | **CSS NET port type** | Consequence of #4: a reachable NET state could never launch. D5. |
| 7 | ~~**3- and 4-participant launches**~~ **LANDED 2026-08-24 (A44)**, on A46's `sim_setup_match_ports` + the four-port golden `q01`; DEVIATIONS D40 and D41 | Was gated because every frozen golden was 2-player. A46 recorded a 4-port one, so D6 was deleted rather than weakened. What remains restricted is narrower and registered: no CPU on ports 2/3 (one AI replay slot), and port 0 must be HMN (D17's amended ground). |
| 8 | **`keytest.js` as a screen** | It is a lookup table, not a screen — no gamemode, nothing reaches it (§9.4). |
| 9 | **`blastzoneWrapping`, `dustLessPerfectWavedash` UI rows** | Upstream has no row for them (their label entries are `""`, `css.js:85`/`:87`) and **zero code reads them**. They are persisted at their defaults and never shown — §3.2. |
| 10 | **Target select semantics** | RENDER measured only in PART (`drawTSSInit` whole, `drawTSS` `:244`-~`:420` of `:244-540`; the tail `:421-540` unread) and the look re-authored from it; `tssControls` (`:43-173`) NOT measured, so the screen stands as **UNVERIFIED** as a whole rather than being declared a gap — §7.1. |
| 11 | **Menu screens on the checksum surface** | Menus are not on the oracle surface at all. Two upstream input bugs (Q4 malformed diagonal guards, Q5 the 60 Hz left-repeat) are therefore *not* reproduced — D9. Faithfulness binds semantics, not typos that only degrade input. |

### 11.0 OWNER RULING SUPERSEDES §11.1 (driver, 2026-07-29)

**§11.1 below is EVIDENCE about upstream's shape; it is NOT the binding
instruction, and where the two differ the OWNER RULING WINS (HARD RULE 5
governs faithfulness to upstream; the owner governs what we ship).**

§11.1 argues the three netplay rows should stay *drawn but refusing*,
reasoning from upstream drawing them unconditionally. Chase's actual C5
ruling (fix_plan, 2026-07-28) is different and binding:

> "For now let's hide spectate / p2p / server. vs. melee should just take
> you to local vs for now. (flag or some way smart to revert this later
> in the future)."

So the SHIPPED behaviour is: the three rows are **HIDDEN**, and **VS
MELEE goes STRAIGHT to local VS** (no Battle-Mode submenu step). The
"named flag" requirement in §11.1 still applies and is the part to keep:
one documented switch that restores the battle page's shape — rows
drawn, submenu reachable — so this is revertible without archaeology.
Nothing is deleted. The switch's scope is exactly §11.1's: the battle
page only. It does NOT touch the CSS port-type cycle — `NET` stays
dropped at BOTH flag values as DEVIATION D5, which carries its own owner
acceptance (§11.1, third bullet).

This is a deliberate, owner-sanctioned DEVIATION from upstream's menu
graph, recorded as such rather than smuggled in.

### 11.1 The netplay flag requirement

**BINDING — owner ruling C5 (2026-07-28), implemented 2026-07-29.**
`FOH_NETPLAY` (`port/foh/foh.h`) covers the **battle page only**.

- At `FOH_NETPLAY 0` — the shipped build — the MPMENU page is unreachable
  and `VS. Melee` runs `menu.js:105`'s Local VS action itself. The three
  Battle-Mode rows (`Spectate`, `P2P`, `Server`) are **HIDDEN, not
  drawn-and-refusing**, because the owner asked for "VS MELEE goes straight
  to local VS".
- Nothing is deleted. The page, its labels and blurbs
  (`foh_render.c` `kMenuText[2]`), its four A-arms, its B-back edge and its
  four judge-registered transitions all still exist and still compile;
  flipping the flag to 1 restores the page SHELL and the `VS. Melee` routing ONLY — the three rows
themselves still emit `deny`/`refused`, because Spectate/Server were never
implemented in this port and upstream's P2P body is commented out. A future
netplay arc owns their actual behaviour.
- The CSS `NET` port type is **explicitly OUT of `FOH_NETPLAY`'s scope**.
  It stays **DEVIATION D5** — a separate registered deviation carrying its
  own owner acceptance — and `togglePort` runs D5's 3-cycle at *either*
  flag value. NET has no implementation anywhere in the port, so putting a
  NET tab behind a flag nobody compiles would be speculative work.
- No netplay symbol may be referenced from a frozen-list TU — the
  constructor-installed pointer-seam pattern established for
  `ml_sim_runai_live` (`port/sim/sim/sim_ai_live.c`) is the precedent.

**NOT BINDING — superseded pre-ruling proposal, retained only as the future
netplay arc's checklist.** Before the ruling, this section asked for ONE
flag covering both the Battle-Mode rows *and* the CSS `NET` port type, with
the disabled rows still DRAWN and `togglePort` switching between the
3-cycle and upstream's 4-cycle with the flag. The ruling above is narrower
and is what ships; where the two disagree, **the ruling wins**. A future
netplay arc must revisit both surfaces. (Recorded because a reviewer
correctly read the old text as binding and called the implementation
incomplete against it — the spec is evidence, and this is the correction.)

---

## §12. Gap totals

> **THIS TABLE IS THE ORIGINAL AUDIT — a historical baseline, NOT current
> state.** Every row is the count as first measured, and the totals
> reconcile against those rows. It is deliberately NOT re-scored per arc:
> re-auditing a row means re-reading upstream and re-testing the screen,
> and only screens that have actually had that treatment could honestly
> move. What has closed since is narrated below and in the per-screen
> sections, which are the current-state source of truth. An earlier
> revision of this arc tried to present the table as current and got it
> wrong — the CSS row in particular still describes the pre-CSS-mechanics
> port, so a "current" reading of it would be false.

Counted from the per-screen tables above, as of the original audit.
"Intentional" counts the **DEVIATIONS this spec mandated for that screen at
audit time**, not debt — so it is D1-D13 only. D14 (audio numeric readout)
and D15 (added B exit on the no-controller screen) were both registered
AFTER this audit and are deliberately not folded into these counts; they
are in the §12.1 register.

| Screen | MATCHES | DIFFERS | MISSING | Intentional |
|---|---:|---:|---:|---:|
| **CSS** | **0** | **8** | **11** | 2 |
| Gameplay options | 4 | 2 | 4 | 0 |
| Audio options | 0 | 0 | **4** (whole screen) | 0 |
| Main menu + submenus | 4 | 3 | 0 | 0 |
| SSS | 3 | 5 | 0 | 1 |
| Credits | 7 | 3 | 0 | 0 |  <!-- A7 2026-08-24: was 0/0/7-whole-screen -->
| Controls family | 3 | 1 | 1 | 0 |
| Splash / title | 4 | 2 | 1 | 0 |
| Target select | — | — | — | UNVERIFIED |
| **Total** | **18** | **21** | **28** | 3 |

That audit-time distribution matched the owner's report exactly: the menus
he could *navigate* scored well (main menu 4/3/0, splash+title 4/2/1), and
the screens he tried to *use* scored badly — CSS scored **0 MATCHES out of
19 behaviours**, and TWO whole screens did not exist.

**What has closed since, by screen** (this list, not the table, is current
state; each entry points at the section that carries the evidence):

| Screen | Since the audit | Where |
|---|---|---|
| Gameplay options | CLOSED modulo D16 — all ten behaviours; held-direction repeat is the registered whole-FOH deviation | §3, §3.1 table |
| Audio options | HOST-PROVEN, DEVICE-PENDING — the screen exists, persists, and its levels are applied to the mixer on the host path; the shipping target has no caller yet (owner ruling "yep wire", 2026-07-29); host proof = check-foh-flows.sh leg [5c], device audio proof outstanding as a work order | §4 |
| Controls family | row 2 CLOSED (no-controller state, exited via D15); row 3 PARTIAL — the S1 map is still read-only (D13's rebinder is NOT built and the screen says so), but the screen now carries the TWO SETTABLE ROWS of C30(c) | §9.6, §9.7 |
| Battle submenu | HIDDEN by owner ruling C5, nothing deleted | §11.1 |
| CSS | the CSS-mechanics arc landed items 1-4 (cursor, token model, port types, ready/launch); the §2 tables and the CSS row above still describe the PRE-arc port and have not been re-audited | §2 |
| Target select | LOOK measured and re-authored; semantics still UNVERIFIED | §7.1 |
| Credits | BUILT 2026-08-24 (A7). D12 + D38 are its only behavioural departures; the three DIFFERS rows in the §8.5 table are structural (no motion trails, draw-loop motion folded into the tick) or 240 px layout. Never rendered ON the device — that is Chase's acceptance playthrough | §8 |
| SSS, splash/title | untouched since the audit | §6, §10 |
>
> Closed by owner ruling C5 rather than by measurement: the battle submenu
> (Spectate / P2P / Server) is HIDDEN behind `FOH_NETPLAY` in
> `port/foh/foh.h` and `VS. Melee` runs `menu.js:105`'s Local VS action
> directly. Nothing was deleted — flipping the flag
> restores the page SHELL and the `VS. Melee` routing, and the judge still
> registers all four of its edges. It does NOT make the three rows work:
> they still refuse, and a future netplay arc owns that.

>
> Closed by owner ruling the same way (2026-08-23, DEVIATION D27): the
> CONTROLS chooser is COLLAPSED behind `FOH_CTL_CHOOSER` in `port/foh/foh.h`
> and the Options `CONTROLS` row runs `menu.js:159-161`'s `changeGamemode(12)`
> directly. Nothing was deleted — flipping that one digit restores the chooser
> page and the D25 route whole, and the judge registers all six of its edges
> under the `ctl` profile. The reason it is a collapse and not a greying is
> A33: the shipped FunKey-OS image compiles no USB host mode, so the
> CONTROLLER side of a two-way choice can never exist, and a chooser whose
> purpose IS the choice has no job left. `port/foh/check-controls-labels.sh`
> builds its witness at BOTH flag values, so the restorable path is exercised
> rather than merely promised.

### 12.1 Deviation register

| # | Deviation | Section |
|---|---|---|
| D1 | d-pad supplies `lsX/lsY ∈ {-1,0,+1}` | §1.2 |
| D2 | `du/dd/dl/dr` unbindable; drops the CSS d-pad-up launch and d-pad-right-Falco arms | §1.2, §2.10 |
| D3 | Cursor speed as a fraction of screen + `FOH_CURSOR_SPEED` knob | §1.2 |
| D4 | Hit regions are our layout's drawn rects, not upstream pixel coords | §1.2 |
| D5 | CSS port-type cycle drops NET (3-cycle); NOT gated on `FOH_NETPLAY` — unconditional until a future netplay arc | §2.7, §11.1 |
| ~~D6~~ | ~~Ports 3–4 stay N/A until 3/4-player conformance is proven~~ **RETIRED 2026-08-24 (A44)** — A46 proved it with the four-port golden `q01`; superseded by D40 | §2.7 |
| D7 | CPU knob is grab-drag at the §1.2 cursor speed | §2.8 |
| D8 | Name tags: keep random + clear, cut free-text entry | §2.9 |
| D9 | Do not reproduce the malformed diagonal guards / 60 Hz left-repeat | §3.4 |
| D10 | Clamp the SSS cursor to the screen | §6.3 |
| D11 | SSS RANDOM resolves off the seeded chain | §6.5 |
| D12 | Credits reticle becomes a relative cursor | §8.3 |
| D13 | Rebind 12 physical buttons, not 56 keyboard slots | §9.3 |
| D14 | Numeric tenths readout beside each audio knob (upstream draws none) — OWNER-RATIFIED 2026-07-29 | §4 |
| D15 | Added `B` exit on the no-controller screen (upstream has no B handler; mouse-only Quit) | §9.2 |
| D16 | Front-of-house navigation is RISING-EDGE only — holding a direction steps ONCE, where upstream repeats 1-then-every-10 frames. Applies to every FOH screen incl. §3 and §4 | §5.5 row 6 |
| D17 | A CPU in port 1 cannot LAUNCH (`sim_setup_match` carries no P1 type) — menu plane unchanged | §2.7 |
| D18 | Name entry becomes a NOVEL letter-grid screen modelled on real Melee (upstream has no canvas name entry at all) — supersedes D8's "cut free-text"; blocked on A14's glyph coverage | §2.9 |
| D19 | The random-tag draw uses a FOH-LOCAL stream and cannot shift the match (upstream shares the sim's stream; our 465-boot-draw pin makes that unavailable) | §2.9 |
| D20 | **HOUSE RULE, owner-requested 2026-08-04:** "Everyone Walljumps" — dead upstream — is given real mechanical effect at the per-character ability gate. Defaults OFF forever; flag-off is bit-identical | §3.3 |
| D21 | **A29, owner-reported P0 (2026-08-23):** endGame's token snap lands on the port's CHOSEN cell. Upstream indexes `charIconPos` — a per-CHARACTER array — by PORT number (`css.js:154`, called `main.js:1381-1384`), so upstream's tokens sit on marth/puff after every match regardless of picks. Demonstrably an upstream TYPO, not design: `setChosenChar` passes TWO args (`css.js:146`) to a callee declaring one and dropping it — we honour the argument the caller passes. Cosmetic upstream (four large panels disambiguate); here the token is the ONLY roster-level pick indicator at 240x240 | §2.6 |
| D22 | **A23, owner-requested P1 (2026-08-22):** the CSS BACK wedge is HELD, not clicked. Upstream hit-tests it as an INSTANT A-click — `handPos.y < 160 && handPos.x > 920` on the A rising edge, calling `changeGamemode(1)` outright (`css.js:358-363`) — and reserves its 30-frame `bHold` counter for the B button (`css.js:186-194`). The owner asked for the arcade behaviour instead: *"I want it to behave like how melee does — holding the cursor OR holding the back button starts progressing a little red bar that fills below the back button, and when it fills it actually backs out."* Those words are the ratification. The deviation is ONLY the TRIGGER: the hand now ARMS upstream's own counter instead of firing instantly, so there is ONE counter with TWO input paths and no second timer. Everything else is upstream verbatim — the 30 frames, the `== 30` equality that fires it exactly once per hold, the `menuBack` sound, and **the red bar itself, which is NOT a deviation**: upstream draws it at `css.js:735-746` (`bestHold` = max over the four ports; a red quad from x 1020 to 1020 + 6·`bestHold` along the wedge's bottom lip, guarded on `bestHold > 0`) and this port simply had not carried it. That `> 0` guard is load-bearing here too — a cold CSS draws no bar, so A23 re-froze **zero** of the judged shots (measured, `check-css-back.sh` T2) | §2.9 |
| D23 | **A32, owner-reported P2 (2026-08-23):** the gameplay screen's per-player row is relabelled `TAP JUMP` and its value is INVERTED ON DISPLAY, so `ON` means tap jump works. Upstream's row is the double negative `"Tapjump off"` with the value `"On"` when the feature is DISABLED (`gameplaymenu.js:182`, `:242`), and it is genuinely unreadable: the owner read it as "L-cancel is off for P2/P3/P4" and filed a defaults bug against a screen whose defaults were correct. (What he actually saw was correct too — P1 has tap jump disabled because a digital d-pad at full deflection tap-jumps on every upward DI, `port/gfx/ctl_style.h:14-23`, and P1 is the only human port on this device today.) The deviation is CONFINED TO THE PIXELS: `FohState.tapJumpOff` keeps upstream's polarity and `foh_persist`/`foh_app`/`foh_dev` hand that same bit to the sim unchanged, so no checksum can see this. Proven both ways by `check-legibility.sh` — the witness asserts the rendered STRING against the state, and its T1 puts the double negative back and must fail | §3.1 |
| D24 | **A25(a), owner-reported P1 (2026-08-23):** the SELECTED target-select slot gets a 2 px border and a body lifted off black (`{52,22,32}`), where upstream's entire selection signal is a ONE-PIXEL stroke that goes grey `rgb(166,166,166)` -> flashing pink (`targetselect.js:270-279`). Owner: *"the highlighting around test 1 or + ADD CODE you selected is not really visible to the eye."* Measured, he is right — on our rects that stroke is 242 changed pixels around a 100x19 black body on a busy brown gradient, at 240x240. The deviation stays inside upstream's OWN idiom, which `foh_render.c:2049-2050` records: the BORDER carries the state and the LABEL is never brightened on hover — so the label keeps `:201`'s grey in both states and only the border and body move. All ELEVEN slots get it, the "+ ADD CODE" slot included (the owner named both); its ring grows 1 px on three sides only, because the info panel's 50%-black face starts on the row below it. `check-legibility.sh` floors every slot at 600 changed pixels, which no one-pixel border on these rects can reach, and its T2 restores the one-pixel border and must fail | §7 |
| D25 | **A24 + A4, owner-reported P2 (2026-08-23):** the Controls chooser is relabelled and REORDERED. Owner: *"controls option in the menu says 'Keyboard Controls'. Clicking it takes you to a menu that says 'Controller' and 'Keyboard', so it shouldn't say 'Keyboard controls' — just Controls. Then, in the submenu, 'Keyboard' actually isn't a keyboard, it's the funkey s controls. So we need to change the name there. Also make funkey controls the first option (controller is first currently, and it shouldn't be, because we can't even use a controller on the funkey s)."* Three renames, all PAINT plus ONE routing swap: the Options row 2 label becomes `CONTROLS` (upstream `Keyboard Controls`, `menu.js:21` — the row opens a CHOOSER, so upstream named it after one of its own two destinations); the chooser's second entry becomes `HANDHELD` (upstream `Keyboard`, `menu.js:23` — there is no keyboard on this device, and `HANDHELD` is a CATEGORY name parallel to `CONTROLLER`, deliberately shorter than it so no width pin moves); and `HANDHELD` becomes row **0**, `CONTROLLER` row 1. **The order half is NOT paint.** The chooser is INDEX-selected (`foh.c` step_menu's ternary), so the labels and the routing move together or the first row lies about where it goes — `check-controls-labels.sh`'s T2 reverts the ternary ALONE, leaves the labels in place, and must fail. **IDENTITY DOES NOT MOVE:** `FOH_CTRL_KEY`/`FOH_CTRL_PAD`, the screen tokens `controls-keyboard`/`controls-controller` and upstream gameModes 12 / 14 are all unchanged — the judge grammar, `foh_app.c`'s save-on-exit arm and every frozen flow expect key on those. The CONTROLLER branch stays REACHABLE (collapsing the submenu is out of scope here: the owner's collapse decision was taken on evidence the driver later retracted, and A24 was HELD for the renames only). `f04-nav`'s two Controls shots keep their names and swap frames. **A4 (same ticket, already shipped as C31):** the style formerly displayed `Normal` reads `CLASSIC` — `ctl_style_name` only; `CTL_STYLE_NORMAL` is still enum 0 because `FohPersist.ctlStyle` stores it verbatim. Asserted here for the first time rather than remembered | §9.1, §9.3 |
| D26 | **A31, owner-reported P1 (2026-08-23):** the Controls > HANDHELD screen gains a REAL rebinder, and it is not the shape D13 sketched. Owner: *"you should be able to rebind any of the 'active mappings'. currently you can't even go to any of those rows, you can only change between 'style:' and 'mod'. changing 'mod' changes the controls. we don't want that. get rid of 'mod' altogether as an option here... what is 'rebind: N/A'? why do we even have that section? also, can we have a 'reset to defaults' button please."* The cursor covered TWO rows and the nine ACTION rows were display-only; it now covers **eleven** — the nine action rows (row 0 the d-pad, rows 1-8 the physical buttons), the style row, and a new **RESET TO DEFAULTS** row that upstream does not have at all. **L/R rebinds by SWAPPING** the selected row's action with whichever button holds the one it steps onto, so the table is always a permutation: no action can be lost, which retires D13's *protected primaries*, its *hold-A clear* and its *no conflict detection* clauses in one stroke. The **mod row is removed from the SCREEN, not the model** (the cell still backs the BOX label table and the persisted record; swapping the shoulders is now a plain rebind). **`rebind: N/A` is deleted** — it was the honest caption of a read-only view of D13's unbuilt rebinder, and the thing it denied now exists. The mechanism is ONE permutation applied at the poll seam (`ctl_bind_apply` in `foh_dev.c`'s `poll_bound`), so `s1_input.h`, the three chord tables and every frozen S1 sweep never see the feature; under the fresh-install IDENTITY binding it is a struct copy, so no recorded session or frozen stream moves. The FOH menu loop keeps the RAW poll on purpose — a player who moved A elsewhere must still be able to reach this screen. Bindings are PER-PORT in the model and in the persisted record (four `bind` rows, each validated as a permutation) with the UI editing port 0 only, per the A33 re-amendment. Proved by `port/foh/check-rebind.sh`, whose T2/T3 pair makes the screen and the buttons lie in turn and requires each to fail alone | §9.3 |
| D27 | **A24 second half, owner ruling 2026-08-23:** the Controls chooser is COLLAPSED — the Options row `CONTROLS` opens the HANDHELD screen directly and its B returns to Options on the row that opened it. Owner: *"collapse now - make easily revertable though please."* The chooser (upstream menuMode 3, `menu.js:138-141`) exists only to make a two-way choice, and **A33 measured that one side can never exist on this hardware's shipped OS**: `CONFIG_USB_MUSB_GADGET=y` with no `HOST`/`DUAL_ROLE` (mutually exclusive Kconfig in 4.14, so host code is not compiled), `dr_mode = "peripheral"` in the DTS, a floating USB ID pin — undoing that means rebuilding and reflashing FunKey-OS, and this project ships an OPK. (A33's earlier POWER kill is RETRACTED: `docs/research/gc-adapter.md` §2. The port is physically there; the renderer line that claimed otherwise is corrected in the same change to `FUNKEY-OS SHIPS NO USB HOST MODE`.) **Collapse, not grey, and the distinction is A10's:** greying suits a page that KEEPS live entries beside the dead ones; a two-entry chooser whose purpose IS the choice has no job left once one side dies. **NOTHING IS DELETED and the revert is ONE DIGIT** — `#define FOH_CTL_CHOOSER` (`port/foh/foh.h`), the `FOH_NETPLAY` precedent exactly: `FOH_MENU_CONTROLS`, `FOH_CTRL_PAD`, `render_ctrl_pad`, the chooser's labels, its A ternary, its B-back edge and all six judge-registered transitions still exist and still compile, and at 1 the D25 route comes back whole. IDENTITY DOES NOT MOVE: the tokens `controls-keyboard`/`controls-controller` and gameModes 12 / 14 are untouched, so `foh_app.c`'s save-on-exit arm (`from == controls-keyboard`, `cause == b`) is unchanged. The judge carries it as a SECOND build profile (`ctl`/`noctl` rows, parsed live out of the header like `FOH_NETPLAY`), so flipping the digit without re-freezing `f04-nav` fails mechanically. `check-controls-labels.sh` builds its witness at **both** values — T2, the half-swap trap, bites in both, and T5 is its twin for the collapsed arm | §9.1, §12.1 |
| D28 | **A27, owner-reported P1 (2026-08-23):** the CSS mode ribbon becomes clickable, and its LABEL carries the mode. Owner: *"there's no way to change between stock mode and 'endless ko fest'. If you click the 'VS Melee' in the CSS it should change modes."* **The TRIGGER is upstream's, verbatim, and deviates in nothing:** an A rising edge inside the widget plays `menuSelect` and calls `setVersusMode(1 - versusMode)` (`css.js:389-394`), from the same position in `step_css` that upstream runs it from — after the hand integrates, between the port-type boxes and the CPU-knob grab, so a hand carrying a token cannot toggle the mode. **Two things deviate, both forced by the rewrite.** (a) THE RECT: upstream hit-tests `y > 100 && y < 160 && x > 380 && x < 910` on a 1200x750 canvas — a band BELOW its header, because upstream draws this blurb as loose 1.25x-scaled text at (390,117) with no plate at all (`css.js:715-721`). Our whole silver header is 26 px tall, so that ratio maps to y 32..51, i.e. under the header and onto the roster row. D22's BACK wedge could use upstream's ratio because 920/1200 x 240 = 184 landed exactly on its drawn extent; this one does not, so the DRAWN EXTENT wins — `FOH_CSS_MODE_{X0,X1,Y0,Y1,CAP}` (`foh.h`) build the plate in `css_header` and hit-test it in `step_css`, one source, which is what D4 asks for. (b) THE LABEL: upstream's strings are 19 and 20 characters against a 74 px plate that holds 12 glyphs of the 5x7 face, so ENDLESS carries upstream's own words over two rows (`ENDLESS` / `KO FEST!`, only the article dropped) and STOCK keeps `VS. MELEE` — the gamemode's own name; `4-MAN` would be a lie on a two-port build (D6). **Keeping the stock label is not cosmetic conservatism:** every judged CSS shot is taken in stock mode, so the default state renders byte-for-byte what it rendered before A27 and NO frozen shot needs re-freezing — the same argument A23's `bHold > 0` guard makes, and asserted the same way, by cmp against a build whose label is unconditional. **The mode is not a label:** `G.sim.versusMode = foh.versusMode` lands immediately BEFORE `sim_setup_match` in both launch bridges because `startGame` reads it (`main.js:1334`), and `port/foh/check-css-mode.sh` proves it in the SIM's own checksum stream, with a tooth (T3) that moves the write one line down and watches the endless match collapse back to 4 stocks | §2.9 item 18, §2.9 item 18a |
| D29 | **A25(c), owner-requested P1 (2026-08-23):** target-select is driven by the FREE HAND CURSOR, not a d-pad index cursor. Owner: *"here I want to utilize our cursor logic like we have at the character select screen where it's a free moving cursor (with the hand pointing). want to keep things DRY here — this needs to be spec'd out and in a smart way."* **THIS REVERSES A RATIFIED REWRITE, and that is the whole cost of it.** `port/foh/foh.h`'s rewrite deltas record that upstream's target-select and stage-select POINTERS were deliberately rewritten INTO index cursors for a device with no mouse; the SSS keeps that rewrite, so after this change the two sibling screens differ in interaction model — TSS is upstream's pointer again, SSS is a 3x2 grid. The owner asked for it after playing it, so it proceeds; either the SSS follows in a later arc or the inconsistency is accepted, and that is his call. **DRY IS THE IMPLEMENTATION, not a slogan:** the motion and the hit predicate are ONE definition in `port/foh/foh_hand.h` (`foh_hand_step` carrying D1/D3 and the clamp; `foh_hand_hit`, whose strict comparisons ARE the CSS's old `css_cell_at` gutter rule, so D4's "no hit region where nothing is drawn" now holds on both screens by construction), and the rect TABLES are caller-owned so each screen's hit region is the renderer's own (`foh_css_cells` / `foh_tss_slots` in `foh.c`; `FOH_TSS_*` in `foh.h`). The CSS extraction is a PURE REFACTOR and is proven so DIFFERENTIALLY, not argued: `port/foh/check-hand.sh` leg [4] builds one driver against the working tree and against the pinned pre-A25(c) commit and requires 60,000 swept CSS frames of hand bits, machine plane, sound queue, token positions and rendered-frame hashes to be byte-identical. **`tssCursor` SURVIVES as the selection** — what a launch launches and what the renderer rings — and is now WRITTEN by the hand rather than stepped by the d-pad, STICKY exactly as the CSS's hover-selects rule is sticky (`css.js:222-226`), so the gutter never leaves the screen with no target and `targetRecords[p1Char][tssCursor]` never loses its index. `foh_look_canonical` deliberately does NOT pin the hand: it pins TICK-driven look counters, and the hand is navigation-driven like `menuColours` and `cssHandX/Y`, neither of which it touches. **`flows/f07-target-t02.flow` is re-cut** (a level-driven cursor needs held runs where an edge-driven one needed taps) and its frozen `.expect` is BYTE-UNCHANGED; `f06` needed no change at all, because the hand re-homes on slot 0's centre at entry exactly as `tssCursor = 0` did. `check-legibility.sh` drops the two grammar pins that guarded its witness's hand copy of the slot geometry — the witness reads `foh_tss_slots()` now — and its T2 one-pixel-border tooth still bites on all eleven slots | §7, §1.2 |

| D30 | **A30(a), owner ruling 2026-08-24:** the Mod shoulder's DEFAULT arrangement flips — **Mod on R, shield on L**. Owner: *"box is good but L should be shield and R should be mod / tilt."* **This is a DEFAULT FLIP, not a table edit.** The 2026-07-29 ruling already made the Mod shoulder a SEPARATE, orthogonal cell (`ctl_style.h:63-70`), and swapping it is a pure RELABELING: `ctl_roles()` (`s1_input.h:173-184`) reads the cell to decide which shoulder carries which role, and the ratified S1 BOX chord table is byte-untouched — the same chord row answers, only the shoulder that reaches it changed. Asserted, not claimed: `port/gfx/ctl_input_witness.c` leg [3c] drives `s1_resolve_style` under BOTH arrangements and requires the same row name and the same coordinates (`L-horizontal-walk`, the ratified Mod walk). The cell is a no-op in NATURAL/CLASSIC, where both shoulders shield. **Two default sites, and only one is the player's:** `ctl_style.c`'s initializer is flipped here, but every FOH binary calls `foh_persist_load()` at boot, which runs `foh_persist_defaults()` and then `ctl_mod_on_r_set(p.modOnR != 0)` — so the fresh-install value the player actually gets is `port/foh/foh_persist.c:52`, which the FOH lane must flip in the same release or this deviation is inert | §9.3 |
| D31 | **A41, owner re-ratification 2026-08-24:** **L-ONLY SHIELDING** on NATURAL and CLASSIC. Owner: *"L-only shielding is totally fine. I want it in fact."* `ctl_roles()`'s non-BOX arm (`port/gfx/s1_input.h`) goes from `*shield = (p->l \|\| p->r)` to `*shield = p->l`. BOX is untouched — it has always split the two shoulders (Mod on one, shield on the other), which is exactly why this deviation names only the other two styles. **This is the first link of a three-link chain and does not stand alone:** L-only shielding FREES R, R carries the C-layer (D32), the C-layer vacates Y, and Y becomes SPECIAL so that X can become a real GRAB (D33). Un-doing D31 alone silently un-does all three. Asserted by `port/gfx/check-ctl-input.sh` leg [3a] (R held on the fresh-install style must emit `r=false, rA=0`) with tooth **t4-bothshields**, which restores the pre-D31 expression and must kill the witness; and swept over all 2048 combos by `port/gfx/s1_sweep.c` (`inC.r == p.l` on the CLASSIC row) | §9.3 |
| D32 | **A41, owner re-ratification 2026-08-24:** the **C-LAYER MOVES FROM Y TO THE R SHOULDER** on NATURAL and CLASSIC, and **BOX LOSES IT**. `ctl_style_has_clayer()` becomes `style != CTL_STYLE_BOX` and `ctl_roles` reads `p->r` instead of `p->y`. **THE ARITHMETIC IS THE ARGUMENT, not taste:** the pad has 8 buttons, START is pause and MENU is the pause menu, so SIX reach gameplay, while the roles wanted number SEVEN — attack, special, jump, grab, shield, Mod, C-layer. Once grab is a real button (D33) every style must drop one, and which one it drops IS the style: BOX pays for its Mod shoulder (D30, owner: *"R should be mod / tilt"*) with the C-layer, while NATURAL and CLASSIC, having no Mod, fit exactly six roles in six buttons and keep it. **NATURAL GAINS A C-STICK it never had** — reversing the pre-A41 note that Natural "gives up the single reachable C-direction", which was a concession to button PRESSURE (`ctl_style.h`), not to its 1:1 identity; with R freed by D31 the pressure is gone, and the alternative was a DEAD shoulder on the default style. Its three new rows re-emit magnitudes already in its own table (1.0 cardinals, 0.7 diagonals) — **no coordinate is invented**. BOX's C-layer rows stay in the ratified table BYTE-EXACT and unreachable, against a future ruling. **Two consequences, both discharged in the same change:** (a) `s1_sweep.c`'s six pinned cs checks (11-13) now run under CLASSIC — same coordinates, same `ls neutral` assertion, different style; (b) the BOX-pinned device rig (`gfx_app.c` -> `judge-s1-coverage.js`) loses its five cs signatures because the style it drives no longer HAS a cs plane; the plane is pinned bit-exactly host-side instead, and the rig gains the D33 button-plane signatures plus two plain-stick chords its session already produced. Asserted by `check-ctl-input.sh` leg [3d] (both styles' cs cardinals + diagonal with a frozen left stick, and BOX emitting no cs at all) with tooth **t5-clayeronY** | §9.3 |
| D33 | **A41, owner re-ratification 2026-08-24 — THE ONE THAT TOUCHES THE FROZEN TABLE:** the FACE PLANE becomes **A=JUMP, B=ATTACK, Y=SPECIAL, X=GRAB (Z)**, **STYLE-INDEPENDENT — BOX INCLUDED**. Owner: *"X->grab, A->jump, Y->special, B->attack"* and, of BOX, *"wtf you can't grab on box?? we want to be able to"*. The style branch in `s1_input_row_style`'s button plane is DELETED; five unconditional assignments replace it. **The owner's premise was measurably false and the ruling still stands:** grab already reached the sim on every style through shield+A (`GUARD.c:75-78`, `GUARDON.c:101-104` — the arm is style-independent), so what was actually broken was that no style had a grab BUTTON and the Controls screen listed none. The re-ratification gives it one. **`in.y` is deliberately left unset** — Melee treats X and Y alike, so a second jump field would be a second name for one bit. **What moved that was pinned:** `s1_sweep.c`'s 2048-combo button invariant (`in.a != p.a \|\| in.b != p.b \|\| in.x != p.x` -> the D33 form, asserted on the BOX *and* CLASSIC rows, plus `in.z == p.x`); its dump line gains an `in.z` column; `judge-s1-coverage.js`'s raw-keysym sidecar pairing (K_A now pairs with `i.x`, K_B with `i.a`, K_Y with `i.b`, K_X with `i.z`) and its `i.z`-never-set invariant; and the Controls screen's label table, whose four face rows are now style-independent (`foh_ctl_labels.h` + the 54-row pin in `check-foh-flows.sh` leg [0m]) — **the screen and the plane moved in the same commit, so the screen never lies**. Asserted by `check-ctl-input.sh` leg [3e] (one press at a time, every style, plus a standalone "BOX GRABS" assertion) with teeth **t6-nograb** and **t7-facescramble** | §9.3, §12.1 |
| D35 | **A43, owner-reported P1 (2026-08-24):** leaving the CSS RELEASES every token and re-homes it on the character its port chose (`css_back`, `port/foh/foh.c`). Owner: *"if you go to the CSS back button while selected as any other character except for falcon, and then come back in to the game, you are selected as falcon... it should remember the last character you were."* **The mechanism is a LEAK, not a re-draw**, and the filed diagnosis ("it goes to the nearest character") describes the symptom rather than the cause: every rest slot in `foh_css_token_pos` already homes on `cssChar[k]` (that is D21). Upstream's grab is module state cleared by exactly TWO arms — the A-drop (`css.js:228-232` and siblings) and the leave-band drop (`css.js:341-347`) — and the back-out at `css.js:186-194` is neither: it calls `changeGamemode(1)`, whose CSS case is `drawCSSInit()` alone (`main.js:571`), and `menu.js:106`'s re-entry adds only `positionPlayersInCSS()`, which moves the four SIM players (`main.js:528-535`) and no token. So upstream re-enters its CSS with the token still on the hand, and this port carried that verbatim. **One B press in the roster band BOTH retrieves the token (`css.js:209-215`) and arms the 30-frame back counter** — upstream's deliberate overlap, §2.11 — so "press B to go back" *is* the grab. On re-entry the hover arm re-selects LIVE from the hand (`css.js:222-226`, the one site that writes BOTH planes), and D22 puts the BACK wedge at x > 184 / y < 26, **directly above roster cell 4**, so the next walk to BACK drags the carried token across FALCON and commits it. That is why it is always falcon: falcon is the cell under the wedge. Upstream's own wedge is an instant A-click on a 1200 px canvas whose roster ends ~300 px short of the hand's clamp, so the identical leak never drags a token over the last cell there — **the deviation exists because D4's screen and D22's wedge give upstream's leak somewhere to land.** **The SELECTION PLANE IS NEVER WRITTEN by this arm**, deliberately: the hover already committed a choice, and clearing it on the way out would trade a wrong display for a silently discarded pick. Scope is D21's exactly — where tokens are DRAWN and hit-tested. The rest slot is re-homed for BOTH ports, not only the carried one, so a re-entry always shows the selection instead of whichever of §2.6's two Q1 formulas last applied. Proved by `port/foh/check-css-backsel.sh`, whose two negative tests delete D35's two statements one at a time: T1 (no release) must LOSE the pick to falcon, T2 (no re-home) must leave every plane clean and only mis-draw the pin | §2.6, §2.9, §2.11 |

| D34 | **A42, owner-reported defect 2026-08-24 — X GRABS FOR REAL:** physical **X emits Melee's Z CHORD, `a` + a LIGHT SHIELD on `lA`** (`S1_ZGRAB_LA = 49.0/140.0`, the B0XX/HayBox light-shield level cited from `docs/research/b0xx-mapping.md` §2.2), replacing D33's `in.z = p->x`. Owner: *"grab with X didn't work, nothing happens."* **D33's premise was a SOURCE COMMENT, and the comment was true of real Melee and false of this engine.** MEASURED over the whole sim: every reader of `MlInput.z` is `{FORWARD,UP,DOWN}SMASH.c`'s `i0->a || i0->z`, `action_state_shortcuts.c:522` (checkForAerials), and `physics.c:983` (lCancelUpdate) — so **`z` is an ALTERNATE ATTACK button and an L-cancel trigger, and it dispatches `GRAB` exactly ZERO times.** The five real grab arms are `GUARD.c:75` / `GUARDON.c:101` (`a` edge while shielding) and `DASH.c:80` / `RUN.c:60` / `KNEEBEND.c:66` (`a` edge **+** `lA > 0 || rA > 0`), so the chord was chosen to reach ALL of them: `WAIT.c:56` and `DASH.c:72` take their analog-shoulder arm into GUARDON, whose `init -> main -> interrupt` chain runs inside the SAME tick and still sees the `a` edge. **The bounds are load-bearing, not taste:** `> 0` or no grab arm fires at all; `< 1` or `GUARDON.c:21` powershields on every grab press. **NOTHING IS TRADED AWAY** — every `z` reader is an `a || z` or a `(a edge) || (z edge)`, and lCancel's third arm is an `lA` edge, so X keeps its alternate-attack and L-cancel roles by construction and `z` is dropped rather than kept as a second name for a bit `a` already sets. **What moved that was pinned:** `s1_sweep.c`'s 2048-combo invariants (`in.a == (p.b || p.x)`, `in.lA == p.x ? S1_ZGRAB_LA : 0`, `in.z` now never set) and its dump's `z` column, which becomes the `lA` column; and `judge-s1-coverage.js`'s grab signature and K_X sidecar pairing. **THE REAL DELIVERABLE IS THE CHECK, not the button:** `check-ctl-input.sh` gains leg **[4]**, `port/gfx/ctl_seam_witness.c`, which drives physical button -> real resolver -> **REAL `sim_game_tick`** -> the actionState the engine actually enters, for every role D33 moved (A->KNEEBEND, B->JAB1, Y->NEUTRALSPECIALGROUND, X->GRAB in all three styles, plus X while shielding, while dashing, and airborne->ATTACKAIRN) — because D33 shipped a dead feature past a GREEN bit assertion, and only an assertion on the ACTION could have caught it. Teeth **t6-nograb** and **t9-seamscramble** run against that witness; **t10-powershield** holds the upper bound. Leg **[5]** commits the emitted-vs-renderable vfx comparison (41 emitted names, 45 templates, difference empty) with tooth **T11**. **STILL OWED:** `foh_ctl_labels.h:48` reads `"GRAB (Z)"` and should lose the `(Z)`; it is `port/foh`'s to change (it is pinned by `check-foh-flows.sh` leg [0m]'s 54-row label table, which must move in the same commit) | §9.3, §12.1 |

| D39 | **A45 T1, 2026-08-24 — the share-code parser accepts a STRICT SUBSET of upstream's input language.** `parseStageCode` reads every number with `parseFloat` / `parseInt`, which coerce arbitrary junk into a stage full of NaNs rather than refusing it: measured this session by executing upstream's own transpiled `encode.js`, the fourteen-character input `"aaaaaaaaaaaaaa"` returns a *stage*, not `null`. `mlk_parse` (`port/sim/stage_code.c`) instead requires every coordinate token to match the alphabet `createStageCode` can emit — `-?\d+(\.\d{1,2})?`, with |hundredths| <= 2^53 — and rejects anything else, which is upstream's OWN "Invalid code" path (`targetselect.js:158-162`) reached on more inputs. **The reason is the iter-38 ban, not taste:** the SDK's static musl `strtod` mis-rounds subnormals and drops `-0`, so decimal parsing on this device is forbidden; integer hundredths / 100.0 is correctly rounded and needs no libc. **On every code `createStageCode` can emit the two agree BYTE FOR BYTE** — 3892 well-formed plus 108 non-fixed-point codes, `cmp`-exact against upstream's executed encoder — and the port additionally refuses loudly rather than truncating when a code exceeds a sim cap (`ML_MAX_SURFACES` and friends; design risk R2, owner ruling owed before A45 T2). Every divergent input is enumerated with both sides' verdicts in `port/sim/calib/expected-stage-code.json` (21 of 42 hostile rows), so the deviation is reviewable rather than asserted. Proved by `port/sim/check-stage-code.sh` -> `STAGECODE MATCH` | §7 |
| D38 | **A7, driver-allocated 2026-08-24 — THE CREDITS DRAW FROM A FOH-LOCAL RANDOM STREAM.** `credits.js` calls `Math.random` in three places: the star constructor and its respawn (`:252-255`, `:321-323`), a scrolling name's starting x (`:42`) and its wobble amplitude and direction (`:49`, `:51`). **In the browser those draws come off the SEEDED stream** — the same measured fact that makes the SSS RANDOM slot a registered refusal (`foh.h`, AGENT-LOG iter 93) — and the sim's stream carries a 465-draw boot pin whose POSITION every golden's checksums depend on. Sharing it would move every frozen stream by however many stars a player happened to watch, so the credits get a generator of their own: mulberry32 (the same ALGORITHM `port/sim/ml_rng.h` uses, with **no shared state and no shared header** — a cross-TU include would make the two planes look connected, which is the thing this separates), seeded from a compile-time constant, its entire state the single `FohState.credRng` field. **The FOH stays a pure function of (state, input)**, so the same flow still replays to the same pixels and `foh_app.c`'s "the FOH machine consumes no RNG" contract is amended in exactly one place rather than broken. **WHAT THIS DELIBERATELY IS NOT:** an authored table of star positions or an index hash. Those are values upstream DRAWS, and inventing them would be the deviation class *"we wrote down what upstream rolled"* landing in the one subsystem whose contract is no invented values; drawing them from a different generator is the smaller departure. Nothing observable outside the credits screen depends on it — no checksum, no flow edge, no launch record — and no committed flow lingers on the screen long enough for a star to matter | §8, §12.1 |
| D42 | **A45 T2, 2026-08-24 — CUSTOM TARGET STAGES ARRIVE AS A FILE ON THE SD CARD, NOT AS A PASTED CODE.** Upstream's `+ ADD CODE` slot exists to accept a ~1 KB string pasted into an HTML textarea (`targetselect.js:132-136`, `:156`). This device has no clipboard, no keyboard and no network, and its on-screen letter grid (D8) is blocked behind A14; entering a 1 KB code on a d-pad grid is on the order of a thousand presses. It DOES have an SD card the owner mounts. So a custom stage is **one file per slot** — `<persist dir>/custom<0..9>.mlstage`, three LF-terminated lines `MLSTAGE1` / the share code / `SUM <64 hex>`, the SUM being sha256 over every preceding byte, which is `foh_persist.c`'s own integrity idiom (`:154` emits it, `:218-222` isolates it) reused rather than a second one invented. Dropping a stage in becomes "copy a file onto the card", which is strictly better than a paste box on this hardware and **deletes the D8/A14 dependency** the spike had scheduled. The code inside the file is upstream's OWN `createStageCode` string, byte-for-byte (D39), so a stage still moves between this port and a browser session unchanged — the transport differs, the artifact does not. On-screen code *display* and *entry* remain possible later (A45 T9) and nothing here forecloses them. **VALIDATE ON READ, ALWAYS:** `/mnt` is journal-less vfat mounted `errors=remount-ro` and the device has a MEASURED power-loss history, so a truncated file is an expected input; `mlk_slot_load` bounds the read, requires the exact grammar, verifies SUM BEFORE parsing, then parses and validates, and every refusal names its rule — a corrupt file can never reach the sim. Proved by `port/sim/target/check-custom-stage.sh` -> `CUSTOM STAGE PLAYS` | §7 |
| D43 | **A45 T2, 2026-08-24 — THE CUSTOM-STAGE LIST DOES NOT CLOBBER. Owner ruling, verbatim: *"ok let's fix it like you say"*.** Upstream destroys a custom stage every time another is added. `targetselect.js:164-166` (the add path) and `:551-552` (the boot reload) both do `customTargetStages[customTargetStages.length - 1] = stage; setCustomTargetStages(customTargetStages.length, ...)`; **executed** in the A45 spike, adding A then B then C leaves `["B","C","C"]`. The cookies are written correctly, so the data survives on disk and is destroyed again on the next load. HARD RULE 5's tiebreak does not decide this mechanically — no golden covers this plane (`CHECKSUM.md` is players + articles), so no checksum stream can diverge either way — and the owner ruled to fix it: **it is user data loss, and it is menu bookkeeping rather than gameplay.** The fix is the value model, not a patch to two call sites: the custom plane is a **fixed array of ten slots addressed by INDEX**, with no append and no length cursor (`MLK_MAX_SLOTS`, `port/sim/target/custom_stage.h`). Slot *i* is the file `custom<i>.mlstage` and nothing else; writing slot *i* cannot reach slot *j*; there is no `length - 1` to be off by one; an empty slot is a NAMED refusal rather than a hole that the next stage shifts into. Both of upstream's clobber sites are therefore structurally absent instead of individually repaired — which matters, because fixing only the reported half (the add path) would have left the reload half live. Proved by `check-custom-stage.sh` leg [2]: three different stages published to slots 0, 1 and 3 — upstream's exact data-destroying sequence — all three sha-identical afterwards, slot 2 still empty and named | §7 |
| D40 | **A44, owner-reported P2 (2026-08-24); PART (b) RETIRED 2026-08-24 (A49) — see §2.7.** ONE HAND OWNS EVERY TOKEN, and ports 2/3 offer HMN or N/A. Owner: *"why can't I turn on player 3 and 4 at the CSS?"* **The obstacle was never the pixels** — A44's predecessor measured that `FOH_CSS_PANEL_{X0,PITCH,W}` = `{1,60,58}` already puts four panels on the 240 px screen, that `render_css` already loops `k = 0..3`, and that `css_ready` already counts all four ports. It was `sim_setup_match`, which pinned slots 2/3 absent by construction, and **A46 removed it**: `sim_setup_match_ports` is upstream's own four-port `harnessSetupMatch` loop, witnessed by the four-port golden `q01` (`STREAM MATCH 3600/3600 frames exact` through the UNCHANGED `verify-stream.js`). So DEVIATION D6 is RETIRED and this narrower pair replaces it. **(a) THE OWNERSHIP GUARD GOES.** Upstream's `playerType[j] == 1 \|\| i == j` (`css.js:300`) stops one human's hand taking another human's token, which is right where every human has a hand. Here there is one input device and one hand, so the guard protects nobody and instead removes the only way to give a HUMAN port a character. P2 could route around it through the CPU detour (§2.7); P3/P4 cannot, because (b) leaves them no CPU state — a port you can switch on and never give a character to is precisely the stub HARD RULE 2 forbids. Upstream's OTHER asymmetry is carried unchanged and widened with it: a port's token stays grabbable while nothing is drawn for it (§2.4's D4 exception (b)), which is upstream's own mismatch between this guard and its token draw. **(b) NO CPU ON PORTS 2/3.** Their cycle is N/A -> HMN -> N/A. AIBRIDGE1 is one recorded stream for one CPU slot, so a CPU P3 has no C-side replay and could not be checksum-verified — **an honest missing state beats a toggle that denies at START**, and the refusal is asserted on the LAUNCH plane too, not merely absent from the widget. **THE LAUNCH GUARD IS NOW THREE CONDITIONS**, each naming what is still unreal rather than a shape the screen dislikes: port 0 must be HMN (D17, on its amended ground), no CPU above port 1 (this deviation), and at least two PARTICIPATING ports — the third counts ports rather than testing two named ones, which is what makes P1 + P3 with P2 off a legal match, as it is upstream. **Persistence is untouched:** `FohPersist` carries no CSS character or type state at all (measured — it holds gameSettings, `ctlStyle`, `modOnR`, `targetRecords` and `bind`), so widening the screen needs no `MLFKPERSIST6`. Proved by `port/foh/check-css-p34.sh` -> `CSS P34 CHECK OK`, whose witness drives the real gestures and whose three teeth restore the ownership guard, write the selection plane by ROSTER INDEX instead of by PORT (the D21/D35 family), and collapse the launch config onto port 0; and by `port/foh/foh_launch_witness.c`, which judges all 81 cells of the four-port type grid against an authored verdict table | §2.4, §2.7, §12.1 |
| D41 | **A44, 2026-08-24 — THE TOKEN 2x2.** Four tokens do not fit one 44 px cell as a row: at `r` = 9 and pitch 20 they span 78 px, and `foh_css_token_pos`'s clamp would then park port 0's token a whole cell LEFT of the character it chose — **re-creating DEVIATION D21's defect** (the token is the only roster-level indicator on this screen) in the one configuration A44 exists to add. They move to upstream's own 2x2 instead, which is the layout this port's constant block has always said upstream draws: `r` 9 -> 7, column and row pitch 20 -> 14, `DX` 12 -> 15, `y` `CELL_Y+11` -> `CELL_Y+9`. The four inequalities those satisfy are written at `FOH_CSS_TOKEN_R` in `port/foh/foh.h` and each is load-bearing: the block is 28x28 inside 44x30, the discs are TANGENT so no point lies inside two tokens (which is what keeps the grab hit test unambiguous now that D40(a) makes all four grabbable), the A-drop slot needs no clamp at all, and the lower row spans 34..62 so it never bleeds into the READY TO FIGHT ribbon the renderer draws after it. **The shape did not change; the numbers did.** Consequence for the checks, handled rather than absorbed: `check-hand.sh`'s extraction differential masks exactly two dump columns (the token pixels and the framebuffer hash) and asserts the mask is LIVE — if the geometry is ever reverted, the unmasked comparison stops differing and the leg says so | §2.6 |
| D47 | **Owner ruling 2026-08-24 (round 4 item 4) — PUFF WALLJUMPS.** D20 gave "Everyone Walljumps" real effect but guarded it on DATA, and puff failed that guard, so the setting silently meant *everyone except puff*: all five characters carry `framesData` `WALLJUMP: 40`, but puff carries **no WALLJUMP ECB and no WALLJUMP animation** — upstream never authored boxes for a state puff cannot enter, and without the guard D20 died `ecb: unknown action state` one frame after the walljump fired. Owner chose **option 1**, reuse an existing puff ECB rather than author new geometry: *"option1 please for puff wall jump. falling seems like it would be best?"* The source state is **WALLTECHJUMP**, and that is a MEASUREMENT, not a resemblance — for **all four** characters that HAVE a WALLJUMP ECB (marth, fox, falco, falcon) the WALLTECHJUMP ECB is **byte-identical** to it, 40 frames and all 160 coordinates, four independent instances. Upstream's own data says a wall-tech-jump ECB *is* a walljump ECB, so the reused number is the one upstream would have authored. Puff HAS WALLTECHJUMP (45 ECB frames, against the 40 that `framesData["WALLJUMP"]` clamps the index to) and HAS a WALLTECHJUMP animation, so `port/gfx/gfx_render.c` aliases the pose the same way rather than leaving puff **invisible** for 40 frames. **FALL — the owner's own hypothesis — is REJECTED, and not on taste:** puff's FALL ECB is 8 frames, so walljump frame 9 would trap `ecb frame out of range`. It is mechanically impossible, and it also ranked only 7th when marth's states were ranked by distance to marth's WALLJUMP ECB (WALLTECHJUMP ranked 1st, at distance exactly 0). **HARD RULE 5 holds:** the numbers still come from the executed-data pipeline unchanged — only the STATE NAME used to reach them deviates. The alias therefore lives at the deviation site (`port/sim/physics.c`'s `walljump_ecb`, the single owner of "which ECB does a walljump use", called by BOTH the ability gate and the per-frame lookup so they cannot drift), **never in the generated tables**: `pipeline/check-tables.sh` round-trips the generated C against a fresh executed-JS walk, so a hand-added puff WALLJUMP ECB would be erased by the next `node pipeline/run.js` *and* would be a house rule disguised as upstream data. Defaults OFF forever; flag-off is bit-identical (`SIM CONFORMS` 8/8). Proved by `port/sim/check-walljump-d47.sh`, which re-asserts every measurement above from the freshly generated CTAB1/ANIM1, proves its instrument on a POSITIVE first (fox walljumps on the witness trace with no house rule at all) before trusting any null, and then witnesses puff — and marth, which **closes #16**: the D20 marth path had never once executed | §3.3 |
| D45 | **A49, owner-reported P1 (2026-08-24) — THE CSS SELECTION PERSISTS ACROSS RESTARTS.** Owner, verbatim: *"i want to MAKE it persistent please. right now it just puts the cursor back where it was when you left. I want it to be the last character."* **MEASURED FIRST: `FohPersist` carried NO character or port-type state at all** (gameSettings, `ctlStyle`, `modOnR`, `targetRecords`, `bind`, volumes, `tapJumpOff`), so a pick had NEVER survived a restart on ANY port — this is a NEW feature, not a regression repaired, and A44's "no bump needed" note was right about the measurement and wrong about the need. Upstream cookies no character at all (`getGameplayCookies` reads gameSettings and nothing else), so PERSISTING is the deviation. **`MLFKPERSIST6`**, one appended `sel <c> <c> <c> <c>` row after the v5 `bind` block, 69 lines; v1..v5 all MIGRATE (a v5 file has no opinion about characters, so every port takes the fresh-install marth and every setting and all 50 target records carry forward). **ONLY the SELECTION plane is stored.** The TOKEN plane is a VIEW of it and is re-homed FROM it at boot — storing a view beside the thing it views is CONTEXT.md's costliest defect class and is what D21/D35/D46 each were. **The PORT TYPES and CPU LEVELS are deliberately NOT persisted**, answered as a design question rather than skipped: restoring them would boot the CSS already READY TO FIGHT off a configuration the player last saw in another session, upstream's own fresh state is `playerType = [-1,-1,-1,-1]` (`main.js:107`) with `addPlayer` arming port 0 (`main.js:495`), and since A49 also made CPU reachable on ports 2/3 it would make an unverified configuration a device's DEFAULT BOOT STATE. Save point = LEAVING THE CSS, by either exit, through the ONE shared predicate `foh_is_save_point` (`foh.h`) both drivers now ask — not the hover arm, which re-selects live on every frame the cursor crosses a cell and would put an SD write inside a drag. Proved by `port/foh/check-css-p34.sh` legs [9]/[T6] | §2.4, §3 |
| D46 | **A49, owner-reported P1 (2026-08-24) — A RELEASED PIN RETURNS TO THE CHARACTER THAT PORT SELECTED.** Owner: *"whenever the pin is let go of (going off) it should go back to the character you selected."* **This is the D21/D35 family's THIRD instance**, and in all three the token was re-homed from something that was NOT the selection: D21 from the PORT INDEX (`charIconPos[k]`), D35 from a leaked GRAB, D46 from a PIXEL FORMULA. Upstream's leave-band rest slot (`css.js:337`) has a different base AND a different pitch from its A-drop slot (`css.js:288`); MEASURED that is a 99 px offset on a 95 px cell pitch, i.e. **one whole cell right of the character just selected** — and `foh_cssbacksel_witness.c` leg [B] had been asserting exactly that as a PRECONDITION since A43. It now homes on the selection. **`foh_css_token_pos` states the rule ONCE for all three rest paths** rather than three arms agreeing, because the failure mode this screen keeps having is one of the three drifting; `cssTokenRest` survives as the record of WHICH path (the witnesses read it to prove they reproduced their precondition), and D41's clamp went with the formula — every base is now `cell_x(c) + DX`, widest case 226 < 240, so a clamp could not fire. **DISARM NAMED, not absorbed:** D46 subsumed the display half of D35, so `check-css-backsel.sh`'s T2 (which deleted D35's rest re-home) stopped biting; it was moved onto the line that now carries the outcome, per CONTEXT.md's rule that a tooth asserts the OUTCOME protected. Proved by `check-css-p34.sh` [8]/[T5], `check-css-backsel.sh` [B]/[T2], `check-css-token-rest.sh` [T1] | §2.6 |
| D50 | **A45 T4, 2026-08-25 — THE TARGET BUILDER'S CONTROL MAP, REBOUND FOR A DEVICE WITH NO `z` AND A d-pad THAT IS ALREADY THE CROSSHAIR.** Upstream's builder (`targetbuilder.js:159-855`) reads FIVE inputs the FunKey-S cannot supply as-is. MEASURED against CLAUDE.md's device keysym list (`u/d/l/r`, `a/b/x/y`, `s`, `k`/`n`, `q`) — there is **no `z` button at all**, and `z` is what upstream binds the GRID cycle to (`:207-212`). Three rebinds, each with its measurement: (a) **the grid moves from `z` to Y.** Upstream's precision modifier is `(y || x)` (`:171`) — TWO buttons doing ONE job, so one of them is free by upstream's own redundancy, and X keeps the modifier. (b) **the tool cycle keeps L/R and DROPS upstream's d-pad-left/right alias** (`:213-230` accepts both): the d-pad IS the crosshair here, so the alias is not a choice. (c) **B LEAVES THE BUILDER**, taking the same edge and the same sound as the pause menu's Quit (`:832-835` `changeGamemode(1)`). Upstream's builder has NO B arm; B is the back button on every other screen in this machine (`menu.js:164-190`, `css.js:186-194`, `stageselect.js:79`, `targetselect.js:76-81`), and a screen whose back button silently does something else is a trap found by falling into it. ONE destination, two ways of pressing it — which is why the judge gains one `target-builder>menu-top>b` edge, not two. Also here because it is the same class: upstream's `Test stage` pause row is **NOT PORTED** (scope, not a stub — `SAVE` then play the slot from Target Test is the same journey and needs no second launch path), and `DELETE` takes its place, because upstream binds delete to `z` on target-select (`targetselect.js:82`) and that button does not exist. Proved by `port/foh/check-tbuild.sh` legs [3]/[T5] | §11 |
| D51 | **A45 T4, 2026-08-25 — A NEW CUSTOM STAGE STARTS FROM A FLOOR.** Upstream's fresh `stageTemp` (`targetbuilder.js:53-72`) has NO ground, ceiling or walls: you draw them with the WALL and POLYGON tools, which are the design spike's **T5 and T7 and are not in this ticket**. A targets-only editor starting there could only ever emit a stage with nothing to stand on — the player spawns, falls through the blastzone, and the "stage" is unplayable. That is not a faithful port of a feature, it is a feature that cannot be used, so a new document starts from a minimal playable template: **upstream's own literals for everything except one ground line** — `startingPoint` `[(-10,0),(10,0),(-30,0),(30,0)]` (`:62`), `blastzone` `Box2D([-250,-250],[250,250])` (`:64`), `scale` 3 (`:65`) — plus a single 200-unit ground surface at y=0. When T5/T7 land, the honest move is to KEEP it as the default and let the player delete it, not to go back to an empty stage. Site: `doc_template` in `port/foh/foh_tbuild.c`; the witness plays a stage built on it end to end (`check-tbuild.sh` leg [7]) | §11 |
| D52 | **A45 T3, 2026-08-25 — TARGET-SELECT'S ELEVENTH SLOT FLIPS A PAGE INSTEAD OF REFUSING, AND THE TEN CUSTOM SLOTS SHARE THE AUTHORED GRID.** Upstream draws the authored stages and the custom stages SIDE BY SIDE, in four columns of a 1200 px canvas (`targetselect.js:196-206`, `:288-294`), with `+ Add Code` at the head of the custom column. At 240 px the grid is two columns of five (`foh_tss_slots`, D29's own table), so four columns do not exist to have. The same ten rects therefore carry whichever family is on show and **slot 10 — which used to emit `ev_refused(s, "addcode")` — toggles between them**, which is the job the design spike named for it once **D42** had already replaced upstream's paste-a-1-KB-code-into-a-textarea transport (`:132-136`) with a file on the SD card. The page is VIEW state and **emits no event**, exactly like `tssCursor` and the hand position; what it makes observable is the LAUNCH, so `TLAUNCH`'s `tstage` domain widens from 0-9 to **0-19** — `MLK_PLAYING_BASE + slot`, which is upstream's OWN numbering at `:140-146` (`setActiveStageCustomTarget(targetSelected-10)` but `setTargetStagePlaying(targetSelected)`). **D43 IS PRESERVED WHOLE:** all ten slots are drawn whether or not they loaded, an absent or corrupt one is drawn DIM WITH ITS REASON in its own place, and nothing is ever compacted or shifted — upstream's delete arm shifts every higher cookie down (`:83-97`), which would make "Custom 4" mean a different stage than it did a second ago. Proved by `check-tbuild.sh` leg [7] and `check-hand.sh` leg [7] | §11 |
| D54 | **A45 T5/T6, 2026-08-26 — THE WALL AND DAMAGE TYPE CYCLES MOVE FROM THE d-pad TO X + SHOULDER.** Upstream cycles `wallType` (`targetbuilder.js:231-248`) and `damageType` (`:249-267`) with **d-pad up/down**, and — unlike the SCALE tool three rows below it (`:172-174`) — it does **not** freeze the crosshair while doing so. On this device the d-pad IS the crosshair (D50, D1), so the two cannot coexist and one of them has to move. **X is already the precision modifier** (`:171`, narrowed from upstream's redundant `y || x` to X-only by D50), so holding it turns the shoulders from the TOOL cycle into the TYPE cycle: **L/R alone still cycles the tool, X+L/X+R cycles the active tool's type.** Nothing new is bound, X keeps its d-pad role, and the tool line names the live type — an invisible modal type on a 240 px screen is the same class of trap as the refusal-that-is-only-a-sound this ticket was filed about. **The rejected alternative is recorded because it is the obvious one:** freezing the crosshair the way SCALE does would have cost precision movement on exactly the two tools (WALL, DAMAGE) whose targets are single surfaces. Upstream also accepts d-pad left/right for the TOOL cycle (`:213`, `:221`); the port cannot, for the same reason, and that half was already D50's. Proved by `port/foh/check-tbuild.sh` leg [9] (`X+R cycles the wall TYPE, not the tool` and `R WITHOUT X still cycles the tool`) | §11 |
| D55 | **A45 T8, 2026-08-26 — SCALE KEEPS UPSTREAM'S d-pad, BECAUSE UPSTREAM ALREADY FROZE THE CROSSHAIR FOR IT. Registration only: nothing is changed.** `targetbuilder.js:172-174` is `if (targetTool === 8) { multi = 0; }` — the crosshair does not move while the SCALE tool is active, so d-pad up/down is free **by upstream's own construction** and the tool is carried verbatim. This row exists because the conflict was real and had to be checked rather than assumed, and because the mechanism is load-bearing and invisible: **MEASURED, the freeze was MISSING from the first T8 implementation and only the witness noticed**, so it now carries its own tooth (`check-tbuild.sh` [T7]) rather than living as a one-line coincidence. The sibling deviation D54 exists precisely because WALL and DAMAGE have no such freeze | §11 |
| D56 | **A45 T7, 2026-08-26 — B POPS A POLYGON VERTEX WHILE ONE IS BEING DRAWN, AND LEAVES THE BUILDER OTHERWISE. This NARROWS D50.** D50 made B the builder's back edge because B is the FOH's universal back button and a screen where it silently does something else is a trap. Upstream's POLYGON tool binds B to *pop the last vertex* (`targetbuilder.js:396-408`) — and does so under its own guard, `if (amDrawingPolygon)`. The two are therefore not in conflict and the port does not have to choose: **upstream's guard decides.** While a polygon is open B pops (and abandons the whole shape at `<= 2` points, `:398-401`); with no polygon open B leaves, on the same edge and the same sound as the pause menu's QUIT. That is also the reading a player expects — a half-drawn polygon is unfinished work, and backing out of it before backing out of the screen is what a back button is for. Proved by `check-tbuild.sh` leg [9] (`B while drawing does NOT leave` / `it pops a vertex and keeps drawing`) | §11 |

---

## §13. Recommended implementation order (QUARANTINED — historical)

> **QUARANTINED. Nothing in §13, §13.1 or §13.2 is binding, current, or
> safe to plan against.** This is the ordering and the consequence analysis
> proposed AT AUDIT TIME, retained only as the record of that reasoning.
> Substantial parts of it have since shipped and other parts were overtaken
> by owner rulings, so rather than maintain a list of which sentences are
> stale — an enumeration that has now been wrong twice — the whole section
> is quarantined wholesale.
>
> **For current state, read §12's "what has closed since" table and the
> per-screen sections.** For remaining work, read that table's open rows.
> Do not derive schemas, field sets or file formats from anything below:
> the `LAUNCH` line and `FohPersist` have both moved on (v2), and several
> "to plan for" consequences were resolved differently than proposed.

Sized **S** (< ~1 task), **M** (~1 task), **L** (multi-task). Ordered by
owner-visible value per unit of work, with dependencies respected.

| # | Item | Size | Why here |
|---|---|---|---|
| 1 | **CSS free hand cursor** — replace the row cursor with a clamped 2D cursor in doubles (§2.2), plus a widget hit-test table (D4). No new widgets yet; re-point the existing ones at the cursor. | **M** | Every remaining CSS item depends on it. This alone converts the CSS from "a list" to "the screen he remembers". |
| 2 | **CSS token model** — grab with B (own) and A (own or CPU), carry, live hover-select, drop with A + announcer sound, leave-band silent commit (§2.4–2.6). | **L** | The headline complaint: *this is the only way upstream lets you pick a character.* Do not ship 1 without 2 — a free cursor with nothing to grab is worse than the row list. |
| 3 | **CSS port types** — clickable HMN/CPU/N-A box per port, `togglePort` 3-cycle, **P1 may be CPU** (§2.7). | **M** | Second-named complaint. Needs the cursor (1) for the click target. Adds `portType[4]` where only `p2Type` exists. |
| 4 | **CSS ready/launch** — `readyToFight` from ≥2 non-N/A ports AND no token held; START gated on it; banner reflects it (§2.10). | **S** | Small, and it is what makes 2 and 3 legible — the banner becomes the feedback channel for the whole screen. |
| 5 | **Gameplay options completion** — add `Flash on L-Cancel` and `Everyone Walljumps`, restore upstream row order, wrap up/down, persist `phantomThreshold`(0.01)/`blastzoneWrapping`/`dustLessPerfectWavedash` hidden (§3). | **S** | Explicitly named by the owner. Note `everyCharWallJump` wires to **nothing** — that is the faithful outcome, do not invent a rule. |
| 6 | **Audio options screen** (§4). | **S** | Two doubles, four inputs, one persist pair — the cheapest whole screen in the spec, and a currently-`refused` row becomes real. |
| 7 | **CSS secondary widgets** — palette cycle on X/Y (0..6), mode ribbon toggling `versusMode`, clickable BACK chevron, random/clear name tags (§2.9, D8). | **S** | All are cursor + A once (1) lands. `versusMode` is sim-visible, so it needs the LAUNCH line extended — see §13.1. |
| 8 | **SSS free cursor** — clamped pointer (D10), sticky hover, persistent position/selection, RANDOM resolving off-chain (D11) (§6). | **M** | Same paradigm as the CSS, so it reuses (1) wholesale; sequenced after the CSS because the CSS is what the owner played. |
| 9 | **Controls chooser destinations** — `Controller` shows upstream's `"Error: no controller detected"` and returns on B (§9.2). | **S** | Converts a `deny` beep into the honest upstream state. Answers most of "controls can't select controller/keyboard" for near-zero cost. |
| 10 | ~~**Credits** (§8) with the relative reticle (D12).~~ **LANDED 2026-08-24 (A7)**, plus DEVIATION D38. | **L** | A whole minigame: 14 `ScrollingText` objects, starfield, twin lasers with deferred hit resolution at `life == 15`, score HUD, dual exit. Real work, low frequency of use. |
| 11 | **Keyboard rebinder** reduced to 12 physical buttons (§9.3, D13). | **L** | Largest and least load-bearing — nobody remaps a 12-button handheld mid-session. Ship last or never; items 1–9 close the owner's report without it. |

### 13.1 Cross-cutting consequences to plan for

1. **The `LAUNCH` trace line grows.** It carries
   `p1= p2= p2type= difficulty= stage= turbo= lcancel= flashlcancel=
   walljump= tapjump= versus= p3= p4= p3type= p4type=`. **A44 appended the
   last four** (item 7's 3-/4-participant launches) and widened `p2type` to
   admit `-1`, because with ports 2/3 live a match can be P1 + P3 with P2
   absent. Remaining items add `p1type` and `palette[4]`. **Appended, never
   renumbered:** every field up to `versus=` keeps its name, position and
   domain, so a frozen line gains a fixed suffix and nothing else. Every
   frozen `port/foh/flows/*.expect` that pins a LAUNCH line is re-frozen in
   the same change — a deliberate, reviewed re-freeze, not drift — and the
   twin `BRIDGE-STATE` line grew the same four columns, read back out of the
   `GameState` so the crossing itself is judged.
2. **`FohPersist` grows.** It currently holds
   `{turbo, lCancelType, tapJumpOff[4], targetRecords[5][10]}`
   (`foh_persist.h:88-93`). Item 5 adds `flashOnLCancel`,
   `everyCharWallJump`, `phantomThreshold` (double, 0.01),
   `blastzoneWrapping`, `dustLessPerfectWavedash`; item 6 adds
   `soundsLevel`, `musicLevel` (doubles). The format already carries
   hex16 IEEE-754 doubles and a SHA-256 seal, so this is a field-list
   extension plus a version bump, and the existing loud reset-to-defaults
   on mismatch is the correct migration.
3. **Flow scripts must gain cursor motion.** `port/foh/flows/` currently
   drives discrete rows. Once the CSS and SSS are pointer-driven, a flow
   step is "hold direction N frames", so the flow format needs a repeat
   count and the frozen structural traces change shape. Land the format
   change with item 1, before items 2–4 pile onto it.
4. **`FOH_CURSOR_SPEED` is the one tuning knob** (D3). Everything else in
   this spec is a measured constant; that one number is a feel judgement
   the owner makes on hardware.

### 13.2 Suggested first slice

Items **1 + 2 + 4** as one arc: free cursor, full token model, and the
ready/launch rule. That is the smallest change that makes the owner's
sentence — *"the hand moves anywhere and clicks anything; B retrieves the
token back to your hand, then you drop it on any character"* — literally
true, and it is the slice where a half-delivery (cursor without tokens)
would be worse than not starting.

---

*End of MENU-SPEC. Every behavioural claim above carries a `file.js:NN`
citation into the upstream pin `27af171`; every departure carries a
**DEVIATION** id registered in §12.1.*
