#!/usr/bin/env bash
# port/review/arc-report.sh — a DIAGNOSTIC READER for fix_plan R4.
#
# IT DOES NOT DECIDE WHETHER A REVIEW ARC IS CLOSED. Arc closure is a
# driver/human judgement, informed by these artifacts (owner ruling
# 2026-07-31, docs/STATE.md §rulings: "keep the PRODUCER, drop the JUDGE'S
# AUTHORITY"). This script reads the harness-written RVERDICT2/RVCAP1/RVREG1
# artifacts and reports two things and nothing else:
#
#   1. EVIDENCE DEFECTS — the recorded bytes are not internally consistent
#      (bad seal, mutated log, cross-wired bundle, NULs, version skew,
#      incoherent VOID reason, artifacts that contradict each other). A
#      defect means "this cannot be read as evidence", not "not closed".
#   2. OBSERVATIONS — what the artifacts SAY: every round's reviewer, role,
#      verdict, times, rc, deadline and scope; whether a §11 fallback pair is
#      complete; what a cap claims; whether the reviewed bytes are still on
#      disk. Facts, with no conclusion drawn from them.
#
#   bash port/review/arc-report.sh --arc <id> [--tier A|A+|B] [--synthetic-ok]
#
#   ARC REPORT <id> …<observations>…                                  (exit 0)
#   ARC REPORT <id>: EVIDENCE DEFECT — <reason>                       (exit 1)
#   ARC REPORT REFUSED: <reason>                                      (exit 2)
#
# EXIT 0 MEANS "A REPORT WAS PRODUCED", NEVER "THE ARC IS CLOSED", and there
# is deliberately NO output line and NO exit code that authorizes anything.
# Do not wire this into a gate, a `&&` chain, or a manifest cite as a pass
# condition: three independent adversarial reviews (2026-07-31,
# .loop/review-r4-tierA{,2}-opus-20260731.log) each found fresh paths to a
# green `ARC CLOSED` line over a review that had not happened, and the owner's
# ruling was that a judge with known false-GREEN paths is worse than no judge
# precisely because the process had declared it authoritative.
#
# WHAT WAS DELETED RATHER THAN DEMOTED, because it was measured UNSOUND and a
# reported observation derived from an unsound rule is still an unsound claim:
#   * the round-order rule (label order vs `started-utc`). It never read
#     `ended-utc`, so a GO that STARTED one second after an adverse round and
#     FINISHED six seconds before it still satisfied it. This script now
#     prints both timestamps for every artifact and states plainly that round
#     ORDER is not established.
#   * §11 basis eligibility (a two-item blacklist over eight VOID reasons).
#     Two eligible "proven codex failures" were manufactured in zero seconds
#     with no repo write (codex off PATH -> rc 127 `reviewer-failed`; one
#     sentence of prompt -> rc 0 `decision-malformed`). This script now prints
#     the VOID reason, rc, deadline, harness-kill and elapsed time, and states
#     that whether that is a PROVEN codex failure is not decided here.
#
# The evidence-defect layer is unchanged and stays FAIL-CLOSED: missing,
# malformed, NUL-contaminated, mutated, duplicated, incompletely-bundled,
# self-seal-broken or cross-wired evidence exits nonzero and prints no
# observations. Every accept in that layer is the end of a chain of exact,
# anchored, full-line matches over re-derived values.
#
# SCOPE OF THE "re-derived" CLAIM, stated precisely: every field that can be
# re-derived FROM THE ARC'S OWN EVIDENCE is re-derived and compared. The
# fields that cannot be, and are TRUSTED rather than verified, are listed
# exhaustively in port/review/FORMAT.md §7 — they are `harness-sha256`, the
# timestamps in all three formats, the VOID reason `timeout`, RVCAP1's
# `authorized-by` and `recurring-class`, and each round's `reviewer`/`role`.
# They rest on the self-seal. Do not restate a shorter list here: an
# understated trust boundary is itself a false green.
#
# What it deliberately does NOT defend against: an actor with write access to
# this repo, who can recompute any seal and edit this script (PROCESS §3 —
# dispositioned in writing, not defended in-script). The threat model is
# accident, corruption, partial failure, staleness and self-deception.

set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=port/review/reviewers.sh
. "$HERE/reviewers.sh"

ARC=''
refuse()  { printf 'ARC REPORT REFUSED: %s\n' "$*"; exit 2; }
# An EVIDENCE DEFECT is "these bytes cannot be read as evidence" — never
# "this arc is not closed". Nothing here rules on closure.
defect() { printf 'ARC REPORT %s: EVIDENCE DEFECT — %s\n' "$ARC" "$*"; exit 1; }

# Observations accumulate and are printed together at the end, AFTER the
# report header, so no single line of this script's output can be quoted as
# an authorization.
OBS=''
obs() { OBS="$OBS  - $*
"; }

sha_file() { shasum -a 256 "$1" | cut -d' ' -f1; }
sha_str()  { printf '%s' "$1" | shasum -a 256 | cut -d' ' -f1; }

# grepc <pattern> <file> — anchored count that is 0, not a pipeline failure,
# on zero matches. Zero matches is a JUDGEMENT ("this log has no verdict"),
# never an abort, so it must not interact with `set -o pipefail`.
grepc() {
  local n
  n="$(LC_ALL=C grep -a -c -- "$1" "$2" 2>/dev/null || true)"
  [ -n "$n" ] || n=0
  printf '%s' "$n" | tr -d ' '
}

# terminal_verdict <file> <upto-line> — "<line>:<GO|NO-GO>" for the LAST
# anchored, unadorned, column-0 verdict at or before <upto-line>, else empty.
# Byte-for-byte the rule the harness used; both sides must agree.
terminal_verdict() {
  local f="$1" upto="$2" region last
  region="$(mktemp)"
  head -n "$upto" "$f" > "$region"
  last="$(LC_ALL=C grep -a -nE '^VERDICT: (GO|NO-GO)$' "$region" | tail -1 || true)"
  rm -f "$region"
  [ -n "$last" ] || return 0
  printf '%s:%s' "${last%%:*}" "${last#*:VERDICT: }"
}

# validate_manifest <owner-artifact> <manifest> — the scope manifest is
# decision-bearing parsed text, so PROCESS §3's whitelist-grammar rule applies
# to it too: every line must match the producer's EXACT grammar
# (`<64 hex>  <path>`), paths must be byte-wise sorted, unique, free of `..`
# and within the path grammar. Anything that merely resembles the format is
# corruption, not something to parse around.
validate_manifest() {
  local a="$1" m="$2" prev='' line sum path
  [ -s "$m" ] || defect "artifact '$a': scope manifest '$m' is empty"
  while IFS= read -r line || [ -n "$line" ]; do
    printf '%s' "$line" | LC_ALL=C grep -qE '^[0-9a-f]{64}  [A-Za-z0-9_.][A-Za-z0-9._/-]{0,239}$' \
      || defect "artifact '$a': scope manifest line does not match the pinned grammar: '$line'"
    sum="${line%%  *}"; path="${line#*  }"
    case "$path" in *..*) defect "artifact '$a': scope manifest path contains '..': '$path'" ;; esac
    if [ -n "$prev" ]; then
      [ "$path" != "$prev" ] \
        || defect "artifact '$a': scope manifest lists '$path' twice"
      [ "$(printf '%s\n%s\n' "$prev" "$path" | LC_ALL=C sort | head -1)" = "$prev" ] \
        || defect "artifact '$a': scope manifest is not byte-wise sorted ('$prev' before '$path')"
    fi
    prev="$path"
  done < "$m"
}

ROOT="$(git rev-parse --show-toplevel 2>/dev/null || true)"
[ -n "$ROOT" ] || refuse "not inside a git worktree"
cd "$ROOT"

WANT_TIER=''
SYNTHETIC_OK=0
ARCDIR="${MLFK_ARC_DIR:-.loop/arc}"
usage() {
  cat <<'USAGE'
arc-report.sh — DIAGNOSTIC reader for review-arc artifacts. It REPORTS; it
does NOT decide whether an arc is closed.

  bash port/review/arc-report.sh --arc <id> [--tier A|A+|B] [--synthetic-ok]

  ARC REPORT <id> …observations…                exit 0  a report was produced
  ARC REPORT <id>: EVIDENCE DEFECT — <reason>   exit 1  bytes unreadable as evidence
  ARC REPORT REFUSED: <reason>                  exit 2  cannot even start

ARC CLOSURE IS A DRIVER/HUMAN JUDGEMENT (owner ruling 2026-07-31). Exit 0
means "a report was produced", never "the arc is closed"; there is no output
line and no exit code here that authorizes anything. This tool does NOT
decide: which round is newest, whether a codex round is PROVEN failed under
PROCESS §11, whether a Tier A+ tier-up is discharged, or whether the reviewed
scope is the right scope. It prints the evidence for those questions and
names them as open. Read port/review/FORMAT.md §7 (binding disclosure) before
relying on anything below its header line.
USAGE
  exit 0
}
while [ $# -gt 0 ]; do
  case "$1" in
    --arc)          ARC="${2:-}"; shift 2 ;;
    --tier)         WANT_TIER="${2:-}"; shift 2 ;;
    --arc-dir)      ARCDIR="${2:-}"; shift 2 ;;
    --synthetic-ok) SYNTHETIC_OK=1; shift ;;
    -h|--help)      usage ;;
    *) refuse "unknown argument '$1'" ;;
  esac
done
printf '%s' "$ARC" | LC_ALL=C grep -qE '^[a-z0-9][a-z0-9._-]{0,63}$' \
  || refuse "--arc '$ARC' fails the arc-id grammar"
