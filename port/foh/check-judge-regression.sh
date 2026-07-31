#!/usr/bin/env bash
# check-judge-regression.sh — OLD-vs-NEW byte-identity regression for the two
# JUDGE-PATH producers this arc modified: judge-foh-trace.js and
# normalize-foh-trace.js.
#
# WHY THIS EXISTS (PROCESS §3 Tier A+, driver ruling 2026-07-29): changing a
# JUDGE is not like changing a feature. A judge that silently got LOOSER stops
# rejecting corruption it used to catch, and every downstream green becomes
# meaningless — the checks keep printing OK while proving less. So a judge
# change owes two things beyond the normal arc: a review by a DIFFERENT
# reviewer, and this — an archived regression showing the judge's verdicts did
# NOT move on inputs it already knew how to judge.
#
# WHAT IS ACTUALLY CLAIMED, and why a blanket claim would be a lie. The first
# draft of this check asserted "the new judge agrees with the old on every
# pre-arc input". MEASURED, that is FALSE — and the measurement is the useful
# part, so it is recorded here rather than smoothed away. This arc changed the
# FLOW GRAPH (owner ruling C5: at FOH_NETPLAY 0 `VS. Melee` goes straight to
# the CSS, so `menu-top>menu-battle` is now OFF-GRAPH), which is exactly why
# five `.expect` files were re-frozen in the same change. Those five pre-arc
# traces therefore MUST be rejected by the new judge — that is the change
# working, not the judge breaking.
#
# So the claim is split into legs, each falsifiable on its own. (The numbering
# below is the CLAIM's; the printed `[1]`..`[5]` are the RUN's steps and do not
# correspond one-to-one — grok Tier A+ NIT.)
#
#   [1] UNCHANGED-FLOW IDENTITY. For the flows this arc did NOT touch, the new
#       judge is BYTE-IDENTICAL to the old — stdout, stderr and exit code, on
#       both argv flags, over accepts AND over five corruption classes. If the
#       judge silently got looser, this is where it shows.
#
#   [2] ENUMERATED MOVEMENT. Every flow whose verdict DID move is listed in
#       MOVED[] below with its old->new verdict and its reason. A flow that
#       moves without an entry FAILS; an entry that does NOT move also FAILS
#       (dead-entry guard). So the set of intended changes is frozen, and the
#       next judge edit cannot quietly add to it.
#
#   [3] NEW-EXPECT ACCEPTANCE. For each moved flow, the new judge ACCEPTS the
#       arc's re-frozen `.expect` at its correct launch flag and REJECTS it at
#       the other. This distinguishes "the graph changed" from "the judge
#       rejects everything" — it does NOT by itself prove absence of loosening
#       (it is the same author's re-frozen expect), which is what leg [4] is
#       for. Claim narrowed after the Tier A+ reviewer called the original
#       wording circular.
#
#   [4] NEW-GRAMMAR ENFORCEMENT (the negative corpus). Legs 1-3 exercise only
#       forms that already existed. This leg perturbs a re-frozen expect that
#       CARRIES the new surface and requires the new judge to reject each
#       violation AT THAT RULE'S OWN DIAGNOSTIC, plus a positive control so a
#       reject-everything judge cannot satisfy it. Both reviewers found the
#       hole this closes, and it is verified closed by re-running their own
#       loosening experiment (widen a domain / a per-screen field -> this
#       check FAILS). What this leg is NOT is the coverage instrument: a
#       fixture exists only for a rule someone thought of, and the round-3
#       reviewer walked through four bypasses planted INSIDE the judging loop
#       that every fixture missed. NOTIFICATION of every change to every rule
#       — including the ones nobody fixtured and the ones that do not exist
#       yet — is [0g], which freezes the tables, the enforcement loop and the
#       whole file.
#
#       NOTIFICATION IS NOT CORRECTNESS, and saying otherwise here was the
#       round-3b MAJOR. [0g] reports that a rule MOVED; it has no opinion on
#       whether where it moved to is right, and its own documented remedy for
#       a detected move is to RE-FREEZE. The rung that can say a domain is
#       WRONG is [0n] below.
#
#   [4b] AUTHORED AUTHORITY + BOUNDARY PROBES ([0n]). The claim neither [0g]
#       nor [5b] can make: that the judge's domains agree with something
#       OUTSIDE the judge. judge-domains.authored.txt is hand-authored, one
#       upstream citation per row, so a widening must ALSO falsify a citation
#       a reviewer can check — a re-freeze cannot launder that. The boundary
#       probes are generated FROM that file and driven through the real judge,
#       so the claim is behavioral for all 17 S fields, not just the eight
#       that [5b] happens to fixture.
#
# THE BASELINE IS A PINNED COMMIT, NOT `HEAD` — and that is load-bearing.
# While this lane is unmerged, HEAD happens to hold the pre-arc judge, so
# `HEAD` would work today and then SILENTLY DIE the moment the driver merges:
# HEAD would become the NEW judge, old and new would be identical, and the
# dead-regression guard would fail forever. Pinning the commit keeps this
# check meaningful after the merge, which is when it actually starts earning
# its keep. Override with MLFK_JUDGE_BASE_REF to re-baseline after a FUTURE
# judge change (and re-measure the two tables below when you do).
#
# BOTH DIRECTIONS ARE COVERED, which is the part that is easy to skip: a
# regression that only feeds VALID traces proves the judge still ACCEPTS what
# it accepted, and says nothing about whether it still REJECTS. Corruption
# fixtures (leg 2) are generated from the same corpus and must produce
# byte-identical REJECTIONS too.
#
# Usage: bash port/foh/check-judge-regression.sh
# Prints `JUDGE REGRESSION OK (...)`, exit 0. Any divergence -> nonzero.
set -euo pipefail

FOH=port/foh
B=$FOH/build/judgereg
fail() { echo "JUDGE REGRESSION FAIL: $1" >&2; exit 1; }
grammar_die() { echo "JUDGE REGRESSION FAIL: $1" >&2; exit 2; }

command -v node >/dev/null || fail "node not on PATH"
git rev-parse --git-dir >/dev/null 2>&1 || fail "not a git tree (the OLD side comes from a pinned commit)"
# Pre-arc baseline: the commit whose judge/normalizer predate this arc.
BASE_REF="${MLFK_JUDGE_BASE_REF:-221510a17074bc34eabb30f93c2857cc792ffee5}"
git rev-parse --verify -q "$BASE_REF^{commit}" >/dev/null \
  || fail "baseline commit $BASE_REF not found (set MLFK_JUDGE_BASE_REF)"

rm -rf "$B"; mkdir -p "$B/old" "$B/corpus" "$B/out"

# --- [0] the OLD pair, straight out of the commit ---------------------------
# $BASE_REF holds the pre-arc pair (see the pin note in the header).
for f in judge-foh-trace.js normalize-foh-trace.js; do
  git show "$BASE_REF:$FOH/$f" > "$B/old/$f" 2>/dev/null \
    || fail "cannot extract $BASE_REF:$FOH/$f (is the file tracked?)"
  [ -s "$B/old/$f" ] || fail "$BASE_REF:$FOH/$f extracted empty"
done
# DEAD-REGRESSION GUARD: if old and new are byte-identical this whole check is
# a no-op that would pass forever. Fail loudly rather than report a hollow OK.
changed=0
for f in judge-foh-trace.js normalize-foh-trace.js; do
  if ! cmp -s "$B/old/$f" "$FOH/$f"; then changed=$((changed + 1)); fi
done
[ "$changed" -gt 0 ] \
  || fail "old and new judges are byte-identical — this regression proves nothing (if the arc no longer changes the judge path, delete this check rather than let it pass hollow)"

# --- [0g] FROZEN DECISION TABLES (the class fix) ----------------------------
# Per-rule negative fixtures ([5b]) catch the loosenings someone thought to
# write a fixture for. The Tier A+ reviewer proved that is not enough by
# STACKING five widenings nobody had guarded — a dropped REFUSED screen
# binding, a widened SFIELD_SCREENS entry, a widened LAUNCH field domain, a
# spurious EDGES entry, and loosened normalizer domains — and watching every
# printed leg stay green.
#
# So the tables themselves are FROZEN: any widening of any of them fails here
# whether or not a fixture exists for it. This is the instrument rung of HARD
# RULE 8 rather than N one-off fixtures, and it covers rules that do not exist
# yet. A deliberate grammar change re-freezes judge-grammar.frozen.txt in the
# same commit — the same discipline every other frozen artifact carries.
#
# AND THE DUMPER ITSELF IS PINNED (Tier A+ round 3 MAJOR). [0g] is only as
# honest as the program that produces `grammar.now`: a one-line poison in
# dump-judge-grammar.js that always prints the frozen bytes makes every hash
# below "match" while the real tables widen. The pin is computed HERE, by
# shasum, and NOT by the dumper — a dumper cannot be its own witness. Editing
# the dumper is therefore exactly as visible as re-freezing the tables, which
# is the point: they are the same act.
#
# RE-FREEZE LOG (the "say why" this leg demands, newest last):
#   Tier A+ round-6 MINOR-1 — judge-foh-trace.js's launch-count check became
#   `launches !== (wantLaunch ? 1 : 0)` (was `wantLaunch !== (launches === 1)`).
#   This is a TIGHTENING, and the dump proves it: the ONLY lines that moved are
#   judge-foh-trace.js's own ENFORCE_REGION and FILE hashes — not one named
#   decision table (EDGES, REFUSED, SVAL_DOM, SFIELD_SCREENS, or any line-form
#   regex) changed, so no accepted-string set widened. The old form accepted
#   launches=2 under wantLaunch=0 whenever the "more than one LAUNCH" rule was
#   absent; the new form accepts only 0. Leg [0n]'s authored probes re-ran
#   green against the tightened judge, which is the witness that matters here.
echo "  [0g] frozen judge/normalizer decision tables"
FROZEN="$FOH/judge-grammar.frozen.txt"
[ -f "$FROZEN" ] || fail "missing $FROZEN (the frozen decision-table pin)"
# foh.h IS A LIVE INPUT TO THE DECISION PLANE (Tier A+ round-3 NIT). The judge
# reads `#define FOH_NETPLAY` out of foh.h at load and SWITCHES which EDGES and
# REFUSED entries exist. Flipping that one digit therefore moves the whole
# decision plane while every hash below stays byte-identical, because the dump
# carries both profile blocks as source text. The frozen tables were frozen
# under profile 0, so profile 0 is pinned here; changing the profile re-freezes
# the tables and re-pins this line in the SAME change.
netdef="$(grep -E '^[[:space:]]*#[[:space:]]*define[[:space:]]+FOH_NETPLAY' "$FOH/foh.h" || true)"
[ "$netdef" = "#define FOH_NETPLAY 0" ] \
  || grammar_die "foh.h gives the judge's build profile as '$netdef', pinned '#define FOH_NETPLAY 0' — the tables below were frozen under profile 0, and a different (or duplicated, or reformatted) profile swaps EDGES/REFUSED entries in and out without moving a single hash."
DUMPER_SHA=5ccd56ba377d51b8fc140e6821e20ad59d58efd9821a4f6513cfa852ec3ddfc2
dsha="$(shasum -a 256 "$FOH/dump-judge-grammar.js" | cut -d' ' -f1)"
[ "$dsha" = "$DUMPER_SHA" ] || grammar_die "dump-judge-grammar.js is $dsha, pinned $DUMPER_SHA — the extractor that produces every hash below changed. A poisoned/edited dumper can make the frozen tables match while the real grammar widens, so re-pin DUMPER_SHA in this script in the SAME change and say why."
node "$FOH/dump-judge-grammar.js" "$FOH/judge-foh-trace.js" \
  "$FOH/normalize-foh-trace.js" > "$B/grammar.now" 2> "$B/grammar.err" \
  || { cat "$B/grammar.err" >&2; fail "grammar dump failed"; }
# The dump must be non-trivial, or freezing it proves nothing.
gtab="$(tail -n 1 "$B/grammar.now" | sed -n 's/^TABLES \([0-9]*\)$/\1/p')"
[ -n "$gtab" ] && [ "$gtab" -ge 26 ] \
  || grammar_die "grammar dump trailer is '$(tail -n 1 "$B/grammar.now")', want 'TABLES <n>' with n >= 26 (22 named tables + per-file DECISION_REGION/ENFORCE_REGION/FILE — the extractor stopped finding them and would freeze nothing)"
cmp -s "$B/grammar.now" "$FROZEN" || {
  diff "$FROZEN" "$B/grammar.now" | head -12 >&2
  fail "the judge/normalizer DECISION TABLES moved (diff above). Every widening of EDGES / REFUSED / SVAL_DOM / SFIELD_SCREENS / any line-form regex lands here, including ones no negative fixture covers. If the change is deliberate, re-freeze $FROZEN in the SAME change and say why."
}
echo "      $gtab tables byte-identical to the frozen pin"

# ANCHORED FULL-OUTPUT DIAGNOSTIC ASSERTION (review-r14 MAJOR).
# The old form was `grep -qF "$phrase" <stderr>` — an UNANCHORED SUBSTRING on
# ONE stream. Stderr carrying the expected phrase PLUS an unrelated error
# satisfied it, so "this fixture died at the rule it targets" was not actually
# proven. This asserts EVERY byte the judge emitted:
#   - stdout is EMPTY (a rejecting judge prints nothing there — measured);
#   - stderr is EXACTLY ONE line (kills the "phrase + unrelated error" shape);
#   - that line is ANCHORED to the judge's diagnostic prefix;
#   - and it carries the human-declared rule phrase, so the identity of the
#     rule stays a typed assertion and is never laundered from output.
# The line's tail (line number + offending text) is covered by DIAG_SHA below
# rather than by 31 typed literals: one deliberate re-freeze on drift, the
# same idiom leg [0g] already uses for the decision tables, instead of a
# per-row pin that churns whenever a fixture's line number moves.
DIAGACC="$B/diag.acc"
: > "$DIAGACC"
assert_one_diag_core() { # <label> <out> <err> <declared-rule-phrase> <anchor-prefix>
  local lbl="$1" of="$2" ef="$3" need="$4" pfx="$5" n
  [ -s "$of" ]     && fail "[$lbl] the judge wrote to STDOUT while rejecting ($(wc -c < "$of") bytes) — a rejecting judge emits its diagnostic on stderr and nothing on stdout; unexpected stdout means this is not the failure path it claims to be"
  n="$(wc -l < "$ef" | tr -d ' ')"
  [ "$n" = 1 ]     || fail "[$lbl] expected EXACTLY ONE stderr diagnostic line, got $n — extra output means the fixture may be dying at more than one rule, so a substring match on it proves nothing (got: $(head -c 300 "$ef"))"
  grep -q -- "^$pfx" "$ef"     || fail "[$lbl] the single stderr line is not an anchored '$pfx' diagnostic (got: $(head -c 300 "$ef"))"
  grep -qF -- "$need" "$ef"     || fail "[$lbl] rejected, but NOT by the rule it targets ('$need') — it died for an unrelated reason, so it proves nothing about that rule (got: $(head -c 300 "$ef"))"
}
# The judge-facing wrapper: same four assertions, plus the accumulator whose
# tail is pinned by DIAG_SHA below. Leg [0n] calls the CORE directly (its ~700
# generated probes are pinned by count and by the authored table, not by a
# frozen transcript) and passes the normalizer's prefix for its own rung.
assert_one_diag() { # <label> <outfile> <errfile> <declared-rule-phrase>
  assert_one_diag_core "$1" "$2" "$3" "$4" "judge-foh-trace: CORRUPT: "
  printf '%s\t%s\n' "$1" "$(cat "$3")" >> "$DIAGACC"
}

# --- [0n] AUTHORED DECISION AUTHORITY + BEHAVIORAL PROBES -------------------
# Tier A+ round-3b BLOCKER, widened by round-16 / Tier A+ round-4 to ALL of
# the judge's decision surfaces. The honest limit of leg [0g], stated plainly:
# a hash pin detects CHANGE, never LOOSENING. `judge-grammar.frozen.txt` is
# produced by running the judge under review through a dumper, so the artifact
# and its witness have the same author, and the documented remedy for a
# detected change is "re-freeze and say why" — which is exactly what a
# widening does.
#
# THE SAME IS TRUE OF THE OTHER PIN ON THIS FILE (Tier A+ round-6 MINOR-2).
# check-foh-flows.sh leg [0a] pins port/foh/judge-foh-trace.js by sha256 under
# the same binding "update the pin in the same commit" discipline. That is a
# change detector, not a loosening detector: re-pinning it is a one-line step
# in any exploit that widens the judge, and round 6 took exactly that step.
# Naming it here so the limit is stated once, in full: NEITHER hash pin is a
# witness. The authored authority below is the only thing that disagrees with
# the judge from outside it.
#
# WHY EVERY SURFACE, NOT JUST `S` (the round-16 finding, both reviewers).
# v1 of this leg authored the S value plane only. Both reviewers then showed,
# independently and with working exploits, that the identical defect survived
# everywhere else: the LAUNCH/TLAUNCH field domains, the EDGES set, the
# REFUSED map, and the normalizer's independent copy of the grammar were each
# witnessed by nothing but their own hash. The second reviewer stacked five
# widenings plus the check's own documented re-freeze remedy and left BOTH
# gates green while launching `difficulty=7 stage=8`; codex named the same
# class through a different pair (the normalizer's NUM -> `[0-9]+`, its
# LAUNCH `turbo` -> `[012]`). Authoring one plane and leaving four is not a
# fix, it is a smaller instance of the same class (HARD RULE 8).
#
# port/foh/judge-domains.authored.txt is the outside disagreement: 89
# HAND-AUTHORED rows (17 S, 16 L, 26 E, 8 R, 2 N, 20 X), each a claim about upstream
# or a registered MENU-SPEC deviation, each with a citation a reviewer can
# check and that a widening would have to falsify.
#
# Two rungs, deliberately different in kind from [0g]:
#  (a) SEMANTIC, not source-text. The live tables are EVALUATED out of the
#      judge (SVAL_DOM, SFIELD_SCREENS, and the EDGES/REFUSED construction
#      region including both profile blocks); the LAUNCH/TLAUNCH character
#      classes of BOTH programs and the normalizer's own per-field S value
#      classes are EXPANDED to integer sets. All are then compared
#      value-by-value to the authored rows, EXACT equality in both
#      directions. A comment reflow moves [0g]'s hash and not this; a value
#      widening moves both. The normalizer spells its S domains inside RE_S,
#      so that fifth copy is compared here too rather than trusted.
#  (b) BEHAVIORAL, over the DOMAIN x SCREEN CROSS-PRODUCT (codex BLOCKER 2).
#      v1 probed each field only on its own home screen, so per-screen
#      authorization was asserted for one screen per field and inferred for
#      the rest. Every S field is now probed at its boundaries on EVERY
#      screen the committed flows reach, and the value alphabet itself is
#      probed from a FIXED wider universe (-2..12 plus malformed forms)
#      rather than from the judge's own alphabet — so widening RE_S_NUM is
#      caught by the universe instead of silently widening the sweep that was
#      supposed to catch it. Unauthored edges and mis-screened refusals are
#      probed the same way, so the sets compared in (a) are proven to be the
#      sets actually ENFORCED.
#
# BOTH PROGRAMS. Every probe that is a pure line-form or value-domain claim
# is driven through the judge AND the device-side normalizer, so the two
# independent copies cannot drift apart with only their own hashes as
# witnesses. Screen/edge/refusal-binding probes are judge-only: the
# normalizer has no screen model, which is itself an authored fact.
#
# Every rejection is validated over the program's WHOLE output (codex MAJOR):
# empty stdout, exactly one anchored diagnostic line, and that line carrying
# the DECLARED wording of the rule being targeted — never an unanchored
# substring on one stream, and never a phrase laundered from the output.
echo "  [0n] authored decision authority (5 surfaces) + behavioral probes"
AUTHDOM="$FOH/judge-domains.authored.txt"
[ -f "$AUTHDOM" ] || fail "missing $AUTHDOM (the authored decision authority — without it leg [0n] has nothing to disagree with the judge)"
mkdir -p "$B/dom"
cat > "$B/dom/gen.js" <<'GENEOF'
"use strict";
// Generated by check-judge-regression.sh leg [0n]. Reads the AUTHORED
// decision authority, evaluates the judge's LIVE tables and both programs'
// value classes, asserts they agree, and writes the behavioral probe corpus
// derived FROM THE AUTHORED ROWS.
const fs = require("fs");
const [, , AUTH, JUDGE, NORM, HDRP, OUT, FLOWS] = process.argv;
function die(m) { console.error("domain-authority: " + m); process.exit(3); }

