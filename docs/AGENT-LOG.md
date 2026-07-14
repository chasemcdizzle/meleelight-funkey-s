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
