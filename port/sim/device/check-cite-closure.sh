#!/bin/bash
# port/sim/device/check-cite-closure.sh — structured closure verification for
# the M4 freeze manifest (fix_plan C25 / C26 / C27).
#
# WHAT THE MANIFEST'S CITE COLUMN CANNOT PROVE, AND THIS CAN.
# verify_m4.sh stage [0] proves that every `reviewed-go` row cites at least one
# `.loop/*.log` whose TERMINAL anchored verdict is `VERDICT: GO`. That closed
# the iter-127 adorned/non-terminal class. Three holes stayed open, all with
# the same root cause — the cite is FREE TEXT, so nothing binds a verdict to a
# producer:
#   C25  the GO may belong to a DIFFERENT arc than the one that reviewed this
#        producer. "A real GO exists" is not "this producer was reviewed".
#   C26  a GO can be positionally terminal but semantically inert (pasted
#        transcript, fenced example). No anchored scan can tell them apart.
#   C27  a non-arc row's cite is matched by SUBSTRING, so a cite that DENIES
#        its gate ("...NOT-proven-by-SIM-CONFORMS...") passes.
# This check reads `port/sim/device/m4-closure-ledger.txt` instead: one
# fixed-arity record per closed manifest row, binding producer path + producer
# sha + verdict + the verdict's exact location in a sha-pinned log + the
# artifact that binds the arc to THIS producer + reviewer identity + round.
# Nothing here parses prose.
#
# PROCESS §3 whitelist-grammar rule is the construction rule throughout:
# every grammar below was MEASURED over the real corpus first, every parse is
# anchored and full-line, and anything that resembles-but-does-not-match is a
# refusal, never a partial parse and never a silent skip.
#
# REGISTERED RESIDUALS, stated honestly (raised as [H] by the round-2 arc and
# NOT closeable in this layer):
#   (1) C26's last shape - a NO-GO report followed by a pasted transcript whose
#       FOREIGN `VERDICT: GO` sits at EOF (or is followed only by an allowed RC
#       marker) - is byte-for-byte indistinguishable in tail shape from a
#       genuine terminal GO. Pinning the bytes and the line makes the claim
#       singular, frozen and driver-attested; it does not make it semantically
#       live. The durable fix is the reviewer HARNESS writing the verdict to its
#       own artifact instead of into the transcript (a PROCESS change). A LIVE
#       specimen exists: .loop/review-c25-1-codex-VOID.log, this lane's own
#       round-1 log, whose last anchored verdict belongs to a quoted foreign log.
#   (2) C25's `x-*` rows (19 of 59) rest on the verdict artifact and the binding
#       artifact being rounds of ONE arc. The corpus carries no machine-readable
#       arc identity - measured, arcs span iteration numbers - so this layer
#       GRADES and COUNTS those rows instead of pretending. Durable fix: an arc
#       id emitted by the review harness.
#   (2b) BOTH (1) and (2) have the SAME durable fix: the reviewer HARNESS
#       emitting its verdict to its own artifact, carrying an ARC ID and the
#       scope it reviewed. One PROCESS change closes both residuals — worth
#       surfacing to the owner rather than tracking them as two problems.
#   (3) a driver cap (`--cap`) is an assertion of intent, verified only against
#       a dated AGENT-LOG section that mentions the producer's unique basename.
#       That authenticates "some iteration logged that day discussed this file",
#       not "this specific closure". It is weaker than a GO by design.
#       STATED EXACTLY, because two review rounds asked for more and the corpus
#       cannot supply it: the ONE live cap is port/sim/device/adbsh.sh, its
#       unique binding section is the PARSER-AUDIT iteration beginning at
#       docs/AGENT-LOG.md:5555 (basename first mentioned at :5619), while the
#       section whose header actually says CAPPED-CLOSED is at :6756 and never
#       mentions adbsh.sh at all. So the binding proves that a dated iteration
#       discussed the file — NOT the cap and NOT its disposition. Requiring a
#       canonical cap record (date + exact producer path + disposition) would
#       FALSE-REJECT the only live cap, and writing that record into the ledger
#       of record is a driver/owner act, not a lane act — inventing the
#       evidence is precisely what this item exists to stop. Recorded here so
#       the weakness is visible at the point of use; the durable fix is a
#       driver-written cap record, then this arm binding to it.
#
# Prints `CITE CLOSURE OK: ...` and exits 0. Any violation -> refusal on
# stderr and a nonzero exit BEFORE the success line is printed. Negative
# tests: `.loop/closure-teeth.sh` (every tooth asserts the success line is
# ABSENT — a nonzero rc alone proves nothing, since a tree with open rows
# legitimately refuses at a later stage).
set -eu

MANIFEST=port/sim/device/m4-freeze-manifest.txt
LEDGER=port/sim/device/m4-closure-ledger.txt
AGENT_LOG=docs/AGENT-LOG.md

# INTEGRITY ANCHOR over the ledger's full bytes. Regenerate with
#   python3 port/sim/device/gen-closure-ledger.py --write
# which prints the replacement value. (No fixed point to solve: the ledger
# does not contain this script's hash, unlike the manifest/gate pair.)
LEDGER_SHA256=6c9df647b43b30ed26efcffda49615d88b1d80cd8b5df32ee031df177982fdba

refuse() { echo "CITE CLOSURE REFUSED: $*" >&2; exit 1; }

sha_of() { shasum -a 256 "$1" | awk '{print $1}'; }

