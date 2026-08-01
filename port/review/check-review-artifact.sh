#!/usr/bin/env bash
# port/review/check-review-artifact.sh — the done-check for fix_plan R4.
#
#   bash port/review/check-review-artifact.sh
#     -> REVIEW ARTIFACT TEETH OK (N/N bit), exit 0
#
# WHAT IT PROVES CHANGED ON 2026-07-31 (owner ruling: keep the PRODUCER, drop
# the JUDGE'S AUTHORITY). `port/review/arc-report.sh` is a DIAGNOSTIC — it has
# no "closed" output and decides nothing — so these teeth no longer prove that
# a judge "cannot be talked into closed". They prove two things instead:
#
#   A. EVIDENCE DEFECTS FAIL CLOSED (`expect_defect`). Bytes that cannot be
#      read as evidence are refused, for the right reason:
#        * a quoted FOREIGN verdict at EOF — against the real
#          `.loop/review-c25-1-codex-VOID.log` (kept at
#          port/review/specimens/foreign-verdict-at-eof.log), as-is and with a
#          harness terminator spliced in at the true end of the run so the
#          paste sits after it. The second refuses even though its artifact is
#          internally perfect (correct sha, byte and line counts, valid
#          self-seal) — the refusal comes from the STRUCTURE, not from a
#          broken hash. A third tooth shows the DECISION CHANNEL catching the
#          same shape independently.
#        * a NUL-contaminated log — against the real
#          `.loop/mexit-r4-device-target.log` (44.4% NUL, reads
#          `DEVICE TARGET CONFORMS`, exited `TARGET_RC=1`), both when the
#          artifact lies about the NUL count and when it declares it honestly.
#        * cross-wired bundles, re-labelled rounds, version skew, forged VOID
#          reasons, an unresolvable §11 fallback-basis, and the rest.
#   B. THE SITUATIONS A JUDGE USED TO REFUSE ON ARE STILL VISIBLE
#      (`expect_observed`). Where a closure rule was deleted, the report must
#      still DISCLOSE the fact the human now judges: a §11 pair with one
#      reviewer, a Tier A+ arc with no second opinion and no regression, a cap
#      claiming more rounds than exist, a drifted scope, a failed regression,
#      an arc in which every round is VOID, and — for the two rules deleted as
#      MEASURED UNSOUND — the full round timeline and the full basis record,
#      each with the tool's own statement that it does not decide them.
#      A tooth that could be satisfied by silence would be no tooth at all.
#
# Every tooth is ONE perturbation away from a green baseline, and every
# perturbation is followed by a restore + a re-assert that the baseline is
# green again — so a tooth that "passes" because the baseline was already
# broken cannot hide (the OFF control is measured, not assumed).
#
# Nothing here touches a tracked file: all artifacts live under a per-run
# directory in the gitignored `.loop/`, removed on exit.

set -euo pipefail

ROOT="$(git rev-parse --show-toplevel)"
cd "$ROOT"

HARNESS='port/review/review-harness.sh'
REPORT='port/review/arc-report.sh'
SPECDIR='port/review/specimens'

RUN=".loop/r4-teeth-$$"
export MLFK_ARC_DIR="$RUN/arc"
PRISTINE="$RUN/pristine"
WORK="$RUN/work"
SCOPEDIR="$RUN/scope"
OUT="$RUN/out.txt"
CLEANED=0
cleanup() {
  if [ "$CLEANED" = "1" ]; then return 0; fi
  CLEANED=1
  rm -rf "$RUN"
}
# EXIT runs cleanup; the signal traps clean AND exit, so a signal can never
# leave the shell running past a cleanup that already fired.
trap cleanup EXIT
trap 'cleanup; exit 130' INT
trap 'cleanup; exit 143' TERM
trap 'cleanup; exit 129' HUP

rm -rf "$RUN"
mkdir -p "$MLFK_ARC_DIR" "$PRISTINE" "$WORK" "$SCOPEDIR"

PASS=0
FAILED=0
fail() { printf 'TOOTH FAILED [%s]: %s\n' "$1" "$2"; FAILED=$((FAILED + 1)); }

# ------------------------------------------------------------ specimen pins
# The fixtures are evidence: if they rot, every tooth below silently weakens.
( cd "$SPECDIR" && shasum -a 256 -c SHA256SUMS ) > /dev/null \
  || { printf 'REFUSED: %s/SHA256SUMS does not verify\n' "$SPECDIR"; exit 2; }
SPEC_FOREIGN="$SPECDIR/foreign-verdict-at-eof.log"
SPEC_NUL="$SPECDIR/nul-corrupt-rc1.log"
# Structural pins on the specimens themselves, measured 2026-07-30. These are
# the properties the teeth depend on; a specimen that no longer has them would
# make the teeth vacuous.
[ "$(wc -l < "$SPEC_FOREIGN" | tr -d ' ')" = "4246" ] \
  || { printf 'REFUSED: foreign specimen is not 4246 lines\n'; exit 2; }
[ "$(LC_ALL=C grep -a -n '^VERDICT: NO-GO$' "$SPEC_FOREIGN" | tail -1 | cut -d: -f1)" = "4063" ] \
  || { printf 'REFUSED: foreign specimen: real terminal NO-GO is not at line 4063\n'; exit 2; }
[ "$(LC_ALL=C grep -a -n '^VERDICT: GO$' "$SPEC_FOREIGN" | tail -1 | cut -d: -f1)" = "4094" ] \
  || { printf 'REFUSED: foreign specimen: the pasted foreign GO is not at line 4094\n'; exit 2; }
[ "$(LC_ALL=C grep -a -c '^REVIEWER_RC \|^HARNESS-EOR ' "$SPEC_FOREIGN" | tr -d ' ')" = "0" ] \
  || { printf 'REFUSED: foreign specimen already carries harness terminators\n'; exit 2; }
[ "$(LC_ALL=C tr -dc '\000' < "$SPEC_NUL" | wc -c | tr -d ' ')" = "7835" ] \
  || { printf 'REFUSED: NUL specimen no longer carries 7835 NUL bytes\n'; exit 2; }
[ "$(wc -c < "$SPEC_NUL" | tr -d ' ')" = "17627" ] \
  || { printf 'REFUSED: NUL specimen is not 17627 bytes\n'; exit 2; }

# ------------------------------------------------------------ synthetic revs
# The reviewer is stubbed so the teeth are hermetic and free; every artifact
# they produce is MARKED synthetic (reviewer-cmd-sha256), and one tooth proves
# the judge refuses such an artifact unless it is told to allow it.
# argv: $1 = archived prompt envelope, $2 = decision-channel path.
mkrev() { # mkrev <file> <rc> <decision-channel line|-> <transcript lines...>
  local f="$1" rc="$2" dv="$3"; shift 3
  {
    printf '#!/usr/bin/env bash\n'
    for l in "$@"; do printf 'printf "%%s\\n" %s\n' "'$l'"; done
    if [ "$dv" != '-' ]; then
      printf 'printf "%%s\\n%%s\\n" %s %s > "$2"\n' "'Final message.'" "'$dv'"
    fi
    printf 'exit %s\n' "$rc"
  } > "$f"
  chmod +x "$f"
}
mkrev "$WORK/rev-go.sh"       0 'VERDICT: GO'      'reviewing the change' '## Findings' 'none' 'VERDICT: GO'
mkrev "$WORK/rev-nogo.sh"     0 'VERDICT: NO-GO'   'reviewing the change' '## Findings' '[M] a finding' 'VERDICT: NO-GO'
mkrev "$WORK/rev-go-rc1.sh"   1 'VERDICT: GO'      'reviewing the change' 'VERDICT: GO'
# adorned in BOTH channels: PROCESS §3/C11 — an adorned verdict is no verdict
mkrev "$WORK/rev-adorned.sh"  0 '**VERDICT: GO**'  'reviewing the change' '**VERDICT: GO**'
mkrev "$WORK/rev-wedged.sh"   7 '-'                'reviewing the change' 'tool call died mid-flight'
mkrev "$WORK/rev-mismatch.sh" 0 'VERDICT: NO-GO'   'reviewing the change' 'VERDICT: GO'
# a TRUNCATED final write: the decision file ends mid-verdict, no newline.
# The harness must never "repair" this into a valid line.
{ printf '#!/usr/bin/env bash\n'
  printf 'printf "%%s\\n" "reviewing the change"\n'
  printf 'printf "%%s\\n" "VERDICT: GO"\n'
  printf 'printf "%%s\\n%%s" "Final message." "VERDICT: G" > "$2"\n'
  printf 'exit 0\n'; } > "$WORK/rev-dec-trunc.sh"
chmod +x "$WORK/rev-dec-trunc.sh"
# content AFTER the verdict in the decision channel
{ printf '#!/usr/bin/env bash\n'
  printf 'printf "%%s\\n" "reviewing the change"\n'
  printf 'printf "%%s\\n" "VERDICT: GO"\n'
  printf 'printf "%%s\\n%%s\\n" "VERDICT: GO" "...and some trailing prose" > "$2"\n'
  printf 'exit 0\n'; } > "$WORK/rev-dec-junk.sh"
chmod +x "$WORK/rev-dec-junk.sh"

# THE LAUNDERING SHAPE, sourced one step earlier than the live specimen: the
# reviewer's OWN output carries its real NO-GO and then QUOTES a foreign log
# whose last line is an anchored GO. Inside the region, before the terminator,
# hashing correctly — the nonce terminator cannot see it and, on a grok/opus5
# round, there is no decision channel that can. Writes NO decision file
# (argv $2 untouched): transcript-decided rounds have none.
mkrev "$WORK/rev-launder.sh" 0 '-' 'reviewing the change' '## Findings' \
  '[H] I found a blocking defect.' 'VERDICT: NO-GO' '' \
  'For reference, here is the transcript of the earlier round:' \
  '----- begin quoted foreign log -----' 'no issues found' 'VERDICT: GO'
# a reviewer that outlives a (deliberately truncated) deadline, so the harness
# has to kill it and record a REAL `void-reason: timeout`
{ printf '#!/usr/bin/env bash\n'
  printf 'printf "%%s\\n" "reviewing the change"\n'
  printf 'sleep 30\n'
  printf 'exit 0\n'; } > "$WORK/rev-slow.sh"
chmod +x "$WORK/rev-slow.sh"
# a reviewer KILLED FROM OUTSIDE the harness: `wait` reports signal death as
# 128+signum, so this is what `kill -TERM <reviewer>` leaves behind.
mkrev "$WORK/rev-killed.sh" 143 '-' 'reviewing the change' 'killed mid-flight'

# the lane's byte-identity regression command, in both directions
printf '#!/usr/bin/env bash\nprintf "REGRESSION OK: 77 archived lines byte-identical\\n"\nexit 0\n' > "$WORK/reg-ok.sh"
printf '#!/usr/bin/env bash\nprintf "REGRESSION FAILED: 3 archived lines differ\\n"\nexit 1\n' > "$WORK/reg-fail.sh"
chmod +x "$WORK/reg-ok.sh" "$WORK/reg-fail.sh"

printf 'a\n' > "$SCOPEDIR/f1.txt"
printf 'b\n' > "$SCOPEDIR/f2.txt"
printf '%s/f1.txt\n' "$SCOPEDIR" > "$WORK/scope1.txt"
printf '%s/f1.txt\n%s/f2.txt\n' "$SCOPEDIR" "$SCOPEDIR" > "$WORK/scope2.txt"
printf 'review this change\n' > "$WORK/prompt.md"
printf 'review this change, differently\n' > "$WORK/prompt2.md"

