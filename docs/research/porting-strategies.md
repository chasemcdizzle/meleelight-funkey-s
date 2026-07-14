# Porting-strategy survey: meleelight → FunKey-S

Resolves wayfinder ticket #4. Question: which strategies can run meleelight's
engine at a **hard 60fps sim+render, 240×240**, on the FunKey-S (single-core
ARM Cortex-A7 @1.2GHz, 64MB DDR2, musl/buildroot, SDL 1.2, armv7 hard-float),
while remaining **provably faithful** to the browser original (per-frame state
checksums against input replays)?

Date: 2026-07-13. Branch: `research/porting-strategies`.

---

## 0. What we are actually porting (measured, not assumed)

The "2.2MB of JS" framing undersells the *data* and oversells the *code*.
Measured on a fresh clone of `schmooblidon/meleelight` (`master`):

| Directory | Bytes (JS source) | Files | Nature |
|---|---:|---:|---|
| `src/animations/` | **30.1 MB** | 754 | Pure data: `Int16Array` vector-path coordinate lists (render-only) |
| `src/characters/` | 1.42 MB | 447 | Pure data: per-character framedata objects (attributes, hitboxes, action states) |
| `src/main/` | 456 KB | 142 | Game loop, render, characters' action-state logic, UI |
| `src/physics/` | 200 KB | 7 | The sim core: `physics.js`, `environmentalCollision.js`, `hitDetection.js`, `interpolatedCollision.js`, `article.js`, `actionStateShortcuts.js` |
| `src/menus/` | 155 KB | 11 | Menus (not sim-critical) |
| `src/stages/` | 116 KB | 25 | Stage geometry data + a little logic |
| `src/input/` | 103 KB | 25 | Input/controller mapping |
| `src/target/` | 72 KB | 3 | Target-test mode |

Language profile of the sim-critical code (grep-verified):

- ES6 modules + **Flow** type annotations (`@flow` in 4/7 physics files, 9 in
  `main/`) — types exist but are unsound/partial.
- `src/physics/` contains **zero classes**; `main/` has 3 files with `class`.
  Mostly plain procedural functions over plain objects + arrays. No `Map`/`Set`,
  no `Symbol`, no generators in the sim path.
- **No `Math.fround` anywhere** — the entire sim is plain IEEE-754 doubles.
- `Math.*` usage in sim code (physics+main+characters):
  `abs`×185, `random`×134, `PI`×131, `floor`×131, `sign`×65, `max`×57,
  `pow`×54, `sin`×37, `min`×37, `round`×36, `cos`×36, `sqrt`×14, `atan/atan2`×14,
  `tan`×1, `log`×1.
- `Math.random` in `src/physics/` is **cosmetic only** (SFX shout selection in
  `actionStateShortcuts.js`); the other uses are in menus/UI/effects. Any
  gameplay-visible randomness must be replaced by a seeded PRNG on *both* sides
  of the oracle regardless of strategy (Math.random is unseedable and differs
  per engine).

**Consequences for strategy scoring:**

1. The port problem is really **~800 KB of logic + ~31.5 MB of mechanical data**.
   Anything that machine-translates the data and concentrates human/agent effort
   on the logic wins on effort *and* on verifiability.
2. The animation data is already `Int16Array` literals — a one-page script turns
   it into C arrays / a binary blob (~half the source size once binary,
   fits comfortably in flash and can be mmap'd; only the active characters'
   data needs to be resident).
3. The absence of `fround`, typed-array tricks (outside data), and exotic ES
   features makes both *interpretation* and *translation* unusually tractable.

## 1. The faithfulness oracle and float semantics (applies to every strategy)

The kill-rule is bit-level: per-frame state checksums of the port must equal the
browser original's under identical input replays. What can and cannot diverge:

- **Representation**: JS `Number` is IEEE-754 binary64. C `double` on armv7
  VFPv3/v4 hard-float is the same binary64, and (unlike x87) ARM VFP registers
  are 64-bit — no extended-precision double-rounding hazard. ARMv7 NEON is
  single-precision only, so doubles cannot be silently auto-vectorized onto the
  non-IEEE (flush-to-zero) NEON path; they stay on IEEE-compliant VFP.
- **Basic arithmetic**: ECMA-262 requires `+ - * /`, `%`-adjacent ops and
  `Math.sqrt` to be correctly rounded IEEE operations — identical to C double
  ops, **provided** the compiler is pinned: `-ffp-contract=off` (no fused
  multiply-add contraction), no `-ffast-math`/`-Ofast`, no
  `-funsafe-math-optimizations`. This must be a locked flag set in the port's
  build from day one.