# r4 [M]: `test -L <file>` tests only the FINAL component, so a symlinked
# `.loop` (or any parent of a producer) redirects the bytes outside the
# worktree while existence, hashing and coverage all still pass. Walk every
# component. Memoised because the 89 producers share ~15 directories.
DIRS_OK=""
dir_ok() { # <repo-relative path> — refuses if ANY parent component is a symlink
  do_acc=""
  do_rest="${1%/*}"
  [ "$do_rest" = "$1" ] && return 0          # no directory part
  OLDIFS="$IFS"; IFS=/
  for do_c in $do_rest; do
    IFS="$OLDIFS"
    case "$do_c" in
      ..|.)
        IFS="$OLDIFS"
        refuse "path '$1' contains a '$do_c' component — evidence paths must be
  plain repo-relative paths that cannot escape the worktree" ;;
    esac
    do_acc="${do_acc:+$do_acc/}$do_c"
    case " $DIRS_OK " in *" $do_acc "*) IFS=/; continue ;; esac
    if [ -L "$do_acc" ]; then
      IFS="$OLDIFS"
      refuse "path component '$do_acc' of '$1' is a SYMLINK — evidence must live
  in this worktree, not wherever a directory link points"
    fi
    if [ ! -d "$do_acc" ]; then
      IFS="$OLDIFS"
      refuse "path component '$do_acc' of '$1' is not a directory"
    fi
    DIRS_OK="$DIRS_OK $do_acc"
    IFS=/
  done
  IFS="$OLDIFS"
  return 0
}


# --- [1] ledger identity -------------------------------------------------
[ -e "$MANIFEST" ] || refuse "manifest missing: $MANIFEST"
if [ -L "$LEDGER" ]; then refuse "ledger is a SYMLINK, not evidence: $LEDGER"; fi
[ -f "$LEDGER" ] || refuse "ledger missing: $LEDGER"
[ -s "$LEDGER" ] || refuse "ledger is EMPTY: $LEDGER"
dir_ok "$LEDGER"
dir_ok "$MANIFEST"
if [ -L "$MANIFEST" ]; then refuse "manifest is a SYMLINK: $MANIFEST"; fi
if [ -f "$AGENT_LOG" ]; then
  dir_ok "$AGENT_LOG"
  if [ -L "$AGENT_LOG" ]; then refuse "ledger of record is a SYMLINK: $AGENT_LOG"; fi
fi
have="$(sha_of "$LEDGER")"
[ "$have" = "$LEDGER_SHA256" ] || refuse \
  "ledger bytes do not match the pinned anchor
  pinned:  $LEDGER_SHA256
  on disk: $have
  Any edit to the ledger must update LEDGER_SHA256= in this script in the
  SAME commit (hashes prove identity, not approval — PROCESS §4)."

# r9 [M]: the generator decodes its inputs as STRICT UTF-8 and refuses on
# failure (r8 [M]); this layer did not, so a manifest COMMENT carrying an
# invalid byte was refused by `gen` and ACCEPTED here — an encoding invariant
# enforced in one layer is not an invariant, it is the two-parser class again.
#
# The validator is deliberately the SAME decoder the generator uses, not an
# equivalent one. MEASURED, 2026-07-30, over a 20-case adversarial corpus:
# /usr/bin/iconv -f UTF-8 -t UTF-8 DISAGREES with Python's codec on 2 cases —
# it ACCEPTS `\xf4\x90\x80\x80` (a code point above U+10FFFF) and 5-byte
# sequences that Python refuses. Substituting iconv here would have reproduced
# the very divergence this fixes, one family narrower. If this is ever
# "simplified", re-run that corpus first.
#
# NOTE the manifest is NOT ASCII: it carries 176 bytes outside printable
# ASCII (em dashes in comments), so a byte whitelist would false-reject the
# live file. UTF-8 validity is the correct shared invariant, measured.
command -v python3 >/dev/null 2>&1 || refuse \
  "python3 is required to verify the UTF-8 invariant both layers depend on,
  and it is not on PATH. Refusing rather than skipping the check."
for encf in "$MANIFEST" "$LEDGER"; do
  python3 -c 'import sys
open(sys.argv[1], "rb").read().decode("utf-8")' "$encf" 2>/dev/null || refuse \
    "$encf is not valid UTF-8.
  The generator decodes strictly and would refuse to derive from these bytes,
  so accepting them here would let the two layers disagree about the same
  file. Corrupt input is corruption, not a character to be invented."
done

# --- [2] ledger grammar: anchored, fixed arity, no partial parses ---------
# Refuse the whole file on the first malformed line. A record with the wrong
# arity is corruption, not a row to skip: a truncated write that dropped the
# reviewer and round fields must not read as a valid 10-field closure.
# r5 [M]: `$1`/`NF` tolerated leading whitespace, runs of spaces, tabs and
# trailing whitespace, none of which the generator ever emits. The grammar is
# now the EXACT canonical line: `CLOSURE` plus exactly 11 single-space-
# separated fields containing no space, tab or CR.
bad="$(awk '
  /^#/ { next }
  /^[ \t]*$/ { next }
  /^LEDGER-OPEN-ROWS (0|[1-9][0-9]*)$/ { next }
  /^CLOSURE( [^ \t\r]+){11}$/ { next }
  { print NR ": not a canonical CLOSURE record: " $0; exit }
' "$LEDGER")"
[ -z "$bad" ] || refuse "ledger grammar violation at line $bad"