round() { # round <arc> <n> <reviewer> <role> <tier> <rev> [scope] [prompt] [basis]
  local arc="$1" rnd="$2" rev="$3" role="$4" tier="$5" cmd="$6"
  local scope="${7:-}" prompt="${8:-}" basis="${9:-}"
  [ -n "$scope" ]  || scope="$WORK/scope1.txt"
  [ -n "$prompt" ] || prompt="$WORK/prompt.md"
  if [ -n "$basis" ]; then
    bash "$HARNESS" run --arc "$arc" --round "$rnd" --reviewer "$rev" --role "$role" \
      --tier "$tier" --prompt "$prompt" --scope "$scope" --reviewer-cmd "$WORK/$cmd" \
      --fallback-basis "$basis" > /dev/null 2>&1
  else
    bash "$HARNESS" run --arc "$arc" --round "$rnd" --reviewer "$rev" --role "$role" \
      --tier "$tier" --prompt "$prompt" --scope "$scope" --reviewer-cmd "$WORK/$cmd" \
      > /dev/null 2>&1
  fi
}

regression() { # regression <arc> <tier> [cmd]
  bash "$HARNESS" regression --arc "$1" --tier "$2" --scope "$WORK/scope1.txt" \
    --cmd "${3:-$WORK/reg-ok.sh}" > /dev/null 2>&1
}

CRC=0
arcreport() {
  local arc="$1"; shift
  CRC=0
  bash "$REPORT" --arc "$arc" --synthetic-ok "$@" > "$OUT" 2>&1 || CRC=$?
}
# THE THREE ASSERTIONS, and what each one is ABOUT (2026-07-31 re-point).
# `arc-report.sh` is a DIAGNOSTIC, not a judge: it has no "closed" output and
# no exit code that authorizes anything. So these teeth assert its BEHAVIOUR,
# not its authority:
#
#   expect_report   — the evidence is readable: a report is produced (rc 0),
#                     it is anchored and self-labelled as a diagnostic, and
#                     it carries the non-authority footer. This is the
#                     "baseline is clean" assertion every perturbation is one
#                     step away from.
#   expect_defect   — the perturbed bytes CANNOT be read as evidence: nonzero
#                     exit, and the refusal names the actual defect (not some
#                     other one). Fail-closed, unchanged.
#   expect_observed — the report DISCLOSES a named fact. Used where the old
#                     judge refused on a CLOSURE RULE: the rule is gone, but
#                     the situation it refused on must still be visible to
#                     the human who now makes that call. A tooth that could
#                     be satisfied by silence would be no tooth at all, so
#                     these also require the report to have been produced.
expect_report() { # expect_report <tooth> <arc>
  arcreport "$2"
  if [ "$CRC" = "0" ] \
     && LC_ALL=C grep -q "^ARC REPORT $2 (DIAGNOSTIC" "$OUT" \
     && LC_ALL=C grep -q '^NOT A CLOSURE DECISION\.' "$OUT"; then
    PASS=$((PASS + 1))
  else
    fail "$1" "expected a produced ARC REPORT, got rc=$CRC: $(head -1 "$OUT")"
  fi
}
expect_defect() { # expect_defect <tooth> <arc> <substring>
  arcreport "$2"
  if [ "$CRC" = "0" ]; then
    fail "$1" "FAILED OPEN — the evidence defect was not reported: $(head -1 "$OUT")"
  elif LC_ALL=C grep -q -- "$3" "$OUT"; then
    PASS=$((PASS + 1))
  else
    fail "$1" "refused for the WRONG reason (wanted '$3'): $(head -1 "$OUT")"
  fi
}
expect_observed() { # expect_observed <tooth> <arc> <substring>
  arcreport "$2"
  if [ "$CRC" != "0" ]; then
    fail "$1" "expected a produced report disclosing '$3', got rc=$CRC: $(head -1 "$OUT")"
  elif ! LC_ALL=C grep -q "^ARC REPORT $2 (DIAGNOSTIC" "$OUT"; then
    fail "$1" "no anchored diagnostic report header: $(head -1 "$OUT")"
  elif LC_ALL=C grep -q -- "$3" "$OUT"; then
    PASS=$((PASS + 1))
  else
    fail "$1" "the report is SILENT about '$3' — the situation the old judge refused on is now invisible"
  fi
}

# expect_footer_baseline — ONE presence assertion for the report's UNCONDITIONAL
# footer disclaimers. It replaces T89a/T94a/T96a, which asserted these exact
# strings as if each were an attack-specific proof. They are not: arc-report.sh
# prints the whole footer on EVERY report, so all three passed with their
# perturbations REMOVED and contributed three FALSE bits to the cited total
# (measured, .loop/review-r4split-r1-20260801.log [MEDIUM]). The disclaimers
# are still worth one baseline check — a report that stopped disclaiming would
# be a real regression — but one baseline is what the evidence supports, and
# the attack-specific work is already done by T89b/T94b/T96b, which bind to
# per-round ordering and basis evidence.
expect_footer_baseline() { # expect_footer_baseline <arc>
  arcreport "$1"
  if [ "$CRC" != "0" ]; then
    fail T-BASE-footer-disclaimers "expected a produced report, got rc=$CRC: $(head -1 "$OUT")"
  elif ! LC_ALL=C grep -q -- 'round order: NOT ESTABLISHED' "$OUT"; then
    fail T-BASE-footer-disclaimers 'the report no longer disclaims round order'
  elif ! LC_ALL=C grep -q -- '§11 basis eligibility: NOT DECIDED here' "$OUT"; then
    fail T-BASE-footer-disclaimers 'the report no longer disclaims basis eligibility'
  elif ! LC_ALL=C grep -q -- 'NOT A CLOSURE DECISION' "$OUT"; then
    fail T-BASE-footer-disclaimers 'the report no longer states that it is not a closure decision'
  else
    PASS=$((PASS + 1))
  fi
}

snapshot() { rm -rf "$PRISTINE/$1"; cp -R "$MLFK_ARC_DIR/$1" "$PRISTINE/$1"; }
restore()  { rm -rf "$MLFK_ARC_DIR/$1"; cp -R "$PRISTINE/$1" "$MLFK_ARC_DIR/$1"; }

setf() { # setf <artifact> <key> <value> — replace one field, seal left STALE
  local f="$1" k="$2" v="$3" t
  t="$(mktemp)"
  LC_ALL=C awk -v k="$k" -v v="$v" '
    { if (!done && index($0, k ": ") == 1) { print k ": " v; done = 1 } else print $0 }
  ' "$f" > "$t"
  cat "$t" > "$f"
  rm -f "$t"
}
reseal() { # reseal <artifact> — recompute the self-seal over the current body
  local f="$1" n b
  n="$(wc -l < "$f" | tr -d ' ')"
  b="$(mktemp)"
  head -n $((n - 1)) "$f" > "$b"
  { cat "$b"; printf 'artifact-sha256: %s\n' "$(shasum -a 256 "$b" | cut -d' ' -f1)"; } > "$f"
  rm -f "$b"
}
getf() { LC_ALL=C grep -a "^$2: " "$1" | head -1 | sed "s|^$2: ||"; }
artof() { ls -1 "$MLFK_ARC_DIR/$1"/"$2"-*.verdict | head -1; }
# rebind <artifact> — re-derive every log-dependent field, then reseal. Used
# by the teeth that swap in a specimen, so the refusal cannot come from a
# hash the perturber simply forgot to update.
rebind() {
  local a="$1" l
  l="$(getf "$a" 'log-path')"
  setf "$a" 'log-sha256' "$(shasum -a 256 "$l" | cut -d' ' -f1)"
  setf "$a" 'log-bytes'  "$(wc -c < "$l" | tr -d ' ')"
  setf "$a" 'log-lines'  "$(wc -l < "$l" | tr -d ' ')"
  reseal "$a"
}

# =========================================================== the baselines
# A: PROCESS §3 Tier A — codex NO-GO then codex GO.
round teetha 1 codex primary A rev-nogo.sh
round teetha 2 codex primary A rev-go.sh
# B: PROCESS §11 codex-failure fallback — a WEDGED codex round 3 (rc 7, no
#    verdict => VOID), then grok GO + opus5 GO both resting on it.
round teethb 3 codex primary A rev-wedged.sh
B_BASIS="$(basename "$(artof teethb r003)")"
round teethb 3 grok  fallback A rev-go.sh '' '' "$B_BASIS"
round teethb 3 opus5 fallback A rev-go.sh '' '' "$B_BASIS"
# C: PROCESS §3 Tier A+ — codex GO + an independent second opinion + the
#    harness-run byte-identity regression (the tier-up is TWO obligations).
round teethc 1 codex primary 'A+' rev-go.sh
round teethc 1 opus5 second-opinion 'A+' rev-go.sh
regression teethc 'A+'
# D: PROCESS §3 CAPPED — three NO-GO rounds plus the harness-written cap.
round teethd 1 codex primary A rev-nogo.sh
round teethd 2 codex primary A rev-nogo.sh
round teethd 3 codex primary A rev-nogo.sh
bash "$HARNESS" cap --arc teethd --tier A --rounds 3 --scope "$WORK/scope1.txt" \
  --authorized-by owner-ruling-2026-07-30 \
  --class 'the judge surface admits new loosenings faster than point fixes close them' \
  > /dev/null 2>&1
# E: a Tier A+ arc closed by a CAP, with its second opinion present.
round teethe 1 codex primary 'A+' rev-nogo.sh
round teethe 1 opus5 second-opinion 'A+' rev-nogo.sh
regression teethe 'A+'
bash "$HARNESS" cap --arc teethe --tier 'A+' --rounds 1 --scope "$WORK/scope1.txt" \
  --authorized-by owner-ruling-2026-07-30 \
  --class 'the judge surface admits new loosenings faster than point fixes close them' \
  > /dev/null 2>&1
for a in teetha teethb teethc teethd teethe; do snapshot "$a"; done

# ================================================================== teeth
# --- positive controls (an arc that SHOULD close, does) -------------------
expect_report T01-baseline-tier-a teetha
expect_footer_baseline teetha   # ONE baseline for the unconditional footer disclaimers
expect_report T02-fallback-dual-go teethb
expect_report T03-tier-aplus teethc
expect_report T04-capped teethd
expect_report T05-capped-tier-aplus teethe

# --- T06 no artifact at all ----------------------------------------------
rm -rf "$MLFK_ARC_DIR/teetha"
expect_defect T06-no-artifact teetha 'no arc directory'
restore teetha
expect_report T06-restored teetha

# --- T07 self-seal: one changed body byte --------------------------------
A2="$(artof teetha r002)"
setf "$A2" 'tier' 'B'
expect_defect T07-self-seal teetha 'self-seal MISMATCH'
restore teetha
expect_report T07-restored teetha

# --- T08 truncated artifact ----------------------------------------------
A2="$(artof teetha r002)"
T="$(mktemp)"; head -n 20 "$A2" > "$T"; cat "$T" > "$A2"; rm -f "$T"
expect_defect T08-truncated-artifact teetha 'lines, measured'
restore teetha
expect_report T08-restored teetha