// --- the authored rows ------------------------------------------------------
const S = [], L = [], E = [], R = [], N = [], X = [];
for (const raw of fs.readFileSync(AUTH, "utf8").split("\n")) {
  const ln = raw.trim();
  if (!ln || ln[0] === "#") continue;
  let m;
  if ((m = /^S ([a-z0-9]+) (-?[0-9]+) (-?[0-9]+) ([a-z,-]+) (\S.*)$/.exec(ln))) {
    S.push({ f: m[1], lo: +m[2], hi: +m[3], scr: m[4].split(","), cite: m[5] });
    if (+m[2] > +m[3]) die("authored S row '" + m[1] + "' has lo > hi");
  } else if ((m = /^L (launch|tlaunch) ([a-z0-9]+) (-?[0-9]+) (-?[0-9]+) (\S.*)$/.exec(ln))) {
    L.push({ line: m[1], f: m[2], lo: +m[3], hi: +m[4], cite: m[5] });
    if (+m[3] > +m[4]) die("authored L row '" + m[2] + "' has lo > hi");
  } else if ((m = /^E (any|net|nonet) ([a-z-]+) ([a-z-]+) ([a-z]+) (\S.*)$/.exec(ln))) {
    E.push({ prof: m[1], from: m[2], to: m[3], cause: m[4], cite: m[5] });
  } else if ((m = /^R (any|net|nonet) ([a-z0-9]+) ([a-z,-]+) (\S.*)$/.exec(ln))) {
    R.push({ prof: m[1], tok: m[2], scr: m[3].split(","), cite: m[4] });
  } else if ((m = /^N ([a-z]+) (\S+) (\S.*)$/.exec(ln))) {
    N.push({ rule: m[1], val: m[2], cite: m[3] });
  } else if ((m = /^X ([a-z]+) (own|preempt) (\S.*)$/.exec(ln))) {
    X.push({ id: m[1], mode: m[2], cite: m[3] });
  } else {
    die("unparseable authored row: '" + ln + "'");
  }
}
const WANT = { S: 17, L: 16, E: 26, R: 8, N: 2, X: 20 };
for (const [k, arr] of [["S", S], ["L", L], ["E", E], ["R", R], ["N", N], ["X", X]]) {
  if (arr.length !== WANT[k])
    die("the authored table has " + arr.length + " " + k + " rows, want " +
        WANT[k] + " — a decision surface gained or lost an entry. Move the " +
        "row WITH its citation and re-pin this count in the SAME change.");
}

// --- the build profile, parsed exactly as the judge parses it ---------------
const HDR = fs.readFileSync(HDRP, "utf8");
const NET_DEFS = HDR.match(/^[ \t]*#[ \t]*define[ \t]+FOH_NETPLAY\b.*$/mg) || [];
if (NET_DEFS.length !== 1)
  die("foh.h carries " + NET_DEFS.length + " FOH_NETPLAY definitions, want exactly 1");
const M_NET = /^#define FOH_NETPLAY ([01])$/.exec(NET_DEFS[0]);
if (!M_NET) die("the FOH_NETPLAY definition is not exactly '#define FOH_NETPLAY [01]'");
const NETPLAY = M_NET[1] === "1";
const liveProf = p => p === "any" || p === (NETPLAY ? "net" : "nonet");

const jsrc = fs.readFileSync(JUDGE, "utf8");
const nsrc = fs.readFileSync(NORM, "utf8");

// --- (a1) live SVAL_DOM / SFIELD_SCREENS, EVALUATED -------------------------
function lit(name) {
  const start = jsrc.indexOf("const " + name + " = {");
  if (start < 0) die("live table " + name + " not found in the judge source — leg [0n] cannot compare what it cannot find");
  const open = jsrc.indexOf("{", start);
  const end = jsrc.indexOf("\n};", open);
  if (end < 0) die("live table " + name + " has no terminating '\\n};'");
  let v;
  try { v = new Function("return (" + jsrc.slice(open, end + 2) + ")")(); }
  catch (e) { die("live table " + name + " did not evaluate: " + e.message); }
  if (!v || typeof v !== "object") die(name + " did not evaluate to an object");
  return v;
}
const dom = lit("SVAL_DOM"), scrn = lit("SFIELD_SCREENS");
const ak = S.map(r => r.f).slice().sort().join(",");
for (const [nm, tab] of [["SVAL_DOM", dom], ["SFIELD_SCREENS", scrn]]) {
  const lk = Object.keys(tab).slice().sort().join(",");
  if (lk !== ak)
    die("the judge's " + nm + " FIELD SET disagrees with the authored authority.\n" +
        "  judge:    " + lk + "\n  authored: " + ak +
        "\nA field entered or left the S surface. Move the authored row in the SAME change, with its upstream citation.");
}
for (const r of S) {
  const d = dom[r.f];
  if (!Array.isArray(d) || d.length !== 2) die("SVAL_DOM." + r.f + " is not a [lo,hi] pair");
  if (d[0] !== r.lo || d[1] !== r.hi)
    die("DOMAIN DISAGREEMENT on S '" + r.f + "': the judge permits [" + d[0] + "," + d[1] +
        "], the authored authority says [" + r.lo + "," + r.hi + "].\n" +
        "  authored citation: " + r.cite + "\n" +
        "This is the loosening leg [0g] cannot see: re-freezing the grammar dump only makes the judge agree with ITSELF again. To move a domain, move the authored row and cite the upstream change that makes it true.");
  const s = scrn[r.f];
  if (!Array.isArray(s)) die("SFIELD_SCREENS." + r.f + " is not an array");
  if (s.slice().sort().join(",") !== r.scr.slice().sort().join(","))
    die("SCREEN DISAGREEMENT on S '" + r.f + "': the judge permits [" + s.join(",") +
        "], the authored authority says [" + r.scr.join(",") + "].\n" +
        "  authored citation: " + r.cite +
        "\nA screen gaining the right to write a field is a machine change; cite it.");
}

// --- source-text helpers ----------------------------------------------------
function reSrc(source, who, name) {
  const i = source.indexOf("const " + name + " ");
  if (i < 0) die(who + " has no " + name);
  const j = source.indexOf(";\n", i);
  if (j < 0) die(who + "'s " + name + " has no terminator");
  return source.slice(i, j);
}
function strCat(text) { // concatenate the JS string literals of an expression
  const parts = text.match(/"([^"\\]*)"/g);
  if (!parts) return "";
  return parts.map(s => s.slice(1, -1)).join("");
}
function expandClass(spec, who, what) {
  const out = new Set();
  for (let i = 0; i < spec.length; i++) {
    if (spec[i + 1] === "-" && i + 2 < spec.length) {
      for (let c = spec.charCodeAt(i); c <= spec.charCodeAt(i + 2); c++)
        out.add(String.fromCharCode(c));
      i += 2;
    } else out.add(spec[i]);
  }
  if (out.size === 0) die(who + "'s " + what + " character class is empty");
  return out;
}
function expandVals(spec, who, what) { // "[0-4]" | "(?:-1|[01])" | "(?:10|[0-9])"
  let body = spec.trim();
  const g = /^\(\?:(.*)\)$/.exec(body);
  if (g) body = g[1];
  const out = new Set();
  for (const alt of body.split("|")) {
    const cm = /^\[([^\]]+)\]$/.exec(alt);
    if (cm) for (const c of expandClass(cm[1], who, what)) out.add(c);
    else if (/^-?[0-9]+$/.test(alt)) out.add(alt);
    else die(who + "'s " + what + " value spec has an alternative leg [0n] cannot expand: '" + alt + "' (in '" + spec + "')");
  }
  return out;
}
function wantSet(lo, hi) {
  const a = [];
  for (let v = lo; v <= hi; v++) a.push(String(v));
  return a.sort().join(",");
}

// --- (a2) the normalizer's OWN per-field S value classes --------------------
// review-r3 made this copy spell every field's real domain so "the two copies
// can only disagree loudly". That claim was witnessed by the copy itself; it
// is witnessed by the authored table here.
{
  const txt = strCat(reSrc(nsrc, "the normalizer", "RE_S"));
  const alt = /(?:\(\?:([a-z0-9|[\]-]+)\)|([a-z0-9]+)) (\[[^\]]+\]|\(\?:[^)]+\))/g;
  const nd = {};
  let m;
  while ((m = alt.exec(txt)) !== null) {
    const fields = (m[1] || m[2]).split("|");
    for (const raw of fields) {
      const ex = /^([a-z]+)\[([0-9])-([0-9])\]$/.exec(raw);
      const names = [];
      if (ex) { for (let i = +ex[2]; i <= +ex[3]; i++) names.push(ex[1] + i); }
      else names.push(raw);
      for (const nm of names) {
        if (nm === "refused") continue; // the refusal surface, checked below
        if (nd[nm]) die("the normalizer's RE_S binds '" + nm + "' twice");
        nd[nm] = m[3];
      }
    }
  }
  const nk = Object.keys(nd).slice().sort().join(",");
  if (nk !== ak)
    die("the NORMALIZER's RE_S field set disagrees with the authored authority.\n" +
        "  normalizer: " + nk + "\n  authored:   " + ak +
        "\nThe device-side copy governs what the FOH may emit on the device. It moves WITH the authored row or not at all.");
  for (const r of S) {
    const got = [...expandVals(nd[r.f], "the normalizer", r.f)].sort().join(",");
    const want = wantSet(r.lo, r.hi);
    if (got !== want)
      die("DOMAIN DISAGREEMENT in the NORMALIZER for S '" + r.f + "': it accepts {" +
          got + "}, the authored authority says {" + want + "}.\n" +
          "  authored citation: " + r.cite +
          "\nThis copy runs on the DEVICE. A widening here is a widening of what the shipped FOH may record, with only its own hash as witness.");
  }
}

// --- (a3) live EDGES / REFUSED, EVALUATED including the profile blocks ------
// The region is sliced from the judge's own source and executed with NETPLAY
// bound to the header's value, so BOTH profile blocks are real code here and
// the one in force is decided by foh.h, never by what a trace contains.
const eStart = jsrc.indexOf("const EDGES = new Set([");
if (eStart < 0) die("the judge's EDGES set was not found");
const rMark = jsrc.indexOf('REFUSED.set(t, ["menu-battle"])', eStart);
if (rMark < 0) die("the judge's netplay REFUSED block was not found — the EDGES/REFUSED region cannot be delimited");
const eEnd = jsrc.indexOf("\n}\n", rMark);
if (eEnd < 0) die("the judge's netplay REFUSED block has no closing brace");
let live;
try {
  live = new Function("NETPLAY",
    jsrc.slice(eStart, eEnd + 3) + "\nreturn { EDGES: EDGES, REFUSED: REFUSED };")(NETPLAY);
} catch (e) { die("the judge's EDGES/REFUSED region did not evaluate: " + e.message); }
if (!(live.EDGES instanceof Set)) die("the judge's EDGES did not evaluate to a Set");
if (!(live.REFUSED instanceof Map)) die("the judge's REFUSED did not evaluate to a Map");

const authEdges = E.filter(r => liveProf(r.prof))
  .map(r => r.from + ">" + r.to + ">" + r.cause).sort();
const liveEdges = [...live.EDGES].slice().sort();
if (authEdges.join("\n") !== liveEdges.join("\n")) {
  const extra = liveEdges.filter(x => authEdges.indexOf(x) === -1);
  const missing = authEdges.filter(x => liveEdges.indexOf(x) === -1);
  die("EDGE SET DISAGREEMENT at FOH_NETPLAY=" + (NETPLAY ? "1" : "0") + ".\n" +
      "  in the judge but NOT authored: " + (extra.join(" ") || "(none)") + "\n" +
      "  authored but NOT in the judge: " + (missing.join(" ") || "(none)") + "\n" +
      "An unauthored edge is a new path through the FOH. Add the row with the upstream (or registered-deviation) citation that makes it legal, in the SAME change.");
}
const authRef = R.filter(r => liveProf(r.prof))
  .map(r => r.tok + "=" + r.scr.slice().sort().join("|")).sort();
const liveRef = [...live.REFUSED.entries()]
  .map(([k, v]) => k + "=" + v.slice().sort().join("|")).sort();
if (authRef.join("\n") !== liveRef.join("\n")) {
  const extra = liveRef.filter(x => authRef.indexOf(x) === -1);
  const missing = authRef.filter(x => liveRef.indexOf(x) === -1);
  die("REFUSAL MAP DISAGREEMENT at FOH_NETPLAY=" + (NETPLAY ? "1" : "0") + ".\n" +
      "  in the judge but NOT authored: " + (extra.join(" ") || "(none)") + "\n" +
      "  authored but NOT in the judge: " + (missing.join(" ") || "(none)") + "\n" +
      "A refusal token is a promise that a visible affordance does nothing, bound to the screen that may emit it. Cite it.");
}

// --- (a4) LAUNCH / TLAUNCH character classes, in BOTH programs --------------
// The field expression is read out of each program's OWN regex source, so it
// may only be parsed with a grammar this reader actually MODELS. An
// alternation, a quantifier, an escape, a negation or a multi-digit literal
// is a construct whose accepted set this function would silently mis-report:
// measured (Tier A+ round 5), `difficulty=([1-4]|[5-9])` read back as
// {1,2,3,4} and the widened judge stayed green while launching difficulty 7.
// An unmodelled construct is therefore a HARD FAIL, never a guess.
function fieldExpr(text, who, field) {
  const at = text.indexOf(field + "=");
  if (at < 0) die(who + " has no '" + field + "=' field on its launch line");
  let tok = text.slice(at + field.length + 1);
  tok = tok.split(" ")[0];            // the line form separates fields by a space
  tok = tok.replace(/\$[\s\S]*$/, "");  // the end-of-line anchor and anything after it
  // a trailing ')' with no opener in this token closes the WHOLE-line group
  while (tok.endsWith(")") &&
         tok.split("(").length < tok.split(")").length) tok = tok.slice(0, -1);
  if (tok.length === 0) die(who + "'s launch-line expression for '" + field + "' is empty");
  return tok;
}
function classesFor(text, who, field) {
  const tok = fieldExpr(text, who, field);
  const out = [];
  for (const part of tok.split(",")) {
    let q = part;
    if (/^\(.*\)$/.test(q)) q = q.slice(1, -1);   // a capture group around one class
    const cls = /^\[([^\]\\^]+)\]$/.exec(q);
    if (cls) { out.push(cls[1]); continue; }
    if (/^[0-9]$/.test(q)) { out.push(q); continue; } // a fixed single-digit literal
    die(who + "'s launch-line expression for '" + field + "' is `" + part +
        "`, which this reader does not model (only a single character class, " +
        "optionally capture-wrapped, or one fixed digit — no alternation, " +
        "quantifier, escape, negation or multi-digit literal).\n" +
        "The accepted set of such a construct cannot be reported honestly here, " +
        "and a mis-read set is exactly how a widened launch field stays green. " +
        "Teach this reader the construct, in the same change that introduces it.");
  }
  if (out.length === 0) die(who + " has no character class for '" + field + "'");
  return out;
}
for (const [who, source] of [["the judge", jsrc], ["the normalizer", nsrc]]) {
  const lt = reSrc(source, who, "RE_LAUNCH");
  const tt = reSrc(source, who, "RE_TLAUNCH");
  for (const r of L) {
    const text = r.line === "launch" ? lt : tt;
    let spec;
    const tj = /^tapjump([1-4])$/.exec(r.f);
    if (tj) {
      const all = classesFor(text, who, "tapjump");
      if (all.length !== 4)
        die(who + "'s launch line binds " + all.length + " tapjump classes, want 4");
      spec = all[+tj[1] - 1];
    } else {
      const all = classesFor(text, who, r.f);
      if (all.length < 1) die(who + " has no character class for '" + r.f + "'");
      spec = all[0];
    }
    const got = [...expandClass(spec, who, r.f)].sort().join(",");
    const want = wantSet(r.lo, r.hi);
    if (got !== want)
      die("LAUNCH FIELD DOMAIN DISAGREEMENT in " + who + " for '" + r.f +
          "': it accepts {" + got + "}, the authored authority says {" + want + "}.\n" +
          "  authored citation: " + r.cite + "\n" +
          "Every field on this line becomes a sim_setup_match argument, so a widening buys a match the record does not describe.");
  }
}
// the authored claim that there is deliberately NO p1type column
for (const [who, source] of [["the judge", jsrc], ["the normalizer", nsrc]]) {
  if (/p1type=/.test(reSrc(source, who, "RE_LAUNCH")))
    die(who + "'s LAUNCH line gained a 'p1type=' field. The authored authority says foh.c:682-687 REFUSES to launch any port configuration that would need one — that refusal has been weakened, or the row block is stale.");
}

// --- (a5) the authored N rules: the S token ALPHABET and the frame anchor ---
const CANON = "(0|[1-9][0-9]*)";
let ALPHA = null;
for (const r of N) {
  if (r.rule === "stokenalphabet") {
    ALPHA = new Set();
    for (const it of r.val.split(",")) {
      const rg = /^(-?[0-9])-([0-9])$/.exec(it);
      if (rg) { for (let v = +rg[1]; v <= +rg[2]; v++) ALPHA.add(String(v)); }
      else if (/^-?[0-9]+$/.test(it)) ALPHA.add(it);
      else die("authored stokenalphabet item '" + it + "' is not a value or a range");
    }
    // the judge's RE_S_NUM: its FIELD list and its VALUE alphabet, semantically
    const txt = reSrc(jsrc, "the judge", "RE_S_NUM");
    const gs = txt.match(/\(([^()]*)\)/g);
    if (!gs || gs.length !== 3)
      die("the judge's RE_S_NUM no longer has exactly 3 groups (anchor, field, value) — leg [0n] cannot compare what it cannot parse");
    const gval = gs[2].slice(1, -1), gfld = gs[1].slice(1, -1);
    const gotA = [...expandVals(gval, "the judge", "RE_S_NUM alphabet")].sort().join(",");
    const wantA = [...ALPHA].sort().join(",");
    if (gotA !== wantA)
      die("S TOKEN ALPHABET DISAGREEMENT: the judge's RE_S_NUM admits {" + gotA +
          "}, the authored authority says {" + wantA + "}.\n" +
          "  authored citation: " + r.cite +
          "\nThe alphabet decides WHICH rule rejects a token, so widening it re-routes probes past the per-field domain check.");
    const fl = [];
    for (const raw of gfld.split("|")) {
      const ex = /^([a-z]+)\[([0-9])-([0-9])\]$/.exec(raw);
      if (ex) { for (let i = +ex[2]; i <= +ex[3]; i++) fl.push(ex[1] + i); }
      else fl.push(raw);
    }
    if (fl.slice().sort().join(",") !== ak)
      die("the judge's RE_S_NUM FIELD list disagrees with the authored authority.\n" +
          "  RE_S_NUM: " + fl.slice().sort().join(",") + "\n  authored: " + ak);
    continue;
  }
  if (r.rule !== "frameanchor") die("unknown authored N rule '" + r.rule + "'");
  if (r.val !== "canonical-decimal") die("unknown authored frameanchor value '" + r.val + "'");
  // PER-FORM, not "appears somewhere": every line form of both programs must
  // anchor its frame field with the canonical spelling.
  for (const kind of ["T", "S", "SHOT", "LAUNCH", "TLAUNCH", "END"]) {
    if (jsrc.indexOf("/^" + kind + " " + CANON + " ") < 0)
      die("the judge's " + kind + " line form does not anchor its frame field with " +
          CANON + ".\n  authored citation: " + r.cite);
  }
  if (!/const NUM = "\(0\|\[1-9\]\[0-9\]\*\)";/.test(nsrc))
    die("the normalizer's NUM constant is no longer exactly " + CANON +
        ".\n  authored citation: " + r.cite);
  for (const nm of ["RE_T", "RE_S", "RE_SHOT", "RE_LAUNCH", "RE_TLAUNCH", "RE_END"]) {
    if (reSrc(nsrc, "the normalizer", nm).indexOf('" + NUM + "') < 0)
      die("the normalizer's " + nm + " no longer anchors its frame field with the " +
          "shared NUM constant.\n  authored citation: " + r.cite);
  }
  for (const [who, source] of [["the judge", jsrc], ["the normalizer", nsrc]]) {
    const loose = source.match(/\^(?:T|S|SHOT|LAUNCH|TLAUNCH|END) \[0-9\]\+/g);
    if (loose)
      die(who + " anchors a line form with the LOOSE `[0-9]+` frame field (" +
          loose.join(", ") + "), not the canonical " + CANON + ".\n" +
          "  authored citation: " + r.cite);
  }
}
if (!ALPHA) die("the authored authority has no stokenalphabet N row");

// --- probe bases: committed .expect traces, one site per reachable screen ----
const RE_T_ = /^T (0|[1-9][0-9]*) ([a-z-]+) ([a-z-]+) (timer|start|a|b|bhold|launch)$/;
const bases = [];
for (const nm of ["f01-vs-g01.expect", "f03-options.expect", "f06-target-t01.expect"]) {
  const p = FLOWS + "/" + nm;
  if (!fs.existsSync(p)) die("probe base " + p + " is missing");
  const raw = fs.readFileSync(p, "utf8");
  const lines = raw.slice(0, -1).split("\n");
  const hm = /^FOHTRACE1 flow=(\S+)$/.exec(lines[0]);
  if (!hm) die(nm + " has no FOHTRACE1 header");
  const launch = lines.some(l => /^(LAUNCH|TLAUNCH) /.test(l)) ? "1" : "0";
  const sites = {};
  for (let k = 1; k < lines.length; k++) {
    const m = RE_T_.exec(lines[k]);
    if (!m || m[4] === "launch") continue;
    sites[m[3]] = { at: k + 1, frame: m[1] };   // insert AFTER the arrival T
  }
  bases.push({ nm, file: p, flow: hm[1], launch, lines, sites });
}
function siteFor(screen) {
  for (const b of bases) if (b.sites[screen]) return b;
  return null;
}
const allScreens = [];
for (const b of bases) for (const sc of Object.keys(b.sites))
  if (allScreens.indexOf(sc) === -1) allScreens.push(sc);