# r2 [M]: an UNTERMINATED final record is counted by every awk scan but
# SKIPPED by the `while read` loop below (read returns nonzero on a partial
# line and the body never runs), so the checker could print success without
# ever validating that producer. Count records here and assert at the end that
# exactly that many were processed.
LEDGER_RECORDS="$(awk '$1=="CLOSURE" && NF==12 { n++ } END { print n+0 }' "$LEDGER")"
[ "$LEDGER_RECORDS" -gt 0 ] || refuse "ledger contains no CLOSURE records"

# r5 [M]: an OPEN manifest row has no ledger witness BY CONSTRUCTION, so
# DELETING one used to pass cardinality silently — every remaining record still
# validated and the checker printed success. The generator therefore emits the
# open-row count it derived, and it is asserted against the manifest below.
# r6 [M]: this metadata line was neither a SINGLETON nor CANONICAL. Two
# `LEDGER-OPEN-ROWS` lines both parsed and the LAST silently won, and
# `[0-9]+` accepted `05` / `0000` for 5 / 0 — a machine-generated file with two
# spellings of one value is exactly the duplicate-key/producer-exact shape
# PROCESS §3 forbids. Exactly one line, canonical decimal, or refuse.
LEDGER_OPEN_N="$(awk '/^LEDGER-OPEN-ROWS /{ n++ } END { print n+0 }' "$LEDGER")"
[ "$LEDGER_OPEN_N" = 1 ] || refuse \
  "ledger carries $LEDGER_OPEN_N 'LEDGER-OPEN-ROWS' lines; exactly one is the
  canonical form (a duplicate would be silently resolved by scan order)"
LEDGER_OPEN="$(awk '/^LEDGER-OPEN-ROWS (0|[1-9][0-9]*)$/ { v = $2; ok = 1 }
  END { if (ok) print v }' "$LEDGER")"
# r6 [L] REACHABILITY, stated rather than left silent: this `case` CANNOT fire
# on any input. The whole-file grammar at [2] admits a `LEDGER-OPEN-ROWS` line
# ONLY in the canonical spelling `(0|[1-9][0-9]*)`, and the singleton arm above
# has already refused every count except exactly 1 — so by here the value is
# always present and always plain decimal. `05` is refused as a GRAMMAR
# violation (that is the arm T50 asserts), never here. It is KEPT, not deleted,
# because it is the extraction's own postcondition: if the grammar above is ever
# loosened, this is what stops a padded value from being read as a count. A
# defence that cannot fire today must be labelled as such, not left to read as
# live coverage.
case "$LEDGER_OPEN" in
  ''|*[!0-9]*) refuse "ledger has no canonical 'LEDGER-OPEN-ROWS <n>' line
  (n must be plain decimal with no leading zeros, sign or padding)" ;;
esac
case "$(tail -c 1 "$LEDGER" | od -An -c | tr -d ' ')" in
  '\n') : ;;
  *) refuse "ledger does not end with a newline — its final record would be
  silently skipped by the record loop (r2 [M])" ;;
esac

# r4 [M]: a trailing CR rode along inside field 12 (`round`) and every awk
# field split hid it. The ledger is machine-generated; a CR anywhere in it is
# corruption.
crbad="$(awk '/\r/ { print NR; exit }' "$LEDGER")"
[ -z "$crbad" ] || refuse "ledger line $crbad contains a CR byte"

dupe="$(awk '$1=="CLOSURE"{c[$2]++} END{for(p in c) if(c[p]!=1) print p" x"c[p]}' \
  "$LEDGER")"
[ -z "$dupe" ] || refuse "duplicate closure record(s): $dupe"

# --- [3] manifest side: cardinality in both directions --------------------
# Closed status => exactly one record. Open status => exactly zero. An
# arc-in-flight row with a closure record would be the laundering this whole
# item exists to stop, so it is refused explicitly rather than ignored.
# WHOLE-FILE whitelist grammar over the manifest (r2 [M]). Scanning only for
# lines that MATCH the record pattern let an indented / CR-tainted / truncated
# row disappear together with its closure obligation. Measured over the live
# manifest: every line is a comment, a blank, or a well-formed record, so this
# refuses nothing genuine.
mdupe="$(awk '
  /^[0-9a-f]{64} [^ \t\r]+ [^ \t\r]+ [^ \t\r]+$/ { c[$2]++ }
  END { for (p in c) if (c[p] != 1) print p " x" c[p] }' "$MANIFEST")"
[ -z "$mdupe" ] || refuse "duplicate manifest row(s): $mdupe
  Duplicate rows never reach per-record checks, so a duplicated OPEN row would
  be invisible to the cardinality rule (r4 [M])."

mbad="$(awk '
  /^#/ { next }
  /^[ \t]*$/ { next }
  /^[0-9a-f]{64} [^ \t\r]+ [^ \t\r]+ [^ \t\r]+$/ { next }
  { print NR ": " $0; exit }' "$MANIFEST")"
[ -z "$mbad" ] || refuse "manifest line is neither comment, blank, nor a
  well-formed record — at $mbad"