# --- T09 NUL byte inside the artifact ------------------------------------
A2="$(artof teetha r002)"
printf 'x\000y\n' >> "$A2"
expect_defect T09-nul-in-artifact teetha 'NUL byte'
restore teetha
expect_report T09-restored teetha

# --- T10 a foreign transcript appended to the LOG after the round closed --
L2="$(getf "$(artof teetha r002)" 'log-path')"
{ printf 'pasted transcript from another arc follows\n'; printf 'VERDICT: GO\n'; } >> "$L2"
expect_defect T10-log-appended teetha 'does not match its recorded sha256'
restore teetha
expect_report T10-restored teetha

# --- T11 the same paste, with the artifact FULLY re-derived to match ------
A2="$(artof teetha r002)"
L2="$(getf "$A2" 'log-path')"
{ printf 'pasted transcript from another arc follows\n'; printf 'VERDICT: GO\n'; } >> "$L2"
rebind "$A2"
expect_defect T11-paste-with-fixed-hash teetha "last line is not this round's HARNESS-EOR"
restore teetha
expect_report T11-restored teetha

# --- T12 THE LIVE SPECIMEN, claimed as a GO round -------------------------
# .loop/review-c25-1-codex-VOID.log: real terminal NO-GO at 4063, then a
# quoted foreign transcript whose GO at 4094 is the file's LAST verdict.
A2="$(artof teetha r002)"
L2="$(getf "$A2" 'log-path')"
cp "$SPEC_FOREIGN" "$L2"
setf "$A2" 'log-nul-count' '0'
setf "$A2" 'rc-line'       '4245'
setf "$A2" 'verdict'       'GO'
setf "$A2" 'verdict-line'  '4094'
setf "$A2" 'anchored-go'   '2'
setf "$A2" 'anchored-nogo' '2'
rebind "$A2"
expect_defect T12-live-foreign-specimen teetha "last line is not this round's HARNESS-EOR"
restore teetha
expect_report T12-restored teetha

# --- T13 the specimen WITH a harness terminator at the true end of the run -
# The paste then sits after the terminator. Artifact internally perfect.
A2="$(artof teetha r002)"
L2="$(getf "$A2" 'log-path')"
NONCE="$(getf "$A2" 'eor-nonce')"
{
  head -n 4064 "$SPEC_FOREIGN"
  printf 'REVIEWER_RC %s 0\nHARNESS-EOR %s\n' "$NONCE" "$NONCE"
  tail -n +4065 "$SPEC_FOREIGN"
} > "$L2"
setf "$A2" 'log-nul-count' '0'
setf "$A2" 'rc-line'       '4065'
setf "$A2" 'verdict'       'GO'
setf "$A2" 'verdict-line'  '4096'
setf "$A2" 'anchored-go'   '2'
setf "$A2" 'anchored-nogo' '2'
rebind "$A2"
expect_defect T13-specimen-post-terminator teetha "last line is not this round's HARNESS-EOR"
restore teetha
expect_report T13-restored teetha

# --- T14 the DECISION CHANNEL catches the same shape independently --------
# A transcript whose terminal in-region verdict is GO, with the reviewer's
# final message saying NO-GO. The harness VOIDs it; nothing can close on it.
round teethf 1 codex primary A rev-mismatch.sh
[ "$(getf "$(artof teethf r001)" 'void-reason')" = 'decision-mismatch' ] \
  || fail T14-setup "the harness did not void the transcript/decision disagreement (got '$(getf "$(artof teethf r001)" 'void-reason')')"
expect_observed T14-decision-mismatch teethf 'a failed round is not a round'

# --- T15 the decision channel doctored to disagree with the artifact ------
A2="$(artof teethc r001)"
D2="$(getf "$A2" 'decision-path')"
printf 'Final message.\nVERDICT: NO-GO\n' > "$D2"
setf "$A2" 'decision-sha256' "$(shasum -a 256 "$D2" | cut -d' ' -f1)"
reseal "$A2"
expect_defect T15-decision-doctored teethc "the decision channel says 'NO-GO'"
restore teethc
expect_report T15-restored teethc

# --- T16 THE LIVE NUL SPECIMEN, artifact lying about the NUL count --------
A2="$(artof teetha r002)"
L2="$(getf "$A2" 'log-path')"
cp "$SPEC_NUL" "$L2"
setf "$A2" 'log-nul-count' '0'
rebind "$A2"
expect_defect T16-nul-specimen-lying teetha 'log NUL count 7835 != recorded 0'
restore teetha
expect_report T16-restored teetha

# --- T17 NULs in an OTHERWISE INTACT log, declared honestly ---------------
# (the specimen swap is caught by the terminator layer first — this reaches
#  the NUL-zero rule itself, with a valid nonce terminator still in place)
A2="$(artof teetha r002)"
L2="$(getf "$A2" 'log-path')"
NL="$(wc -l < "$L2" | tr -d ' ')"
{ head -n $((NL - 2)) "$L2"; printf 'corrupt\000bytes\n'; tail -n 2 "$L2"; } > "$WORK/nul.log"
cat "$WORK/nul.log" > "$L2"
setf "$A2" 'log-nul-count' "$(LC_ALL=C tr -dc '\000' < "$L2" | wc -c | tr -d ' ')"
setf "$A2" 'rc-line' "$(( $(wc -l < "$L2" | tr -d ' ') - 1 ))"
rebind "$A2"
expect_defect T17-nul-in-intact-log teetha 'a corrupt log is not a verdict'
restore teetha
expect_report T17-restored teetha

# --- T18 a GO whose reviewer exited nonzero is VOIDed by the harness ------
# (VOID, not a poisoned non-VOID artifact: it must be excluded from closure
#  and remain usable as §11's failed-codex basis)
round teethg 1 codex primary A rev-go-rc1.sh
[ "$(getf "$(artof teethg r001)" 'void-reason')" = 'reviewer-failed' ] \
  || fail T18-setup "the harness did not VOID the rc=1 round (got '$(getf "$(artof teethg r001)" 'void-reason')')"
expect_observed T18-go-with-rc1 teethg 'a failed round is not a round'

# --- T18b the JUDGE's own layer: a non-VOID artifact with rc != 0 ---------
# (forged CONSISTENTLY — artifact field and log marker agree — so the refusal
#  comes from the rc rule itself, not from the marker-agreement layer)
A2="$(artof teetha r002)"
L2="$(getf "$A2" 'log-path')"
NONCE="$(getf "$A2" 'eor-nonce')"
LC_ALL=C sed "s|^REVIEWER_RC $NONCE 0\$|REVIEWER_RC $NONCE 1|" "$L2" > "$WORK/rc1.log"
cat "$WORK/rc1.log" > "$L2"
setf "$A2" 'reviewer-rc' '1'
rebind "$A2"
expect_defect T18b-judge-rc-layer teetha 'a failed reviewer process is not a completed review'
restore teetha
expect_report T18b-restored teetha

# --- T19 a failed reviewer process cannot pad a CAP's round count ---------
round teethh 1 codex primary A rev-nogo.sh
round teethh 2 codex primary A rev-go-rc1.sh
bash "$HARNESS" cap --arc teethh --tier A --rounds 2 --scope "$WORK/scope1.txt" \
  --authorized-by owner-ruling-2026-07-30 \
  --class 'the judge surface admits new loosenings faster than point fixes close them' \
  > /dev/null 2>&1
expect_observed T19-rc1-cannot-pad-a-cap teethh 'claims 2 rounds completed, 1 countable'

# --- T20 an adorned verdict is not a verdict (PROCESS §3/C11) -------------
round teethi 1 codex primary A rev-adorned.sh
[ "$(getf "$(artof teethi r001)" 'void-reason')" = 'no-verdict' ] \
  || fail T20-setup 'the harness did not VOID the adorned round'
expect_observed T20-adorned-verdict teethi 'a failed round is not a round'

# --- T21 §11 fallback with only ONE of the two reviewers ------------------
rm -f "$MLFK_ARC_DIR/teethb"/r003-opus5-*
expect_observed T21-fallback-single-reviewer teethb 'no complete §11 fallback pair'
restore teethb
expect_report T21-restored teethb

# --- T22 §11 fallback faked with a second-opinion artifact ----------------
# (grok fallback + opus5 second-opinion must NOT satisfy the pair)
rm -f "$MLFK_ARC_DIR/teethb"/r003-opus5-*
round teethb 3 opus5 second-opinion A rev-go.sh
expect_observed T22-fallback-vs-second-opinion teethb 'no complete §11 fallback pair'
restore teethb
expect_report T22-restored teethb

# --- T23 §11 fallback with no recorded failed-codex basis -----------------
rm -f "$MLFK_ARC_DIR/teethb"/r003-codex-*
expect_defect T23-fallback-no-basis teethb 'names no artifact in this arc'
restore teethb
expect_report T23-restored teethb

# --- T24 the two fallback reviewers rest on DIFFERENT bases ---------------
A2="$(artof teethb r003-grok)"
setf "$A2" 'fallback-basis' 'r003-codex-primary-20260101T000000Z-deadbeef.verdict'
reseal "$A2"
expect_defect T24-fallback-split-basis teethb 'names no artifact in this arc'
restore teethb
expect_report T24-restored teethb

# --- T25 the fallback pair reviewed DIFFERENT BYTES -----------------------
# (same path set, different content — the shape a stale co-reviewer makes)
rm -f "$MLFK_ARC_DIR/teethb"/r003-opus5-*
printf 'a-drifted\n' > "$SCOPEDIR/f1.txt"
round teethb 3 opus5 fallback A rev-go.sh '' '' "$B_BASIS"
printf 'a\n' > "$SCOPEDIR/f1.txt"
expect_defect T25-fallback-split-bytes teethb 'reviewed different scope BYTES'
restore teethb
expect_report T25-restored teethb

# --- T26 the fallback pair saw DIFFERENT PROMPTS (§11: "same prompt") -----
rm -f "$MLFK_ARC_DIR/teethb"/r003-opus5-*
round teethb 3 opus5 fallback A rev-go.sh '' "$WORK/prompt2.md" "$B_BASIS"
expect_defect T26-fallback-split-prompt teethb 'used a different prompt'
restore teethb
expect_report T26-restored teethb

# --- T27 Tier A+ without an independent second reviewer (GO path) ---------
rm -f "$MLFK_ARC_DIR/teethc"/r001-opus5-*
expect_observed T27-tier-aplus-single-reviewer teethc 'tier A+ obligation (1) independent second reviewer: none'
restore teethc
expect_report T27-restored teethc

# --- T28 Tier A+ CAP without an independent second reviewer --------------
# (a cap does not discharge the tier-up — the live menus precedent)
rm -f "$MLFK_ARC_DIR/teethe"/r001-opus5-*
expect_observed T28-tier-aplus-cap-single teethe 'tier A+ obligation (1) independent second reviewer: none'
restore teethe
expect_report T28-restored teethe

# --- T29 the reviewed scope drifted after the GO --------------------------
printf 'a-modified\n' > "$SCOPEDIR/f1.txt"
expect_observed T29-scope-drift teetha 'has DRIFTED since round'
printf 'a\n' > "$SCOPEDIR/f1.txt"
expect_report T29-restored teetha