allScreens.sort();
if (allScreens.length < 8)
  die("the probe bases reach only " + allScreens.length + " screens (" +
      allScreens.join(",") + ") — the domain x screen cross-product would be vacuous");

const man = [];
let n = 0;
const seenNames = new Set();
function push(name, base, lines, expect, jneed, nneed, target) {
  // INJECTIVE OR NOTHING: two probes sharing a name overwrite one file, and
  // both then "pass" as whichever survived (measured once already, see the
  // tag() note below). A collision is a generator bug, never a silent merge.
  if (seenNames.has(name)) die("probe name collision: '" + name + "' is generated twice");
  seenNames.add(name);
  const path = OUT + "/" + name + ".txt";
  // Two structural probes are about the FILE's bytes, not its lines (an
  // empty trace, and one whose final newline was lost to a torn write), so a
  // body may arrive as a raw string that is written verbatim.
  fs.writeFileSync(path,
    typeof lines === "string" ? lines : lines.join("\n") + "\n");
  man.push([name, path, base.flow, base.launch, expect, jneed, nneed, target].join("\t"));
  n++;
}
// The DECLARED diagnostic wordings. These are typed here from each program's
// rule, not read back from its output: a probe that dies for an unrelated
// reason must not be able to satisfy its own assertion.
const formNeed = (lineNo, line) =>
  "line " + lineNo + " matches no FOHTRACE1 form: '" + line + "'";
const domNeed = (v, f, lineNo) =>
  "S value " + v + " outside the pinned domain of " + f + " at line " + lineNo;
const scrNeed = (f, lineNo, screen) =>
  "S field '" + f + "' at line " + lineNo + " is emitted on '" + screen +
  "', which cannot write it";
const refUnregNeed = (tok, lineNo) =>
  "unregistered refused entry '" + tok + "' at line " + lineNo;
const refScrNeed = (tok, lineNo, screen) =>
  "refusal '" + tok + "' at line " + lineNo + " is emitted on '" + screen +
  "', which cannot refuse it";
const edgeNeed = (edge, lineNo) =>
  "off-graph transition '" + edge + "' at line " + lineNo;

// A token outside the AUTHORED alphabet dies on FORM; inside it, on DOMAIN.
// Driven by the authored row, never by either program's own regex.
const inAlphabet = t => ALPHA.has(String(t));
function emitS(name, base, screen, field, value, target) {
  const st = base.sites[screen];
  const line = "S " + st.frame + " " + field + " " + value;
  const out = base.lines.slice();
  out.splice(st.at, 0, line);
  const lineNo = st.at + 1;                       // 1-based, header included
  const row = S.filter(r => r.f === field)[0];
  let expect = "A", jneed = "-", nneed = "-";
  if (!inAlphabet(value)) {
    expect = "R"; jneed = nneed = formNeed(lineNo, line);
  } else if (row && (+value < row.lo || +value > row.hi)) {
    expect = "R"; jneed = domNeed(+value, field, lineNo);
    nneed = formNeed(lineNo, line);              // the normalizer rejects on FORM
  } else if (row && row.scr.indexOf(screen) === -1) {
    expect = "R"; jneed = scrNeed(field, lineNo, screen);
    if (target !== "J") die("a screen-authorization probe cannot target the normalizer: it has no screen model");
  }
  push(name, base, out, expect, jneed, nneed, target);
}

// (b1) DOMAIN x SCREEN CROSS-PRODUCT (codex BLOCKER 2). Every S field is
// probed at its authored boundaries on EVERY reachable screen: accepted iff
// the authored row grants BOTH the value and the screen.
// lo-1 / hi+1 are only PROBEABLE when the authored alphabet can spell them;
// the authored file records honestly that HI+1 above 10 is unrepresentable.
const inAlpha = v => ALPHA.has(String(v));
for (const r of S) {
  for (const sc of r.scr) {
    if (!siteFor(sc))
      die("no committed .expect reaches screen '" + sc + "', so the authored row for '" +
          r.f + "' cannot be probed there. Add a flow that visits it rather than dropping the probe.");
  }
  for (const sc of allScreens) {
    const b = siteFor(sc);
    emitS("x-" + r.f + "-" + sc + "-lo", b, sc, r.f, r.lo, "J");
    if (r.hi !== r.lo) emitS("x-" + r.f + "-" + sc + "-hi", b, sc, r.f, r.hi, "J");
    if (inAlpha(r.lo - 1)) emitS("x-" + r.f + "-" + sc + "-below", b, sc, r.f, r.lo - 1, "J");
    if (inAlpha(r.hi + 1)) emitS("x-" + r.f + "-" + sc + "-above", b, sc, r.f, r.hi + 1, "J");
  }
}

// (b2) THE VALUE ALPHABET ITSELF, from a FIXED universe wider than either
// program's own alphabet, on each field's home screen, through BOTH programs.
// Deriving the sweep from RE_S_NUM would let a widening of RE_S_NUM widen the
// very sweep meant to catch it.
const UNIVERSE = ["-2", "-1", "0", "1", "2", "3", "4", "5", "6", "7", "8", "9",
                  "10", "11", "12", "00", "010", "+1", "1.0"];
// INJECTIVE probe naming: "-1" and "+1" must not collide onto one file, or
// one probe silently overwrites the other and both "pass" as whichever
// survived (measured: they did).
const tag = t => t.replace(/-/g, "m").replace(/\+/g, "p").replace(/\./g, "d");
for (const r of S) {
  const home = siteFor(r.scr[0]);
  for (const t of UNIVERSE)
    emitS("u-" + r.f + "-" + tag(t), home, r.scr[0], r.f, t, "B");
}

// (b3) EDGES, behaviorally: the evaluated set above is exact, so this proves
// the set that was compared is the set that is ENFORCED.
const liveEdgeSet = new Set(liveEdges);
for (const b of bases) {
  let made = 0;
  for (let k = 1; k < b.lines.length && made < 2; k++) {
    const m = RE_T_.exec(b.lines[k]);
    if (!m || m[4] === "launch") continue;
    const cand = allScreens.find(s =>
      s !== m[3] && !liveEdgeSet.has(m[2] + ">" + s + ">" + m[4]));
    if (!cand) continue;
    const edge = m[2] + ">" + cand + ">" + m[4];
    const line = "T " + m[1] + " " + m[2] + " " + cand + " " + m[4];
    const out = b.lines.slice();
    out[k] = line;
    push("e-" + b.flow + "-" + made, b, out, "R", edgeNeed(edge, k + 1), "-", "J");
    made++;
  }
  if (made === 0) die("could not build an unauthored-edge probe from " + b.nm);
}

// (b4) REFUSALS, behaviorally: legal on the bound screen, rejected elsewhere,
// and an unregistered token rejected outright.
for (const r of R) {
  if (!liveProf(r.prof)) continue;
  const home = siteFor(r.scr[0]);
  if (!home) die("no committed .expect reaches screen '" + r.scr[0] +
                 "', so the authored refusal row for '" + r.tok + "' cannot be probed");
  {
    const st = home.sites[r.scr[0]];
    const out = home.lines.slice();
    out.splice(st.at, 0, "S " + st.frame + " refused " + r.tok);
    push("r-" + r.tok + "-ok", home, out, "A", "-", "-", "J");
  }
  const other = allScreens.find(s => r.scr.indexOf(s) === -1 && siteFor(s));
  if (!other) die("no illegal screen available to probe refusal '" + r.tok + "'");
  {
    const b = siteFor(other), st = b.sites[other];
    const out = b.lines.slice();
    out.splice(st.at, 0, "S " + st.frame + " refused " + r.tok);
    push("r-" + r.tok + "-bad", b, out, "R", refScrNeed(r.tok, st.at + 1, other), "-", "J");
  }
}
{ // an unregistered token: the map is closed, not merely non-empty
  const b = bases[0], sc = Object.keys(b.sites).sort()[0], st = b.sites[sc];
  const out = b.lines.slice();
  out.splice(st.at, 0, "S " + st.frame + " refused notarefusal");
  push("r-unregistered", b, out, "R", refUnregNeed("notarefusal", st.at + 1), "-", "J");
}

// (b5) THE FRAME-ANCHOR FORM, through BOTH programs. This is the exploit
// codex demonstrated: widening the normalizer's NUM to `[0-9]+` while the
// judge kept the canonical form left both gates green.
for (const b of bases) {
  for (let k = 1; k < b.lines.length; k++) {
    const m = /^(T|S|SHOT|LAUNCH|TLAUNCH|END) ([0-9]+) (.*)$/.exec(b.lines[k]);
    if (!m) continue;
    const line = m[1] + " 0" + m[2] + " " + m[3];
    const out = b.lines.slice();
    out[k] = line;
    const nd = formNeed(k + 1, line);
    push("z-" + b.flow + "-" + m[1], b, out, "R", nd, nd, "B");
    break;
  }
}
// (b6) THE LAUNCH PLANE, BEHAVIORALLY (Tier A+ round-5 BLOCKER). (a4) reads
// each program's regex SOURCE; a construct that reader mis-models leaves the
// whole launch plane witnessed by a static read only — and a static read is
// defeatable by any construct it does not model (measured: an alternation
// widened `difficulty` to 7 and both gates stayed green). These probes settle
// the plane by EXECUTION instead: every authored L field is driven at every
// value of a FIXED universe wider than either program's own class, through
// BOTH programs, and each rejection must arrive at the rule's OWN declared
// wording. The universe is fixed here, never derived from a program's regex,
// for the same reason (b2)'s is.
const LUNIVERSE = ["-1", "0", "1", "2", "3", "4", "5", "6", "7", "8", "9",
                   "10", "00", "+1", "1.0", "x"];
const lbase = {};
for (const b of bases) {
  for (let k = 1; k < b.lines.length; k++) {
    if (/^LAUNCH /.test(b.lines[k])) lbase.launch = { b, k };
    if (/^TLAUNCH /.test(b.lines[k])) lbase.tlaunch = { b, k };
  }
}
for (const kind of ["launch", "tlaunch"])
  if (!lbase[kind])
    die("no probe base carries a " + kind.toUpperCase() + " line, so the authored L rows for it cannot be probed behaviorally. Add a flow that launches rather than dropping the probes.");
// Rewrites ONE field of a launch line, leaving every other column intact, so
// each probe isolates exactly the field its authored row governs.
function setField(line, field, value) {
  const tj = /^tapjump([1-4])$/.exec(field);
  const name = tj ? "tapjump" : field;
  const at = line.indexOf(" " + name + "=");
  if (at < 0) die("the probe base's launch line has no '" + name + "=' column: " + line);
  const start = at + name.length + 2;
  let end = line.indexOf(" ", start);
  if (end < 0) end = line.length;
  let val = value;
  if (tj) {
    const cols = line.slice(start, end).split(",");
    if (cols.length !== 4) die("the base's tapjump tuple is not 4 columns: " + line);
    cols[+tj[1] - 1] = value;
    val = cols.join(",");
  }
  return line.slice(0, start) + val + line.slice(end);
}
let nL = 0;
for (const r of L) {
  const { b, k } = lbase[r.line];
  const before = nL;
  for (const t of LUNIVERSE) {
    const line = setField(b.lines[k], r.f, t);
    const out = b.lines.slice();
    out[k] = line;
    // The authored row decides, not either program: a value is legal iff it is
    // a canonical single digit inside [lo,hi]. Everything else must die on the
    // line FORM, in both programs, at the declared wording.
    const ok = /^[0-9]$/.test(t) && +t >= r.lo && +t <= r.hi;
    const nd = formNeed(k + 1, line);
    push("l-" + r.line + "-" + r.f + "-" + tag(t), b, out,
         ok ? "A" : "R", ok ? "-" : nd, ok ? "-" : nd, "B");
    nL++;
  }
  if (nL - before !== LUNIVERSE.length)
    die("the L row for '" + r.f + "' generated " + (nL - before) + " probes, want " + LUNIVERSE.length);
}
if (nL !== L.length * LUNIVERSE.length)
  die("the launch plane generated " + nL + " probes, want " + L.length + " authored L rows x " + LUNIVERSE.length + " universe values");
fs.writeFileSync(OUT + "/lplane.txt", String(L.length) + " " + String(LUNIVERSE.length) + "\n");
// (b7) THE DELIMITER PLANE, through BOTH programs (Tier A+ round-18 BLOCKER;
// widened round-19 to every separator POSITION; widened round-21 to every
// PARSER FORM and every delimiter KIND).
//
// A trace line's fields are held apart by delimiters. If either program's
// regex spells a delimiter loosely -- ` +` where the grammar says one space,
// or an unanchored `=` inside a k=v token -- then a malformed line
// canonicalizes to exactly the bytes of a valid one and BOTH gates go green
// after a documented re-freeze. This plane closes that as a CLASS: every
// delimiter of every form the committed traces carry is perturbed and driven
// through BOTH programs, and each rejection must arrive at that program's OWN
// declared wording.
//
// ROUND-21 CLASS FIX (Tier A+ round-20 BLOCKERs 2 and 3). The round-19 plane
// was still narrow in two ways no static read of it revealed:
//
//   (1) it picked ONE base per KEYWORD (`T`,`S`,`SHOT`,...), but the programs
//       parse by FORM, not by keyword. `S` has TWO forms -- `S N W N` (screen
//       + carry) and `S N W W` (the RE_S_REF `refused` arm, carried only by
//       f04-nav.expect and f07-target-t02.expect) -- and the second was
//       probed by nothing. The HEADER was excluded outright, so the one line
//       every trace must carry had zero delimiter probes.
//   (2) it perturbed ASCII SPACES only. The `=` of a k=v token and the `,`
//       inside such a value are delimiters too: loosening `transitions=` to
//       `transitions *=` is invisible to a space-only plane.
//
// So the base set is now one line per measured FORM SIGNATURE over EVERY
// committed FOHTRACE1 .expect, header included. That is deliberately a WIDER
// pool than `bases`: `bases` exists for the screen cross-product and stays at
// three files so those counts do not ripple. And the delimiter set is a
// function of the LINE's own token grammar -- every inter-token space, the
// FIRST `=` of every k=v token, every `,` inside such a value. Nothing here
// is derived from either program's regex; that is the whole point, and the
// same reason (b2)'s and (b6)'s universes are authored rather than read back.

// A form SIGNATURE is the line's token SHAPE: keyword verbatim, then one
// letter per token -- N canonical integer, K a k=v token, W anything else.
// Computed from the line's own bytes, never from a parser's regex.
const formSig = (line) => line.split(" ").map((t, i) =>
  i === 0 ? t : (/^-?[0-9]+$/.test(t) ? "N" : (t.indexOf("=") >= 0 ? "K" : "W"))
).join(" ");

// The delimiter POSITIONS of a line, from that token grammar.
function delimPositions(line) {
  const sp = [], eq = [], cm = [];
  let at = 0;
  for (const tok of line.split(" ")) {
    const e = tok.indexOf("=");
    if (e >= 0) {
      eq.push(at + e);
      for (let j = e + 1; j < tok.length; j++) if (tok[j] === ",") cm.push(at + j);
    }
    at += tok.length;
    if (at < line.length) { sp.push(at); at++; }
  }
  return { sp, eq, cm };
}

// Spaces and k=v punctuation get different perturbation sets because deleting
// a space joins two TOKENS while deleting an `=` joins a key to its VALUE --
// both are real corruptions, neither is a substitute for the other.
const SEPPOS = [
  ["dbl", (s, i) => s.slice(0, i) + s[i] + s.slice(i)],       // doubled
  ["tab", (s, i) => s.slice(0, i) + "\t" + s.slice(i + 1)],   // tab for space
  ["del", (s, i) => s.slice(0, i) + s.slice(i + 1)],          // removed
];
const PUNCPOS = [
  ["dbl", (s, i) => s.slice(0, i) + s[i] + s.slice(i)],       // `==` / `,,`
  ["del", (s, i) => s.slice(0, i) + s.slice(i + 1)],          // removed
  ["spc", (s, i) => s.slice(0, i) + " " + s.slice(i)],        // space before
];
// Position-independent anchor perturbations (the `^` and `$` ends of a form).
const SEPANCHOR = [
  ["lead", l => " " + l],
  ["trail", l => l + " "],
  ["suffix", l => l + " zz"],
];

// MEASURED-then-FROZEN: every form the committed traces carry, and how many
// delimiters of each kind that form's probed line holds. Pinned in BOTH
// directions -- a NEW form (a flow gains a line kind) fails here rather than
// going silently unprobed, and a form that LOSES a column fails rather than
// silently shrinking the plane while this leg still prints OK.
const FORMSHAPE = {
  "HDR":                            { sp: 1,  eq: 1,  cm: 0 },
  "T N W W W":                      { sp: 4,  eq: 0,  cm: 0 },
  "S N W N":                        { sp: 3,  eq: 0,  cm: 0 },
  "S N W W":                        { sp: 3,  eq: 0,  cm: 0 },
  "SHOT N W":                       { sp: 2,  eq: 0,  cm: 0 },
  "LAUNCH N K K K K K K K K K K K": { sp: 12, eq: 11, cm: 3 },
  "TLAUNCH N K K":                  { sp: 3,  eq: 2,  cm: 0 },
  "END N K":                        { sp: 2,  eq: 1,  cm: 0 },
};

// The wider pool: EVERY committed FOHTRACE1 trace. `.bstate.` companions are
// not FOHTRACE1 traces (measured: 6 of the 13 .expect files), so they are
// excluded by name and every remaining file MUST carry a header -- a silent
// skip here would drop a form from the pin above.
const formBase = {};
const formFiles = [];
for (const nm of fs.readdirSync(FLOWS)
       .filter(n => /\.expect$/.test(n) && n.indexOf(".bstate.") < 0).sort()) {
  const p = FLOWS + "/" + nm;
  const lines = fs.readFileSync(p, "utf8").slice(0, -1).split("\n");
  const hm = /^FOHTRACE1 flow=(\S+)$/.exec(lines[0]);
  if (!hm) die("committed trace " + nm + " carries no FOHTRACE1 header, so its " +
    "line forms cannot be probed behaviorally");
  formFiles.push(nm);
  const launch = lines.some(l => /^(LAUNCH|TLAUNCH) /.test(l)) ? "1" : "0";
  const b = { nm, file: p, flow: hm[1], launch, lines };
  for (let k = 0; k < lines.length; k++) {
    const sg = k === 0 ? "HDR" : formSig(lines[k]);
    if (!formBase[sg]) formBase[sg] = { b, k };
  }
}
if (formFiles.length < 5)
  die("the delimiter plane found only " + formFiles.length + " committed FOHTRACE1 " +
    "traces — the committed flow set is expected to be larger, so forms are going unprobed");

const gotSigs = Object.keys(formBase).sort();
const wantSigs = Object.keys(FORMSHAPE).sort();
if (gotSigs.join(" | ") !== wantSigs.join(" | "))
  die("the committed traces carry form signatures {" + gotSigs.join(" | ") + "} " +
    "but FORMSHAPE pins {" + wantSigs.join(" | ") + "}. A form the programs parse " +
    "is either newly present and probed by nothing, or gone and this plane just " +
    "shrank. Re-measure and re-pin FORMSHAPE deliberately, in the same change.");

let nW = 0;
const wcounts = [];
for (const sig of wantSigs) {
  const { b, k } = formBase[sig];
  const src = b.lines[k];
  const d = delimPositions(src);
  const want = FORMSHAPE[sig];
  if (d.sp.length !== want.sp || d.eq.length !== want.eq || d.cm.length !== want.cm)
    die("the '" + sig + "' probe base '" + src + "' carries " + d.sp.length + " space / " +
      d.eq.length + " '=' / " + d.cm.length + " ',' delimiters, want pinned " +
      want.sp + "/" + want.eq + "/" + want.cm + " — the form's column count moved, so " +
      "this plane would silently probe a different number of positions. Re-measure " +
      "and re-pin FORMSHAPE deliberately.");
  // slug is derived from the signature itself (no hand-naming) and is
  // hyphen-free, because the shell's per-group accounting splits the probe
  // name at the first `-` after the slug.
  const slug = sig.toLowerCase().replace(/[^a-z0-9]+/g, "");
  const muts = [];
  const emit = (tag, list, set) => {
    for (let j = 0; j < list.length; j++) {
      const t2 = (j < 10 ? "0" : "") + String(j);
      for (const p of set) muts.push([tag + t2 + p[0], p[1](src, list[j])]);
    }
  };
  emit("sp", d.sp, SEPPOS);
  emit("eq", d.eq, PUNCPOS);
  emit("cm", d.cm, PUNCPOS);
  for (const a of SEPANCHOR) muts.push([a[0], a[1](src)]);
  const nwant = (want.sp + want.eq + want.cm) * 3 + SEPANCHOR.length;
  if (muts.length !== nwant)
    die("the '" + sig + "' form generated " + muts.length + " perturbations, want " + nwant);
  for (const mu of muts) {
    const line = mu[1];
    if (line === src)
      die("perturbation " + mu[0] + " is a NO-OP on the '" + sig + "' line '" + src +
        "' — a probe that changes nothing proves nothing");
    const out = b.lines.slice();
    out[k] = line;
    // DECLARED diagnostic wordings, typed here from each program's OWN rule,
    // never read back from its output. The header is the one form where the
    // two programs word it differently: the judge names the exact header it
    // demanded, the normalizer names the bytes it got.
    let jneed, nneed;
    if (k === 0) {
      jneed = "header line is not exactly 'FOHTRACE1 flow=" + b.flow + "': '" + line + "'";
      nneed = "bad header: '" + line + "'";
    } else {
      jneed = formNeed(k + 1, line);
      nneed = jneed;
    }
    // The manifest is TAB-separated and `tab` perturbations put a TAB into the
    // offending line the wording echoes — truncate at the first tab. `grep -F`
    // is a substring match, so the rule and its line number stay pinned
    // exactly; only the echoed tail goes unpinned, and only for tab probes.
    push("w-" + slug + "-" + mu[0], b, out, "R",
      jneed.split("\t")[0], nneed.split("\t")[0], "B");
    nW++;
  }
  wcounts.push("w-" + slug + " " + String(muts.length));
}
const nWwant = wantSigs.reduce((a, s) =>
  a + (FORMSHAPE[s].sp + FORMSHAPE[s].eq + FORMSHAPE[s].cm) * 3 + SEPANCHOR.length, 0);