miss="$(awk -v L="$LEDGER" -v wantopen="$LEDGER_OPEN" '
  BEGIN { while ((getline l < L) > 0) { n=split(l,f," "); if (n==12 && f[1]=="CLOSURE") have[f[2]]=1 } }
  /^[0-9a-f]{64} [^ \t\r]+ [^ \t\r]+ [^ \t\r]+$/ {
    st = $3
    closed = (st=="reviewed-go" || st=="oracle-frozen" || st=="grandfathered-m1" || st=="grandfathered-m2")
    open   = (st=="arc-in-flight" || st=="arc-pending")
    if (!closed && !open) { print "unknown status " st " on " $2; next }
    if (closed && !($2 in have)) print "closed row with NO closure record: " $2 " (" st ")"
    if (open  &&  ($2 in have)) print "OPEN row carrying a closure record: " $2 " (" st ")"
    if (open) nopen++
    seen++
  }
  END {
    # NON-VACUITY: if the anchored row pattern ever stopped matching (an awk
    # regex-dialect difference, a manifest reformat), every check above would
    # pass over ZERO rows and this stage would be silently green.
    if (seen == 0) print "no manifest rows parsed at all — row grammar drift"
    if (nopen + 0 != wantopen + 0) print "manifest has " nopen + 0 " open row(s); the ledger was derived from " wantopen + 0
  }' "$MANIFEST")"
[ -z "$miss" ] || refuse "manifest/ledger cardinality
$(printf '%s\n' "$miss" | sed 's/^/  /')"

# r9 [M]: OPEN rows had NO path validation on this side. The generator applies
# safe_path() to EVERY manifest row, open ones included (that was r7b [L]), so
# replacing an OPEN producer with a symlink made `gen --write` refuse while
# `check` happily passed the unchanged ledger — the generator/checker
# divergence class, in the accident/corruption category the review bar names.
# An OPEN row has no closure record BY CONSTRUCTION, so it never reaches the
# per-record loop below where these checks live; it needs its own pass.
# The loop runs in THIS shell, not a subshell, so `refuse` really exits (the
# manifest grammar forbids whitespace in the path field, so word splitting on
# the awk output is exact).
open_paths="$(awk '
  /^[0-9a-f]{64} [^ \t\r]+ [^ \t\r]+ [^ \t\r]+$/ &&
  ($3 == "arc-in-flight" || $3 == "arc-pending") { print $2 }' "$MANIFEST")"
for op in $open_paths; do
  if [ -L "$op" ]; then
    refuse "$op: OPEN-row producer is a SYMLINK
  An open arc's producer is still evidence-in-waiting. The generator refuses
  this row; accepting it here would let the two layers disagree."
  fi
  dir_ok "$op"
done

orphan="$(awk -v M="$MANIFEST" '
  BEGIN { while ((getline l < M) > 0) if (l ~ /^[0-9a-f]{64} /) { split(l,f," "); row[f[2]]=1 } }
  $1=="CLOSURE" && !($2 in row) { print $2 }' "$LEDGER")"
[ -z "$orphan" ] || refuse "closure record for a path that is not a manifest row: $orphan"

# --- [3b] the artifacts a record may name -------------------------------
# r3 [H]: `.loop/*` was a bare string PREFIX, so
# `.loop/../port/sim/device/m4-freeze-manifest.txt` passed containment, was a
# regular file, and contains every producer path and sha — i.e. ANY real GO
# could be paired with the manifest itself as coverage. Two closures:
#   (a) an ANCHORED artifact grammar, identical to verify_m4.sh's cite_refs
#       character class: `.loop/<name>.(log|txt)` with NO path separator after
#       `.loop/`, so traversal is unrepresentable rather than filtered;
#   (b) MEMBERSHIP — the artifact must be one of the references the manifest's
#       own cite for THAT row resolves to (brace alternatives expanded). The
#       ledger may re-express the arc's evidence, never introduce new evidence.
# The (path, ref) pairs are materialised once here with the same
# boundary-anchored scan verify_m4.sh uses, so the two layers cannot drift.
CITEREFS="$(mktemp)" || refuse "cannot create a temporary file"
trap 'rm -f "$CITEREFS"' EXIT