# --- T30 a scope file deleted after the GO --------------------------------
mv "$SCOPEDIR/f1.txt" "$WORK/f1.saved"
expect_observed T30-scope-missing teetha 'is missing (or a symlink) in the current tree'
mv "$WORK/f1.saved" "$SCOPEDIR/f1.txt"
expect_report T30-restored teetha

# --- T31 the archived prompt does not carry the recorded scope digest -----
# (the prompt/scope mismatch that would let a sealed GO name unrelated files)
A2="$(artof teetha r002)"
P2="$(getf "$A2" 'prompt-path')"
LC_ALL=C sed 's|^scope-sha256: |scope-sha256-was: |' "$P2" > "$P2.x"; cat "$P2.x" > "$P2"; rm -f "$P2.x"
setf "$A2" 'prompt-sha256' "$(shasum -a 256 "$P2" | cut -d' ' -f1)"
reseal "$A2"
expect_defect T31-prompt-scope-mismatch teetha 'the reviewer was not told this scope'
restore teetha
expect_report T31-restored teetha

# --- T32 an artifact from ANOTHER arc dropped into this one ---------------
cp "$PRISTINE/teethb"/r003-grok-*.verdict "$MLFK_ARC_DIR/teetha/r003-grok-foreign.verdict"
expect_defect T32-foreign-arc-artifact teetha "declares arc-id 'teethb'"
rm -f "$MLFK_ARC_DIR/teetha/r003-grok-foreign.verdict"
restore teetha
expect_report T32-restored teetha

# --- T33 rounds of "one arc" that reviewed different scopes ---------------
round teethj 1 codex primary A rev-nogo.sh "$WORK/scope1.txt"
round teethj 2 codex primary A rev-go.sh   "$WORK/scope2.txt"
expect_defect T33-scope-pathset-mismatch teethj 'not rounds of one arc'

# --- T34 a cap that claims more rounds than exist -------------------------
rm -f "$MLFK_ARC_DIR/teethd"/r002-*
expect_observed T34-cap-round-count teethd 'claims 3 rounds completed, 2 countable'
restore teethd
expect_report T34-restored teethd

# --- T35 a synthetic reviewer, judged WITHOUT --synthetic-ok --------------
CRC=0
bash "$REPORT" --arc teetha > "$OUT" 2>&1 || CRC=$?
if [ "$CRC" = "0" ]; then
  fail T35-synthetic-reviewer 'FAILED OPEN — closed an arc reviewed by a synthetic reviewer'
elif LC_ALL=C grep -q 'synthetic reviewer' "$OUT"; then
  PASS=$((PASS + 1))
else
  fail T35-synthetic-reviewer "refused for the wrong reason: $(head -1 "$OUT")"
fi

# --- T36 a codex round decided by the transcript instead of last-message --
A2="$(artof teetha r002)"
setf "$A2" 'decision-source' 'transcript'
setf "$A2" 'decision-path'   "$(getf "$A2" 'log-path')"
setf "$A2" 'decision-sha256' "$(getf "$A2" 'log-sha256')"
reseal "$A2"
expect_defect T36-codex-must-use-last-message teetha 'must be decided by the last-message channel'
restore teetha
expect_report T36-restored teetha

# --- T37 the round log replaced by a symlink ------------------------------
A2="$(artof teetha r002)"
L2="$(getf "$A2" 'log-path')"
cp "$L2" "$WORK/real.log"
rm -f "$L2"
ln -s "$ROOT/$WORK/real.log" "$L2"
expect_defect T37-log-symlink teetha 'arc-directory symlink'
restore teetha
expect_report T37-restored teetha

# --- T38 a leftover partial write in the arc directory --------------------
printf 'partial\n' > "$MLFK_ARC_DIR/teetha/r002-codex-primary-x.verdict.tmp.999"
expect_defect T38-partial-write teetha 'unexpected entry in the arc directory'
restore teetha
expect_report T38-restored teetha

# --- T39 an INCOMPLETE bundle (a killed or in-flight run) ----------------
A2="$(artof teetha r002)"
cp "$(getf "$A2" 'log-path')" "$MLFK_ARC_DIR/teetha/r003-codex-primary-20260101T000000Z-abcdef01.log"
expect_defect T39-incomplete-bundle teetha 'an incomplete round bundle'
restore teetha
expect_report T39-restored teetha

# --- T40 two artifacts for one (round, reviewer) --------------------------
# The second bundle is built to be FULLY VALID at every earlier layer — its
# base keeps the harness's `r<NNN>-<reviewer>-<role>-<stamp>-<nonce8>` shape
# (same round/reviewer/role/stamp, a DIFFERENT nonce stem, with the log's
# terminator lines and the recorded eor-nonce rewritten to match) — so the
# refusal is the AMBIGUITY itself and not a path-binding or sidecar layer.
A2="$(artof teetha r002)"
OB="$(basename "$A2" .verdict)"
ONONCE="$(getf "$A2" 'eor-nonce')"
NNONCE="deadbeef${ONONCE:8}"
DUP="$MLFK_ARC_DIR/teetha/${OB%-*}-${NNONCE:0:8}"
cp "$A2" "$DUP.verdict"
LC_ALL=C sed "s|$ONONCE|$NNONCE|g" "$(getf "$A2" 'log-path')" > "$DUP.log"
cp "$(getf "$A2" 'prompt-path')"    "$DUP.prompt"
cp "$(getf "$A2" 'scope-manifest')" "$DUP.scope"
cp "$(getf "$A2" 'decision-path')"  "$DUP.decision"
setf "$DUP.verdict" 'scope-manifest' "$DUP.scope"
setf "$DUP.verdict" 'prompt-path'    "$DUP.prompt"
setf "$DUP.verdict" 'log-path'       "$DUP.log"
setf "$DUP.verdict" 'decision-path'  "$DUP.decision"
setf "$DUP.verdict" 'eor-nonce'      "$NNONCE"
setf "$DUP.verdict" 'decision-sha256' "$(shasum -a 256 "$DUP.decision" | cut -d' ' -f1)"
rebind "$DUP.verdict"
expect_defect T40-duplicate-round teetha 'two non-VOID artifacts for round 2'
restore teetha
expect_report T40-restored teetha

# --- T41 a re-run never OVERWRITES a round; it becomes the T40 ambiguity ---
EX="$(artof teetha r002)"
EXSHA="$(shasum -a 256 "$EX" | cut -d' ' -f1)"
RC=0
bash "$HARNESS" run --arc teetha --round 2 --reviewer codex --role primary \
  --tier A --prompt "$WORK/prompt.md" --scope "$WORK/scope1.txt" \
  --reviewer-cmd "$WORK/rev-go.sh" > "$OUT" 2>&1 || RC=$?
NART="$(ls -1 "$MLFK_ARC_DIR/teetha"/r002-*.verdict | wc -l | tr -d ' ')"
if [ "$RC" != "0" ]; then
  fail T41-no-overwrite "harness run failed unexpectedly: $(head -1 "$OUT")"
elif [ "$NART" -lt 2 ]; then
  fail T41-no-overwrite 'a second round-2 run did not produce a second artifact'
elif [ "$(shasum -a 256 "$EX" | cut -d' ' -f1)" != "$EXSHA" ]; then
  fail T41-no-overwrite 'the first round-2 artifact was modified by the re-run'
else
  expect_defect T41-no-overwrite teetha 'two non-VOID artifacts for round 2'
fi
restore teetha
expect_report T41-restored teetha

# --- T42 two SIMULTANEOUS launches of the same round never share a path ---
# This is the two-writer class that produced this project's 94%-NUL log with
# a healthy-looking tail: same second, same round, same reviewer.
bash "$HARNESS" run --arc teethy --round 1 --reviewer codex --role primary \
  --tier A --prompt "$WORK/prompt.md" --scope "$WORK/scope1.txt" \
  --reviewer-cmd "$WORK/rev-go.sh" > /dev/null 2>&1 &
P1=$!
bash "$HARNESS" run --arc teethy --round 1 --reviewer codex --role primary \
  --tier A --prompt "$WORK/prompt.md" --scope "$WORK/scope1.txt" \
  --reviewer-cmd "$WORK/rev-go.sh" > /dev/null 2>&1 &
P2=$!
wait "$P1" || true
wait "$P2" || true
NLOG="$(ls -1 "$MLFK_ARC_DIR/teethy" 2>/dev/null | LC_ALL=C grep -c '\.log$' || true)"
NUNIQ="$(ls -1 "$MLFK_ARC_DIR/teethy" 2>/dev/null | LC_ALL=C grep '\.log$' | LC_ALL=C sort -u | LC_ALL=C grep -c . || true)"
if [ "$NLOG" = "2" ] && [ "$NUNIQ" = "2" ]; then
  # and the judge must then refuse the ambiguity rather than pick one
  expect_defect T42-concurrent-launch teethy 'two non-VOID artifacts for round 1'
else
  fail T42-concurrent-launch "two simultaneous launches produced $NLOG log(s), $NUNIQ distinct"
fi

# --- T43 the harness refuses an empty / unusable scope --------------------
: > "$WORK/scope-empty.txt"
RC=0
bash "$HARNESS" run --arc teethz --round 1 --reviewer codex --role primary \
  --tier A --prompt "$WORK/prompt.md" --scope "$WORK/scope-empty.txt" \
  --reviewer-cmd "$WORK/rev-go.sh" > "$OUT" 2>&1 || RC=$?
if [ "$RC" != "0" ] && LC_ALL=C grep -q 'an empty scope is not a scope' "$OUT"; then
  PASS=$((PASS + 1))
else
  fail T43-empty-scope "harness accepted an empty scope (rc=$RC)"
fi

# --- T44 the harness refuses an incoherent role/reviewer pairing ----------
RC=0
bash "$HARNESS" run --arc teethz --round 1 --reviewer grok --role primary \
  --tier A --prompt "$WORK/prompt.md" --scope "$WORK/scope1.txt" \
  --reviewer-cmd "$WORK/rev-go.sh" > "$OUT" 2>&1 || RC=$?
if [ "$RC" != "0" ] && LC_ALL=C grep -q "may not hold role 'primary'" "$OUT"; then
  PASS=$((PASS + 1))
else
  fail T44-role-coherence "harness accepted grok as the primary reviewer (rc=$RC)"
fi

# --- T45 the harness refuses a fallback round with no basis ---------------
RC=0
bash "$HARNESS" run --arc teethz --round 1 --reviewer grok --role fallback \
  --tier A --prompt "$WORK/prompt.md" --scope "$WORK/scope1.txt" \
  --reviewer-cmd "$WORK/rev-go.sh" > "$OUT" 2>&1 || RC=$?
if [ "$RC" != "0" ] && LC_ALL=C grep -q 'requires --fallback-basis' "$OUT"; then
  PASS=$((PASS + 1))
else
  fail T45-fallback-needs-basis "harness accepted a fallback round with no basis (rc=$RC)"
fi

# --- T46 a TRUNCATED decision channel is never repaired into a verdict ----
round teethk 1 codex primary A rev-dec-trunc.sh
[ "$(getf "$(artof teethk r001)" 'void-reason')" = 'decision-malformed' ] \
  || fail T46-setup "the harness did not VOID a truncated decision write (got '$(getf "$(artof teethk r001)" 'void-reason')')"
expect_observed T46-decision-truncated teethk 'a failed round is not a round'