# The arc ROOT gets the same grammar the producer applies to it
# (review-harness.sh: repo-relative, no `..`). This reader used to accept
# `--arc-dir` and `MLFK_ARC_DIR` unvalidated while the producer validated
# them — an asymmetry, and PROCESS §3's whitelist-grammar rule is not
# conditional on which end of the pipe is reading.
printf '%s' "$ARCDIR" | LC_ALL=C grep -qE '^[A-Za-z0-9_.][A-Za-z0-9._/-]{0,200}$' \
  || refuse "arc root '$ARCDIR' must be a repo-relative path"
case "$ARCDIR" in *..*) refuse "arc root '$ARCDIR' must not contain '..'" ;; esac
if [ -n "$WANT_TIER" ]; then
  case "$WANT_TIER" in A|A+|B) : ;; *) refuse "--tier must be A, A+ or B" ;; esac
fi

DIR="$ARCDIR/$ARC"
[ -e "$DIR" ] || defect "no arc directory at '$DIR' (a round with no harness artifact is not a round)"
if [ -L "$DIR" ]; then defect "arc directory '$DIR' is a symlink"; fi
[ -d "$DIR" ] || defect "'$DIR' is not a directory"

# ------------------------------------------------ arc-directory whitelist
# Anything the harness did not write is a tell (a leftover .tmp is a partial
# write; anything else is a hand-placed file). Refuse rather than ignore.
# `for f in "$DIR"/*` misses dotfiles and `.[!.]*` misses names starting `..`,
# so entries are enumerated with `ls -A`, which lists every name exactly once.
# Membership is EXACT, not merely suffix-shaped: each suffix must belong to a
# base of the right family, so an orphan `foo.cmd` or a `cap-*.prompt` is a
# refusal rather than ignored debris.
while IFS= read -r nm; do
  [ -n "$nm" ] || continue
  f="$DIR/$nm"
  case "$nm" in
    r[0-9][0-9][0-9]-*.verdict|r[0-9][0-9][0-9]-*.log|r[0-9][0-9][0-9]-*.prompt|r[0-9][0-9][0-9]-*.scope|r[0-9][0-9][0-9]-*.decision) : ;;
    cap-*.cap|cap-*.scope) : ;;
    reg-*.reg|reg-*.scope|reg-*.log|reg-*.cmd) : ;;
    *) defect "unexpected entry in the arc directory: '$f'" ;;
  esac
  if [ -L "$f" ]; then defect "arc-directory symlink: '$f'"; fi
  [ -f "$f" ] || defect "'$f' is not a regular file"
done <<EOF
$(ls -A "$DIR" 2>/dev/null || true)
EOF

