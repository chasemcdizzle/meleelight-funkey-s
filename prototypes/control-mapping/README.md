# FunKey control-mapping prototype (wayfinder #9)

Browser fork of meleelight where the keyboard stands in for the FunKey-S's
physical controls (all digital: d-pad, A/B/X/Y, L/R, Start, Fn), routed
through the digital→analog mapping layer proposed in
`docs/research/b0xx-mapping.md` (branch `research/b0xx-mapping`). Purpose:
FEEL whether dash, walk, tilts-vs-smashes, short/full hop, wavedash and DI
work under the proposed layout before any device work.

## Run

```sh
# clone upstream (once) — MIT, private use only
git clone https://github.com/schmooblidon/meleelight /path/to/meleelight

# build (docker node:8) + serve on :8765 — one command
./serve.sh /path/to/meleelight

# open
http://localhost:8765/dist/meleelight.html          # scheme S1 (default)
http://localhost:8765/dist/meleelight.html?scheme=s2  # or s3
```

`serve.sh` applies `funkey-mapping.patch` to the clone if needed, npm-installs
and builds inside `docker node:8` (the proven recipe from
`spikes/determinism`), then serves the clone root with `python3 -m
http.server 8765`. The page must be served from the clone ROOT (the dist html
references `../src/input/gamepad/includeGamepadSVG.js`).

The only expected console noise is a 404 for `/favicon.ico` (upstream).

## Keyboard → FunKey → function legend

Arrows = d-pad under the right hand; the WASD diamond mirrors the FunKey face
buttons (X top, Y left, A right, B bottom) under the left hand; Q/E = L/R
shoulders. Edit the `KEYBINDS` table at the top of `funkeyMapping.js`
(= `src/input/funkey/funkeyMapping.js` in the clone) to rebind.

| Key | FunKey button | S1 (default) | S2 | S3 |
|---|---|---|---|---|
| Arrow keys | d-pad U/D/L/R | stick | stick | stick |
| W | X | jump | jump | jump |
| A | Y | **C-stick layer (hold)** | shield | **grab (Z)** |
| D | A | attack | attack | attack |
| S | B | special | special | special |
| Q | L | **Mod** | Mod X | Mod |
| E | R | shield | Mod Y | shield |
| Enter | Start | start/pause | start/pause | start/pause |
| F | Fn | (reserved by FunKey OS — unused) | — | — |
| F8 | *(lab only)* | cycle scheme s1→s2→s3 | | |
| F9 | *(lab only)* | toggle debug overlay | | |

S2 extras: hold **both** shoulders (Q+E) for the C-stick layer.
S1/S3 grab = shield+A (E then D) — native meleelight behavior.

### Technique cheat-sheet (S1)

| Technique | Input | Coordinate emitted |
|---|---|---|
| Dash / f-smash | tap →(+A) | ±1.0 flick |
| Walk / f-tilt | Q + → (+A) | 0.6625 |
| U-tilt / d-tilt | Q + ↑/↓ (+A) | ±0.5375 |
| Short vs full hop | W tap vs hold | timing (authentic) |
| Wavedash 45° | W, then E + ↓-diag | (±0.7000, −0.6875) |
| Wavedash ~30° | W, then Q + E + ↓-diag | (±0.6375, −0.3750) |
| Shield drop | E + ↓-diag on platform | lsY −0.6875 ∈ (−0.70, −0.65] |
| Spotdodge / roll | in shield: ↓ / ←→ | ±1.0 flick |
| DI | d-pad (diag = 0.7,0.7) | |
| C-stick aerial | hold A(key) + d-pad | cs ±1.0 / (0.7,0.7); drift stops |
| Grab / JC grab | E+D / W then E+D | shield+A |

## Debug overlay (top-right)

Live synthesized left-stick dot (green) and C-stick dot (orange) on a unit
square with meleelight's REAL threshold geometry — per-axis bands, not rings:
gray band = 0.28 deadzone, teal = 0.3 walk/tilt floor, red vertical = 0.79
dash/f-smash, orange horizontal = 0.66 u-smash/tap-jump, red dashed = −0.69
crouch, purple band = shield-drop (−0.70, −0.65]. Text shows held FunKey
buttons, active modifier, WHICH coordinate family produced the value, and the
final GC-out fields. This is how you see why a tilt came out as a smash.
Console API: `window.__funkey` (`setScheme("s2")`, `.state`, `.resolve(...)`).