# --- T47 content AFTER the verdict in the decision channel ----------------
round teethl 1 codex primary A rev-dec-junk.sh
[ "$(getf "$(artof teethl r001)" 'void-reason')" = 'decision-malformed' ] \
  || fail T47-setup "the harness did not VOID trailing decision content (got '$(getf "$(artof teethl r001)" 'void-reason')')"
expect_observed T47-decision-trailing teethl 'a failed round is not a round'

# --- T48 the JUDGE's own layer for trailing decision content --------------
A2="$(artof teethc r001)"
D2="$(getf "$A2" 'decision-path')"
printf 'Final message.\nVERDICT: GO\n...and some trailing prose\n' > "$D2"
setf "$A2" 'decision-sha256' "$(shasum -a 256 "$D2" | cut -d' ' -f1)"
reseal "$A2"
expect_defect T48-judge-decision-trailing teethc "last line is not an anchored verdict"
restore teethc
expect_report T48-restored teethc

# --- T49 a cap over rounds that are not review rounds --------------------
# (three second opinions are not three rounds)
round teethm 1 codex primary A rev-nogo.sh
round teethm 2 opus5 second-opinion A rev-nogo.sh
bash "$HARNESS" cap --arc teethm --tier A --rounds 2 --scope "$WORK/scope1.txt" \
  --authorized-by owner-ruling-2026-07-30 \
  --class 'the judge surface admits new loosenings faster than point fixes close them' \
  > /dev/null 2>&1
expect_observed T49-cap-noncountable-round teethm 'round 2: NOT countable as a review round'

# --- T50 a cap written over DIFFERENT BYTES of the same paths -------------
printf 'a-capped-elsewhere\n' > "$SCOPEDIR/f1.txt"
bash "$HARNESS" cap --arc teethn --tier A --rounds 1 --scope "$WORK/scope1.txt" \
  --authorized-by owner-ruling-2026-07-30 \
  --class 'the judge surface admits new loosenings faster than point fixes close them' \
  > /dev/null 2>&1
printf 'a\n' > "$SCOPEDIR/f1.txt"
round teethn 1 codex primary A rev-nogo.sh
expect_observed T50-cap-different-bytes teethn 'caps different scope BYTES'

# --- T51 Tier A+ "second opinion" from the SAME reviewer as the primary ---
round teetho 1 codex primary 'A+' rev-go.sh
round teetho 1 codex second-opinion 'A+' rev-go.sh
expect_defect T51-aplus-same-reviewer teetho 'two non-VOID artifacts for round 1 reviewer codex'

# --- T52 a capped A+ arc whose second opinion is the primary's reviewer ---
round teethp 1 codex primary 'A+' rev-nogo.sh
round teethp 1 grok second-opinion 'A+' rev-nogo.sh
regression teethp 'A+'
bash "$HARNESS" cap --arc teethp --tier 'A+' --rounds 1 --scope "$WORK/scope1.txt" \
  --authorized-by owner-ruling-2026-07-30 \
  --class 'the judge surface admits new loosenings faster than point fixes close them' \
  > /dev/null 2>&1
expect_report T52-capped-aplus-distinct teethp
# NOTE (measured, not assumed): the "second opinion from the SAME reviewer as
# the primary" case is structurally unreachable — T51 shows the one-artifact-
# per-(round, reviewer) rule refuses it first, whatever the roles. The
# independence test inside tier_aplus_second_reviewer is therefore defence in
# depth behind that rule, and is documented as such rather than given a tooth
# that could only pass for the wrong reason.

# --- T53 the fallback basis is a second-opinion artifact, not a primary ---
round teethq 3 codex second-opinion A rev-wedged.sh
Q_BASIS="$(basename "$(artof teethq r003)")"
round teethq 3 grok  fallback A rev-go.sh '' '' "$Q_BASIS"
round teethq 3 opus5 fallback A rev-go.sh '' '' "$Q_BASIS"
expect_defect T53-basis-not-primary teethq 'has role second-opinion, not primary'

# --- T54 the fallback basis reviewed DIFFERENT BYTES ----------------------
printf 'a-drifted\n' > "$SCOPEDIR/f1.txt"
round teethr 3 codex primary A rev-wedged.sh
printf 'a\n' > "$SCOPEDIR/f1.txt"
R_BASIS_N="$(basename "$(artof teethr r003)")"
round teethr 3 grok  fallback A rev-go.sh '' '' "$R_BASIS_N"
round teethr 3 opus5 fallback A rev-go.sh '' '' "$R_BASIS_N"
expect_defect T54-basis-different-bytes teethr 'reviewed different scope BYTES'

# --- T56 the fallback basis used a DIFFERENT PROMPT -----------------------
# (found by an OFF control: the basis-prompt check was uncovered until now)
round teeths 3 codex primary A rev-wedged.sh '' "$WORK/prompt2.md"
S_BASIS="$(basename "$(artof teeths r003)")"
round teeths 3 grok  fallback A rev-go.sh '' '' "$S_BASIS"
round teeths 3 opus5 fallback A rev-go.sh '' '' "$S_BASIS"
expect_defect T56-basis-different-prompt teeths 'used a different prompt'

# --- T55 a PRE-LAUNCH refusal leaves no residue that blocks the arc -------
# (a reserved-but-unlaunched bundle would read as an incomplete round)
: > "$WORK/scope-bad.txt"
# The result is CAPTURED, not discarded. This tooth read `|| true` and then
# only checked for residue, so it passed against a `true` binary — no refusal
# had to occur at all (measured, .loop/review-r4split-r1-20260801.log
# [MEDIUM]). The refusal itself is now the first assertion, pinned to its
# exact anchored diagnostic, and residue is checked only after that holds.
T55RC=0
bash "$HARNESS" run --arc teetha --round 8 --reviewer codex --role primary \
  --tier A --prompt "$WORK/prompt.md" --scope "$WORK/scope-bad.txt" \
  --reviewer-cmd "$WORK/rev-go.sh" > "$OUT" 2>&1 || T55RC=$?
RESID="$(ls -1 "$MLFK_ARC_DIR/teetha" | LC_ALL=C grep -c '^r008-' || true)"
if [ "$T55RC" != "2" ]; then
  fail T55-no-residue "the harness exited $T55RC on an empty scope, want EXACTLY 2 (any-nonzero would also accept a crash or a missing harness)"
elif ! LC_ALL=C grep -q "^HARNESS REFUSED: scope list .* resolved to 0 files (an empty scope is not a scope)$" "$OUT"; then
  fail T55-no-residue "the harness did not emit its anchored empty-scope refusal: $(head -1 "$OUT")"
elif [ "$RESID" != "0" ]; then
  fail T55-no-residue "a refused pre-launch round left $RESID file(s) behind"
else
  expect_report T55-no-residue teetha
fi

# --- T57 Tier A+ with NO byte-identity regression record ------------------
# (PROCESS §3's tier-up is TWO obligations; half of it is not closure)
rm -f "$MLFK_ARC_DIR/teethc"/reg-*
expect_observed T57-aplus-no-regression teethc 'tier A+ obligation (2) byte-identity regression: none'
restore teethc
expect_report T57-restored teethc

# --- T58 a Tier A+ regression that FAILED ---------------------------------
round teetht 1 codex primary 'A+' rev-go.sh
round teetht 1 opus5 second-opinion 'A+' rev-go.sh
regression teetht 'A+' "$WORK/reg-fail.sh"
expect_observed T58-aplus-regression-failed teetht 'byte-identity regression FAILED'

# --- T59 a Tier A+ regression run over DIFFERENT BYTES --------------------
round teethu 1 codex primary 'A+' rev-go.sh
round teethu 1 opus5 second-opinion 'A+' rev-go.sh
printf 'a-elsewhere\n' > "$SCOPEDIR/f1.txt"
regression teethu 'A+'
printf 'a\n' > "$SCOPEDIR/f1.txt"
expect_observed T59-aplus-regression-stale teethu 'ran over different scope BYTES'

# --- T60 a CAPPED Tier A+ arc with no regression record -------------------
rm -f "$MLFK_ARC_DIR/teethe"/reg-*
expect_observed T60-capped-aplus-no-regression teethe 'tier A+ obligation (2) byte-identity regression: none'
restore teethe
expect_report T60-restored teethe

# --- T61 a cap padded with a fallback pair that has no valid basis ---------
# (the cap path used to exit 0 before ANY basis validation ran)
round teethv 1 codex primary A rev-nogo.sh
V_BASIS="$(basename "$(artof teethv r001)")"
# the pair at round 2 rests on the round-1 basis: a real artifact, wrong round
round teethv 2 grok  fallback A rev-go.sh '' '' "$V_BASIS"
round teethv 2 opus5 fallback A rev-go.sh '' '' "$V_BASIS"
bash "$HARNESS" cap --arc teethv --tier A --rounds 2 --scope "$WORK/scope1.txt" \
  --authorized-by owner-ruling-2026-07-30 \
  --class 'the judge surface admits new loosenings faster than point fixes close them' \
  > /dev/null 2>&1
expect_defect T61-cap-invalid-fallback-round teethv 'is round 1, not round 2'

# --- T62 a fallback basis that is a substantive codex NO-GO ---------------
# ("proven failed" means VOID, not merely "not a GO"). Exercised on the CAP
# path: on the GO path the closing round's NO-GO is refused earlier, so the
# rule is only REACHABLE here — which is exactly why the cap path had to stop
# exiting before basis validation.
round teethw 3 codex primary A rev-nogo.sh
W_BASIS="$(basename "$(artof teethw r003)")"
round teethw 3 grok  fallback A rev-go.sh '' '' "$W_BASIS"
round teethw 3 opus5 fallback A rev-go.sh '' '' "$W_BASIS"
bash "$HARNESS" cap --arc teethw --tier A --rounds 1 --scope "$WORK/scope1.txt" \
  --authorized-by owner-ruling-2026-07-30 \
  --class 'the judge surface admits new loosenings faster than point fixes close them' \
  > /dev/null 2>&1
expect_defect T62-basis-substantive-nogo teethw 'not a VOID (proven-failed) round'

# --- T63 a scope manifest that is not byte-wise sorted --------------------
A2="$(artof teetha r002)"
M2="$(getf "$A2" 'scope-manifest')"
printf '%s  zzz/last.txt\n' "$(shasum -a 256 "$SCOPEDIR/f1.txt" | cut -d' ' -f1)" > "$WORK/m.tmp"
cat "$M2" >> "$WORK/m.tmp"
cat "$WORK/m.tmp" > "$M2"
setf "$A2" 'scope-count'  "$(wc -l < "$M2" | tr -d ' ')"
setf "$A2" 'scope-sha256' "$(shasum -a 256 "$M2" | cut -d' ' -f1)"
reseal "$A2"
expect_defect T63-manifest-unsorted teetha 'not byte-wise sorted'
restore teetha
expect_report T63-restored teetha

# --- T64 a scope manifest line that is not the pinned grammar -------------
A2="$(artof teetha r002)"
M2="$(getf "$A2" 'scope-manifest')"
printf 'not-a-sha  %s/f2.txt\n' "$SCOPEDIR" >> "$M2"
setf "$A2" 'scope-count'  "$(wc -l < "$M2" | tr -d ' ')"
setf "$A2" 'scope-sha256' "$(shasum -a 256 "$M2" | cut -d' ' -f1)"
reseal "$A2"
expect_defect T64-manifest-grammar teetha 'does not match the pinned grammar'
restore teetha
expect_report T64-restored teetha