# ------------------------------------------------ complete bundles only
# A killed run leaves .log/.prompt/.scope with no .verdict. Judging around
# such a bundle is exactly how a stale GO survives a newer, failed attempt —
# so an incomplete bundle refuses the whole arc. (An arc with a round in
# flight is, correctly, not closed.)
for f in "$DIR"/*.log "$DIR"/*.prompt "$DIR"/*.scope "$DIR"/*.decision; do
  [ -e "$f" ] || continue
  b="${f%.*}"
  case "$b" in
    "$DIR"/cap-*) continue ;;   # the cap bundle is <base>.cap + <base>.scope
    "$DIR"/reg-*) continue ;;   # regression bundle: .reg + .scope + .log + .cmd
  esac
  [ -f "$b.verdict" ] \
    || defect "'$f' has no matching '$b.verdict' — an incomplete round bundle (a killed or in-flight run)"
done
for f in "$DIR"/cap-*.scope; do
  [ -e "$f" ] || continue
  b="${f%.*}"
  [ -f "$b.cap" ] || defect "'$f' has no matching '$b.cap' — an incomplete cap bundle"
done
for f in "$DIR"/reg-*.scope "$DIR"/reg-*.log "$DIR"/reg-*.cmd; do
  [ -e "$f" ] || continue
  b="${f%.*}"
  [ -f "$b.reg" ] || defect "'$f' has no matching '$b.reg' — an incomplete regression bundle"
done

# ============================================================ artifact I/O
# An RVERDICT2 file is EXACTLY 37 lines: magic, 34 `key: value` lines in this
# exact order, "END RVERDICT2", then the seal. Positional + anchored: body
# line i+1 must be "<key>: <value>" with <key> equal to KEY[i] and <value>
# matching RE[i]. Anything else is corruption, and corruption is a refusal.
#
# THE MAGIC IS THE VERSION. RVERDICT1 carried a 29-field grammar, then a
# 32-field one, under one magic — and this reader reported the skew as
# "expected 35 lines, measured 31 (truncated or extended)", i.e. as
# CORRUPTION. A format that cannot tell its own older records from damaged
# ones cannot read its own history, so: any change to the field list bumps
# the magic, and `read_artifact` names a version skew distinctly (below).
KEY=(  ''  arc-id round reviewer role tier closure-rule fallback-basis \
       reviewer-cmd-sha256 scope-count scope-sha256 scope-manifest \
       prompt-sha256 prompt-path log-path log-sha256 log-bytes log-lines \
       log-nul-count decision-source decision-path decision-sha256 \
       eor-nonce reviewer-rc rc-line verdict verdict-line anchored-go \
       anchored-nogo void-reason timeout-sec harness-kill harness-sha256 \
       started-utc ended-utc )
RE_ARCID='^[a-z0-9][a-z0-9._-]{0,63}$'
RE_SHA='^[0-9a-f]{64}$'
RE_UINT='^[0-9]{1,12}$'
RE_PATH='^[A-Za-z0-9_.][A-Za-z0-9._/-]{0,239}$'
RE_UTC='^[0-9]{4}-[0-9]{2}-[0-9]{2}T[0-9]{2}:[0-9]{2}:[0-9]{2}Z$'
RE=(   ''  "$RE_ARCID" '^[1-9][0-9]{0,2}$' '^(codex|grok|opus5)$' \
       '^(primary|fallback|second-opinion)$' '^(A|A\+|B)$' \
       '^(process3-tier-a-go|process11-fallback-dual-go|process3-tier-aplus-second)$' \
       '^(-|[A-Za-z0-9][A-Za-z0-9._-]{0,119})$' \
       "$RE_SHA" "$RE_UINT" "$RE_SHA" "$RE_PATH" "$RE_SHA" "$RE_PATH" \
       "$RE_PATH" "$RE_SHA" "$RE_UINT" "$RE_UINT" "$RE_UINT" \
       '^(last-message|transcript)$' "$RE_PATH" "$RE_SHA" \
       '^[0-9a-f]{32}$' '^[0-9]{1,3}$' "$RE_UINT" '^(GO|NO-GO|VOID)$' \
       "$RE_UINT" "$RE_UINT" "$RE_UINT" \
       '^(-|nul-bytes|no-verdict|timeout|duplicate-terminator|decision-empty|decision-malformed|decision-mismatch|reviewer-failed|ambiguous-verdict)$' \
       '^[1-9][0-9]{0,5}$' '^(none|harness-timeout)$' \
       "$RE_SHA" "$RE_UTC" "$RE_UTC" )
NFIELD=34
NBODY=$((NFIELD + 2))

# Values of the artifact currently being validated land in F[1..NFIELD].
F=()

# read_artifact <file> <magic> <nbody>
#   nbody = number of body lines INCLUDING the magic and the END line.
# Verifies: regular non-empty file, no NULs, no CR, exact line count, magic
# first line, "END <magic>" at line nbody, seal line last and correct.
read_artifact() {
  local f="$1" magic="$2" nbody="$3" nul lines seal want body got1 family
  [ -f "$f" ] || defect "artifact '$f' is missing or not a regular file"
  [ -s "$f" ] || defect "artifact '$f' is empty"
  nul="$(LC_ALL=C tr -dc '\000' < "$f" | wc -c | tr -d ' ')"
  [ "$nul" = "0" ] || defect "artifact '$f' contains $nul NUL byte(s)"
  if LC_ALL=C grep -q $'\r' "$f"; then defect "artifact '$f' contains CR"; fi
  # VERSION SKEW IS NOT CORRUPTION, and must not be reported as it. A record
  # written by an older grammar under an older magic is well-formed evidence
  # this reader cannot read — a completely different fact from a truncated or
  # padded file, and the one that made this item's OWN seven-round arc
  # unreadable ("expected 35 lines, measured 31"). Checked FIRST, before the
  # line count, because the line count is exactly the misleading symptom.
  got1="$(sed -n '1p' "$f")"
  if [ "$got1" != "$magic" ]; then
    family="$(printf '%s' "$magic" | LC_ALL=C sed 's/[0-9]*$//')"
    if printf '%s' "$got1" | LC_ALL=C grep -qE "^${family}[0-9]+\$"; then
      defect "artifact '$f' was written by an older format ($got1); this reader reads $magic — re-run the round, do not hand-migrate the record"
    fi
    defect "artifact '$f': line 1 is not '$magic'"
  fi
  # `wc -l` counts NEWLINES, so bytes appended after the last newline are
  # invisible to it: `printf 'junk' >> art` leaves the count unchanged. Require
  # the file to END with a newline, and compare the LAST line literally.
  [ "$(tail -c 1 "$f" | wc -l | tr -d ' ')" = "1" ] \
    || defect "artifact '$f': does not end with a newline (bytes appended after the seal)"
  lines="$(wc -l < "$f" | tr -d ' ')"
  [ "$lines" = "$((nbody + 1))" ] \
    || defect "artifact '$f': expected $((nbody + 1)) lines, measured $lines (truncated or extended)"
  [ "$(sed -n "${nbody}p" "$f")" = "END $magic" ] \
    || defect "artifact '$f': line $nbody is not 'END $magic'"
  seal="$(tail -n 1 "$f")"
  case "$seal" in
    'artifact-sha256: '*) : ;;
    *) defect "artifact '$f': last line is not the artifact-sha256 seal" ;;
  esac
  seal="${seal#artifact-sha256: }"
  printf '%s' "$seal" | LC_ALL=C grep -qE "$RE_SHA" \
    || defect "artifact '$f': seal value is not a sha256"
  body="$(mktemp)"
  head -n "$nbody" "$f" > "$body"
  want="$(sha_file "$body")"
  rm -f "$body"
  [ "$want" = "$seal" ] \
    || defect "artifact '$f': self-seal MISMATCH (body hashes $want, seal claims $seal)"
}

# parse_verdict_artifact <file> -> fills F[1..NFIELD]
parse_verdict_artifact() {
  local f="$1" i line k v
  read_artifact "$f" 'RVERDICT2' "$NBODY"
  F=('')
  i=1
  while [ "$i" -le "$NFIELD" ]; do
    line="$(sed -n "$((i + 1))p" "$f")"
    k="${KEY[$i]}"
    case "$line" in
      "$k: "*) : ;;
      *) defect "artifact '$f' line $((i + 1)): expected key '$k'" ;;
    esac
    v="${line#$k: }"
    printf '%s' "$v" | LC_ALL=C grep -qE "${RE[$i]}" \
      || defect "artifact '$f' field '$k': value '$v' fails its grammar"
    # RE_PATH permits `..` (it is a character-class grammar, and `.` is legal
    # in a path component), so every PATH-typed field gets the traversal
    # rejection the producer applies. Unreachable today — sidecar paths are
    # DERIVED from the artifact's own basename below — but an understated
    # grammar is how a later loosening becomes a hole.
    if [ "${RE[$i]}" = "$RE_PATH" ]; then
      case "$v" in *..*) defect "artifact '$f' field '$k': path contains '..': '$v'" ;; esac
    fi
    F[$i]="$v"
    i=$((i + 1))
  done
}
# Field index constants (KEY order above).
I_ARC=1; I_ROUND=2; I_REVIEWER=3; I_ROLE=4; I_TIER=5; I_RULE=6; I_BASIS=7
I_CMDSHA=8; I_SCOPEN=9; I_SCOPESHA=10; I_SCOPEMAN=11; I_PROMPTSHA=12
I_PROMPTPATH=13; I_LOGPATH=14; I_LOGSHA=15; I_LOGBYTES=16; I_LOGLINES=17
I_LOGNUL=18; I_DSRC=19; I_DPATH=20; I_DSHA=21; I_NONCE=22; I_RC=23
I_RCLINE=24; I_VERDICT=25; I_VLINE=26; I_GO=27; I_NOGO=28; I_VOIDR=29
I_TIMEOUT=30; I_HKILL=31; I_HSHA=32; I_START=33; I_END=34

# utc_epoch <YYYY-MM-DDTHH:MM:SSZ> — seconds since the Unix epoch, computed
# ARITHMETICALLY (Howard Hinnant's days_from_civil) rather than through
# `date`, whose parsing flags differ between BSD and GNU. Elapsed-time
# reasoning about a recorded round must not depend on which host reads it.
utc_epoch() {
  LC_ALL=C awk -v s="$1" 'BEGIN{
    y = substr(s,1,4)+0; mo = substr(s,6,2)+0; d = substr(s,9,2)+0;
    h = substr(s,12,2)+0; mi = substr(s,15,2)+0; se = substr(s,18,2)+0;
    yy = y - (mo <= 2 ? 1 : 0);
    era = int((yy >= 0 ? yy : yy - 399) / 400);
    yoe = yy - era * 400;
    doy = int((153 * (mo + (mo > 2 ? -3 : 9)) + 2) / 5) + d - 1;
    doe = yoe * 365 + int(yoe/4) - int(yoe/100) + doy;
    printf "%d", (era * 146097 + doe - 719468) * 86400 + h*3600 + mi*60 + se;
  }'
}

# ------------------------------------------------------- per-round checks
validate_round() {
  local f="$1" logp scopeman promptp decp nul bytes lines rcl eorl p
  local got_rc tv dv vr_rcl vr_tv vr_dv vr_want log_rc_line base_p
  local bn n8 fstamp fpre want_pre want_stamp elapsed

  [ "${F[$I_ARC]}" = "$ARC" ] \
    || defect "artifact '$f' declares arc-id '${F[$I_ARC]}', not '$ARC'"

  # role <-> reviewer <-> rule <-> basis coherence (PROCESS §3 tiers, §11 roles)
  case "${F[$I_ROLE]}" in
    primary)
      if reviewer_is_fallback "${F[$I_REVIEWER]}"; then
        defect "artifact '$f': reviewer '${F[$I_REVIEWER]}' cannot hold role primary"
      fi
      [ "${F[$I_RULE]}" = 'process3-tier-a-go' ] \
        || defect "artifact '$f': role primary must carry closure-rule process3-tier-a-go"
      [ "${F[$I_BASIS]}" = '-' ] \
        || defect "artifact '$f': only a fallback round may carry a fallback-basis" ;;
    fallback)
      reviewer_is_fallback "${F[$I_REVIEWER]}" \
        || defect "artifact '$f': role fallback is for the §11 pair, not '${F[$I_REVIEWER]}'"
      [ "${F[$I_RULE]}" = 'process11-fallback-dual-go' ] \
        || defect "artifact '$f': role fallback must carry closure-rule process11-fallback-dual-go"
      [ "${F[$I_BASIS]}" != '-' ] \
        || defect "artifact '$f': a §11 fallback round must name the failed codex artifact it rests on" ;;
    second-opinion)
      [ "${F[$I_RULE]}" = 'process3-tier-aplus-second' ] \
        || defect "artifact '$f': role second-opinion must carry closure-rule process3-tier-aplus-second"
      [ "${F[$I_BASIS]}" = '-' ] \
        || defect "artifact '$f': only a fallback round may carry a fallback-basis" ;;
  esac

  # the decision channel: codex rounds MUST be decided by --output-last-message,
  # a channel only the reviewer's final message reaches. A transcript-decided
  # codex round is exactly the shape a tool-emitted or quoted verdict exploits.
  if [ "${F[$I_REVIEWER]}" = 'codex' ]; then
    [ "${F[$I_DSRC]}" = 'last-message' ] \
      || defect "artifact '$f': a codex round must be decided by the last-message channel, not the transcript"
  fi

  # ---- the FILENAME is evidence, and is bound to the record.
  # The harness composes every round path as
  # `r<NNN>-<reviewer>-<role>-<stamp>-<nonce8>` from ONE clock reading, so
  # round, reviewer, role, the start time and the nonce are all re-derivable
  # from the name this reader ENUMERATES. That matters for ordering: the
  # closing round used to be `max(round)` over a caller-supplied label, with
  # the contradicting times sitting unread in the artifacts AND in the
  # filenames (H1, measured 2026-07-31 — a codex NO-GO recorded two seconds
  # AFTER a GO, labelled round 1 vs round 9, closed the arc). No rule judges
  # those times any more — the monotonicity rule that did read only
  # `started-utc` and was measured unsound, and was deleted (FORMAT.md §5.3);
  # the report PRINTS both ends of both rounds instead. This binding is what
  # stops them from being re-labelled in place, and it is what makes a
  # hand-placed bundle (rather than a harness-produced one) visible.
  # Ordered AFTER the coherence block on purpose: when a record's role and
  # reviewer are incoherent, the refusal should name the incoherence, not the
  # path that faithfully reflects it.
  bn="${f##*/}"; bn="${bn%.verdict}"
  n8="${bn##*-}"
  fpre="${bn%-*}"
  fstamp="${fpre##*-}"
  fpre="${fpre%-*}"
  want_pre="$(printf 'r%03d-%s-%s' "${F[$I_ROUND]}" "${F[$I_REVIEWER]}" "${F[$I_ROLE]}")"
  [ "$fpre" = "$want_pre" ] \
    || defect "artifact '$f': its path says '$fpre' but the record says round ${F[$I_ROUND]} / ${F[$I_REVIEWER]} / ${F[$I_ROLE]}"
  [ "$n8" = "$(printf '%s' "${F[$I_NONCE]}" | cut -c1-8)" ] \
    || defect "artifact '$f': its path's nonce stem '$n8' is not the first 8 hex of the recorded eor-nonce"
  want_stamp="$(printf '%s' "${F[$I_START]}" | LC_ALL=C tr -d ':-')"
  [ "$fstamp" = "$want_stamp" ] \
    || defect "artifact '$f': its path's UTC stamp '$fstamp' is not its recorded started-utc ${F[$I_START]}"

  # ---- the round's own clock must be coherent, and a claimed TIMEOUT must
  # have actually reached its deadline. `--timeout-sec` is a caller knob; it
  # is now RECORDED, so "this round timed out" is a checkable statement
  # instead of a trusted one (H3).
  [ "$(utc_epoch "${F[$I_END]}")" -ge "$(utc_epoch "${F[$I_START]}")" ] \
    || defect "artifact '$f': ended-utc ${F[$I_END]} precedes started-utc ${F[$I_START]}"
  elapsed="$(( $(utc_epoch "${F[$I_END]}") - $(utc_epoch "${F[$I_START]}") ))"
  if [ "${F[$I_VOIDR]}" = 'timeout' ]; then
    [ "${F[$I_HKILL]}" = 'harness-timeout' ] \
      || defect "artifact '$f': void-reason timeout but harness-kill is '${F[$I_HKILL]}' — only the harness's own deadline produces a timeout"
    [ "$elapsed" -ge "${F[$I_TIMEOUT]}" ] \
      || defect "artifact '$f': claims void-reason timeout after ${elapsed}s, but its recorded deadline is ${F[$I_TIMEOUT]}s — the round never reached it"
  fi
  if [ "${F[$I_VOIDR]}" != 'timeout' ] && [ "${F[$I_HKILL]}" != 'none' ]; then
    defect "artifact '$f': harness-kill is '${F[$I_HKILL]}' but void-reason is '${F[$I_VOIDR]}' — a harness-killed round is a timeout"
  fi

  # reviewer identity: the round must have been run by the built-in
  # invocation for its declared reviewer id, unless the caller explicitly
  # accepts synthetic reviewers (only the teeth do).
  if [ "$SYNTHETIC_OK" != "1" ]; then
    [ "${F[$I_CMDSHA]}" = "$(sha_str "$(reviewer_descriptor "${F[$I_REVIEWER]}")")" ] \
      || defect "artifact '$f': reviewer-cmd-sha256 is not the built-in ${F[$I_REVIEWER]} invocation (synthetic reviewer)"
  fi

  # evidence must live inside the arc directory (it has to outlive the
  # worktree, PROCESS §12.3(5), and must not point at unrelated files)
  # Sidecar paths are DERIVED from this artifact's own basename, not merely
  # checked for containment: otherwise two rounds' bundles could be cross-wired
  # (round 7's artifact pointing at round 2's log) and every hash would still
  # agree. There is exactly one legal path per sidecar.
  base_p="${f%.verdict}"
  [ "${F[$I_LOGPATH]}" = "$base_p.log" ] \
    || defect "artifact '$f': log-path is '${F[$I_LOGPATH]}', not its own '$base_p.log'"
  [ "${F[$I_SCOPEMAN]}" = "$base_p.scope" ] \
    || defect "artifact '$f': scope-manifest is '${F[$I_SCOPEMAN]}', not its own '$base_p.scope'"
  [ "${F[$I_PROMPTPATH]}" = "$base_p.prompt" ] \
    || defect "artifact '$f': prompt-path is '${F[$I_PROMPTPATH]}', not its own '$base_p.prompt'"
  if [ "${F[$I_DSRC]}" = 'last-message' ]; then
    [ "${F[$I_DPATH]}" = "$base_p.decision" ] \
      || defect "artifact '$f': decision-path is '${F[$I_DPATH]}', not its own '$base_p.decision'"
  else
    [ "${F[$I_DPATH]}" = "$base_p.log" ] \
      || defect "artifact '$f': a transcript decision-path must be its own log"
  fi

  logp="${F[$I_LOGPATH]}"; scopeman="${F[$I_SCOPEMAN]}"
  promptp="${F[$I_PROMPTPATH]}"; decp="${F[$I_DPATH]}"
  # NOTE (defence in depth, deliberately unreachable): the arc-directory
  # whitelist above already refuses any symlink in the directory, and the
  # containment check above requires every evidence path to be IN it — so
  # these two symlink guards cannot fire today. They carry their own distinct
  # wording so that, if containment is ever loosened, the tooth that fires
  # names the layer that caught it.
  for p in "$logp" "$scopeman" "$promptp"; do
    [ -f "$p" ] || defect "artifact '$f': evidence file '$p' is missing or not a regular file"
    if [ -L "$p" ]; then defect "artifact '$f': evidence-layer symlink guard: '$p'"; fi
    [ -s "$p" ] || defect "artifact '$f': evidence file '$p' is empty"
  done
  [ -f "$decp" ] || defect "artifact '$f': decision channel '$decp' is missing"
  if [ -L "$decp" ]; then defect "artifact '$f': evidence-layer symlink guard: '$decp'"; fi

  # the decision channel must be the transcript itself, or a separate file
  if [ "${F[$I_DSRC]}" = 'transcript' ] && [ "$decp" != "$logp" ]; then
    defect "artifact '$f': decision-source transcript must point at the log"
  fi
  if [ "${F[$I_DSRC]}" = 'last-message' ] && [ "$decp" = "$logp" ]; then
    defect "artifact '$f': decision-source last-message must point at its own file"
  fi

  [ "$(sha_file "$promptp")" = "${F[$I_PROMPTSHA]}" ] \
    || defect "artifact '$f': prompt '$promptp' does not match its recorded sha256"
  [ "$(sha_file "$scopeman")" = "${F[$I_SCOPESHA]}" ] \
    || defect "artifact '$f': scope manifest '$scopeman' does not match its recorded sha256"
  [ "$(wc -l < "$scopeman" | tr -d ' ')" = "${F[$I_SCOPEN]}" ] \
    || defect "artifact '$f': scope manifest line count != scope-count ${F[$I_SCOPEN]}"
  validate_manifest "$f" "$scopeman"
  [ "$(sha_file "$decp")" = "${F[$I_DSHA]}" ] \
    || defect "artifact '$f': decision channel '$decp' does not match its recorded sha256"

  # the prompt envelope the reviewer actually saw must carry THIS scope digest
  # AND this arc/round/tier — so those three are re-derived from the bytes the
  # reviewer read, not merely grammar-checked in the artifact
  # POSITIONAL and LITERAL, not an existential regex: the harness writes the
  # scope digest as the envelope's LAST line, so a prompt that merely QUOTES an
  # older scope block cannot satisfy this. `grep -F -x` also stops the arc-id's
  # legal `.` from acting as a regex wildcard.
  [ "$(tail -n 1 "$promptp")" = "scope-sha256: ${F[$I_SCOPESHA]}" ] \
    || defect "artifact '$f': the archived prompt's last line is not the recorded scope-sha256 — the reviewer was not told this scope"
  LC_ALL=C grep -qxF "arc-id: ${F[$I_ARC]} · round: ${F[$I_ROUND]} · tier: ${F[$I_TIER]}" "$promptp" \
    || defect "artifact '$f': the archived prompt does not carry this artifact's arc-id/round/tier"

  # ---- log integrity, re-measured
  [ "$(sha_file "$logp")" = "${F[$I_LOGSHA]}" ] \
    || defect "artifact '$f': log '$logp' does not match its recorded sha256 (mutated or appended to after the round closed)"
  bytes="$(wc -c < "$logp" | tr -d ' ')"
  [ "$bytes" = "${F[$I_LOGBYTES]}" ] || defect "artifact '$f': log byte count $bytes != recorded ${F[$I_LOGBYTES]}"
  lines="$(wc -l < "$logp" | tr -d ' ')"
  [ "$lines" = "${F[$I_LOGLINES]}" ] || defect "artifact '$f': log line count $lines != recorded ${F[$I_LOGLINES]}"
  nul="$(LC_ALL=C tr -dc '\000' < "$logp" | wc -c | tr -d ' ')"
  [ "$nul" = "${F[$I_LOGNUL]}" ] \
    || defect "artifact '$f': log NUL count $nul != recorded ${F[$I_LOGNUL]}"
  # NONCE-SCOPED counts (see review-harness.sh): a fixed token is collidable
  # by any reviewer that quotes this project's documentation.
  rcl="$(grepc "^REVIEWER_RC ${F[$I_NONCE]} " "$logp")"
  eorl="$(grepc "^HARNESS-EOR ${F[$I_NONCE]}\$" "$logp")"
  [ "$(tail -c 1 "$logp" | wc -l | tr -d ' ')" = "1" ] \
    || defect "artifact '$f': log '$logp' does not end with a newline (bytes appended after the terminator)"

  # The terminator pair is verified for EVERY artifact, VOID included, and the
  # rc is taken from the LOG MARKER rather than from the artifact's assertion:
  # a forged `reviewer-failed` VOID would otherwise be accepted and could then
  # license a §11 dual-reviewer fallback.
  #
  # The harness appends its pair as the log's LAST TWO LINES in every case, so
  # that requirement is UNCONDITIONAL — including for a `duplicate-terminator`
  # VOID. What that reason relaxes is ONLY the count-equals-one rule, because a
  # duplicated marker is precisely what it records; a log with ZERO
  # nonce-matching markers is not a duplicate, it is a fabrication.
  [ "$(tail -n 1 "$logp")" = "HARNESS-EOR ${F[$I_NONCE]}" ] \
    || defect "artifact '$f': log's last line is not this round's HARNESS-EOR nonce (content follows the terminator, or the nonce does not match)"
  log_rc_line="$((lines - 1))"
  [ "$(sed -n "${log_rc_line}p" "$logp")" = "REVIEWER_RC ${F[$I_NONCE]} ${F[$I_RC]}" ] \
    || defect "artifact '$f': the log's rc marker does not carry the recorded reviewer-rc ${F[$I_RC]}"
  if [ "${F[$I_VOIDR]}" != 'duplicate-terminator' ]; then
    [ "$rcl" = "1" ] || defect "artifact '$f': log has $rcl nonce-matching REVIEWER_RC lines, expected exactly 1"
    [ "$eorl" = "1" ] || defect "artifact '$f': log has $eorl nonce-matching HARNESS-EOR lines, expected exactly 1"
  else
    { [ "$rcl" -gt 1 ] || [ "$eorl" -gt 1 ]; } \
      || defect "artifact '$f': void-reason duplicate-terminator but the log has no duplicated nonce-matching terminator"
  fi

  # A VOID round is not a round (PROCESS §3): it must SAY so, its derived
  # fields must all be zeroed, and its stated reason must match what the
  # evidence shows. It is then excluded from closure — but it still has to be
  # coherent, so a VOID can never be quietly re-read as evidence.
  if [ "${F[$I_VERDICT]}" = 'VOID' ]; then
    [ "${F[$I_VOIDR]}" != '-' ] || defect "artifact '$f': VOID with no void-reason"
    { [ "${F[$I_RCLINE]}" = "0" ] && [ "${F[$I_VLINE]}" = "0" ] \
      && [ "${F[$I_GO]}" = "0" ] && [ "${F[$I_NOGO]}" = "0" ]; } \
      || defect "artifact '$f': a VOID round must carry zeroed rc-line/verdict-line/anchored counts"
    # The reason is re-derived by REPLAYING THE PRODUCER'S PRECEDENCE CHAIN in
    # the same order the harness applies it, then compared for equality. The
    # earlier per-reason spot checks accepted overlapping witnesses and so
    # could confirm a MISLABELLED reason (transcript GO + junk decision reads
    # as both `decision-mismatch` and `decision-malformed`); the machine-
    # readable reason has to be right, not merely plausible.
    #
    # `timeout` is the one reason with no independent witness (a killed
    # reviewer also exits nonzero, which is indistinguishable from
    # `reviewer-failed`), so it is accepted as TRUSTED and disclosed as such
    # in FORMAT.md §7.
    if [ "${F[$I_VOIDR]}" != 'timeout' ]; then
      vr_want=''
      if [ "${F[$I_RC]}" != "0" ]; then
        vr_want='reviewer-failed'
      elif [ "$nul" != "0" ]; then
        vr_want='nul-bytes'
      elif [ "$rcl" != "1" ] || [ "$eorl" != "1" ]; then
        vr_want='duplicate-terminator'
      else
        vr_rcl="$log_rc_line"
        vr_tv="$(terminal_verdict "$logp" "$((vr_rcl - 1))")"
        if [ -z "$vr_tv" ]; then
          vr_want='no-verdict'
        elif [ "${F[$I_DSRC]}" = 'last-message' ]; then
          if [ ! -s "$decp" ]; then
            vr_want='decision-empty'
          elif [ "$(LC_ALL=C tr -dc '\000' < "$decp" | wc -c | tr -d ' ')" != "0" ]; then
            vr_want='nul-bytes'
          else
            vr_dv="$(tail -1 "$decp")"
            if [ "$vr_dv" != 'VERDICT: GO' ] && [ "$vr_dv" != 'VERDICT: NO-GO' ]; then
              vr_want='decision-malformed'
            elif [ "${vr_tv#*:}" != "${vr_dv#VERDICT: }" ]; then
              vr_want='decision-mismatch'
            fi
          fi
        else
          # TRANSCRIPT-DECIDED: a region carrying BOTH anchored verdicts has
          # two candidates and a positional tie-break, not a decision. Same
          # derivation the producer applies (review-harness.sh), replayed at
          # the same point in the chain so a MISLABELLED reason cannot pass.
          local vr_reg vr_go vr_nogo
          vr_reg="$(mktemp)"
          head -n "$((vr_rcl - 1))" "$logp" > "$vr_reg"
          vr_go="$(grepc '^VERDICT: GO$' "$vr_reg")"
          vr_nogo="$(grepc '^VERDICT: NO-GO$' "$vr_reg")"
          rm -f "$vr_reg"
          if [ "$vr_go" != "0" ] && [ "$vr_nogo" != "0" ]; then
            vr_want='ambiguous-verdict'
          fi
        fi
      fi
      [ -n "$vr_want" ] \
        || defect "artifact '$f': recorded VOID (${F[$I_VOIDR]}) but the evidence shows a well-formed round"
      [ "$vr_want" = "${F[$I_VOIDR]}" ] \
        || defect "artifact '$f': void-reason is '${F[$I_VOIDR]}' but the evidence derives '$vr_want'"
    fi
    return 0
  fi
  [ "${F[$I_VOIDR]}" = '-' ] \
    || defect "artifact '$f': verdict ${F[$I_VERDICT]} carries void-reason ${F[$I_VOIDR]}"

  # A FAILED reviewer process is not a completed review, whatever it printed.
  # (The live specimen of this mode reads DEVICE TARGET CONFORMS and exited 1.)
  # A NON-VOID round with a nonzero rc is therefore refused outright — so it
  # can neither close an arc nor pad a cap's round count. A VOID round may
  # carry a nonzero rc: that IS the recorded shape of a wedged reviewer, and
  # §11's fallback rests on exactly such a record.
  [ "${F[$I_RC]}" = "0" ] \
    || defect "artifact '$f': the reviewer exited rc=${F[$I_RC]} — a failed reviewer process is not a completed review"

  [ "$nul" = "0" ] \
    || defect "artifact '$f': log '$logp' carries $nul NUL byte(s) — a corrupt log is not a verdict"

  # ---- the harness terminator's POSITION (its presence, nonce and rc were
  # already verified above, for VOID rounds too). The verdict is read only
  # from the region ahead of it: a transcript pasted after the run cannot be
  # in that region, and cannot reproduce the nonce.
  [ "${F[$I_RCLINE]}" = "$((lines - 1))" ] \
    || defect "artifact '$f': rc-line ${F[$I_RCLINE]} is not the line before HARNESS-EOR ($((lines - 1)))"
  got_rc="$(sed -n "${F[$I_RCLINE]}p" "$logp")"
  [ "$got_rc" = "REVIEWER_RC ${F[$I_NONCE]} ${F[$I_RC]}" ] \
    || defect "artifact '$f': log line ${F[$I_RCLINE]} is not 'REVIEWER_RC <nonce> ${F[$I_RC]}'"

  # ---- verdict, RE-EXTRACTED from the transcript region AND from the
  # decision channel. Both must exist and agree with the artifact and with
  # each other.
  tv="$(terminal_verdict "$logp" "$((${F[$I_RCLINE]} - 1))")"
  [ -n "$tv" ] || defect "artifact '$f': no anchored verdict in the harness region, yet the artifact claims ${F[$I_VERDICT]}"
  [ "${tv#*:}" = "${F[$I_VERDICT]}" ] \
    || defect "artifact '$f': terminal in-region verdict is '${tv#*:}', artifact claims '${F[$I_VERDICT]}'"
  [ "${tv%%:*}" = "${F[$I_VLINE]}" ] \
    || defect "artifact '$f': terminal verdict is at log line ${tv%%:*}, artifact claims ${F[$I_VLINE]}"
  if [ "${F[$I_DSRC]}" = 'last-message' ]; then
    [ "$(LC_ALL=C tr -dc '\000' < "$decp" | wc -c | tr -d ' ')" = "0" ] \
      || defect "artifact '$f': the decision channel carries NUL bytes"
    # the verdict must be the channel's LAST line — nothing may follow it, and
    # a truncated final write ('VERDICT: G') is not a verdict
    dv="$(tail -1 "$decp")"
    { [ "$dv" = 'VERDICT: GO' ] || [ "$dv" = 'VERDICT: NO-GO' ]; } \
      || defect "artifact '$f': the decision channel's last line is not an anchored verdict (truncated write, or content after the verdict)"
    [ "${dv#VERDICT: }" = "${F[$I_VERDICT]}" ] \
      || defect "artifact '$f': the decision channel says '${dv#VERDICT: }', the artifact claims '${F[$I_VERDICT]}'"
  fi

  # in-region counts, re-derived
  local region got_go got_nogo
  region="$(mktemp)"
  head -n "$((${F[$I_RCLINE]} - 1))" "$logp" > "$region"
  got_go="$(grepc '^VERDICT: GO$' "$region")"
  got_nogo="$(grepc '^VERDICT: NO-GO$' "$region")"
  rm -f "$region"
  { [ "$got_go" = "${F[$I_GO]}" ] && [ "$got_nogo" = "${F[$I_NOGO]}" ]; } \
    || defect "artifact '$f': in-region verdict counts ($got_go GO / $got_nogo NO-GO) disagree with the artifact (${F[$I_GO]}/${F[$I_NOGO]})"

  # ---- AMBIGUOUS TRANSCRIPT: the reader's own layer for failure mode #1 on
  # the paths that have no decision channel (§11's fallback pair, the Tier A+
  # second opinion — both never codex). The producer VOIDs this shape; this
  # re-derives it from the log so a producer that did not is refused rather
  # than believed. Both counts were just re-measured above, so the decision is
  # made on measured bytes, not on the artifact's assertion.
  #
  # codex rounds are deliberately exempt: they are decided by a channel no
  # quoted text can reach, and real codex reviews DO quote opposing verdicts
  # (2 of the 7 rounds of this item's own arc).
  if [ "${F[$I_DSRC]}" = 'transcript' ]; then
    { [ "$got_go" = "0" ] || [ "$got_nogo" = "0" ]; } \
      || defect "artifact '$f': a transcript-decided round whose region carries BOTH an anchored GO ($got_go) and an anchored NO-GO ($got_nogo) has no terminal verdict — it must be VOID (ambiguous-verdict), not read positionally"
  fi
  return 0
}

