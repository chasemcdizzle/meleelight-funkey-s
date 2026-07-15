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
