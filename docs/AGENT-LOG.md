# AGENT-LOG — append-only iteration ledger

Format per iteration (appended by the loop, step F): `iter, phase, task,
done-check cmd+exit, .loop log paths, artifact hashes, CHECKER evidence,
zoom-out note, next`. Sentinel lines (`LOOP STOP: …`) are always the LAST
line when present — see `docs/LOOP.md` §H.

---

iter 0 · 2026-07-14 · SPEC LOCKED (wayfinder ticket #13, map issue #1)
- All research/spike/prototype branches merged onto main as the evidence
  appendix; PLAN.md / CLAUDE.md / docs/LOOP.md / docs/loop/* / licensing
  files / this ledger written. The loop has not run yet.
- To arm: README §Running the loop (branch agent/auto, scripts/loop.sh).
- First expected iteration: REPLAN on M0 (fix_plan.md items are seeded
  with `done-check: …` by design).

iter 1 · 2026-07-14 · phase M0 · REPLAN (LOOP.md C-b: all seeded M0 items had `done-check: …`)
- Rewrote fix_plan.md §M0 into 7 ordered, independently provable tasks
  (build recipe → harness productionization → fdlibm both sides →
  CHECKSUM.md freeze → golden #1 stream freeze → QuickJS runtime →
  full golden set + verify_goldens.sh), each with an exact runnable
  done-check. Concretized the M0 exit gate into CLAUDE.md §Commands as
  `bash oracle/verify_goldens.sh` (browser ×2 + frozen streams + QuickJS
  reproduction + 5-char/6-stage/≥1-CPU coverage; exact equality, full
  length). §Gates table untouched.
- Conventions fixed: upstream clone OUTSIDE the tree at
  `${MELEELIGHT_CLONE:-$HOME/.cache/meleelight-funkey-s/upstream}`;
  goldens at `oracle/goldens/gNN-<p1>-<p2>-<stage>.{trace,sha256}.json`
  + manifest.json; spikes/determinism stays frozen, oracle/ is the
  maintained copy.
- PROVISIONAL (auto-adopted): golden-set composition = g01 existing
  Fox/Marth/Battlefield + g02–g06 human pairs covering remaining
  chars/stages + g07–g08 CPU(d5) traces; fdlibm surface fixed at
  sin/cos/tan/atan/atan2/pow (spike exp. D exposure + tan for the M4 AI).
- next: execute M0 task 1 (oracle/build-upstream.sh).

iter 2 · 2026-07-14 · phase M0 · task 1: oracle/build-upstream.sh (pinned clone+patch+build recipe as a committed tool)
- done-check: `bash oracle/build-upstream.sh && test -f "${MELEELIGHT_CLONE:-$HOME/.cache/meleelight-funkey-s/upstream}/dist/meleelight.html" && grep -rq __harnessInputs "${MELEELIGHT_CLONE:-$HOME/.cache/meleelight-funkey-s/upstream}/dist/js/"` → exit 0
  (full cold build: clone → pin 27af171 → patch → prune deepstream.io/
  electron*/postinstall → docker node:8 npm install + animations + build;
  idempotent re-run verifies+skips in <0.1 s).
- logs: .loop/iter2-m0t1-donecheck.log (cold build, DONE-CHECK EXIT: 0),
  .loop/checker/m0t1.log (CHECKER re-run, exit 0).
- artifacts: oracle/build-upstream.sh; oracle/meleelight-harness.patch
  sha256 65f06d7a1aec59bfa7999c8584e238e66c6ea1160f98528189bf9938f006042b
  (byte-identical to the frozen spike copy); clone HEAD
  27af171d983184c7100be87d4c5ba30c0e35a5ae at
  ~/.cache/meleelight-funkey-s/upstream.
- CHECKER (mode=task): verified=true, tamper=false, gaps=[]; evidence:
  done-check exit 0, patch shasum match, pin exact, tree touches only
  oracle/ (allowed in M0), remotes = origin only.
- zoom-out: the failure CLASS here is "proven recipe living as README
  prose" — every consumer (spike, prototype) re-derived the same prune
  steps by hand. Class fix applied: recipe-as-executable, one idempotent
  script, self-verifying, clone pinned outside the tree; future M0 tasks
  reuse it instead of re-reading spike docs. No recipe defect found (the
  documented spike commands worked as written; the mid-build "container
  exited" ping was inter-phase timing, not a failure).
- next: M0 task 2 — productionize the spike harness into oracle/harness/
  + golden #1 trace copy, two-fresh-runs IDENTICAL done-check.

iter 3 · 2026-07-14 · phase M0 · task 2: productionize spike harness → oracle/harness/ + golden #1 trace copy
- done-check (verbatim fix_plan): `cd oracle/harness && npm install
  >/dev/null && node run.js --dist "${MELEELIGHT_CLONE:-$HOME/.cache/meleelight-funkey-s/upstream}"
  --trace ../goldens/g01-fox-marth-battlefield.trace.json --frames 3600
  --seed 1337 --out out/g01-a.json && node run.js … --out out/g01-b.json
  && node compare.js out/g01-a.json out/g01-b.json` → printed
  `IDENTICAL checksum streams` (3600/3600 frames, rngCalls 134=134),
  DONE-CHECK EXIT: 0.
- logs: .loop/iter3-m0t2-donecheck.log (writer run, exit 0),
  .loop/iter3-m0t2-param-smoke.log (behavioral proof of the NEW params:
  --p1 4 --p2 1 --stage 3 boots and diverges from default at frame 1;
  --cpu --difficulty 5 diverges at frame 116 with AI rng draws 54 vs 9;
  bad --p1 5 / --stage 6 / missing --trace all rejected exit 1),
  .loop/checker/m0t2.log (CHECKER re-run, exit 0).
- artifacts: oracle/harness/{run.js,init.js,pagelib.js,compare.js,
  gen-trace.js,package.json (pins playwright 1.61.1 exact),.gitignore};
  oracle/goldens/g01-fox-marth-battlefield.trace.json sha256
  60c332c5b65cf08c0b2e2530425b03111054916c378bca03ba501eb000aa20b9
  (byte-identical to frozen spikes/determinism/harness/trace-p1p2.json;
  oracle gen-trace.js regenerates it byte-identically at seed 1337/3800f).
  run.js changes vs spike: required --trace (no hidden default),
  range-validated --p1/--p2 (0-4), --stage (0-5), --cpu --difficulty
  (1-9, default 5), match params recorded in output meta.
- CHECKER (mode=task, sub-agent): verified=true, tamper=false, gaps=[];
  evidence: done-check exit 0 + IDENTICAL, trace sha match, playwright
  pin exact, exact-equality compare (strict !==, no epsilon), tree
  touches only oracle/ additions (M0-allowed), remotes = origin only.
- zoom-out: two classes considered. (a) "maintained copy silently drifts
  from frozen evidence" — instrumented at copy time: golden trace copied
  byte-identically (sha-checked) and gen-trace.js regeneration proves the
  generator copy is drift-free; future oracle/ edits are tamper-guarded
  by CHECKER anyway. (b) "done-check only exercises the default path of
  newly added parameters" — a real gap class for parameterization tasks;
  fixed at the instrument level by adding a param smoke log
  (.loop/iter3-m0t2-param-smoke.log: alt chars/stage/cpu + rejection
  cases) as registered evidence WITHOUT touching the frozen done-check
  (gates never weakened, only supplemented). Task 7 (g02-g08) will
  exercise every param through real goldens, closing the class fully.
- next: M0 task 3 — vendored fdlibm both sides (port/fdlibm/ C sources +
  NOTICES entry first, JS Math shim in oracle/harness/init.js,
  oracle/fdlibm-crosscheck/ → CROSSCHECK OK).

iter 4 · 2026-07-14 · phase M0 · task 3: vendored fdlibm BOTH sides + crosscheck
- done-check (verbatim fix_plan): `bash oracle/fdlibm-crosscheck/run.sh`
  → printed `CROSSCHECK OK`, DONE-CHECK EXIT: 0.
- logs: .loop/iter4-m0t3-donecheck.log (writer run, exit 0),
  .loop/iter4-m0t3-harness-rerun.log (two fresh shim-active harness runs
  → IDENTICAL checksum streams, 3600/3600 frames, rngCalls 134=134,
  exit 0), .loop/checker/m0t3.log (CHECKER re-run, exit 0).
- artifacts: port/fdlibm/{fdlibm.h, fdlibm.c (sha256 46e7eb3ccb114887…),
  fdlibm.js (sha256 da767867ccdc9f9d…)} — sin/cos/tan/atan/atan2/pow,
  NOTICES entry landed first, Sun+V8 notices carried verbatim in headers;
  oracle/fdlibm-crosscheck/{run.sh, gen-inputs.js, csweep.c, jssweep.js,
  check-constants.js, sanity-ulp.js, extract-args.js, .gitignore};
  oracle/harness/init.js (Math shim installed from window.__fdlibm,
  hard-fails if fdlibm.js not injected; --capture-math logging),
  oracle/harness/run.js (injects port/fdlibm/fdlibm.js before init.js by
  default, --native-libm / --capture-math flags, meta.fdlibm recorded).
- evidence: (1) CONSTANTS OK — 176 decimal-literal/bit-pattern pairs
  verified across BOTH files; (2) sweep — 257,287 deterministic inputs
  (every fdlibm branch threshold ±ulps, denormals, ±0, ±Inf, NaN incl.
  payload, k·π/2 cancellation to k=400, Payne-Hanek exponent spread, pow
  overflow/underflow z-boundaries, yisint parity boundaries, sim ranges)
  → C and JS BYTE-IDENTICAL on first run; (3) SANITY OK — all 257,287
  within 16 ulp of node native Math, worst = 1 ulp; (4) golden #1 stream
  — 11,488 captured calls (sin 2027 · cos 2018 · atan 656 · atan2 6499 ·
  pow 288; tan 0 until the M4 AI port) replayed browser/C/JS three-way
  BYTE-IDENTICAL (inputs sha256 b164802a…, args sha256 c6a6337a…).
- behavioral proof the shim matters: shimmed g01 checksum stream first
  diverges from the pre-shim iter-3 stream at frame 1671 (browser native
  libm ≠ fdlibm in live sim inputs) while remaining run-to-run identical
  — exactly the drift the shim pins away. atan/atan2 match node's native
  Math bit-exactly over 100k random inputs (V8 ships this same fdlibm
  atan), independently validating those ports; sin/cos/tan differ from
  native ~0.5% of args (platform libm trig), pow rarely (≤1 ulp).
- PROVISIONAL (auto-adopted): (a) fdlibm source lineage fixed = V8
  12.4.254 src/base/ieee754.cc (sha256 recorded in fdlibm.c header),
  using the pure-fdlibm fdlibm_sin/fdlibm_cos bodies — settles PLAN §2's
  "exact source file set fixed at M0"; scalbn from classic Sun s_scalbn.
  (b) V8's signaling_NaN() returns → canonical quiet NaN
  0x7FF8000000000000 on BOTH sides (JS cannot reliably produce sNaN;
  identical bit pattern preserved crosscheck-exactly).
- CHECKER (mode=task, sub-agent): verified=true, tamper=false, gaps=[];
  evidence: done-check re-run exit 0, gates are plain `cmp` on full
  64-bit patterns (no epsilon anywhere in the binding path; 16-ulp bound
  is a supplementary guard), license notices present, shim injected by
  default and hard-fails when missing, tree touches only NOTICES +
  oracle/harness + new port/ + new crosscheck (M0-allowed), branch
  agent/auto, remotes = origin only.
- zoom-out: the crosscheck proves C≡JS IDENTITY, not CORRECTNESS — a
  transcription typo copied into both sides sails through bit-exact
  comparison. That is a CLASS (shared-source-error blindness), and it got
  two instruments, not a one-off eyeball: check-constants.js re-derives
  every constant from its commented bit pattern in BOTH files (kills the
  dominant typo vector mechanically), and sanity-ulp.js bounds every
  sweep result against an INDEPENDENT implementation (node native Math)
  at 16 ulp — gross algorithm errors show up as thousands of ulps.
  Neither weakens the bit-exact gates; both run inside the done-check.
  Same class-thinking applied downstream: when M2's C sim diverges from
  the oracle, "identical-but-wrong shared assumptions" should be the
  first suspect list, and independent-reference bounds the standard
  instrument. Also reused iter-3's lesson (new flags need behavioral
  proof beyond the default path): --capture-math and --native-libm are
  exercised by the done-check itself (capture feeds gate 4) and by the
  divergence-at-frame-1671 observation respectively.