if (nW !== nWwant)
  die("the delimiter plane generated " + nW + " probes, want " + nWwant +
    " (sum over form signatures of delimiters x 3 perturbations + " +
    SEPANCHOR.length + " anchor perturbations)");
// Line 1 is <forms> <total>; then one PINNED count per form group, so the
// shell's accounting no longer assumes uniform group size.
fs.writeFileSync(OUT + "/wplane.txt",
  String(wantSigs.length) + " " + String(nW) + "\n" + wcounts.join("\n") + "\n");
// (b8) THE STRUCTURAL / TRACE-INTEGRITY PLANE, by execution (Tier A+ round-6
// BLOCKER). (b2)-(b7) settle the judge's VALUE decisions: which values, which
// screens, which edges, which tokens, which separators. Every one of them is a
// decision about a LINE. Nothing above this point constrains the SHAPE of a
// trace -- the rules that make it a trace rather than a bag of independently
// valid lines: the header, chain continuity, frame monotonicity, launch
// adjacency and uniqueness, SHOT-name uniqueness, END and its transition
// count. Those rules were witnessed only by leg [0g]'s hash of the judge's own
// source (which cannot see a LOOSENING, the whole reason this file exists) and
// by legs [5]/[5b]'s negative corpus, which is a FIXED set of hand-made
// fixtures: delete a structural rule the corpus does not happen to exercise
// and every gate stays green. Each authored X row is now driven by a corrupt
// trace built here, and the judge must reject it at that rule's OWN declared
// wording -- typed below from the rule, never read back from its output.
const xNeed = {
  empty:        ()            => "empty trace",
  torn:         ()            => "missing trailing newline (torn write)",
  header:       (flow, got)   => "header line is not exactly 'FOHTRACE1 flow=" +
                                 flow + "': '" + got + "'",
  afterend:     ln            => "content after END at line " + ln,
  tregress:     ln            => "T frame regressed at line " + ln,
  tchain:       (ln, fr, cur) => "T-chain break at line " + ln + ": departs '" +
                                 fr + "' but the machine is on '" + cur + "'",
  launchadj:    ln            => "LAUNCH at line " + ln + " is not immediately " +
                                 "preceded by its 'T <f> sss match launch'",
  tlaunchadj:   ln            => "TLAUNCH at line " + ln + " is not immediately " +
                                 "preceded by its 'T <f> target-select target-match launch'",
  sregress:     ln            => "S frame regressed at line " + ln,
  shotregress:  ln            => "SHOT frame regressed at line " + ln,
  shotdup:      ln            => "duplicate SHOT name at line " + ln,
  endframe:     ()            => "END frame below the last event frame",
  endcount:     (decl, cnt)   => "END transitions=" + decl + " but " + cnt +
                                 " T lines counted",
  noend:        ()            => "no END line (truncated trace)",
  launchexpect: (want, saw)   => "launch expectation: want " + want + ", saw " + saw,
};
// The bases are committed .expect traces, so a construction that cannot find
// the line it needs is a COVERAGE loss, not a probe to quietly drop.
function xLast(b, re) {
  for (let k = b.lines.length - 1; k >= 1; k--) if (re.test(b.lines[k])) return k;
  return -1;
}
function xNeedLine(b, re, what) {
  const k = xLast(b, re);
  if (k < 0)
    die("probe base " + b.nm + " carries no " + what + ", so the structural " +
        "rule guarding it cannot be probed behaviorally. Add a flow that emits " +
        "one rather than dropping the probe.");
  return k;
}
function xAnyBase(re, what) {
  for (const b of bases) { const k = xLast(b, re); if (k >= 0) return { b, k }; }
  die("no committed .expect probe base carries " + what + ", so the structural " +
      "rule guarding it cannot be probed behaviorally. Add a flow that emits " +
      "one rather than dropping the probe.");
}
// A frame-regression probe only proves anything if some EARLIER line already
// pushed lastFrame above the value we regress to.
function xMaxFrameBefore(b, k) {
  let mx = 0;
  for (let j = 1; j < k; j++) {
    const m = /^(?:T|S|SHOT|LAUNCH|TLAUNCH) (0|[1-9][0-9]*) /.exec(b.lines[j]);
    if (m && Number(m[1]) > mx) mx = Number(m[1]);
  }
  return mx;
}
function xZeroFrame(b, k) {
  if (xMaxFrameBefore(b, k) === 0)
    die("line " + (k + 1) + " of " + b.nm + " has no earlier framed line above " +
        "frame 0, so regressing it to 0 would not regress anything");
  const out = b.lines.slice();
  out[k] = b.lines[k].replace(/^([A-Z]+) (?:0|[1-9][0-9]*) /, "$1 0 ");
  if (out[k] === b.lines[k])
    die("could not zero the frame of '" + b.lines[k] + "'");
  return out;
}
const RE_LAUNCH_T = /^T (?:0|[1-9][0-9]*) [a-z-]+ [a-z-]+ launch$/;
const bL = bases.filter(b => xLast(b, /^LAUNCH /) >= 0)[0];
const bT = bases.filter(b => xLast(b, /^TLAUNCH /) >= 0)[0];
if (!bL) die("no probe base carries a LAUNCH line, so the launch-shape rules cannot be probed");
if (!bT) die("no probe base carries a TLAUNCH line, so the target-launch-shape rules cannot be probed");
// Shared shape of both launch families: the launch record and the `T ... launch`
// immediately above it. Located once so the four probes below cannot drift.
function xLaunchPair(b, kind) {
  const kl = xNeedLine(b, new RegExp("^" + kind + " "), "a " + kind + " line");
  const kt = kl - 1;
  if (kt < 1 || !RE_LAUNCH_T.test(b.lines[kt]))
    die("the " + kind + " line of " + b.nm + " is not preceded by its 'T ... launch' " +
        "line, so the base itself no longer has the shape the rule is about");
  const m = /^T (?:0|[1-9][0-9]*) ([a-z-]+) ([a-z-]+) launch$/.exec(b.lines[kt]);
  return { kl, kt, from: m[1], to: m[2] };
}
function xEndOf(b) {
  const k = xNeedLine(b, /^END /, "an END line");
  const m = /^END (0|[1-9][0-9]*) transitions=(0|[1-9][0-9]*)$/.exec(b.lines[k]);
  if (!m) die("the END line of " + b.nm + " does not parse");
  const tCount = b.lines.filter(l => /^T /.test(l)).length;
  if (Number(m[2]) !== tCount)
    die("probe base " + b.nm + " declares transitions=" + m[2] + " but carries " +
        tCount + " T lines -- the base is already inconsistent");
  return { k, frame: m[1], decl: Number(m[2]), tCount };
}
const XBUILD = {
  empty: { mode: "own", build: () => ({ b: bL, body: "", need: xNeed.empty() }) },
  torn: { mode: "own", build: () => ({
    b: bL, body: bL.lines.join("\n"), need: xNeed.torn() }) },
  header: { mode: "own", build: () => {
    const out = bL.lines.slice();
    out[0] = out[0] + "-not-this-flow";
    return { b: bL, body: out, need: xNeed.header(bL.flow, out[0]) };
  } },
  afterend: { mode: "own", build: () => {
    const e = xEndOf(bL);
    const out = bL.lines.slice();
    out.push(bL.lines[e.k]);          // END twice: a stale file, or two runs
    return { b: bL, body: out, need: xNeed.afterend(out.length) };
  } },
  tregress: { mode: "own", build: () => {
    const { b, k } = xAnyBase(/^T (?:0|[1-9][0-9]*) [a-z-]+ [a-z-]+ (?:timer|start|a|b|bhold)$/,
                              "a non-launch T line");
    return { b, body: xZeroFrame(b, k), need: xNeed.tregress(k + 1) };
  } },
  tchain: { mode: "own", build: () => {
    const b = bL;
    const k = xNeedLine(b, /^T /, "a T line");
    const m = /^T ((?:0|[1-9][0-9]*)) ([a-z-]+) ([a-z-]+) ([a-z]+)$/.exec(b.lines[k]);
    if (m[2] === "startup")
      die("the last T line of " + b.nm + " already departs 'startup', so " +
          "rewriting its origin to 'startup' would not break the chain");
    const out = b.lines.slice();
    out[k] = "T " + m[1] + " startup " + m[3] + " " + m[4];
    return { b, body: out, need: xNeed.tchain(k + 1, "startup", m[2]) };
  } },
  launchadj: { mode: "own", build: () => {
    const { kl, kt } = xLaunchPair(bL, "LAUNCH");
    const out = bL.lines.slice();
    out.splice(kt, 1);                // orphan the LAUNCH from its transition
    return { b: bL, body: out, need: xNeed.launchadj(kl) };
  } },
  // PREEMPTED (authored `preempt`): the LAUNCH frame must EQUAL the frame of
  // the T line above it, and that T line's own frame is regress-checked first,
  // so regressing the pair dies on the T. The probe proves the preemption.
  launchregress: { mode: "preempt", build: () => {
    const { kl, kt } = xLaunchPair(bL, "LAUNCH");
    const out = xZeroFrame(bL, kt);
    out[kl] = bL.lines[kl].replace(/^LAUNCH (?:0|[1-9][0-9]*) /, "LAUNCH 0 ");
    if (out[kl] === bL.lines[kl]) die("could not zero the LAUNCH frame");
    return { b: bL, body: out, need: xNeed.tregress(kt + 1) };
  } },
  // PREEMPTED: a second launch needs a second `T ... launch`, which departs a
  // screen the machine has already left, and no authored E row leaves `match`.
  launchonce: { mode: "preempt", build: () => {
    const { kl, kt, from, to } = xLaunchPair(bL, "LAUNCH");
    const out = bL.lines.slice();
    out.splice(kl + 1, 0, bL.lines[kt], bL.lines[kl]);
    return { b: bL, body: out, need: xNeed.tchain(kl + 2, from, to) };
  } },
  tlaunchadj: { mode: "own", build: () => {
    const { kl, kt } = xLaunchPair(bT, "TLAUNCH");
    const out = bT.lines.slice();
    out.splice(kt, 1);
    return { b: bT, body: out, need: xNeed.tlaunchadj(kl) };
  } },
  tlaunchregress: { mode: "preempt", build: () => {
    const { kl, kt } = xLaunchPair(bT, "TLAUNCH");
    const out = xZeroFrame(bT, kt);
    out[kl] = bT.lines[kl].replace(/^TLAUNCH (?:0|[1-9][0-9]*) /, "TLAUNCH 0 ");
    if (out[kl] === bT.lines[kl]) die("could not zero the TLAUNCH frame");
    return { b: bT, body: out, need: xNeed.tregress(kt + 1) };
  } },
  tlaunchonce: { mode: "preempt", build: () => {
    const { kl, kt, from, to } = xLaunchPair(bT, "TLAUNCH");
    const out = bT.lines.slice();
    out.splice(kl + 1, 0, bT.lines[kt], bT.lines[kl]);
    return { b: bT, body: out, need: xNeed.tchain(kl + 2, from, to) };
  } },
  sregress: { mode: "own", build: () => {
    const { b, k } = xAnyBase(/^S (?:0|[1-9][0-9]*) [a-z0-9]+ -?[0-9]+$/,
                              "a numeric S line");
    return { b, body: xZeroFrame(b, k), need: xNeed.sregress(k + 1) };
  } },
  // The refusal form is a SEPARATE arm of the judge with its own frame check,
  // and no committed base emits one -- so the probe SYNTHESISES a legal refusal
  // from an authored R row (exactly as (b4) does) and then regresses its frame.
  srefregress: { mode: "own", build: () => {
    for (const r of R) {
      if (!liveProf(r.prof)) continue;
      for (const sc of r.scr) {
        const b = siteFor(sc);
        if (!b) continue;
        const st = b.sites[sc];
        if (Number(st.frame) === 0) continue;   // nothing to regress below
        const out = b.lines.slice();
        out.splice(st.at, 0, "S 0 refused " + r.tok);
        return { b, body: out, need: xNeed.sregress(st.at + 1) };
      }
    }
    die("no authored refusal row can be placed on a reachable screen at a " +
        "frame above 0, so the refusal arm's frame check cannot be probed");
  } },
  shotregress: { mode: "own", build: () => {
    const { b, k } = xAnyBase(/^SHOT /, "a SHOT line");
    return { b, body: xZeroFrame(b, k), need: xNeed.shotregress(k + 1) };
  } },
  shotdup: { mode: "own", build: () => {
    const { b } = xAnyBase(/^SHOT /, "a SHOT line");
    const ks = [];
    for (let k = 1; k < b.lines.length; k++) if (/^SHOT /.test(b.lines[k])) ks.push(k);
    if (ks.length < 2)
      die("probe base " + b.nm + " carries only one SHOT line, so a duplicate " +
          "NAME cannot be built without also changing a frame");
    const first = /^SHOT (?:0|[1-9][0-9]*) (\S+)$/.exec(b.lines[ks[0]]);
    const last = ks[ks.length - 1];
    const out = b.lines.slice();
    out[last] = b.lines[last].replace(/ \S+$/, " " + first[1]);
    if (out[last] === b.lines[last])
      die("the first and last SHOT of " + b.nm + " already share a name");
    return { b, body: out, need: xNeed.shotdup(last + 1) };
  } },
  endframe: { mode: "own", build: () => {
    const e = xEndOf(bL);
    return { b: bL, body: xZeroFrame(bL, e.k), need: xNeed.endframe() };
  } },
  endcount: { mode: "own", build: () => {
    const e = xEndOf(bL);
    const out = bL.lines.slice();
    out[e.k] = "END " + e.frame + " transitions=" + (e.decl + 1);
    return { b: bL, body: out, need: xNeed.endcount(e.decl + 1, e.tCount) };
  } },
  noend: { mode: "own", build: () => {
    const e = xEndOf(bL);
    const out = bL.lines.slice();
    out.splice(e.k, 1);
    return { b: bL, body: out, need: xNeed.noend() };
  } },
  // The trace stops launching but stays perfectly well formed: the transition
  // count is repaired too, so ONLY the expectation can catch it.
  launchexpect: { mode: "own", build: () => {
    const { kt } = xLaunchPair(bL, "LAUNCH");
    const e = xEndOf(bL);
    const out = bL.lines.slice();
    out[e.k] = "END " + e.frame + " transitions=" + (e.decl - 1);
    out.splice(kt, 2);                // the T ... launch and its LAUNCH
    return { b: bL, body: out, need: xNeed.launchexpect(1, 0) };
  } },
};
{
  // BIJECTION: an authored row with no construction is an unprobed claim, and
  // a construction with no authored row is a probe nothing stands behind.
  const ids = X.map(r => r.id);
  for (const r of X) {
    if (!XBUILD[r.id])
      die("authored X row '" + r.id + "' has no probe construction, so the " +
          "structural rule it claims is unwitnessed. Build the probe, or " +
          "delete the row and say why in the same change.");
    if (XBUILD[r.id].mode !== r.mode)
      die("authored X row '" + r.id + "' is declared " + r.mode + " but its " +
          "probe is built as " + XBUILD[r.id].mode);
  }
  for (const id of Object.keys(XBUILD))
    if (ids.indexOf(id) === -1)
      die("probe construction '" + id + "' has no authored X row to stand " +
          "behind it");
}
let nX = 0;
for (const r of X) {
  const g = XBUILD[r.id].build();
  // `k-` (kind/shape): the `x-` prefix is the S cross-product plane's.
  push("k-" + r.id, g.b, g.body, "R", g.need, "-", "J");
  nX++;
}
if (nX !== X.length)
  die("the structural plane generated " + nX + " probes, want " + X.length +
      " authored X rows");
fs.writeFileSync(OUT + "/xplane.txt",
  String(X.length) + " " + String(X.filter(r => r.mode === "preempt").length) + "\n");
// (b9) THE BOUNDED-DELTA PLANE, by execution (Tier A+ round-20 BLOCKER 1).
//
// normalize-foh-trace.js MODE 2 (`--bounded`) is the judge that stops a
// multi-second mid-run device stall from normalizing away: it re-checks every
// device tick against the injector's pinned cadence. Until this plane it was
// covered by NOTHING behavioral. Leg [0g] would NOTIFY if its source bytes
// changed, but no probe ever RAN it, so every numeric bound in it could have
// been loosened to infinity with all three gates still green. A bound that is
// never executed at its edge is not a bound; it is a comment.
//
// Every decision is probed AT the bound (must accept) and ONE TICK BEYOND
// (must reject) at that rule's OWN declared wording AND its OWN exit code --
// rc 3 `BOUND VIOLATION` for the measured envelopes, rc 2 `CORRUPT` for
// grammar and structure, rc 1 for the usage gate. The bound VALUES are
// restated here from the module's measured-then-frozen header block rather
// than read back out of it, so both directions bind: loosening OFS_HI
// 240 -> 300 fails the "241 rejects" probe, and tightening it to 200 fails
// the "240 accepts" probe.
//
// Device traces are SYNTHESISED from the committed .expect by tick arithmetic
// only -- every line's structural payload is copied verbatim -- so each probe
// isolates exactly one tick decision and nothing else. That is what a device
// run varies too: the structure is what the elision leg already pins, the
// ticks are what this mode exists to judge.

// --- QUANTIFIER plane: the trace grammar's one BOUNDED repetition ----------
// Round-21 B1: every other plane probes CHOICE (which alternative, which
// delimiter, which value); nothing probed HOW MANY. `[a-z0-9-]{1,32}` on SHOT
// names is the only bounded repetition in the FOHTRACE1 grammar, and it is
// duplicated across the two independent copies — so `{1,32}` -> `{1,33}` in
// one of them was a silent drift. The bounds are READ OUT OF THE PROGRAMS
// (never typed here), the copies must agree, and the plane probes AT both
// limits and one step OUTSIDE each, through BOTH programs.
{
  const qre = /\[a-z0-9-\]\{(\d+),(\d+)\}/g;
  const grab = (src, label) => {
    const out = [];
    let m;
    qre.lastIndex = 0;
    while ((m = qre.exec(src)) !== null) out.push(m[1] + "," + m[2]);
    if (!out.length)
      die("no bounded repetition found in " + label + " — the quantifier plane " +
        "is keyed to the SHOT-name `[a-z0-9-]{lo,hi}`; re-key it in the SAME change");
    return out;
  };
  const qj = grab(fs.readFileSync(JUDGE, "utf8"), "the judge");
  const qn = grab(fs.readFileSync(NORM, "utf8"), "the normalizer");
  const uniq = Array.from(new Set(qj.concat(qn)));
  if (uniq.length !== 1)
    die("the two grammar copies disagree on the SHOT-name repetition bound " +
      "(judge: " + qj.join(" ") + " / normalizer: " + qn.join(" ") + "). One " +
      "copy drifted; fix it — do not re-pin.");
  const QSITES = 3;   // MEASURED: judge x1, normalizer x2 (trace + flow copy)
  if (qj.length + qn.length !== QSITES)
    die("the SHOT-name repetition now appears at " + (qj.length + qn.length) +
      " sites, pinned " + QSITES + " — a copy was added or removed; re-pin in " +
      "the SAME change.");
  const [qlo, qhi] = uniq[0].split(",").map(Number);
  // a base that actually carries a SHOT line, MEASURED not assumed
  let qb = null, qi = -1;
  for (const b of bases) {
    const k = b.lines.findIndex(l => /^SHOT /.test(l));
    if (k >= 0) { qb = b; qi = k; break; }
  }
  if (qb === null)
    die("no probe base carries a SHOT line — the quantifier plane would be vacuous");
  const qhead = /^(SHOT (?:0|[1-9][0-9]*) )/.exec(qb.lines[qi])[1];
  for (const [tag, len, ok] of [["lo", qlo, true], ["hi", qhi, true],
                                ["over", qhi + 1, false], ["under", qlo - 1, false]]) {
    const ln = qhead + "a".repeat(len);
    const lines = qb.lines.slice();
    lines[qi] = ln;
    // "-" placeholders, never "": the manifest is read with IFS=<tab> and a
    // tab is IFS WHITESPACE, so two adjacent empty fields would collapse and
    // shift every later column (the existing accept probes do the same).
    const nd = ok ? "-" : formNeed(qi + 1, ln);
    push("quant-shotname-" + tag + "-" + len, qb, lines, ok ? "A" : "R",
      nd, nd, "B");
  }
}