# ==================================================== sweep the artifacts
ART_LIST="$(ls -1 "$DIR" 2>/dev/null | LC_ALL=C grep -E '\.verdict$' || true)"
[ -n "$ART_LIST" ] || defect "no RVERDICT2 artifacts in '$DIR'"

# V_* = EVERY artifact (VOID included); R_* = the non-VOID subset, which is
# what "a round happened" observations are computed over. There is
# deliberately no `maxround` and no "closing round": designating one was the
# label-trusting step that H1 exploited, and nothing here needs it.
R_ROUND=(); R_REVIEWER=(); R_ROLE=(); R_FILE=()
V_ROUND=(); V_REVIEWER=(); V_NAME=(); V_VERDICT=(); V_ROLE=(); V_PROMPTSHA=()
V_BASIS=()
V_SCOPESHA=()
V_START=(); V_VOIDR=(); V_TIMEOUT=(); V_HKILL=(); V_RC=()
V_END=(); V_SCOPEMAN=(); V_GO=(); V_NOGO=(); V_DSRC=(); V_PATHSET=()
TIER=''
n=0
nv=0
while IFS= read -r name; do
  [ -n "$name" ] || continue
  f="$DIR/$name"
  parse_verdict_artifact "$f"
  validate_round "$f"
  if [ -z "$TIER" ]; then TIER="${F[$I_TIER]}"; fi
  [ "$TIER" = "${F[$I_TIER]}" ] \
    || defect "artifact '$f' declares tier ${F[$I_TIER]} but the arc's other artifacts declare $TIER"
  V_ROUND[$nv]="${F[$I_ROUND]}"; V_REVIEWER[$nv]="${F[$I_REVIEWER]}"
  V_NAME[$nv]="$name"; V_VERDICT[$nv]="${F[$I_VERDICT]}"
  V_ROLE[$nv]="${F[$I_ROLE]}"; V_PROMPTSHA[$nv]="${F[$I_PROMPTSHA]}"
  V_BASIS[$nv]="${F[$I_BASIS]}"
  V_SCOPESHA[$nv]="${F[$I_SCOPESHA]}"
  V_START[$nv]="${F[$I_START]}"
  V_VOIDR[$nv]="${F[$I_VOIDR]}"
  V_TIMEOUT[$nv]="${F[$I_TIMEOUT]}"
  V_HKILL[$nv]="${F[$I_HKILL]}"
  V_RC[$nv]="${F[$I_RC]}"
  V_END[$nv]="${F[$I_END]}"
  V_SCOPEMAN[$nv]="${F[$I_SCOPEMAN]}"
  V_GO[$nv]="${F[$I_GO]}"
  V_NOGO[$nv]="${F[$I_NOGO]}"
  V_DSRC[$nv]="${F[$I_DSRC]}"
  V_PATHSET[$nv]="$(LC_ALL=C awk '{print $2}' "${F[$I_SCOPEMAN]}" | LC_ALL=C sort | shasum -a 256 | cut -d' ' -f1)"
  nv=$((nv + 1))
  if [ "${F[$I_VERDICT]}" = 'VOID' ]; then continue; fi
  R_ROUND[$n]="${F[$I_ROUND]}"
  R_REVIEWER[$n]="${F[$I_REVIEWER]}"
  R_ROLE[$n]="${F[$I_ROLE]}"
  R_FILE[$n]="$f"
  n=$((n + 1))