- **Transcendentals are the only real divergence surface.** ECMA-262 leaves
  `Math.sin/cos/tan/atan/atan2/pow/log/exp` implementation-approximated (it
  now *recommends* fdlibm). V8 computes them via `src/base/ieee754.cc`, a
  port of Sun's **fdlibm**, giving bit-identical results on every OS Chrome
  runs on ([v8 source](https://github.com/v8/v8/blob/master/src/base/ieee754.cc);
  [scrapfly analysis of cross-engine math bits](https://scrapfly.dev/posts/browser-math-os-fingerprint/);
  [Mozilla fdlibm intent thread](https://groups.google.com/a/mozilla.org/g/dev-platform/c/0dxAO-JsoXI/m/eEhjM9VsAgAJ)).
  SpiderMonkey uses a slightly different fdlibm port; JSC uses host `cmath`.
  musl's libm is yet another implementation.
  **Therefore: pin the oracle to one engine (V8 — Chrome or Node, pinned
  version) and make the port call the *same fdlibm-derived routines* —
  vendor V8's `ieee754.cc` (BSD-licensed fdlibm lineage) or upstream fdlibm
  into the C port instead of calling musl's `sin()`/`pow()`.** Meleelight's
  transcendental surface is small (sin, cos, tan, atan/atan2, pow, log), so
  this is a bounded, testable component: fuzz each function over the doubles
  the sim actually produces and require bit equality vs Node before trusting
  the sim at all.
- **`Math.random`**: unseedable, engine-specific (V8 uses xorshift128+ but the
  seed is opaque). Replace with a seeded PRNG *shim in the browser build* and
  the identical PRNG in the port; or confirm (as measured above) that no
  gameplay state depends on it and excise it from the checksum domain.
- **Checksum mechanics**: hash the raw 64-bit patterns of the state fields
  (`Float64Array`-view the numbers), never decimal string forms; canonicalize
  NaN if any path can produce it; include integer-ish fields as-is. This works
  identically in JS and C.
- **Semantics beyond floats** (relevant to translation strategies): `x | 0`
  truncation, `Math.floor` on negatives, `%` sign behavior, NaN propagation,
  `==` coercions, property iteration order where objects are iterated. A
  rewrite must treat each of these as a checklist item; an interpreter gets
  them for free.

The oracle harness itself (headless replay + checksum stream from the browser
build) is ticket #7; every strategy below is scored on how much *additional*
machinery it needs to make that oracle pass plausible.

---

## 2. Strategy A — manual/agent-driven rewrite to C/C++

**What:** Translate the ~28.6k LOC of logic (physics 200KB / main 456KB /
input+stages+menus+target ~450KB) to C99/C++ by an autonomous agent loop,
module-by-module, keeping structure parallel to the JS; machine-generate the
31.5MB of data (see Strategy E — in practice A subsumes E's data pipeline).

- **Maturity:** the *pattern* is proven in-house: the sibling
  `~/code_projects/ssb64-funkey-s` project runs an agent loop against a
  frame-diff oracle (`oracle/`, `fps_golden/`, `arm_match.log` show
  deterministic replay slices compared frame-by-frame on device); its 60fps
  campaign is still WIP, but the *translate → replay → diff → fix* loop
  discipline works. Meleelight is a friendlier subject than an N64 decomp:
  the source is readable, modular, procedural JS with Flow hints and tiny
  math utils (`Vec2D`, `Box2D`), zero classes in `src/physics/`.
- **ARM32 + musl:** trivially yes — plain C via the FunKey SDK (buildroot
  toolchain already exercised by ssb64-funkey-s).
- **Performance:** best of all options by construction. Melee-like 2-player
  sim + vector-path software rendering at 240×240 was achieved on far weaker
  hardware natively; a C translation of a canvas-vector fighter is expected
  to spend most of its frame in *rendering* (polygon fill of animation
  paths), not sim. Perf risk concentrates in the renderer (SDL 1.2 software
  canvas replacement), which is orthogonal to the strategy choice.
- **Memory:** logic is small; data compiled to binary `int16` blobs is
  ~10–15MB on flash, mmap-able, with only active characters resident. Fits.
- **Effort:** highest raw translation volume — but agent-driven and
  *embarrassingly loop-shaped*: each module is translated, then replay
  checksums localize divergence to a module/frame. JS semantics checklist
  (`|0`, `%` sign, `Math.floor` on negatives, NaN propagation, coercions,
  iteration order) must be part of the loop's review gate.
- **Oracle fit:** strong but earned, not free. Every JS-semantics slip is a
  potential silent divergence; the whole point of the per-frame checksum
  loop is that such slips surface at the exact frame + field they occur.
  Transcendentals solved by vendoring fdlibm/V8-`ieee754.cc` (§1). The
  canvas renderer is *outside* the checksum domain (state, not pixels), so
  render liberties don't threaten faithfulness.
- **Verdict:** viable; the reference strategy every other option must beat.

## 3. Strategy B — embedded JS engine on-device (QuickJS et al.)

**What:** Cross-compile a JS interpreter for armv7/musl, embed it behind a
bespoke SDL 1.2 + canvas-2D-subset shim (~1–2 KLOC C), run meleelight's JS
(bytecode-precompiled via `qjsc`) unmodified.

### Engine field (July 2026)

| Engine | Spec / activity | bench-v8 score¹ | Size | Notes |
|---|---|---:|---|---|
| **QuickJS** (Bellard) | ES2025; release 2026-06-04, "+42% vs previous" | 942 (2019) / 835 (2025) | ~367 KiB | Best perf/size; musl-proven (Alpine packages it); needs `-latomic` or `CONFIG_ATOMICS` off on armv7 ([bellard.org/quickjs](https://bellard.org/quickjs/), [docs](https://bellard.org/quickjs/quickjs.html)) |
| **quickjs-ng** | v0.15.1 (2026-06); releases ~2mo, heavy CI | 651; ~3% slower than Bellard's ([#876](https://github.com/quickjs-ng/quickjs/issues/876)) | similar | Maintenance-friendlier fork ([repo](https://github.com/quickjs-ng/quickjs)) |
| **Duktape** | **ES5.1** + partial ES2015; last release Feb 2022 | 408–506 (~2x slower than qjs) | 96–187 KB | Would require Babel-transpiling the whole bundle to ES5 ([duktape.org](https://duktape.org/), [post-ES5 wiki](https://wiki.duktape.org/postes5features)) |
| **JerryScript** | v3.0.0 Dec 2024; MCU-first | 250 (~4x slower than qjs, maintainers' own numbers — [#4386](https://github.com/jerryscript-project/jerryscript/issues/4386)) | 258 KB (Thumb-2) | Size-optimized, wrong tool for throughput |
| **MuJS** | ES5.1; 1.3.x active | 158–358 (slowest full engine) | 244 KB | Disqualified on perf ([mujs.com](https://mujs.com/), [mpv complaint](https://github.com/mpv-player/mpv/issues/13131)) |
| **Hermes** (interpreter) | RN-coupled releases; ARM32 = Android/bionic only | 968 (2019) / 1535 (2025) — ~1.8x qjs | 27–36 MB | musl port uncharted; huge binary for 64MB device ([repo](https://github.com/facebook/hermes)) |
| **XS/Moddable** | >99% test262; MCU-first, SDK-shaped toolchain | mixed vs qjs | 1.2 MB | No advantage over QuickJS here ([moddable.com/faq](https://www.moddable.com/faq)) |
| elk/mJS | tiny subsets (no arrays/closures/stdlib) | — | 20 KB | Instantly disqualified ([elk](https://github.com/cesanta/elk)) |

¹ bench-v8 overall scores: 2019 = Bellard's own table on an i5-4570 @3.2GHz
([mirror](https://github.com/quickjs-zh/QuickJS/blob/master/bench.md)); 2025 =
32-engine rerun ([ahaoboy 2025-07](https://dev.to/ahaoboy/js-engine-benchmark-2025-7-8-163b)).
Same tables: **V8 JIT 33,640 (2019) / 45,318 (2025); V8 `--jitless` 1,916.**

Bellard's own MQuickJS (2025–26, 100KB ROM/10KB RAM, ships its own
deterministic libm) is an interesting watch-item but is a restricted ES5-ish
subset requiring rewrites ([bellard/mquickjs](https://github.com/bellard/mquickjs)).

### Throughput estimate for the FunKey-S (the load-bearing arithmetic)

1. QuickJS is **~30–45x slower than desktop V8 JIT** (942 vs 33,640 on
   identical hardware; ~54x on 2025 engines; QuickJS-2026's +42% claws some
   back).
2. The Cortex-A7 is **~12.5x slower than that 2013 desktop core**: Raspberry
   Pi 2 (same Cortex-A7 µarch, 900MHz) scores Geekbench 5 single-core **52**
   ([browser.geekbench.com](https://browser.geekbench.com/v5/cpu/23548532))
   vs the i5-4570's **865**
   ([cpu-monkey](https://www.cpu-monkey.com/en/benchmark-intel_core_i5_4570-geekbench_5_64bit_single_core));
   scaled 900MHz→1.2GHz, and the V3s's 64MB DDR2 won't help.
3. **Combined: ~375–560x slower than meleelight under desktop V8.** If the
   per-frame sim costs 0.3–1ms under desktop V8 (typical for a 2-character
   physics/hitbox game of this size), interpreted on-device it costs
   **~110–560ms/frame → 2–9 fps before rendering a single pixel**. Even an
   implausibly cheap 0.05ms desktop frame scales to ~19–28ms, already over
   the 16.6ms budget with zero render headroom. Hermes's 1.8x or bytecode
   precompilation (removes parse time only) do not change the order of
   magnitude.

Ecosystem evidence agrees (§7): TIC-80's QuickJS carts are the slow path even
at 240×136; the JS-on-handheld existence proofs (nx.js canvas games on
Switch's Cortex-A57, crisp-game-lib's 232 Duktape minigames on aarch64
Anbernics — [muOS thread](https://community.muos.dev/t/crisp-game-lib-portable-with-duktape/374))
all involve far stronger cores or games 10–100x lighter. **No off-the-shelf
QuickJS+SDL1.2 runtime exists** — txiki.js has no graphics layer
([saghul/txiki.js](https://github.com/saghul/txiki.js)); quickjs-canvas
(QuickJS+SDL2 canvas API) is an incomplete experiment worth mining for design
([kirjavascript/quickjs-canvas](https://github.com/kirjavascript/quickjs-canvas)).

### Oracle fit and floats

Best-in-class *in principle* — it runs the same JS, so semantics come for
free — with one real defect: QuickJS/Duktape/MuJS delegate transcendentals to
**host libm** (musl here), which differs in last-ULP bits from V8's fdlibm
(§1). Fix is tractable and required: patch the engine's Math builtin table to
call vendored fdlibm (~1-file change in `quickjs.c`), plus the seeded-PRNG
shim for `Math.random`. Basic arithmetic/sqrt/fround are IEEE-exact already
(QuickJS numbers are int32-or-binary64 with NaN-boxing on 32-bit).

### Verdict

**Fails the 60fps kill-criterion by 1–2 orders of magnitude** on every
benchmark chain and every precedent. Retains two real uses: (i) the cheapest
possible falsification spike — QuickJS cross-compiles for armv7/musl
trivially, so we can run the *actual* headless sim on-device and replace the
estimate with a measurement; (ii) a debug/oracle scaffold: an on-device (or
host-side) QuickJS+fdlibm runtime that replays inputs and emits reference
checksums next to the C port.

## 4. Strategy C — AOT JS/TS compilation

### Static Hermes (`shermes`)

- **Status (July 2026): still unshipped as a native compiler.** Announced at
  React Native EU 2023 ([Mikov's deck](https://speakerdeck.com/tmikov2023/static-hermes-react-native-eu-2023-announcement));
  development lives in the `static_h` branch (`main` is in maintenance mode —
  [discussion #1587](https://github.com/facebook/hermes/discussions/1587));
  **no pre-built shermes binaries; build from source only**
  ([discussion #1137](https://github.com/facebook/hermes/discussions/1137)).
  What actually shipped — "Hermes V1" in React Native 0.82 (Oct 2025), default
  by RN 0.84 — is the faster VM, **explicitly without the native AOT compiler
  or JIT** ([RN 0.82 blog](https://reactnative.dev/blog/2025/10/08/react-native-0.82),
  [Software Mansion](https://swmansion.com/blog/welcoming-the-next-generation-of-hermes-67ab5679e184/)).
- **The headline "300x" requires soundly-typed TS/Flow** (`shermes -typed`).
  Untyped ES6 compiles but degrades to `any`-everywhere, i.e. interpreter-class
  performance ([#1587](https://github.com/facebook/hermes/discussions/1587)).
  Meleelight's Flow annotations are partial/unsound — this would be a typed
  rewrite, not a port.
- **Toolchain:** Hermes IR → LLVM → object (C backend via `-emit-c`); statically
  linked standalone binaries work
  ([Devon Govett's Dec 2025 walkthrough](https://devongovett.me/blog/static-hermes.html)),
  but there is **no documented ARM32/musl cross-compile workflow**, and
  standalone mode lacked timers/event-loop and typed-array typing as of the
  latest reports.
- **Verdict: not viable** for this project's constraints (research-grade
  toolchain, typed-rewrite requirement, no ARM32 story, heavy runtime for 64MB).

### porffor

- v0.61.13 (Apr 2026); version number tracks its **~61% test262 pass rate**
  ([README](https://github.com/CanadaHonk/porffor/blob/main/README.md),
  [porffor.dev](https://porffor.dev/)). Self-described pre-alpha: "Expect
  nothing to work!". Fatal for meleelight: **no closures** (no variables shared
  between scopes except args/globals); native output is an experimental
  wasm→C backend. **Verdict: not viable.**

### AssemblyScript

- Mature as a language for *new* code, but it is not JS: strict-TS dialect,
  **no closures over locals** ([issue #798](https://github.com/AssemblyScript/assemblyscript/issues/798),
  [Limitations](https://github.com/AssemblyScript/assemblyscript/wiki/Limitations)),
  no `any`/unions, nominal classes, explicit numerics. Its `f64` *is* IEEE-754
  double, so numerics could be preserved — but a mechanical JS→AS translation
  dies on closures/dynamic objects first. Perf on compute kernels is good but
  naive idiomatic ports can come out *slower* than V8-JIT'd JS
  ([surma.dev js-to-asc](https://surma.dev/things/js-to-asc/)). Output is wasm,
  so it still needs the §5 wasm→C chain to reach the device. **Verdict: only
  viable as a full manual rewrite — at which point plain C (Strategy A/E) is
  simpler, faster, and has no wasm layer. Not shortlisted.**

### JS→C transpilers (ts2c, NectarJS/NerdLang)

- **ts2c**: ~70% of *ES3*, numbers are `int16_t` — **no floats**, instantly
  fatal for a physics sim ([repo](https://github.com/andrei-markeev/ts2c)).
- **NectarJS/NerdLang**: small-subset compiler, effectively dormant
  ([nerd issue #46](https://github.com/NerdLang/nerd/issues/46)).
- No maintained general-purpose real-world-ES6→C transpiler exists in 2026.
  **Verdict: not viable.**

## 5. Strategy D — wasm routes (JS-engine-in-wasm → wasm2c/w2c2; wasm as compiled-code carrier)

The wasm→C tooling itself is genuinely mature on ARM32+musl:

- **wasm2c (WABT):** Frank Denis (Jul 2026) measured wasm2c output at 0.887x
  Wasmer / 0.814x Wasmtime run-time (geomean) with lower RSS — "very close to
  a no-brainer" for precompiled trusted modules
  ([00f.net 2026](https://00f.net/2026/07/08/webassembly-compilation-to-c-2026/));
  wasm runtimes are typically ~1.25x native on compute
  ([Bytecode Alliance](https://bytecodealliance.org/articles/wasmtime-10-performance));
  Firefox ships wasm2c for RLBox sandboxing
  ([Bugzilla 1827704](https://bugzilla.mozilla.org/show_bug.cgi?id=1827704)).
  Net: **~85–95% of native for compute-shaped code**.
- **w2c2:** emits C89, explicitly supports ARM32 (down to retro systems),
  claims Coremark within ~7% of native, active
  ([turbolent/w2c2](https://github.com/turbolent/w2c2)).
- **WAMR AOT:** `wamrc --target=armv7`/VFP documented, near-native claims,
  tiny runtime — though 32-bit targets keep explicit bounds checks
  ([WAMR](https://github.com/bytecodealliance/wasm-micro-runtime),
  [build docs](https://github.com/bytecodealliance/wasm-micro-runtime/blob/main/doc/build_wamr.md)).

But the *JS* route through it fails on arithmetic of compounded slowdowns:

- The only way to put unmodified JS inside wasm is to ship a JS engine in the
  module (Javy = QuickJS-in-wasm, built for short-lived serverless calls, not
  a persistent 60Hz loop — [Shopify](https://shopify.engineering/javascript-in-webassembly-for-shopify-functions)).
- Native QuickJS is already ~2–3x slower than V8's *interpreter* and 20–100x
  slower than V8's JIT ([bellard.org/quickjs](https://bellard.org/quickjs/),
  [godot-ECMAScript issue](https://github.com/Geequlim/ECMAScript/issues/16));
  the wasm2c wrapper subtracts another ~1.1–2x (interpreter dispatch loops fare
  worse than kernels). Compounded: **~30–150x below desktop JIT, before the
  ~15–25x hardware gap** (§3). And since we can already cross-compile QuickJS
  natively for armv7/musl, the wasm wrapper adds nothing but overhead here —
  its sandboxing/portability benefits solve problems we don't have.
- **Oracle fit** (for completeness): actually good — a wasm2c'd engine is
  deterministic and self-contained — but perf is disqualifying.
- **Verdict: JS-engine-in-wasm is not viable for 60fps. The wasm→C chain
  (w2c2/WAMR) is only interesting as a carrier for already-compiled code
  (AS/Rust/C), where plain native C is simpler anyway. Not shortlisted.**

## 6. Strategy E — hybrid: hand/agent-rewritten sim core + machine-translated data (+ optionally interpreted cold code)

**What:** Sharpen A by explicitly splitting the codebase along its measured
seams:

1. **Data plane (31.5MB, `animations/` + `characters/` + `stages/` data):**
   fully mechanical translation. `animations/*.js` are `Int16Array` literal
   arrays → emit a binary pack + generated C index; `characters/*.js` are
   nested object literals → run them **in Node** and serialize the resulting
   objects to C structs / flat tables (executing the original JS to produce
   the data eliminates translation error by construction — the generator is
   the oracle for the data plane).
2. **Sim core (~200KB physics + the action-state machines in `main/`):**
   agent-rewritten C, structure-parallel to the JS (same file/function names,
   same field order) so that per-frame, per-field checksum diffs map 1:1 to
   source locations.
3. **Cold code (menus, UI, target-test):** either also translated, or — if a
   JS engine is on-device anyway per Strategy B — left interpreted, since
   menus have no 60fps sim requirement. (Two-runtime complexity is only
   worth it if B's engine is already present; otherwise translate.)

- **Oracle fit:** the best of any strategy: the data plane is *generated from
  the original by execution* (provably equivalent), shrinking the trusted
  hand-translation surface to ~15–20k LOC of sim logic that the checksum
  loop hammers.
- **Perf/memory/platform:** identical to A (it *is* A with a codified data
  pipeline and a smaller hand-translation surface).
- **Verdict:** viable; this is the disciplined form of A and they should be
  scored together (A/E) rather than as rivals.

## 7. Precedents — how web/JS games actually reached this hardware class

Surveyed: PortMaster (500+ ports), FunKey-S OPK catalog, Miyoo Mini/OnionOS,
RG35XX, plus console ports of famous web-origin games. Headline: **not a
single shipped port in any of these ecosystems executes a game's JavaScript
on-device** (no browser, Electron, Node, or bare JS engine).

- **PortMaster's method taxonomy** — native recompiles, engine
  reimplementations (devilutionX pattern), bundled native runtimes (Godot/FRT,
  GMLoader, Love2D, Java), Box86/64 — contains no HTML5/JS route at all
  ([portmaster.games/porting.html](https://portmaster.games/porting.html)).
  GameMaker titles that also exist as HTML5 games arrive via their compiled
  Android ARM runner (`libyoyo.so`), never their JS builds
  ([GMLoader ports](https://github.com/Fraxinus88/GMloader-ports)). The
  community route to a portable Friday Night Funkin' (HTML5/Haxe) was
  reverse-engineering the transpiled JS *back into Haxe* to compile natively
  ([FNF-NewgroundsPort](https://github.com/AngelWyvern/FNF-NewgroundsPort)).
- **Interpreted JS exists on handhelds only inside fantasy consoles, and it's
  the slow path.** TIC-80 runs JS carts via QuickJS (Duktape before v1.1 —
  [issue #1191](https://github.com/nesbox/TIC-80/issues/1191)); even on
  desktop a water-sim cart ran **~5fps in JS vs full speed in Lua**
  ([issue #617](https://github.com/nesbox/TIC-80/issues/617)), and on RG35XX
  some carts are "really, really slow, so unplayable" at 240×136
  ([rasterweb.net](https://rasterweb.net/raster/2023/07/24/tic-80-retro-fantasy-computer/)).
  PICO-8 content reaches FunKey-S via **FAKE-08/retro8, C++ reimplementations**
  ([lexaloffle thread](https://www.lexaloffle.com/bbs/?tid=42034)). WASM-4's
  answer to low-end handhelds is **wasm4-aot** — transpile the cart to C via
  w2c2/wasm2c and statically link ([asiekierka/wasm4-aot](https://github.com/asiekierka/wasm4-aot)):
  interpretation gives out; AOT-to-C is the escape hatch.
- **FunKey-S catalog:** SDL C/C++ throughout (e.g. joyrider3774's eight SDL
  ports — [funkey_sdl2_games](https://github.com/joyrider3774/funkey_sdl2_games));
  no web-game port, no JS runtime. Even emulator ceilings are tight (only
  MAME 2000 is fast enough). Toolchain = Buildroot external tree + prebuilt
  GCC SDK ([FunKey-OS](https://github.com/FunKey-Project/FunKey-OS),
  [SDK docs](https://doc.funkey-project.com/developer_guide/tutorials/build_system/build_program_using_sdk/)).
- **The strongest direct precedent — CrossCode** (Impact.js, ~100k LOC JS) on
  Switch/consoles: the team **measured interpretation and rejected it** ("turns
  out to not be fast enough"; JIT unavailable), then built an **AOT JS→C++
  pipeline**; independently Robert Konrad built a JS→Haxe→C++ compiler for it —
  "a significant speedup over bundling a JS interpreter"
  ([Siliconera interview](https://www.siliconera.com/crosscode-interview-radical-fish-games-on-console-ports-and-whats-next/),
  [HN thread](https://news.ycombinator.com/item?id=26753620)). A Switch-class
  CPU is far faster than a 1.2GHz single A7.
- **Other web-origin games went the rewrite route:** Vampire Survivors
  (Phaser JS → full Unity/IL2CPP rewrite —
  [Wikipedia](https://en.wikipedia.org/wiki/Vampire_Survivors)); A Dark Room
  (browser JS → Ruby/mRuby rewrite —
  [Wikipedia](https://en.wikipedia.org/wiki/A_Dark_Room)); Celeste Classic
  (PICO-8 → hand-written C/SDL `ccleste`, now on 3DS/PSX/N-Gage —
  [lemon32767/ccleste](https://github.com/lemon32767/ccleste)).
- **Miyoo Mini** (dual Cortex-A7 @1.2GHz, 128MB, no GPU — the closest cousin
  to the FunKey-S): OnionOS ports are all native binaries/RetroArch cores; no
  web-game route exists there either
  ([OnionUI/Onion](https://github.com/OnionUI/Onion),
  [retrocatalog specs](https://retrocatalog.com/retro-handhelds/miyoo-mini)).

**Pattern:** (1) manual C/C++/SDL rewrite dominates and is the only route with
shipped FunKey-class precedents; (2) mechanical AOT JS→C++ translation appears
exactly when the codebase is too big to rewrite (CrossCode) — and exists
*because* interpretation was measured and rejected on much faster hardware;
(3) nobody ships interpreted JS games on GPU-less sub-1.5GHz ARM32 devices.

## 8. Comparison matrix

Kill-criteria: locked 60fps sim+render on 1.2GHz single Cortex-A7; fits 64MB;
provable behavioral equivalence vs the browser original.

| | A: agent C rewrite | B: embedded JS engine (QuickJS) | C: AOT JS (shermes/porffor/AS/ts2c) | D: JS-engine-in-wasm → wasm2c | E: hybrid (A + executed-data pipeline) |
|---|---|---|---|---|---|
| **Maturity** | Pattern proven in-house (ssb64-funkey-s loop + oracle) | Engines mature (QuickJS ES2025, 2026-06); no SDL1.2 runtime exists — bespoke shim | shermes unshipped/research; porffor pre-alpha (no closures); AS/ts2c ≠ JS | wasm2c/w2c2/WAMR mature; the JS payload story is Javy-shaped (serverless) | Same as A |
| **ARM32 + musl** | Native, FunKey SDK — trivial | QuickJS musl-proven (Alpine); `-latomic` nit | No ARM32/musl workflow (shermes); others N/A | w2c2 explicitly ARM32; fine | Same as A |
| **60fps realistic?** | Yes (perf risk lives in the renderer, common to all) | **No: est. 2–9fps sim-only** (~375–560x below desktop V8, benchmark chain §3) | No for untyped JS (interpreter-class); typed rewrite ≠ faithful port | No: QuickJS penalty × wasm penalty ≈ 30–150x below JIT before HW gap | Yes |
| **Memory (64MB)** | Fits; data mmap'd binary (~10–15MB flash) | Engine tiny (~400KB) but 30MB JS data as live JS objects is a real heap risk | — | — | Fits (best: flat tables, no JS object overhead) |
| **SDL 1.2 integration** | Direct | Bespoke C canvas-subset shim (~1–2 KLOC) | — | — | Direct |
| **Oracle fit (per-frame checksums, float semantics)** | Strong; earned via loop + fdlibm vendoring + JS-semantics checklist | Best in principle (same code); needs fdlibm-patched Math table + PRNG shim | Unverifiable toolchains (immature) or semantic-gap rewrites | Good but moot (perf) | **Best of the compiled options**: data plane provably equivalent by construction; hand surface shrunk to ~15–20k LOC |
| **Effort** | High volume, loop-shaped | Lowest to first pixel | Highest (rewrite + toolchain R&D) | High, for negative value | High-but-focused (A minus mechanical data) |
| **Verdict** | **Viable (reference)** | Fails kill-criteria; keep as instrument | Not viable (2026) | Not viable | **Viable — recommended** |

## 9. Recommended shortlist for the on-device feasibility spike (ticket #8)

### S1 (primary): A/E — agent-driven, structure-parallel C rewrite with an executed-data pipeline

The only strategy class with (i) shipped precedents on exactly this hardware
(entire FunKey/Miyoo catalogs), (ii) direct precedent for "JS game too big to
rewrite by hand" (CrossCode's AOT JS→C++ — built *because* interpretation
failed on far faster silicon), (iii) an in-house proven execution pattern
(ssb64-funkey-s's translate→replay→diff loop), and (iv) the best oracle story:
the 31.5MB data plane is generated by *executing the original JS* (equivalent
by construction), and the ~15–20k-LOC hand-translated sim core is
structure-parallel so checksum divergences map 1:1 to source. Perf and memory
clear the bar by construction; the open risk is concentrated, measurable, and
strategy-independent.

**Spike content:** (1) rasterize real `src/animations/` paths (converted
Int16 packs) with an SDL 1.2 software renderer at 240×240 on-device — measure
ms/frame for representative 2-character scenes (the true perf unknown);
(2) fdlibm bit-equality harness: vendored fdlibm/V8-`ieee754.cc` on armv7 vs
pinned Node, fuzzed over sim-realistic domains; (3) translate one physics
module and checksum-lock it against a Node replay slice to calibrate the
loop's divergence-fix rate.

**Biggest open risk:** the software renderer — meleelight draws
anti-aliased vector paths via canvas; nobody has demonstrated that a faithful-
*looking* polygon-fill of those paths fits the ~half-frame budget on a
GPU-less A7 at 240×240. (Sim faithfulness is checksummed state, not pixels,
so the renderer may cut corners — but "60fps render" is still a kill
criterion.) Secondary risk: divergence-fix convergence rate — thousands of
small JS-semantics traps; the loop catches each, but the schedule depends on
how fast they burn down.

### S2 (instrument, cheap): native QuickJS on-device measurement

Not a candidate to *ship* — the benchmark chain (§3) and every precedent (§7)
say 2–9fps — but it is the cheapest possible falsification: QuickJS
cross-compiles for armv7/musl in minutes, `qjsc` precompiles the bundle, and
the headless sim can be replayed on-device to convert the estimate into a
measured ms/frame number. Three payoffs regardless of outcome: (a) a hard
number that either buries Strategy B or (if it shocks by 10x) reopens a
B/E split (interpreted menus atop a C sim core); (b) the fdlibm-patched
QuickJS doubles as the *reference oracle runtime* — replaying inputs and
emitting reference checksums adjacent to the C port during S1's loop; (c) it
forces the headless-replay harness (ticket #7) to exist early.

**Biggest open risk:** opportunity cost / sunk-instrument temptation — the
measurement is expected to confirm failure, and the fdlibm Math-table patch
plus canvas shim must stay minimal or S2 quietly becomes a doomed porting
effort. Timebox it; its deliverable is a number and an oracle runtime, not a
game.

### Not shortlisted

Strategy C (no tool in 2026 compiles arbitrary untyped ES6 to fast ARM32
native; shermes needs a typed rewrite and has no ARM32/musl story) and
Strategy D (compounded interpreter×wasm slowdown; its only mature pieces —
w2c2/WAMR — solve problems we don't have when native C is already available).

---

*Method note: codebase numbers in §0 measured on a fresh clone
(`schmooblidon/meleelight@master`, 2026-07-13); benchmark and status claims
cited inline to primary sources; hardware-gap chain uses same-µarch Geekbench
data (Raspberry Pi 2, Cortex-A7). This survey feeds tickets #7 (oracle
spike), #8 (feasibility spike), and #10 (strategy lock).*