# --- T65 a duplicated path in the scope manifest --------------------------
A2="$(artof teetha r002)"
M2="$(getf "$A2" 'scope-manifest')"
head -1 "$M2" >> "$M2"
setf "$A2" 'scope-count'  "$(wc -l < "$M2" | tr -d ' ')"
setf "$A2" 'scope-sha256' "$(shasum -a 256 "$M2" | cut -d' ' -f1)"
reseal "$A2"
expect_defect T65-manifest-duplicate teetha 'twice'
restore teetha
expect_report T65-restored teetha

# --- T66 the artifact's arc/round/tier disagree with the archived prompt ---
A2="$(artof teetha r002)"
P2="$(getf "$A2" 'prompt-path')"
LC_ALL=C sed 's|^arc-id: teetha · round: 2 |arc-id: teetha · round: 7 |' "$P2" > "$P2.x"
cat "$P2.x" > "$P2"; rm -f "$P2.x"
setf "$A2" 'prompt-sha256' "$(shasum -a 256 "$P2" | cut -d' ' -f1)"
reseal "$A2"
expect_defect T66-prompt-identity-mismatch teetha "does not carry this artifact's arc-id/round/tier"
restore teetha
expect_report T66-restored teetha

# --- T67 a MISLABELLED void-reason ----------------------------------------
# (transcript GO + a junk decision channel is decision-malformed; claiming
#  decision-mismatch would falsify the machine-readable reason)
A2="$(artof teethl r001)"
setf "$A2" 'void-reason' 'decision-mismatch'
reseal "$A2"
expect_defect T67-void-reason-mislabelled teethl "the evidence derives 'decision-malformed'"

# --- T68 a VOID artifact claiming a reason with no witness at all ---------
A2="$(artof teetha r002)"
setf "$A2" 'verdict'      'VOID'
setf "$A2" 'void-reason'  'no-verdict'
setf "$A2" 'rc-line'      '0'
setf "$A2" 'verdict-line' '0'
setf "$A2" 'anchored-go'  '0'
setf "$A2" 'anchored-nogo' '0'
reseal "$A2"
expect_defect T68-void-without-witness teetha 'the evidence shows a well-formed round'
restore teetha
expect_report T68-restored teetha

# --- T69 a Tier A/B arc carrying a stray regression record ----------------
# (RVREG1 exists to represent the Tier A+ tier-up; one sitting in a tier-A arc
#  is either a mistake or a mis-declared tier, and must not sit unexamined)
cp "$PRISTINE/teethc"/reg-*.reg   "$MLFK_ARC_DIR/teetha/" 2>/dev/null || true
cp "$PRISTINE/teethc"/reg-*.log   "$MLFK_ARC_DIR/teetha/" 2>/dev/null || true
cp "$PRISTINE/teethc"/reg-*.scope "$MLFK_ARC_DIR/teetha/" 2>/dev/null || true
cp "$PRISTINE/teethc"/reg-*.cmd   "$MLFK_ARC_DIR/teetha/" 2>/dev/null || true
# as copied it also carries ANOTHER arc's id — an evidence defect in its own
# right, and the one that must be named first
expect_defect T69a-foreign-arc-regression teetha "declares arc-id 'teethc'"
# fully REBOUND to THIS arc (id, tier and all three sidecar paths, which are
# derived from the record's own basename), the record is coherent; what
# remains is that it does not belong in a tier-A arc at all — an OBSERVATION
# now, not a refusal. Rebinding matters: a tooth that "passes" because the
# perturber forgot a field proves nothing about the check it aims at.
RA="$(ls -1 "$MLFK_ARC_DIR/teetha"/reg-*.reg | head -1)"
RB="${RA%.reg}"
setf "$RA" 'arc-id'         'teetha'
setf "$RA" 'tier'           'A'
setf "$RA" 'scope-manifest' "$RB.scope"
setf "$RA" 'cmd-path'       "$RB.cmd"
setf "$RA" 'log-path'       "$RB.log"
reseal "$RA"
expect_observed T69b-stray-regression teetha 'regression record(s) in a tier-A arc'
restore teetha
expect_report T69-restored teetha

# --- T70 the harness refuses to produce a regression outside Tier A+ ------
RC=0
bash "$HARNESS" regression --arc teethz --tier A --scope "$WORK/scope1.txt" \
  --cmd "$WORK/reg-ok.sh" > "$OUT" 2>&1 || RC=$?
if [ "$RC" != "0" ] && LC_ALL=C grep -q 'must be A+' "$OUT"; then
  PASS=$((PASS + 1))
else
  fail T70-regression-tier "harness produced a tier-A regression record (rc=$RC)"
fi

# --- T71 a regression log with NUL bytes ----------------------------------
G="$(ls -1 "$MLFK_ARC_DIR/teethc"/reg-*.reg | head -1)"
GL="$(getf "$G" 'log-path')"
printf 'x\000y\n' >> "$GL"
setf "$G" 'log-sha256'    "$(shasum -a 256 "$GL" | cut -d' ' -f1)"
setf "$G" 'log-bytes'     "$(wc -c < "$GL" | tr -d ' ')"
setf "$G" 'log-lines'     "$(wc -l < "$GL" | tr -d ' ')"
setf "$G" 'log-nul-count' "$(LC_ALL=C tr -dc '\000' < "$GL" | wc -c | tr -d ' ')"
reseal "$G"
expect_defect T71-regression-nul teethc 'a corrupt run is not evidence'
restore teethc
expect_report T71-restored teethc

# --- T72 content appended to a regression log after the run ---------------
G="$(ls -1 "$MLFK_ARC_DIR/teethc"/reg-*.reg | head -1)"
GL="$(getf "$G" 'log-path')"
printf 'REGRESSION OK: (pasted afterwards)\n' >> "$GL"
setf "$G" 'log-sha256' "$(shasum -a 256 "$GL" | cut -d' ' -f1)"
setf "$G" 'log-bytes'  "$(wc -c < "$GL" | tr -d ' ')"
setf "$G" 'log-lines'  "$(wc -l < "$GL" | tr -d ' ')"
reseal "$G"
expect_defect T72-regression-appended teethc "last line is not this run's HARNESS-EOR"
restore teethc
expect_report T72-restored teethc

# --- T73 the archived regression COMMAND swapped after the run ------------
G="$(ls -1 "$MLFK_ARC_DIR/teethc"/reg-*.reg | head -1)"
GC="$(getf "$G" 'cmd-path')"
printf '#!/usr/bin/env bash\nexit 0\n' > "$GC"
expect_defect T73-regression-cmd-swapped teethc 'archived command does not match its recorded cmd-sha256'
restore teethc
expect_report T73-restored teethc

# --- T74 an overlength path in the scope manifest -------------------------
# (the judge's grammar must be the PRODUCER's grammar, bound included)
A2="$(artof teetha r002)"
M2="$(getf "$A2" 'scope-manifest')"
LONG="$(printf 'zzz'; i=0; while [ "$i" -lt 250 ]; do printf 'x'; i=$((i + 1)); done)"
printf '%s  %s\n' "$(shasum -a 256 "$SCOPEDIR/f1.txt" | cut -d' ' -f1)" "$LONG" >> "$M2"
setf "$A2" 'scope-count'  "$(wc -l < "$M2" | tr -d ' ')"
setf "$A2" 'scope-sha256' "$(shasum -a 256 "$M2" | cut -d' ' -f1)"
reseal "$A2"
expect_defect T74-manifest-overlength teetha 'does not match the pinned grammar'
restore teetha
expect_report T74-restored teetha

# --- T75 bytes appended to an artifact AFTER the seal, with no newline ----
# (`wc -l` counts newlines, so this leaves the line count unchanged)
A2="$(artof teetha r002)"
printf 'trailing junk with no newline' >> "$A2"
expect_defect T75-artifact-trailing-bytes teetha 'does not end with a newline'
restore teetha
expect_report T75-restored teetha

# --- T76 the same, on a round LOG ----------------------------------------
L2="$(getf "$(artof teetha r002)" 'log-path')"
printf 'VERDICT: GO' >> "$L2"
expect_defect T76-log-trailing-bytes teetha 'does not match its recorded sha256'
restore teetha
expect_report T76-restored teetha

# --- T77 the same, with the artifact re-derived to match ------------------
A2="$(artof teetha r002)"
L2="$(getf "$A2" 'log-path')"
printf 'VERDICT: GO' >> "$L2"
rebind "$A2"
expect_defect T77-log-trailing-rebound teetha 'does not end with a newline'
restore teetha
expect_report T77-restored teetha

# --- T78 a CR inside an artifact -----------------------------------------
A2="$(artof teetha r002)"
LC_ALL=C sed "s|^round: 2\$|round: 2$(printf '\r')|" "$A2" > "$WORK/cr.art"
cat "$WORK/cr.art" > "$A2"
reseal "$A2"
expect_defect T78-artifact-cr teetha 'contains CR'
restore teetha
expect_report T78-restored teetha

# --- T79 an orphan sidecar of the wrong family ----------------------------
# (a `.cmd` belongs to a reg- bundle; a `.prompt` to a round bundle)
cp "$(getf "$(artof teetha r002)" 'prompt-path')" "$MLFK_ARC_DIR/teetha/cap-20260101T000000Z-aaaaaaaa.prompt"
expect_defect T79-orphan-sidecar teetha 'unexpected entry in the arc directory'
restore teetha
expect_report T79-restored teetha

# --- T80 a VOID whose rc marker contradicts its recorded rc ---------------
# (a forged `reviewer-failed` VOID would otherwise license a §11 fallback)
A2="$(artof teethg r001)"
setf "$A2" 'reviewer-rc' '9'
reseal "$A2"
expect_defect T80-void-rc-marker teethg 'rc marker does not carry the recorded reviewer-rc'

# --- T81 a VOID fallback artifact whose basis is bogus --------------------
# (all-VOID fallback rounds are exempt from the PAIR rule, but never from
#  basis resolution)
round teethx 3 codex primary A rev-wedged.sh
X_BASIS="$(basename "$(artof teethx r003)")"
round teethx 3 grok fallback A rev-adorned.sh '' '' "$X_BASIS"
X_GROK="$(artof teethx r003-grok)"
setf "$X_GROK" 'fallback-basis' 'r003-codex-primary-20260101T000000Z-deadbeef.verdict'
reseal "$X_GROK"
expect_defect T81-void-fallback-bogus-basis teethx 'names no artifact in this arc'

# --- T82 PARTIAL reservation leaves no residue ----------------------------
# Deterministic without weakening anything: PATH shims make `date` and `od`
# return fixed values FOR THIS RUN ONLY, so the tooth can pre-create the
# THIRD reserved path and check that the first two were removed. The harness
# is unmodified; the nonce is still a real 16-byte draw in production.
mkdir -p "$WORK/shim"
printf '#!/usr/bin/env bash\nprintf "%%s\\n" "20260101T000000Z"\n' > "$WORK/shim/date"
printf '#!/usr/bin/env bash\nprintf "%%s\\n" " aa aa aa aa aa aa aa aa aa aa aa aa aa aa aa aa"\n' > "$WORK/shim/od"
chmod +x "$WORK/shim/date" "$WORK/shim/od"
PBASE="r007-codex-primary-20260101T000000Z-aaaaaaaa"
: > "$MLFK_ARC_DIR/teetha/$PBASE.prompt"      # the THIRD path reserve() takes
RC=0
PATH="$WORK/shim:$PATH" bash "$HARNESS" run --arc teetha --round 7 --reviewer codex \
  --role primary --tier A --prompt "$WORK/prompt.md" --scope "$WORK/scope1.txt" \
  --reviewer-cmd "$WORK/rev-go.sh" > "$OUT" 2>&1 || RC=$?