done <<EOF
$ART_LIST
EOF
# ---- ROUND ORDER IS NOT ESTABLISHED, AND THIS TOOL NO LONGER CLAIMS IT IS.
#
# `round` is a caller LABEL (`--round N`), stamped by the harness into the
# envelope and then "re-derived" by grepping that same envelope — circular.
# A rule requiring the label order to agree with `started-utc` shipped here
# on 2026-07-31 and was MEASURED UNSOUND the same day: it never read
# `ended-utc`, so a GO that STARTED one second after an adverse round and
# FINISHED six seconds before it satisfied it, as did two rounds launched in
# the same second. The rule is DELETED rather than reported, because an
# observation derived from an unsound rule is still an unsound claim.
#
# What is printed instead: `started-utc` AND `ended-utc` for every artifact,
# plus an explicit statement that round order is not established. Ordering is
# a driver/human read of those times (and of the file dates, which a human
# reading the arc directory also sees).

# ============================================ per-artifact basis coherence
# EVERY fallback artifact's own basis is RESOLVED, VOID ones included. This
# is a referential-integrity check on the recorded bytes: an artifact that
# names a basis which is absent, is not codex, is not `primary`, belongs to
# another round, reviewed other bytes or another prompt, or is not VOID, is
# an INCOHERENT RECORD — it cannot be read as evidence of anything.
#
# It is NOT, and no longer pretends to be, a ruling on PROCESS §11's "a codex
# round PROVEN failed" precondition. That eligibility rule (a two-item
# blacklist over eight VOID reasons) was measured bypassable in zero seconds
# from the sanctioned CLI with no repo write, twice, and is DELETED. The
# facts it rested on — void-reason, rc, deadline, harness-kill, elapsed —
# are printed in the observations below for a human to weigh.
i=0
while [ "$i" -lt "$nv" ]; do
  if [ "${V_ROLE[$i]}" = 'fallback' ]; then
    basis_ok=0
    j=0
    while [ "$j" -lt "$nv" ]; do
      if [ "${V_NAME[$j]}" = "${V_BASIS[$i]}" ]; then
        [ "${V_REVIEWER[$j]}" = 'codex' ] \
          || defect "artifact '${V_NAME[$i]}': its fallback-basis '${V_BASIS[$i]}' is a ${V_REVIEWER[$j]} artifact, not codex"
        [ "${V_ROLE[$j]}" = 'primary' ] \
          || defect "artifact '${V_NAME[$i]}': its fallback-basis '${V_BASIS[$i]}' has role ${V_ROLE[$j]}, not primary"
        [ "${V_ROUND[$j]}" = "${V_ROUND[$i]}" ] \
          || defect "artifact '${V_NAME[$i]}': its fallback-basis '${V_BASIS[$i]}' is round ${V_ROUND[$j]}, not round ${V_ROUND[$i]}"
        [ "${V_VERDICT[$j]}" = 'VOID' ] \
          || defect "artifact '${V_NAME[$i]}': its fallback-basis '${V_BASIS[$i]}' is a codex ${V_VERDICT[$j]}, not a VOID (proven-failed) round"
        [ "${V_SCOPESHA[$j]}" = "${V_SCOPESHA[$i]}" ] \
          || defect "artifact '${V_NAME[$i]}': its fallback-basis '${V_BASIS[$i]}' reviewed different scope BYTES"
        [ "${V_PROMPTSHA[$j]}" = "${V_PROMPTSHA[$i]}" ] \
          || defect "artifact '${V_NAME[$i]}': its fallback-basis '${V_BASIS[$i]}' used a different prompt"
        basis_ok=1
      fi
      j=$((j + 1))
    done
    [ "$basis_ok" = "1" ] \
      || defect "artifact '${V_NAME[$i]}': its fallback-basis '${V_BASIS[$i]}' names no artifact in this arc"
  fi
  i=$((i + 1))