- next: M0 task 4 — freeze the checksum spec as oracle/CHECKSUM.md
  (field list, serialization rules, percentShake exclusion) → SPEC OK.

## iter 5 — 2026-07-14 — M0 task 4: freeze the checksum spec (oracle/CHECKSUM.md)
- phase: M0. task: fix_plan §M0 item 4 — write oracle/CHECKSUM.md as the
  frozen, normative checksum spec (field list, serialization, SHA-256,
  frame boundary, RNG channel, percentShake exclusion, versioning rule).
- done-check: `bash -c 'for t in actionState timer percent stocks hitboxes
  phys articles percentShake SHA-256 mulberry32 "\-0"; do grep -q -- "$t"
  oracle/CHECKSUM.md || { echo "missing: $t"; exit 1; }; done; echo SPEC
  OK'` → `SPEC OK`, exit 0 (.loop/iter5-m0t4-donecheck.log; CHECKER re-run
  .loop/checker/m0t4.log).
- artifacts: oracle/CHECKSUM.md (spec v1, 271 lines, sha256
  cdc48d11014a41ae…). Every normative rule cites file:line in
  oracle/harness/{pagelib,init,run,compare}.js,
  oracle/meleelight-harness.patch, and the patched upstream
  (src/main/player.js, src/main/main.js, src/physics/article.js) — all
  read end-to-end this iteration; no code changes were needed.