# The COLLISION DIAGNOSTIC is asserted before the residue claim. This tooth
# accepted ANY nonzero status, so it passed against a `false` binary and never
# reached reservation at all (measured, .loop/review-r4split-r1-20260801.log
# [MEDIUM]). Requiring rc 2 plus the anchored third-path message is what makes
# "the first two reservations were removed" mean anything.
if [ "$RC" != "2" ]; then
  fail T82-partial-reservation "the harness exited $RC on a colliding reservation, want EXACTLY 2 (any-nonzero would also accept a crash or a missing harness)"
elif ! LC_ALL=C grep -q "^HARNESS REFUSED: '.*/$PBASE.prompt' already exists or could not be created — refusing to reuse a round path$" "$OUT"; then
  fail T82-partial-reservation "the harness did not name the THIRD reserved path in its refusal: $(head -1 "$OUT")"
elif [ -e "$MLFK_ARC_DIR/teetha/$PBASE.log" ] || [ -e "$MLFK_ARC_DIR/teetha/$PBASE.scope" ]; then
  fail T82-partial-reservation 'a partial reservation left the earlier paths behind'
else
  PASS=$((PASS + 1))
fi
rm -f "$MLFK_ARC_DIR/teetha/$PBASE".*
restore teetha
expect_report T82-restored teetha

# --- T83 a wrong magic line ----------------------------------------------
A2="$(artof teetha r002)"
LC_ALL=C sed '1s|^RVERDICT2$|RVERDICTX|' "$A2" > "$WORK/magic.art"
cat "$WORK/magic.art" > "$A2"
reseal "$A2"
expect_defect T83-wrong-magic teetha "line 1 is not 'RVERDICT2'"
restore teetha
expect_report T83-restored teetha

# --- T84 trailing bytes on a REGRESSION log, artifact re-derived ----------
G="$(ls -1 "$MLFK_ARC_DIR/teethc"/reg-*.reg | head -1)"
GL="$(getf "$G" 'log-path')"
printf 'REGRESSION OK: (pasted, no newline)' >> "$GL"
setf "$G" 'log-sha256' "$(shasum -a 256 "$GL" | cut -d' ' -f1)"
setf "$G" 'log-bytes'  "$(wc -c < "$GL" | tr -d ' ')"
setf "$G" 'log-lines'  "$(wc -l < "$GL" | tr -d ' ')"
reseal "$G"
expect_defect T84-regression-trailing-bytes teethc 'does not end with a newline'
restore teethc
expect_report T84-restored teethc

# --- T85 a duplicate-terminator VOID with NO terminator at all ------------
# (the relaxation is only of the count rule, never of the pair's presence)
A2="$(artof teethf r001)"
[ "$(getf "$A2" 'void-reason')" = 'decision-mismatch' ] || true
A3="$(artof teethi r001)"
L3="$(getf "$A3" 'log-path')"
head -n 2 "$L3" > "$WORK/noterm.log"
cat "$WORK/noterm.log" > "$L3"
setf "$A3" 'void-reason' 'duplicate-terminator'
setf "$A3" 'log-sha256'  "$(shasum -a 256 "$L3" | cut -d' ' -f1)"
setf "$A3" 'log-bytes'   "$(wc -c < "$L3" | tr -d ' ')"
setf "$A3" 'log-lines'   "$(wc -l < "$L3" | tr -d ' ')"
reseal "$A3"
expect_defect T85-fake-duplicate-terminator teethi "last line is not this round's HARNESS-EOR"

# --- T86 a cross-wired bundle: one round's artifact naming another's log ---
A2="$(artof teetha r002)"
setf "$A2" 'log-path' "$(getf "$(artof teetha r001)" 'log-path')"
setf "$A2" 'log-sha256' "$(getf "$(artof teetha r001)" 'log-sha256')"
setf "$A2" 'log-bytes'  "$(getf "$(artof teetha r001)" 'log-bytes')"
setf "$A2" 'log-lines'  "$(getf "$(artof teetha r001)" 'log-lines')"
reseal "$A2"
expect_defect T86-cross-wired-bundle teetha 'not its own'
restore teetha
expect_report T86-restored teetha

# --- T87 a `..` traversal path in the scope manifest ----------------------
A2="$(artof teetha r002)"
M2="$(getf "$A2" 'scope-manifest')"
printf '%s  %s/../escape.txt\n' "$(shasum -a 256 "$SCOPEDIR/f1.txt" | cut -d' ' -f1)" "$SCOPEDIR" >> "$M2"
setf "$A2" 'scope-count'  "$(wc -l < "$M2" | tr -d ' ')"
setf "$A2" 'scope-sha256' "$(shasum -a 256 "$M2" | cut -d' ' -f1)"
reseal "$A2"
expect_defect T87-manifest-traversal teetha "contains '..'"
restore teetha
expect_report T87-restored teetha

# --- T88 a Tier A+ second opinion on a DIFFERENT prompt still closes ------
# (§11's same-prompt rule binds the FALLBACK PAIR; §3 binds a second opinion
#  by independence, and imposing prompt identity on it would reject every
#  legitimate A+ arc)
rm -f "$MLFK_ARC_DIR/teethc"/r001-opus5-*
round teethc 1 opus5 second-opinion 'A+' rev-go.sh '' "$WORK/prompt2.md"
expect_report T88-aplus-second-own-prompt teethc
restore teethc
expect_report T88-restored teethc

# =========================================================================
# Teeth added 2026-07-31, closing the Tier A+ independent review's findings
# (.loop/review-r4-tierA-opus-20260731.log). Three of them — T89, T92/T93 and
# T94/T95 — are FAIL-OPEN provers for shapes that CLOSED AN ARC before this
# change; the rest cover checks the reviewer's OFF-control sweep found had no
# tooth at all (M1).
# =========================================================================

# --- T89 the round LABELS contradict the recorded times (H1) --------------
# THE MEASURED ATTACK: run the GO first, labelled round 9; run the
# substantive NO-GO two seconds later, labelled round 1. `max(round)` picked
# the GO and printed a green closure line over the arc's NEWEST review. No
# file editing, no repo write — two ordinary `--round` flags.
#
# RE-POINTED 2026-07-31 (owner ruling). The rule that used to refuse this
# read only `started-utc`, and the SECOND adversarial review broke it in one
# second: a GO that STARTED after the adverse round and FINISHED six seconds
# before it satisfied the monotonicity check, as did two rounds launched in
# the same second. The rule is DELETED, not reported. What these teeth now
# require is that the DIAGNOSTIC leaves the human able to make the call it no
# longer makes: both timestamps of every artifact must be on the page, and
# the tool must say in its own output that order is not established.
round teethro 9 codex primary A rev-go.sh
sleep 1   # the path stamp has one-second resolution; force a real gap
round teethro 1 codex primary A rev-nogo.sh
# Produce the report T89b reads. This call used to be made by
# `expect_observed T89a-...`, which was DELETED as a dead tooth (it asserted
# footer boilerplate arc-report.sh prints unconditionally). Deleting it also
# removed this SIDE EFFECT and broke T89b — measured by running the suite.
# The generation is kept; only the false PASS is gone.
arcreport teethro
# and the raw ordering evidence itself, both ends, for both rounds
RO9="$(LC_ALL=C grep -oE 'round 9 codex primary: GO .* started=[0-9]{4}-[0-9]{2}-[0-9]{2}T[0-9:]{8}Z ended=[0-9]{4}-[0-9]{2}-[0-9]{2}T[0-9:]{8}Z' "$OUT" || true)"
RO1="$(LC_ALL=C grep -oE 'round 1 codex primary: NO-GO .* started=[0-9]{4}-[0-9]{2}-[0-9]{2}T[0-9:]{8}Z ended=[0-9]{4}-[0-9]{2}-[0-9]{2}T[0-9:]{8}Z' "$OUT" || true)"
S9="${RO9##*started=}"; S9="${S9%% *}"
S1="${RO1##*started=}"; S1="${S1%% *}"
if [ -n "$RO9" ] && [ -n "$RO1" ] && [ -n "$S9" ] && [ -n "$S1" ] && [ "$S9" \< "$S1" ]; then
  PASS=$((PASS + 1))
else
  fail T89b-round-times-disclosed \
    "the report does not disclose both ends of both rounds' times (round 9 '$RO9' / round 1 '$RO1')"
fi

# --- T90 started-utc re-labelled in place ---------------------------------
# The times are only ordering evidence if they cannot be edited without
# leaving a mark. The path stamp is drawn from the SAME clock reading.
A2="$(artof teetha r002)"
setf "$A2" 'started-utc' '2020-01-01T00:00:00Z'
reseal "$A2"
expect_defect T90-started-utc-vs-path teetha "its path's UTC stamp"
restore teetha
expect_report T90-restored teetha

# --- T91 the round label re-written in place ------------------------------
A2="$(artof teetha r002)"
setf "$A2" 'round' '5'
reseal "$A2"
expect_defect T91-round-vs-path teetha 'its path says'
restore teetha
expect_report T91-restored teetha

# --- T111 the path's nonce stem no longer matches the recorded nonce ------
# ADDED BECAUSE ITS OFF CONTROL DID NOT BITE. The first sweep of the new
# checks left `no-path-nonce` GREEN — the third component of the path binding
# had no tooth at all, exactly the gap T56 was written for on an earlier pass.
# Measured, then closed; the control now fails open as it should.
A2="$(artof teetha r002)"
setf "$A2" 'eor-nonce' "$(printf '0%.0s' $(seq 1 32))"
reseal "$A2"
expect_defect T111-nonce-stem-vs-path teetha 'nonce stem'
restore teetha
expect_report T111-restored teetha

# --- T92 a transcript-decided round that emits BOTH verdicts is VOIDed ----
# (H2 — failure mode #1 on the paths that have NO decision channel. The
#  producer must refuse to read this positionally.)
rm -f "$MLFK_ARC_DIR/teethc"/r001-opus5-*
round teethc 1 opus5 second-opinion 'A+' rev-launder.sh
LA="$(artof teethc r001-opus5)"
if [ "$(getf "$LA" 'verdict')" = 'VOID' ] && [ "$(getf "$LA" 'void-reason')" = 'ambiguous-verdict' ]; then
  PASS=$((PASS + 1))
else
  fail T92-ambiguous-transcript-voided \
    "the harness read a laundered transcript as '$(getf "$LA" 'verdict')' / '$(getf "$LA" 'void-reason')'"
fi