done

# ---- one non-VOID artifact per (round, reviewer): two disagreeing records
# for one round is exactly the two-writer class, and must not be arbitrated
# by whichever the reader happened to open.
i=0
while [ "$i" -lt "$n" ]; do
  j=$((i + 1))
  while [ "$j" -lt "$n" ]; do
    if [ "${R_ROUND[$i]}" = "${R_ROUND[$j]}" ] && [ "${R_REVIEWER[$i]}" = "${R_REVIEWER[$j]}" ]; then
      defect "two non-VOID artifacts for round ${R_ROUND[$i]} reviewer ${R_REVIEWER[$i]} (${R_FILE[$i]}, ${R_FILE[$j]})"
    fi
    j=$((j + 1))
  done
  i=$((i + 1))
done

# ---- machine-readable arc identity: every round of ONE arc reviews ONE
# scope path set, and every artifact of ONE round reviews ONE set of BYTES.
# This is what the 19 `x-*` cross-artifact rows lacked. Applied to ALL
# rounds, VOID artifacts included — the old form checked only the "closing"
# round, and there is no closing round here to privilege.
ARC_PATHSET="${V_PATHSET[0]}"
i=0
while [ "$i" -lt "$nv" ]; do
  [ "${V_PATHSET[$i]}" = "$ARC_PATHSET" ] \
    || defect "round ${V_ROUND[$i]} ($DIR/${V_NAME[$i]}) reviewed a different scope path set than this arc's other rounds — these are not rounds of one arc"
  j=$((i + 1))
  while [ "$j" -lt "$nv" ]; do
    if [ "${V_ROUND[$i]}" = "${V_ROUND[$j]}" ] \
       && [ "${V_SCOPESHA[$i]}" != "${V_SCOPESHA[$j]}" ]; then
      defect "round ${V_ROUND[$i]}: ${V_NAME[$i]} reviewed different scope BYTES than its co-reviewer ${V_NAME[$j]}"
    fi
    j=$((j + 1))
  done
  i=$((i + 1))
done

# ---- cap record: structural validation only. A cap is a singleton, its
# grammar is pinned, and it must name the arc and path set it caps. What it
# CLAIMS (`rounds-completed`, `authorized-by`, `recurring-class`) is reported,
# never adjudicated.
CAP_LIST="$(ls -1 "$DIR" 2>/dev/null | LC_ALL=C grep -E '\.cap$' || true)"
CAPCOUNT="$(printf '%s' "$CAP_LIST" | LC_ALL=C grep -c . | tr -d ' ' || true)"
[ "$CAPCOUNT" -le 1 ] || defect "$CAPCOUNT cap artifacts in '$DIR' — a cap is a singleton"

CAP_ROUNDS=''
CAP_CLASS=''
CAP_BY=''
CAP_SCOPESHA=''
if [ "$CAPCOUNT" = "1" ]; then
  capf="$DIR/$CAP_LIST"
  read_artifact "$capf" 'RVCAP1' 13
  CAPKEY=( '' arc-id tier closure-rule rounds-completed authorized-by \
           recurring-class scope-count scope-sha256 scope-manifest \
           harness-sha256 written-utc )
  CAPRE=( '' "$RE_ARCID" '^(A|A\+|B)$' '^process3-capped$' '^[1-9][0-9]{0,2}$' \
          '^[a-z0-9][a-z0-9.-]{2,63}$' '^[ -~]+$' "$RE_UINT" "$RE_SHA" \
          "$RE_PATH" "$RE_SHA" "$RE_UTC" )
  CF=('')
  i=1
  while [ "$i" -le 11 ]; do
    line="$(sed -n "$((i + 1))p" "$capf")"
    k="${CAPKEY[$i]}"
    case "$line" in "$k: "*) : ;; *) defect "cap '$capf' line $((i + 1)): expected key '$k'" ;; esac
    v="${line#$k: }"
    printf '%s' "$v" | LC_ALL=C grep -qE "${CAPRE[$i]}" \
      || defect "cap '$capf' field '$k': value '$v' fails its grammar"
    CF[$i]="$v"
    i=$((i + 1))
  done
  # length bound as a shell test (BSD grep rejects {m,n} with n > 255)
  { [ "${#CF[6]}" -ge 24 ] && [ "${#CF[6]}" -le 400 ]; } \
    || defect "cap '$capf': recurring-class must be 24..400 chars (measured ${#CF[6]}) — PROCESS §3 requires a cap to NAME its class"
  [ "${CF[1]}" = "$ARC" ] || defect "cap '$capf' declares arc-id '${CF[1]}', not '$ARC'"
  [ "${CF[2]}" = "$TIER" ] || defect "cap '$capf' declares tier ${CF[2]}, rounds declare $TIER"
  case "${CF[9]}" in
    *..*) defect "cap '$capf': scope manifest path contains '..'" ;;
  esac
  [ "${CF[9]}" = "${capf%.cap}.scope" ] \
    || defect "cap '$capf': scope-manifest is not its own '${capf%.cap}.scope'"
  [ -f "${CF[9]}" ] || defect "cap '$capf': scope manifest '${CF[9]}' missing"
  if [ -L "${CF[9]}" ]; then defect "cap '$capf': scope manifest is a symlink"; fi
  [ "$(sha_file "${CF[9]}")" = "${CF[8]}" ] || defect "cap '$capf': scope manifest does not match its recorded sha256"
  [ "$(wc -l < "${CF[9]}" | tr -d ' ')" = "${CF[7]}" ] \
    || defect "cap '$capf': scope manifest line count != scope-count ${CF[7]}"
  validate_manifest "$capf" "${CF[9]}"
  [ "$(LC_ALL=C awk '{print $2}' "${CF[9]}" | LC_ALL=C sort | shasum -a 256 | cut -d' ' -f1)" = "$ARC_PATHSET" ] \
    || defect "cap '$capf' caps a different scope path set than the arc's rounds"
  CAP_ROUNDS="${CF[4]}"
  CAP_CLASS="${CF[6]}"
  CAP_BY="${CF[5]}"
  CAP_SCOPESHA="${CF[8]}"
