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

**Quirk Q1 (carried verbatim):** the rest position here uses base `518` and
pitch `93`, while the A-drop path (§2.5) uses base `419` and pitch `95`.
The two disagree by a few px — a token dropped by leaving the band lands
slightly off the token dropped with A. Upstream authored inconsistency;
faithfulness > tidiness.

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
never launch. When the netplay flag lands, restore the 4-cycle verbatim.

**DEVIATION D6 — ports 3 and 4 stay N/A.** Their panels render (upstream
draws all four, `css.js:881-882`) but their type boxes do not toggle.
Justification: every frozen golden and the whole `check-sim.sh` conformance
surface is 2-player; a 3- or 4-participant launch is unverified against the
oracle. This is a *launch-plane* limitation, not a menu-plane one — when
3/4-player conformance is proven, delete this deviation and nothing else.

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
| Mode ribbon | `y ∈ (100,160)`, `x ∈ (380,910)`, A edge | `css.js:390-395` | `setVersusMode(1 - versusMode)` |
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
you keep holding, you exit at frame 30 while still nominally carrying. The
counter is per-port and resets on release. The CSS also has the BACK
chevron (§2.9) as an A-clickable exit to the same place. A max-of-all-ports
`bHold` drives an on-screen exit indicator (`css.js:736-740`).

### 2.12 CSS gap table

| # | Behaviour | Spec | Our port today | Gap |
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
| 16 | BACK chevron clickable | `css.js:358-363` | Drawn but unreachable (`foh_render.c:1352-1356`) | **MISSING** |
| 17 | Palette cycle X/Y, 0..6 | `css.js:366-389` | Not implemented | **MISSING** |
| 18 | Mode ribbon toggles `versusMode` | `css.js:390-395`, sim-visible | Not implemented; `versus=0` hardcoded in LAUNCH | **MISSING** |
| 19 | Name tags | random / clear / type (`css.js:415-439`) | Not implemented | **MISSING** (type-tag cut by D8) |
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

**So we are missing rows 2 and 3, and our row order is wrong.** Our port
renders `TURBO`, `L-CANCEL`, `TAP JUMP OFF` (`foh_render.c:1688-1708`) —
three of five, with Tapjump promoted from index 4 to index 2.

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
| **`everyCharWallJump`** | **NONE — dead setting** | zero readers anywhere in `src/` |
| `tapJumpOffp1..4` | **sim** (checksum) | `actionStateShortcuts.js:507,516,520`; `fox/moves/DOWNSPECIALAIR.js:138` |
| `phantomThreshold` | **sim** (checksum) | `hitDetection.js:335,337,348`; `physics.js:1039-1040` |

**"Everyone Walljumps" is a dead toggle upstream.** It is written by the
menu and displayed, and nothing ever reads it. Implementing it faithfully
means: add the row, persist the 0/1, and wire it to nothing. That is the
correct and cheapest outcome — the owner noticed its absence because it is
*visible*, not because it changes play. Do not invent a walljump rule to
give it meaning; that would be a faithfulness violation.

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
| 3 | Row 2 `Flash on L-Cancel`, 0/1 | absent | **MISSING** |
| 4 | Row 3 **`Everyone Walljumps`**, 0/1 | absent | **MISSING** |
| 5 | Row 4 `Tapjump off`, 4 columns | present but at index 2 | **DIFFERS** (order) |
| 6 | A changes value; L/R move column only | matches | **MATCHES** |
| 7 | Up/down wrap | clamped, no wrap (`foh.c:436-485`) | **DIFFERS** |
| 8 | `phantomThreshold` persisted at `0.01`, hidden | not in `FohPersist` (`foh_persist.h:88-93`) | **MISSING** |
| 9 | `blastzoneWrapping`, `dustLessPerfectWavedash` persisted at `0`, hidden | absent | **MISSING** |
| 10 | B saves all keys then → main menu | B → menu-options (`foh_app.c:559-563`) | **MATCHES** |

---

## §4. Audio options (gameMode 10)

Entirely **MISSING** from our port — `menu-options` row 0 emits
`deny` + `refused audio` (`foh.c:185-188`).

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
| 6 | Repeat cadence 1-then-every-10 | not implemented (single-step per edge) | **DIFFERS** |
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
port: this spec does **not** cover `targetselect.js`, because the owner's
report did not raise target test and the file was not read for this pass.

**Registered as an open item**, not as a gap: before target-select is
called faithful, `src/stages/targetselect.js` must get the same treatment
the screens above got. Until then our implementation stands as-is and is
marked **UNVERIFIED**.

---

## §8. Credits (gameMode 13)