// BOUNDED RE-PIN LOG (append one entry per BWANT/BSITES change, newest last).
//   iter 95, round 20  BWANT 29  — plane created: 2 carriers (f01 LAUNCH,
//     f04 no-launch), 9 accept / 20 reject.
//   iter 95, round 21  BWANT 41, BSITES 16 — codex NO-GO: the carrier set was
//     chosen by ME, not by the rule, so the TLAUNCH arm of the END decision was
//     probed by nothing (deleting it left every probe green). Fixes, all
//     class-level: (a) f06-target-t01 joins as the TLAUNCH carrier and BKINDS
//     pins the carrier set to SPAN {LAUNCH, TLAUNCH, none} in both directions;
//     (b) every equality is probed on BOTH sides (END ±1 per carrier — `!==`
//     -> `>` used to survive); (c) quantifier/anchor bounds get their over- and
//     at-limit probes (end-max 7 vs 8 digits, FLOW1 header suffix/glue/lead,
//     SHOT name 32 vs 33); (d) the plane is now a function of THE PROGRAM —
//     every die()/boundDie() in the --bounded region must be reached by a
//     rejecting probe matching ALL its literal fragments. Teeth measured:
//     deleting the TLAUNCH arm fails 3 probes (2 of them still exit 3, with the
//     WRONG rule — rc-only judgment would have passed them); dropping one probe
//     names its orphaned site; adding an arm trips BSITES.
// The bounds, restated (normalize-foh-trace.js header block, measured iter 95).
const BID_SLACK = 2, BOFS_LO = 40, BOFS_HI = 240, BDEV_NEG = 90, BDEV_POS = 30;
// The injection cadence model, restated (flow-to-fkscript.js, 1:1 since the
// CSS free-cursor arc): T^(F) = round((LEAD_MS + round((F-370)*1000/60))*60/1000).
const bmodel = (F) => Math.round(((8200 + Math.round((F - 370) * (1000 / 60))) * 60) / 1000);
const BANCHOR = 100;   // a valid mid-band anchor, inside [OFS_LO, OFS_HI]

// Carriers, MEASURED rather than assumed: f01-vs-g01 is the LAUNCH path (END
// tick must equal the LAUNCH tick) and f04-nav is the NO-LAUNCH path (END tick
// must equal the leg's foh-max). Both carry pre-input and post-input events,
// so both phases of the model are exercised.
const bcar = {};
for (const id of ["f01-vs-g01", "f04-nav", "f06-target-t01"]) {
  const ep = FLOWS + "/" + id + ".expect";
  const fp = FLOWS + "/" + id + ".flow";
  const lines = fs.readFileSync(ep, "utf8").slice(0, -1).split("\n");
  const fl = fs.readFileSync(fp, "utf8").slice(0, -1).split("\n");
  let fi = 0;
  for (let k = 1; k < fl.length; k++) {
    const m = /^I (0|[1-9][0-9]*) (-|[A-Z]+)$/.exec(fl[k]);
    if (m && m[2] !== "-" && fi === 0) fi = Number(m[1]);
  }
  if (fi === 0) die("bounded carrier " + id + " has no non-neutral FLOW1 input row");
  const ev = [];
  for (let i = 1; i < lines.length; i++) {
    const t = lines[i].split(" ");
    ev.push({ kind: t[0], tick: Number(t[1]), rest: t.slice(2).join(" ") });
  }
  const hasLaunch = ev.some(e => e.kind === "LAUNCH" || e.kind === "TLAUNCH");
  const jpre = ev.findIndex(e => e.kind !== "END" && e.tick < fi);
  const janc = ev.findIndex(e => e.kind !== "END" && e.tick >= fi);
  let jlast = -1, jlaunch = -1;
  for (let j = 0; j < ev.length; j++) {
    if (ev[j].kind !== "END" && ev[j].tick >= fi) jlast = j;
    if (ev[j].kind === "LAUNCH" || ev[j].kind === "TLAUNCH") jlaunch = j;
  }
  if (jpre < 0 || janc < 0 || jlast <= janc)
    die("bounded carrier " + id + " no longer carries a pre-input event, an anchor " +
      "event and a LATER post-input event (measured: it did) — the bounded plane " +
      "cannot place its probes. Re-measure the carriers.");
  const endTickExp = ev[ev.length - 1].kind === "END" ? ev[ev.length - 1].tick : null;
  if (endTickExp === null) die("bounded carrier " + id + " does not end with an END line");
  bcar[id] = { id, ep, fp, lines, ev, fi, hasLaunch, jpre, janc, jlast, jlaunch,
    endTickExp };
}
// CARRIER-KIND COVERAGE, pinned both directions (round-21 BLOCKER). The END
// rule branches on `d.kind === "LAUNCH" || d.kind === "TLAUNCH"`, so deleting
// the TLAUNCH arm is INVISIBLE to a carrier set that only has LAUNCH and
// no-launch traces -- which is exactly what happened. The probe set must be a
// function of the KINDS THE RULE DISTINGUISHES, never of the files I happened
// to pick. A new launch kind fails here as "spanned by no carrier"; a lost
// carrier fails as "the kind set shrank".
const BKINDS = { "f01-vs-g01": "LAUNCH", "f04-nav": "none", "f06-target-t01": "TLAUNCH" };
const bkindSeen = {};
for (const id of Object.keys(bcar)) {
  const c = bcar[id];
  const k = c.jlaunch < 0 ? "none" : c.ev[c.jlaunch].kind;
  if (BKINDS[id] !== k)
    die("bounded carrier " + id + " now has launch kind '" + k + "', pinned '" +
      BKINDS[id] + "' — the END rules would be probed by the wrong carrier");
  bkindSeen[k] = (bkindSeen[k] || 0) + 1;
}
const bkindStr = Object.keys(bcar).map(id => BKINDS[id]).join("+");
for (const k of ["LAUNCH", "TLAUNCH", "none"])
  if (!bkindSeen[k])
    die("no bounded carrier spans END-rule kind '" + k + "' — that arm of the " +
      "rule would be judged by nothing. Add a carrier whose .expect carries it.");

// Synthesise a device trace: identity ticks before the flow's first input,
// model(F)+anchor after it, END pinned to the LAUNCH device tick or to foh-max.
// `bump` shifts ONE event; `endBump` shifts only the END line.
function bdev(car, opt) {
  const fi = opt.fi === undefined ? car.fi : opt.fi;
  const dts = [];
  let launchDev = null;
  for (let j = 0; j < car.ev.length; j++) {
    const e = car.ev[j];
    let dt;
    if (e.kind === "END") dt = null;
    else if (e.tick < fi) dt = e.tick;
    else dt = bmodel(e.tick) + opt.anchor;
    if (dt !== null && opt.bump && opt.bump.j === j) dt += opt.bump.d;
    if (e.kind === "LAUNCH" || e.kind === "TLAUNCH") launchDev = dt;
    dts.push(dt);
  }
  const endTick = (launchDev !== null ? launchDev : opt.endMax) + (opt.endBump || 0);
  const out = [car.lines[0]];
  for (let j = 0; j < car.ev.length; j++) {
    const e = car.ev[j];
    const tk = dts[j] === null ? endTick : dts[j];
    out.push(e.kind + " " + tk + (e.rest ? " " + e.rest : ""));
  }
  return out.join("\n") + "\n";
}

const bman = [];
const bseen = new Set();
let bAcc = 0, bRej = 0;
// name, expect, device, flow, end-max, mode, expected rc, declared wording.
function bpush(name, car, devText, endMax, mode, rc, need, flowPath) {
  if (bseen.has(name)) die("duplicate bounded probe name '" + name + "'");
  bseen.add(name);
  const dp = OUT + "/b-" + name + ".dev.txt";
  fs.writeFileSync(dp, devText);
  if (rc === 0) bAcc++; else bRej++;
  bman.push([name, car.ep, dp, flowPath || car.fp, String(endMax), mode,
    String(rc), need].join("\t"));
}
// The success line is asserted WHOLE (the shell cmp's stdout against exactly
// this), so a run that also printed a complaint cannot read as a pass.
const bOK = (car, dp, anchor) =>
  "bounded OK " + dp + " (events=" + car.ev.length + ", anchor=" + anchor + ")";
// A synthetic FLOW1 file, authored from the FLOW1 grammar (not from the
// program's regex), for the decisions no committed flow can express.
function bflow(name, text) {
  const p = OUT + "/b-" + name + ".flow";
  fs.writeFileSync(p, text);
  return p;
}

const F01 = bcar["f01-vs-g01"], F04 = bcar["f04-nav"],
      F06 = bcar["f06-target-t01"];
const dpOf = (name) => OUT + "/b-" + name + ".dev.txt";

// --- baselines: the accepting shape of both END rules ------------------------
bpush("f01-baseline", F01, bdev(F01, { anchor: BANCHOR, endMax: F01.endTickExp }),
  F01.endTickExp, "full", 0, bOK(F01, dpOf("f01-baseline"), BANCHOR));
bpush("f04-baseline", F04, bdev(F04, { anchor: BANCHOR, endMax: F04.endTickExp }),
  F04.endTickExp, "full", 0, bOK(F04, dpOf("f04-baseline"), BANCHOR));
bpush("f06-baseline", F06, bdev(F06, { anchor: BANCHOR, endMax: F06.endTickExp }),
  F06.endTickExp, "full", 0, bOK(F06, dpOf("f06-baseline"), BANCHOR));

// --- ID_SLACK: the identity phase, both directions ---------------------------
for (const sgn of [1, -1]) {
  const tag = sgn > 0 ? "pos" : "neg";
  for (const [edge, mag] of [["at", BID_SLACK], ["over", BID_SLACK + 1]]) {
    const nm = "idslack-" + tag + "-" + edge;
    const d = sgn * mag;
    const txt = bdev(F01, { anchor: BANCHOR, endMax: F01.endTickExp, bump: { j: F01.jpre, d } });
    const e = F01.ev[F01.jpre];
    bpush(nm, F01, txt, F01.endTickExp, "full", edge === "at" ? 0 : 3,
      edge === "at" ? bOK(F01, dpOf(nm), BANCHOR)
        : "pre-input event " + (F01.jpre + 1) + " (" + e.kind + " " + e.rest +
          "): device tick " + (e.tick + d) + " vs host " + e.tick +
          " exceeds ID_SLACK " + BID_SLACK);
  }
}

// --- OFS_LO / OFS_HI: the run's anchor offset --------------------------------
for (const [nmk, ok, bad] of [["lo", BOFS_LO, BOFS_LO - 1], ["hi", BOFS_HI, BOFS_HI + 1]]) {
  for (const [edge, a] of [["at", ok], ["over", bad]]) {
    const nm = "ofs-" + nmk + "-" + edge;
    const txt = bdev(F01, { anchor: a, endMax: F01.endTickExp });
    const e = F01.ev[F01.janc];
    bpush(nm, F01, txt, F01.endTickExp, "full", edge === "at" ? 0 : 3,
      edge === "at" ? bOK(F01, dpOf(nm), a)
        : "anchor event " + (F01.janc + 1) + " (" + e.kind + " " + e.rest +
          "): injection offset " + a + " ticks outside [" + BOFS_LO + "," + BOFS_HI + "]");
  }
}

// --- DEV_NEG / DEV_POS: mid-run cadence deviation ----------------------------
// Driven on f04 (no LAUNCH), so shifting the last post-input event cannot also
// move the END tick and confound two rules in one probe.
for (const [nmk, ok, bad] of [["neg", -BDEV_NEG, -BDEV_NEG - 1],
                              ["pos", BDEV_POS, BDEV_POS + 1]]) {
  for (const [edge, d] of [["at", ok], ["over", bad]]) {
    const nm = "dev-" + nmk + "-" + edge;
    const txt = bdev(F04, { anchor: BANCHOR, endMax: F04.endTickExp, bump: { j: F04.jlast, d } });
    const e = F04.ev[F04.jlast];
    bpush(nm, F04, txt, F04.endTickExp, "full", edge === "at" ? 0 : 3,
      edge === "at" ? bOK(F04, dpOf(nm), BANCHOR)
        : "event " + (F04.jlast + 1) + " (" + e.kind + " " + e.rest +
          "): cadence deviation " + d + " ticks outside [-" + BDEV_NEG + ",+" +
          BDEV_POS + "] (anchor " + BANCHOR + ") — mid-run stall or schedule defect");
  }
}

// --- the two END rules, each off by exactly one tick --------------------------
// An EQUALITY is two bounds, not one. Probing only END+1 leaves `!==` -> `>`
// (or `<`) accepted, so every equality here is probed on BOTH sides.
for (const car of [F01, F06]) {
  if (car.jlaunch < 0 || car.ev[car.jlaunch].tick < car.fi)
    die("bounded carrier " + car.id + "'s launch event is not a post-input " +
      "event — the END-vs-LAUNCH probe cannot compute the device launch tick");
  const lt = bmodel(car.ev[car.jlaunch].tick) + BANCHOR;
  const kind = car.ev[car.jlaunch].kind;
  for (const d of [-1, 1]) {
    const nm = "end-" + kind.toLowerCase() + (d < 0 ? "-early" : "-late");
    const txt = bdev(car, { anchor: BANCHOR, endMax: car.endTickExp, endBump: d });
    bpush(nm, car, txt, car.endTickExp, "full", 3,
      "END tick " + (lt + d) + " != LAUNCH tick " + lt);
  }
}
for (const d of [-1, 1]) {
  const nm = "end-nolaunch" + (d < 0 ? "-early" : "-late");
  const txt = bdev(F04, { anchor: BANCHOR, endMax: F04.endTickExp, endBump: d });
  bpush(nm, F04, txt, F04.endTickExp, "full", 3,
    "no-launch END tick " + (F04.endTickExp + d) + " != foh-max " + F04.endTickExp);
}

// --- structure and grammar (rc 2) ---------------------------------------------
{
  const base = bdev(F01, { anchor: BANCHOR, endMax: F01.endTickExp });
  const bl = base.slice(0, -1).split("\n");

  // header equality: a valid header naming a different flow
  const h = bl.slice(); h[0] = "FOHTRACE1 flow=zzz";
  bpush("hdr-differ", F01, h.join("\n") + "\n", F01.endTickExp, "full", 2,
    "trace headers differ");

  // line-count equality: one event short. NOT the END line -- dropping that
  // trips parseTrace's earlier no-END rule and the probe would prove nothing
  // about the count (measured: it did, first run of this plane).
  const c = bl.slice(); c.splice(F01.jlast + 1, 1);
  bpush("linecount", F01, c.join("\n") + "\n", F01.endTickExp, "full", 2,
    "structural line counts differ (" + F01.ev.length + " vs " + (F01.ev.length - 1) + ")");

  // structural mismatch: another committed line's payload, VERBATIM, moved
  // onto a same-kind line — both lines are valid forms, so this dies at the
  // pairing rule and not at the grammar.
  let sj = -1, sk = -1;
  for (let j = 0; j < F01.ev.length && sj < 0; j++)
    for (let k = 0; k < F01.ev.length; k++)
      if (j !== k && F01.ev[j].kind === F01.ev[k].kind &&
          F01.ev[j].rest !== F01.ev[k].rest &&
          F01.ev[j].rest !== "" && F01.ev[k].rest !== "") { sj = j; sk = k; break; }
  if (sj < 0) die("no two same-kind lines with different payloads in " + F01.id +
    " — the structural-mismatch probe cannot be built from committed bytes");
  const s = bl.slice();
  const st = s[sj + 1].split(" ");
  s[sj + 1] = F01.ev[sj].kind + " " + st[1] + " " + F01.ev[sk].rest;
  bpush("structmismatch", F01, s.join("\n") + "\n", F01.endTickExp, "full", 2,
    "structural mismatch at event " + (sj + 1) + ": '" + F01.ev[sj].kind + " " +
    F01.ev[sj].rest + "' vs '" + F01.ev[sj].kind + " " + F01.ev[sk].rest + "'");

  // end-max grammar: a non-canonical decimal
  bpush("endmax-grammar", F01, base, "007", "full", 2, "end-max grammar: '007'");
  // ...and its LENGTH bound, both sides. `(0|[1-9][0-9]{0,6})` admits at most
  // 7 digits; probing only well-formed values leaves `{0,6}` -> `{0,7}` free.
  // The 7-digit case is driven on the no-launch carrier with its END moved to
  // match, so it is the GRAMMAR that is being probed and not the END rule.
  {
    const wide = 9999999;                      // 7 digits: the maximum admitted
    const t = bdev(F04, { anchor: BANCHOR, endMax: wide });
    bpush("endmax-len-max", F04, t, wide, "full", 0,
      bOK(F04, dpOf("endmax-len-max"), BANCHOR));
    bpush("endmax-len-over", F04, t, "99999999", "full", 2,
      "end-max grammar: '99999999'");
  }

  // the usage gate: end-max omitted entirely
  bpush("arity", F01, base, F01.endTickExp, "noend", 1,
    "usage: node normalize-foh-trace.js --bounded");

  // --- FLOW1 grammar, on synthetic flows ------------------------------------
  bpush("flow-header", F01, base, F01.endTickExp, "full", 2,
    "flow header must be exactly FLOW1", bflow("flow-header", "FLOW2\nI 375 A\nEND 900\n"));
  // ANCHOR probes for the header: a WRONG header is not the same test as an
  // EXTENDED one. `lines[0] !== "FLOW1"` -> `!lines[0].startsWith("FLOW1")`
  // keeps rejecting FLOW2 while accepting `FLOW1 junk`. Same idiom the
  // delimiter plane uses per trace form, applied to the flow grammar.
  for (const [tag, hdr] of [["suffix", "FLOW1 junk"], ["glued", "FLOW1x"],
                            ["lead", " FLOW1"]]) {
    bpush("flow-header-" + tag, F01, base, F01.endTickExp, "full", 2,
      "flow header must be exactly FLOW1",
      bflow("flow-header-" + tag, hdr + "\nI 375 A\nEND 900\n"));
  }
  // the SHOT-name length bound INSIDE the flow grammar, both sides
  {
    const n32 = "a".repeat(32), n33 = "a".repeat(33);
    bpush("flow-shot-len-max", F01, base, F01.endTickExp, "full", 0,
      bOK(F01, dpOf("flow-shot-len-max"), BANCHOR),
      bflow("flow-shot-len-max", "FLOW1\nI 375 A\nSHOT 380 " + n32 + "\nEND 900\n"));
    bpush("flow-shot-len-over", F01, base, F01.endTickExp, "full", 2,
      "flow line 3 matches no FLOW1 form: 'SHOT 380 " + n33 + "'",
      bflow("flow-shot-len-over", "FLOW1\nI 375 A\nSHOT 380 " + n33 + "\nEND 900\n"));
  }
  bpush("flow-newline", F01, base, F01.endTickExp, "full", 2,
    "flow missing trailing newline", bflow("flow-newline", "FLOW1\nI 375 A\nEND 900"));
  bpush("flow-empty", F01, base, F01.endTickExp, "full", 2,
    "empty flow line at 3", bflow("flow-empty", "FLOW1\nI 375 A\n\nEND 900\n"));
  bpush("flow-form", F01, base, F01.endTickExp, "full", 2,
    "flow line 3 matches no FLOW1 form: 'X 1 Y'",
    bflow("flow-form", "FLOW1\nI 375 A\nX 1 Y\nEND 900\n"));
  bpush("flow-noinput", F01, base, F01.endTickExp, "full", 2,
    "flow has no non-neutral input row",
    bflow("flow-noinput", "FLOW1\nI 375 -\nI 400 -\nEND 900\n"));

  // --- anchor-null posture, BOTH directions ---------------------------------
  // A flow whose first input lands past every event tick leaves the whole run
  // in the identity phase, so no event can anchor the cadence.
  const lateFlow = bflow("anchornull", "FLOW1\nI 5000 A\nEND 5001\n");
  const late = bdev(F01, { anchor: BANCHOR, endMax: F01.endTickExp, fi: 99999 });
  bpush("anchornull-undeclared", F01, late, F01.endTickExp, "full", 3,
    "no post-input observable event — anchored cadence judgment is impossible " +
    "and flow '" + F01.id + "' is not declared input-free (explicit whitelist" +
    "/arg required)", lateFlow);
  bpush("anchornull-declared", F01, late, F01.endTickExp, "free", 0,
    bOK(F01, dpOf("anchornull-declared"), "none(declared)"), lateFlow);
  bpush("inputfree-stale", F01, base, F01.endTickExp, "free", 2,
    "declared input-free but event(s) anchored the cadence (anchor=" + BANCHOR +
    ") — stale declaration for flow '" + F01.id + "'");
}