fi

# ---- regression records (RVREG1): structural validation of every record
# present. Whether the arc OWES one, and whether its rc discharges anything,
# is reported below and decided by nobody here.
REG_NAMES="$(ls -1 "$DIR" 2>/dev/null | LC_ALL=C grep -E '\.reg$' || true)"
REGCOUNT="$(printf '%s' "$REG_NAMES" | LC_ALL=C grep -c . | tr -d ' ' || true)"
REG_OBS=''
validate_regression() {
  local regf="$1" regbase i line k v p
  read_artifact "$regf" 'RVREG1' 18
  REGKEY=( '' arc-id tier scope-count scope-sha256 scope-manifest cmd-sha256 \
           cmd-path log-path log-sha256 log-bytes log-lines log-nul-count \
           eor-nonce rc harness-sha256 run-utc )
  REGRE=( '' "$RE_ARCID" '^(A|A\+|B)$' "$RE_UINT" "$RE_SHA" "$RE_PATH" "$RE_SHA" \
          "$RE_PATH" "$RE_PATH" "$RE_SHA" "$RE_UINT" "$RE_UINT" "$RE_UINT" \
          '^[0-9a-f]{32}$' '^[0-9]{1,3}$' "$RE_SHA" "$RE_UTC" )
  GF=('')
  i=1
  while [ "$i" -le 16 ]; do
    line="$(sed -n "$((i + 1))p" "$regf")"
    k="${REGKEY[$i]}"
    case "$line" in "$k: "*) : ;; *) defect "regression '$regf' line $((i + 1)): expected key '$k'" ;; esac
    v="${line#$k: }"
    printf '%s' "$v" | LC_ALL=C grep -qE "${REGRE[$i]}" \
      || defect "regression '$regf' field '$k': value '$v' fails its grammar"
    GF[$i]="$v"
    i=$((i + 1))
  done
  [ "${GF[1]}" = "$ARC" ] || defect "regression '$regf' declares arc-id '${GF[1]}', not '$ARC'"
  [ "${GF[2]}" = "$TIER" ] || defect "regression '$regf' declares tier ${GF[2]}, the arc declares $TIER"
  # sidecars DERIVED from this record's own basename (see validate_round)
  regbase="${regf%.reg}"
  [ "${GF[5]}" = "$regbase.scope" ] \
    || defect "regression '$regf': scope-manifest is not its own '$regbase.scope'"
  [ "${GF[7]}" = "$regbase.cmd" ] \
    || defect "regression '$regf': cmd-path is not its own '$regbase.cmd'"
  [ "${GF[8]}" = "$regbase.log" ] \
    || defect "regression '$regf': log-path is not its own '$regbase.log'"
  for p in "${GF[5]}" "${GF[7]}" "${GF[8]}"; do
    [ -f "$p" ] || defect "regression '$regf': evidence file '$p' is missing"
    if [ -L "$p" ]; then defect "regression '$regf': evidence file '$p' is a symlink"; fi
  done
  [ "$(sha_file "${GF[5]}")" = "${GF[4]}" ] \
    || defect "regression '$regf': scope manifest does not match its recorded sha256"
  [ "$(wc -l < "${GF[5]}" | tr -d ' ')" = "${GF[3]}" ] \
    || defect "regression '$regf': scope manifest line count != scope-count ${GF[3]}"
  validate_manifest "$regf" "${GF[5]}"
  # the ARCHIVED command bytes: `cmd-sha256` is only evidence if the thing it
  # names is still openable and still hashes the same
  [ "$(sha_file "${GF[7]}")" = "${GF[6]}" ] \
    || defect "regression '$regf': the archived command does not match its recorded cmd-sha256"
  [ -s "${GF[7]}" ] \
    || defect "regression '$regf': the archived command is empty"
  [ "$(sha_file "${GF[8]}")" = "${GF[9]}" ] \
    || defect "regression '$regf': log does not match its recorded sha256"
  [ "$(wc -c < "${GF[8]}" | tr -d ' ')" = "${GF[10]}" ] \
    || defect "regression '$regf': log byte count disagrees with the record"
  [ "$(wc -l < "${GF[8]}" | tr -d ' ')" = "${GF[11]}" ] \
    || defect "regression '$regf': log line count disagrees with the record"
  [ "$(LC_ALL=C tr -dc '\000' < "${GF[8]}" | wc -c | tr -d ' ')" = "${GF[12]}" ] \
    || defect "regression '$regf': log NUL count disagrees with the record"
  [ "${GF[12]}" = "0" ] \
    || defect "regression '$regf': its log carries NUL bytes — a corrupt run is not evidence"
  # the SAME three rules a round log gets: trailing newline (so bytes after the
  # final one cannot hide from `wc -l`), `tail -n 1` rather than an index, and
  # NONCE-SCOPED marker counting (a regression command that prints the
  # documented marker examples must not be rejected — round 5's defect).
  [ "$(tail -c 1 "${GF[8]}" | wc -l | tr -d ' ')" = "1" ] \
    || defect "regression '$regf': its log does not end with a newline (bytes appended after the terminator)"
  { [ "$(grepc "^REVIEWER_RC ${GF[13]} " "${GF[8]}")" = "1" ] \
    && [ "$(grepc "^HARNESS-EOR ${GF[13]}\$" "${GF[8]}")" = "1" ]; } \
    || defect "regression '$regf': its log has no single nonce-matching harness terminator pair"
  [ "$(tail -n 1 "${GF[8]}")" = "HARNESS-EOR ${GF[13]}" ] \
    || defect "regression '$regf': its log's last line is not this run's HARNESS-EOR nonce"
  [ "$(sed -n "$((${GF[11]} - 1))p" "${GF[8]}")" = "REVIEWER_RC ${GF[13]} ${GF[14]}" ] \
    || defect "regression '$regf': its rc marker does not match the recorded rc"
  REG_RC="${GF[14]}"
  REG_SCOPESHA="${GF[4]}"
}
REG_RC=''
REG_SCOPESHA=''
G_REGNAME=(); G_REGRC=(); G_REGSCOPE=()
nreg=0
while IFS= read -r rn; do
  [ -n "$rn" ] || continue
  validate_regression "$DIR/$rn"
  G_REGNAME[$nreg]="$rn"; G_REGRC[$nreg]="$REG_RC"; G_REGSCOPE[$nreg]="$REG_SCOPESHA"
  nreg=$((nreg + 1))
  REG_OBS="$REG_OBS$rn rc=$REG_RC scope=$(printf '%s' "$REG_SCOPESHA" | cut -c1-12) "
done <<EOF
$REG_NAMES
EOF

# ======================================================== OBSERVATIONS ONLY
# Nothing below this line refuses anything. Everything below is a FACT read
# out of the artifacts, or an explicitly named OPEN QUESTION.

if [ -n "$WANT_TIER" ] && [ "$WANT_TIER" != "$TIER" ]; then
  obs "caller passed --tier $WANT_TIER; the artifacts declare tier $TIER (--tier is an argument, not a derivation — FORMAT.md §7)"
fi

# --- scope currency, per DISTINCT reviewed byte-set. A drifted scope means
# the artifacts describe bytes that are no longer on disk. Reported, because
# whether that matters is a judgement about the change, not about the bytes.
SEEN_SCOPES=''
i=0
while [ "$i" -lt "$nv" ]; do
  s="${V_SCOPESHA[$i]}"
  case " $SEEN_SCOPES " in
    *" $s "*) i=$((i + 1)); continue ;;
  esac
  SEEN_SCOPES="$SEEN_SCOPES $s"
  man="${V_SCOPEMAN[$i]}"
  # name EVERY round that reviewed these bytes, so a one-line currency
  # statement is not silently attributed to the first round that happens to
  # carry the digest
  sr=''
  j=0
  while [ "$j" -lt "$nv" ]; do
    if [ "${V_SCOPESHA[$j]}" = "$s" ]; then sr="$sr ${V_ROUND[$j]}"; fi
    j=$((j + 1))
  done
  sr="$(printf '%s' "$sr" | tr ' ' '\n' | LC_ALL=C sort -n -u | LC_ALL=C grep -v '^$' | tr '\n' ',' | sed 's/,$//')"
  lbl="scope $(printf '%s' "$s" | cut -c1-12) (reviewed by round(s) $sr)"
  missing=''
  now="$(mktemp)"
  while IFS= read -r mline; do
    [ -n "$mline" ] || continue
    p="${mline#*  }"
    if [ ! -f "$p" ] || [ -L "$p" ]; then
      missing="$p"
      break
    fi
    printf '%s  %s\n' "$(sha_file "$p")" "$p" >> "$now"
  done < "$man"
  if [ -n "$missing" ]; then
    obs "$lbl: path '$missing' is missing (or a symlink) in the current tree"
  elif ! cmp -s "$now" "$man"; then
    drift="$( (diff "$man" "$now" || true) | LC_ALL=C grep -c '^>' | tr -d ' ' || true)"
    obs "$lbl: the reviewed scope has DRIFTED since round ${V_ROUND[$i]} ($drift file(s) differ) — those artifacts cover bytes that are no longer on disk"
  else
    obs "$lbl: current (the reviewed bytes are still on disk)"
  fi
  rm -f "$now"
  i=$((i + 1))