The owner reports "credits doesn't work". Measured: it is not implemented
at all in our port (`foh.c:197-200` emits `deny` + `refused credits`), and
upstream's version has a real dependency problem on this hardware.

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
| 1 | Screen exists at all | absent, `refused credits` | **MISSING** |
| 2 | 14 scrolling names, shootable | — | **MISSING** |
| 3 | Reticle | — | **MISSING** (D12 changes the drive) |
| 4 | A fires, 8-frame cooldown + 1-deep buffer | — | **MISSING** |
| 5 | X/Y laser colour | — | **MISSING** |
| 6 | START/L/R fast-forward | — | **MISSING** |
| 7 | B exits; 2500-frame auto-exit | — | **MISSING** |

---

## §9. Controls — device chooser, keyboard rebinder, controller calibration

The owner reports "controls can't select controller/keyboard". Measured
topology and disposition:

### 9.1 The device chooser IS implementable and IS missing

Page 3 of `menu.js` (`menuMode = CONTROLLERCALIB`) titled `"Controls"`,
rows `["Controller", "Keyboard"]` (`menu.js:22-23,29,31-32`). It is not a
separate module — it is drawn by the same generic `drawMainMenu` and
operated by the same `menuMove` (A confirm, B back, up/down cycle). Our
port already renders exactly this page (`foh_render.c:37`) but **both rows
refuse** (`foh.c:202-207`).

So the *chooser itself* — the thing the owner named — is a plain 2-row
menu page we already draw, and only its two destinations are hard.

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
a `deny` beep. That is faithful (it is upstream's literal string for this
condition) and it answers the owner's complaint without building a
calibrator for hardware that does not exist.

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
- Exit goes to gameMode 1 (main menu), **not** back to the Controls
  chooser.

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
| 2 | Row 0 → controller configurator | `deny` + `refused controller` | **DIFFERS** (should show `"Error: no controller detected"`) |
| 3 | Row 1 → keyboard rebinder | `deny` + `refused keyboard` | **MISSING** |
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
| 1 | **Controller calibration** (`controllermenu.js`, `gamepadCalibration.js`) | Mouse-only DOM/SVG interaction (`controllermenu.js:236-242`); gameMode 14 runs **no** control function (`main.js:947-949`); hard-requires `navigator.getGamepads` (`gamepad.js:9-11`). No mouse and no gamepad exist on the device. Replaced by upstream's own `"Error: no controller detected"` state — §9.2. |
| 2 | **Keyboard rebinder's modifier and range item types** | Types 1 and 2 scale an analog stick that does not exist; upstream's own UI already leaves `cstick.ranges` unreachable (`settings.js:22`, no `keymapItems` entry). §9.3 / D13. |
| 3 | **Target builder** (gameMode 4, `menu.js:87-90`) | A level *editor*. No text entry, no mouse, no file surface on the device. Row stays visible and refuses, matching upstream's own P2P-style dead row. |
| 4 | **Netplay: Spectate, P2P, Server** (`menu.js:108-121`) | Requires the `streamclient` stack, a server, and `inServerMode`. Owner ruling: **hide behind a named flag.** See §11.1. Note P2P is *already* dead upstream — its body is commented out (`menu.js:114-116`) — so P2P specifically is faithful when it does nothing. |
| 5 | **CSS free-text name tags** | jQuery HTML `<input>` overlay (`css.js:438`) committed by `keys[13]`. No DOM, no keyboard. Random-tag and clear-tag survive — D8. |
| 6 | **CSS NET port type** | Consequence of #4: a reachable NET state could never launch. D5. |
| 7 | **3- and 4-participant launches** | Menu-plane support is specified (four port panels exist); the *launch* is gated because every frozen golden and the whole `check-sim.sh` surface is 2-player. D6 — a launch-plane limit, deleted when 3/4-player conformance is proven. |
| 8 | **`keytest.js` as a screen** | It is a lookup table, not a screen — no gamemode, nothing reaches it (§9.4). |
| 9 | **`blastzoneWrapping`, `dustLessPerfectWavedash` UI rows** | Upstream has no row for them (their label entries are `""`, `css.js:85`/`:87`) and **zero code reads them**. They are persisted at their defaults and never shown — §3.2. |
| 10 | **Target select semantics** | Not measured this pass; `src/stages/targetselect.js` was not read. Our screen stands as **UNVERIFIED** rather than being declared a gap — §7. |
| 11 | **Menu screens on the checksum surface** | Menus are not on the oracle surface at all. Two upstream input bugs (Q4 malformed diagonal guards, Q5 the 60 Hz left-repeat) are therefore *not* reproduced — D9. Faithfulness binds semantics, not typos that only degrade input. |