# r6 [M]: the two layers disagreed on MALFORMED brace groups and NEITHER
# refused — an EMPTY group expands to one alternative in the generator's Python
# and to ZERO in this awk (so the generator could select an artifact this
# membership set can never contain), and an UNMATCHED `{` is passed through
# there and mangled into a concatenation here. MEASURED over the live manifest:
# all 3 brace refs are well-formed with non-empty alternatives, so refusing the
# malformed forms in BOTH layers costs zero real rows and replaces two silent,
# divergent behaviours with one refusal.
brbad="$(awk '
  /^[0-9a-f]{64} [^ \t\r]+ [^ \t\r]+ [^ \t\r]+$/ {
    s = $4
    while (match(s, /\.loop\/[A-Za-z0-9._{},-]+\.(log|txt)/)) {
      ref = substr(s, RSTART, RLENGTH)
      s   = substr(s, RSTART + RLENGTH)
      no = gsub(/[{]/, "{", ref); nc = gsub(/[}]/, "}", ref)
      if (no == 0 && nc == 0) continue
      if (no != 1 || nc != 1 || index(ref, "{") > index(ref, "}")) {
        print $2 ": " ref " has a malformed brace group"; exit }
      alts = ref; sub(/^[^{]*[{]/, "", alts); sub(/[}].*/, "", alts)
      if (alts == "" || alts ~ /^,/ || alts ~ /,$/ || alts ~ /,,/) {
        print $2 ": " ref " has an empty brace alternative"; exit }
    }
  }' "$MANIFEST")"
[ -z "$brbad" ] || refuse "manifest cite grammar: $brbad
  Exactly one non-empty '{a,b}' group is representable; anything else is
  parsed differently by the generator and by this checker, so it is refused
  by both rather than resolved differently by each."

awk '
  function emit(p, ref) {
    if (ref ~ /[{]/) {
      pre = ref; sub(/[{].*/, "", pre)
      alts = ref; sub(/^[^{]*[{]/, "", alts); sub(/[}].*/, "", alts)
      post = ref; sub(/^[^}]*[}]/, "", post)
      n = split(alts, A, ",")
      for (k = 1; k <= n; k++) print p "\t" pre A[k] post
    } else print p "\t" ref
  }
  /^[0-9a-f]{64} [^ \t\r]+ [^ \t\r]+ [^ \t\r]+$/ {
    p = $2; s = $4
    while (match(s, /\.loop\/[A-Za-z0-9._{},-]+\.(log|txt)/)) {
      ref = substr(s, RSTART, RLENGTH)
      s   = substr(s, RSTART + RLENGTH)
      nxt = substr(s, 1, 1)
      if (nxt == "" || nxt == "+" || nxt == "-") emit(p, ref)
    }
  }' "$MANIFEST" > "$CITEREFS"

art_ok() { # <producer> <artifact> — anchored grammar + cite membership
  case "$2" in
    .loop/*) : ;;
    *) refuse "$1: artifact '$2' is not under .loop/" ;;
  esac
  ao_name="${2#.loop/}"
  case "$ao_name" in
    *[!A-Za-z0-9._,{}-]*) refuse "$1: artifact '$2' is not a bare .loop/ name
  (path traversal and subdirectories are not representable here)" ;;
  esac
  # r6 [M]: `.` and `..` pass the character class (both are made of allowed
  # bytes) and were refused only incidentally, by the .log/.txt extension rule
  # further down — so a traversal-shaped artifact was diagnosed as a filename
  # typo. Refuse the two directory entries HERE, with the traversal diagnosis
  # they earn, before any weaker rule can claim the refusal.
  case "$ao_name" in
    .|..) refuse "$1: artifact '$2' is not a bare .loop/ name
  ('.' and '..' are directory entries, not artifacts; path traversal and
  subdirectories are not representable here)" ;;
  esac
  case "$ao_name" in
    *.log|*.txt) : ;;
    *) refuse "$1: artifact '$2' is not a .log or .txt file" ;;
  esac
  dir_ok "$2"
  grep -qxF -- "$1	$2" "$CITEREFS" || refuse \
    "$1: artifact '$2' is not among the references this row's manifest cite
  resolves to. The ledger may re-express the arc's evidence, never add new
  evidence the manifest never claimed."
}

# --- [4] per-record verification -----------------------------------------
n_go=0; n_cap=0; n_gate=0; n_self=0; n_cross=0; n_sha=0; n_seen=0
while read -r kw path psha verdict ev evsha vline cov covref covsha reviewer round; do
  [ "$kw" = CLOSURE ] || continue
  n_seen=$((n_seen + 1))

  # 4a. producer identity: ledger == manifest == bytes on disk.
  mrow="$(awk -v p="$path" '$0 ~ /^[0-9a-f]{64} / { if ($2 == p) print $1" "$3 }' \
    "$MANIFEST")"
  [ -n "$mrow" ] || refuse "$path: no manifest row (orphan closure record)"
  set -- $mrow
  [ $# -eq 2 ] || refuse "$path: manifest row is malformed or duplicated"
  msha="$1"; status="$2"
  [ "$psha" = "$msha" ] || refuse \
    "$path: ledger sha $psha != manifest sha $msha"
  if [ -L "$path" ]; then refuse "$path: producer is a SYMLINK"; fi
  dir_ok "$path"
  [ -f "$path" ] || refuse "$path: producer file does not exist"
  dsha="$(sha_of "$path")"
  [ "$psha" = "$dsha" ] || refuse \
    "$path: closure record pins $psha but the bytes on disk are $dsha
  A closure approves BYTES. Re-review and re-derive; do not re-pin silently."

  # 4b. verdict vocabulary, matched WHOLE-FIELD (C27). Substring matching is
  # what let `...NOT-proven-by-SIM-CONFORMS...` pass; a closed vocabulary in
  # its own field makes the denial form unrepresentable.
  case "$verdict" in
    GO|CAPPED-CLOSED|PROVEN-BY-HARD-RULE-3|\
PROVEN-BY-M1-EXIT-GATE|PROVEN-BY-M2-EXIT-GATE) : ;;
    *) refuse "$path: verdict '$verdict' is not in the closed vocabulary" ;;
  esac
  # r2 [H]: the gate token is RECOMPUTED from the manifest's ANCHORED status
  # field and required to match, so the ledger field is checked evidence
  # rather than a value the checker trusts. The generator derives it the same
  # way and never reads cite prose — substring-matching a cite for a gate name
  # turns `...NOT-proven-by-SIM-CONFORMS...` into an affirmative claim, which
  # is C27 relocated, not closed.
  case "$status" in
    reviewed-go)      want_verdict="" ;;
    oracle-frozen)    want_verdict=PROVEN-BY-HARD-RULE-3 ;;
    grandfathered-m1) want_verdict=PROVEN-BY-M1-EXIT-GATE ;;
    grandfathered-m2) want_verdict=PROVEN-BY-M2-EXIT-GATE ;;
    *) refuse "$path: status '$status' is not a closed status" ;;
  esac
  if [ -n "$want_verdict" ]; then
    [ "$verdict" = "$want_verdict" ] || refuse \
      "$path: status '$status' means '$want_verdict', but the record claims '$verdict'"
  else
    case "$verdict" in
      GO|CAPPED-CLOSED) : ;;
      *) refuse "$path: status 'reviewed-go' cannot carry verdict '$verdict'" ;;
    esac
  fi

  # 4b-bis. The verdict x coverage PRODUCT, as a closed set, checked BEFORE any
  # per-verdict field check (r3 [H]): enforcing it only coverage -> verdict let
  # a `reviewed-go` row claim CAPPED-CLOSED with cov=x-sha and skip the
  # driver-date, AGENT-LOG and reviewer checks entirely while still reaching the
  # success line. Checking it first also means an illegal combination is
  # reported as itself rather than as a downstream field symptom.
  case "$verdict/$cov" in
    GO/self-sha|GO/self-path|GO/self-base|GO/x-sha|GO/x-path|GO/x-base) : ;;
    CAPPED-CLOSED/ledger) : ;;
    PROVEN-BY-*/gate) : ;;
    *) refuse "$path: verdict '$verdict' may not carry coverage kind '$cov'" ;;
  esac

  # 4c. arc closures: the verdict must be AT the pinned line of a sha-pinned
  # log, must be that log's LAST anchored verdict, and nothing but blanks and
  # one reviewer RC marker may follow it.
  if [ "$verdict" = GO ]; then
    art_ok "$path" "$ev"
    # r6 [M]: art_ok accepts `.log` OR `.txt` (7 live covrefs are `.txt` diff
    # artifacts), but only a `.log` can carry a VERDICT — the generator never
    # even reads a non-`.log` reference when looking for one. MEASURED: all 59
    # live `ev` are `.log`. Enforce here what the generator assumes, so the
    # checker cannot accept a record the generator would never emit.
    case "$ev" in
      *.log) : ;;
      *) refuse "$path: evidence '$ev' is not a .log — only a review log may
  carry a verdict; a .txt artifact may bind coverage, never close an arc" ;;
    esac
    if [ -L "$ev" ]; then refuse "$path: evidence is a SYMLINK: $ev"; fi
    [ -f "$ev" ] || refuse "$path: evidence does not exist: $ev
  Copy the arc's logs into the repo-root .loop/ — an arc run in another
  worktree leaves them invisible here (PROCESS §12.3(5))."
    [ -s "$ev" ] || refuse "$path: evidence is EMPTY: $ev"
    esha_now="$(sha_of "$ev")"
    [ "$evsha" = "$esha_now" ] || refuse \
      "$path: evidence $ev has changed since closure
  pinned:  $evsha
  on disk: $esha_now
  Appending to a cited log silently changes what any scan reads. Pinned
  evidence makes that tamper-evident instead."
    case "$vline" in ''|*[!0-9]*) refuse "$path: verdict line '$vline' is not a number" ;; esac

    # MEASURED GRAMMAR (all 21 cited terminal-GO logs, 2026-07-29): the only
    # thing that ever follows a terminal GO is a single `<TOOL>_RC=<n>` line.
    # A pasted transcript or a fenced example is followed by prose, so this
    # positive whitelist refuses both C26 shapes with zero false rejections on
    # genuine data. (A fence-PARITY rule was measured and REJECTED instead: it
    # false-REDs .loop/c6-review-codex-r4.log, whose real terminal GO trails
    # an indented stray ``` at :4943.)
    # r2 [M]: implemented EXACTLY as the measured grammar states. Everything
    # after the pinned verdict must be an EMPTY line or a reviewer RC marker,
    # and there must be AT MOST ONE marker — the previous code accepted many
    # and silently used the first, so two conflicting identities passed. No
    # whitespace is stripped beyond a trailing CR, matching the generator byte
    # for byte (they disagreed before: it stripped, this did not).
    vstat="$(awk -v want="$vline" '
      { s = $0; sub(/\r$/, "", s)
        if (s == "VERDICT: GO" || s == "VERDICT: NO-GO") { last = NR; lastv = s }
        if (NR == want) atline = s
        if (NR > want && s != "") {
          if (s ~ /^[A-Z][A-Z0-9]*_RC=0$/) { marks++; m = s; sub(/_RC=.*/, "", m); mk = mk (mk ? "," : "") m }
          else trailing++ } }
      END {
        printf "%s|%s|%d|%d|%d|%s\n", atline, lastv, last, trailing + 0, marks + 0, mk }' "$ev")"
    at="${vstat%%|*}";      rest="${vstat#*|}"
    lastv="${rest%%|*}";    rest="${rest#*|}"
    lastn="${rest%%|*}";    rest="${rest#*|}"
    trailing="${rest%%|*}"; rest="${rest#*|}"
    nmarks="${rest%%|*}";   marklist="${rest#*|}"
    [ "$at" = "VERDICT: GO" ] || refuse \
      "$path: $ev:$vline is not an unadorned 'VERDICT: GO' (got: '$at')
  Verdicts are emitted UNADORNED at column 0 (PROCESS §3/C11); an adorned
  verdict is void, not interpreted."
    [ "$lastn" = "$vline" ] || refuse \
      "$path: $ev:$vline is not the LAST anchored verdict (last is :$lastn '$lastv')"
    [ "$trailing" = 0 ] || refuse \
      "$path: $ev has $trailing line(s) of content after the verdict at :$vline
  Measured over the whole corpus, nothing but a single <TOOL>_RC=<n> marker
  ever follows a terminal verdict. Content after it is the appended-transcript
  shape (fix_plan C26) — a GO that is positionally terminal but inert."

    [ "$nmarks" -le 1 ] || refuse \
      "$path: $ev carries $nmarks reviewer RC markers after the verdict ($marklist)
  The measured grammar allows at most one; more than one means the log is a
  concatenation and its identity is ambiguous."

    # 4d. reviewer identity is DERIVED and compared, never accepted as given,
    # and never DEFAULTED (r2 [M]): calling a markerless log `codex` was an
    # invention, not a derivation.
    # r8 [L]: this said "10 of the 17" and had gone stale. RE-MEASURED over the
    # live ledger with the generator's own parser: 18 distinct pinned closure
    # logs, 7 carry an RC marker, 11 carry none (9 of those end
    # `unattributed`, 2 resolve by filename token). The reproduction command
    # is in gen-closure-ledger.py's reviewer_of() docstring — one source, so
    # the two copies of this measurement cannot drift apart again.
    evbase="${ev##*/}"
    case "$marklist" in
      CODEX|CODEX3) want_rev=codex ;;
      GROK)         want_rev=grok ;;
      OPUS)         want_rev=opus5 ;;
      '')           case "$evbase" in
                      *grok*) want_rev=grok ;;
                      *opus*) want_rev=opus5 ;;
                      *)      want_rev=unattributed ;;
                    esac ;;
      *)            refuse "$path: unknown reviewer RC marker '${marklist}_RC=' in $ev" ;;
    esac
    [ "$reviewer" = "$want_rev" ] || refuse \
      "$path: reviewer '$reviewer' contradicts the evidence ($ev says '$want_rev')"

    # 4e. round is DERIVED from the evidence basename and compared exactly.
    # r4 [M]: a substring test accepted meaningless values (`.` matched every
    # basename). The generator computes exactly this, so equality costs nothing
    # and removes the whole class.
    want_round="${evbase%.log}"
    case "$want_round" in review-*) want_round="${want_round#review-}" ;; esac
    [ "$round" = "$want_round" ] || refuse \
      "$path: round '$round' is not the evidence log's round ('$want_round' from $evbase)"
  else
    [ "$ev" = "-" ]    || refuse "$path: verdict '$verdict' must carry ev '-', got '$ev'"
    [ "$evsha" = "-" ] || refuse "$path: verdict '$verdict' must carry evsha '-'"
    [ "$vline" = "-" ] || refuse "$path: verdict '$verdict' must carry vline '-'"
    # r4 [M]: `round` was unchecked for all 25 non-GO records.
    [ "$round" = "-" ] || refuse "$path: verdict '$verdict' must carry round '-', got '$round'"
  fi

  # 4f. coverage: what binds this ARC to THIS producer.
  # MEASURED (see the ledger header): the closure round frequently does not
  # name the producer at all — the arc's earlier rounds do. So the binding is
  # a separate, separately sha-pinned artifact, and refusing to model that
  # would false-reject 22 of 59 genuine rows.
  base="${path##*/}"
  case "$cov" in
    self-sha|self-path|self-base|x-sha|x-path|x-base)
      # `self-*` = the binding lives in the SAME sha-pinned artifact that
      # carries the verdict, so no cross-artifact arc relation is assumed.
      # STATED HONESTLY (r3 [H]): a substring match is not a scope
      # declaration — it proves the closing artifact MENTIONS this producer
      # (or its exact sha), not that the review's scope was this producer.
      # A harness-emitted scope record is the only thing that would.
      # `x-*` = the binding lives in a DIFFERENT cited artifact, so the row
      # additionally rests on those two artifacts being rounds of ONE arc,
      # which nothing in the corpus makes machine-checkable. Those rows are
      # graded and counted rather than silently equated with the strong ones;
      # the durable fix is an arc id emitted by the review harness.
      case "$cov" in
        self-*) [ "$covref" = "$ev" ] || refuse \
          "$path: cov=$cov claims the verdict log itself binds the producer, but covref ($covref) != ev ($ev)" ;;
        x-*)    [ "$covref" != "$ev" ] || refuse \
          "$path: cov=$cov claims a cross-artifact binding, but covref == ev ($ev) — it must be recorded as self-${cov#x-}" ;;
      esac
      art_ok "$path" "$covref"
      if [ -L "$covref" ]; then refuse "$path: covref is a SYMLINK: $covref"; fi
      [ -f "$covref" ] || refuse "$path: covref does not exist: $covref"
      [ -s "$covref" ] || refuse "$path: covref is EMPTY: $covref"
      csha_now="$(sha_of "$covref")"
      [ "$covsha" = "$csha_now" ] || refuse \
        "$path: covref $covref has changed since closure
  pinned:  $covsha
  on disk: $csha_now"
      case "${cov#*-}" in
        sha)  needle="$psha" ;;
        path) needle="$path" ;;
        base)
          ndup="$(awk -v b="$base" '$0 ~ /^[0-9a-f]{64} / { n=$2; sub(/^.*\//,"",n); if (n==b) c++ } END { print c+0 }' "$MANIFEST")"
          [ "$ndup" = 1 ] || refuse \
            "$path: cov=base but basename '$base' is not unique among pinned producers ($ndup) — the needle would be vacuous"
          needle="$base" ;;
      esac
      grep -qF -- "$needle" "$covref" || refuse \
        "$path: covref $covref does not contain the ${cov#*-} needle '$needle'
  The cited arc therefore does not demonstrably cover this producer (C25)."
      case "$cov" in self-*) n_self=$((n_self + 1)) ;; *) n_cross=$((n_cross + 1)) ;; esac
      case "$cov" in *-sha) n_sha=$((n_sha + 1)) ;; esac
      n_go=$((n_go + 1)) ;;
    ledger)
      [ "$verdict" = CAPPED-CLOSED ] || refuse "$path: cov=ledger requires verdict CAPPED-CLOSED"
      [ "$covsha" = "-" ] || refuse "$path: cov=ledger must carry covsha '-'"
      case "$covref" in
        [0-9][0-9][0-9][0-9]-[0-9][0-9]-[0-9][0-9]) : ;;
        *) refuse "$path: cov=ledger covref '$covref' is not a YYYY-MM-DD date" ;;
      esac
      ndup="$(awk -v b="$base" '$0 ~ /^[0-9a-f]{64} / { n=$2; sub(/^.*\//,"",n); if (n==b) c++ } END { print c+0 }' "$MANIFEST")"
      [ "$ndup" = 1 ] || refuse \
        "$path: CAPPED-CLOSED binds on basename, but '$base' is not unique ($ndup) — vacuous"
      [ -f "$AGENT_LOG" ] || refuse "$path: cov=ledger but $AGENT_LOG is missing"
      # r6 [M]: this bound on `index(header, date) > 0` — ANY H2 whose text
      # happens to contain the date, INCLUDING one whose prose mentions it
      # ("re-examine the 2026-07-17 cap") — and on a bare substring for the
      # basename, so `xadbsh.shy` counted. It also counted MENTION LINES across
      # every such section, so several unrelated sections could each
      # contribute. Now: the date must be a delimited header FIELD, the
      # basename must be a whole token, and EXACTLY ONE section may bind.
      # MEASURED over docs/AGENT-LOG.md: 179 `## ` headers, 177 carry a date,
      # and all 177 carry it in one of exactly two delimited shapes
      # (`— <date> — ` x176, `(<date>, ` x1). The live cap binds exactly one
      # section. Zero false rejections; a date buried in prose no longer binds.
      nbind="$(awk -v d="$covref" -v p="$base" '
        # r7b [M]: this re-sliced s after a rejected occurrence, so the NEXT
        # occurrence lost its left-hand context and adbsh.shadbsh.sh was
        # accepted as containing a whole-token basename, while the generator
        # Python regex rejected it. Track ABSOLUTE offsets into the original
        # line instead, and advance by ONE character so adjacent repeats are
        # still examined. (NOTE: no apostrophes here - this comment lives
        # INSIDE a single-quoted awk program, and one apostrophe ends it.)
        function tok(s,   i, base, c1, c2) {
          base = 0
          while ((i = index(substr(s, base + 1), p)) > 0) {
            i += base
            c1 = (i == 1) ? "" : substr(s, i - 1, 1)
            c2 = substr(s, i + length(p), 1)
            if (c1 !~ /[A-Za-z0-9._-]/ && c2 !~ /[A-Za-z0-9._-]/) return 1
            base = i
          }
          return 0
        }
        /^## / {
          if (insec && hit) n++
          insec = ($0 ~ ("^## .* — " d " — ") || $0 ~ ("^## .*\\(" d ", ")) ? 1 : 0
          hit = 0
        }
        insec && tok($0) { hit = 1 }
        END { if (insec && hit) n++; print n + 0 }' "$AGENT_LOG")"
      [ "$nbind" = 1 ] || refuse \
        "$path: CAPPED-CLOSED names $covref but $nbind dated section(s) of
  $AGENT_LOG mention '$base' as a whole token; exactly one must. A bare date
  is not evidence, and a date that only appears in a section's PROSE is not a
  section date."
      [ "$reviewer" = driver ] || refuse "$path: a driver cap must record reviewer 'driver'"
      n_cap=$((n_cap + 1)) ;;
    gate)
      case "$verdict" in PROVEN-BY-*) : ;; *) refuse "$path: cov=gate requires a PROVEN-BY-* verdict" ;; esac
      [ "$covref" = "-" ] || refuse "$path: cov=gate must carry covref '-'"
      [ "$covsha" = "-" ] || refuse "$path: cov=gate must carry covsha '-'"
      [ "$reviewer" = gate ] || refuse "$path: cov=gate must record reviewer 'gate'"
      n_gate=$((n_gate + 1)) ;;
    *) refuse "$path: unknown coverage kind '$cov'" ;;
  esac