- precision findings pinned in spec §9 (doc-vs-shorthand, not code bugs):
  (1) the envelope key order is FIXED-LITERAL (p0..p3 then articles;
  per-player actionState,timer,percent,stocks,hit,hitboxes,phys), NOT
  sorted — PLAN §3's "sorted keys" is only true of nested objects
  (pagelib.js:30 vs 50-62); (2) cycle detection is path-based (seen-set
  add/delete), so shared DAG references serialize in full at each
  occurrence; (3) the seeded RNG stream includes exactly ONE off-step draw
  before frame 1 (startGame → setBackgroundType, patched upstream
  main.js:1322) — confirmed rngCallsOutsideStep==1 in every recorded run
  output (oracle/harness/out/*.json); the C port must burn one draw at
  match start. Conformance channels fixed: per-frame hash stream (binding)
  + end-of-run draw count (binding) + rngCallsOutsideStep==1 (diagnostic).
- CHECKER (mode=task, sub-agent): verified=true, tamper=false, gaps=[];
  evidence: done-check exit 0 (all 11 tokens), spec-vs-code spot-check
  passed on every cited rule at the exact cited lines (serialization,
  hash, frame boundary, mulberry32 verbatim, percentShake patch hunk,
  __harness hooks, compare.js/run.js secondary cites), all required topics
  present, diff touches only M0-allowed paths, branch agent/auto.
- zoom-out: a frozen spec that merely paraphrases code rots into a second
  source of truth — the CLASS is doc/code drift. Instruments applied
  instead of trusting prose: (a) every normative rule carries a file:line
  citation so drift is mechanically auditable; (b) §8 makes the spec
  self-invalidating — any normative edit forces a version bump + re-freeze
  of every golden stream in the same change, so a drifted spec cannot
  coexist with valid streams; (c) the under-specified spots found while
  transcribing (envelope order, path-based cycles, the startGame draw)
  were pinned as numbered findings rather than silently papered over —
  each was exactly the kind of "both sides copy the same wrong assumption"
  hole iter 4's crosscheck lesson warned about for the M2 port.
- next: M0 task 5 — record + freeze golden #1's checksum stream
  (oracle/record.sh g01 + verify-stream.js) → STREAM MATCH.

## iter 6 — 2026-07-14 — M0 task 5: record + freeze golden #1's checksum stream
- phase: M0. task: fix_plan §M0 item 5 — oracle/record.sh + verify-stream.js;
  freeze oracle/goldens/g01-fox-marth-battlefield.sha256.json (fdlibm shim
  active, spec v1).
- done-check: `cd oracle/harness && node run.js --dist "$MELEELIGHT_CLONE…"
  --trace ../goldens/g01-fox-marth-battlefield.trace.json --frames 3600
  --seed 1337 --out out/g01-fresh.json && node verify-stream.js
  out/g01-fresh.json ../goldens/g01-fox-marth-battlefield.sha256.json` →
  `STREAM MATCH g01-fox-marth-battlefield: 3600/3600 frames exact,
  rngCalls=134, rngCallsOutsideStep=1, specVersion=1`, exit 0
  (.loop/iter6-m0t5-donecheck.log; CHECKER re-run .loop/checker/m0t5.log).
- artifacts: oracle/goldens/g01-fox-marth-battlefield.sha256.json (FROZEN,
  sha256 6426169198c7cf27…, 3600 frames, spec v1, rngCalls=134,
  rngCallsOutsideStep=1, frame-1 hash = CHECKSUM.md §5 anchor 9f4c6df7…);
  oracle/goldens/manifest.json (golden registry, seeded with g01 — the
  SINGLE param source for record/freeze/verify; task 7 extends it + adds
  coverage assertions); oracle/record.sh (2 fresh runs → compare →
  freeze → self-verify); oracle/harness/{streamlib,freeze-stream,
  verify-stream}.js.
- evidence beyond the done-check (.loop/iter6-m0t5-record.log,
  -rerecord.log, -negative.log): (1) two fresh runs bit-identical before
  freezing (compare.js IDENTICAL, rngCalls 134==134); (2) re-running
  record.sh reproduces the frozen file BYTE-IDENTICALLY ("unchanged
  (byte-identical re-freeze)"; sha256 equal before/after) — the file is
  timestamp/environment-free by design, so `git diff` after any re-record
  is itself a drift detector; (3) negative tests — verifier rejects a
  pre-shim run (meta.fdlibm undefined), a tampered frozen file (streamSha256
  seal), a tampered run hash (divergence at frame 3000, exit 2), and
  freeze-stream refuses a differing overwrite without --refreeze.
- PROVISIONAL (auto-adopted): manifest.json created now (task 7's text
  says it writes the manifest — it will extend this one; record.sh being
  manifest-driven is the class-level design over per-golden hardcoding);
  frozen-file format {golden, specVersion, params(+traceSha256), rngCalls,
  rngCallsOutsideStep, streamSha256, frames[{f,h}]}, one frame per line;
  frames=3600 per the done-check (the 3800-frame trace's tail 200 frames
  stay unconsumed — held-last semantics make the stream well-defined).
- CHECKER (mode=task, sub-agent): verified=true, tamper=false, gaps=[];
  evidence: done-check exit 0 with STREAM MATCH; verify-stream.js read —
  exact equality, full length, no epsilon/tolerance/prefix; frozen file
  has no timestamps/env data; git diff HEAD empty, only 6 new files all
  under oracle/ (M0-allowed); branch agent/auto.
- zoom-out: the CLASS is "contract artifacts that drift silently or pass
  vacuously". Instruments over process-trust: (a) determinism by
  construction — the frozen file contains nothing environmental, so
  re-freeze byte-identity (proved this iter) turns git diff into the drift
  alarm; (b) the §8 versioning rule got a mechanical enforcer —
  verify-stream parses the live spec version out of CHECKSUM.md and fails
  any stream frozen under a different version, so spec-bump-without-
  refreeze cannot pass a gate; (c) freeze-stream refuses differing
  overwrites (--refreeze is a deliberate, visible act); (d) a verifier
  that has never failed is unproven — four negative tests exercised every
  rejection path before the gate was trusted. Manifest-as-single-source
  kills the params-copied-in-N-places class before task 7 multiplies the
  goldens.
- next: M0 task 6 — fdlibm-patched QuickJS oracle runtime + replay rig
  (oracle/qjs/build.sh + replay.sh g01) → QJS MATCH g01.

## iter 7 — 2026-07-14 — M0 task 6: fdlibm-patched QuickJS oracle runtime + replay rig
- phase: M0. task: fix_plan §M0 item 6 — oracle/qjs/{build.sh,qjs_oracle.c,
  sha256.c/h,shim.js,replay-main.js,replay.sh}: prove the meleelight sim
  replays bit-identically OUTSIDE a browser, on a runtime we control, with
  our fdlibm as the only Math (porting-strategies.md role).
- done-check: `bash oracle/qjs/build.sh && bash oracle/qjs/replay.sh g01`
  → `QJS MATCH g01`, exit 0 (.loop/iter7-m0t6-donecheck.log; CHECKER
  independent re-run .loop/checker/m0t6.log). Replay: 3600/3600 frames
  exact vs the frozen g01 stream via the UNCHANGED verify-stream.js,
  rngCalls=134, rngCallsOutsideStep=1, specVersion=1; ~9.3 s (~385 fps).
- artifacts: oracle/qjs/build.sh (bellard/quickjs pinned @
  42d08be5f28abfdf881110bba3713f6a256d8d97, VERSION 2026-06-04 — commit
  recovered by byte-matching the spike's surviving /tmp/qjs-src tree,
  .git was gone; all 18 consumed sources sha256-pinned in-script; host
  build + armv7 static cross-build COMPILE-ONLY per fix_plan, 906 KB,
  sha256 c96291775b082fd5…); qjs_oracle.c (embedder: Math table repointed
  at port/fdlibm C pre-JS; __qjs_sha256/__evalFile/__readFile/__writeFile;
  unhandled-rejection = fatal; __replayExit completion proof); sha256.c
  (FIPS 180-4, written from spec, NIST self-test at startup); shim.js
  (host-object shims, each with a documented why); replay-main.js (driver:
  VERBATIM oracle/harness/init.js+pagelib.js, so the serialization
  contract has ONE implementation per side); replay.sh (manifest-driven
  params, verdict by unchanged verify-stream.js). Run output
  oracle/qjs/out/qjs-g01.json (untracked, sha256 df6f633874a1649a…).
- evidence beyond the done-check: (1) negative test — QJS_ORACLE_NO_REPOINT=1
  dies at boot: "fdlibm repoint NOT active: Math.sin(-502630247.09…)"
  1-ulp host-libm mismatch caught by the 4110-input bitwise assertion
  (.loop/iter7-m0t6-neg-norepoint.log); (2) negative test — seed 1338 run
  rejected by verify-stream param pin; (3) negative test — wrong stage
  (--stage 1) diverges at frame 75: the stream is a live detector, not
  vacuous; (4) rerun determinism — two full replays both QJS MATCH
  (.loop/iter7-m0t6-rerun{1,2}.log); (5) boot RNG pin — browser boot
  consumes exactly 465 seeded draws (measured: 1 jQuery expando + 464
  stagerender bgStar, all module-eval, stable at +3 s), asserted == in
  the qjs driver since mulberry32 state is never re-seeded at setupMatch.
- found-and-fixed divergence (frame 403, interPolatedHitboxPhantom ±0.01):
  main.js feature-detects Web Storage via `typeof(Storage)`; without the
  Storage INTERFACE global, getCookie returns "" instead of localStorage's
  null, and getGameplayCookies treats "" as stored → Number("") = 0 for
  EVERY gameSettings entry (phantomThreshold 0.01→0). Fix: expose Storage
  in shim.js — path parity with the browser, not a value tweak.
- PROVISIONAL (auto-adopted): QuickJS pin = master-at-spike-date commit
  42d08be5 (not the 3d5e064e release commit — the spike's tree
  byte-matches only the former); timers/rAF registered-but-never-run under
  qjs (browser-parity argument documented in shim.js; every timer consumer
  is sim-irrelevant by harness construction); qjs run meta.fdlibm=true
  (fdlibm active in its strongest form — the Math table IS the C fdlibm,
  asserted bitwise at boot); QJS_ORACLE_NO_REPOINT env hook exists solely
  so the negative test can prove the assertion bites; arm cross-build kept
  inside build.sh (so the done-check exercises it) with QJS_SKIP_ARM=1
  iteration escape.
- CHECKER (mode=task, sub-agent): verified=true, tamper=false, gaps=[];
  evidence: independent full re-run exit 0 "QJS MATCH g01"; verify path
  confirmed exact-equality full-length via unchanged committed verifier;
  no hardcoded frame hashes in oracle/qjs (only the QuickJS source pins +
  NIST vectors); git diff --stat HEAD empty, only new files under
  oracle/qjs/ (M0-allowed); branch agent/auto.
- zoom-out: two CLASSES surfaced. (a) "missing browser global ≠ crash":
  feature detection converts absence into a SILENT path flip (Storage →
  "" → Number("")=0 across all gameSettings). One-off would be "set
  phantomThreshold"; the class fix shipped is path-parity shimming plus
  loud boot instruments (bitwise fdlibm assertion, RNG-draw pin 465) so
  environment drift dies at boot instead of surfacing as a latent
  mid-stream divergence — and the exact-equality stream remains the
  backstop that caught it (instrument > class fix > one-off held). (b)
  "two implementations of one contract drift": avoided twice by reuse —
  init.js/pagelib.js run VERBATIM under qjs, and the verdict comes from
  the same verify-stream.js the browser gate uses; the one collision this
  caused (pagelib's window.__sha256 clobbering the embedder's global —
  mutual recursion until "string too long") was fixed class-level by
  namespacing ALL embedder globals __qjs_*.
- next: M0 task 7 — record the full golden set g02–g08 (traces via
  gen-trace.js variants, record.sh freeze, manifest coverage) + commit
  oracle/verify_goldens.sh → ALL GOLDENS OK (then M0 phase-advance).

## iter 8 — 2026-07-14 — M0 task 7: golden set g02–g08 + oracle/verify_goldens.sh (FINAL M0 task)
- phase: M0. task: fix_plan §M0 item 7 — record the full golden set per
  the REPLAN composition contract and commit the M0 exit gate script.
- roster (manifest.json is the single param source; manifest seed = run
  seed = gen-trace.js seed, trace 3800 frames, 3600 verified):
  g01 fox/marth/battlefield 1337 (untouched) · g02 falco/puff/ystory 7302
  · g03 falcon/fox/pstadium 7303 · g04 puff/falcon/dreamland 7344 ·
  g05 marth/falco/fdest 7305 · g06 falcon/marth/fountain 7306 ·
  g07 falco vs CPU-falcon d5/battlefield 7307 · g08 fox vs CPU-puff
  d5/fdest 7308. Coverage: all 5 chars (each ≥2×), all 6 stages, 2 CPU.
- done-check: `bash oracle/verify_goldens.sh` → `ALL GOLDENS OK`, exit 0
  (.loop/iter8-m0t7-donecheck.log: 8× GOLDEN OK, 8× QJS MATCH, coverage
  assertion "8 goldens, chars {0,1,2,3,4}, stages {0,1,2,3,4,5}, cpu
  goldens: 2"). Per golden: 2 fresh browser runs → compare.js
  bit-identity → verify-stream.js BOTH runs vs frozen (exact equality,
  full 3600, RNG channels, spec/trace/param pins) → qjs replay judged by
  the same unchanged verifier.
- recording evidence: every record.sh double-run was IDENTICAL on the
  first attempt for all 7 new goldens (.loop/iter8-m0t7-record-{1,2}.log)
  — NO new nondeterminism from puff/falco/falcon movesets, the 5 new
  stages (incl. fountain platforms), or d5 AI; the spike-era seeding
  holds. QJS reproduced all 7 with zero shim changes
  (.loop/iter8-m0t7-qjs-g0{2..8}.log) — no new Storage-class gotcha.
  rngCalls: g02 125 · g03 119 · g04 115 · g05 185 · g06 160 · g07 81 ·
  g08 1496 (AI RNG usage is matchup-dependent; g08 pounds the stream —
  strong RNG-channel coverage). rngCallsOutsideStep == 1 everywhere.
- trace sanity (gameplay quality contract, applied uniformly before
  freezing — .loop/iter8-m0t7-sanity-{1,2}.log): ≥1 KO (DEAD*), ≥1
  DAMAGE*/CAPTUREDAMAGE (real hits), both players ≥1 stock at frame 3600.
  All 8 pass; endpoints (stocks/percents): g01 [4,2]/[43,0] ·
  g02 [4,1]/[0,0] · g03 [4,2]/[19,0] · g04 [4,3]/[8,7] · g05 [3,3]/[13,13]
  · g06 [2,3]/[13,10] · g07 [3,4]/[0,16] · g08 [1,4]/[15,94]. New states
  vs g01: CLIFF*, DOWNBOUND/DOWNWAIT/DOWNATTACK, THROWBACK/THROWNPUFFBACK,
  DAMAGEFLYN/DAMAGEFALL, SMASHTURN, JAB3, NEUTRALSPECIALAIR.
- found-and-rejected degenerates (g04, puff-P1/falcon-P2/dreamland): seed
  7304 hits-but-no-KO (puff P1's rollout — a MOVEMENT neutral-B, unlike
  fox/falco lasers — intercepts P2's scripted walk-off); 7314 P2 hit 0
  stocks → match ENDED before 3600 (post-match frames = weak trailing
  coverage); 7324/7354/7374 KOs but ZERO hit states (pure SDs);
  7334 nothing. 7344 passes all three criteria and is the frozen g04.
- artifacts (sha256 first 16): verify_goldens.sh 6a01ed5e35737584;
  frozen streams g02 1c8432eb9b635b0b · g03 760c99dfdceb6fa3 ·
  g04 1d7eeb6807524a86 · g05 6b32b5c5ecba698b · g06 16c0017bf75ad264 ·
  g07 9580e25b5e52b835 · g08 a49e8913315354d0; manifest.json +85 lines.
  g01 trace/stream byte-untouched (git status clean on them; its frozen
  rng behavior unaffected — no harness/shim/spec change this iteration).
- CHECKER (mode=task, sub-agent): verified=true, tamper=false, gaps=[];
  evidence: independent full gate re-run exit 0 ending "ALL GOLDENS OK"
  (.loop/checker/iter8-donecheck.log) with 8× GOLDEN OK + 8× QJS MATCH;
  no tolerance logic anywhere in the verification path (grep); all 16
  golden artifacts + manifest coverage confirmed; every frozen file
  specVersion=1 with params matching its manifest entry; diff touches
  only oracle/goldens/manifest.json + new files under oracle/ (M0's
  contract); branch agent/auto.
- PROVISIONAL (auto-adopted): roster matchups/stages/seeds (fix_plan
  composition honored: g02–g06 human-vs-human over the 5 remaining
  stages, g07–g08 CPU d5 on two distinct stages, every char ≥1×); the
  gameplay quality contract as the recording bar (documented in the
  manifest comment so re-recorders apply it); verify_goldens.sh checks
  BOTH fresh runs against the frozen stream (cheap belt-and-braces);
  CPU-golden filenames keep the plain gNN-<p1>-<p2>-<stage> convention
  (cpu flag lives in the manifest, the single param source).
- zoom-out: the g04 failures are a CLASS — open-loop scripted traces
  encode geometry/moveset assumptions (projectile neutral-B, stationary
  interception, walk-off reachability) that new char/stage combos break
  SILENTLY: the run still completes, the stream still freezes, coverage
  is just quietly weak. Per the hierarchy (instrument > class fix >
  registered one-off), the fix shipped is an INSTRUMENT — the measurable
  per-trace gameplay quality contract (KO + real hits + match-live
  endpoint) applied to every golden before freezing and recorded in the
  manifest — plus the gate's manifest-computed coverage assertion so the
  8-golden set can never silently regress below 5 chars/6 stages/1 CPU.
  The seed choice 7344 itself is the registered one-off riding on that
  instrument. Also NULL-result worth logging: the feared "new gameplay
  RNG site" class (spec §8 territory) did NOT materialize — seeding is
  global-Math.random-wide, so char-specific moves can't reach an unseeded
  site; first-attempt identity on all 14 recording runs is the evidence.
- next: M0 phase-advance iteration (type C-c): CHECKER mode=phase-advance
  re-runs `bash oracle/verify_goldens.sh` as the exit gate, then fix_plan
  Current phase → M1 and `MILESTONE PASS: M0` — driver-owned per the loop
  protocol.

## iter 9 — 2026-07-14 — MILESTONE PASS: M0 → phase-advance to M1 + REPLAN

MILESTONE PASS: M0

- Phase-advance (LOOP.md F-advance): M0's exit gate
  `bash oracle/verify_goldens.sh` was run and verified by the DRIVER
  (writer ≠ checker; 8× GOLDEN OK + 8× QJS MATCH, "ALL GOLDENS OK",
  exit 0); issue #14 closed by the driver. fix_plan.md
  `Current phase:` flipped M0 → M1.
- REPLAN (LOOP.md C-b; docs/loop/REPLAN.md): fix_plan.md §M1 rewritten
  into 5 ordered tasks, each with an exact runnable done-check:
  (1) pipeline skeleton + animations serializer
  (`bash pipeline/check-animations.sh` → ANIMATIONS OK) ·
  (2) extractor bundle + framedata/attributes/ECB/hitbox → generated C
  tables (`check-tables.sh` → TABLES OK) · (3) stage geometry → C tables,
  one source of truth with collision (`check-stages.sh` → STAGES OK) ·
  (4) SFX → 22050 mono S16LE PCM + music → 22050 stereo S16LE raw PCM +
  executed-JS sound map (`check-audio.sh` → AUDIO OK) · (5) full-run
  byte-stability + coverage gate (`verify_pipeline.sh` → PIPELINE OK).
  M1 EXIT GATE concretized into CLAUDE.md §Commands as
  `bash pipeline/verify_pipeline.sh` (two fresh full runs byte-identical
  + artifact re-hash + pinned expected.json coverage). §Gates table
  untouched (its M1 cell stays the binding definition; the runnable
  command lives in §Commands per the gate-concretization rule).
- Conventions fixed (fix_plan §M1 header): pipeline/ is host-node,
  executed-JS ONLY (built animations bundle via window-shimmed require;
  engine/stage/sound tables via a webpack extractor built with
  upstream's own docker node:8 toolchain — nothing hand-transcribed);
  deterministic manifest (sorted keys, sha256+bytes per artifact,
  upstream HEAD + source hashes, no timestamps, no absolute paths);
  pinned coverage in pipeline/expected.json (measured-then-frozen);
  formats specified in pipeline/FORMATS.md, little-endian pinned (host
  arm64 LE / device ARMv7 LE); audio artifacts marked Nintendo-derived
  PRIVATE, never distributed.
- Measured reconciliation backing the coverage pins (executed the built
  bundle in node, cross-checked against the src tree): anatomy's "754
  animation files" = 744 exported action states + 5 index.js + 5 DEAD
  falco files (ILLUSIONFX, THROWNDOC{BACK,DOWN,FORWARD,UP} — present on
  disk, never required by falco/index.js, referenced nowhere in
  src/main). Exact executed counts: 27,820 frames · 27,808 paths ·
  7,747,148 int16 coords (~15.5 MB raw, matches anatomy §2's ≈15 MB) ·
  max path length 650 coords (fits u16) · 0 null frames · 0 non-Int16Array
  paths. Audio: dist/sfx has 204 wavs, exactly 180 `new Howl` sites in
  src/main/sfx.js; 8 music oggs. Per-char states: marth 155 · puff 154 ·
  fox 142 · falco 148 · falcon 145.
- PROVISIONAL (auto-adopted): ANIM1 binary layout (per-char file: header
  + sorted state directory + string table + u32 frame-offset tables +
  frame records of u16 path counts / u16 coord counts / raw int16
  coords; documented in pipeline/FORMATS.md for the C side); node (not
  headless Chrome) as the animations-bundle executor — the bundle is
  pure Int16Array data construction, engine-neutral, and node execution
  of the SAME built artifact keeps the executed-JS principle intact;
  expected.json pins EXACT counts (stronger than PLAN's "~27.9k";
  measured-then-frozen like goldens, never hand-invented).
- next: execute M1 task 1 (pipeline skeleton + animations serializer).

## iter 9 (cont.) — 2026-07-14 — M1 task 1: pipeline skeleton + animations serializer

- done-check: `bash pipeline/check-animations.sh` → exit 0, ends
  `ANIMATIONS OK` (.loop/iter9-m1t1-donecheck.log; first measurement run
  .loop/iter9-m1t1-firstrun.log). Two FRESH runs → byte-identical
  manifest.json (sha256 d001897c48b2aea6…, stable across three runs
  total incl. the dev run); verify-artifacts re-hashed all 5 binaries
  both runs; check-expected asserted the full pinned contract INCLUDING
  the live 754-file reconciliation against the upstream src tree.
- shipped: pipeline/run.js (stage-registry runner, deterministic
  manifest: sorted keys, no timestamps/abs paths, upstream HEAD +
  source sha256 provenance) · pipeline/stages/animations.js (executed-JS:
  window-shimmed node require of the BUILT dist/js/animations.js, walks
  the live objects, hard in-run decoder round-trip on every coord) ·
  pipeline/lib/animbin.js (ANIM1 encoder + validating decoder — the C
  reference) · pipeline/lib/{manifest,verify-artifacts,check-expected}.js
  · pipeline/expected.json (pinned contract) · pipeline/FORMATS.md
  (ANIM1 spec v1, PROVISIONAL, little-endian pinned: host arm64 LE /
  device ARMv7 LE; absolute-offset header/state-directory/string-table/
  frame-offset-tables/frame-records; absent-frame sentinel 0; u16
  pathCount/coordCount with hard throws on overflow, max measured 650).
- measured (executed data, now frozen in expected.json): 5 chars ·
  744 states · 27,820 frames · 27,808 paths · 7,747,148 int16 coords ·
  0 absent frames · 0 irregular paths · 0 non-Int16Array paths · total
  15,734,864 artifact bytes (~15.0 MiB, matches anatomy §2's ≈15 MB;
  4 empty frames exist: marth 4, fox 4, falco 4 have frames<->paths
  deltas — frames with pathCount 0, kept verbatim). expected.json bytes
  value was pinned FROM the measured run (placeholder corrected before
  commit — values are executed, never invented).
- verification beyond the done-check (negative tests, not committed as
  artifacts): 1-byte artifact tamper → verify-artifacts HASH mismatch
  exit 1; stray file in run dir → exit 1; corrupted version field →
  decoder throws; end-to-end spot-check: fox APPEAL frame-1 path-0
  coords decoded from the binary == the raw upstream source literal
  (-13,-148,-17,-154,…) — source text → built bundle → executed →
  ANIM1 → decode, bit-faithful the whole chain.
- artifact hashes (sha256 first 16): check-animations.sh
  677aaa578f418491 · run.js 07da5b35b32f5137 · lib/animbin.js
  63107cb140f2f639 · stages/animations.js 5dfb7dd31e5e3ff7 ·
  expected.json 7027d3871544ba05.
- PROVISIONAL (auto-adopted): ANIM1 layout as specced (FORMATS.md §2);
  plain-node execution of the animations bundle (pure Int16Array data
  construction, engine-neutral — cross-checked against raw source
  literals; the browser path stays available via the oracle harness if
  a future stage ever needs engine-specific evaluation); empty frames
  serialized as pathCount 0 records (distinct from ABSENT frames,
  offset 0 — both cases in the spec).
- zoom-out: the serializer validates EVERYTHING it copies (int16 range,
  u16 widths, ASCII names, shape contract counts) and hard-throws
  rather than clamping — the CLASS to prevent is silent data coercion
  in a pipeline whose whole point is bit-faithfulness; the instrument
  is the in-run decoder round-trip + measured-then-frozen expected.json
  (any upstream or generator drift becomes a loud diff, exactly like
  the golden streams). Also registered: the dead-file reconciliation is
  an INSTRUMENT (re-derived live every check) rather than a comment, so
  the "754 files" claim can never silently rot.
- next: M1 task 2 — extractor bundle (docker node:8 webpack) +
  framedata/attributes/ECB/hitbox → generated C tables
  (`bash pipeline/check-tables.sh`).

## iter 10 — 2026-07-14 — M1 task 2: extractor bundle + engine tables → generated C

- done-check: `bash pipeline/check-tables.sh` → exit 0, ends `TABLES OK`
  (.loop/iter10-m1t2-donecheck.log). Two FRESH runs (animations+tables)
  → byte-identical manifest.json; verify-artifacts re-hashed all 8
  artifacts both runs; check-expected asserted the pinned tables coverage
  (+ animations, unchanged); tables-anim-xref matched the frozen
  framesData/ECB↔ANIM1 reconciliation; round-trip gate: compiled C
  tables (cc -std=c99 -O1 -ffp-contract=off -Wall -Wextra -Werror)
  printed a canonical leaf dump byte-identical to a fresh executed-JS
  walk — 38,832 leaf values, bit-exact.
- shipped: pipeline/extractor/{extractor.entry.js, extractor.config.js,
  build-extractor.sh} (webpack entry importing ONLY the per-char
  attributes/ecb data modules + the main/characters registries — no
  index.js aggregators, so no main.js god-module tentacles; built with
  upstream's own docker node:8 webpack + babel query identical to the
  game build's happypack loader; idempotent stamp cache) ·
  pipeline/stages/tables.js (CTAB1 generator: ml_tables.h with X-macro
  field lists + ml_f64() memcpy decoder, ml_tables.c data, tables.json
  canonical model) · pipeline/lib/tables-schema.js (pinned typing/order;
  validating executed-JS walk; hard-throws on typing violations) ·
  pipeline/lib/tables_check.c (C round-trip dump walker, implements
  FORMATS.md §3.6 via the generated X-macros) · pipeline/lib/tables-dump.js
  (fresh JS walk dump) · pipeline/lib/tables-anim-xref.js (live
  framesData/ECB vs decoded ANIM1 reconciliation vs frozen pins) ·
  check-tables.sh · FORMATS.md §3 (CTAB1 spec) · expected.json tables
  section (coverage + perChar + animXref lists) · check-expected.js
  gained an explicit stage-scope arg (check-animations.sh now passes
  "animations"; assertion strength for animations unchanged — verified,
  ANIMATIONS OK regression run .loop/iter10-checkanim-regression.log).
- measured (executed data, now frozen in expected.json): 5 chars ·
  46 attribute fields/char (35 f64 + 6 i32 + 3 i32-arrays + 2 bool, key
  set asserted exact) · framesData 80 states/char (400) · intangibility
  10/char (50) · ECB 755 states / 27,997 frames / 4 empty states (puff
  DEAD*, kept verbatim frameCount 0/NULL) · hitboxes 243 moves / 616
  hitboxes / 2,743 offset vecs / 21 single-Vec2D offsets · 6,277 f64
  values · 118,097 i32 values. ANIM1 xref (NOT uniform equality —
  measured): framesData 370 equal / 26 differ / 4 puff no-anim; ECB 715
  equal / 27 differ / 13 no-anim (falco dead-file states, TECHWALLJUMP
  class, falcon DEAD*/SLEEP) — exact sorted lists pinned.
- verification beyond the done-check (negative tests, not committed):
  1-ulp bit-pattern tamper in ml_tables.c → round-trip cmp fails;
  tampered artifact → verify-artifacts HASH mismatch exit 1; framesData
  119.5 injected → generator throws "expected int32"; extra attribute
  key injected → key-set drift throw; xref pin perturbed → xref exit 1
  (pin restored, verified). End-to-end spot checks source-text →
  bundle → executed → C → compiled dump: fox gravity 0.23 =
  3fcd70a3d70a3d71 ✓, fox fair1 size 5.156 = 40149fbe76c8b439 / dmg 7 ✓,
  fox ATTACKAIRB ecb[0] = 4,3,9,13 ✓, fox WAIT 120 / ESCAPEN 2,14 ✓.
- artifact hashes (sha256 first 16): check-tables.sh b303eb5452508d22 ·
  stages/tables.js 7da8aa726ef56a8f · lib/tables-schema.js
  e75c7674055e45a8 · lib/tables_check.c 7122cc57c20d4c5f ·
  lib/tables-dump.js 64dc8f16b5b70661 · lib/tables-anim-xref.js
  a574fec40685b677 · extractor.entry.js b30628cb3d474516 ·
  extractor.config.js de2b308c76c2f935 · build-extractor.sh
  2f6e72fc6a540087 · expected.json 7d4f41397ff600b7.
- PROVISIONAL (auto-adopted): CTAB1 layout + field typing (FORMATS.md §3;
  bits-authoritative doubles with decimal comments — the inverse pairing
  of classic fdlibm but the same value↔bits discipline; measured-integral
  + semantically-integral fields as C ints, waitAnimSpeed pinned f64 with
  its speed siblings); plain-node execution of the extractor bundle (pure
  data construction, engine-neutral — same basis as the animations stage,
  spot-checked against raw source literals); actionSounds + raw offsets
  registry exposed by the bundle but not emitted (sounds are task 4;
  offsets reach the engine only via hitbox objects); setVelocities/
  posOffset stay with the M2 sim port (attached to actionState function
  objects behind the god-module boundary).
- zoom-out: the defect CLASS this task guards against is silent value
  drift between executed JS and C — the instrument is the dual-dump
  round trip (compiled C data vs fresh executed walk, byte-compared,
  38.8k leaves) plus generator hard-throws on typing surprises, so a
  classification error is a build failure, not a quiet coercion. Second
  class surfaced: ASSUMED identities between related executed datasets
  (framesData vs animation frames "should match") are false upstream —
  registered the reconciliation-pin pattern (measure, freeze exact
  lists, re-derive live every check) as the standard instrument for
  every future cross-dataset check (stages/audio tasks 3-4). Also note
  my initial survey scan MISSED the 4 empty puff ECB arrays that the
  validating walk caught — surveys inform design, validators gate:
  never promote a survey observation to a contract without a hard check.
- next: M1 task 3 — stage geometry → generated C tables from the same
  extractor (`bash pipeline/check-stages.sh`).

## iter 11 — 2026-07-14 — M1 task 3: stage geometry → generated C tables (STAB1)

- phase M1, task: fix_plan §M1 task 3 (stage geometry → generated C from
  the same extractor bundle; 6 VS stages; ONE source of truth).
- done-check: `bash pipeline/check-stages.sh` → STAGES OK, exit 0
  (.loop/iter11-check-stages.log; CHECKER re-run .loop/checker/
  check-stages.log). Regressions: check-animations.sh → ANIMATIONS OK
  (.loop/iter11-checkanim-regression.log), check-tables.sh → TABLES OK
  (.loop/iter11-checktables-regression.log; also re-run by CHECKER —
  the extractor bundle changed, tables stage re-verified against it).
- CHECKER (separate sub-agent, mode=task): verified=true, tamper=false —
  evidence: STAGES OK exit 0 with 412-leaf bit-exact round trip; all cmp
  (no epsilon anywhere); artifacts fresh; TABLES OK 38,832 leaves;
  ANIMATIONS OK incl. live 754-file reconciliation; diff touches only
  pipeline/* and expected.json is additive-only (animations/tables pins
  byte-untouched); tables-schema.js refactor preserves all assertions.
- what landed: extractor entry now also imports upstream's OWN vs-stages
  aggregator → `window.__stages` (same bundle, task-2 infrastructure
  extended as planned); webpack `externals` stubs the exact request
  strings `main/main`, `../../main/main`, `stages/activeStage`,
  `../activeStage`, `../../physics/environmentalCollision` ("var {}") —
  ystory/fountain reference those ONLY inside movingPlatforms/
  updatePlatform bodies (M2 sim logic, never called at extraction; data
  literals verified self-contained). build-extractor.sh gained __stages +
  document.-leak hard guards (guard proven live: it first tripped on the
  word "document." inside my own entry-file comment — comments bundle
  verbatim; reworded). New: pipeline/stages/stages.js (STAB1 generator:
  ml_stages.{h,c} + stages.json) · lib/stages-schema.js (pinned schema +
  hard-throw walk; loadExtractor SHARED with tables — loader refactored
  into tables-schema.js so one window-shim serves every extractor
  global) · lib/stages_check.c + lib/stages-dump.js (dual-dump round
  trip) · check-stages.sh · FORMATS.md §4 (STAB1 spec) · expected.json
  stages section + check-expected.js stages block · run.js registration.
- measured (executed data, now frozen in expected.json): 6 stages ·
  polygon 6 rings/112 verts · grounds 12 · platforms 15 · ceilings 16 ·
  wallL 40 · wallR 40 · ledges 12 (all 2/stage, parallel ledgePos) ·
  connected 2 stages (ystory, fountain; only "g" labels at the pin) ·
  movingPlats 3 (ystory [0], fountain [1,2]) · 866 f64 · 177 i32 ·
  empty-list faithfulness: fdest platforms 0, ystory ceilings 0 (count
  0/NULL, kept verbatim). Round trip: 412 leaf values bit-exact
  (compiled C vs fresh executed-JS walk).
- verification beyond the done-check (negative tests, not committed):
  1-ulp bit tamper in ml_stages.c → dump cmp fails; tampered artifact →
  verify-artifacts exit 1; offset 600.5 injected → "expected int32"
  throw; SurfaceProperties third element injected → hard-throw (never
  silently dropped); extra stage key (background) → key-set throw;
  expected.json pin perturbed → check-expected exit 1 (restored). Spot
  checks source-text → executed → C: battlefield scale 4.5 =
  4012000000000000 ✓, fountain platform[1] = (platL 21 → 4035000000000000,
  22.125) with const substitution executed not transcribed ✓, ystory
  blastzone minX -175.7 = c065f66666666666 ✓, fdest ledgePos ±68.4
  (authored quirk vs its ±85.6 ground — battlefield copy-paste upstream)
  carried verbatim ✓.
- artifact hashes (sha256 first 16): check-stages.sh a3736e35894b1541 ·
  stages/stages.js 0966e4acc48823f8 · lib/stages-schema.js
  c3a2bfa3ca789c05 · lib/stages_check.c e40bbb2e98177ffa ·
  lib/stages-dump.js 04f09ec864d18ca3 · extractor.entry.js
  b45257f0574a2ff6 · extractor.config.js d3c0c3d5393c9280 ·
  build-extractor.sh 01d09048837aea97 · expected.json 742ecffa05a11428 ·
  lib/check-expected.js a6f4b62a92a49800 · lib/tables-schema.js
  c7c84ad7c6f9557a · run.js cb51c5c75e7c3b27 · FORMATS.md
  fb0bf286e93fc16b.
- PROVISIONAL (auto-adopted): STAB1 layout (FORMATS.md §4) — stage order
  = oracle --stage ids; authored (never sorted) geometry order because
  physics/render index surface lists positionally; offset typed int32[2]
  (screen pixels, integral + semantically integral); ml_stage_f64 named
  apart from ml_f64 so both generated headers can share a TU; externals
  stubbing as the god-module boundary mechanism for data modules whose
  FUNCTION BODIES (not data) reference the engine — movingPlatforms
  logic itself is M2 sim-port territory alongside setVelocities (§3.4).
- zoom-out: same defect class as task 2 (silent executed-JS↔C drift),
  same instrument (dual-dump round trip + hard-throw typing) — the
  pattern held with zero adaptation, confirming it as the standard for
  every generated-table stage. NEW class surfaced: BOUNDARY-CROSSING
  IMPORTS in otherwise-pure data modules (2nd instance of the god-module
  boundary; task 2 dodged it by not importing index.js, task 3 could
  not dodge — stages ARE the module with the imports). Registered
  instrument: externals-stub the exact request strings + a
  bundle-content hard guard (document.-leak) + exact-key-set asserts on
  every extracted object, so a stub that swallowed a REAL module fails
  the build, not the data. Also: the leak guard tripping on its own
  comment is a reminder that guards must be tested against the artifact
  they actually scan (comments are bundle bytes too) — a guard you never
  saw fire is an untested guard.
- next: M1 task 4 — audio conversion + sound map
  (`bash pipeline/check-audio.sh`).

## iter 12 — 2026-07-14 — M1 task 4: audio conversion + executed-JS sound map (SND1)

- phase M1, task: fix_plan §M1 task 4 (all 204 dist/sfx wavs → 22050 Hz
  mono S16LE raw PCM, all 8 dist/music oggs → 22050 Hz stereo S16LE raw
  PCM, + the sound map executed out of upstream's own main/sfx +
  main/music via the extractor bundle).
- done-check: `bash pipeline/check-audio.sh` → AUDIO OK, exit 0
  (.loop/iter12-check-audio.log; CHECKER re-run
  .loop/checker/check-audio.log). Regressions: ANIMATIONS OK / TABLES OK
  / STAGES OK (.loop/iter12-check-{animations,tables,stages}.log; also
  re-run by CHECKER — the extractor bundle changed, both table stages
  re-verified against it).
- CHECKER (separate sub-agent, mode=task): verified=true, tamper=false —
  evidence: AUDIO OK exit 0 (byte-stability cmp ×2, 214 artifacts
  re-hashed ×2, expected.json audio pins, no-commit guard, zero
  epsilon/tolerance anywhere in the verification path); all three
  regression checks green; diff surface exactly as declared (CLAUDE.md
  §Commands additive-only, expected.json audio section additive with
  animations/tables/stages pins byte-untouched); nothing under
  pipeline/build tracked by git.
- what landed: extractor entry imports upstream's own main/sfx (the 180
  named Howls INCLUDING its module-scope changeVolume pass) + main/music
  (8 MusicManager track Howls) → window.__sounds; loadExtractor gained a
  browser-parity shim (window === global, adds swept afterwards) + a
  Howl capture shim (records constructor cfg verbatim, loads no audio;
  upstream's own _volume writes land on the instances). New: stage
  pipeline/stages/audio.js (ffmpeg conversion + SND1 sounds.json +
  audio/README.md provenance notice) · lib/sounds-schema.js (pinned
  schema + hard-throw walk: cfg key sets, volume [0,1] as f64 bits,
  sprite windows int32, actionSounds referential integrity into the sfx
  map) · check-audio.sh · FORMATS.md §5 (SND1 spec incl. §5.3 volume
  semantics + §5.4 C-emission judgment) · expected.json audio section +
  check-expected.js audio block (incl. blob-shape and aggregate-hash
  recompute) · run.js registration · build-extractor.sh __sounds guard.
- measured (executed data, now frozen in expected.json): 204 sfx blobs
  (5,458,598 bytes / 2,729,299 samples) · 180 mapped sounds over 179
  distinct wavs (electricfizz.wav shared by electricfizz +
  loudelectricfizz) · 25 unmapped wavs (converted anyway; upstream
  content) · 1 looping sound (furaloop) · 8 music tracks (181,407,012
  bytes / 45,351,753 sample frames ≈ 34 min; SD-streaming budget fine
  per audio-spike) · actionSounds 5 chars / 65 states / 36 events, all
  referentially intact. KEY SEMANTIC FIND: every Howl's authored cfg
  volume is DEAD at runtime — sfx.js's own load-time
  changeVolume(sounds, 0.5) / changeVolume(MusicManager, 0.3) overwrites
  _volume with masterDefault × (volumeOverwrites[name] || 1); the map
  records the post-load effective volume (what Howler actually plays)
  AND the authored cfgVolume as provenance. Babel 6 drops the valueless
  `static whatisPlaying;` class property — Object.keys(MusicManager)
  yields exactly the 8 track Howls, which is the only reason upstream's
  own changeVolume doesn't crash at load; schema asserts the track set.
- verification beyond the done-check (negative tests, all restored):
  ffmpeg pin perturbed to 7.0.0 → stage hard-fails BEFORE converting
  ("would DRIFT frozen artifact bytes") · blob tampered (2 bytes
  appended) → verify-artifacts SIZE mismatch exit 1 · aggregate
  artifactsSha256 pin perturbed → check-expected exit 1. Faithfulness
  spot-checks: dash.pcm 6,773 samples = 0.3072 s vs source wav 0.307125 s
  (16000→22050 resample) ✓ · menu.pcm 4,417,729 frames = 200.351 s vs
  ogg 200.350521 s ✓ · volume 0.15 bits 3fc3333333333333 == python
  struct pack of 0.5×0.3 ✓ · marth JUMP → [[1,"jump"]] matches source ✓.
- artifact hashes (sha256 first 16): check-audio.sh 4db800301f56b655 ·
  stages/audio.js 160d1b216bfae26f · lib/sounds-schema.js
  2bf032fa7e674ed6 · lib/tables-schema.js d98d72d8ab9e2b65 ·
  lib/check-expected.js b40ffde29d689e98 · extractor.entry.js
  ee80674e83b1b741 · build-extractor.sh f7ee63cd0d878d46 · expected.json
  98a7d974c22ed088 · run.js 52cd133bcfe022c6 · FORMATS.md
  26078b9e4708630c. Frozen output aggregate (expected.json
  audio.artifactsSha256): e776a885df21edd4… over 214 artifacts,
  ffmpeg 8.1.1.
- PROVISIONAL (auto-adopted): SND1 as JSON-canonical with C emission
  deferred to the M4 mixer task (FORMATS.md §5.4 — the sound map's only
  consumer is the M4 mixer; emitting a C surface now would freeze a
  mixer-facing API before the mixer design exists; the M4 task generates
  its C table FROM sounds.json with the CTAB1 dual-dump discipline).
  Also: recording post-load effective volume as the primary `volume`
  (with authored cfgVolume kept) — executed-state-over-authored-text is
  the same principle as every other stage. Also: unmapped wavs converted
  anyway (upstream content; map records reachability).
- zoom-out: two prior classes held with zero adaptation (hard-throw
  schema walk = the silent-drift instrument; measured-then-frozen
  expected.json pins). NEW class surfaced and instrumented:
  TOOLCHAIN-VERSION-DEPENDENT OUTPUT BYTES — byte-stability ×2 runs the
  SAME binary twice and is structurally blind to version drift, so any
  stage whose bytes depend on an external tool build (ffmpeg resampler
  here; docker node:8 webpack already pinned by image tag for the same
  reason) needs an explicit version+argv pin that fails BEFORE
  generating, plus a frozen output aggregate that fails after. Same
  family as the browser-libm drift M0 solved with fdlibm — the fix is
  always: pin the transform, freeze the bytes. 2nd instance of the
  browser-global-identity gotcha (qjs getCookie class): sfx.js reads
  back window-assigned names as bare globals; shim window = global for
  parity of paths, not just survival — a detached window object
  "survives" every module except the one that matters.
- next: M1 task 5 — full-pipeline runner + M1 exit gate
  (`bash pipeline/verify_pipeline.sh`), then the phase-advance iteration.

## iter 13 — 2026-07-14 — M1 task 5: full-pipeline runner + M1 exit gate (FINAL M1 task)

- phase M1, task: fix_plan §M1 task 5 — `pipeline/verify_pipeline.sh`,
  the M1 EXIT GATE recorded in CLAUDE.md §Commands (REPLAN iter 9). §M1
  is now EMPTY; the next iteration is the M1 phase-advance (driver/
  CHECKER owns re-running the gate and advancing the phase — NOT done
  here by design).
- done-check: `bash pipeline/verify_pipeline.sh` → PIPELINE OK, exit 0
  (.loop/iter13-verify-pipeline.log — first full run, 57s wall;
  .loop/iter13-verify-pipeline-final.log — clean re-run of the exact
  committed script after negative-testing).
- what landed (ONE new file + bookkeeping; the task COMPOSES iters 9-12,
  zero changes to any existing check): verify_pipeline.sh orchestrates
  (b) all four task-level checks UNCHANGED (each keeps its own fresh
  double-run + stage-specific gates: ANIMATIONS/TABLES/STAGES/AUDIO OK),
  then (a) one whole-pipeline double-run from a clean slate into
  build/gate-{a,b} — manifest.json byte-identical (cmp) + every artifact
  re-hashed in BOTH dirs with stray rejection (identical manifests +
  per-file hash verification in both => all 225 artifacts byte-identical
  across runs), (c) the FULL expected.json contract on the integrated
  run (check-expected.js DEFAULT scope = all four sections: 5 chars /
  744 states / 27,808 paths / live 754-file reconciliation / 6 stages /
  204 SFX blobs + 180 mapped sounds / 8 tracks / ffmpeg version+argv
  pins + frozen audio aggregate e776a885…), (d) compiled round-trips
  against gate-a's OWN artifacts — ml_tables.c and ml_stages.c compile
  (cc -ffp-contract=off) and their canonical dumps match fresh
  executed-JS walks with leaf counts PINNED in the gate (38832 + 412;
  scratch in build/gate-rt, removed) plus tables-anim-xref on gate-a,
  (e) no-commit guard over ALL of pipeline/build (gitignored `build*/`,
  nothing tracked/staged).
- evidence (from .loop/iter13-verify-pipeline.log): ANIMATIONS OK ·
  TABLES OK (38832 leaves) · STAGES OK (412 leaves) · AUDIO OK (214
  artifacts ×2) · full-run byte-stability cmp OK · verify-artifacts 225
  OK ×2 · check-expected "audio, animations, tables, stages (incl. live
  754-file reconciliation)" OK · integrated round-trip 38832+412
  bit-exact · no-commit guard OK · gate wall-time 57s.
- negative tests (all restored, restore proven byte-identical via cmp):
  perturbed manifest copy → cmp exit 1 · set -euo pipefail propagation →
  failing subcommand aborts · END-TO-END: leaf pin 412→413 sed'd into
  the actual script → full gate run exits 1 at exactly that assert
  (.loop/iter13-negtest-pin.log), script restored + final clean pass
  re-run.
- artifact hashes (sha256 first 16): pipeline/verify_pipeline.sh
  4d79777b62e125e1 · fix_plan.md 3dce2fe4c41b90c4. No other file
  touched; CLAUDE.md §Commands already carried the gate entry verbatim
  (REPLAN iter 9) — nothing to append there.
- PROVISIONAL (auto-adopted): gate composition = four unchanged
  task-level checks + ONE whole-pipeline double-run + gate-a-local
  round-trips/xref, accepting repeated work (~10 stage executions,
  57s total) over refactoring the checks for reuse — wall-time is sane
  and the assignment/LOOP forbid weakening any existing check; the
  round-trips are re-run against the INTEGRATED run (not only the
  --only runs the task checks use) so the gate stands alone even if a
  task check's scope drifts. Leaf counts 38832/412 pinned INSIDE the
  gate (measured-then-frozen; a silent shrink of either dump can no
  longer pass on cmp-equality alone).
- zoom-out (MILESTONE-LEVEL — what M1 produced as CLASSES, per CLAUDE.md
  rule 8): M1's real yield beyond the artifacts is four reusable
  instruments the M2+ grind inherits: (1) EXECUTED-OVER-AUTHORED as the
  universal extraction stance — every stage executes upstream's own
  built/bundled code and serializes live values (never transcribes
  source text); its three god-module/browser-global gotcha instances
  (index.js boundary, externals-stubbed sim imports, window===global)
  are all the SAME lesson: reproduce the runtime's paths, not just its
  survival. (2) MEASURED-THEN-FROZEN pins (expected.json, leaf counts,
  dead-file lists, xref reconciliations) as the anti-assumption
  instrument — every "obvious" identity we tested was FALSE somewhere
  (754≠744, framesData≠anim frames, ECB≠anim, authored volume≠played
  volume); never assert an identity you haven't measured, freeze what
  you measured. (3) PIN-THE-TRANSFORM + FREEZE-THE-BYTES for any
  toolchain-dependent output (ffmpeg version+argv+aggregate; docker
  node:8 by image tag; fdlibm in M0) — byte-stability ×2 alone is
  structurally blind to version drift. (4) DUAL-DUMP ROUND-TRIP
  (compiled C ↔ fresh executed-JS, bit patterns not decimals) as the
  generated-code gate — M2's translation grind should reuse exactly
  this shape per module (slice-dump both sides, cmp). The gate itself
  is the composition proof: one command, every instrument armed.
- next: M1 PHASE-ADVANCE iteration (LOOP F-advance: CHECKER
  mode=phase-advance re-runs `bash pipeline/verify_pipeline.sh`,
  flips Current phase → M2-CAL, logs MILESTONE PASS: M1).

## iter 14 — 2026-07-14 — M1 PHASE-ADVANCE

- phase M1 → M2-CAL (LOOP F-advance). CHECKER mode=phase-advance: re-ran
  the M1 EXIT GATE `bash pipeline/verify_pipeline.sh` fresh → PIPELINE OK,
  exit 0, 64s wall (.loop/iter14-m1-gate.log). All sub-gates green:
  ANIMATIONS OK · TABLES OK (38832 leaves) · STAGES OK (412 leaves) ·
  AUDIO OK · full-run byte-stability + 225-artifact re-hash ×2 ·
  expected.json contract incl. live 754-file reconciliation ·
  integrated compiled round-trips bit-exact · no-commit guard.
- fix_plan.md `Current phase:` flipped to M2-CAL; M1 section closed.
- next: REPLAN M2-CAL (concretize the 3 seed items + exit gate).

MILESTONE PASS: M1
