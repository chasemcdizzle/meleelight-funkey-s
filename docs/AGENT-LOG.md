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

## iter 15 — 2026-07-14 — REPLAN: M2-CAL concretized

- phase M2-CAL, REPLAN iteration (LOOP C-b): the 3 seed items rewritten
  with runnable done-checks; M2-CAL exit gate concretized into CLAUDE.md
  §Commands as `bash port/sim/check-envcoll.sh` (ENVCOLL MATCH, exit 0).
- key decisions (PROVISIONAL, auto-adopted; full rationale in fix_plan
  §M2-CAL preamble): (1) runtime module-boundary wrap via
  served-bytes-only webpack-cache exposure — oracle/ untouched (HARD RULE
  3), every capture run judged by the unchanged verify-stream.js against
  the frozen stream so instrumentation cannot silently perturb; (2) canon
  v1 value serialization = CHECKSUM.md structure with bit-pattern-hex
  numbers (injective, stricter-or-equal, keeps the Ryu-class shortest
  float formatter out of the measured translation rate — it is a known
  one-time M2 cost, priced separately); (3) capture set g01+g04+g06
  (anchor + puff collision stress + fountain moving platforms making the
  stage arg vary per call).
- next: task 1 (capture rig).

## iter 16 — 2026-07-14 — M2-CAL task 1: module-boundary capture rig

- phase M2-CAL, task 1 (fix_plan §M2-CAL item 1).
- done-check: `bash port/sim/calib/check-capture.sh` → CAPTURE OK, exit 0
  (.loop/iter16-check-capture.log): per golden (g01/g04/g06) two fresh
  capture runs byte-identical JSONL, 6× STREAM MATCH against frozen
  goldens (unchanged verify-stream.js — instrumentation non-perturbation
  PROVEN, not assumed), pins OK.
- what landed: port/sim/calib/{FORMAT.md, capturelib.js, run-capture.js,
  check-capture-pins.js, check-capture.sh, expected-capture.json}.
  oracle/ untouched (HARD RULE 3) — the runner reuses
  oracle/harness/{init,pagelib}.js + port/fdlibm/fdlibm.js verbatim by
  path; the ONLY intervention is a served-bytes-only injection exposing
  the webpack module cache, then wrapping the 13 exported functions of
  the environmentalCollision module object post-load.
- measured (now frozen in expected-capture.json): g01 119,619 records ·
  g04 34,052 · g06 33,004 (186,675 total). Notables: findCollision fires
  65,751× in g01 only (fox/falco laser articles); getSameAndOther 4× in
  g06 only (real corner collisions — angular rets present); five exports
  (hLine*/vLine*/lineThrough) have ZERO live call sites in these traces;
  frame-0 (setup-time) boundary calls: none.
- gotcha class logged (JS-semantics, for the M2 brief): upstream leaves
  player ECB1 UNINITIALIZED on frame 1 — Vec2D{x:undef,y:undef} flows
  into runCollisionRoutine (2 records, both frame 1). undef behaves as
  NaN under every arithmetic/comparison path in the module (ToNumber),
  and NO return value ever carries undef (pinned invariant) — so the C
  replay parser maps arg-undef -> canonical NaN 0x7ff8000000000000. If a
  ret ever carried undef the pin fails loudly instead of silently
  mismatching.
- negative test: perturbed run-meta count -> CAPTURE PIN FAIL, exit 1.
- zoom-out: the capture rig is an INSTRUMENT (hierarchy top): the same
  served-bytes webpack-cache wrap generalizes to ANY module boundary for
  M2's module-by-module slices — nothing module-specific except the
  export list + the stage projection rule.
- next: task 2 (structure-parallel C translation + replay driver).

## iter 17 — 2026-07-14 — M2-CAL task 2: structure-parallel C translation + replay driver

- phase M2-CAL, task 2 (fix_plan §M2-CAL item 2).
- done-check: `bash port/sim/calib/check-replay-runs.sh` → build OK
  (cc -O2 -ffp-contract=off -Wall -Wextra -Werror), REPLAY RAN 119619
  records, 0 divergences, exit 0 (.loop/iter17-check-replay.log). Also
  ran g04 + g06: 34,052 and 33,004 records, 0 divergences each
  (.loop/iter17-replay-g0{4,6}.log). ALL 186,675 recorded boundary calls
  replay BIT-IDENTICAL on the first successful build (one prior compile
  fix: -Werror unused variable; zero behavioral fixes).
- what landed: port/sim/{environmental_collision.{c,h}, ml_js.h,
  stage_types.h, util/{vec2d,lin_alg,find_smallest_within,
  solve_quadratic_equation,line_angle,extreme_point,ecb_transform,
  zip_labels,draw_ecb}.h} — full module, no stubs (all 13 exports incl.
  the five with zero live call sites); port/sim/calib/{canon.{c,h},
  replay_envcoll.c, check-replay-runs.sh}. JS slice 1,689 lines →
  2,209 lines C translation (+798 lines rig).
- comparator negative tests (all restored, tree verified clean):
  (a) transcription typo (line2.b.x for line2.a.x in ONE term of
  coordinateInterceptParameter) → 11,883 divergences, first at line 155;
  (b) single corrupted nibble in a capture copy → exactly 1 divergence
  at exactly that line; (c) instructive NON-test: +1 ulp on
  additionalOffset (1e-5) → 0 divergences — mathematically expected
  (the 1.6e-21 absolute change vanishes below every consumer's result
  ulp), a reminder that constant-level ulp probes are NOT valid
  comparator tests here.