done < "$LEDGER"

# r2 [M]: prove every counted record actually reached the per-record checks.
[ "$n_seen" = "$LEDGER_RECORDS" ] || refuse \
  "processed $n_seen record(s) but the ledger declares $LEDGER_RECORDS —
  a record was skipped without being validated."
# r3 [H]: and every processed record must land in exactly one reported
# category, so none can pass through uncounted and be omitted from the total.
[ "$((n_go + n_cap + n_gate))" = "$n_seen" ] || refuse \
  "category totals ($n_go GO + $n_cap capped + $n_gate gate) do not account for
  all $n_seen processed records."
[ "$((n_self + n_cross))" = "$n_go" ] || refuse \
  "coverage grading ($n_self self + $n_cross cross) does not account for all
  $n_go arc-GO records."

# The grading is REPORTED, not hidden, because it is the honest measure of how
# much each row rests on: a `*-sha` bind means the artifact carries the
# producer's EXACT current sha256 (only a packet that showed those bytes has
# it); `self-*` means the verdict artifact itself carries the binding. Neither
# is a SCOPE declaration — see the registered residuals above.
echo "CITE CLOSURE OK: $((n_go + n_cap + n_gate)) records verified ($n_go arc-GO, $n_cap driver-capped, $n_gate gate-proven; $n_self of the arc-GO rows name the producer in the verdict artifact itself, $n_cross rest on a cross-artifact arc relation; $n_sha bind on the producer's exact sha256)"
