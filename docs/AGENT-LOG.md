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