- zoom-out (the calibration's real finding, for the M2 brief): the zero
  divergence count is not luck — it is the yield of DESIGN-STAGE class
  prevention: (1) js_max/js_min/js_sign helpers instead of fmax/fmin
  (NaN + ±0 semantics differ); (2) canonical-NaN mapping for
  ToNumber(undefined) args (pinned by the capture invariant); (3)
  key-presence modeling (DT_ABSENT) so optional damageType keys
  serialize exactly as JS construction sites create them; (4) fdlibm on
  both sides; (5) -ffp-contract=off everywhere; (6) expression shapes
  copied verbatim (no algebraic "cleanup"); (7) capture-first workflow —
  reading real record shapes (undef ECB1s, damageType key patterns,
  surface echoes) BEFORE finalizing the value model. These become
  mandatory rules in every M2 module brief.
- next: task 3 (gate script + M2CAL-REPORT.md + metrics/projection).

## iter 18 — 2026-07-14 — M2-CAL task 3: burn-down + metrics + GO recommendation

- phase M2-CAL, task 3 (final task; §M2-CAL now empty — next iteration is
  the phase-advance, driver/CHECKER-owned as with M1).
- done-check: `bash port/sim/check-envcoll.sh` → ENVCOLL MATCH, exit 0
  (.loop/iter18-check-envcoll.log; from-scratch run with captures deleted:
  .loop/iter18-gate-fresh.log — 3 fresh recordings, 3× STREAM MATCH,
  3× pins OK, 3× 0-divergence strict replay). Gate negative-tested:
  VERDICT needle removed from the report → exit 1.
- THE MEASUREMENT (full table + caveats: docs/M2CAL-REPORT.md):
  1.69 KLOC JS slice → 186,675 boundary records over 3 goldens →
  0 divergences (0 div/KLOC); burn-down opened and stayed at zero
  (trivially converged, no oscillation); wall ~40 min M2-CAL total
  (~25 min/KLOC translate+converge); comparator sensitivity proven
  (typo → 11,883 divs; corrupt nibble → exactly 1). Projection:
  ~14–19 KLOC remaining ⇒ 8–12 comparable slices ⇒ 15–30
  agent-iterations for M2 at 2–3× pessimism for unpriced classes
  (mutation-heavy modules, float formatter, integration). PLAN §4
  NO-GO conditions (can't converge without epsilon-cheating; >10×
  budget): neither is remotely near.
- VERDICT: GO — recommend the loop proceeds to M2 on phase-advance.
- zoom-out (M2-CAL milestone-level): the calibration's product is not
  the zero — it is (a) the 7 mandatory design-stage prevention rules
  (M2CAL-REPORT §3) that turned anticipated divergence classes into
  non-events, now binding for every M2 module brief; (b) the reusable
  module-boundary instrument (capture rig + canon replay), which makes
  every M2 module slice-verifiable BEFORE integration exactly as PLAN §4
  M2 requires; (c) the honest caveat register (§6) telling M2 where its
  first real divergences will come from (in-place mutation, key
  iteration order, the Ryu-class formatter).
- next: M2-CAL PHASE-ADVANCE (driver re-runs the gate, flips Current
  phase → M2, logs MILESTONE PASS: M2-CAL; GO path — no human gate).

## iter 19 — 2026-07-14 — M2-CAL PHASE-ADVANCE + M2 REPLAN

- phase-advance (LOOP §F-advance): re-ran the M2-CAL exit gate
  `bash port/sim/check-envcoll.sh` → ENVCOLL MATCH, exit 0
  (.loop/iter19-envcoll-gate.log — 3× STREAM MATCH, 3× pins OK,
  186,675/186,675 records 0 divergences, report needles present).
  Verdict in docs/M2CAL-REPORT.md §7: GO — no human gate on this path.
  Current phase flipped M2-CAL → M2 in fix_plan.md.

MILESTONE PASS: M2-CAL

- REPLAN (same driver-directed iteration, docs/loop/REPLAN.md): fix_plan
  §M2 concretized into 17 dependency-ordered cluster tasks, each with an
  exact runnable done-check on the M2-CAL pattern (capture cluster
  boundary over goldens → structure-parallel C → strict replay to
  bit-identical → CLUSTER MATCH). Measured the true remaining surface
  during recon (physics 3,841 + moves 28,716 + main-slice + input +
  util; attributes/ECB/stage data are M1 tables, not code). Conventions
  fixed: generalized spec-driven capture rig; mutation-capture (post-state
  field) for mutating clusters; the 7 M2CAL-REPORT §3 prevention rules
  binding on every task; check-envcoll.sh as the per-iteration regression
  guard. M2 EXIT GATE concretized into CLAUDE.md §Commands as
  `bash port/sim/check-sim.sh` (all 8 goldens, unchanged verify-stream.js,
  full length, rngCalls pins — built by task 17).
- judgment calls (PROVISIONAL, auto-adopted, documented in fix_plan §M2
  conventions): (a) AI-as-recorded-input bridge for g07/g08 — PLAN keeps
  ai.js JS-side until M4 yet the gate covers CPU goldens; upstream already
  consumes AI as an input bank identical to human input, so the bridge
  records aiInputBank + RNG-draw counts non-perturbingly (write-set recon
  hard-checked in task 16) and M4's port replaces it live; (b) documented
  util skips (firstNonNull dead upstream, deepValue target-only,
  randomAnnulusPoint vfx-only, deepCopy family + createHitBox family
  deferred to their value-model tasks).
- zoom-out: the REPLAN's structure IS the class-level control for the
  grind: one reusable instrument (spec-driven capture/replay rig) +
  inherited prevention-rule list (extensible at rule 8+) instead of 17
  ad-hoc verification schemes.
- next: task 1 (util/math substrate).

## iter 20 — 2026-07-14 — M2 task 1: util/math substrate

- phase M2, task 1 (fix_plan §M2 item 1).
- done-check: `bash port/sim/calib/check-util-replay.sh` → UTIL MATCH,
  exit 0 (.loop/iter20-check-util-replay.log): per golden g01/g04/g06 —
  2 fresh captures byte-identical, 2× STREAM MATCH (non-perturbation),
  pins OK, strict replay 0 divergences. Totals: 1,970,207 boundary
  records (g01 887,889 · g04 276,114 · g06 806,204) across 34 wrapped
  functions in 11 util modules. Conformance guard:
  `bash port/sim/check-envcoll.sh` → ENVCOLL MATCH, exit 0
  (.loop/iter20-envcoll-regression.log) — the M2-CAL gate stays green
  after the rig generalization AND the canon v1.1 change (measured
  no-op: zero NaNs in the frozen envcoll captures).
- what landed: FULL structure-parallel C for every sim-imported util
  module — port/sim/util/{vec2d.h (+dot, +JsNum/JsVec2D undef-at-rest
  accessor model), lin_alg.h (all 11 linAlg exports), box2d.h,
  segment2d.h, to_list.h, find_smallest_within.h (+seed form),
  detect_intersections.h (4 exports + private helpers)}; rig
  generalization port/sim/calib/{capturelib.js (spec-driven engine,
  envcoll spec built in — fresh envcoll capture proven BYTE-IDENTICAL to
  the frozen M2-CAL one, .loop/iter20-envcoll-parity.log), spec-util.js,
  run-capture.js --spec, replay_util.c, check-spec-pins.js,
  expected-capture-util.json, check-util-replay.sh}; FORMAT.md → canon
  v1.1 + spec/undef-allowlist docs. Documented skips (fix_plan §M2
  conventions): firstNonNull (dead upstream), deepValue (target-only),
  randomAnnulusPoint (vfx), deepCopy/createHitBox families (their
  value-model tasks).
- burn-down: first strict replay of g01 → 108 divergences in exactly TWO
  classes, both fixed at CLASS level (zero record-level patching):
  (1) 84× getXOrYCoord undefined-echo (accessor semantics) → rule 8;
  (2) 24× solveQuadraticEquation NaN-payload mismatch (V8 payload
  propagation vs clang FADD commutation; 130 dotProd records matched
  only by hardware luck) → canon v1.1 + rule 9. Re-capture + replay:
  0 divergences across all three goldens.
- comparator negative tests (all restored, tree verified): transcription
  typo (dotProd(lineVec,…) for dotProd(pointVec,…) in ONE term of
  orthogonalProjection) → 11,544 divergences; single corrupted capture
  nibble → exactly 1 divergence at exactly that line; pins tamper
  (appended bogus record) → CAPTURE PIN FAIL, exit 1.
- honest coverage note: 16/34 boundary fns have zero live records over
  these traces (Segment2D trio, Vec2D#dot, distanceToLine/intersectsAny/
  lineDistanceToLines, inverseMatrix/multMatVect/norm/scalarProd/reflect/
  manhattanDist, flipXOrY, squashECBAt, ecbFocusFromAngularParameter) —
  translated + compiled anyway (M2-CAL report §6 precedent);
  squashECBAt/ecbFocus are exercised inside the C envcoll replay via
  internal calls; the rest gain live records from later clusters
  (hitDetection creates Segment2Ds) or stay dead upstream.
- zoom-out: both divergence classes were INSTRUMENT-level fixes
  (canon/value-model), not per-record patches, and are now inherited by
  every remaining M2 task as mandatory rules 8/9 (fix_plan §M2). The
  rule-9 lesson generalizes: exact-bit comparison must be exactly as
  strict as JS VALUE semantics — stricter (raw NaN payloads) manufactures
  unreproducible divergences, looser (epsilon) is forbidden; canon v1.1
  is the fixed point. Also: the spec-driven rig means every remaining
  cluster's capture is now a ~100-line spec file + a replay dispatch,
  not a new instrument.
- next: task 2 (player/game-state value model + mutation-capture rig
  upgrade).

## iter 21 — 2026-07-14 — M2 task 2: player value model + mutation-capture rig upgrade

- phase M2, task 2 (fix_plan §M2 item 2).
- done-check: `bash port/sim/calib/check-player-model.sh` → PLAYER MODEL
  MATCH, exit 0 (.loop/iter21-check-player-model.log): per golden
  g01/g04/g06 — 2 fresh player-spec captures byte-identical, 2× STREAM
  MATCH (non-perturbation), pins OK (7,200 records each, 5-field
  post-state form), strict replay 0 divergences across all 21,600
  snapshots (round-trip + deep-copy independence + merge property per
  record). Conformance guards: `bash port/sim/check-envcoll.sh` →
  ENVCOLL MATCH, exit 0 (.loop/iter21-envcoll-regression.log);
  `bash port/sim/calib/check-util-replay.sh` → UTIL MATCH, exit 0
  (.loop/iter21-util-regression.log) — both stay green after the rig's
  post-state upgrade (4-field records byte-identical to before).
- what landed: value model `port/sim/ml_player.h` (MlPlayer/MlPhysics/
  MlHitboxes/MlHitboxSpec + JsBool undef-at-rest + ml_player_copy/
  ml_hitboxes_copy/ml_hitboxes_merge_from — the type-specialized
  deepCopy/deepObjectMerge); canon bridge port/sim/calib/
  player_canon.{h,c} (strict marshaller, rule 7 + sorted-key serializer;
  reusable for later clusters' pre/post player states); driver
  replay_player.c; spec-player.js (wraps physics.js's exported
  physics(i,·) — update(i) itself is main.js-internal to gameTick, NOT
  namespace-wrappable; physics is update's tail so post-physics ==
  post-update); rig upgrade capturelib.js (optional post callback →
  5th tab field) + check-spec-pins.js (postStateFns: 5-field records
  enforced for mutation-captured fns, 4 for all others);
  expected-capture-player.json; check-player-model.sh; NEW capture-FIRST
  instrument survey-shapes.js (per-path type/key-set/length report);
  FORMAT.md (post-state field, player spec + projections). Artifact
  hashes (sha256/12): ml_player.h fe294870902f · player_canon.c
  572dac69abca · player_canon.h 00412d85664a · replay_player.c
  7205b03dc4ec · spec-player.js d533376fdfd6 · survey-shapes.js
  1ee5717c428a · check-player-model.sh f469d51f4e3b ·
  expected-capture-player.json 59c24278876b.
- capture-FIRST payoff (survey over 21,600 snapshots BEFORE writing the
  struct): hitboxes.id[j] takes exactly TWO shapes — constructor
  ActiveHitbox {offset: Vec2D} vs chars-data {offset: Vec2D[1..24],
  +clank/hitAirborne/hitGrounded/throwextra} (move code aliases id[j] to
  chars[c].hitboxes entries); 13 runtime-added presence-modeled fields
  (IASATimer, inAerial, hit.reverse, hitboxes.frames, phys.grabTech/
  laserCombo/ledgeHangTimer/autocancel — lowercase, coexists with
  constructor autoCancel — /rollOut×7); phys.canWallJump undef-at-rest
  in 87% of snapshots (rule 8 extended to bools, JsBool); phys.passing
  runtime-added but always present post-update (physics.js:1067 is the
  1st statement). Zero cyc/fn tokens in the domain.
- burn-down: 0 divergences on the first successful build (one required-
  key count fixed pre-replay: 75 phys keys, not 74). The prevention rules
  held; no new record-level fixes.
- comparator negative tests (all restored, tree verified): out-of-domain
  key injected into one record → MARSHAL FAIL, exit 3
  (.loop/iter21-negA.log); single serializer byte perturbed ("frame"→
  "framf") → 7200/7200 divergences (.loop/iter21-negB.log); merge typo
  (target active[0] surviving) → 163 divergences (.loop/iter21-negC.log);
  4-field physics record → CAPTURE PIN FAIL, exit 1
  (.loop/iter21-pins-neg.log). NOTE (gotcha class, now in CLAUDE.md
  §Commands): a corrupted capture NIBBLE round-trips cleanly (exit 0) —
  round-trip model checks are self-referential for value edits, so model
  tasks' teeth come from model/serializer perturbations, not data
  corruption (unlike computational replay tasks).
- discoveries → rules: RULE 10 (fix_plan §M2) — upstream deepObjectMerge's
  3-arg sim call sites leave exclusionList undefined, falsifying the
  recursion guard: the "deep merge" is a SHALLOW per-key REFERENCE
  assignment (prevFrameHitboxes arrays become ALIASES of hitboxes').
  RULE 8 extended (undef-at-rest bools; void-mutator undef returns).
- honest coverage: merge frames-retention branch (target keeps its own
  `frames` when source lacks it) has ZERO live cases in these captures —
  property-tested in replay_player.c, first live coverage from task 5's
  physics capture, which also owns the live in-frame aliasing (rule 10)
  and the g07/g08 CPU-trace shapes (task 16).
- zoom-out: the two instruments built here are class-level controls, not
  one-offs — survey-shapes.js turns "guess the value model" into a
  measured enumeration for EVERY remaining cluster (rule 7 now has a
  runnable tool), and the 5th-field post-state form is the single
  mutation-capture mechanism tasks 3-14 inherit (no per-cluster capture
  formats). The negative-test lesson (round-trip ≠ compute-replay teeth)
  generalizes to task 15's formatter check design: differential vs JS
  String(x), never self-referential.
- next: task 3 (interpretInputs + input buffer + meleeInputs).

## iter 22 — 2026-07-14 — M2 task 3: interpretInputs + input buffer + meleeInputs

- phase M2, task 3 (fix_plan §M2 item 3).
- done-check: `bash port/sim/calib/check-input-replay.sh` → INPUT MATCH,
  exit 0 (.loop/iter22-check-input-replay.log): per golden g01/g04/g06 —
  2 fresh input-spec captures byte-identical, 2× STREAM MATCH
  (non-perturbation, sweep included), pins OK (750,992 records each,
  counts identical across goldens BY STRUCTURE — see
  expected-capture-input.json comment), strict replay 0 divergences
  across all 2,252,976 records. Conformance guards all green:
  `bash port/sim/check-envcoll.sh` → ENVCOLL MATCH
  (.loop/iter22-envcoll-regression.log); `bash
  port/sim/calib/check-util-replay.sh` → UTIL MATCH
  (.loop/iter22-util-regression.log); `bash
  port/sim/calib/check-player-model.sh` → PLAYER MODEL MATCH
  (.loop/iter22-player-regression.log) — util/player re-captured through
  the modified run-capture.js, proving the sweep hook is a no-op for
  sweepless specs.
- boundary problem + solution: interpretInputs (main.js:668) is
  main.js-internal to gameTick (built bundle: direct `input[i] =
  interpretInputs(` calls, verified — NOT namespace-dereferenced), so it
  cannot be wrapped. Its OUTPUT is captured as a physics args projection
  [i, inputBuffers[i]] (gameTick:1065-1067 makes inputBuffers[i] exactly
  interpretInputs' return, or gameTick:919's fresh nullInputs() during
  the 90-frame starting window — measured: pollInputs 7020 = 2×3510,
  physics 7200 = 2×3600). The replay is a full-trace CHAIN: C rebuilds
  every frame's 8-deep buffer from ITS OWN previous output + the recorded
  pollInputs injection and must match the projection bit-exactly — z/s
  always-shift (main.js:673), pause-aware pastOffset (:680), pause/
  frameAdvance bookkeeping, end-of-tick frameByFrame handling, over
  3600 frames × 2 slots × 3 goldens.
- what landed: value model port/sim/ml_input.h (plain bool/double —
  survey-shapes.js over the captures measured ZERO undef-at-rest in the
  22-key Input domain, unlike the player model); translations
  port/sim/input/{melee_inputs.h (full meleeInputs.js incl. quadrant
  renormalization via lin_alg inverseMatrix/multMatVect, discretise/
  unitRetract/meleeRound), input.h (inputData with the verbatim
  r<-list[5]/l<-list[6] swap, nullInput(s), pollInputs seam),
  interpret_inputs.{c,h} (MlInputSimState = the god-module's
  input-globals slice: pause/frameAdvance [4][2] init true per
  main.js:165-166, controllerResetCountdowns, giveInputs,
  wasFrameByFrame; ml_input_out_of_domain traps on the
  lifecycle/AI/network arms)}; ml_js.h += js_round (ECMAScript
  Math.round = V8 Float64Round: r=ceil(x); if (r-0.5>x) r-=1 — ties
  toward +Inf, -0 preserved, exact where floor(x+0.5) is wrong); rig:
  spec-input.js (11 wrapped fns + deterministic 1,780-call sweep),
  run-capture.js generic `spec.sweep()` hook (frame-0 records, before
  setupMatch), replay_input.c (chain driver + strict marshal),
  input_canon.{h,c} (reusable 22-key bridge for tasks 5/16),
  expected-capture-input.json, check-input-replay.sh; FORMAT.md (input
  spec + sweep section). Artifact hashes (sha256/12): ml_input.h
  ddc44b194d74 · ml_js.h c4fbec42def1 · melee_inputs.h abd552f00261 ·
  input.h 6edaf10c6dc5 · interpret_inputs.h d79b6c0e13a1 ·
  interpret_inputs.c 76b191c3cb1f · input_canon.h 0f732a488565 ·
  input_canon.c c9c2e29ae794 · replay_input.c 8d85095ace75 ·
  spec-input.js 229a58277470 · expected-capture-input.json 21f36e293792
  · check-input-replay.sh 25040ef523ce · run-capture.js 22c8570f319f.
- burn-down: 0 divergences on the first successful build across all
  three goldens — rules 1-10 held (js_round was written to spec BEFORE
  first replay, capture-FIRST survey preceded the value model).
- comparator negative tests (all restored, tree re-verified 0 div):
  (a) z-history shift typo (slot[7-k] for slot[6-k]) → 280 divergences,
  first diff exactly at a z byte (.loop/iter22-negA-detail.log);
  (b) js_round → naive floor(x+0.5) → 73 divergences, ALL in frame-0
  sweep records — proof the sweep catches what zero live records never
  would; (c) single corrupted capture bool in one physics buffer →
  exactly 1 divergence at exactly that line; (d) appended bogus record →
  CAPTURE PIN FAIL, exit 1. NOTE: a corrupted nibble in a deaden ARG
  replays clean (deadzone maps both values to 0) — args are inputs, not
  assertions; teeth live in ret/projection fields.
- honest coverage: pastOffset=0 (pause-frozen history), the pause/
  frameAdvance edges, interpretPause's playing toggle, AI/gamepad arms,
  and the startGame/endGame combos have ZERO live cases (goldens never
  pause by quality contract — traces press z but playing=true keeps
  frameByFrame false). Translated verbatim; behavior needing another
  cluster's surface traps via ml_input_out_of_domain (AI bank = task 16,
  lifecycle = task 17) rather than guessing. First live coverage of the
  pause paths would need a pausing trace — noted for task 17's judgment.
- zoom-out: task 1 registered "zero live records — translated anyway" as
  accepted debt; this task turned that CLASS into an instrument — the
  spec-level synthetic-domain sweep (rule 11, FORMAT.md), applied where
  the dead-in-capture surface has a real future domain (meleeInputs IS
  the M3 device stick path per b0xx-mapping §3). The negative test
  quantified why: a genuine Math.round semantics bug is invisible to
  100% of live traffic here (73/0 sweep/live divergences). Remaining
  clusters with boilerplate-dead branches (moves' interrupt chains)
  inherit the mechanism. Also reusable: input_canon marshals the Input
  shape once for tasks 5 (physics consumes inputBuffers) and 16 (AI
  bridge records aiInputBank Inputs).
- next: task 4 (actionStateShortcuts + state-machine scaffolding,
  C mulberry32 + sound-event queue seam).

## iter 23 — 2026-07-14 — M2 task 4: actionStateShortcuts + state-machine scaffolding + C mulberry32 + sound-event queue seam

- phase M2, task 4 (fix_plan §M2 item 4).
- done-check: `bash port/sim/calib/check-asshort-replay.sh` → ASSHORT
  MATCH, exit 0 (.loop/iter23-asshort-check.log): per golden g01/g04/g06
  — 2 fresh asshort captures byte-identical, 2× STREAM MATCH
  (non-perturbation incl. the rule-12 impure-but-restored sweep), pins OK
  (32,201 / 35,076 / 30,239 records), rngBoot pin OK (per-golden seed +
  465 boot draws — the BROWSER boot-draw count equals the qjs oracle boot
  pin, cross-confirming the boot-stream class), strict replay 0
  divergences across all 97,516 records (ret AND {dsp,mut,rng,snd}
  post-state). Conformance guards all green: check-envcoll.sh → ENVCOLL
  MATCH (.loop/iter23-envcoll-regression.log), check-util-replay.sh →
  UTIL MATCH (.loop/iter23-util-regression.log), check-player-model.sh →
  PLAYER MODEL MATCH (.loop/iter23-player-regression.log),
  check-input-replay.sh → INPUT MATCH (.loop/iter23-input-regression.log).
- what landed: capture spec-asshort.js (29 wrapped exports with READ-SET
  arg projections; owner-stack event attribution: sounds via 180 wrapped
  Howl.play, dispatches via non-recording loggers on every actionStates
  deep-copy entry + the 5 moves-index tables + JUMPAERIALB/F module
  objects, seeded draws as standalone Math.random records or post rng
  lists; frame-0 rngBoot record [seed, bootDraws] -> fast-forwarded
  state). C: port/sim/ml_rng.h (mulberry32 == init.js:32-40 bit-exact),
  port/sim/ml_events.{h,c} (sound-event queue seam for the M4 mixer +
  dispatch-note seam + logged ml_random), port/sim/
  action_state_shortcuts.{h,c} (verbatim translations of all 29 exports;
  charAttributes/intangibility through generated CTAB1 ml_tables — FIRST
  consumer of the M1 data path, live-cross-checked by the
  executeIntangibility sweep against the real marth table rows;
  actionStates registry as_setupActionStates/as_lookup/as_dispatch with
  opaque MlMoveDef until tasks 7-12, mechanics driver-self-checked),
  replay_asshort.c (chained mulberry32 draw-for-draw over every recorded
  seeded draw incl. the off-step pre-frame-1 startGame draw, asserted
  exactly 1 standalone frame-0 draw; strict marshals; post envelope
  compare), expected-capture-asshort.json (counts + undefRetAllowed +
  postStateFns + rngBootDraws=465), check-asshort-replay.sh (regenerates
  ml_tables via the pipeline, builds with -ffp-contract=off, 2×capture +
  verify-stream + pins + rngBoot pin + strict replay + no-commit guard);
  FORMAT.md "asshort" section; fix_plan §M2 rule 12. Artifact hashes
  (sha256/12): action_state_shortcuts.c 64144ffc64b2 ·
  action_state_shortcuts.h 68cafbb70644 · ml_rng.h 046bdcb7ac7c ·
  ml_events.c f2c715066772 · ml_events.h c8e8b4e614a9 · spec-asshort.js
  38608c3f05ff · replay_asshort.c 8a70f272897a ·
  expected-capture-asshort.json f9855295d1f0 · check-asshort-replay.sh
  57161996c716.
- burn-down: 0 divergences on the first successful build across all three
  goldens (rules 1-11 held; capture-FIRST survey preceded the value
  modeling — measured versusMode=0 in harness matches, IASATimer always
  present in live IASA records, shieldstun/applyDouble/lock truthiness
  domains, keyboard-quantized axes).
- comparator negative tests (all restored, tree re-verified 0 div):
  (a) shieldTilt formula constant 0.01→0.011 → 307 mut divergences;
  (b) rngBoot fast-forward off-by-one → 135 divergences (state pin at
  line 1 + every subsequent draw — chain teeth); (c) foxshout3→foxshout9
  → 11 divergences, ALL in frame-0 sweep records (sound-event stream
  teeth; zero live randomShout traffic exists to catch it); (d) spurious
  dispatch on checkForIASA's false arm → 3 divergences on g06's live
  records (the must-not-dispatch teeth); (e) single corrupted capture
  nibble → exactly 1 divergence; (f) appended bogus record → CAPTURE PIN
  FAIL exit 1. LESSON: a 0.79→0.78 checkForDash threshold nudge produced
  ZERO divergences — trace inputs are keyboard-quantized and no live axis
  value falls in (0.78, 0.79]; negative tests must perturb across values
  that OCCUR (recorded in rule 12).
- honest coverage: randomShout and executeIntangibility had ZERO live
  records over all three goldens (traces never land smash/throw shouts,
  never roll/tech) — covered by the rule-12 sweep (swapped-RNG KO-shout
  calls covering every shout outcome ×5 chars; restored player[3]
  injection exercising real intangibility rows trigger/no-trigger). Turbo
  interrupts (turbo off in goldens), shieldDepletion's break branch,
  isFinalDeath's true arms, and ALL positive dispatch paths (checkForIASA
  never fires an aerial/jump cancel live) remain zero-live: translated
  verbatim, dispatch verified must-not-fire on every live record + event
  teeth via sweep; first positive live coverage arrives with the moves
  clusters' traces or task 17.
- zoom-out: (1) rule 11's "sweeps must be pure" was too coarse — the real
  invariant is NET purity (restore exactly + never draw the seeded
  stream), and the rig's existing guards (×2 byte-stability, STREAM
  MATCH) mechanically PROVE net purity, so the class fix is rule 12
  rather than new machinery. This unlocks sweep coverage for a whole
  class of read-player/impure-effect boundaries that rule 11 wrote off.
  (2) The RNG channel is now an instrument, not just a spec paragraph:
  boot draws measured in-browser (465) match the qjs boot pin — two
  independent runtimes agreeing on the boot stream — and every future
  cluster that draws (hitDetection task 6 has no draws; moves/articles
  do) inherits the chained draw-for-draw verification for free via
  standalone Math.random records. (3) The sound-event queue seam
  established here is the M4 mixer's contract: events are now part of
  the verified boundary surface, so growing the checksum surface with
  sound events (CHECKSUM.md §7 note) has a C-side consumer design
  already validated.
- next: task 5 (physics.js core + interpolatedCollision — the
  mutation-heavy per-player update pipeline; pre/post player + stage).

## iter 24 — 2026-07-14 — M2 task 5: physics.js core + interpolatedCollision — PHYSICS MATCH

- phase M2, task 5 (the mutation-heavy per-player update pipeline — the
  cluster M2CAL-REPORT §6 flagged as the unpriced mutation class).
- done-check: `bash port/sim/calib/check-physics-replay.sh` → exit 0,
  `PHYSICS MATCH` (.loop/task5-donecheck.log). Per golden (g01/g04/g06):
  2 fresh captures byte-identical (cmp), both runs STREAM MATCH vs the
  frozen streams (3600/3600 exact; instrumentation non-perturbing incl.
  the finalCheck asFlags re-dump), pins OK
  (expected-capture-physics.json), strict replay 0 divergences —
  17,188 + 14,805 + 15,067 = 47,060 records: 21,600 physics(i,·)
  mutation captures (full pre/post envelopes), 21,441 dispatch seams,
  120 getter seams, 3,896 interpolatedCollision (3,791 live + 105
  rule-11 sweep), 3 asFlags dumps.
- conformance guard + regressions all green (.loop/task5-reg-*.log):
  ENVCOLL MATCH · UTIL MATCH · PLAYER MODEL MATCH · INPUT MATCH ·
  ASSHORT MATCH (player_canon.c changed this iteration — the task-2
  round-trip stays byte-exact with the rule-8 extensions).
- design: physics(i, inputBuffers) wrapped with a PRE-state args envelope
  (players/stage/globals + rule-10 alias probes; stage projected per
  record → fountain moving platforms faithful) and a POST envelope
  {alias, hq, players, snd}. NOT-YET-TRANSLATED surfaces are ORACLE-FED
  SEAMS consumed by the C replay in strict call order (FIFO): move
  dispatches (tasks 7-12) verify phase+moveName (via the asFlags
  table)+slot+extra-args bit-exactly then RESYNC the sim from the
  recorded post-dispatch state; hitDetection launch getters (task 6)
  verify args bit-exactly and inject the recorded return. hitQueue rows
  attributed by mark/collect (resetHitQueue reassigns the array each
  tick — push is unwrappable); physics' 4 direct sound sites on the
  ml_events seam. ecbSquashData (module-private, unreadable) chained in
  C across the file; the shared nullSquashDatum is provably never
  written (physics.h note 3). asFlags: frame-0 dump of the 14
  per-(char,state) flags physics branches on, drift-checked post-run by
  the new run-capture.js finalCheck() hook.
- DIVERGENCE LEDGER (frame · function · root-cause class · fix · min):
  1. frame 0 · load_flags(canGrabLedge) · captured domain wider than
     modeled (plain `false`; false[k] is undefined in JS, no throw) ·
     bool arm → [falsy,falsy] semantics (rule-7 marshal catch working
     as designed) · 5m.
  2. frame 1 · cv_player(phys.ECB1/ECBp) · rule-8 undef-at-rest is not
     field-level only — Vec2D COMPONENTS hold undefined in frame-1
     pre-physics states (and mid-frame dispatch posts) · class fix:
     ecb1Undef/ecbpUndef component masks in ml_player.h (values carry
     canonical NaN — every consumer is arithmetic/comparison) +
     phys.passing became presence-modeled (absent pre-first-call) · 25m.
  3. frame 1 · dealWithLedges(canGrabLedge trap) · trap hoisted above
     the snap-box guard upstream short-circuits on — false abort where
     JS never reads the flag · moved traps to the exact lazy dereference
     sites (rule 13b) · 10m.
  4. frame 2856+ (22 live records) · hitlagSwitchUpdate velocity
     integration · JS COMPOUND ASSIGNMENT groups its whole RHS:
     `pos.x += cVel.x + kVel.x` is pos.x + (cVel.x + kVel.x); the C
     translation left-flattened it — 1-ulp FP divergence class ·
     class fix + NEW RULE 13a · 15m.
  Zero divergences after the four class fixes; no one-offs.
- comparator negative tests (all restored, tree re-verified 0 div):
  (a) shieldHP regen 0.07→0.071 → 971 divergences; (b) land
  WAIT/LANDING cVel.y threshold −1→−3 → 8 (dispatch-name seam teeth);
  (c) spurious hq push at the hitlag-exit site → 30 (hq envelope teeth);
  (d) getLaunchAngle lsX/lsY swap → 12 = every getter record (seam args
  teeth); (e) merge alias flags untracked → 26 (post alias-probe
  teeth); (f) sweepCC overlap midpoint (q1 coeff) → 2, sweepCC
  quadratic a1 → 1, sweepAABB line-case pt → 1 (sweep+live
  interpolatedCollision teeth); (g) corrupted POST nibble → exactly 1.
  MEASURED SEAM PROPERTY: a corrupted PRE nibble that the next dispatch
  resync overwrites before any observable read is MASKED (0 div) —
  oracle-fed seams bound each record's sensitivity to state the C code
  actually consumes; nibble teeth must target POST fields.
- honest coverage (documented, not silent): damaging-stage collisions
  (hq rows) have zero live cases — no VS-stage surface carries
  damageType (grep-verified); reachable future domain = M4 target
  stages; hq teeth proven by (c). The pos→ECB1[0] alias WRITE-THROUGH is
  modeled but zero-live-OBSERVABLE (visible only via grounded movement
  under a low ceiling — moveAlongGround; no golden reaches one); the
  alias STATE itself is probe-verified every record. Turbo interrupts
  remain zero-live (off in all goldens) — wired to the as_turbo* C
  translations, self-flagging through the seam FIFO if ever reached.
  aabbChecker's corner-case sweep parameter is ordering-observable only
  (the returned point is the corner regardless of t) — translated
  verbatim; its selection lattice is teeth-covered via sweepCC (same
  code shape).
- artifacts (sha256/12): physics.c 78c2dd5a48d8 · physics.h 57f6a30fdc0c
  · interpolated_collision.c e341e64c6206 · interpolated_collision.h
  1301d273809d · spec-physics.js a5757dcb1e91 · replay_physics.c
  11fac3027df1 · expected-capture-physics.json dd7f60a08435 ·
  check-physics-replay.sh 7c94e5580bcd · ml_player.h 4e5e577fa99a ·
  player_canon.c f5e27b486bbe · run-capture.js 7e0cf1406ed5. Logs:
  .loop/task5-donecheck.log, .loop/task5-reg-*.log,
  .loop/task5-capture*.log.
- zoom-out: (1) the ONLY live-divergence class in a 1,233-line
  mutation-heavy translation was an operator DESUGARING (compound
  assignment grouping) — rules 1-12 pre-empted everything else; rule 13
  now pins desugarings + trap placement as part of "expression shape",
  which every remaining cluster (moves, hitDetection — both full of
  `+=`) inherits. (2) The oracle-fed dispatch seam is the class-level
  instrument for translating AROUND untranslated mutation surfaces: it
  verifies the caller's control flow (site order + names + args) while
  the recording supplies the callee's effects — tasks 6-12 can now be
  built in ANY order against the same physics capture, and each task
  that lands replaces a seam with real C, with the seam FIFO as the
  drift alarm. (3) Discovered for task 17: outOfCameraTimer is
  incremented by the RENDER/camera plane but consumed by sim logic
  (percent++ — checksummed); the full C gameTick needs that camera
  slice ported or the field's writer identified and translated —
  registered here so integration doesn't chase it as a mystery
  divergence. (4) The rule-8 arc completed: undef-at-rest now spans
  numbers (task 1), bools (task 2), and COMPONENTS of nested value
  objects (this task) — the class instrument (survey-shapes before
  finalizing any model) caught each extension before it cost a
  wrong-model rewrite.
- next: task 6 (hitDetection + hitQueue + hitbox value model — the
  getter seams recorded here become its first live cross-checks).

## iter 25 — 2026-07-14 — M2 task 6: hitDetection + hitQueue + hitbox value model — HITDET MATCH

- phase M2, task 6 (queue-ordered hit resolution: hitDetect fills the
  hitQueue, executeHits resolves rows, checkPhantoms settles deferred
  phantom damage; plus the launch getters task 5 left as oracle-fed
  seams — their REAL C bodies land here).
- done-check: `bash port/sim/calib/check-hitdet-replay.sh` → exit 0,
  `HITDET MATCH` (.loop/task6-donecheck.log). Per golden (g01/g04/g06):
  2 fresh captures byte-identical (cmp), both runs STREAM MATCH vs the
  frozen streams (3600/3600 exact; instrumentation non-perturbing incl.
  the finalCheck hdFlags re-dump), pins OK (expected-capture-hitdet.json
  incl. ZERO pins for the 15 internal-only exports and Math.randomW),
  strict replay 0 divergences — 18,348 + 18,285 + 18,344 = 54,977
  records: 54,000 mutation-captured pipeline calls (hitDetect×2 +
  executeHits + checkPhantoms + resetHitQueue per frame, full pre/post
  envelopes with the hq/phq queue value model), 34 dispatch seams, 636
  pure records (live physics/article callers + the 164-call rule-11
  sweep), 903 RNG-chain records (3 rngBoot + 301 standalone draws +
  screenShake's owner draws in post rng lists) — the chain replays ONE C
  mulberry32 draw-for-draw across each file.
- conformance guard + regressions all green (.loop/task6-reg-*.log):
  ENVCOLL MATCH · UTIL MATCH · PLAYER MODEL MATCH · INPUT MATCH ·
  ASSHORT MATCH · PHYSICS MATCH (player_canon.c + check-spec-pins.js
  changed this iteration — every prior spec's check re-verified).
- design: uniform mutation envelopes (pre {alias, characterSelections,
  gameMode, gameSettings{phantomThreshold}, hq, phq, playerType,
  players}; post {alias(3), hq, phq, players, rng, snd}); the queues are
  marshalled from each record's pre-state (rows enter from OTHER
  clusters' windows — THROW moves during update, physics' damaging-stage
  rows — so chaining is impossible by construction). Dispatches from
  inside hitdet boundaries (CATCHCUT/DAMAGEN2/DAMAGEFLYN(+!isThrow bool)/
  GUARD/SHIELDBREAKFALL/FURASLEEPSTART/CAPTUREPULLED/THROWNFALCONDIVE/
  WAIT/DOWNDAMAGE/CAPTUREDAMAGE/CAPTURECUT .init + .onClank/.onPlayerHit)
  are oracle-fed seams verified in call order and resynced from post
  {alias, hq, players}. RNG: the asshort rngBoot/standalone-draw
  discipline + a NEW instrument for the owner-draw vs dispatch-window-
  draw order ambiguity (below). hit_detection.c models both measured
  MlHitboxSpec shapes' undefined-key reads (CONSTRUCTOR entries lack
  clank/hitGrounded/hitAirborne/throwextra: falsy/never-loose-equal,
  undefined != 6 TRUE), the hitList push/splice write-through of the
  rule-10 prevFrameHitboxes alias, the throw arm's pos-reassignment
  alias break, executeHits' HELD hitbox reference as a by-value copy
  (reassignment-safe), upstream's dead hurtboxState-typo arm
  (documented constant-false), and traps at every JS-throw site
  (bluntHit's string-arg drawVfx, cssHits' rpsPoints, the
  getLaunchAngle prompt arm). charAttributes.weight reads via M1 CTAB1
  ml_tables (2nd consumer of the generated data path).
- DIVERGENCE LEDGER (fn · root-cause class · fix · min):
  1. getKnockback (live article records) · captured arg domain wider
     than modeled — article passes RAW actionStates crouch/vCancel
     flag reads (undefined for most states); the strict marshaller
     hard-failed exactly as designed (rule 7) · truthiness domain
     bool|undefined (cv_truthy_bu) · 5m.
  Replay divergences after first successful build: ZERO on all three
  goldens (rules 1-13 held; no expression-shape or desugaring misses).
  Rig-scale class fix: check-spec-pins.js read the whole JSONL via
  readFileSync — node's ~512 MB string cap (ERR_STRING_TOO_LONG) at
  g06's 542 MB capture; rewritten as a line STREAM with byte-identical
  checks (class fix — any future spec's capture can exceed the cap).
- comparator negative tests (all restored, tree re-verified 0 div):
  (a) corrupted POST players nibble → exactly 1; (b) hurtWidth 8→12 →
  2 (8→8.5 bit NOTHING — occurring hit margins exceed 0.25; rule-12
  corollary, 2nd measured instance); (c) getKnockback +18→+18.01 → 8
  (sweep + live); (d) DAMAGEFLYN threshold 80→30 → 36 dispatch-seam
  divergences (80→79 bit nothing — no occurring kb in [79,80));
  (e) screenShake 4→3 draws → 50 (chain misalignment cascade);
  (f) swordweakhit typo → 9 (snd teeth); (g) hitList alias mirror
  skipped → 16 (rule-10 teeth); (h) phantom classification inverted →
  14 = every direct hit (storedPhantom-arm teeth; the 0.01
  phantomThreshold band itself is unbitable on these traces).
- honest coverage (documented, not silent): zero live records for
  clanks (hitHitCollision/interpolatedHitHitCollision/CATCHCUT/onClank),
  phantoms (storedPhantom rows, checkPhantoms' settle arm — phq empty
  all 10,800 live calls), throws (isThrow rows/THROWNFALCONDIVE — no
  golden lands a grab-throw), executeGrabTech, shieldbreak, powershield,
  jabReset/DOWNDAMAGE, FURASLEEPSTART + furaloop.stop, ground-bounce,
  electric hits (shocked), interpolated shield/hurt variants, cssHits
  (gameMode 2) and the stage-damage arm (M4 target stages) — translated
  verbatim, guarded by ml_hd_out_of_domain traps at upstream throw
  sites, teeth proven by (a)-(h). Live coverage DID include: regular
  hits with launch (DAMAGEN2 ×27, DAMAGEFLYN ×3 on g06 incl. the
  fire-type kb>=140 sound tier), shield hits (GUARD ×3), grabs
  (CAPTUREPULLED ×3, CAPTUREDAMAGE ×3), fox-laser article calls
  (getKnockback/knockbackSounds ×12 live on g01), and both hq row
  shapes (6-elem shield + 7-elem regular).
- artifacts (sha256/12): hit_detection.c 4fbe26a47c03 · hit_detection.h
  857a646228b7 · spec-hitdet.js 03eb6489a80a · replay_hitdet.c
  597cf8cc78f7 · expected-capture-hitdet.json 9343c2f48f2c ·
  check-hitdet-replay.sh a8c517ac0275 · check-spec-pins.js cfe33cf8415e
  · player_canon.c 244402c08c37 · ml_player.h 9690e25aab1b · ml_events.c
  a449043b39fb · ml_events.h f2edc682e136 · FORMAT.md dff57baa0cc8.
  Logs: .loop/task6-donecheck.log, .loop/task6-reg-*.log,
  .loop/task6-capture-*.log.
- zoom-out: (1) NEW RULE 14 (fix_plan §M2): when a boundary both draws
  the seeded stream itself AND contains dispatch windows whose moves may
  draw, the owner-draw/window-draw order is unrecoverable from the
  record stream — measure it, record window draws under a distinct name
  (Math.randomW), pin the count, hard-fail on one. The moves clusters
  (tasks 7-12; move code draws shouts AND calls other moves) inherit
  this instrument — without it their replays would silently assume an
  order. (2) The rule-12 corollary is now a measured CLASS (2nd
  instance): threshold nudges smaller than the data's occurring margins
  are no-op teeth (task 4: keyboard-quantized axes; here: hit-geometry
  margins > 0.25 and the 0.01 phantom band) — negative tests must FLIP
  a classification, not graze a boundary. (3) Held-object-reference
  semantics extend rule 10's lesson from "copy helpers may alias" to
  "seam resyncs must not rebind upstream's held references": executeHits
  holds `hitbox` across dispatches — the C by-value copy is the
  translation of object identity, and the same pattern will recur in
  every move that caches a player field object across an init call.
  (4) The rig scaled past a platform limit (node string cap) rather
  than a design limit — the streaming pins checker is the class fix and
  the capture format needed no change; envelope size is dominated by
  players canon ×6 records/frame, which task 17's integration will NOT
  pay (it replays traces, not captures).
- next: task 7 (characters/shared moves — the move-object template
  {name,init,main,interrupt}; the physics AND hitdet dispatch seams both
  become live cross-checks for its boundary captures).

## iter 26 — 2026-07-14 — M2 task 7: characters/shared moves — MOVES SHARED MATCH

- phase M2, task 7 (the shared move set: 79 move objects
  {name,init,main,interrupt(,land)} driven by player[p].timer — the
  MlMoveDef table's first real bodies; task 5's physics dispatch seams
  and task 6's hitdet dispatch seams are this cluster's live
  cross-checks by construction: the same trace's dispatches now execute
  as real C bodies under this spec's own boundary).
- done-check: `bash port/sim/calib/check-moves-shared-replay.sh` →
  exit 0, `MOVES SHARED MATCH` (.loop/task7-donecheck.log). Per golden
  (g01/g04/g06): 2 fresh captures byte-identical (cmp), both runs
  STREAM MATCH vs the frozen streams (3600/3600 exact; finalCheck
  mvData re-dump drift-guarded), pins OK
  (expected-capture-moves-shared.json), strict replay 0 divergences —
  4,985 + 5,351 + 5,061 = 15,397 records: 14,943 mutation-captured
  top-level shared-move phase calls (args [phase,name,[slot,...extras],
  inputs(8-deep x4),pre]; post {alias4,hq,players,rng,snd,vfx}; 44
  frame-0 rule-11/12 sweep records per golden on a separate sweep
  mulberry32), 210 mdispatch seams (per-char inits from shared
  interrupt chains — JAB1/FORWARDTILT/GRAB/NEUTRALSPECIAL*/ATTACKAIR*/
  DOWNATTACK/CATCHATTACK), 238 standalone seeded draws, 3 mvData +
  3 rngBoot records. The seeded chain replays ONE C mulberry32
  draw-for-draw (owner draws: CAPTUREWAIT mash wiggle — live on
  g01+g04's grab phases — LANDING-family circleDust 4-draw sites,
  DEAD* screenShake; window draws via seam rng lists — measured ZERO).
- conformance guard + regressions ALL GREEN (.loop/task7-reg-*.log):
  ENVCOLL MATCH · UTIL MATCH · PLAYER MODEL MATCH · INPUT MATCH ·
  ASSHORT MATCH · PHYSICS MATCH · HITDET MATCH (ml_events.{c,h} gained
  the vfx queue and physics.h's MlStageX gained respawnPoints/
  respawnFace this iteration — every prior spec re-verified against the
  rebuilt TUs).
- design: boundary = every SHARED-ORIGIN actionStates entry's phase fns
  (1212 wrapped; origin MEASURED by fn identity vs the shared index
  module — deepCopyObject(true,·) deep-copies data, copies FUNCTIONS by
  reference; puff overrides FURAFURA/JUMPAERIALB/JUMPAERIALF → her
  entries are per-char seams, task 12). Records fire outside any move
  record's scope (inScope==0): under TOP-LEVEL per-char windows
  recording is chain-safe (window draws are standalone records pushed
  at draw time), under an attributing record's seam window it is NOT
  (would misorder the chain) — nested shared→shared calls stay
  transparent (the C tree calls its own bodies). hq is carried OPAQUE
  (reserialized canon string): no shared move imports hitDetection's
  queues — seam resyncs swap the string whole. C: port/sim/characters/
  shared/moves/*.c (79 files, structure-parallel, 5,548 lines) +
  moves_index.c/moves.h (mv_dispatch via the task-4 as_lookup registry;
  MvX extras pack; AsTri phase returns — upstream interrupt arms that
  FALL THROUGH without a return (SMASHTURN/TILTTURN tilt arms,
  OTTOTTO/OTTOTTOWAIT's 2nd GUARDON arm, RUN's chain end, SQUATWAIT's
  restart, ENTRANCE, STOPCEIL's FALL arm, CAPTURE* grabbedBy===-1
  guards) return undefined, carried verbatim). Data planes: framesData/
  charAttributes/charHitboxes("thrown") from CTAB1 ml_tables (3rd/4th
  consumers of the M1 generated path — DAMAGEFLYN's thrown.id0 write is
  live on g06, cross-checking the hitbox table emission); index.js
  setVelocities/posOffset patches + CAPTUREDAMAGE.setPositions +
  actionSounds rows + palettes[pPal] from the frame-0 mvData dump (NEW
  RULE 15). Sweep (44 calls): a REAL upstream playerObject(0,[10,20],1)
  in net-restored slot 3 covers rolls/techs/walls/shieldbreak-chain/
  FURASLEEP (live-executes blendColours + rgb() string formatting)/
  DOWNSTAND*/DEADUP/DEADRIGHT/SLEEP/PASS/MISSFOOT/THROWNFALCONDIVE/
  grabbedBy-guards + the extra-arg init variants, with Math.random
  swapped for the sweep generator (rule 12).
- DIVERGENCE LEDGER (fn · root-cause class · fix · min):
  1. sweep synthetic player (capture bring-up) · playerObject's pos
     parameter is an ARRAY (physicsObject reads pos[0]/pos[1]) — the
     {x,y} object gave undefined components and the strict marshaller
     hard-failed exactly as designed (rule 7) · pass [10,20] · 3m.
  Replay divergences after the first successful build: ZERO on all
  three goldens (rules 1-14 held; no expression-shape, desugaring,
  alias, or chain-order misses across 15,397 records).
- comparator negative tests (all restored, tree re-verified 0 div):
  (a) corrupted POST players nibble → exactly 1; (b) WAIT self-restart
  frames−1 → 1; (c) circleDust 4→3 draws → 96 (chain cascade);
  (d) footstep sound typo → 22; (e) double-jump direction −0.3→0.3 →
  0/1/1 on g01/g04/g06 — a CLASSIFICATION flip (JUMPAERIALB vs F) that
  bites only where neutral double-jumps occur (rule-12 corollary, 3rd
  measured instance); (f) GUARD GRAB→APPEAL → 8 seam-args divergences.
- honest coverage (documented, not silent): FURAFURA unswept (init
  stores the Howl play id into furaLoopID — an emulator-environment
  value outside the sim domain; the C body traps at that exact site);
  finishGame (isFinalDeath true) trapped as task 17's lifecycle
  surface; CLIFFJUMP*/CLIFFGETUP*/CLIFFESCAPE*/CLIFFATTACK* and
  THROW*/CATCHATTACK per-char seam arms plus every dispatch arm the
  traces never fire are translated verbatim and verified must-not-fire
  on every live record; the shared JUMPAERIALB/F MODULE objects
  (checkForIASA's import path) are wrapped but zero-live — a live puff
  record through them would fail the seam-name verification loudly.
  Live coverage DID include: 40+ distinct top-level (name,phase) pairs
  incl. REBIRTH (respawn data), CLIFFCATCH/CLIFFWAIT mains (ledge
  deref), DEADDOWN/DEADLEFT inits (screenShake draws), DAMAGEFLYN init
  ×3 (thrown.id0), CAPTUREWAIT/CAPTUREDAMAGE/CATCHWAIT/CAPTURECUT/
  CATCHCUT grab chains, GUARD/GUARDON shield stack, and 101-126
  sound-playing records per golden.
- artifacts (sha256/12): moves.h f4ebac8d3ee1 · moves_index.c
  45c6e2c5b863 · moves/*.c (79 files) 0b05efc96185 ·
  spec-moves-shared.js 581cb1a091a3 · replay_moves_shared.c
  9bdb9597b6db · expected-capture-moves-shared.json 79ff94357c91 ·
  check-moves-shared-replay.sh 7546ddb80765 · ml_events.c 21aacfb127f2
  · ml_events.h 05a23a11d3d9 · physics.h b50c8f08cdaa · FORMAT.md
  3a7e79be60d3. Logs: .loop/task7-donecheck.log, .loop/task7-reg-*.log.
- zoom-out: (1) NEW RULE 15 (fix_plan §M2): per-char table COMPOSITION
  is executed data — measure shared-vs-override by fn identity, dump
  the registry + the post-setup data patches in the frame-0 mvData
  record, and build the C registry FROM the dump; tasks 8-12 inherit
  the instrument (their specs extend the same dump instead of assuming
  file layout). (2) Rule 14 got STRONGER, not just re-pinned: seam
  posts carry the window's rng list and the replay advances the chain
  at the seam's exact structural point — chain-order recoverability is
  a property of resync-seam architecture, not an assumption; the
  measured value stayed zero, but tasks 8-12 get a mechanism instead of
  a pin. (3) Rule 13's family gained a member: control-flow FALLTHROUGH
  is part of the expression shape — seven upstream interrupt arms
  return undefined by falling off the chain, and truthiness-checking
  callers (`if (!interrupt(...))`) keep executing their else-body after
  a dispatch fired; modeling interrupts as bool instead of the
  tri-state would have been a silent class error across every move.
  (4) The scope-condition distinction (inScope vs stack-empty) is the
  chain-order lesson generalized: what makes a nested boundary
  recordable is not stack depth but whether an ATTRIBUTING record's
  draws could interleave — the same criterion will govern tasks 8-12's
  wrappers when per-char moves become the attributing boundary and
  shared moves become their nested C tree.
- next: task 8 (characters/fox moves, 4,450 ln — extend the mvData/
  registry instrument to fox's per-char table; the 210 mdispatch seams
  recorded here become live cross-checks for the per-char clusters).

## iter 27 — 2026-07-14 — M2 task 8: characters/fox moves — MOVES fox MATCH

- phase M2, task 8 (the fox per-char move set: 61 move objects, 192 phase
  fns — the first per-char cluster; the task-7 shared bodies are its
  nested C tree, and the article boundary gets its first real seam).
- done-check: `bash port/sim/calib/check-moves-fox-replay.sh` → exit 0,
  `MOVES fox MATCH` (.loop/task8-donecheck.log). Per golden (g01/g03/
  g08): 2 fresh captures byte-identical (cmp), both runs STREAM MATCH vs
  the frozen streams (3600/3600 exact; finalCheck mvData re-dump
  drift-guarded), pins OK (expected-capture-moves-fox.json), strict
  replay 0 divergences — 1738 + 888 + 2934 = 5560 records: 3758
  mutation-captured fox-origin phase calls (args [phase,name,[slot],
  inputs|null,pre] — THROWN*{BACK,DOWN} inits arrive 1-arg upstream;
  post {alias,hq,players,rng,snd,vfx}; 168 frame-0 rule-11/12 sweep
  records per golden on a separate sweep mulberry32), 75 article seams
  (LASER ×27 live on g01 AND g08 + 7 sweep spawns each; g03 sweep-only),
  0 live mdispatch seams (measured: fox never threw a non-fox over these
  traces), 1721 standalone seeded draws (1488 on g08 — the CPU golden's
  AI plane, chain-verified draw-for-draw), 3 mvData + 3 rngBoot records.
- GOLDENS (PROVISIONAL, auto-adopted): g01/g03/g08 — the fox carriers.
  Deviation from the §M2 g01/g04/g06 convention documented: the
  convention's PURPOSE is live coverage and g04/g06 field no fox slot.
  g08 is the FIRST CPU-golden capture: run-capture.js takes the manifest
  cpu params unchanged, AI stays JS-side per the M2 AI policy, and the
  verify-stream guard + the standalone-draw chain hold exactly as on
  human traces (rngCalls 1496 reproduced).
- conformance guard + regressions ALL GREEN (.loop/task8-reg-*.log):
  ENVCOLL MATCH · UTIL MATCH · PLAYER MODEL MATCH · INPUT MATCH ·
  ASSHORT MATCH · PHYSICS MATCH · HITDET MATCH · MOVES SHARED MATCH —
  every prior spec re-verified against the edited shared TUs
  (ml_player.h/player_canon.c/input_canon.c/shared moves_index.c).
- design: boundary = every FOX-ORIGIN actionStates entry fn (identity vs
  the characters/fox/moves module index — rule 15's instrument extended
  per its recipe; fox-origin measured ONLY on table 2: 61 states/192 fns)
  + the 2 article inits (194 wrapped). SHARED entries stay UNWRAPPED —
  the task-7 scope lesson applied in reverse: at top level they are
  chain-safe silent surface (their draws are standalone records), inside
  a fox record they are the TRANSPARENT nested C tree (shared bodies
  linked in; fox JAB1.interrupt → WAIT.init is a direct MODULE import
  upstream and a direct mv_WAIT.init call in C). Upstream's TWO dispatch
  graphs are mirrored distinctly: direct module imports (fox→shared,
  fox→fox, MOVES[checkFor-payload] via fox_move_def) vs actionStates
  TABLE dispatch (only the THROW* victim arms — mv_dispatch: fox victim
  = registered body, non-fox victim = mdispatch seam; THROWBACK/
  THROWDOWN dispatch 1-arg). mv_checkForIASA (shared moves_index.c) is
  checkForIASA with REAL dispatch — shared JUMPAERIALB/F MODULE objects
  (puff's table overrides deliberately bypassed, the import-path
  semantics) + a registered per-char module index (mv_register_char_
  module; tasks 9-12 register theirs); the task-4 note-based
  as_checkForIASA stays the asshort boundary unchanged. hitQueue.push
  from fox THROW* = mv_hq_push6 appending the row's canon onto the
  opaque hq carrier. Article seam: inits only READ player state, mutate
  only JS-side article queues, draw no RNG → 4-field FIFO records, args
  verified bit-exactly, no resync — the documented task-13 boundary.
  Data planes: charHitboxes assigns via the GENERALIZED
  mv_assign_hitbox_id (CTAB1, 5th consumer); fox move-object data arrays
  (ATTACKDASH/APPEAL/FIREFOXBOUNCE/THROWFORWARD setVelocities*, 20
  THROWN*.offset incl. authored expressions like -7.74-0.08, CLIFF*
  offset/setVelocities) from the mvData fox dump (rule 15 — never
  retyped), served by mv_fox_arr/mv_fox_pair/mv_fox_arr_len.
- DIVERGENCE LEDGER (root-cause class · fix · min):
  1. hitbox spec offsetSingle (5 sweep divergences, THROW* records) ·
     charHitboxes entries are ALWAYS 12-key createHitbox objects but the
     throw hitboxes pass a SINGLE Vec2D offset (attributes.js:749) — a
     third sub-shape the task-2 survey never saw; the CONSTRUCTOR
     fallback in the assign helper (inherited from task 7's
     mv_assign_thrown_id0) mis-shaped them and was unreached-wrong for
     every prior capture · ml_player.h offsetSingle + player_canon
     marshal/ser + mv_assign_hitbox_id always-CHARDATA (class fix) · 25m.
  2. AI number-valued input buttons (g08 marshal hard-fail, exactly as
     rule 7 designs) · ai.js writes NUMBERS into aiInputBank button
     fields (`.l = 0`/`= 1.0`) — the task-3 "buttons are real JS
     booleans" pin was a HUMAN-golden fact · verified buttons are
     truthiness-only upstream (no raw propagation into player state in
     characters/ or actionStateShortcuts.js), then widened
     ml_input_from_canon: CV_NUM buttons marshal by JS truthiness
     (NEW RULE 16) · 20m.
  Replay divergences after those two class fixes: ZERO on all three
  goldens (rules 1-15 held; no expression-shape, desugaring, alias,
  chain-order, or data-plane misses across 5560 records).
- comparator negative tests (all restored, tree re-verified 0 div):
  (a) corrupted POST players nibble → exactly 1; (b) JAB1 WAIT threshold
  17→16 → 3/11/0 on g01/g03/g08 (bites where the value occurs — rule-12
  corollary); (c) laser article y 7→8 → 27/27 on g01/g08 (article-args);
  (d) THROWDOWN hq-push flag true→false → exactly 1 (the hq append
  model's teeth); (e) THROWUP dispatched through marth's table →
  seam-underflow divergence (mdispatch FIFO teeth despite zero live
  seams); (f) circleDust 4→3 draws → 107/81 (sweep-chain cascade);
  (g) ATTACKDASH setVelocities index off-by-one → 3 (rule-11 sweep teeth
  on the rule-15 data plane).
- honest coverage (documented, not silent): SIDESPECIALAIR.init's
  grounded arm is UNSWEEPABLE — upstream itself stack-overflows (main's
  grounded arm re-enters main with grounded still true; found when the
  sweep crashed the page, removed with a spec comment); the CLIFF*
  onLedge===-1 arm WRITES the move table (`this.canGrabLedge = false`) —
  C traps at the exact site and the finalCheck-guarded mvData dump would
  catch the drift if it ever fired live; THROWNFALCO*/FALCON* offset
  overruns past the array end trap (upstream throws — no clamp in that
  family); interrupt-tail WALK arms shadowed by checkForTilts stay
  unswept; live mdispatch (non-fox THROW victims) zero — seam-guarded,
  loud on first live record. Live coverage DID include: JAB1/FORWARDTILT/
  DOWNTILT/GRAB/CATCHATTACK/DOWNATTACK chains, ATTACKAIRN/F/B/D incl.
  land arms, NEUTRALSPECIALGROUND with 54 live LASER spawns, and fox as
  THROWN victim (THROWNPUFFBACK live on g08).
- artifacts (sha256/12): fox moves.h 58a122c4057c · fox moves_index.c
  8c6c637a81aa · fox moves/*.c (61 files, 4983 lines) 19f5c8bc2f99 ·
  spec-moves-fox.js fd0b7a961b1f · replay_moves_fox.c b1e2e122e3e3 ·
  expected-capture-moves-fox.json 2e4b3e497e13 ·
  check-moves-fox-replay.sh 04194bb92770 · ml_player.h b77e472f8539 ·
  player_canon.c 37dba6a20e92 · input_canon.c 2b18537ca82c · shared
  moves.h 11150dcc5871 · shared moves_index.c 5d7270a386d2 · FORMAT.md
  c8f5dbd20967. Logs: .loop/task8-donecheck.log, .loop/task8-reg-*.log.
- zoom-out: (1) NEW RULE 16 (fix_plan §M2): value models measured on
  human goldens do NOT transfer to CPU goldens — the AI plane authors its
  own value shapes (number buttons); re-survey before reuse, widen only
  after verifying the consumption mode. Task 16's AI-input bridge
  inherits this directly. (2) The offsetSingle find is rule 7 WORKING as
  the instrument: both divergence classes this iteration were caught by
  strict marshallers hard-failing outside the measured domain, not by
  chasing wrong outputs — capture-FIRST plus hard-fail marshalling
  remains the class-level defense; notably the wrong CONSTRUCTOR
  fallback had sat unreached in verified code since task 7 — "verified"
  means verified OVER THE CAPTURED DOMAIN, nothing more. (3) The
  per-char cluster recipe is now mechanical for tasks 9-12: wrap
  char-origin by module-index identity, leave shared unwrapped, seam the
  other chars, extend the mvData dump with {origin, data:array-props},
  register the module index for checkForIASA, reuse the sweep
  scaffolding (slot-3 + cs[3] injection, self-grab throws, default-stage
  cliffs, article/hq splice-restore). Falco (task 9) adds its laser/
  DOWNSPECIAL articles to the same article-seam pattern. (4) Upstream's
  import-graph vs table-dispatch distinction is now an explicit modeling
  axis (module calls ≠ actionStates dispatch — puff's JUMPAERIALB/F
  overrides make the difference observable); rule 15's "composition is
  executed data" extends to the DISPATCH GRAPH, not just the data plane.
- next: task 9 (characters/falco moves, 4,488 ln — extend the mvData/
  registry instrument to falco; falco carries g02/g05/g07 (g07 = the
  second CPU golden); falco's laser/THROWFORWARD article sites reuse the
  article-seam pattern with isFox:false options).

## iter 28 — 2026-07-14 — M2 task 9: characters/falco moves — MOVES falco MATCH

- phase M2, task 9 (the falco per-char move set: 69 move objects, 214
  phase fns — the second per-char cluster, executed with task 8's
  now-mechanical recipe; first cluster to run on the g02/g05/g07 falco
  carriers).
- done-check: `bash port/sim/calib/check-moves-falco-replay.sh` → exit 0,
  `MOVES falco MATCH` (.loop/task9-donecheck.log). Per golden
  (g02/g05/g07): two fresh captures byte-identical, STREAM MATCH ×2
  against the frozen streams (non-perturbation), pins green, C replay
  0 divergences over 1779 + 1240 + 1619 = 4638 records (4218 move
  records incl. 217 sweep calls per golden, 83 article seams, 0 live
  mdispatch, 331 standalone draws, 3 mvData + 3 rngBoot records).
- GOLDENS: g02 falco/puff/ystory · g05 marth/falco/fdest · g07
  falco/CPU-falcon/battlefield — the falco carriers per the task-8
  carrier convention; g07 is the SECOND CPU-golden capture (AI JS-side,
  81 rngCalls reproduced, seeded draws chain-verified draw-for-draw).
- conformance guard + regressions ALL GREEN (.loop/task9-reg-*.log):
  ENVCOLL MATCH · UTIL MATCH · PLAYER MODEL MATCH · INPUT MATCH ·
  ASSHORT MATCH · PHYSICS MATCH · HITDET MATCH · MOVES SHARED MATCH ·
  MOVES fox MATCH — every prior spec re-verified against the edited
  shared TUs (ml_player.h/player_canon.c gained the marth shieldBreaker
  fields).
- design: task 8's recipe followed EXACTLY (wrap falco-origin by
  module-index identity on table 3; shared entries unwrapped; non-falco
  per-char entries mdispatch seams; article inits as 4-field FIFO seams;
  mvData dump extended with falco {origin, data}; per-char data arrays
  served by mv_falco_arr/pair/len — rule 15; sweep scaffolding reused
  with falco-measured timer arms). Falco's article options carry
  isFox:false on every LASER/ILLUSION init, THROWDOWN's four lasers add
  partOfThrow:true, ILLUSION is always type 0 — the C seams serialize
  the extra keys in canon-sorted position. checkForIASA has NO
  characterSelections==3 branch upstream: a falco IASA aerial payload
  dispatches NOTHING (returns true bare) — no char-module registration
  in the falco replay, verbatim fidelity.
- falco STRUCTURE deltas vs fox (measured by per-file diff BEFORE
  translating — the recipe's cheapest instrument; all carried verbatim):
  THROW* inits have NO grabbing===-1 guard (the guard lives only in the
  interrupts as a bare-return arm — swept, ret undef); THROWN* have NO
  grabbedBy===-1 guards and NO offset-length clamps (player[-1]/offset
  overruns throw upstream — mv_player/mv_falco_pair trap; the no-snap
  THROWNPUFF{UP,FORWARD,DOWN} family tolerates grabbedBy=-1 until
  timer>0 — swept); CLIFF* have NO onLedge===-1 canGrabLedge table-write
  arm (fox's quirk; ledge[-1] throws upstream — mv_ledge_point traps);
  the shine is a 4-sub-state machine per environment
  (DOWNSPECIAL{AIR,GROUND}{START,LOOP,END,TURN} + init-only delegate
  entries) whose land/platform-drop arms write actionState directly;
  THROWFORWARD's setVelocities index is Math.max(0,·)-clamped; APPEAL
  carries no setVelocities plane; falco throws FIRE LASERS (the THROW*
  mains are laser-crossing chains).
- DIVERGENCE LEDGER (root-cause class · fix · min):
  1. marth phys.shieldBreakerCharge/ChargeAttempt/Charging (g05 marshal
     hard-fail, rule 7 by design) · runtime-added by marth
     NEUTRALSPECIAL* — a move family NO prior golden ever fired; the
     task-2 player model was verified over g01/g04/g06 (+g03/g08) only ·
     presence-modeled trio in ml_player.h + player_canon marshal/ser ·
     15m.
  2. player.shieldBreakerID (same records) · marth's Howl play id,
     furaLoopID's runtime-added cousin · presence-modeled number ·
     5m.
  Replay divergences after those two model widenings: ZERO on all three
  goldens on the first C build (rules 1-16 held; no expression-shape,
  desugaring, alias, chain-order, or data-plane misses across 4638
  records — the recipe is mechanical as advertised).
- comparator negative tests (all restored, tree re-verified 0 div):
  (a) corrupted POST players nibble → exactly 1; (b) FORWARDTILT actives
  [T,T,T,F]→[T,T,F,F] (the falco-vs-fox delta itself) → 11/12/9 live
  divergences (bites occurring values — rule 12); (c) NSG laser article
  y 7→8 → 31/13/29 (article-args; live lasers on g02/g07, sweep-only on
  g05); (d) THROWDOWN hq-push flag true→false → exactly 1 (hq append
  model teeth); (e) THROWUP dispatched through marth's table →
  seam-underflow divergence (mdispatch FIFO teeth despite zero live
  seams); (f) circleDust 4→3 draws → 77/62/42 (sweep-chain cascade);
  (g) THROWFORWARD setVelocities index off-by-one → 1 (rule-15 data
  plane); (h) shine AIRTURN threshold 3→4 → 2 each (sub-state machine
  teeth).
- honest coverage (documented, not silent): THROW*.init without a grab
  and snap-family THROWN*.init with grabbedBy=-1 are unsweepable
  (upstream itself throws on actionStates[undefined]/player[-1]);
  THROWN* offset overruns trap (no clamps in falco's family); live
  mdispatch (non-falco THROW victims) zero over g02/g05/g07 —
  seam-guarded, loud on first live record. Live coverage DID include:
  NSG laser loops (621 main calls on g02/g07 with 19 live LASER spawns
  each), JAB1/FORWARDTILT/DOWNTILT/GRAB chains, ATTACKAIRB/F/N incl.
  land arms, JAB2 (g07), and falco as marth's grab victim on g05.
- artifacts (sha256/12): falco moves.h b53d576076ad · falco
  moves_index.c c907f45bf12c · falco moves/*.c (69 files, 5168 lines)
  6f9462403421 · spec-moves-falco.js df86b801cbb0 ·
  replay_moves_falco.c ea9a11bf1106 · expected-capture-moves-falco.json
  4c8545cca73b · check-moves-falco-replay.sh 2d1373d338a7 · ml_player.h
  1f7ee1a501d8 · player_canon.c fb2bf8436208 · FORMAT.md ead02ca29e6b.
  Logs: .loop/task9-donecheck.log, .loop/task9-cap-*.log,
  .loop/task9-reg-*.log.
- zoom-out: (1) rule 16 EXTENDED (fix_plan §M2): the CPU-golden lesson
  generalizes — ANY new golden widens the player-plane value domain for
  ALL FOUR slots, not just the newly-captured char's (g05's marth had
  never charged a shield breaker in any prior capture; the falco capture
  inherited the miss because envelopes carry every player). "Verified"
  still means verified OVER THE CAPTURED DOMAIN; the class-level defense
  stays capture-FIRST + rule-7 hard-fail marshalling, which caught both
  instances in minutes. Budget one marshal-widening round whenever a
  spec adopts a golden no prior spec captured. (2) The per-char recipe
  held at zero translation divergences — the structure-delta diff pass
  (diff fox/moves vs falco/moves BEFORE translating) is the step that
  made it mechanical: every falco quirk (guard removals, shine machine,
  laser throws, isFox options) was known before a line of C was written.
  Tasks 10-12 (falcon/marth/puff) should keep that pass; marth/puff have
  no fox sibling, so diff against the closest translated char and expect
  more fresh files. (3) No new rule minted beyond the rule-16 extension —
  zero one-offs registered; both fixes were class-level model widenings.
- next: task 10 (characters/falcon moves, 4,432 ln — falcon carriers are
  g03 falcon/fox/pstadium, g04 puff/falcon/dreamland, g06
  falcon/marth/fountain; g07's CPU falcon is ALSO a falcon carrier —
  pick per the live-coverage convention; extend the mvData/registry
  instrument to falcon; falcon has no articles, so the article seam
  list should pin to zero unless raptorBoost spawns one — measure).

## iter 29 — 2026-07-14 — M2 task 10: characters/falcon moves — MOVES falcon MATCH

- phase M2, task 10 (the falcon per-char move set: 67 move objects, 217
  falcon-origin fns — the third per-char cluster, executed with the
  task-8/9 mechanical recipe; first cluster with NON-phase move fns and
  the first with a zero-article pin).
- done-check: `bash port/sim/calib/check-moves-falcon-replay.sh` → exit
  0, `MOVES falcon MATCH` (.loop/task10-donecheck.log). Per golden
  (g03/g04/g06): two fresh captures byte-identical (cmp), STREAM MATCH
  ×2 against the frozen streams (non-perturbation; finalCheck mvData
  re-dump drift-guarded), pins green
  (expected-capture-moves-falcon.json), strict C replay 0 divergences —
  1847 + 1316 + 1810 = 4973 records: 4633 mutation-captured falcon-origin
  fn calls (294 frame-0 rule-11/12 sweep records per golden on a separate
  sweep mulberry32), 0 live mdispatch seams, 0 article records, 334
  standalone seeded draws, 3 mvData + 3 rngBoot records.
- GOLDENS: g03 falcon/fox/pstadium · g04 puff/falcon/dreamland · g06
  falcon/marth/fountain — back on the g01/g04/g06-era convention with
  g03 replacing g01 (g01 fields no falcon). CARRIER MEASUREMENT: the
  task brief flagged g07's CPU falcon as a candidate — measured: it
  fires ZERO live falcon-origin moves over its 3600 frames (the d5 AI
  never attacked with a falcon-origin state; every falcon record a g07
  capture would add is sweep-only). Carrier membership is measured LIVE
  COVERAGE, never char presence. No new golden adopted → rule 16's
  re-survey budget was already paid by tasks 2-9: ZERO value-model
  changes this iteration (falcon's raptorBoost/landingMultiplier/
  upbAngleMultiplier are constructor fields modeled since task 2).
- conformance guard + regressions ALL GREEN (.loop/task10-reg-*.log):
  ENVCOLL MATCH · UTIL MATCH · PLAYER MODEL MATCH · INPUT MATCH ·
  ASSHORT MATCH · PHYSICS MATCH · HITDET MATCH · MOVES SHARED MATCH ·
  MOVES fox MATCH · MOVES falco MATCH — every prior spec re-verified
  against the edited shared TUs (shared moves.h/moves_index.c gained the
  special-phase hook).
- design: task 8's recipe followed (wrap falcon-origin by module-index
  identity on table 4; shared entries unwrapped; non-falcon per-char
  entries mdispatch seams; mvData extended with falcon {origin, data} —
  85 arrays over 50 states incl. the UPSPECIAL/UPSPECIALTHROW/
  DOWNSPECIALAIR PAIR setVelocities; data served by mv_falcon_arr/pair/
  len, rule 15; sweep scaffolding reused with falcon-measured arms). NEW
  DISPATCH SURFACE CLASS: falcon move objects carry NON-phase fns —
  onPlayerHit(p) on SIDESPECIAL{GROUND,AIR} (hitDetection.js:493's
  specialOnHit arm; 1-arg, inputs null) and onWallCollide(p,input,
  wallFace,wallNum) on DOWNSPECIALGROUND (physics.js:122's
  specialWallCollide arm; args canon [slot,wallFace,wallNum], extras
  marshalled DX_STR+DX_NUM). Modeled WITHOUT touching MlMoveDef:
  `mv_register_special_phases` (shared moves.h) is a driver-registered
  (state,phase)→MvFn lookup mv_dispatch routes the two phase names
  through — the alternative (two new MlMoveDef fields) tripped
  -Wmissing-field-initializers across all 209 existing positional
  initializers; the hook keeps every prior file byte-identical and
  unregistered lookups reproduce the upstream missing-property
  TypeError (trap). Tasks 11-12 reuse it (puff's NEUTRALSPECIAL* family
  carries the same two fns). ARTICLES: falcon imports `articles` in 6
  files and never dereferences it — dead imports, zero call sites,
  measured; the spec still wraps LASER/ILLUSION as a measurement
  instrument, the pins freeze article=0, and the replay FIFOs any
  article record into an unconsumed-seam failure (tripwire for the
  measured claim).
- falcon STRUCTURE deltas (measured by per-file diff BEFORE translating;
  all carried verbatim): THROWN* family is byte-identical to FOX's
  shapes — guarded THROWN{PUFF,MARTH,FOX}* + unguarded
  THROWN{FALCO,FALCON}* — so those 20 C files (+ byte-identical
  GRAB/CATCHATTACK) are the task-8 fox translations with renames, diffs
  data-only (offsets from the mvData dump); THROW* keep fox's
  grabbing===-1 init guard but fire NO lasers (falcon throws are plain
  hq-push crossings); CLIFF* keep fox's onLedge===-1 canGrabLedge
  table-write arm (C traps; falcon CLIFF pos arms drop fox's inner
  timer>=14 gate and CLIFFATTACK* play falcondoublejump in INIT);
  SIDESPECIALGROUND writes `this.canEdgeCancel` at RUNTIME — a SCALAR
  move-table write INVISIBLE to the array-only mvData dump (a new
  sub-shape of the fox canGrabLedge class): modeled as C module state
  (mv_falcon_ssg_set_canEdgeCancel; write-only in this cluster — its
  only sim reader is physics' per-state flag lookup, wired in task 17;
  the sweep restores the table value, rule 12); UPSPECIALCATCH/
  UPSPECIALTHROW draw the seeded stream INLINE in move code (2 draws
  per firefoxtail spawn ×3 — values render-only, DRAWS are chain
  state) and UPSPECIALCATCH's interrupt pushes
  [grabbing,p,0,false,true,false]; DEAD-ARM QUIRKS from upstream typos:
  SIDESPECIALGROUNDHIT.main reads player[p].phys.timer (physicsObject
  has no timer → undefined<18 false, the 0.30313 arm never fires) and
  DOWNSPECIALGROUNDENDAIR.main reads player.timer (the ARRAY's —
  undefined, both arms dead) — carried as commented dead arms (rule 13
  family: the SHAPE includes the dead test), never "fixed"; the
  firefoxtail vfx windows read id[0].offset[frame] for render-only
  positions — mv_falcon_hb0_off performs the read for crash-fidelity
  and discards the value.
- DIVERGENCE LEDGER: EMPTY — 0 replay divergences on the first
  successful build across all 4973 records, zero value-model changes,
  zero class fixes (rules 1-16 held; the structure-delta diff pass +
  capture-first measurement caught every quirk before a line of C).
- comparator negative tests (all restored, tree re-verified 0 div):
  (a) corrupted POST players nibble → exactly 1; (b) FORWARDTILT active
  9→8 → 9/17/9 live divergences (bites occurring values — rule 12);
  (c) THROWUP hq-push flag → exactly 1; (d) THROWUP dispatched through
  marth's table → seam-underflow (mdispatch FIFO teeth despite zero
  live seams); (e) circleDust 4→3 draws → 49/103/97 (sweep-chain
  cascade); (f) onWallCollide wall-face condition flip → 3 each (all
  three sweep arms bite); (g) NEUTRALSPECIALGROUND setVelocities index
  off-by-one → 324/2/324 (rule-15 data plane; bites the LIVE falcon
  punches on g03/g06); (h) special-phase registration removed → OUT OF
  DOMAIN abort at the first onPlayerHit record (the upstream
  missing-property TypeError); (i) UPSPECIALCATCH inline draws 2→1 →
  8 each (inline-draw chain teeth).
- honest coverage (documented, not silent): live falcon THROW*/THROWN*,
  raptor-boost hits (onPlayerHit) and wall collides (onWallCollide) are
  ZERO over g03/g04/g06 — sweep-covered (294 calls incl. both
  onPlayerHit sites, all three onWallCollide arms, the full
  FALCONDIVE catch/throw chains and all 20 THROWN* inits), seams loud
  on first live record; THROWN{FALCO,FALCON}* grabbedBy=-1/overrun arms
  unswept (upstream itself throws); the CLIFF* canGrabLedge write arm
  traps (mvData drift-guarded). Live coverage DID include: 693-call
  NEUTRALSPECIALGROUND falcon-punch arcs on g03 AND g06 (the rule-15
  data plane live), JAB1/2/3 chains, FORWARDTILT/DOWNTILT, GRAB/
  CATCHATTACK, ATTACKAIRN/F/B/D incl. land arms, and CLIFFATTACKQUICK
  on g06.
- artifacts (sha256/12): falcon moves.h 1de40720d279 · falcon
  moves_index.c 6facb4fce9e6 · falcon moves/*.c (67 files, 5379 lines)
  a9b9ccc4c55b · spec-moves-falcon.js 768ae707a7a4 ·
  replay_moves_falcon.c 14f365307a28 ·
  expected-capture-moves-falcon.json bc0289c91105 ·
  check-moves-falcon-replay.sh 994f56ece4e7 · shared moves.h
  8946cfa096a2 · shared moves_index.c fd7ad9801db2 · FORMAT.md
  fa6eac43763f. Logs: .loop/task10-donecheck.log,
  .loop/task10-probe-*.log, .loop/task10-reg-*.log.
- zoom-out: (1) NO new rule minted and an EMPTY divergence ledger — the
  first zero-fix cluster. The class-level machinery did the work up
  front: the structure-delta diff pass surfaced every falcon quirk
  (dead-arm typos, inline draws, the scalar table write, the THROWN
  family split) before translation, and rule 15/16 instruments needed
  no widening because the carriers were already-captured goldens. The
  recipe's cost curve is bending: task 8 took two class fixes, task 9
  two model widenings, task 10 zero. (2) The special-phase hook is a
  CLASS decision, not a one-off: puff (task 12) carries the same
  onPlayerHit/onWallCollide pair on its NEUTRALSPECIAL* family, so the
  dispatch surface got a reusable registry rather than a falcon-shaped
  patch; rejecting the MlMoveDef-field alternative also kept 209
  translated files untouched (instrument > class fix > one-off,
  applied to API shape). (3) Carrier selection is now explicitly a
  MEASUREMENT, not a roster read: g07 carries a falcon but zero live
  falcon moves — the convention's purpose (live coverage) survives only
  if candidacy is checked against captures, one probe run per
  candidate. (4) The article-zero pin generalizes the honest-coverage
  discipline to ABSENCE claims: "falcon has no articles" is not a
  comment but a wrapped boundary + a zero pin + an unconsumed-FIFO
  tripwire — absence measured the same way presence is.
- next: task 11 (characters/marth moves, 5,659 ln — marth carriers:
  g01 fox/MARTH/battlefield, g05 MARTH/falco/fdest, g06
  falcon/MARTH/fountain (g04 fields no marth; probe-measure per the
  carrier convention); marth has no fox/falco sibling — diff against
  the closest translated char and expect more fresh files; the
  shieldBreaker fields are already presence-modeled from task 9;
  extend the mvData/registry instrument to marth; marth reportedly has
  a checkForIASA char-0 MODULE branch — register the marth module index
  via mv_register_char_module).

## iter 30 — 2026-07-14 — M2 task 11: characters/marth moves — MOVES marth MATCH

- phase M2, task 11 (the marth per-char move set: 75 move objects, 244
  marth-origin fns — the fourth per-char cluster; first with the onClank
  special phase, the sbid Howl-id oracle seam, and a char-data-plane
  VALUE write).
- done-check: `bash port/sim/calib/check-moves-marth-replay.sh` → exit 0,
  `MOVES marth MATCH` (.loop/task11-donecheck.log). Per golden
  (g01/g05/g06): two fresh captures byte-identical (cmp), STREAM MATCH
  ×2 against the frozen streams (non-perturbation; finalCheck mvData
  re-dump drift-guarded), pins green
  (expected-capture-moves-marth.json), strict C replay 0 divergences —
  1608 + 1986 + 1586 = 5180 records: 4735 mutation-captured marth-origin
  fn calls (394 frame-0 rule-11/12 sweep records per golden on a
  separate sweep mulberry32), 0 live mdispatch seams, 0 article records
  — marth has ZERO `articles` imports anywhere under characters/marth/
  (stronger than falcon's dead imports; pinned ZERO with the replay's
  unconsumed-FIFO tripwire), 439 standalone seeded draws, 3 mvData +
  3 rngBoot records.
- GOLDENS: g01 fox/MARTH/battlefield · g05 MARTH/falco/fdest · g06
  falcon/MARTH/fountain — the marth carriers, PROBE-MEASURED per the
  task-10 carrier convention (live marth-origin move records:
  1082/1417/1054; g04 fields no marth). g05 is the first golden with a
  LIVE special-family arc across all captures: 531 live
  NEUTRALSPECIALGROUND mains + 12 live Howl play-ids.
- conformance guard + regressions ALL GREEN (.loop/task11-reg-*.log):
  ENVCOLL MATCH · UTIL MATCH · PLAYER MODEL MATCH · INPUT MATCH ·
  ASSHORT MATCH · PHYSICS MATCH · HITDET MATCH · MOVES SHARED MATCH ·
  MOVES fox MATCH · MOVES falco MATCH · MOVES falcon MATCH — every
  prior spec re-verified against the edited shared TUs (ml_player.h/
  player_canon.c gained the dancingBlade pair; shared moves_index.c's
  mv_dispatch special-phase arm gained "onClank").
- design: task 8's recipe followed (wrap marth-origin by module-index
  identity on table 0 — 242 phase fns + 2 onClank; shared entries
  unwrapped; non-marth per-char entries mdispatch seams; mvData extended
  with marth {origin, data} — setVelocities incl. UPSPECIAL's PAIR
  array, CLIFF*/THROWN* offsets, canGrabLedge pairs — served by
  mv_marth_arr/pair/len, rule 15; sweep scaffolding reused with
  marth-measured arms). NEW SEAM CLASS (rule 14's value-plane cousin):
  `player[p].shieldBreakerID = sounds.shieldbreakercharge.play()`
  consumes a GLOBAL howler counter id (advanced by every sound in the
  match, mostly outside this cluster's records — unrecoverable chain
  state); the capture records each consumed id in the record's post
  "sbid" list (the envelope's 7th key), the replay injects it via
  mv_howl_play_id and re-emits the CONSUMED list — count/order teeth in
  the post compare, value teeth in player.shieldBreakerID. NOT
  checksummed (no Howl id reaches CHECKSUM.md) — ×2 byte-stability +
  STREAM MATCH stay the determinism proof. `.stop(id)` = the
  "shieldbreakercharge.stop" token (furaloop.stop's cousin). NEW
  char-data-plane WRITE instance (falcon canEdgeCancel's class, VALUE
  sub-shape): NEUTRALSPECIAL*'s timer-46 `hitboxes.id[i].dmg = newDmg`
  mutates the GLOBAL charHitboxes objects (player.js:132 aliases chars
  data — NO per-player copy). newDmg == the authored 7 while
  shieldBreakerCharge < 30 (measured live domain {0,1} on g05); a live
  CHARGED punch followed by a later nsg/nsa hitbox assign diverges LOUD
  at that init record (C reads pristine CTAB1) — documented hole; the
  sweep's >=120 arm mutates and RESTORES all 8 dmg fields (rule 12
  net-restore; C needs NO module state because every record marshals
  its pre and the only cross-record C read is the CTAB1 init assign).
  onClank(p,input) on DOWNSPECIAL{GROUND,AIR} (hitDetection.js:71-72's
  specialClank arm) reuses the task-10 special-phase hook —
  mv_dispatch's routed-name list gained "onClank",
  mv_register_special_phases(marth_special_phase). checkForIASA's
  char-0 MARTHMOVES arm (actionStateShortcuts.js:400-401) is now REAL
  dispatch via mv_register_char_module(0, marth_move_def) — and
  MEASURED dead-by-construction upstream: only fox/falco/falcon aerials
  call checkForIASA (each only when cs[p] is that char), while marth's
  ATTACKAIRF/B INLINE the aerial-IASA logic (checkForDoubleJump →
  shared JUMPAERIALB/F modules; checkForAerials payload →
  marth[a[1]].init — swept).
- marth STRUCTURE deltas (measured by per-file diff BEFORE translating;
  all carried verbatim): THROW* dispatch victims 2-ARG
  (.init(grabbing, input); fox's THROWBACK/THROWDOWN are 1-arg) and
  randomShout in INIT after the -1 guard; THROWUP's interrupt -1 arm
  returns false where the other three fall through undefined (rule 13
  family); THROWN{PUFF,MARTH,FOX}* are GUARDED with per-file
  clamp-vs-guard ORDER variation (THROWNMARTHUP/MARTHFORWARD/FOXFORWARD
  clamp-first, the rest guard-first; THROWNMARTHBACK alone has NO init
  -1 guard; THROWNPUFFUP wraps its body in a vacuous
  `if(player[p].phys)`); THROWN{FALCO,FALCON}* are fox's unguarded
  family with ONE body delta — THROWNFALCOBACK ADDS `face *= -1` in
  init; all 8 CLIFF* keep fox's onLedge===-1 canGrabLedge table-write
  trap arm and CLIFFGETUPQUICK sets ledgeRegrabCount = TRUE (authored
  quirk, others false); smashes charge via the timer==3/7/3 hold
  machine (DOWNSMASH on loose ==); the dancing blade's
  phys.dancingBlade/dancingBladeDisable are runtime-added — rule 16
  widening, presence-modeled in ml_player.h (no prior golden fired
  marth SIDESPECIAL*); SIDESPECIAL{GROUND,AIR}4DOWN compute their
  charHitboxes key at runtime ("db{ground,air}4down" +
  floor((timer-7)/6)) on a timer%6 switch with the shout6 cutoff at 37;
  UPSPECIAL rotates its PAIR setVelocities by phys.upbAngleMultiplier
  via fdlibm sin/cos and lands on falcon's 3-disjunct guard shape;
  hitboxes.id[i].dmg writes mirror through the rule-10 id alias
  (mv_hb_set_dmg).
- DIVERGENCE LEDGER (root-cause class · fix · min):
  1. THROWNFALCOBACK face flip (1 sweep divergence on the g01 probe) ·
     the task-10 "fox translations renamed" shortcut trusted the
     delta-2 LINE COUNT as "imports only" — the two lines were the
     offset DATA and an ADDED `face *= -1` · re-diffed all 8 fox-family
     bodies properly (only THROWNFALCOBACK differs), added the flip ·
     10m. Lesson pinned in CLAUDE.md §Commands: read the diff BODY,
     never trust the line count.
  Replay divergences after that one fix: ZERO on all three goldens
  (rules 1-16 held; the sbid seam and the dmg-plane design were built
  capture-first, before any C ran).
- comparator negative tests (all restored, tree re-verified 0 div):
  (a) corrupted POST players nibble → exactly 1; (b) FORWARDTILT active
  7→6 → 11/15/11 live divergences (bites occurring values — rule 12);
  (c) THROWUP hq-push flag → exactly 1; (d) THROWUP dispatched through
  an unregistered state → seam-underflow (mdispatch FIFO teeth despite
  zero live seams); (e) sbid injection dropped (id → 0) → 2/14/2 —
  bites g05's 12 LIVE shieldBreakerID records; (f) charge-tint overlay
  string format → 3/6/3 (live blend frames on g05); (g)
  dancingBladeCombo window 26→9 → exactly 1 each (the sweep enable
  arm); (h) onClank registration removed → OUT OF DOMAIN at the first
  onClank record (the upstream missing-property TypeError); (i)
  UPSPECIAL setVel index off-by-one → post divergence + the
  out-of-range trap (rule-15 data plane).
- honest coverage (documented, not silent): live marth THROW*/THROWN*/
  CLIFF*/dancing-blade/counter/UPSPECIAL/APPEAL are ZERO over
  g01/g05/g06 — sweep-covered (394 calls incl. every chain dispatch,
  the full charge machine, all 20 THROWN* inits + guard/clamp arms, all
  8 CLIFF*), seams/traps loud on first live record; a live CHARGED
  shield-breaker punch (charge >= 30) is the documented
  char-data-plane hole (loud at the next hitbox assign); guarded-THROWN
  offset overruns and THROWNMARTHBACK's grabbedBy=-1 are unswept
  (upstream itself throws); the CLIFF* canGrabLedge write arm traps
  (mvData drift-guarded). Live coverage DID include: the g05
  NEUTRALSPECIALGROUND arcs (charge window, blend overlay, play-id and
  release-stop arms live), JAB1/JAB2 combo chains, FORWARDTILT, GRAB,
  DOWNATTACK (g06), ATTACKAIRN/F/B incl. land arms, and marth as grab
  victim via the nested THROWNMARTH* tree.
- artifacts (sha256/12): marth moves.h 20e1d5a6d404 · marth
  moves_index.c 333d1ad115e3 · marth moves/*.c (75 files, 6351 lines)
  65bb561e9de0 · dancing_blade_combo.c 6483d2c355bb ·
  dancing_blade_air_mobility.c a2544c55d0c8 · spec-moves-marth.js
  d90e1fc046c3 · replay_moves_marth.c ba33fe646463 ·
  expected-capture-moves-marth.json 5d65b6f72254 ·
  check-moves-marth-replay.sh 4117e35772ea · ml_player.h f052853974ff ·
  player_canon.c bf4e6e2baca0 · shared moves.h 960b2f742d09 · shared
  moves_index.c 126038513bc1 · FORMAT.md 47ffd0caf000. Logs:
  .loop/task11-donecheck.log, .loop/task11-probe-*.log,
  .loop/task11-regs.log + .loop/task11-reg-*.log.
- zoom-out: (1) The sbid seam is an INSTRUMENT-level answer to a new
  CLASS: "sim state fed by a third-party library's global counter"
  (howler ids). Rule 14's chain-order lesson generalizes to VALUE
  chains: when a boundary consumes a stream the records can't
  reconstruct, record-and-inject at the exact consumption site (the
  task-5 launch-getter discipline) rather than modeling the library.
  Puff's FURAFURA furaLoopID (task 7's trap) is the same class — task
  12 can now un-trap it with the same sbid mechanism if puff carries
  live FURAFURA. (2) The ledger's single miss is a RECIPE bug, not a
  translation bug: task 10 minted "delta-2 files are renames" and this
  task inherited it without re-reading the diff BODY — the class fix is
  the pinned rule (read the body; a 2-line delta can be 2 semantic
  lines), and the probe-replay step caught it mechanically before any
  full run. (3) The char-data-plane write (dmg) extends falcon's
  canEdgeCancel class from FLAGS to VALUES the M1 tables own: the
  zoom-out answer stays "measure the live domain, restore in the
  sweep, leave the loud trap for the unexercised arm" — no C module
  state, because the capture pre-marshals bound the exposure to init
  assigns. (4) Cost curve: task 8 two class fixes, task 9 two model
  widenings, task 10 zero, task 11 one recipe-inherited miss + one
  planned widening (dancingBlade pair) — the recipe holds mechanical;
  puff (task 12, the last per-char cluster) inherits the special-phase
  hook (NEUTRALSPECIAL* onPlayerHit/onWallCollide), the sbid mechanism
  (furaLoopID), and the table-override lesson (puff's FURAFURA/
  JUMPAERIALB/F are per-char OVERRIDES of shared states — rule 15's
  origin map already distinguishes them).
- next: task 12 (characters/puff moves, 4,599 ln — puff carriers:
  g02 falco/PUFF/ystory, g04 PUFF/falcon/dreamland, g08 fox/CPU-PUFF/
  fdest (probe-measure g08's CPU puff per the carrier convention);
  puff OVERRIDES shared FURAFURA/JUMPAERIALB/JUMPAERIALF (rule 15's
  origin map measures them as puff-origin); NEUTRALSPECIAL* carry
  onPlayerHit/onWallCollide (the task-10 hook); FURAFURA's furaLoopID
  = sounds.furaloop.play() un-traps with the task-11 sbid mechanism;
  ROLLOUT family fields modeled since task 2).

## iter 31 — 2026-07-14 — M2 task 12: characters/puff moves — MOVES puff MATCH

- phase M2, task 12 (the puff per-char move set: 71 move objects, 221
  puff-origin fns — the fifth and LAST per-char cluster; first with
  table OVERRIDES of shared states, the first LIVE mdispatch seam, and
  the chd char-data-plane mechanism — NEW RULE 17).
- done-check: `bash port/sim/calib/check-moves-puff-replay.sh` → exit 0,
  `MOVES puff MATCH` (.loop/task12-donecheck.log). Per golden
  (g02/g04/g08): two fresh captures byte-identical (cmp), STREAM MATCH
  ×2 against the frozen streams (non-perturbation; finalCheck mvData
  re-dump drift-guarded), pins green
  (expected-capture-moves-puff.json), strict C replay 0 divergences —
  1362 + 1716 + 2935 = 6013 records: 4307 mutation-captured puff-origin
  fn calls (404 frame-0 rule-11/12 sweep records per golden on a
  separate sweep mulberry32), 1 LIVE mdispatch seam (g08 — see below),
  0 article records (puff has ZERO `articles` references anywhere under
  characters/puff/, marth-strength; pinned ZERO with the
  unconsumed-FIFO tripwire), 1699 standalone seeded draws (1491 on
  g08's AI plane, chain-verified draw-for-draw), 3 mvData + 3 rngBoot
  records.
- GOLDENS: g02 falco/PUFF/ystory · g04 PUFF/falcon/dreamland · g08
  fox/CPU-PUFF/fdest — the puff carriers, PROBE-MEASURED per the
  task-10 carrier convention (live puff-origin move records:
  843/1215/1037). g08's d5 CPU puff ATTACKS (unlike g07's falcon) and
  fires the FIRST LIVE mdispatch seam of any per-char cluster: its
  THROWBACK dispatches fox's THROWNPUFFBACK on the victim's table at
  frame 353 — the seam FIFO's first live exercise (args verified in
  call order, post resynced).
- conformance guard + regressions ALL GREEN (.loop/task12-reg-*.log):
  ENVCOLL MATCH · UTIL MATCH · PLAYER MODEL MATCH · INPUT MATCH ·
  ASSHORT MATCH · PHYSICS MATCH · HITDET MATCH · MOVES SHARED MATCH ·
  MOVES fox MATCH · MOVES falco MATCH · MOVES falcon MATCH · MOVES
  marth MATCH — every prior spec re-verified against the edited shared
  TUs (ml_player.h/player_canon.c gained rollOutTurnTimer).
- design: task 8's recipe followed (wrap puff-origin by module-index
  identity on table 1 — 217 phase fns + 3 onPlayerHit + 1
  onWallCollide; shared entries unwrapped; non-puff per-char entries
  mdispatch seams; mvData extended with puff {origin, data} — served by
  mv_puff_arr/pair/len, rule 15; sweep scaffolding reused with
  puff-measured arms). TABLE OVERRIDES measured and asserted BOTH
  directions (rule 15's origin map): puff's index REPLACES shared
  FURAFURA/JUMPAERIALB/JUMPAERIALF on table 1; fn identity classifies
  them puff-origin there, shared on tables 0/2/3/4. Puff's FURAFURA is
  TRIVIAL (WAIT.init only — NO furaloop/furaLoopID), so the task
  brief's planned sbid un-trap is measured-moot: no puff move consumes
  a Howl id (no `= sounds.` assignment under characters/puff/ — the
  sbid mechanism is NOT carried; deviation from the brief, measured).
  NEW MECHANISM (rule 17, the char-data-plane class resolved at CLASS
  level): every move record's pre carries "chd" — the EXECUTED
  charHitboxes {moveKey: {idN: {dmg, size}}} VALUE plane at record
  time. Puff writes the M1-owned plane at runtime through
  player.hitboxes.id ALIASES: NEUTRALSPECIAL{GROUND,AIR}'s post-release
  dmg writes run EVEN WHEN UNCHARGED — through whatever STALE id
  objects the previous move assigned (cross-move provenance that a
  value-copy C model cannot track) — and UPSPECIAL (sing) cycles
  id[0].size through 10.937/1/12.890. MEASURED before building: g04's
  live puff drifted jab1's dmg 3→7 at frame 1038 (983 records carry
  the drifted plane; g02/g08 live-clean — their non-baseline chd
  variants are the sweep's own restored mutations). The C's
  pf_assign_hitbox_id feeds dmg/size from chd, never assumed-pristine
  CTAB1 — dmg/size are the ONLY createHitbox fields with upstream
  write sites (grep-measured over characters/ + physics/ + main/);
  task 11's marth-style documented hole would have produced LOUD live
  divergences here. Special phases: onPlayerHit ×3 + onWallCollide
  through the task-10 mv_register_special_phases hook;
  mv_register_char_module(1, puff_move_def) makes checkForIASA's
  char-1 PUFFMOVES arm real (dead-by-construction upstream — marth
  precedent). Helper modules puff_multi_jump_drift/puff_next_jump
  (characters/puff/*.js level — the dancing-blade-helpers analogue);
  puffNextJump dispatches COMPUTED index keys
  "AERIALTURN"/"JUMPAERIAL" + (1 + jumpsUsed) — rung 6 unreachable
  (every multijump dispatch jumpsUsed<5-guarded or capped by
  JUMPAERIAL5's armless interrupt), trapped.
- puff STRUCTURE deltas (measured per-file diffs BEFORE translating —
  bodies read in full, the task-11 lesson held; all carried verbatim):
  ROLLOUT (NSG/NSA/NSGT) is a movement special with its own
  charge/launch/turn machine on runtime-added phys.rollOut* — NSG/NSA
  mains do NOT advance timer at the top (the charge-scaled advance
  `timer += 1 + 2*(charge/44)` sits MID-BODY; sweep presets must hit
  exact arm timers) and their interrupts ALWAYS return false (the
  WAIT/FALLSPECIAL arms fire the inits and still return false); NSG's
  overlay check runs AFTER its timer advance, NSA's BEFORE (per-file);
  the multijump ladder AERIALTURN1-5/JUMPAERIAL1-5 (cVel.y rungs
  1.65/1.59/1.47/1.36/1.25, rungs 2-5 set doubleJumped, AERIALTURN
  flips face at t===6 and hands off at t===13, JUMPAERIAL1-4 carry the
  t>28 multijump arm, 5 does not); ATTACKAIRN's t===7 increments
  hitboxes.FRAMES (plural) and ATTACKAIRB's t===8 writes lowercase
  phys.autocancel — upstream typos on runtime-added fields (modeled
  since task 2); THROW* dispatch victims 2-ARG through the TABLE with
  fractional timers (K/releaseFrame, floor(+0.01) crossings);
  THROWBACK's window carries the floor-over-COMPARISON typo
  (`Math.floor(timer + 0.01 < 37)` — floor of a boolean, truthiness
  kept verbatim, rule-13 family); THROWDOWN's crossing has NO
  grabbing===-1 guard where FORWARD/BACK do; THROWN{PUFF,MARTH,FOX}*
  guarded with per-file variation (THROWNPUFFUP's vacuous
  `if(player[p].phys)` nesting, THROWNMARTHFORWARD's clamp-BEFORE-guard
  order, THROWNMARTH* init pos snaps, THROWNMARTHBACK/THROWNFALCOBACK
  flip face but keep PLAIN-face x, THROWNFALCONDOWN has reverseModel
  but NO flip); THROWN{FALCO,FALCON}* are the old-style unguarded
  family (init pos snap; player[-1]/overruns throw — traps);
  THROWNPUFFBACK's offsetVel arm is COMMENTED OUT upstream (dead data,
  dumped anyway); all 8 CLIFF* keep the canGrabLedge table-write trap
  arm and CLIFFGETUPQUICK alone sets ledgeRegrabCount=TRUE; pound
  rotates airVelocities by lsY*PI*(20/180) via fdlibm sin/cos.
- DIVERGENCE LEDGER: EMPTY — 0 replay divergences on the first
  successful build across all 6013 records (rules 1-17 held). The
  class work was front-loaded and capture-first: (1) the chd mechanism
  was designed from the measured g04 drift BEFORE any C ran; (2)
  phys.rollOutTurnTimer was widened (rule 16's budgeted round — the
  only value-model change); (3) two sweep-preset authoring fixes
  (NSG/NSA's no-top-advance timers) were caught by reading the bodies,
  pre-capture.
- comparator negative tests (all restored, tree re-verified 0 div):
  (a) corrupted POST players nibble → exactly 1; (b) FORWARDTILT
  active 6→5 → 5/19/5 live divergences (bites occurring values — rule
  12); (c) THROWUP hq-push flag → exactly 1; (d) THROWUP dispatched
  through an unregistered state → seam-underflow (FIFO teeth); (e) chd
  IGNORED (pristine CTAB1 dmg/size) → 2/8/2 — bites g04's LIVE drifted
  assigns, the mechanism's own teeth; (f) extra UPSMASH randomShout
  draw → 16 each (chain cascade); (g) release-vel 0.09→0.08 → 2
  sweep-only (live releases were charge-0: the formula is invariant
  there — rule-12 corollary, 4th measured instance); (g2) newDmg base
  12→11 → 8/378/8 (bites g04's 378 live dmg-write records); (h)
  onPlayerHit registration removed → OUT OF DOMAIN at the first record
  (the upstream missing-property TypeError); (i) sing size 12.890→12.5
  → exactly 1 each.
- honest coverage (documented, not silent): live puff
  THROW*/THROWN*/CLIFF*/sing-late-windows/rest are zero-to-thin over
  g02/g04/g08 — sweep-covered (404 calls incl. the full rollout
  machine both environments with the STALE-id dmg arms, all sing size
  windows, rest twins, pound incl. the rotated arm, the whole
  multijump ladder, THROW* crossings + CATCHCUT/-1 arms, all 20
  THROWN* inits + guard/clamp/order arms, all 8 CLIFF*), seams/traps
  loud on first live record; CLIFF* canGrabLedge write arm traps
  (mvData drift-guarded); unguarded-THROWN -1/overrun arms unswept
  (upstream throws); AERIALTURN6/JUMPAERIAL6 unreachable by
  construction. Live coverage DID include: the FULL live rollout
  domain (g04's 378-record dmg-write arc — the drifted chd plane
  replayed bit-exactly — plus g08 CPU rollouts and a live NSGT turn),
  the multijump ladder, JAB1/JAB2, all tilts and smashes, aerials
  incl. land arms, GRAB, DOWNATTACK, and puff as grab victim
  (THROWNPUFFBACK live via the g08 seam).
- artifacts (sha256/12): puff moves.h b87d97b1a350 · puff
  moves_index.c 53173218a916 · puff moves/*.c (71 files, 5658 lines)
  29f846131b54 · puff_multi_jump_drift.c 2be58fb5ffea ·
  puff_next_jump.c e2dce0763bd6 · spec-moves-puff.js bda8ad01d79d ·
  replay_moves_puff.c ffd31fa39602 · expected-capture-moves-puff.json
  3fb98382ae10 · check-moves-puff-replay.sh 931ffc04db74 · ml_player.h
  762cc6131113 · player_canon.c c5cff799396c · FORMAT.md 18ac9fab0f75.
  Logs: .loop/task12-donecheck.log, .loop/task12-probe-*.log,
  .loop/task12-cap-*.log, .loop/task12-reg-*.log.
- zoom-out: (1) NEW RULE 17 (fix_plan §M2): the M1-owned char-data
  VALUE plane is MEASURED per record where upstream writes it. The
  falcon-canEdgeCancel → marth-dmg → puff-dmg/size progression was one
  CLASS growing: flags (module state), then a benign value write
  (documented hole), then live-drifting values with cross-move STALE-id
  provenance no value-copy model can track. The class fix is the chd
  pre-projection — executed data per record, instrument > documented
  hole — and it was chosen from MEASUREMENT (the g04 drift probe),
  not speculation; the marth approach would have diverged loudly on
  g04. Task 17's integration replaces the projection with the sim's
  own live plane (C owns charHitboxes then — the projection is exactly
  the state the integrated sim maintains anyway). (2) The sbid
  non-carry is the same discipline in reverse: the brief predicted the
  mechanism would be needed; measurement (no Howl-id consumption in
  puff) deleted the surface instead of porting it. Briefs predict;
  captures decide. (3) Cost curve across the per-char ladder: task 8
  two class fixes → 9 two widenings → 10 zero → 11 one recipe miss →
  12 ZERO divergences with one planned widening — but 12's real cost
  moved UP-FRONT into measurement (probe + drift diff + body reading).
  The recipe's final form: measure carriers, diff and READ bodies,
  survey the value domain, design seams from measured chains, THEN
  translate. (4) The per-char surface is COMPLETE (tasks 7-12, five
  chars + shared, ~28.7k upstream lines translated at 0-2 divergences
  per cluster); the remaining M2 surface is articles (13), platforms
  (14), the formatter (15), the AI bridge (16), and integration (17) —
  no more per-char unknowns; rule 17's chd hand-off is the only new
  contract task 17 inherits from this iteration.
- next: task 13 (articles — article.js 639 ln: fox/falco laser +
  ILLUSION queues and article hit detection; articles are CHECKSUMMED
  (CHECKSUM.md §2 `articles` key); the task-8/9 article-seam FIFOs
  (LASER/ILLUSION init args verified bit-exactly on g01/g03/g08 +
  g02/g05/g07) become real C bodies; puff/falcon/marth pinned
  article-zero).

## iter 32 — 2026-07-15 — M2 task 13: articles (article.js -> port/sim/article.{c,h})

- phase M2, task 13 (fix_plan §M2). done-check:
  `bash port/sim/calib/check-article-replay.sh` -> ARTICLE MATCH, exit 0
  (.loop/task13-donecheck.log). Conformance guard
  `bash port/sim/check-envcoll.sh` -> ENVCOLL MATCH (task13-reg logs).
- WHAT: the fox/falco projectile plane became real C. Capture spec
  `spec-article.js` (1101 wrapped: 4 gameTick pipeline calls
  mutation-captured over the three article queues — aArticles IS
  CHECKSUM.md §2's checksummed `articles` key — + resetAArticles
  endGame-only pinned ZERO + LASER/ILLUSION init as first-class "ainit"
  mutation records + 6 internal-only helpers pinned ZERO + 1088 per-char
  seam loggers, moves-shared machinery). C `port/sim/article.{c,h}`
  (750+131 ln): MlArticle/MlArticles value model (LASER 13-key /
  ILLUSION 8-key instance shapes, survey-measured; hb = per-article
  12-key createHitbox with offsetSingle — article-owned, NO rule-17
  global-plane aliasing), structure-parallel bodies for init/main/
  execute/destroy/hitDetection/executeHits/wallDetection + the 5
  collision helpers; executeArticleHits dispatches GUARD/SHIELDBREAKFALL/
  DAMAGEFLYN/DAMAGEN2/CAPTUREDAMAGE through mv_dispatch into task-7's
  REAL shared bodies, with task-6 hit_detection linked (getKnockback/
  getHitstun/knockbackSounds real; hd_flags crouch/vCancel; CTAB1
  weight) and screenShake's 4 seeded draws. Replay driver
  `replay_article.c` (1422 ln) + pins + check script.
- CARRIERS (probe-MEASURED over all six fox/falco goldens — the task-10
  discipline): g01 27 live spawns / 12 live hits (fox, battlefield),
  g02 19/6 (falco, ystory), g08 27/21 (fox CPU, fdest; 1496 AI draws).
  Measured OUT: g03/g05 field ZERO live articles; g07 spawns 19 lasers
  that never connect. Measured: ZERO live ILLUSIONs on ANY golden (all
  live articles are lasers); all live eah rows are HURT rows on victim
  slot 1; live fox laser hits are ZERO-KNOCKBACK (kg=bk=sk=0 —
  percent-only; g01/g08's 33 hits never draw/dispatch); g02's falco
  lasers are the ONLY live kb>0 hits (live screenShake draws + live
  DAMAGEN2 dispatches). All six goldens replayed: 0 divergences each
  (the three non-carriers as free extra evidence).
- RESULT: 45,229 records over g01/g02/g08 (14,400 live pipeline + 41
  sweep records per golden + ainit + dumps), byte-stable x2, 6x STREAM
  MATCH, 0 replay divergences on the FIRST successful build across all
  SIX goldens (rules 1-18 held; divergence ledger opened and closed at
  zero). Sweep: 72 rule-11/12 calls covering every reachable zero-live
  arm (spawn variants, movement/death ladders, duplicate-destroy
  splice(-1) quirk, reflect/powershield/shieldbreak/vCancel/crouch/
  CAPTUREDAMAGE/groundBounce/blunthit, interpolated arms, clank-loop
  read path, clean miss); no reachable arm is unswept.
- NEW RULE 18 (fix_plan §M2): module state fully enclosed by captured
  boundaries gets a QUEUE-CHAIN instrument (C chains it across records;
  the replay compares chained state against every in-match record's pre
  before re-marshaling — a wrong mutation flags at the NEXT record even
  when its own record replays clean) + LEAN-WHEN-EMPTY envelopes (the
  read-set projection gated on driving-queue emptiness: ~20x capture
  size cut, zero teeth loss). Tasks 14 (movingPlatforms stage state)
  and 17 inherit both.
- UN-SEAMING (tasks 8/9 -> 13): the moves-fox/falco article-seam FIFOs
  stay as-is (their checks re-run GREEN); the article capture records
  the SAME upstream crossings at the article module boundary and replays
  them through the REAL C bodies — the seam's [name, options] canon is
  byte-identical to the ainit args prefix, so task 17's integration
  replaces mv_article_laser*/mv_article_illusion* driver seams with
  direct article.c calls (documented, FORMAT.md "The article spec").
- upstream quirks carried verbatim (never fixed): ILLUSION init's
  `options.isFox || true` is ALWAYS true (falco illusions get fox hb
  values); LASER wall-death consumes the sweep parameter by TRUTHINESS
  (sweep 0 falsy); the hurt arm never sets articleDestroyed ->
  duplicate destroy pushes -> `splice(queue[k]-k, 1)` can go NEGATIVE
  (JS splice removes from the END — art_splice1 implements ToInteger +
  negative-start); executeArticleHits ALIASES hit.hitPoint to the
  article's pos (modeled by value — unobservable: destroyOnHit lasers
  never run another main, ILLUSION main reassigns pos first); the clank
  block is commented upstream but its GUARD evaluates (id[k].clank reads
  through both hitbox shapes); LASER.init's strokeStyle/fillStyle are
  render-only table writes; SHIELDBREAKFALL's `break` exits the whole
  row loop skipping GUARD.init.
- comparator negative tests (all restored, tree re-verified 0 div):
  (a) corrupted POST nibble -> exactly 1; (b) fox laser speed 7->6 ->
  77/23/77 live; (c) screenShake 4->3 draws -> 1/99/1 (bites g02's live
  kb>0 hits); (d) ahd hitList push dropped -> 26/14/44 live; (e)
  DAMAGEN2 dispatch dropped -> 6 live on g02, 0/0 elsewhere (kb=0 hits
  never dispatch — measured, not assumed); (f) splice -k dropped -> 1
  each (sweep-only: live dstq len <= 1 — rule-12 corollary, 5th
  instance); (g) foxshinereflect typo / ILLUSION kg 40->41 -> 1 each
  (sweep); (h) post-record chain corruption -> 27/19/27 PURE chain
  divergences with zero record divergences (rule 18's own teeth).
- honest coverage: live reflects, shield hits, powershield reflects,
  shieldbreaks, illusions, multi-destroy frames, CAPTUREDAMAGE/
  groundBounce/vCancel/blunthit arms are ZERO over the carriers — all
  sweep-covered; mdispatch measured zero (FIFO teeth proven); the dead
  else of ILLUSION's ground patch is unreachable by construction
  (isFox always true).
- REGRESSIONS: ALL twelve prior cluster checks re-run GREEN cold (UTIL/
  PLAYER MODEL/INPUT/ASSHORT/PHYSICS/HITDET/MOVES SHARED/fox/falco/
  falcon/marth/puff MATCH) + ENVCOLL MATCH (.loop/task13-reg-*.log) —
  the diff touches no prior C or spec surface (new files + docs only).
- artifacts (sha256/12): article.h a0a7664259fc · article.c 1e6d3140e680
  · spec-article.js a864d6d882fe · replay_article.c d60bd510f595 ·
  expected-capture-article.json 59418efa1f9e · check-article-replay.sh
  e94a4633c7ef · FORMAT.md ac9cc2885090. Logs: .loop/task13-donecheck.log,
  .loop/task13-probe-g0{1,2,3,5,7,8}.log, .loop/task13-survey-g01.log,
  .loop/task13-reg-*.log.
- zoom-out: (1) NEW RULE 18 — the chain-verify + lean-when-empty pair is
  an INSTRUMENT-level answer (hierarchy: instrument > class fix) to two
  latent classes at once: silent inter-record queue drift (previously
  only caught when a later record's own body happened to read the
  drifted state) and capture-size bloat from full envelopes on
  no-op-frame records (hitdet's 542 MB class). It was designed BEFORE
  any C ran, capture-first. (2) The un-seaming closes the loop the task-8
  brief opened: seams are IOUs, and this task shows the redemption
  pattern — capture the same crossings at the owning module's boundary,
  replay with real bodies, prove the seam args byte-identical. Task 14
  (platforms: the M1-externals-stubbed god-module bodies) and task 17
  redeem the remaining IOUs the same way. (3) Measurement again beat
  the brief: "laser spam is common" predicted rich hit coverage, but
  fox lasers turned out ZERO-knockback — without the g02 falco carrier
  the entire kb>0 arm family (draws + dispatches) would have been
  sweep-only; carrier choice by measured live coverage (not char
  presence, not spawn counts) is now three-times-proven doctrine.
- next: task 14 (movingPlatforms stage-tick logic — ystory/fountain
  updatePlatform + pstadium if live; the M1-externals-stubbed god-module
  bodies; stage pre/post-state capture; rule 18's chain instrument
  applies to the stage plane).

## iter 33 — 2026-07-15 — M2 task 14: movingPlatforms stage-tick logic — PLATFORMS MATCH

- phase M2, task 14 (fix_plan §M2). done-check:
  `bash port/sim/calib/check-platforms-replay.sh` -> PLATFORMS MATCH,
  exit 0 (.loop/task14-donecheck.log). Conformance guard
  `bash port/sim/check-envcoll.sh` -> ENVCOLL MATCH
  (.loop/task14-reg-envcoll.log).
- WHAT: the last M1-externals-stubbed god-module bodies became real C.
  Capture spec `spec-platforms.js` (6 wrapped: every VS stage object's
  movingPlatforms — main.js:1058, the FIRST call of the mode-3 tick, one
  record per frame; per-call assert getActiveStage() === the owning
  stage object). C `port/sim/stages/{moving_platforms.{h,c},ystory.c,
  fountain.c}`: MpSim slice struct (full platform plane, platformStates,
  starting, 4x player {grounded,onSurface,pos}); ystory = Randall's
  four sequential NON-exclusive rail arms + rider loop; fountain =
  updatePlatform's private state machine (seeded draws via ml_random) +
  starting reset arm + transfer arms; the four other stages are EMPTY
  upstream (measured) — literal no-ops behind the mp_movingPlatforms
  dispatcher. Replay driver `replay_platforms.c` + pins + check script.
  No M1 tables consumed: the rail/platform constants are upstream CODE
  literals (edgeOffset precedent); the movingPlats STAB1 presence pins
  are asserted at install.
- NEW RIG MECHANISM (the __wpCache injection class, 2nd instance):
  fountain's platformStates is module-PRIVATE (a closure `let` no export
  reaches) — run-capture.js's served dist/js/main.js gains a SECOND
  behavior-neutral injection: a quote/newline-free window getter
  (`window.__mpFountainPS`) right after the declaration (unique-match
  hard-fail; safe inside the webpack eval-wrapped module string; disk
  untouched). This made rule 18's chain-verify DIRECT (ps observable in
  every pre/post) instead of effects-only, let survey-shapes measure the
  real ps value domain capture-FIRST, and let the sweep drive the
  private machine through its live entry objects.
- RULE 18 EXTENDED: the chain carrier is the FULL platform plane (static
  platforms included) in every record — the "nothing else writes the
  platform plane" enclosure assumption is now a per-record MEASUREMENT.
  g01 (battlefield) is captured as the static-stage class representative:
  3600 lean {plat} records pin the one-call-per-tick contract and prove
  plane non-drift on a static stage.
- CARRIERS g01/g02/g06 — by STAGE IDENTITY, a measured-coverage
  degenerate case: only ystory (g02) and fountain (g06) have non-empty
  movingPlatforms bodies (the brief's "pstadium if live" resolved: its
  body is EMPTY, nothing to translate). Live coverage measured
  (.loop/task14-cap-g06.log + probe): g06 fields the FULL fountain
  machine — 90 starting-arm frames, 27 owner draws over 26 records, 15
  distinct destinations incl. the sink (-additionalOffset) and
  base-return (19.875) arms, both newTimer formulas; g02 fields
  Randall's full rail loop (~3 circuits, all arms + both double-fire
  corners). ZERO live anywhere: the ystory rider arm and all four
  fountain transfer arms — sweep-covered (52 rule-11/12 calls;
  platformStates restored by the starting arm itself: the reset IS the
  restore).
- RESULT: 3787 + 3778 + 3786 records over g01/g02/g06 (3652
  movingPlatforms each = 3600 live + 52 sweep), byte-stable x2, 6x
  STREAM MATCH, 0 replay divergences on the FIRST successful build
  (rules 1-18 held; divergence ledger opened and closed at zero).
- upstream shapes carried verbatim: the rail arms' last-arm-wins `move`
  reassignment (corner frames run TWO arms in one call: arm1->arm3
  bottom-left, arm2->arm4 top-right; a bottom-right double is IMPOSSIBLE
  — arm 2 is tested before arm 3); the commented-out `pos.y += move[1]`
  rider line (pos.y snaps to plat[0].y instead); fountain's selection
  draw CONSUMED even when the |y|<0.075 base-return arm ignores t;
  `timer--` on fractional timers; the transfer guard reading the
  POST-update platform y (a rising platform transfers only while still
  below 0.075 after its +0.075 step).
- comparator negative tests (all restored, tree re-verified 0 div):
  (a) corrupted POST nibble -> exactly 1; (b) rail step
  0.354845->0.354846 -> 7206 live on g02; (c) arrival band 0.075->0.076
  -> 1927 live on g06; (d) selection draw hoisted out of the base-return
  arm -> 199 on g06 (the consumed-but-ignored draw bites); (e) starting
  reset 22.125->22.126 -> 272 live on g06; (f) chain corruption after a
  clean record -> 2/1 PURE chain divergences (chain-plat + chain-ps on
  g06, chain-plat on g02; zero record divergences — rule 18's own
  teeth); (g) ground->platform transfer dropped -> 1 sweep-only
  (rule-12 corollary, 6th instance); (h) rider arm dropped -> 8
  sweep-only on both goldens.
- honest coverage: live riders and transfers are ZERO over the carriers
  (falcon/marth never rode a moving platform when it mattered) — all
  sweep-covered; no reachable arm is unswept; the four empty stage
  bodies are vacuously exact (their records still pin count + plane
  non-drift).
- REGRESSIONS: ALL thirteen prior cluster checks re-run GREEN cold
  (UTIL/PLAYER MODEL/INPUT/ASSHORT/PHYSICS/HITDET/MOVES SHARED/fox/
  falco/falcon/marth/puff/ARTICLE MATCH) + ENVCOLL MATCH
  (.loop/task14-reg-*.log) — required this iteration because the diff
  touches shared rig infrastructure (run-capture.js's second injection);
  the green re-runs prove the injection behavior-neutral across every
  spec.
- artifacts (sha256/12): moving_platforms.h 8d041e360561 ·
  moving_platforms.c 40a5a23fb093 · ystory.c 55f4d3169a8e · fountain.c
  e22982bc5aea · spec-platforms.js 2b8a74b9e9ff · replay_platforms.c
  67b76e864e47 · expected-capture-platforms.json 0f7bb8ba4f15 ·
  check-platforms-replay.sh 0c18da95178b · run-capture.js a6d8f4d45bb4 ·
  FORMAT.md 6dc43ee74db9. Logs: .loop/task14-donecheck.log,
  .loop/task14-cap-g06.log, .loop/task14-survey-g06.log,
  .loop/task14-reg-*.log.
- zoom-out: (1) The platformStates getter is the instrument-level answer
  (hierarchy: instrument > class fix) to a CLASS this task surfaced:
  module-private closure state that a boundary's behavior depends on.
  The alternatives — mirroring the state machine JS-side (transcription
  the capture could not check: curve-fitting hazard) or chaining it
  blind in C (effects-only verification with reconstruction ambiguity) —
  were both rejected for direct observation via a served-bytes exposure,
  the same mechanism class that already exposes the webpack cache. Task
  17's integration inherits the lesson: any remaining private state
  (main.js's module lets) must be observed, not reconstructed. (2)
  Rule-18's chain carrier widened from "the state the module owns" to
  "the whole plane the module is ASSUMED to own exclusively" — turning
  an enclosure assumption into a measurement for free (~300 B/record).
  That generalizes: task 17's per-frame checksum IS this instrument at
  full scope. (3) Carrier-by-measurement hit its degenerate case: when
  coverage is determined by stage identity (a per-golden constant),
  probing all eight goldens would have been ritual — the measurement was
  reading the six stage bodies. Measurement discipline includes knowing
  when the measurement is already done.
- next: task 15 (ECMAScript shortest-float formatter + CHECKSUM.md ser
  in C — differential against JS String(x) over captured snapshots + an
  adversarial sweep; done-check check-format.sh -> FORMAT MATCH).

## iter 34 — 2026-07-15 — M2 task 15: ECMAScript float formatter + CHECKSUM.md ser in C — FORMAT MATCH

- phase M2, task 15 (fix_plan §M2): the Ryu-class risk flagged since
  M2-CAL — the C `String(x)` + CHECKSUM.md §3/§4 serializer that task
  17's checksum-stream emission byte-equality stands on.
- done-check: `bash port/sim/calib/check-format.sh` -> FORMAT MATCH,
  exit 0 (.loop/task15-donecheck.log; failure paths verified nonzero by
  direct invocation — see gotcha below).
- WHAT: (1) vendored the Ryu double->shortest-decimal core at port/ryu/
  (ulfjack/ryu pinned @ commit 4c0618b0e44f7ef027ebae05d2cc7812048f7c8f,
  7 files + both license texts byte-verbatim, per-file sha256 pins in
  port/ryu/PROVENANCE.sha256 re-verified at the top of every check run;
  NOTICES entry added BEFORE the code landed — Apache-2.0 OR BSL-1.0
  dual, headers carried, ZERO local modifications). (2) port/sim/
  ml_fmt.{h,c}: ml_fmt_dtoa = ECMA-262 §6.1.6.1.20 steps 6-10 applied
  to Ryu's digits+exponent (#includes the verbatim ryu/d2s.c into its
  own TU to reach the static d2d/d2d_small_int/decimalLength17;
  small-int trailing-zero fold mirrors d2s_buffered_n, d2s.c:475-493);
  specials NaN/±Infinity/0 (String(-0)==="0") handled bitwise — the TU
  contains no FP arithmetic at all. (3) port/sim/ml_ser.{h,c}: the §3
  ser primitives citing pagelib.js line-by-line (ml_sb_num = numStr with
  the explicit -0 token, pagelib.js:10-13; JSON.stringify escaping;
  T/F; undef/null/fn/cyc tokens; envelope contract documented =
  pagelib.js:41-64 fixed-literal order) + ml_sha256_hex (§4, links
  oracle/qjs/sha256.c — the NIST-self-tested TU the qjs oracle uses; no
  copy, one implementation). (4) the differential rig: calib/fmt_diff.c
  (--self-test 28 frozen anchors + escape/hash pins; --gen; --format;
  --extract; --composite with §3.1 envelope builder), fmt-js-ref.js +
  fmt-composite.js (JS reference side), check-format-pins.js +
  expected-format.json, check-format.sh.
- DIFFERENTIAL COVERAGE (all cmp-byte-exact, 0 divergences on the first
  full run of each corpus — no burn-down was needed):
  * adversarial: 5,469,538 deterministic patterns (corpus sha256 pinned
    e3d542b79b19…): specials + payload NaNs, all 2047 exponents × 8
    mantissa templates × both signs, subnormal bit ladders, powers of
    2/5/10 ±3 ulp, ECMA 1e21/1e-7/1e-6/1e22 threshold straddles ±64 ulp,
    67 known-hard shortest-repr/parse literals ±64 ulp, 4M seeded raw
    u64 + 1M integer-path + 1M short-decimal + 400k subnormal/extreme-
    exponent randoms. C String(x)+serNum vs V8 String(x)+oracle numStr.
  * captured: 249,225 unique double bit patterns extracted from ALL 55
    existing capture jsonl files (5.7 GB — every spec, every golden;
    cold-run floor = 26,478 = g01 player+article baseline, pinned as
    minimum). Same two-column differential.
  * composite: 39,971 cases — every g01 player post snapshot (7,200) +
    every article args/post envelope (29,135) as V record-tree cases +
    3,600 per-frame CHECKSUM.md §2/§3.1 envelopes (captured slot
    snapshots + that frame's post aArt queue; 760 with live articles;
    -0 live in 3 cases) + 36 synthetic 4-slot envelopes (p2/p3 arm).
    C canon-parse->ser->SHA-256 vs the oracle's OWN code: ser/numStr
    extracted from pagelib.js source bytes (new Function over the
    slice), envelopes through the actual window.__serializeState +
    window.__sha256 under window===global. Zero transcription on the
    reference side.
  * byte-stability: C --format twice -> cmp identical; corpus generator
    seeded (splitmix64), sha256-pinned.
- comparator negative tests (perturb -> count -> restore -> re-verified
  clean): (a) step-6/7 threshold n<=21 -> 22 -> 27,075 adv diffs; (b)
  step-8 bound -6 -> -7 -> 16,229; (c) positive exponent '+' dropped ->
  2,156,736; (d) -0 ser token dropped -> 3,294 adv + 3 LIVE composite
  (g01 fields real -0); (e) T/F swapped -> 17,061 composite; (f)
  envelope literal order (timer/percent swapped) -> 3,636 = every
  envelope case; (g) sha256 hex nibble -> 39,971 = every case. Tooth (b'
  — small-int trailing-zero fold) is OUTPUT-NEUTRAL by construction
  (small ints < 2^53 < 1e16 never reach exponent form): documented, not
  faked.
- REGRESSIONS: none required — the diff adds new TUs/files only (canon.c
  , run-capture.js, all replay drivers, all spec files untouched; the
  teeth edits to ml_fmt.c/ml_ser.c/fmt_diff.c were restored and the
  restored tree re-passed the full done-check).
- artifacts (sha256/12): ml_fmt.h 4278f702aafa · ml_fmt.c 7ed7ac33b3d3 ·
  ml_ser.h 376d363a768b · ml_ser.c d66dc5c33d1a · fmt_diff.c
  2ffca40bc972 · fmt-js-ref.js ac721309cf49 · fmt-composite.js
  68a1a4bdcd8c · check-format.sh d7151f6cf94b · check-format-pins.js
  79983717678b · expected-format.json f3f221825b28 ·
  port/ryu/PROVENANCE.sha256 47cfc5c220a6. Log:
  .loop/task15-donecheck.log.
- gotcha classes (new): (1) DIFFERENTIAL REFERENCES MUST NOT BE
  TRANSCRIPTIONS — a transcribed JS ser would mirror its own bugs on
  both sides; the rig evals the oracle's actual pagelib.js bytes
  (numStr/ser via source slice, envelopes via the real
  __serializeState/__sha256 under the window===global shim, the M1
  task-4 browser-parity class). (2) zsh `time cmd | tee` pipestatus
  reported 0 for a mid-pipe failure — gate exit codes are verified by
  direct invocation, never through a pipe.
- zoom-out: (1) This task is itself the class-level fix for the
  "float-format drift" defect class: every future C serialization site
  (task 17's stream emission, M3 device conformance, M4's grown
  surface) consumes ml_sb_num/ml_fmt_dtoa — one implementation, one
  differential harness, instead of per-site formatting decisions. The
  instrument (the 5.47M-pattern corpus + capture extraction) outlives
  the task: any future formatter touch re-proves against it for free.
  (2) Vendoring discipline generalized: the fdlibm precedent (pin +
  verbatim + NOTICES-first + mechanical provenance check IN the gate)
  was reused unchanged for Ryu — provenance verification belongs in the
  done-check, not in a one-time review. (3) The "-0 live in 3 composite
  cases" measurement closes a coverage question the corpus alone could
  not: the sim DOMAIN actually produces -0, so the ser token is
  load-bearing, not defensive.
- next: task 16 — AI-input bridge for CPU goldens (write-set recon
  hard-check, per-frame aiInputBank + RNG-draw counts over g07/g08,
  STREAM MATCH guarded; done-check check-ai-bridge.sh -> AI BRIDGE OK).

## iter 35 — 2026-07-15 — M2 task 16: AI-input bridge for CPU goldens — AI BRIDGE OK

- phase M2, task 16 (fix_plan §M2 "AI policy"): AI-as-recorded-input —
  the M2 exit gate covers CPU goldens g07/g08 while ai.js stays JS-side
  until M4; the bridge records the AI's verified write-set and the C sim
  replays CPU slots from it without executing AI logic.
- done-check: `bash port/sim/calib/check-ai-bridge.sh` -> AI BRIDGE OK,
  exit 0 (.loop/task16-donecheck.log). Per golden (g07/g08): x2
  byte-identical fresh captures, 2x STREAM MATCH (non-perturbation guard
  over the recon instrumentation), pins OK, bridge artifact
  deterministic (x2 + cmp), strict C replay 0 divergences (17812 +
  17893 records) — FIRST successful build, ledger opened and closed at
  zero.
- WHAT (capture): spec-ai.js wraps runAI (mutation record, post
  {bank: post-runAI aiInputBank[i][0], bk: {ca,cs,cta,lm} AI-private
  player bookkeeping incl. the upstream `curentAction` typo (ai.js:
  1254), rng: the in-window seeded draw list}) + the spec-input chain
  pair (pollInputs, physics [i, inputBuffers[i]]) — also the FIRST
  input-chain capture over CPU goldens. Draw accounting closes exactly:
  g07 = 81 standalone + 0 attributed (its falcon AI hits ZERO RNG
  arms); g08 = 162 standalone + 1334 attributed in 1081 runAI windows
  == the frozen rngCalls 1496.
- WRITE-SET RECONCILIATION (the task's hard-check, two instruments):
  (1) grep-measured: every LIVE assignment target in upstream ai.js is
  aiInputBank[i][0].{a,b,csX,csY,l,lA,lsX,lsY,x,y,z} or player[i]
  bookkeeping {currentAction,currentSubaction,lastMash,curentAction} or
  window.isOffstage (module-load fn def) — the player[i].inputs.* /
  phys.* / cpu.timer / bank .r writes are ALL inside block comments.
  (2) runtime, EVERY runAI call: in-page pre/post canon diff of all 4
  players (minus the 4-key allowlist), all 32 bank rows, playerType,
  characterSelections, gameSettings, activeStage -> wsViol records
  pinned ZERO + spec finalCheck throw. Measured: wsViol == 0 on both
  goldens across all runs. Cost: negligible (26s captures — full-player
  canon x8 per call is fine; airtight > sampled).
- WHAT (C): port/sim/input/ai_input.h — rule 16's CLASS FIX: the
  tagged JS-typed input model (MlAiVal bool|number|undefined — the AI
  writes NUMBER buttons, and helper-AI literals with missing keys
  assign UNDEFINED through `inputs.<f>` reads, e.g. CPUTech's
  {lsX,lsY,a} read as inputs.l) now carries THE single interpretInputs
  implementation (ml_ai_interpret_inputs, truthiness/ToNumber at the
  exact upstream coercion sites); interpret_inputs.c's
  ml_interpret_inputs became a conversion wrapper around it (human
  domain = all-bool buttons, asserted on exit; interpretPause exposed
  as shared ml_interpret_pause). port/sim/ai_bridge.{h,c}: AIBRIDGE1
  artifact loader + ml_ai_bridge_apply (burn recorded draws
  bit-verified on the CHAINED mulberry32; verify the never-AI-written
  fields dd,dl,dr,du,r,rA,raw*,s against the chain — the recording
  cannot smuggle state through fields the AI provably does not write;
  install the row). calib/build-ai-bridge.js distills the artifact
  (pure deterministic stream transform); calib/replay_ai.c chains BOTH
  slots through the shared MlInputSimState (human via
  ml_interpret_inputs, CPU via the tagged core), drives the bank from
  the ARTIFACT (task 17's consumption path) cross-checked against every
  runAI record, models the pollInputs bank-row ALIAS (slot 0 re-copied
  post-runAI — upstream runAI runs BETWEEN interpretInputs and physics
  inside update(i)), rule-18 chain-verifies the bank at every poll.
- upstream facts pinned during recon: harnessSetupMatch sets mType =
  "keyboard" for CPU slots too -> interpretInputs takes the KEYBOARD
  arm for them (the raw*/deaden AI arm at main.js:787-795 NEVER runs in
  the captured domain; bank raw* fields stay 0 for the whole trace);
  pollInputs returns the bank ROW ITSELF for playertype-1 slots
  (input.js:135) — record ret = pre-runAI row, physics slot 0 =
  post-runAI row (one object, two read points).
- comparator negative tests (perturb -> count -> restore, all
  re-verified clean after): capture nibble in a history-slot field ->
  exactly 1; artifact DROPPED DRAW -> 1234 (the chain shift cascades
  through every later draw — the brief's requirement bites); artifact
  draw VALUE nibble -> exactly 2 (record-vs-artifact + apply
  bit-verify), chain intact; artifact bank lsX nibble -> 10 (runAI
  row compare + slot-0 physics input divergence + next-frame rule-18
  poll + 8-deep history propagation); never-written field s=B1 ->
  divergences then chain-state HARD ABORT (the injected s drives the C
  keyboard-arm pause machine -> playing toggles -> poll grounding
  marshal-fail: the pause logic is provably live on CPU slots);
  in-page junk write (player[0].__wsTooth inside the runAI window) ->
  3510 wsViol records + capture run exit 1 (finalCheck throw). Spec
  restored byte-identical (diff-verified).
- REGRESSIONS: interpret_inputs.{h,c} were rebased onto the tagged core
  -> full `bash port/sim/calib/check-input-replay.sh` re-run -> INPUT
  MATCH (.loop/task16-regress-input.log; also smoke-replayed the three
  existing captures pre-rerun: 3x 750992 records, 0 divergences).
  Conformance guard `bash port/sim/check-envcoll.sh` -> ENVCOLL MATCH
  (.loop/task16-guard-envcoll.log). No other check links the touched
  TUs (grep-verified; input_canon.{h,c} untouched).
- artifacts (sha256/12): ai_input.h 83c93c386dfc · interpret_inputs.c
  b0ec7b7446fe · interpret_inputs.h ce030d1b6ecd · ai_bridge.h
  e718b152b433 · ai_bridge.c 96dd51b5c18d · spec-ai.js 27c4856c424c ·
  build-ai-bridge.js 73985b74d4d1 · replay_ai.c 3aeff2b7e4ce ·
  check-ai-bridge.sh 3f2193ad7794 · expected-capture-ai.json
  9a2d016bb247. Logs: .loop/task16-donecheck.log,
  .loop/task16-regress-input.log, .loop/task16-guard-envcoll.log,
  .loop/task16-tooth-ws.log.
- honest coverage: undefined bank values are ZERO-LIVE on g07/g08
  (model + marshal + artifact token U support them; CPUTech's
  missing-key path never fired); runAI-skipped SLEEP frames zero-live
  (handled by construction — no record, chain carries); the tagged
  core's gamepad/AI arms remain live-uncovered exactly as they were in
  the plain core (task-3 honest-coverage note carries over; traps
  unchanged); `bk` bookkeeping is recorded but consumed by nothing in
  M2 (it is the M4 ai.js port's future differential surface).
- zoom-out: (1) rule 16's "value models measured on human goldens do
  not transfer" got its CLASS-LEVEL fix: the input plane is now modeled
  as tagged JS values ONCE, with the human bool/double model as a
  verified projection — no second interpretInputs twin to drift (the
  alternative, a structure-parallel duplicate for the AI slot, was
  rejected for exactly that drift class). (2) The write-set recon is an
  INSTRUMENT, not a one-time audit: it runs inside every future
  check-ai-bridge.sh capture, so an upstream-understanding error (or a
  future golden re-record with different AI behavior) fails loudly at
  the capture, not silently in task 17's integration. (3) The
  never-written-field verification generalizes rule 18: when a
  recording carries a plane the recorded actor only PARTIALLY writes,
  pin the measured write set and chain-verify the complement — the
  same discipline as the platforms spec's static-platform chain. (4)
  ai.js's own quirks (curentAction typo, undefined-through-missing-key
  literals) are carried as DATA here, deferring their verbatim-code
  reckoning to the M4 port — the bk record gives that port a ready
  differential.
- next: task 17 — integration: the full C gameTick (flattened
  game-state struct, trace loader, M1 data consumption, mode-3 tick
  order, boot-RNG parity, checksum-stream emission via task 15's
  ml_fmt/ml_ser, headless platform TU), replay ALL 8 goldens
  end-to-end vs the oracle streams; done-check `bash
  port/sim/check-sim.sh` -> SIM CONFORMS — the M2 exit gate.

## iter 36 — 2026-07-16 — M2 task 17: INTEGRATION — the full C gameTick, SIM CONFORMS (M2 exit gate seed)

- done-check: `bash port/sim/check-sim.sh` -> **SIM CONFORMS**, exit 0 —
  ALL 8 goldens (g01–g06 human, g07/g08 CPU via the task-16 AI bridge)
  replayed end-to-end by the headless C sim, judged by the UNCHANGED
  oracle/harness/verify-stream.js against the frozen streams: 3600/3600
  frames exact per golden, rngCalls equal (134/125/119/115/185/160/81/
  1496), rngCallsOutsideStep == 1 everywhere, specVersion 1.
- WHAT LANDED (the composition layer; sixteen bit-verified clusters
  become one simulator):
  - `port/sim/sim/` — sim.h (GameState: the flattened god-module struct),
    sim_boot.c (player.js constructors + start()/harnessSetupMatch/
    startGame verbatim; STAB1->MlStageX; the startingPoint-array .x/.y
    quirk -> frame-1 ECB undef masks), sim_tick.c (main.js:1050-1092
    mode-3 tick in call order + update(i) + matchTimerTick + the
    movingPlatforms MpSim bridge + ALL host seams: mlp_dispatch/
    hd_dispatch -> mv_dispatch, mlp_hd_* -> hit_detection.c bodies,
    mv_article_* -> article.c inits (the task-13 seam-to-body swap),
    mv_hq_push6 -> the live hitQueue, runAI -> ml_ai_bridge_apply with
    the pollInputs bank-row alias re-copy, rule-16 truthiness projection
    of the tagged AI buffers, every *_out_of_domain -> sim_fatal),
    sim_ser.c (per-frame CHECKSUM envelope: ser_player + lifted article
    canon ser -> canon parse -> the fmt_diff §3.1 envelope -> SHA-256;
    sim_frame_envelope = the localization instrument), sim_data.c
    (SIMDATA1 loader + asFlags/hdFlags tables + rule-15 move-data seams
    for all five chars + the registries incl. fox/marth/puff char-module
    registration + composed special-phase lookup + falcon SSG
    canEdgeCancel runtime overlay + the rule-17 LIVE charHitboxes plane),
    sim_main.c (trace loader, boot-RNG parity: 465 boot draws + counter
    reset + the ONE off-step startGame draw; draw counts recovered from
    the mulberry32 state delta via the modular inverse of 0x6D2B79F5 —
    no wrapper on the hot path), trace-to-txt.js / wrap-run.js.
  - `port/sim/calib/dump-sim-data.js` — SIMDATA1 dumper (boot-time page
    dump of asFlags + hdFlags + the mvData union + palettes0; ×2
    byte-identical in the gate; every section byte-equal to the frozen
    captures' frame-0 records — cross-validated).
  - `port/sim/check-sim.sh` — the M2 exit gate (CLAUDE.md §Commands).
  - rule-17 instrument: WEAK-default hooks mv_chd_assign_note /
    mv_chd_write_dmg / mv_chd_write_size in shared moves_index.c (strong
    overrides in sim_data.c own the live plane; every existing replay
    driver keeps its exact per-record-chd behavior — no driver or check
    script changed); puff pf_hb_set_* + marth mv_hb_set_dmg call the
    write hooks; falcon gained mv_falcon_ssg_get_canEdgeCancel.
- LEDGER (integration divergences; M2CAL localization: first divergent
  frame -> oracle --capture-frames envelope byte-diff -> per-stage NaN
  probe -> capture attribution across g08's moves/article/hitdet specs):
  | # | golden/frame | module | cause class | fix | min |
  |---|---|---|---|---|---|
  | 1 | g08 f371 (first divergent hash; surfaced as an AI-bridge chain mismatch at f602 — 4 unconsumed screenShake draws) | hit_detection.c executeRegularHit THROW arm | cross-cluster value-model staleness: the offsetSingle CHARDATA sub-shape (discovered task 8) was never back-propagated into task 6's zero-live throw arm — `hitbox.offset.x` read NaN'd on the thrown hitbox | accept `ML_HB_CHARDATA && offsetSingle` in the single-Vec2D offset read (hit_detection.c:975) | ~45 |
  plus two non-sim infra rows: bash-3.2 `set -u` empty-array expansion
  and ANSI-coloured `node -e console.log(number)` output in check-sim.sh
  (script fixes, no sim surface).
- Re-verification after the fix + hooks: `bash port/sim/check-envcoll.sh`
  -> ENVCOLL MATCH (standing guard); `bash
  port/sim/calib/check-hitdet-replay.sh` -> HITDET MATCH (fresh ×2
  captures, 18344 records, 0 divergences — the touched cluster's own
  done-check); binary replays of the moves-shared/fox/falco/falcon/
  marth/puff and article clusters against their existing frozen captures
  all 0 divergences (weak hooks proven behavior-neutral; their full
  re-record done-checks not re-run this iteration by judgment — the
  hooks are no-ops there by construction and the replays prove it).
- HONEST NOTES / registered items:
  - outOfCameraTimer RESOLVED honestly: the only ++ sites are
    render.js:183/200 (renderPlayer's miniView arm) and the ORACLE ITSELF
    ran with __harnessNoRender=true (renderTick returns before them), so
    the frozen streams' domain has outOfCameraTimer ≡ 0 except the
    explicit =0 writes already in the translated moves/physics. The C
    sim carries the field + physics' outOfCameraUpdate read site
    verbatim; the render-plane increment belongs to the M3/M4 renderer
    (startGame's one renderPlayer call is an =0/=false no-op at spawn).
  - mv_howl_play_id returns a monotone counter: howler's global play-id
    is audio-plane state consumed into player.shieldBreakerID, which is
    NOT on the checksum surface (only .stop(id) reads it back); the M4
    mixer owns real ids.
  - marth's charged shield-breaker (charge >= 30) global-plane write now
    flows through the live chd plane (mv_chd_write_dmg), closing task
    11's documented hole at the class level.
  - shared-move hitbox assigns read the live plane via the assign-note
    overlay; a chd write through an id slot with UNKNOWN provenance and
    CHARDATA shape traps loudly (never silent).
  - ml_events queues are reset per tick stage (observability channels;
    the M4 mixer owns a real per-frame drain).
  - matchTimer <= 0 (finishGame) and physics stage-damage hq rows trap
    (outside the golden domain / M4 target stages).
- ZOOM OUT: the ledger's one sim divergence is an instance of a CLASS —
  "a value-model sub-shape discovered by a LATER cluster is not
  back-propagated into EARLIER clusters' zero-live arms" (offsetSingle:
  task 8 discovery vs task 6 consumer). Instrument applied: grepped every
  shape-conditional offset read across hit_detection.c/article.c/
  physics.c — exactly one instance existed (article.c and physics.c
  already handle offsetSingle/array-index correctly); fixed at the site
  with the class documented in the code comment. Not promoted to a
  numbered rule (first occurrence; rule threshold 3) but recorded here:
  any future value-model widening should grep earlier clusters'
  consumers of that model for stale shape conditionals. Second zoom-out:
  the integration itself validated the M2 method — with 16
  capture-verified clusters, the WHOLE 8-golden composition produced
  exactly ONE divergence, and it lived in a documented zero-live arm.
- next: M2 EXIT GATE is runnable and green (`bash port/sim/check-sim.sh`
  -> SIM CONFORMS). Phase-advance iteration (CHECKER gate run + REPLAN
  M3 concretization) is the loop's next step.


## LOOP STOP: m3-device (2026-07-16, driver)

M2 gate passed (SIM CONFORMS, driver-verified cold). M3 (#18) claimed; its first work (device backend bring-up, OPK, device conformance, perf) requires the FunKey-S on ADB — no device attached at phase entry. Loop stopped per protocol; restart the /loop with the device plugged in and powered on. fix_plan Current phase should flip to M3 at the REPLAN when the loop resumes.

## iter 37 — 2026-07-16 — M3 REPLAN: phase entry, task ladder + exit-gate concretization

MILESTONE PASS: M2 — `bash port/sim/check-sim.sh` -> SIM CONFORMS, exit 0
(all 8 goldens bit-exact through the headless C sim, driver-verified cold
at the phase-advance; issue #17 closed). The iter-36 `m3-device` LOOP STOP
is CLEARED: the FunKey-S is attached and healthy on ADB
(id 12c00003237f5528, `adb devices` -> device; /tmp tmpfs 128 MB, /mnt SD
18 GB free, kernel 4.14.14-funkey armv7l).

- Type: REPLAN (LOOP §C-b) — fix_plan `Current phase:` flipped M2 -> M3;
  §M3 seed items rewritten as 7 dependency-ordered tasks, each with an
  exact runnable done-check; the §Gates M3 *(REPLAN)* cell concretized
  into CLAUDE.md §Commands ("M3 EXIT GATE": `bash
  port/sim/device/verify_m3.sh` — device conformance all 8 + p99 < 16.67
  ms full frame w/ render+audio + OPK frontend launch + live S1 session
  replay, host-judged; human-gate sentinel on pass).
- Ladder: (1) armv7 correctness rung — the FULL sim + fdlibm sweep +
  format differential cross-built static, run ON DEVICE, g01 conformance
  host-judged (validates the entire M2 result on the real CPU before
  anything visual); (2) all-8 device conformance + sim-only p99
  instrumentation; (3) renderer core host-side (ANIM1 + AA scanline
  rasterizer per PLAN §5, structural silhouette check vs the oracle
  canvas, measured-then-frozen threshold); (4) three-backend platform
  seam + SDL1.2 live device render (p99 render ≤ 8 ms, full frame
  < 16.67 ms); (5) S1 input layer at the poll seam + uinput-driven live
  session with three-way replay determinism; (6) audio-on perf (spike
  mixer fed by ml_events, underruns == 0); (7) OPK + frontend launch +
  gate assembly.
- Judgment calls tagged PROVISIONAL (auto-adopted) in fix_plan §M3:
  audio placement (M3 ships the measured spike audio path so the perf
  gate honestly includes the audio callback; full mixer/music stay M4);
  live-session policy (autonomous sessions are uinput-injected through
  the real SDL path; Chase's hands-on S1 ratification stays the phase-end
  human gate).
- Measured this REPLAN (conventions now pinned in fix_plan §M3): this
  adbd does NOT propagate exit codes (`adb shell false` -> host exit 0) —
  all device steps RC-echo checked; /tmp is a 128 MB tmpfs (big sweep
  artifacts go to /mnt/mlfk-scratch); busybox 1.32 + /usr/bin/time
  present.
- No implementation this commit (REPLAN contract). next: task 1 (armv7
  correctness rung — DEVICE CONFORMS g01).


## iter 38 — 2026-07-16 — M3 task 1: armv7 correctness rung — DEVICE CONFORMS g01

- done-check: `bash port/sim/device/check-device-g01.sh` -> **DEVICE
  CONFORMS g01**, exit 0 (clean invocation, 3:58 wall; log
  `.loop/m3-task1-donecheck.log`). All seven rungs green: host data
  plane + references; stamp-cached armv7 static cross-build (SDK gcc
  10.2, every TU `-O2 -ffp-contract=off -Wall -Wextra -Werror -static`);
  ON DEVICE: 257,287-line fdlibm sweep == host byte-exact; 432,319-line
  exact-math family sweep == the HOST-LIBM ANCHOR byte-exact;
  fmt_diff --self-test (28 anchors); the 5,469,538-pattern adversarial
  corpus GENERATED ON DEVICE == the frozen-pin host corpus byte-exact
  and its full format sweep == host byte-exact; the full g01 golden
  replay -> UNCHANGED verify-stream.js: STREAM MATCH 3600/3600 frames,
  rngCalls=134, rngCallsOutsideStep=1, specVersion=1. **Device g01 wall
  clock: 21 s / 3600 frames (~5.8 ms/frame sim-only average,
  informational — p99 instrumentation is task 2).**
- WIP REVIEW (ccebc9b, the 529-storm handoff): adbsh.sh verified against
  live adbd behavior (RC-echo + CR strip correct); check-device-g01.sh
  scaffolding reviewed line-by-line against the task spec — TU list
  matched check-sim.sh exactly, all helper CLIs matched; kept, then
  EXTENDED (mathsweep step [5], renumbered 7 steps). The
  `__attribute__((fallthrough))` envcoll edit verified: gcc 10.2's
  -Wextra implicit-fallthrough level rejects the old prose comment;
  grep swept the whole sim — that case is the ONLY C switch fallthrough
  (other "fall through" hits are JS-`||`/return-shape comments), and the
  -Werror cross-build is the standing class instrument. Stale partial
  cross-build discarded (forced rebuild).
- LEDGER (armv7 divergences; localized per the documented procedure —
  sweep-first ladder meant NO sim frame diff was ever needed):
  | # | surfaced at | cause class | fix | 
  |---|---|---|---|
  | 1 | fdlibm sweep line 25 (sin/cos/tan of DBL_MAX, 2^66, >medium-path args — Payne-Hanek only; 15,640 diff lines) | device libm floor() is IDENTITY for non-integers — the SDK's static musl libc.a math objects were built with unsafe-FP optimization, folding the ±2^52 "toint" trick (floor(1.5)==1.5 measured; -O0 == -O2, semantic not optimizer) | exact floor/ceil vendored into port/fdlibm/fdlibm.c as STRONG symbol overrides (Sun s_floor/s_ceil restated 64-bit; NOTICES extended) — fd__rem_pio2 and all ~86 sim floor sites + js_round's ceil inherit with ZERO call-site churn |
  | 2 | mathsweep fmod rows (fmod(0,0)==1.0 on device; -0 results -> +0) | same libc class: `(x*y)/(x*y)` NaN arm folds to 1.0, `0*x` signed-zero arm folds to +0 | exact fmod vendored as third strong override (Sun e_fmod restated 64-bit; NaN arm returns the canonical qNaN per rule 9) |
  | 3 | device-generated corpus line 58,441 (subnormal anchors ±1 ulp, 000/800-exponent lines) | device libc strtod mis-rounds SUBNORMAL results (same class, floatscan's FP paths) | fmt_diff --gen sections 4/5 made strtod-free via machine-generated bit-pattern anchor tables — corpus BYTE-IDENTICAL, proven by the untouched expected-format.json pin (count+sha256) every run |
  | 4 | device-generated corpus line 4,569,768 (1,653 lines, all -0 -> +0) | device libc strtod drops the SIGN OF ZERO (`sign*y` folded); count matches the "-0e<k>" literal frequency exactly | section 8 computes ±0 without the parser (m==0 arm); its remaining strtod use is normal-range by construction and re-proven by the corpus cmp every run. Also sequenced the previously-unsequenced sm64() snprintf args (latent cross-compiler order hazard; pinned order confirmed by the corpus cmp) |
- Verification chain (all fresh this iteration): host anchor differential
  — mathsweep host-with-fdlibm == host-libm over all 432,319 lines
  (proves the overrides equal a known-good libm, not just themselves);
  device == host anchor over the same corpus; fdlibm crosscheck
  `bash oracle/fdlibm-crosscheck/run.sh` -> CROSSCHECK OK (constants +
  C/JS sweep + browser golden stream three-way, fdlibm.c edit guarded);
  `bash port/sim/calib/check-format.sh` -> FORMAT MATCH (fmt_diff.c edit
  guarded; adversarial pin sha unchanged e3d542b79b19…);
  `bash port/sim/check-sim.sh` -> SIM CONFORMS (host sim now links the
  overrides — all 8 goldens still bit-exact);
  `bash port/sim/check-envcoll.sh` -> ENVCOLL MATCH (fallthrough edit).
  Device hygiene verified: /tmp/mlfk + /mnt/mlfk-scratch absent after
  the run (trap), no leftover processes.
- Logs: `.loop/m3-task1-{hostrefs,armbuild,armbuild2,armbuild3,
  dev-fdlibm,o0-discriminator,dev-fix-verify,dev-fix-verify2,dev-fmt,
  dev-fmt2,dev-fmt3,dev-g01,donecheck,reg-sim,reg-envcoll,reg-format,
  reg-fdlibm}.log`.
- ZOOM OUT (rule 8): the four ledger rows are ONE class — "the device
  toolchain's libc math/parse symbols are untrusted: its libc.a was
  built with unsafe-FP flags, so every algebraic-identity code path
  (toint rounding, (x*y)/(x*y), 0*x, sign*y) is folded away". Fixed at
  class level, not per-symptom: (a) LINK-level strong overrides in the
  one TU every sim/oracle binary already links (fdlibm.c) — no call-site
  edits, qjs-oracle device builds inherit automatically; (b) the
  generator's parser dependence REMOVED where the breakage lives,
  guarded by the frozen pin; (c) a STANDING INSTRUMENT (mathsweep, in
  the done-check forever) that differentially sweeps the whole reachable
  non-transcendental libm surface against a host anchor every run —
  instrument > class fix > one-off, all three rungs applied. Rule
  candidate for future device tasks (recorded in fix_plan §M3 task-1
  DONE note): trust NO device-libc math symbol without a differential
  sweep; sqrt/fabs pass only because they compile to VFP instructions.
- HONEST NOTES: round/trunc are also broken on device (round loses -0
  and rounds nothing) but have ZERO call sites in the sim — not
  overridden, and mathsweep deliberately sweeps only the reachable
  surface {floor, ceil, sqrt, fabs, fmod, js_round}; any future use
  must extend the overrides + sweep. Host mathsweep-with-fdlibm
  comparison may partially inline-fold on clang (frintm/frintp) — the
  binding-level proof is the DEVICE leg, where gcc emits real calls.
  fmt_diff --self-test runs on-device by design (its judgment is the
  28 pinned anchors compiled in; the 5.47M differential is host-judged).
  The 21 s device wall clock includes trace/simdata parse + stream
  write to tmpfs; per-frame p99 timing (the M3 perf gate's real
  currency) lands with task 2's --timing flag.
- next: task 2 — all-8 device conformance + sim-only p99 timing
  (`check-device-conform.sh`). Note for task 2: AI-bridge artifacts for
  g07/g08 must ride to the device; reuse adbsh.sh + the stamp-cached
  build; big pulls run ~4.4 MB/s (58 s for the 238 MB format output —
  budget pull time, not compute).