done

# --- per-round composition: what is present, never what it licenses.
ROUNDS_ALL="$(
  i=0
  while [ "$i" -lt "$nv" ]; do printf '%s\n' "${V_ROUND[$i]}"; i=$((i + 1)); done \
    | LC_ALL=C sort -n -u
)"
COUNTABLE=0
NONVOID_TOTAL="$n"
while IFS= read -r rnd; do
  [ -n "$rnd" ] || continue
  has_codex_primary=0
  fb_live=''
  has_primary_any=0
  i=0
  while [ "$i" -lt "$n" ]; do
    if [ "${R_ROUND[$i]}" = "$rnd" ]; then
      case "${R_ROLE[$i]}" in
        primary)
          has_primary_any=1
          [ "${R_REVIEWER[$i]}" != 'codex' ] || has_codex_primary=1 ;;
        fallback) fb_live="$fb_live ${R_REVIEWER[$i]}" ;;
      esac
    fi
    i=$((i + 1))
  done
  pair_complete=1
  for r in $REVIEW_FALLBACK_PAIR; do
    case " $fb_live " in *" $r "*) : ;; *) pair_complete=0 ;; esac
  done
  if [ -n "$fb_live" ]; then
    if [ "$pair_complete" = "1" ]; then
      obs "round $rnd: §11 fallback artifacts present ($(printf '%s' "$fb_live" | sed 's/^ //')) — pair COMPLETE"
    else
      obs "round $rnd: §11 fallback artifacts present ($(printf '%s' "$fb_live" | sed 's/^ //')) — pair INCOMPLETE (PROCESS §11 needs non-VOID fallback artifacts from BOTH $REVIEW_FALLBACK_PAIR); no complete §11 fallback pair"
    fi
    if [ "$has_primary_any" = "1" ]; then
      obs "round $rnd: carries BOTH a non-VOID primary artifact and §11 fallback artifacts — PROCESS §11's fallback replaces a failed primary round, so this round mixes a primary GO with a §11 fallback and no single rule describes it"
    fi
  fi
  if [ "$has_codex_primary" = "1" ] || { [ -n "$fb_live" ] && [ "$pair_complete" = "1" ]; }; then
    COUNTABLE=$((COUNTABLE + 1))
  else
    obs "round $rnd: NOT countable as a review round (no non-VOID codex primary artifact and no complete §11 fallback pair)"
  fi
done <<EOF
$ROUNDS_ALL
EOF

if [ "$n" = "0" ]; then
  obs "every artifact in this arc is VOID — a failed round is not a round (PROCESS §3), so this arc records no completed review"
fi

# --- Tier A+ tier-up obligations: PRESENCE reported, discharge not decided.
SO_OBS=''
i=0
while [ "$i" -lt "$n" ]; do
  if [ "${R_ROLE[$i]}" = 'second-opinion' ]; then
    indep='independent'
    j=0
    while [ "$j" -lt "$n" ]; do
      if [ "${R_ROUND[$j]}" = "${R_ROUND[$i]}" ] && [ "${R_ROLE[$j]}" != 'second-opinion' ] \
         && [ "${R_REVIEWER[$j]}" = "${R_REVIEWER[$i]}" ]; then
        indep='NOT independent (same reviewer also holds role '"${R_ROLE[$j]}"' at this round)'
      fi
      j=$((j + 1))
    done
    SO_OBS="$SO_OBS round ${R_ROUND[$i]}/${R_REVIEWER[$i]} ($indep);"
  fi
  i=$((i + 1))
done
if [ "$TIER" = 'A+' ]; then
  if [ -n "$SO_OBS" ]; then
    obs "tier A+ obligation (1) independent second reviewer: second-opinion artifacts present —$SO_OBS"
  else
    obs "tier A+ obligation (1) independent second reviewer: none (no second-opinion artifact in this arc)"
  fi
  if [ "$REGCOUNT" = "0" ]; then
    obs "tier A+ obligation (2) byte-identity regression: none (no RVREG1 record in this arc)"
  else
    obs "tier A+ obligation (2) byte-identity regression: $REGCOUNT record(s) — $(printf '%s' "$REG_OBS" | sed 's/ $//')"
  fi
elif [ "$REGCOUNT" != "0" ]; then
  obs "$REGCOUNT RVREG1 regression record(s) in a tier-$TIER arc — RVREG1 exists to represent the Tier A+ tier-up and nothing in a tier-$TIER arc reads one"
elif [ -n "$SO_OBS" ]; then
  obs "second-opinion artifacts in a tier-$TIER arc:$SO_OBS"
fi
# regression rc and scope binding, stated per record (a nonzero rc is a
# FAILED byte-identity regression; a scope sha matching no round means the
# regression ran over bytes no artifact in this arc reviewed)
k=0
while [ "$k" -lt "$nreg" ]; do
  rn="${G_REGNAME[$k]}"
  if [ "${G_REGRC[$k]}" = "0" ]; then
    obs "regression '$rn': rc=0 (the archived command exited 0 — FORMAT.md §7 bounds what that proves)"
  else
    obs "regression '$rn': rc=${G_REGRC[$k]} — the byte-identity regression FAILED"
  fi
  match=''
  i=0
  while [ "$i" -lt "$nv" ]; do
    if [ "${V_SCOPESHA[$i]}" = "${G_REGSCOPE[$k]}" ]; then match="$match ${V_ROUND[$i]}"; fi
    i=$((i + 1))
  done
  if [ -n "$match" ]; then
    obs "regression '$rn': ran over the scope BYTES of round(s) $(printf '%s' "$match" | tr ' ' '\n' | LC_ALL=C sort -n -u | LC_ALL=C grep -v '^$' | tr '\n' ',' | sed 's/,$//')"
  else
    obs "regression '$rn': ran over different scope BYTES than every round in this arc"
  fi
  k=$((k + 1))
done

if [ "$CAPCOUNT" = "1" ]; then
  obs "cap present: authorized-by=$CAP_BY, recurring-class=\"$(printf '%s' "$CAP_CLASS" | cut -c1-80)\""
  obs "cap claims $CAP_ROUNDS rounds completed, $COUNTABLE countable round(s) measured on disk (a countable round has a non-VOID codex primary artifact or a complete §11 fallback pair)"
  cap_match=''
  i=0
  while [ "$i" -lt "$nv" ]; do
    if [ "${V_SCOPESHA[$i]}" = "$CAP_SCOPESHA" ]; then cap_match=1; fi
    i=$((i + 1))
  done
  [ -n "$cap_match" ] \
    || obs "cap caps different scope BYTES than every round in this arc"
fi

# ============================================================ emit report
printf 'ARC REPORT %s (DIAGNOSTIC — this tool does not decide whether an arc is closed)\n' "$ARC"
printf '  arc-dir: %s   tier-declared: %s   artifacts: %s (%s non-VOID)   scope-path-set: %s\n' \
  "$DIR" "$TIER" "$nv" "$NONVOID_TOTAL" "$(printf '%s' "$ARC_PATHSET" | cut -c1-12)"
printf '  rounds (label order is a CALLER LABEL, printed as recorded):\n'
i=0
while [ "$i" -lt "$nv" ]; do
  printf '    round %s %s %s: %s' \
    "${V_ROUND[$i]}" "${V_REVIEWER[$i]}" "${V_ROLE[$i]}" "${V_VERDICT[$i]}"
  if [ "${V_VERDICT[$i]}" = 'VOID' ]; then
    printf ' void-reason=%s' "${V_VOIDR[$i]}"
  fi
  printf ' rc=%s timeout-sec=%s harness-kill=%s started=%s ended=%s decision=%s in-region go=%s nogo=%s scope=%s\n' \
    "${V_RC[$i]}" "${V_TIMEOUT[$i]}" "${V_HKILL[$i]}" "${V_START[$i]}" "${V_END[$i]}" \
    "${V_DSRC[$i]}" "${V_GO[$i]}" "${V_NOGO[$i]}" \
    "$(printf '%s' "${V_SCOPESHA[$i]}" | cut -c1-12)"
  if [ "${V_ROLE[$i]}" = 'fallback' ]; then
    printf '      fallback-basis: %s\n' "${V_BASIS[$i]}"
  fi
  i=$((i + 1))
done
printf '  observations:\n'
if [ -n "$OBS" ]; then printf '%s' "$OBS"; else printf '  - none\n'; fi
cat <<'FOOT'
  open questions this tool does NOT answer (see FORMAT.md §5/§7):
  - round order: NOT ESTABLISHED. `round` is a caller flag; compare the
    started/ended times above (and the file dates) yourself. The rule that
    used to check this read only `started-utc` and was measured unsound.
  - §11 basis eligibility: NOT DECIDED here. Whether a VOID codex round is a
    PROVEN failure is a human read of its void-reason, rc, deadline and
    elapsed time above; the rule that used to decide it was manufacturable
    in zero seconds from the sanctioned CLI.
  - whether the reviewed SCOPE is the right scope, and whether the prompt
    actually demanded an adversarial review, are caller assertions.
  - whether any Tier A+ tier-up obligation is DISCHARGED.
NOT A CLOSURE DECISION. Arc closure is a driver/human judgement informed by
this evidence (owner ruling 2026-07-31). Exit 0 means a report was produced.
FOOT
exit 0