// --- DECISION-SITE COVERAGE over the bounded region --------------------------
// The round-21 BLOCKER in one sentence: a probe plane sized by MY imagination
// silently omits arms. This makes the plane a function of THE PROGRAM: every
// die()/boundDie() site inside normalize-foh-trace.js's --bounded region must
// be reached by at least one REJECTING probe, matched on ALL of that site's
// literal message fragments (not just its first). Delete an arm and its
// literals vanish -> the pin below fails as "site count changed"; add an arm
// and it fails as "probed by nothing". Scope is the mode-2 region ONLY;
// parseTrace's own sites are covered by the main (probes.tsv) plane, which
// drives both programs through it.
let bSiteCount = 0;
{
  const src = fs.readFileSync(NORM, "utf8");
  const a = src.indexOf('if (process.argv[2] === "--bounded") {');
  const b = src.indexOf("// ---- MODE 1:");
  if (a < 0 || b < 0 || b <= a)
    die("cannot locate the --bounded region of " + NORM + " — the coverage " +
      "extractor is keyed to its markers; re-key it in the SAME change");
  const region = src.slice(a, b);
  // every die("...")/boundDie("...") call, with ALL its string literals
  const sites = [];
  const re = /\b(die|boundDie)\(/g;
  let m;
  while ((m = re.exec(region)) !== null) {
    let i = m.index + m[0].length, depth = 1, lits = [], cur = null;
    for (; i < region.length && depth > 0; i++) {
      const ch = region[i];
      if (cur !== null) {
        if (ch === "\\") { cur += region[i + 1]; i++; }
        else if (ch === '"') { lits.push(cur); cur = null; }
        else cur += ch;
        continue;
      }
      if (ch === '"') cur = "";
      else if (ch === "(") depth++;
      else if (ch === ")") depth--;
    }
    if (depth !== 0) die("unbalanced " + m[1] + "() call while extracting the " +
      "bounded decision sites — re-key the extractor");
    sites.push({ fn: m[1], lits: lits.filter(x => x.length >= 3),
      line: region.slice(0, m.index).split("\n").length });
  }
  const BSITES = 16;   // MEASURED (iter 95, round-21 fix): die/boundDie calls
                       // in the --bounded region. Re-pin in the SAME change.
  if (sites.length !== BSITES)
    die("the --bounded region now has " + sites.length + " decision sites, " +
      "pinned " + BSITES + " — an arm was added or deleted. Re-pin BSITES and " +
      "add/remove the probe that reaches it, in the SAME change.");
  const needs = bman.filter(r => r.split("\t")[6] !== "0")
                    .map(r => r.split("\t")[7]);
  const orphan = [];
  for (const st of sites) {
    const hit = needs.some(nd => st.lits.every(l => nd.indexOf(l) >= 0));
    if (!hit) orphan.push("line ~" + st.line + " " + st.fn + "(" +
      JSON.stringify(st.lits.join(" … ")) + ")");
  }
  if (orphan.length)
    die("bounded decision sites reached by NO rejecting probe:\n  " +
      orphan.join("\n  ") + "\nEvery arm must be judged by a probe that " +
      "declares its wording; add one per site above.");
  bSiteCount = sites.length;
  fs.writeFileSync(OUT + "/bsites.txt", String(sites.length) + "\n");
}

const BWANT = 41;
if (bman.length !== BWANT)
  die("the bounded plane generated " + bman.length + " probes, want " + BWANT +
    " — a decision was added or dropped; re-pin BWANT here in the SAME change " +
    "and say why in the BOUNDED RE-PIN note.");
fs.writeFileSync(OUT + "/bprobes.tsv", bman.join("\n") + "\n");
fs.writeFileSync(OUT + "/bplane.txt",
  String(bman.length) + " " + String(bAcc) + " " + String(bRej) + " " +
  String(Object.keys(bcar).length) + " " + bkindStr + " " +
  String(bSiteCount) + "\n");

fs.writeFileSync(OUT + "/probes.tsv", man.join("\n") + "\n");
console.log(String(n));
GENEOF
nprobe="$(node "$B/dom/gen.js" "$AUTHDOM" "$FOH/judge-foh-trace.js" \
  "$FOH/normalize-foh-trace.js" "$FOH/foh.h" "$B/dom" "$FOH/flows" \
  2> "$B/dom/gen.err")" \
  || { cat "$B/dom/gen.err" >&2; fail "a live decision surface disagrees with the authored authority $AUTHDOM (detail above)"; }
echo "      semantic: SVAL_DOM/SFIELD_SCREENS, EDGES, REFUSED, the normalizer's RE_S domains and the LAUNCH/TLAUNCH classes of BOTH programs agree with all 69 authored rows"
: > "$B/dom/ran.txt"
pacc=0; prej=0; pnorm=0
while IFS="$(printf '\t')" read -r pnm ppath pflow plaunch pexp pjn pnn ptgt; do
  [ -n "$pnm" ] || continue
  case "$ptgt" in
    J|B)
      prc=0
      node "$FOH/judge-foh-trace.js" "$ppath" "$pflow" "$plaunch" \
        > "$B/dom/$pnm.j.out" 2> "$B/dom/$pnm.j.err" || prc=$?
      if [ "$pexp" = A ]; then
        [ "$prc" = 0 ] \
          || fail "[0n] probe $pnm: the judge REJECTED what the authored authority PERMITS — either the judge narrowed, or the authored row is wrong. Fix whichever is actually wrong; do not re-pin. (got: $(head -c 200 "$B/dom/$pnm.j.err"))"
        pacc=$((pacc + 1))
      else
        [ "$prc" != 0 ] \
          || fail "[0n] probe $pnm: the judge ACCEPTED what the authored authority FORBIDS ('$pjn'). This is the loosening a re-freeze of the grammar dump cannot show."
        assert_one_diag_core "0n $pnm judge" "$B/dom/$pnm.j.out" "$B/dom/$pnm.j.err" \
          "$pjn" "judge-foh-trace: CORRUPT: "
        prej=$((prej + 1))
      fi
      ;;
  esac
  case "$ptgt" in
    N|B)
      nrc=0
      node "$FOH/normalize-foh-trace.js" "$ppath" "$B/dom/$pnm.norm" \
        > "$B/dom/$pnm.n.out" 2> "$B/dom/$pnm.n.err" || nrc=$?
      if [ "$pexp" = A ]; then
        [ "$nrc" = 0 ] \
          || fail "[0n] probe $pnm: the NORMALIZER rejected what the authored authority permits and the judge accepts — the device-side copy has drifted from the host-side one. (got: $(head -c 200 "$B/dom/$pnm.n.err"))"
      else
        [ "$nrc" != 0 ] \
          || fail "[0n] probe $pnm: the NORMALIZER accepted what the authored authority forbids and the judge rejects. The two independent copies of the grammar have drifted, and each is witnessed only by its own hash."
        assert_one_diag_core "0n $pnm normalizer" "$B/dom/$pnm.n.out" "$B/dom/$pnm.n.err" \
          "$pnn" "normalize-foh-trace: CORRUPT: "
      fi
      pnorm=$((pnorm + 1))
      ;;
  esac
  printf '%s\n' "$pnm" >> "$B/dom/ran.txt"
done < "$B/dom/probes.tsv"
# PROBE-COUNT RE-PIN LOG (the "say why" this pin demands, newest last):
#   Tier A+ round-19 BLOCKER — the separator plane (b7) became POSITIONAL:
#   every separator of every probed line kind is doubled/tabbed/deleted rather
#   than a fixed 7-perturbation list that happened to hit only the keyword
#   separator and the one before the final token. That plane went 6x7=42 ->
#   96 probes (T 15, S 12, SHOT 9, LAUNCH 39, TLAUNCH 12, END 9 = 3 per
#   separator + 3 anchor probes each), so the total moved 1165 -> 1219, +54.
#   This is strictly MORE coverage: no probe was removed, and the 42 positions
#   the old list covered are a subset of the 96.
#   Tier A+ round-21 BLOCKERs 2+3 — the separator plane (b7) became the
#   DELIMITER plane: one base per parser FORM SIGNATURE over every committed
#   FOHTRACE1 trace (was: one per KEYWORD over the 3-file `bases` pool, which
#   left the header line and the `S N W W` refused arm unprobed) and every
#   delimiter KIND, not just ASCII spaces (a k=v token's `=` and the `,`
#   inside its value are delimiters too). 8 form signatures x (delimiters x 3
#   perturbations + 3 anchor probes) = 168 probes (hdr 9, tnwww 15, snwn 12,
#   snww 12, shotnw 9, launch 81, tlaunchnkk 18, endnk 12), so the plane went
#   96 -> 168 and the total moved 1219 -> 1291, +72. Strictly MORE coverage:
#   every old position is still probed, the 30 space positions of the old 6
#   keyword bases being a subset of the 30 here. All 144 delimiter
#   perturbations were MEASURED to be rejected by both programs before
#   authoring, so the plane asserts rejection uniformly with no carve-outs.
#   Tier A+ round-21 B1 — the QUANTIFIER plane: every plane above probes WHICH
#   alternative, none probed HOW MANY. The grammar's one bounded repetition
#   (`[a-z0-9-]{1,32}` on SHOT names, duplicated across both programs) now gets
#   4 probes through BOTH programs — AT lo and hi (accepted) and one step
#   outside each (rejected) — with lo/hi READ OUT of the two sources and
#   asserted equal, so a `{1,33}` in one copy fails as a disagreement rather
#   than as a silently widened domain. 1291 -> 1295, +4. Strictly MORE
#   coverage: no probe removed.
[ "$nprobe" = "1295" ] \
  || fail "leg [0n] generated $nprobe behavioral probes, want 1295. The probe set is a FUNCTION of the authored table (S rows x every reachable screen x boundary, S rows x the fixed value universe, unauthored-edge and refusal-binding probes, and the frame-anchor form through both programs, every authored L field x the fixed launch-value universe through both programs, every DELIMITER POSITION of every parser FORM SIGNATURE perturbed (spaces doubled/tabbed/deleted; a k=v token's '=' and the ',' inside its value doubled/deleted/space-prefixed) plus 3 anchor probes per form through both programs, and one corrupt trace per authored TRACE-INTEGRITY row). A different count means the authored table or the reachable-screen set changed shape — re-pin here in the SAME change and say why."
# EVERY GENERATED PROBE WAS ACTUALLY RUN AND JUDGED (Tier A+ round-5 MINOR-1).
# Without this, a probe could be generated, counted into the pin, and then
# skipped by the dispatch loop -- the count would still look right.
nran="$(wc -l < "$B/dom/ran.txt" | tr -d ' ')"
[ "$nran" = "$nprobe" ] \
  || fail "leg [0n] generated $nprobe probes but ran $nran of them: the dispatch loop skipped probes the pin still counts."
[ "$((pacc + prej))" = "$nprobe" ] \
  || fail "leg [0n] judged $((pacc + prej)) probes but generated $nprobe: every probe must reach the judge (a normalizer-only probe would need this accounting widened deliberately, in the same change)."
# PER-FIELD launch-plane accounting: the L probes are a function of the
# authored table (one group per L row, one probe per universe value), so a
# field whose probes silently vanished cannot hide inside the total.
lrows="$(cut -d' ' -f1 "$B/dom/lplane.txt")"; luniv="$(cut -d' ' -f2 "$B/dom/lplane.txt")"
lgrp="$(grep '^l-' "$B/dom/ran.txt" | sed -E 's/^(l-[a-z]+-[a-z0-9]+)-.*$/\1/' | sort | uniq -c)"
lbad="$(printf '%s\n' "$lgrp" | awk -v n="$luniv" 'NF && $1 != n { print $2 "=" $1 }')"
lng="$(printf '%s\n' "$lgrp" | grep -c .)"
{ [ -z "$lbad" ] && [ "$lng" = "$lrows" ]; } \
  || fail "leg [0n] launch-plane probe accounting is off: $lng field groups (want $lrows, one per authored L row), off-count groups: ${lbad:-none} (want $luniv probes each). The launch plane is judged by EXECUTION, and a missing group means a field is judged by nothing."
echo "      $nprobe probes: $pacc accepted at the authored bounds, $prej rejected outside them at the rule's OWN declared diagnostic ($pnorm also driven through the normalizer)"
# PER-KIND separator-plane accounting, the (b7) twin of the launch tally: one
# group per line kind, one probe per perturbation. A kind whose probes
# vanished cannot hide inside the total.
wkinds="$(head -1 "$B/dom/wplane.txt" | cut -d' ' -f1)"; wtot="$(head -1 "$B/dom/wplane.txt" | cut -d' ' -f2)"
wgrp="$(grep '^w-' "$B/dom/ran.txt" | sed -E 's/^(w-[a-z0-9]+)-.*$/\1/' | sort | uniq -c)"
# The separator plane is POSITIONAL (round-19), so groups are NOT uniform in
# size: each line kind contributes 3 perturbations per separator it actually
# carries, plus 3 anchor probes. wplane.txt therefore ships the PINNED count
# per group and the accounting compares against that table, not against one
# shared number -- a group that silently shrank still cannot hide.
wbad="$(printf '%s\n' "$wgrp" | awk 'NR==FNR { if (FNR > 1) want[$1] = $2; next }
  NF { if (!($2 in want)) print $2 "=unpinned"; else if ($1 != want[$2]) print $2 "=" $1 "(want " want[$2] ")" }' \
  "$B/dom/wplane.txt" -)"
wng="$(printf '%s\n' "$wgrp" | grep -c .)"
wran="$(grep -c '^w-' "$B/dom/ran.txt" || true)"
{ [ -z "$wbad" ] && [ "$wng" = "$wkinds" ] && [ "$wran" = "$wtot" ]; } \
  || fail "leg [0n] delimiter-plane probe accounting is off: $wng form-signature groups (want $wkinds), $wran probes ran (want $wtot), off-count groups: ${wbad:-none}. The delimiters are judged by EXECUTION, and a missing group means a form's delimiters are judged by nothing."
echo "      launch plane behavioral: $lrows authored L fields x $luniv fixed-universe values, each through BOTH programs"
echo "      delimiter plane behavioral: $wkinds form signatures (header included), EVERY space doubled/tabbed/deleted and EVERY k=v '=' and ',' doubled/deleted/space-prefixed + 3 anchor probes each = $wtot probes, each through BOTH programs"
# PER-ROW structural-plane accounting, the (b8) twin: exactly one corrupt
# trace per authored X row, each rejected at its rule's OWN wording. A row
# whose probe vanished cannot hide inside the total.
xrows="$(cut -d' ' -f1 "$B/dom/xplane.txt")"; xpre="$(cut -d' ' -f2 "$B/dom/xplane.txt")"
xran="$(grep -c '^k-' "$B/dom/ran.txt" || true)"
[ "$xran" = "$xrows" ] \
  || fail "leg [0n] ran $xran structural probes but the authored table has $xrows X rows. The SHAPE of a trace is judged by EXECUTION, and a missing probe means a structural rule is judged by nothing."
echo "      structural plane behavioral: $xrows authored trace-integrity rules, one corrupt trace each ($xpre of them proving a PREEMPTION rather than the dead rule)"

# --- (b9) THE BOUNDED-DELTA PLANE, dispatched -------------------------------
# `--bounded` is the mode that stops a multi-second mid-run device stall from
# normalizing away. Every numeric bound in it is probed AT the bound (accept)
# and one tick beyond (reject at that rule's OWN wording and OWN exit code).
bacc=0; brej=0; bran=0
while IFS="$(printf '\t')" read -r bnm bexp bdev bflow bmax bmode brc bneed; do
  [ -n "$bnm" ] || continue
  brcgot=0
  case "$bmode" in
    full) node "$FOH/normalize-foh-trace.js" --bounded "$bexp" "$bdev" "$bflow" "$bmax" \
            > "$B/dom/b-$bnm.out" 2> "$B/dom/b-$bnm.err" || brcgot=$? ;;
    free) node "$FOH/normalize-foh-trace.js" --bounded "$bexp" "$bdev" "$bflow" "$bmax" input-free \
            > "$B/dom/b-$bnm.out" 2> "$B/dom/b-$bnm.err" || brcgot=$? ;;
    noend) node "$FOH/normalize-foh-trace.js" --bounded "$bexp" "$bdev" "$bflow" \
            > "$B/dom/b-$bnm.out" 2> "$B/dom/b-$bnm.err" || brcgot=$? ;;
    *) fail "[0b] probe $bnm: unknown dispatch mode '$bmode'" ;;
  esac
  [ "$brcgot" = "$brc" ] \
    || fail "[0b] probe $bnm: --bounded exited $brcgot, want $brc. The exit code IS the contract — check-device-foh.sh only distinguishes pass from fail, but a BOUND VIOLATION (3) and a CORRUPT trace (2) are different defects and the wrong one means the wrong rule fired. (stderr: $(head -c 300 "$B/dom/b-$bnm.err"))"
  if [ "$brc" = 0 ]; then
    # whole-output equality: a run that ALSO printed a complaint cannot read as
    # a pass, and the reported anchor/event count is part of the judgment.
    printf '%s\n' "$bneed" > "$B/dom/b-$bnm.want"
    cmp -s "$B/dom/b-$bnm.want" "$B/dom/b-$bnm.out" \
      || fail "[0b] probe $bnm: --bounded accepted at the bound but its success line is not the declared one. want: '$bneed' got: '$(head -c 300 "$B/dom/b-$bnm.out")'"
    [ ! -s "$B/dom/b-$bnm.err" ] \
      || fail "[0b] probe $bnm: --bounded accepted at the bound but wrote to stderr: $(head -c 300 "$B/dom/b-$bnm.err")"
    bacc=$((bacc + 1))
  else
    case "$brc" in
      1) bpfx="usage: " ;;
      2) bpfx="normalize-foh-trace: CORRUPT: " ;;
      3) bpfx="normalize-foh-trace: BOUND VIOLATION: " ;;
      *) fail "[0b] probe $bnm: unhandled expected rc $brc" ;;
    esac
    assert_one_diag_core "0b $bnm bounded" "$B/dom/b-$bnm.out" "$B/dom/b-$bnm.err" \
      "$bneed" "$bpfx"
    brej=$((brej + 1))
  fi
  bran=$((bran + 1))
done < "$B/dom/bprobes.tsv"
read -r bwant bwacc bwrej bwcar bwkinds bwsites < "$B/dom/bplane.txt"
[ "$bran" = "$bwant" ] && [ "$bacc" = "$bwacc" ] && [ "$brej" = "$bwrej" ] \
  || fail "leg [0b] ran $bran bounded probes ($bacc accepted / $brej rejected) but generated $bwant ($bwacc / $bwrej). Every generated probe must be RUN and land on the side the generator declared."
echo "      bounded-delta plane behavioral: $bwant probes over $bwcar measured carriers spanning the END rule's kinds ($bwkinds), $bwacc accepted AT the frozen bounds and $bwrej rejected one tick beyond them (equalities probed on BOTH sides), each at its own declared diagnostic and exit code; all $bwsites die/boundDie sites in the --bounded region are reached by a rejecting probe"

# --- [0e] EMITTER / GRAMMAR AGREEMENT ---------------------------------------
# Tier A+ round-8 BLOCKER 2. `judge-domains.authored.txt`'s whole value is that
# it is a hand-authored claim about something OUTSIDE the judge, so that a
# widening has to falsify a document by hand. Its `X` section named BOTH
# emitters and claimed both "must satisfy every row" -- and the second reviewer
# MEASURED that claim false: the device emitter (foh_dev.c) does not emit this
# arc's two new LAUNCH columns at all. Leg [0n] could never catch that, because
# it compares the authored rows against the two JS programs and never against
# the C emitters, so the emitter paragraph was unenforced prose sitting inside
# an otherwise-enforced file.
#
# Amending the prose would fix the instance. This leg fixes the CLASS, in the
# lane's standing idiom (measured-then-frozen, pinned BOTH directions): the
# LAUNCH field sequence is extracted from the judge's own RE_LAUNCH and from
# each emitter's own concatenated C format string, and the difference must
# equal a PINNED pending set. So:
#   * a THIRD field drifting apart is a hard failure, and
#   * when the registered foh_dev.c patch lands, this leg FAILS until the
#     pending set is emptied in the same change -- the exception cannot rot.
# Order is compared too, not just membership: a reordered emitter would produce
# a line no grammar accepts.
echo "  [0e] emitter/grammar agreement (LAUNCH columns)"
# MEASURED-then-FROZEN. BOTH emitters now satisfy the grammar: the registered
# foh_dev.c patch (.loop/menus-p2-device-workorder-audio.md §A2, docs/MENU-SPEC.md)
# LANDED in the same change that merged this leg -- foh_dev.c's LAUNCH format
# string gained `flashlcancel=%d walljump=%d` after `lcancel=%d`. Both pending
# sets are therefore EMPTY, which is the strongest form of this pin: any column
# either emitter fails to emit is now a hard failure with no exception to hide
# behind. The exception did not rot; it was consumed.
EMIT_PENDING_APP=""
EMIT_PENDING_DEV=""
mkdir -p "$B/emit"
cat > "$B/emit/agree.js" <<'EMITEOF'
const fs = require("fs");
const [jp, appp, devp, pendApp, pendDev] = process.argv.slice(2);
function die(m) { console.error("  [0e] " + m); process.exit(1); }

// The judge's own LAUNCH grammar, read out of its source (never transcribed).
const js = fs.readFileSync(jp, "utf8");
const mj = /const RE_LAUNCH\s*=\s*(\/\^LAUNCH [^\n]*\/);/.exec(js);
if (!mj) die("could not locate RE_LAUNCH in " + jp + " (its shape moved) — this is corrupt evidence, never a pass");
const names = s => {
  const out = [];
  const re = / ([a-z0-9]+)=/g;
  let m;
  while ((m = re.exec(s)) !== null) out.push(m[1]);
  return out;
};
const want = names(mj[1]);
if (want.length < 8) die("RE_LAUNCH yielded only " + want.length + " field names — the extractor no longer understands the grammar");