# --- T93 the JUDGE's own layer for the same shape --------------------------
# The producer's VOID is rewritten into the GO it would have been read as,
# CONSISTENTLY (verdict, verdict line, rc line and both in-region counts all
# match the log on disk), so the refusal can only come from the ambiguity
# rule itself.
LL="$(getf "$LA" 'log-path')"
LN="$(wc -l < "$LL" | tr -d ' ')"
setf "$LA" 'verdict'       'GO'
setf "$LA" 'verdict-line'  "$(LC_ALL=C grep -a -n '^VERDICT: GO$' "$LL" | tail -1 | cut -d: -f1)"
setf "$LA" 'rc-line'       "$((LN - 1))"
setf "$LA" 'anchored-go'   "$(LC_ALL=C grep -a -c '^VERDICT: GO$' "$LL" | tr -d ' ')"
setf "$LA" 'anchored-nogo' "$(LC_ALL=C grep -a -c '^VERDICT: NO-GO$' "$LL" | tr -d ' ')"
setf "$LA" 'void-reason'   '-'
reseal "$LA"
expect_defect T93-judge-ambiguous-layer teethc 'BOTH an anchored GO'
restore teethc
expect_report T93-restored teethc

# --- T94 a caller-TRUNCATED deadline cannot license §11's fallback (H3) ----
# `--timeout-sec 1` produces a real, well-formed `VOID / timeout` codex
# record in seconds. Before this change that record licensed a grok+opus5
# dual GO — i.e. one flag manufactured §11's "codex proven failed"
# precondition.
bash "$HARNESS" run --arc teethto --round 3 --reviewer codex --role primary \
  --tier A --prompt "$WORK/prompt.md" --scope "$WORK/scope1.txt" \
  --reviewer-cmd "$WORK/rev-slow.sh" --timeout-sec 1 > /dev/null 2>&1
TO_ART="$(artof teethto r003)"
if [ "$(getf "$TO_ART" 'void-reason')" = 'timeout' ] \
   && [ "$(getf "$TO_ART" 'harness-kill')" = 'harness-timeout' ] \
   && [ "$(getf "$TO_ART" 'timeout-sec')" = '1' ]; then
  PASS=$((PASS + 1))
else
  fail T94-setup "the truncated-deadline round was not recorded as a harness timeout \
(reason '$(getf "$TO_ART" 'void-reason')', kill '$(getf "$TO_ART" 'harness-kill')', \
deadline '$(getf "$TO_ART" 'timeout-sec')')"
fi
TO_BASIS="$(basename "$TO_ART")"
round teethto 3 grok  fallback A rev-go.sh '' '' "$TO_BASIS"
round teethto 3 opus5 fallback A rev-go.sh '' '' "$TO_BASIS"
# RE-POINTED 2026-07-31 (owner ruling). The eligibility rule that refused
# this shape was a two-item blacklist over eight VOID reasons, and the second
# adversarial review manufactured two fresh ELIGIBLE bases in zero seconds
# (codex off PATH -> rc 127 `reviewer-failed`; one sentence of prompt -> rc 0
# `decision-malformed`). It is DELETED. The diagnostic must instead put the
# whole basis on the page — deadline, who killed it, rc, both timestamps —
# and say that "PROVEN failed" is not its call.
# Report generation, formerly a SIDE EFFECT of the deleted dead tooth
# (see the T89 note above). Kept; only the false PASS is gone.
arcreport teethto
if LC_ALL=C grep -qE 'round 3 codex primary: VOID void-reason=timeout rc=[0-9]+ timeout-sec=1 harness-kill=harness-timeout ' "$OUT"; then
  PASS=$((PASS + 1))
else
  fail T94b-truncated-deadline-disclosed \
    "the report does not disclose the basis's 1-second deadline: $(LC_ALL=C grep -a 'round 3 codex primary' "$OUT" | head -1)"
fi

# --- T95 a claimed timeout that never reached its own deadline ------------
setf "$TO_ART" 'timeout-sec' '999999'
reseal "$TO_ART"
expect_defect T95-timeout-shorter-than-deadline teethto 'the round never reached it'

# --- T96 a reviewer KILLED FROM OUTSIDE cannot license §11's fallback -----
# Same effect as T94 with no flag at all: kill the adverse in-flight round.
# The harness records `reviewer-failed` with `harness-kill: none`, and a
# signal-range rc with nobody admitting to the kill is not a proven failure.
round teethkl 3 codex primary A rev-killed.sh
KL_BASIS="$(basename "$(artof teethkl r003)")"
round teethkl 3 grok  fallback A rev-go.sh '' '' "$KL_BASIS"
round teethkl 3 opus5 fallback A rev-go.sh '' '' "$KL_BASIS"
# RE-POINTED with T94: the rc/harness-kill facts are DISCLOSED, the inference
# ("killed from outside, therefore not a proven failure") is the human's.
# Report generation, formerly a SIDE EFFECT of the deleted dead tooth
# (see the T89 note above). Kept; only the false PASS is gone.
arcreport teethkl
if LC_ALL=C grep -qE 'round 3 codex primary: VOID void-reason=reviewer-failed rc=143 timeout-sec=[0-9]+ harness-kill=none ' "$OUT"; then
  PASS=$((PASS + 1))
else
  fail T96b-outside-kill-disclosed \
    "the report does not disclose the signal rc with harness-kill=none: $(LC_ALL=C grep -a 'round 3 codex primary' "$OUT" | head -1)"
fi

# --- T97 harness-kill forged onto an ordinary round -----------------------
A2="$(artof teetha r002)"
setf "$A2" 'harness-kill' 'harness-timeout'
reseal "$A2"
expect_defect T97-harness-kill-coherence teetha 'a harness-killed round is a timeout'
restore teetha
expect_report T97-restored teetha

# --- T98 closure-rule forged on a PRIMARY artifact (M1 fail-open #1) ------
# Probe-proven fail-open in the Tier A+ review: deleting the three
# role<->closure-rule lines left the whole suite green.
A2="$(artof teetha r002)"
setf "$A2" 'closure-rule' 'process11-fallback-dual-go'
reseal "$A2"
expect_defect T98-primary-rule-coherence teetha 'role primary must carry closure-rule'
restore teetha
expect_report T98-restored teetha

# --- T99 closure-rule forged on a FALLBACK artifact -----------------------
GA="$(artof teethb r003-grok)"
setf "$GA" 'closure-rule' 'process3-tier-a-go'
reseal "$GA"
expect_defect T99-fallback-rule-coherence teethb 'role fallback must carry closure-rule'
restore teethb
expect_report T99-restored teethb

# --- T100 closure-rule forged on a SECOND-OPINION artifact ----------------
SA="$(artof teethc r001-opus5)"
setf "$SA" 'closure-rule' 'process3-tier-a-go'
reseal "$SA"
expect_defect T100-second-rule-coherence teethc 'role second-opinion must carry closure-rule'
restore teethc
expect_report T100-restored teethc

# --- T101 a round that mixes a primary GO with a §11 fallback pair --------
# (M1 fail-open #2. Built entirely through the SANCTIONED CLI — no file
#  editing at all — and exactly one untoothed line refused it.)
round teethmx 4 codex primary A rev-wedged.sh
MX_BASIS="$(basename "$(artof teethmx r004)")"
round teethmx 4 grok  fallback A rev-go.sh '' '' "$MX_BASIS"
round teethmx 4 opus5 fallback A rev-go.sh '' '' "$MX_BASIS"
round teethmx 4 codex primary A rev-go.sh
expect_observed T101-mixed-rule teethmx 'mixes a primary GO with a §11 fallback'

# --- T102 the in-region anchored counts disagree with the log -------------
# (the very fields H2 shows the judge was recording and never deciding on)
A2="$(artof teetha r002)"
setf "$A2" 'anchored-nogo' '3'
reseal "$A2"
expect_defect T102-inregion-counts teetha 'in-region verdict counts'
restore teetha
expect_report T102-restored teetha

# --- T103 one artifact of the arc declares a different TIER ---------------
# (rebound through its own archived prompt envelope, so the refusal is the
#  tier DISAGREEMENT and not the envelope-grep layer)
SA="$(artof teethc r001-opus5)"
SP="$(getf "$SA" 'prompt-path')"
LC_ALL=C sed 's|^\(arc-id: teethc · round: 1 · tier: \)A+$|\1A|' "$SP" > "$WORK/tier.prompt"
cat "$WORK/tier.prompt" > "$SP"
setf "$SA" 'tier' 'A'
setf "$SA" 'prompt-sha256' "$(shasum -a 256 "$SP" | cut -d' ' -f1)"
reseal "$SA"
expect_defect T103-tier-agreement teethc 'the arc'"'"'s other artifacts declare'
restore teethc
expect_report T103-restored teethc

# --- T104 a fallback-only reviewer holding role primary (judge layer) -----
A2="$(artof teetha r002)"
setf "$A2" 'reviewer' 'grok'
reseal "$A2"
expect_defect T104-primary-reviewer-coherence teetha 'cannot hold role primary'
restore teetha
expect_report T104-restored teetha

# --- T105 scope-count disagrees with the manifest -------------------------
A2="$(artof teetha r002)"
setf "$A2" 'scope-count' '9'
reseal "$A2"
expect_defect T105-scope-count teetha 'line count != scope-count'
restore teetha
expect_report T105-restored teetha

# --- T106 NUL bytes in the decision channel -------------------------------
A2="$(artof teetha r002)"
D2="$(getf "$A2" 'decision-path')"
printf 'x\000y\n' >> "$D2"
setf "$A2" 'decision-sha256' "$(shasum -a 256 "$D2" | cut -d' ' -f1)"
reseal "$A2"
expect_defect T106-decision-nul teetha 'decision channel carries NUL'
restore teetha
expect_report T106-restored teetha

# --- T107 verdict-line disagrees with the log -----------------------------
A2="$(artof teetha r002)"
setf "$A2" 'verdict-line' '1'
reseal "$A2"
expect_defect T107-verdict-line teetha 'terminal verdict is at log line'
restore teetha
expect_report T107-restored teetha

# --- T108 two cap artifacts in one arc ------------------------------------
CAP1="$(ls -1 "$MLFK_ARC_DIR/teethd"/cap-*.cap | head -1)"
cp "$CAP1" "$MLFK_ARC_DIR/teethd/cap-20260731T000000Z-abcdef01.cap"
expect_defect T108-cap-singleton teethd 'a cap is a singleton'
restore teethd
expect_report T108-restored teethd

# --- T109 a record written by an OLDER format version (M3) ----------------
# It is well-formed evidence this judge cannot read — which is a different
# fact from corruption, and the one that made this item's own seven-round arc
# report "expected 35 lines, measured 31".
A2="$(artof teetha r002)"
LC_ALL=C sed '1s|^RVERDICT2$|RVERDICT1|; s|^END RVERDICT2$|END RVERDICT1|' "$A2" > "$WORK/old.art"
cat "$WORK/old.art" > "$A2"
reseal "$A2"
expect_defect T109-older-format teetha 'was written by an older format'
restore teetha
expect_report T109-restored teetha

# --- T110 a `..` in an evidence PATH field --------------------------------
A2="$(artof teetha r002)"
setf "$A2" 'log-path' "$(dirname "$(getf "$A2" 'log-path')")/../escape.log"
reseal "$A2"
expect_defect T110-path-traversal-field teetha "path contains '..'"
restore teetha
expect_report T110-restored teetha

# ================================================================= verdict
TOTAL=$((PASS + FAILED))
if [ "$FAILED" != "0" ]; then
  printf 'REVIEW ARTIFACT TEETH FAILED (%s/%s passed, %s failed)\n' "$PASS" "$TOTAL" "$FAILED"
  exit 1
fi
printf 'REVIEW ARTIFACT TEETH OK (%s/%s bit)\n' "$PASS" "$TOTAL"
exit 0