### 11.1 The netplay flag requirement

Owner ruling: netplay stays, hidden behind a **named** flag. Concretely:

- The three Battle-Mode rows (`Spectate`, `P2P`, `Server`) and the CSS
  `NET` port type are compiled behind one named build flag,
  e.g. `MLFK_NETPLAY`, **off by default**.
- With the flag off, the rows must remain *drawn* — upstream draws them
  unconditionally (`menuCount[MPMENU] = 4`, `menu.js:31`) and disables P2P
  by gutting its body, not by hiding it. Selecting one refuses.
- With the flag off, `togglePort` runs the 3-cycle of D5. With it on, the
  upstream 4-cycle is restored verbatim and nothing else changes.
- No netplay symbol may be referenced from a frozen-list TU — the
  constructor-installed pointer-seam pattern already established for
  `ml_sim_runai_live` (`port/sim/sim/sim_ai_live.c`) is the precedent.

---

## §12. Gap totals

Counted from the per-screen tables above. "Intentional" = a **DEVIATION**
this spec deliberately mandates, not debt.

| Screen | MATCHES | DIFFERS | MISSING | Intentional |
|---|---:|---:|---:|---:|
| **CSS** | **0** | **8** | **11** | 2 |
| Gameplay options | 4 | 2 | 4 | 0 |
| Audio options | 0 | 0 | **4** (whole screen) | 0 |
| Main menu + submenus | 4 | 3 | 0 | 0 |
| SSS | 3 | 5 | 0 | 1 |
| Credits | 0 | 0 | **7** (whole screen) | 0 |
| Controls family | 3 | 1 | 1 | 0 |
| Splash / title | 4 | 2 | 1 | 0 |
| Target select | — | — | — | UNVERIFIED |
| **Total** | **18** | **21** | **28** | 3 |

The distribution matches the owner's report exactly: the menus he could
*navigate* score well (main menu 4/3/0, splash+title 4/2/1), and the screens
he tried to *use* score badly — CSS is **0 MATCHES out of 19 behaviours**,
and two whole screens do not exist.

### 12.1 Deviation register

| # | Deviation | Section |
|---|---|---|
| D1 | d-pad supplies `lsX/lsY ∈ {-1,0,+1}` | §1.2 |
| D2 | `du/dd/dl/dr` unbindable; drops the CSS d-pad-up launch and d-pad-right-Falco arms | §1.2, §2.10 |
| D3 | Cursor speed as a fraction of screen + `FOH_CURSOR_SPEED` knob | §1.2 |
| D4 | Hit regions are our layout's drawn rects, not upstream pixel coords | §1.2 |
| D5 | CSS port-type cycle drops NET (3-cycle) until the netplay flag lands | §2.7 |
| D6 | Ports 3–4 stay N/A until 3/4-player conformance is proven | §2.7 |
| D7 | CPU knob is grab-drag at the §1.2 cursor speed | §2.8 |
| D8 | Name tags: keep random + clear, cut free-text entry | §2.9 |
| D9 | Do not reproduce the malformed diagonal guards / 60 Hz left-repeat | §3.4 |
| D10 | Clamp the SSS cursor to the screen | §6.3 |
| D11 | SSS RANDOM resolves off the seeded chain | §6.5 |
| D12 | Credits reticle becomes a relative cursor | §8.3 |
| D13 | Rebind 12 physical buttons, not 56 keyboard slots | §9.3 |

---

## §13. Recommended implementation order

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
| 10 | **Credits** (§8) with the relative reticle (D12). | **L** | A whole minigame: 14 `ScrollingText` objects, starfield, twin lasers with deferred hit resolution at `life == 15`, score HUD, dual exit. Real work, low frequency of use. |
| 11 | **Keyboard rebinder** reduced to 12 physical buttons (§9.3, D13). | **L** | Largest and least load-bearing — nobody remaps a 12-button handheld mid-session. Ship last or never; items 1–9 close the owner's report without it. |

### 13.1 Cross-cutting consequences to plan for

1. **The `LAUNCH` trace line grows.** It currently carries
   `p1= p2= p2type= difficulty= stage= turbo= lcancel= tapjump= versus=`
   (`foh_app.c:577-586`). Items 3, 5 and 7 add `p1type`, `flashlcancel`,
   `walljump`, `palette[4]` and make `versus` live. Every frozen
   `port/foh/flows/*.expect` that pins a LAUNCH line must be re-frozen in
   the same change — that is a deliberate, reviewed re-freeze, not drift.
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