// A C format string is a run of ADJACENT string literals; concatenate exactly
// as the compiler does, so a field split across two literals is still seen.
function cfmt(path) {
  const src = fs.readFileSync(path, "utf8");
  const at = src.indexOf('"LAUNCH %ld');
  if (at === -1) die("no LAUNCH format string in " + path);
  if (src.indexOf('"LAUNCH %ld', at + 1) !== -1)
    die(path + " has more than one LAUNCH format string; this leg assumes the single emitter site measured by the arc — re-measure and extend it deliberately");
  let i = at, out = "";
  for (;;) {
    while (i < src.length && /\s/.test(src[i])) i++;
    if (src[i] !== '"') break;
    i++;
    for (; i < src.length && src[i] !== '"'; i++) {
      if (src[i] === "\\") { out += src[i + 1]; i++; } else out += src[i];
    }
    if (i >= src.length) die("unterminated string literal in " + path);
    i++;
  }
  return out;
}
let bad = 0;
for (const [path, pend] of [[appp, pendApp], [devp, pendDev]]) {
  const got = names(cfmt(path));
  const pending = pend.split(" ").filter(Boolean);
  const extra = got.filter(f => want.indexOf(f) === -1);
  const missing = want.filter(f => got.indexOf(f) === -1);
  if (extra.length) {
    console.error("  [0e] " + path + " emits LAUNCH column(s) the judge's RE_LAUNCH has no place for: " + extra.join(",") + " — the emitter and the grammar have diverged");
    bad = 1;
  }
  if (missing.join(" ") !== pending.join(" ")) {
    console.error("  [0e] " + path + " omits LAUNCH column(s) [" + missing.join(",") +
      "] but the pinned pending set is [" + pending.join(",") + "]. " +
      (missing.length < pending.length
        ? "The registered emitter patch has landed — EMPTY the pending set in THIS SAME change, and drop the matching caveat from judge-domains.authored.txt's X section."
        : "An emitter has fallen further behind the grammar, or a new column was added to the judge without an emitter — one of the two must move."));
    bad = 1;
  }
  // ORDER, not just membership: the columns the emitter DOES carry must appear
  // in the grammar's order, or it would emit a line nothing accepts.
  const order = want.filter(f => got.indexOf(f) !== -1);
  if (order.join(" ") !== got.join(" ")) {
    console.error("  [0e] " + path + " emits LAUNCH columns in the order [" + got.join(",") +
      "] but the judge requires [" + order.join(",") + "]");
    bad = 1;
  }
}
if (bad) process.exit(1);
const pd = pendDev.split(" ").filter(Boolean);
console.log("      LAUNCH columns: judge requires " + want.length +
  " (" + want.join(",") + "); the host emitter emits all of them; the device emitter " +
  (pd.length
    ? "is short exactly the registered pending set [" + pd.join(",") + "] (work order §A2)"
    : "emits all of them too (the registered work-order §A2 patch has LANDED; the pending set is empty)") +
  ", pinned in BOTH directions");
EMITEOF
node "$B/emit/agree.js" "$FOH/judge-foh-trace.js" "$FOH/foh_app.c" "$FOH/foh_dev.c" \
     "$EMIT_PENDING_APP" "$EMIT_PENDING_DEV" \
  || fail "leg [0e] emitter/grammar agreement failed (see above) — judge-domains.authored.txt's X-section emitter claim is the thing this leg keeps true"

# [0e] TEETH. All on COPIES; the committed emitters are never touched. A pin
# whose failure side has never been executed is a wish, not a pin.
emit_tooth() { # <id> <which: app|dev> <perl-expr> <needle> <label>
  local id="$1" which="$2" expr="$3" needle="$4" label="$5" rc=0
  local d="$B/emit/$id"
  mkdir -p "$d"
  cp "$FOH/foh_app.c" "$d/foh_app.c"; cp "$FOH/foh_dev.c" "$d/foh_dev.c"
  local src="$d/foh_$which.c"
  perl -0pi -e "$expr" "$src"
  cmp -s "$FOH/foh_$which.c" "$src" \
    && fail "[0e] $id — the tooth edit was a NO-OP on the copy (the pattern did not match); a tooth that changes nothing proves nothing"
  node "$B/emit/agree.js" "$FOH/judge-foh-trace.js" "$d/foh_app.c" "$d/foh_dev.c" \
       "$EMIT_PENDING_APP" "$EMIT_PENDING_DEV" > "$d/out" 2>&1 || rc=$?
  [ "$rc" != 0 ] || fail "[0e] $id — the perturbed emitter passed ($label); this leg would not notice: $(cat "$d/out")"
  grep -qF "$needle" "$d/out" \
    || fail "[0e] $id — the perturbed emitter failed (rc $rc) but not at the declared diagnostic '$needle' ($label): $(cat "$d/out")"
}
# ET1 — the DEVICE emitter REGRESSES back off the landed patch. Before the
# patch landed this tooth ran the other way (perturb the copy FORWARD onto the
# patch, and require the leg to demand the pending set be emptied). That
# direction is no longer expressible against a landed patch and an EMPTY
# pending set — ET1b below re-proves it against a synthetic stale set instead,
# so BOTH directions stay executed. What ET1 proves now is the thing the empty
# set newly buys: with no exception left to hide behind, a device emitter that
# drops the two landed columns is a hard failure.
emit_tooth et1 dev \
  's/lcancel=%d flashlcancel=%d walljump=%d /lcancel=%d /' \
  'An emitter has fallen further behind the grammar' \
  'a device emitter that regresses off the landed patch is caught with no exception to absorb it'
# ET1b — THE ROT DIRECTION, preserved. The emitters are NOT touched: the leg is
# re-run over the committed pair with a synthetic NON-EMPTY pending set, i.e.
# exactly the state this change had to leave behind. A stale exception that
# outlives the patch it excuses must fail, and it must fail at the wording that
# tells the next writer what to do.
et1b_d="$B/emit/et1b"; et1b_rc=0
mkdir -p "$et1b_d"
node "$B/emit/agree.js" "$FOH/judge-foh-trace.js" "$FOH/foh_app.c" "$FOH/foh_dev.c" \
     "$EMIT_PENDING_APP" "flashlcancel walljump" > "$et1b_d/out" 2>&1 || et1b_rc=$?
[ "$et1b_rc" != 0 ] || fail "[0e] et1b — a STALE non-empty pending set passed against the caught-up device emitter; the exception could rot silently: $(cat "$et1b_d/out")"
grep -qF 'The registered emitter patch has landed — EMPTY the pending set in THIS SAME change' "$et1b_d/out" \
  || fail "[0e] et1b — the stale pending set failed (rc $et1b_rc) but not at the declared diagnostic ('the registered emitter patch has landed'): $(cat "$et1b_d/out")"
# ET2 — an emitter falls FURTHER behind (a third column goes missing).
emit_tooth et2 app \
  's/turbo=%d lcancel=%d flashlcancel=%d /turbo=%d flashlcancel=%d /' \
  'An emitter has fallen further behind the grammar' \
  'a column dropped from an emitter is caught, not absorbed into the exception'
# ET3 — the columns are all present but in the WRONG ORDER, which membership
# alone cannot see and which would emit a line no grammar accepts.
emit_tooth et3 app \
  's/turbo=%d lcancel=%d flashlcancel=%d /turbo=%d flashlcancel=%d lcancel=%d /' \
  'but the judge requires' \
  'a REORDERED emitter is caught (membership alone would pass it)'
echo "      [0e] teeth: regressed-device, stale-pending-set, column-dropped and column-reordered emitters all fail"

# --- [1] corpus: the committed flow artifacts AS OF THE BASELINE --------------------
# .expect files ARE frozen FOHTRACE1 traces, so they are directly judgeable.
# bash 3.2 (macOS) has no `mapfile` — CLAUDE.md already records this shell's
# quirks, so the corpus is collected with a plain read loop.
ncorpus=0
while IFS= read -r p; do
  [ -n "$p" ] || continue
  git show "$BASE_REF:$p" > "$B/corpus/$(basename "$p")" || fail "cannot extract $BASE_REF:$p"
  ncorpus=$((ncorpus + 1))
done < <(git ls-tree "$BASE_REF" --name-only "$FOH/flows/" | grep -E '\.expect$' | grep -v '\.bstate\.' | sort)
[ "$ncorpus" -ge 5 ] \
  || fail "corpus too small ($ncorpus .expect files at $BASE_REF) — expected the committed flow set"

# --- [2] the intended-movement table (leg 2) --------------------------------
# MEASURED this session, then FROZEN. Format: <flowId>|<flag>|<oldrc>|<newrc>
# A flow/flag pair that moves and is NOT here fails; a pair listed here that
# does NOT move fails as a dead entry. Reason for every row below: owner
# ruling C5 made `menu-top>menu-battle` OFF-GRAPH at FOH_NETPLAY 0, so these
# pre-arc traces (which take that edge) are now correctly refused, and their
# `.expect` files were re-frozen in the same change.
# Format: <flowId>|<flag>|<oldrc>|<newrc>|<needle the NEW stderr must contain>
#
# THE NEEDLE IS THE POINT (review-r12 MAJOR + Tier A+ MAJOR, same finding from
# both reviewers): pinning only oldrc->newrc licenses the pair to differ in ANY
# way, so a broken judge that still died on these traces for some unrelated
# early parse error would keep this table green while the INTENDED reason
# vanished. Each row therefore pins the reason too.
#
# AND THE REASONS ARE NOT ALL THE SAME — I claimed they were, and measuring
# them disproved it. Eight rows are owner ruling C5 (`menu-top>menu-battle`
# became off-graph in the shipped build). **f03-options is NOT C5**: it moved
# because its pre-arc trace REFUSES the token `audio`, and this arc turned the
# audio screen into a real destination, so that refusal token was retired and
# is now unregistered. Same arc, different cause; both are intended.
#
# The flag=0 rows reject under BOTH judges (2->2) but for a different reason —
# the new judge trips on the off-graph edge / retired refusal before it reaches
# the launch-expectation check. A changed refusal REASON is a changed verdict
# for this check's purposes, so it is enumerated rather than waved through as
# "both rejected, close enough".
MOVED="f01-vs-g01|0|2|2|off-graph transition 'menu-top>menu-battle>a'
f01-vs-g01|1|0|2|off-graph transition 'menu-top>menu-battle>a'
f02-cpu-m01|0|2|2|off-graph transition 'menu-top>menu-battle>a'
f02-cpu-m01|1|0|2|off-graph transition 'menu-top>menu-battle>a'
f03-options|0|2|2|unregistered refused entry 'audio'
f03-options|1|0|2|unregistered refused entry 'audio'
f04-nav|0|0|2|off-graph transition 'menu-top>menu-battle>a'
f04-nav|1|2|2|off-graph transition 'menu-top>menu-battle>a'
f05-vs-g03|0|2|2|off-graph transition 'menu-top>menu-battle>a'
f05-vs-g03|1|0|2|off-graph transition 'menu-top>menu-battle>a'"
# The flows this arc did NOT touch — leg 1's identity corpus. Anything not
# listed as moved must be byte-identical, so this set is DERIVED, not typed.
# NEVER `printf … | grep -q` UNDER `set -o pipefail` (Tier A+ round 3
# BLOCKER). `grep -q` exits at the first match without draining the pipe; the
# writer then takes SIGPIPE (141), pipefail makes that the PIPELINE's status,
# and this predicate silently answers "not moved" for a flow that IS moved —
# which turns leg 1 into a false RED and picks a moved flow as the corruption
# BASE. Whether the race is lost depends on the grep implementation and on
# whether the haystack exceeds one write (MOVED is 605 B, PIPE_BUF is 512).
# MEASURED HERE: 0/200 with /usr/bin/grep on bash 3.2.57 arm64, i.e. this host
# does NOT currently lose it — the reviewer's environment did, and a predicate
# whose answer depends on which grep is on PATH is not a predicate. The
# here-string has no pipe, so the class is gone rather than dodged.
is_moved() { # <id> <flag>
  grep -q "^$1|$2|" <<<"$MOVED"
}

# The NORMALIZER's moved set is DIFFERENT from the judge's, and measuring that
# rather than assuming they match is the point. The normalizer does not
# validate the flow graph at all — it validates LINE FORMS. What moved for it
# is the LAUNCH line, which gained the gameplay-options fields this arc added
# (flashlcancel / walljump / tapjump...), so the new normalizer requires the
# new LAUNCH form and refuses the old one. Hence: the four flows that carry a
# VS LAUNCH line. f04-nav has no launch at all and f06/f07 use TLAUNCH, so all
# three are byte-identical — which is why they are NOT listed here even though
# f04-nav IS in the judge's MOVED table.
NORM_MOVED="f01-vs-g01
f02-cpu-m01
f03-options
f05-vs-g03"
is_norm_moved() { # <id> — same no-pipe form as is_moved, same reason
  grep -qx "$1" <<<"$NORM_MOVED"
}

# --- [3] run both judges over every corpus input ----------------------------
# The launch flag is an argv the caller normally derives per flow. We do NOT
# derive it: we run BOTH values for every input. Agreement (or enumerated
# movement) must hold for each, which is stronger than checking only the one
# "correct" flag and costs nothing.
run_pair() { # <label> <trace> <flowId> <flag> -> sets RC_OLD/RC_NEW, cmps
  local label="$1" trace="$2" fid="$3" flag="$4" rcO=0 rcN=0
  node "$B/old/judge-foh-trace.js" "$trace" "$fid" "$flag" \
    > "$B/out/$label.old.out" 2> "$B/out/$label.old.err" || rcO=$?
  node "$FOH/judge-foh-trace.js" "$trace" "$fid" "$flag" \
    > "$B/out/$label.new.out" 2> "$B/out/$label.new.err" || rcN=$?
  RC_OLD="$rcO"; RC_NEW="$rcN"
}
assert_identical() { # <label>
  local label="$1"
  [ "$RC_OLD" = "$RC_NEW" ] \
    || fail "[$label] exit code moved: old=$RC_OLD new=$RC_NEW — the judge changed its VERDICT on a flow this arc did not touch"
  cmp -s "$B/out/$label.old.out" "$B/out/$label.new.out" \
    || fail "[$label] stdout moved on an untouched flow"
  cmp -s "$B/out/$label.old.err" "$B/out/$label.new.err" \
    || fail "[$label] stderr moved on an untouched flow (diff: $(diff "$B/out/$label.old.err" "$B/out/$label.new.err" | head -4 | tr '\n' ' '))"
}