## Getting into a match (all through the mapped layer)

1. Start screen: press **Enter** (joins keyboard as P1).
2. Main menu: **D** to confirm "VS. Melee" → "Local VS".
3. Char select: arrows move the hand; **S** (B) summons your token to the
   hand when over the character row; hover a portrait; **D** (A) drops it.
   To add a CPU: hover the P2 card's tab (below the portraits) and press D.
4. When "READY TO FIGHT" shows: **Enter**, pick a stage with **D**.

## Design notes / liberties taken vs the b0xx-mapping doc

- **Injection point**: `pollInputs` seam in `src/input/input.js` — the
  keyboard branch returns `funkeyPoll(...)` instead of
  `pollKeyboardInputs(...)`, emitting FINAL Melee-unit, 1/80-quantized
  coordinates. This deliberately bypasses the documented `tasRescale` trap
  (which saturates pre-scale modifiers back to 1.0). Applies to whichever
  player uses the keyboard (in practice P1); gamepads/AI/replays untouched.
- **SOCD = neutral** on keyboard opposite cardinals (task spec; the device's
  cross d-pad can't press opposites, so the policy is moot on hardware; the
  doc's preferred 2IP-no-reactivation is not implemented).
- **C-layer**: left stick goes NEUTRAL while the layer is held (doc: "drift
  freezes" — same practical effect, drift stops). C-layer diagonals emit
  (0.7, 0.7), not B0XX's ASDI coordinate (0.5250, 0.8500); note csY 0.7 ≥
  0.66 means a diagonal C-flick resolves as u/d-smash on the ground.
- **Tap jump forced OFF** for the keyboard player every poll (doc §4 assist:
  digital up = 1.0 would tap-jump on every upward DI). `?tapjump=on` restores.
- **Mod + shield + cardinal** is unspecified in the doc; the plain Mod value
  is kept (e.g. Q+E+↓ emits −0.5375, which will NOT spotdodge or shield-drop).
- **S3 grab quirk (upstream)**: while NOT in live play (menus/pause),
  meleelight treats held keyboard Z as frame-advance and clears the polled
  `z` in place (`src/main/main.js:834`) — so S3's grab doubles as
  frame-advance while paused. In live play grab works normally.
- **S2 C-layer chord** (L+R) suppresses ModX/ModY while held.
- No light shield / analog trigger levels anywhere (single-stage triggers,
  per the doc); digital shield emits `r=true, rA=1.0`.
- Fn is bound (F) but no scheme uses it — reserved by the FunKey OS. F8/F9
  are lab conveniences, not FunKey buttons.
- Extra debug surface added: `window.__funkey` and the overlay module.

## Files

- `funkeyMapping.js` / `funkeyOverlay.js` — the layer, verbatim copies of
  what the patch installs at `src/input/funkey/` (data-driven scheme tables
  + `KEYBINDS` at top).
- `funkey-mapping.patch` — everything against upstream `27af171`: the two
  new modules, the 2-line `pollInputs` seam, and the package.json build
  fixes (drop dead `deepstream.io`/`electron*` devDeps and the postinstall
  script — same fixes as the determinism spike).
- `build.sh` / `serve.sh` — docker node:8 build + one-command serve on :8765.
- `verify/boot-check.js` — headless Playwright check (`npm i playwright`
  next to it, then `node boot-check.js`): boots the page, joins keyboard,
  asserts live synthesized coordinates for the S1/S2 chord table and the S3
  resolver, and fails on page errors. All 15 checks pass as of this commit.

## Verified

Built with docker node:8 (webpack completes, only the known localforage
warning). Headless Chrome: page boots, keyboard joins, menus + char select +
stage select all navigable through the mapped layer, real VS match entered
(Fox vs CPU Marth), dash (1.0 flick) and Mod-walk (0.6625) observed in-game
with the overlay reporting the emitting coordinate family.