same=0; moved=0; cases=0; accepts=0
for f in "$B"/corpus/*.expect; do
  id="$(basename "$f" .expect)"
  for flag in 0 1; do
    run_pair "acc-$id-$flag" "$f" "$id" "$flag"
    cases=$((cases + 1))
    if is_moved "$id" "$flag"; then
      # leg 2: this pair is ALLOWED to move, but only exactly as pinned.
      want="$(printf '%s\n' "$MOVED" | grep "^$id|$flag|" | head -n 1)"
      wantOld="$(printf '%s' "$want" | cut -d'|' -f3)"
      wantNew="$(printf '%s' "$want" | cut -d'|' -f4)"
      wantWhy="$(printf '%s' "$want" | cut -d'|' -f5)"
      [ "$RC_OLD" = "$wantOld" ] && [ "$RC_NEW" = "$wantNew" ] \
        || fail "[$id flag=$flag] pinned movement is $wantOld->$wantNew, measured $RC_OLD->$RC_NEW (the intended-change table is stale)"
      # the REASON, not just the exit code
      assert_one_diag "$id flag=$flag" "$B/out/acc-$id-$flag.new.out" \
        "$B/out/acc-$id-$flag.new.err" "$wantWhy"
      # DEAD-ENTRY GUARD: a listed pair must actually DIFFER — in exit code
      # or in emitted bytes. A row that no longer changes anything is a stale
      # licence to differ and must be removed, or the table stops meaning
      # anything.
      if [ "$RC_OLD" = "$RC_NEW" ] \
         && cmp -s "$B/out/acc-$id-$flag.old.out" "$B/out/acc-$id-$flag.new.out" \
         && cmp -s "$B/out/acc-$id-$flag.old.err" "$B/out/acc-$id-$flag.new.err"; then
        fail "[$id flag=$flag] listed in MOVED but old and new judge agree byte-for-byte (dead entry — remove it)"
      fi
      moved=$((moved + 1))
    else
      assert_identical "acc-$id-$flag"
      same=$((same + 1))
      [ "$RC_OLD" = 0 ] && accepts=$((accepts + 1)) || true
    fi
  done
done
[ "$accepts" -ge 1 ] \
  || fail "no untouched-flow input was ACCEPTED by both judges — leg 1 is not exercising the accept path, so its agreement is vacuous"
[ "$moved" = "$(printf '%s\n' "$MOVED" | grep -c '|')" ] \
  || fail "MOVED lists $(printf '%s\n' "$MOVED" | grep -c '|') pairs but $moved were exercised (a listed flow is missing from the corpus)"
echo "  [1] untouched flows: $same old/new pairs byte-identical ($accepts accepted by both)"
echo "  [2] intended movement: $moved pinned verdict changes, all exactly as enumerated"

# --- [4] the REJECT path, on an UNTOUCHED flow (leg 1 continued) -----------
# A regression that only feeds valid traces proves the judge still says yes and
# says NOTHING about whether it still says no — which is the direction that
# matters, because a judge that quietly stopped rejecting corruption keeps
# printing OK while proving less. Fixtures are built from an ARC-UNTOUCHED
# flow so any divergence here is a real loosening, never the C5 graph change.
perturb() { # <name> <src> <sed-expr>
  sed "$3" "$2" > "$B/corpus-bad/$1.txt"
  cmp -s "$2" "$B/corpus-bad/$1.txt" \
    && grammar_die "corruption fixture '$1' did not change the file (dead fixture)"
  return 0
}
mkdir -p "$B/corpus-bad"
BASE=""; BASEID=""
for f in "$B"/corpus/*.expect; do
  id="$(basename "$f" .expect)"
  if ! is_moved "$id" 0 && ! is_moved "$id" 1; then BASE="$f"; BASEID="$id"; break; fi
done
[ -n "$BASE" ] \
  || fail "no arc-untouched flow in the corpus to build corruption fixtures from (every flow moved — leg 1 cannot be evaluated)"
# awk, not sed: BSD sed (macOS) has no GNU `0,/re/` first-match address, and
# it fails SILENTLY into a no-op fixture — which the dead-fixture guard caught,
# and which is exactly the class this whole check exists to prevent.
perturb_awk() { # <name> <src> <awk-prog>
  awk "$3" "$2" > "$B/corpus-bad/$1.txt"
  cmp -s "$2" "$B/corpus-bad/$1.txt" \
    && grammar_die "corruption fixture '$1' did not change the file (dead fixture)"
  return 0
}
# 1. T-chain break: the 2nd transition departs a screen the 1st did not reach.
perturb_awk corrupt-edge "$BASE" 'BEGIN{n=0} /^T /{n++; if(n==2){$3="css"}} {print}'
# 2. frame monotonicity: the 1st transition jumps far ahead of its successors.
perturb_awk corrupt-frame "$BASE" 'BEGIN{n=0} /^T /{n++; if(n==1){$2=999999}} {print}'
# 3. the END line removed entirely.
perturb corrupt-end "$BASE" '/^END /d'
# 4. an unknown line appended.
{ cat "$BASE"; printf 'GARBAGE not-a-real-line\n'; } > "$B/corpus-bad/corrupt-garbage.txt"
# 5. the header claims a different flow than the judge is told.
perturb_awk corrupt-header "$BASE" 'NR==1{$0="FOHTRACE1 flow=bogus-id"} {print}'
rejects=0
for name in corrupt-edge corrupt-frame corrupt-end corrupt-garbage corrupt-header; do
  refused=0
  for flag in 0 1; do
    run_pair "rej-$name-$flag" "$B/corpus-bad/$name.txt" "$BASEID" "$flag"
    assert_identical "rej-$name-$flag"
    cases=$((cases + 1))
    [ "$RC_OLD" != 0 ] && refused=1 || true
  done
  [ "$refused" = 1 ] \
    || grammar_die "corruption fixture '$name' was ACCEPTED by the OLD judge on both flags — the fixture is not corrupt, so its agreement proves nothing"
  rejects=$((rejects + 1))
done
echo "  [3] reject path (untouched flow): $rejects corruption classes, old/new byte-identical refusals"

# --- [5] leg 3: the NEW judge accepts the arc's RE-FROZEN expects -----------
# This is what separates "the flow graph changed" from "the judge is broken".
# For every flow whose verdict moved, the CURRENT `.expect` (re-frozen in this
# same change) must be ACCEPTED by the new judge. Old trace refused + new trace
# accepted, by one judge, is the signature of an intended graph change.
# UNIQUE flows, CORRECT flag pinned, opposite flag must REJECT (review-r12
# MINOR). The earlier version iterated flow x flag and passed if EITHER flag
# accepted — so a judge that accepted only the WRONG launch expectation passed,
# and the printed count double-counted five flows as ten. The correct flag is
# DERIVED, not typed: a flow whose re-frozen expect carries a LAUNCH/TLAUNCH
# line must be judged with launch=1, and one without it with launch=0. Both
# directions are asserted, so launch-expectation discipline is pinned here too.
reaccept=0
seen_ids=""   # set -u: initialise before the case-glob read below
while IFS='|' read -r id _rest; do
  [ -n "$id" ] || continue
  case " $seen_ids " in *" $id "*) continue;; esac
  seen_ids="$seen_ids $id"
  cur="$FOH/flows/$id.expect"
  [ -f "$cur" ] || fail "leg 3: $cur missing (a moved flow lost its re-frozen expect)"
  if grep -qE '^T?LAUNCH ' "$cur"; then want=1; other=0; else want=0; other=1; fi
  rc=0
  node "$FOH/judge-foh-trace.js" "$cur" "$id" "$want" > /dev/null 2>"$B/out/re-$id.err" || rc=$?
  [ "$rc" = 0 ] \
    || fail "leg 3: the NEW judge REJECTS the arc's own re-frozen $cur at its CORRECT launch flag $want — the verdict movement is judge breakage, not the intended graph/refusal change (got: $(head -c 200 "$B/out/re-$id.err"))"
  rc=0
  node "$FOH/judge-foh-trace.js" "$cur" "$id" "$other" > /dev/null 2>&1 || rc=$?
  [ "$rc" != 0 ] \
    || fail "leg 3: the NEW judge ALSO accepts $cur at the WRONG launch flag $other — the launch expectation is not enforced, so leg 3 would pass a judge that ignores it"
  reaccept=$((reaccept + 1))
done <<EOF
$MOVED
EOF
echo "  [4] re-frozen expects: $reaccept unique moved flows accepted at their correct launch flag, rejected at the other"

# --- [5b] NEGATIVE CORPUS FOR THE *NEW* GRAMMAR ----------------------------
# review-r12 BLOCKER + Tier A+ second-reviewer BLOCKER. BOTH reviewers found
# this independently, and the second reviewer VERIFIED that an S-domain
# loosening sails straight through `JUDGE REGRESSION OK`.
#
# THE HOLE, stated plainly: legs 1-4 prove the judge behaves identically on
# flows the arc did NOT touch, and that its movement on the five it DID touch
# is enumerated. None of that exercises the rules this arc ADDED. Delete the
# SFIELD_SCREENS enforcement, widen soundsvol's domain, or drop a required
# LAUNCH field, and every leg above stays green — the untouched flows never
# emit those forms, and the moved rows only pin exit codes. Identity-on-
# untouched-accepts is necessary and nowhere near sufficient for a whitelist
# judge with no checksum backstop.
#
# So: fixtures perturb a CURRENT (re-frozen) expect that actually carries the
# new surface, and each must be REJECTED by the NEW judge AT ITS OWN
# DIAGNOSTIC — proving the new rules are ENFORCED, not merely present. These
# are new-form traces, so the OLD judge has no opinion worth comparing; the
# claim here is enforcement, not identity.
echo "  [5b] negative corpus for the new grammar:"
NEGSRC="$FOH/flows/f03-options.expect"
[ -f "$NEGSRC" ] || fail "negative corpus source $NEGSRC missing"
# Each fixture must have a target to hit, or it proves nothing.
for tok in soundsvol musicvol flashlcancel walljump; do
  grep -q "^S [0-9]* $tok " "$NEGSRC" \
    || fail "negative corpus: $NEGSRC does not emit '$tok' — the fixture set would not exercise the new S surface"
done
grep -qE '^LAUNCH .* flashlcancel=[01] walljump=[01] ' "$NEGSRC" \
  || fail "negative corpus: $NEGSRC has no new-form LAUNCH line (the LAUNCH fixtures would prove nothing)"
# The leading-zero fixtures need one target per frame-number anchor. f03-options
# carries T / SHOT / END lines; it carries NO `refused` entry, so RE_S_REF's
# anchor is unreachable from it and that fixture uses f04-nav instead.
for pat in '^T [0-9]' '^SHOT [0-9]' '^END [0-9]* transitions='; do
  grep -qE "$pat" "$NEGSRC" \
    || fail "negative corpus: $NEGSRC has no line matching '$pat' — the leading-zero fixture for that line form would have nothing to perturb"
done
grep -qE '^S [0-9]+ refused ' "$FOH/flows/f04-nav.expect" \
  || fail "negative corpus: f04-nav.expect has no 'refused' line — the RE_S_REF leading-zero fixture would have nothing to perturb"
# EVERY BASE MUST BE ACCEPTED AS COMMITTED at its declared launch flag. A
# fixture built on a base the judge already rejects would "fail" for the base's
# own reason and prove nothing about the rule it names.
mkdir -p "$B/neg"
for base in f03-options:1 f04-nav:0; do
  bf="${base%%:*}"; bl="${base##*:}"; rc=0
  node "$FOH/judge-foh-trace.js" "$FOH/flows/$bf.expect" "$bf" "$bl" \
    > /dev/null 2> "$B/neg/base-$bf.err" || rc=$?
  [ "$rc" = 0 ] \
    || fail "negative-corpus base $bf (launch flag $bl) is REJECTED by the new judge as committed — every fixture built on it would die for the base's reason, not its own ($(head -c 200 "$B/neg/base-$bf.err"))"
done
# name|sed-expr|the RULE the diagnostic must name
#
# NEEDLES NAME THE RULE, NOT THE FIELD — and this was a real defect in the
# first version of this leg, caught by re-running the loosening experiment the
# Tier A+ reviewer described. A needle of just "soundsvol" matches ANY
# diagnostic that quotes the offending line, so a fixture rejected for an
# unrelated reason still counted as proof. Every needle below is the rule's own
# wording ("outside pinned domain", "cannot write it", "matches no FOHTRACE1
# form", "off-graph transition"), so a fixture that dies for the wrong reason
# now FAILS.
#
# TARGETS ARE CHOSEN FROM WHAT IS ACTUALLY REACHABLE, which took measuring.
# RE_S_NUM's value alphabet is `(-1|10|[0-9])`, so `soundsvol 11` never reaches
# SVAL_DOM at all — it dies at the line form. That means for soundsvol/musicvol
# (domain [0,10]) the ONLY regex-valid domain violation is -1, and their UPPER
# bound is REDUNDANT with the alphabet. The fields whose domain is genuinely
# narrower than the alphabet (turbo/flashlcancel/walljump [0,1], lcancel [0,2])
# are what actually prove SVAL_DOM is enforced, so they are all covered.
#
# EVERY FRAME-NUMBER ANCHOR HAS ITS OWN LEADING-ZERO FIXTURE (codex r15
# MAJOR). `(0|[1-9][0-9]*)` is written out separately in RE_T, RE_S_NUM,
# RE_S_REF, RE_SHOT, RE_LAUNCH and RE_END (twice — frame AND transitions), so
# relaxing it to `[0-9]+` in ANY ONE of them was a loosening that two fixtures
# could not see. There is now one fixture per occurrence. They are cheap and
# they are exactly the kind of rule nobody would have thought to fixture.
NEG='form-soundsvol-11|s/^S \([0-9]*\) soundsvol .*/S \1 soundsvol 11/|matches no FOHTRACE1 form
domain-soundsvol-neg|s/^S \([0-9]*\) soundsvol .*/S \1 soundsvol -1/|outside the pinned domain of soundsvol
domain-musicvol-neg|s/^S \([0-9]*\) musicvol .*/S \1 musicvol -1/|outside the pinned domain of musicvol
domain-turbo-2|s/^S \([0-9]*\) turbo .*/S \1 turbo 2/|outside the pinned domain of turbo
domain-lcancel-3|s/^S \([0-9]*\) lcancel 2$/S \1 lcancel 3/|outside the pinned domain of lcancel
domain-flashlcancel-2|s/^S \([0-9]*\) flashlcancel .*/S \1 flashlcancel 2/|outside the pinned domain of flashlcancel
domain-walljump-2|s/^S \([0-9]*\) walljump .*/S \1 walljump 2/|outside the pinned domain of walljump
domain-tapjump-2|s/^S \([0-9]*\) tapjump1 .*/S \1 tapjump1 2/|outside the pinned domain of tapjump1
screen-soundsvol-on-gameplay|s/^S \([0-9]*\) turbo .*/S \1 soundsvol 5/|cannot write it
screen-musicvol-on-gameplay|s/^S \([0-9]*\) turbo .*/S \1 musicvol 5/|cannot write it
screen-walljump-on-audio|s/^S \([0-9]*\) musicvol .*/S \1 walljump 1/|cannot write it
screen-flashlcancel-on-audio|s/^S \([0-9]*\) musicvol .*/S \1 flashlcancel 1/|cannot write it
refusal-wrong-screen|s/^S \([0-9]*\) turbo .*/S \1 refused targetbuilder/|cannot refuse it
launch-missing-flashlcancel|s/ flashlcancel=[01]//|matches no FOHTRACE1 form
launch-missing-walljump|s/ walljump=[01]//|matches no FOHTRACE1 form
launch-domain-walljump-2|s/ walljump=[01] / walljump=2 /|matches no FOHTRACE1 form
launch-domain-flashlcancel-2|s/ flashlcancel=[01] / flashlcancel=2 /|matches no FOHTRACE1 form
edge-offgraph-nearmiss|s/^T \([0-9]*\) options-audio \([a-z-]*\) b$/T \1 options-audio css b/|off-graph transition
edge-controls-nearmiss|s/^T \([0-9]*\) menu-options \([a-z-]*\) a$/T \1 menu-options controls-controller a/|off-graph transition
form-s-leadingzero|s/^S \([0-9]*\) turbo /S 0\1 turbo /|matches no FOHTRACE1 form
form-launch-leadingzero|s/^LAUNCH \([0-9]*\) /LAUNCH 0\1 /|matches no FOHTRACE1 form
form-t-leadingzero|s/^T \([0-9]*\) /T 0\1 /|matches no FOHTRACE1 form
form-shot-leadingzero|s/^SHOT \([0-9]*\) /SHOT 0\1 /|matches no FOHTRACE1 form
form-end-leadingzero|s/^END \([0-9]*\) /END 0\1 /|matches no FOHTRACE1 form
form-end-transitions-leadingzero|s/^END \([0-9]*\) transitions=/END \1 transitions=0/|matches no FOHTRACE1 form
form-sref-leadingzero|s/^S \([0-9]*\) refused /S 0\1 refused /|matches no FOHTRACE1 form|f04-nav|0'
negs=0
while IFS= read -r row; do
  [ -n "$row" ] || continue
  nm="$(printf '%s' "$row" | cut -d'|' -f1)"
  ex="$(printf '%s' "$row" | cut -d'|' -f2)"
  need="$(printf '%s' "$row" | cut -d'|' -f3)"
  # Optional fields 4/5 = an ALTERNATE base flow + its launch flag, for rules
  # whose target line f03-options does not carry. Both bases are accepted as
  # committed (asserted above), so a rejection here is the fixture's own.
  nflow="$(printf '%s' "$row" | cut -d'|' -f4)"
  nflag="$(printf '%s' "$row" | cut -d'|' -f5)"
  [ -n "$nflow" ] || { nflow=f03-options; nflag=1; }
  nsrc="$FOH/flows/$nflow.expect"
  [ -f "$nsrc" ] || fail "negative fixture '$nm' names base flow '$nflow', but $nsrc does not exist"
  sed "$ex" "$nsrc" > "$B/neg/$nm.txt"
  cmp -s "$nsrc" "$B/neg/$nm.txt" \
    && grammar_die "negative fixture '$nm' did not change the file (dead fixture — the perturbation missed its target line)"
  rc=0
  node "$FOH/judge-foh-trace.js" "$B/neg/$nm.txt" "$nflow" "$nflag" \
    > "$B/neg/$nm.out" 2> "$B/neg/$nm.err" || rc=$?
  [ "$rc" != 0 ] \
    || fail "negative fixture '$nm' was ACCEPTED by the new judge — a rule this arc added is NOT ENFORCED (the false-green both reviewers found)"
  assert_one_diag "neg $nm" "$B/neg/$nm.out" "$B/neg/$nm.err" "$need"
  negs=$((negs + 1))
done <<EOF
$NEG
EOF
[ "$negs" = 26 ] || fail "negative corpus ran $negs fixtures, want 26 (the table lost a row)"
# ANCHORED FULL-TEXT PIN over every diagnostic asserted above (movement rows
# from leg [2] + all 26 negative fixtures, in deterministic order). The
# per-row checks anchor the PREFIX and the declared RULE; this pins the
# remaining bytes — line numbers and offending text — as ONE hash, so any
# drift in any diagnostic is a deliberate re-freeze rather than 31 typed
# literals that churn whenever a fixture line moves. Same idiom as leg [0g].
DIAG_SHA=3e361944f548794eea1259dc35d68aa47e51f22f5dc74c9a2c246af8084a3164
dgot="$(shasum -a 256 "$DIAGACC" | cut -d' ' -f1)"
[ "$dgot" = "$DIAG_SHA" ] \
  || fail "the asserted diagnostics hash to $dgot, pinned $DIAG_SHA — a judge diagnostic changed its text, line number, or ORDER. Every row still passed its own anchored check, so this is the drift those checks cannot see. Re-pin DIAG_SHA in the SAME change and say why. (dump: $DIAGACC)"
# POSITIVE CONTROL: the in-domain MAXIMUM must be ACCEPTED. Without this, a
# judge that rejected everything would satisfy all 14 negatives above, and the
# whole leg would be vacuous. soundsvol 10 == master volume 1.0, the value a
# user reaching the top of the rail actually produces.
sed 's/^S \([0-9]*\) soundsvol .*/S \1 soundsvol 10/' "$NEGSRC" > "$B/neg/ctl-max.txt"
rc=0
node "$FOH/judge-foh-trace.js" "$B/neg/ctl-max.txt" f03-options 1 \
  > "$B/neg/ctl-max.out" 2> "$B/neg/ctl-max.err" || rc=$?
[ "$rc" = 0 ] \
  || fail "positive control: the judge REJECTS soundsvol 10 (the in-domain maximum, i.e. master volume 1.0) — the negatives above would be satisfied by a judge that rejects everything (got: $(head -c 200 "$B/neg/ctl-max.err"))"
echo "      $negs negative fixtures bite at their own rule (S domains, per-screen fields incl. refusals, LAUNCH form, the walljump and flashlcancel LAUNCH domains -- the two LAUNCH fields fixtured here; all 16 authored LAUNCH/TLAUNCH fields are judged BEHAVIORALLY by leg [0n]'s generated launch-plane probes -- every frame-number anchor, off-graph near-misses) + in-domain max accepted; these fixtures prove the DIAGNOSTICS are real. They are not the coverage instrument: leg [0g] NOTIFIES on any change to any rule (fixtured or not), and leg [0n] is what judges whether the S domains are RIGHT"

# --- [6] the normalizer -----------------------------------------------------
# It does NOT get a [5b]-class per-rule corpus of its own; its line forms are
# frozen by leg [0g] and its reject surface by the corruption fixtures below.
# MODE 1 (elide) is the DEVICE leg's structural comparand: every device FOH
# verdict is a comparison of elided traces, so a silent drift here moves every
# one of them at once. It validates the same grammar as the judge, so it moves
# on exactly the same five C5-affected flows — enumerated, not excused.
norms=0; normmoved=0
for f in "$B"/corpus/*.expect; do
  id="$(basename "$f" .expect)"
  rcO=0; rcN=0
  node "$B/old/normalize-foh-trace.js" "$f" "$B/out/norm-$id.old" >"$B/out/norm-$id.old.log" 2>&1 || rcO=$?
  node "$FOH/normalize-foh-trace.js" "$f" "$B/out/norm-$id.new" >"$B/out/norm-$id.new.log" 2>&1 || rcN=$?
  if is_norm_moved "$id"; then
    # A C5-affected flow: the new normalizer must REFUSE the pre-arc trace
    # (same grammar as the judge) and ACCEPT the arc's re-frozen one.
    # The movement must be old-ACCEPT -> new-REJECT. Asserting only "new
    # rejects" would be satisfied by a pair that BOTH rejected all along, which
    # says nothing about the LAUNCH grammar change (review-r12 MAJOR).
    [ "$rcO" = 0 ] \
      || fail "[norm-$id] listed in NORM_MOVED but the OLD normalizer did NOT accept its own pre-arc trace (rc $rcO) — the row is not describing a real old-accept -> new-reject movement"
    [ "$rcN" != 0 ] \
      || fail "[norm-$id] listed in NORM_MOVED but the new normalizer still ACCEPTS the pre-arc trace (dead entry, or the LAUNCH grammar change was reverted)"
    rc=0
    node "$FOH/normalize-foh-trace.js" "$FOH/flows/$id.expect" "$B/out/norm-$id.cur" >/dev/null 2>&1 || rc=$?
    [ "$rc" = 0 ] \
      || fail "[norm-$id] the new normalizer REJECTS the arc's own re-frozen expect (rc $rc) — the device leg could never pass"
    # ELIDED BYTES PINNED against an INDEPENDENT reproduction of the documented
    # rule (review-r13 BLOCKER): "the frame field of every event line replaced
    # by the literal 'F', and NOTHING else". Accepting was never enough — mode 1
    # could start discarding LAUNCH payload fields and still exit 0, silently
    # weakening every DEVICE structural comparison that consumes this output.
    sed -E 's/^(T|S|SHOT|LAUNCH|TLAUNCH|END) [0-9]+ /\1 F /' \
      "$FOH/flows/$id.expect" > "$B/out/norm-$id.indep"
    cmp -s "$B/out/norm-$id.cur" "$B/out/norm-$id.indep" \
      || fail "[norm-$id] the elided output does NOT match an independent application of the documented elision rule (frame -> 'F', nothing else) — mode 1 is dropping or rewriting content the device legs compare (diff: $(diff "$B/out/norm-$id.indep" "$B/out/norm-$id.cur" | head -4 | tr '\n' ' '))"
    normmoved=$((normmoved + 1))
  else
    [ "$rcO" = "$rcN" ] \
      || fail "[norm-$id] normalizer exit code moved on an untouched flow: old=$rcO new=$rcN"
    if [ "$rcO" = 0 ]; then
      cmp -s "$B/out/norm-$id.old" "$B/out/norm-$id.new" \
        || fail "[norm-$id] ELIDED OUTPUT MOVED on an untouched flow — every device FOH structural comparison would shift silently"
      norms=$((norms + 1))
    fi
  fi
done
[ "$norms" -ge 1 ] \
  || fail "no untouched-flow normalizer output was compared (leg is vacuous)"
# INVENTORY: every NORM_MOVED row must have been exercised, so a row naming a
# flow that is not in the corpus cannot sit there as a silent licence.
nm_rows="$(printf '%s\n' "$NORM_MOVED" | grep -c '[a-z]')"
[ "$normmoved" = "$nm_rows" ] \
  || fail "NORM_MOVED lists $nm_rows flows but $normmoved were exercised (a listed flow is missing from the corpus — dead row)"
# The normalizer's REJECT path (review-r12 MAJOR): the leg above only fed it
# VALID traces, so a grammar/domain loosening in the normalizer could not show.
# Same fixtures the judge's reject path uses, same byte-identity requirement.
normrej=0
for name in corrupt-edge corrupt-frame corrupt-end corrupt-garbage corrupt-header; do
  rcO=0; rcN=0
  node "$B/old/normalize-foh-trace.js" "$B/corpus-bad/$name.txt" "$B/out/nrej-$name.old" \
    > "$B/out/nrej-$name.old.log" 2>&1 || rcO=$?
  node "$FOH/normalize-foh-trace.js" "$B/corpus-bad/$name.txt" "$B/out/nrej-$name.new" \
    > "$B/out/nrej-$name.new.log" 2>&1 || rcN=$?
  [ "$rcO" = "$rcN" ] \
    || fail "[nrej-$name] normalizer exit code moved on a corruption fixture: old=$rcO new=$rcN — the normalizer's reject surface changed"
  cmp -s "$B/out/nrej-$name.old.log" "$B/out/nrej-$name.new.log" \
    || fail "[nrej-$name] normalizer diagnostic moved on a corruption fixture (diff: $(diff "$B/out/nrej-$name.old.log" "$B/out/nrej-$name.new.log" | head -4 | tr '\n' ' '))"
  # If both accepted, the ELIDED BYTES must match too; if both refused, neither
  # may have left an output file behind.
  if [ "$rcO" = 0 ]; then
    cmp -s "$B/out/nrej-$name.old" "$B/out/nrej-$name.new" \
      || fail "[nrej-$name] elided output moved on a corruption fixture"
  else
    # The comment above was an unasserted claim until review-r16 MINOR. A
    # normalizer that refuses a trace but still writes its output path would
    # hand the device leg a PARTIAL elided file to compare against — a
    # rejection that is only advisory. Both sides must leave nothing behind.
    [ ! -e "$B/out/nrej-$name.old" ] && [ ! -e "$B/out/nrej-$name.new" ] \
      || fail "[nrej-$name] a REFUSED normalizer run left an output file behind (old: $([ -e "$B/out/nrej-$name.old" ] && echo present || echo absent), new: $([ -e "$B/out/nrej-$name.new" ] && echo present || echo absent)) — a refusal that still publishes output is advisory, and the device leg would compare partial bytes"
  fi
  normrej=$((normrej + 1))
done
[ "$normrej" = 5 ] || fail "normalizer reject path ran $normrej fixtures, want 5"
echo "  [5] normalizer: $norms untouched elided outputs byte-identical; $normmoved LAUNCH-grammar flows old-accept->new-reject; $normrej corruption fixtures byte-identical"

echo "JUDGE REGRESSION OK (corpus=$ncorpus pairs=$cases identical=$same moved=$moved rejects=$rejects negs=$negs reaccept=$reaccept norms=$norms normmoved=$normmoved normrej=$normrej tables=$gtab)"
