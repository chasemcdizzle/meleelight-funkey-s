#!/usr/bin/env bash
# port/review/review-harness.sh — the reviewing harness (fix_plan R4).
#
# It LAUNCHES a review round and, at completion, writes its OWN verdict
# artifact (format RVERDICT2, grammar pinned in port/review/FORMAT.md).
# The artifact is written BY THE PRODUCER, never hand-authored and never
# inferred by a later reader. `port/review/arc-report.sh` READS those
# artifacts diagnostically; it does not decide whether an arc is closed, and
# neither does this script. Arc closure is a driver/human judgement (owner
# ruling 2026-07-31, docs/STATE.md §rulings; docs/PROCESS.md §3.1). What this
# producer is for is that the judgement rests on a provenance-bound record
# instead of on a reader's interpretation of log bytes.
#
# The five measured failure modes this exists to close (2026-07-30, all in
# one day — see docs/STATE.md §rulings and fix_plan R4):
#   1. foreign-GO-at-EOF laundering: a round's real terminal verdict was
#      NO-GO, and a QUOTED foreign transcript pasted after it left an
#      anchored `VERDICT: GO` as the file's last verdict. Closed here TWICE
#      over: (a) the decision is read from a dedicated DECISION CHANNEL — for
#      codex, `--output-last-message`, which only the reviewer's final message
#      can reach, so no tool output or quoted transcript can supply it — and
#      the transcript must AGREE with that channel or the round is void; and
#      (b) the harness terminates the transcript with a nonce-bearing
#      `REVIEWER_RC <nonce> <rc>` + `HARNESS-EOR <nonce>` pair, reads the
#      transcript's verdict only from the region ahead of it, and seals both
#      files' sha256 — so anything appended afterwards is out of region, after
#      the terminator, AND a hash mismatch. Live specimen kept at
#      port/review/specimens/foreign-verdict-at-eof.log.
#      (c) ADDED 2026-07-31, because (a) and (b) left this mode OPEN on every
#      path that is not codex — §11's fallback pair and the Tier A+ second
#      opinion, i.e. exactly the two-reviewer path failure #4 was about. A
#      reviewer that emits its real NO-GO and then QUOTES a foreign GO is
#      INSIDE the region and BEFORE the terminator; it decided GO, and it
#      closed an arc. Any transcript-decided round whose region carries BOTH
#      anchored verdicts is now VOID (`ambiguous-verdict`): two candidates and
#      a positional tie-break is not a decision.
#   2. no machine-readable arc identity: every artifact carries arc-id, round,
#      reviewer, role and the exact reviewed scope digest.
#   3. fabricated work-status: nothing a writer says is evidence; the judge
#      re-derives every field derivable from the arc's evidence and refuses on
#      disagreement.
#   4. "arc reached GO" under PROCESS §11's two-reviewer fallback with ONE
#      reviewer: role + reviewer + fallback-basis are recorded, and the judge
#      requires BOTH members of the pair, on the same prompt and scope, each
#      resting on the same recorded failed-codex artifact.
#   5. corrupt logs whose readable text contradicts their rc: the artifact
#      records the reviewer rc and the log's NUL count, the judge re-measures
#      both, and ANY nonzero reviewer exit disqualifies the round. Live
#      specimen kept at port/review/specimens/nul-corrupt-rc1.log (44.4% NUL,
#      contains `DEVICE TARGET CONFORMS`, run exited `TARGET_RC=1`).
#
# USAGE (a review round; PROCESS §7.8 — launch it DETACHED and poll for the
# artifact, never for a substring of the log):
#
#   nohup bash port/review/review-harness.sh run \
#     --arc c25-cite-closure --round 7 --reviewer codex --role primary \
#     --tier A --prompt .loop/review-c25-7-prompt.md \
#     --scope .loop/c25-scope.txt > .loop/harness-c25-r7.out 2>&1 &
#
#   # then poll:  ls .loop/arc/c25-cite-closure/r007-*.verdict
#
# USAGE (a PROCESS §11 fallback round — BOTH are required, each naming the
# recorded codex artifact that failed):
#
#   ... run --arc X --round 7 --reviewer grok  --role fallback \
#       --fallback-basis r007-codex-primary-20260730T101500Z-1a2b3c4d.verdict ...
#   ... run --arc X --round 7 --reviewer opus5 --role fallback \
#       --fallback-basis r007-codex-primary-20260730T101500Z-1a2b3c4d.verdict ...
#
# USAGE (a PROCESS §3 cap record; driver duty):
#
#   bash port/review/review-harness.sh cap \
#     --arc menus --tier A+ --rounds 20 --scope .loop/menus-scope.txt \
#     --authorized-by owner-ruling-2026-07-30 \
#     --class 'the judge/normalizer surface admits new loosenings faster than point fixes close them'
#
# EXIT CODES: 0 = artifact written (whatever the verdict — a NO-GO round is a
# successful harness run); 2 = the harness REFUSED (bad arguments, unusable
# scope, path collision) and wrote no artifact.

set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=port/review/reviewers.sh
. "$HERE/reviewers.sh"

die() { printf 'HARNESS REFUSED: %s\n' "$*" >&2; exit 2; }

sha_file() { shasum -a 256 "$1" | cut -d' ' -f1; }
sha_str() { printf '%s' "$1" | shasum -a 256 | cut -d' ' -f1; }

# grepc <pattern> <file> — anchored-line count that is 0, not a pipeline
# failure, when nothing matches (`set -o pipefail` + `grep -c` returning 1 on
# zero matches would otherwise abort the run mid-measurement).
grepc() {
  local n
  n="$(LC_ALL=C grep -a -c -- "$1" "$2" 2>/dev/null || true)"
  [ -n "$n" ] || n=0
  printf '%s' "$n" | tr -d ' '
}

# terminal_verdict <file> <upto-line> — prints "<line>:<GO|NO-GO>" for the LAST
# anchored, unadorned, column-0 verdict at or before <upto-line>, or nothing.
terminal_verdict() {
  local f="$1" upto="$2" region last
  region="$(mktemp)"
  head -n "$upto" "$f" > "$region"
  last="$(LC_ALL=C grep -a -nE '^VERDICT: (GO|NO-GO)$' "$region" | tail -1 || true)"
  rm -f "$region"
  [ -n "$last" ] || return 0
  printf '%s:%s' "${last%%:*}" "${last#*:VERDICT: }"
}

utc() { date -u +%Y-%m-%dT%H:%M:%SZ; }

# ---------------------------------------------------------------- repo root
ROOT="$(git rev-parse --show-toplevel 2>/dev/null || true)"
[ -n "$ROOT" ] || die "not inside a git worktree (the harness records repo-relative paths)"
cd "$ROOT"

ARCDIR="${MLFK_ARC_DIR:-.loop/arc}"
# Repo-RELATIVE by construction: the artifact records repo-relative paths so
# that the judge can re-open every piece of evidence from any checkout, and an
# absolute path would fail its grammar at read time rather than at write time.
printf '%s' "$ARCDIR" | LC_ALL=C grep -qE '^[A-Za-z0-9_.][A-Za-z0-9._/-]{0,200}$' \
  || die "MLFK_ARC_DIR '$ARCDIR' must be a repo-relative path"
case "$ARCDIR" in *..*) die "MLFK_ARC_DIR '$ARCDIR' must not contain '..'" ;; esac

# PROCESS §12.3(5): evidence written inside a LANE worktree dies with the
# worktree while cites point at it. The arc directory is self-contained and
# uses only paths relative to the repo root, so copying it into the main
# tree's .loop/ preserves every reference — but somebody has to do it, so say
# so loudly rather than leaving it to be remembered.
IN_LINKED_WORKTREE=0
MAIN_ROOT=''
if [ "$(git rev-parse --git-common-dir)" != "$(git rev-parse --git-dir)" ]; then
  IN_LINKED_WORKTREE=1
  MAIN_ROOT="$(cd "$(dirname "$(cd "$(git rev-parse --git-common-dir)" && pwd)")" && pwd)"
fi
# The destination is the SAME repo-relative path in the main worktree —
# `.loop/arc/<id>` there, not `.loop/<id>` — because the artifacts reference
# their evidence by that exact relative path. Say the whole path, so the copy
# cannot be done to a destination that silently breaks every reference.
worktree_notice() {
  [ "$IN_LINKED_WORKTREE" = "1" ] || return 0
  printf 'HARNESS NOTICE: this is a LANE worktree. PROCESS §12.3(5): before it is pruned, copy the arc directory to the SAME repo-relative path in the main worktree:\n' >&2
  printf 'HARNESS NOTICE:   mkdir -p %s/%s && cp -R %s/. %s/%s/\n' \
    "${MAIN_ROOT:-<main-root>}" "$(dirname "$1")" "$1" "${MAIN_ROOT:-<main-root>}" "$1" >&2
}

# ------------------------------------------------------------- validators
# Every one of these is an ANCHORED, full-line match (PROCESS §3's
# whitelist-grammar rule): exact match -> accept, resembles-but-does-not ->
# refuse. No permissive parsing anywhere in this file.
v_arcid()    { printf '%s' "$1" | LC_ALL=C grep -qE '^[a-z0-9][a-z0-9._-]{0,63}$'; }
v_round()    { printf '%s' "$1" | LC_ALL=C grep -qE '^[1-9][0-9]{0,2}$'; }
v_tier()     { case "$1" in A|A+|B) return 0 ;; *) return 1 ;; esac; }
v_role()     { case "$1" in primary|fallback|second-opinion) return 0 ;; *) return 1 ;; esac; }
v_base()     { printf '%s' "$1" | LC_ALL=C grep -qE '^[A-Za-z0-9][A-Za-z0-9._-]{0,119}$'; }
# Repo-relative, no spaces, no control chars, no `..` component, no leading `/`
# or `-`. Deliberately narrow: a path the judge cannot re-open is not evidence.
v_path()     {
  printf '%s' "$1" | LC_ALL=C grep -qE '^[A-Za-z0-9_.][A-Za-z0-9._/-]{0,239}$' || return 1
  case "$1" in *..*) return 1 ;; esac
  return 0
}

# ------------------------------------------------------------ scope digest
# Reads a caller-supplied list of repo-relative paths (one per line, `#`
# comments and blank lines allowed) and emits the canonical scope manifest:
# `<sha256>  <path>` sorted byte-wise by path. The manifest IS the reviewed
# scope; its own sha256 is what the artifact binds, what the reviewer is told
# in its prompt envelope, and what the judge later re-derives from the working
# tree to detect drift.
build_scope_manifest() {
  local list="$1" out="$2" n=0 p sum
  : > "$out"
  while IFS= read -r p || [ -n "$p" ]; do
    case "$p" in ''|'#'*) continue ;; esac
    v_path "$p" || die "scope list: rejected path (grammar) '$p'"
    [ -e "$p" ] || die "scope list: '$p' does not exist"
    if [ -L "$p" ]; then die "scope list: '$p' is a symlink"; fi
    [ -f "$p" ] || die "scope list: '$p' is not a regular file"
    sum="$(sha_file "$p")"
    printf '%s  %s\n' "$sum" "$p" >> "$out"
    n=$((n + 1))
  done < "$list"
  [ "$n" -gt 0 ] || die "scope list '$list' resolved to 0 files (an empty scope is not a scope)"
  LC_ALL=C sort -k2 "$out" -o "$out"
  chmod 644 "$out"   # `sort -o` writes through a 600 temp; keep evidence readable
  # No duplicate paths: a doubled path would let two different byte states
  # hash to one manifest depending on read order.
  if [ "$(LC_ALL=C awk '{print $2}' "$out" | LC_ALL=C sort | LC_ALL=C uniq -d | wc -l | tr -d ' ')" != "0" ]; then
    die "scope list '$list' contains duplicate paths"
  fi
  printf '%s' "$n"
}

# ---------------------------------------------------------------- sealing
# Appends the self-seal line. The seal is an INTEGRITY seal against accident,
# corruption and truncation — exactly the bar PROCESS §3 sets for rig scripts.
# It is NOT an anti-adversary control: an actor with repo-write access can
# recompute it, and can also edit this script, so no in-script defence against
# that actor is coherent (§3, dispositioned by default).
seal_artifact() {
  local body="$1"
  printf 'artifact-sha256: %s\n' "$(sha_file "$body")" >> "$body"
}

# ---------------------------------------------------------- path reservation
# `set -C` (noclobber) makes creation ATOMIC: a check-then-create race between
# two concurrent launches ends with one of them refusing, not with two writers
# on one path. That two-writer shape is what produced this project's 94%-NUL
# log with a healthy-looking tail.
# Each successful creation is recorded IMMEDIATELY, so a failure on the third
# path still cleans up the first two. (Populating RESERVED after the whole
# loop would leave residue exactly when reservation partially fails.)
reserve() {
  local f
  for f in "$@"; do
    ( set -C; : > "$f" ) 2>/dev/null \
      || die "'$f' already exists or could not be created — refusing to reuse a round path"
    RESERVED="$RESERVED $f"
  done
}

# A bundle reserved but never launched must leave NO residue: the judge treats
# a .log/.prompt/.scope with no .verdict as an incomplete round and refuses the
# whole arc, so a pre-launch refusal must not poison an existing arc.
# AFTER launch this is disarmed — a wedged or killed run's log IS evidence, and
# the arc being un-closable while a round is in flight is the correct answer.
RESERVED=''
LAUNCHED=0
harness_abort() {
  if [ "$LAUNCHED" = "1" ]; then return 0; fi
  local f
  for f in $RESERVED; do rm -f "$f"; done
  RESERVED=''
}

# ============================================================ run subcommand
cmd_run() {
  local arc='' round='' reviewer='' role='' tier='' prompt='' scope='' basis=''
  local reviewer_cmd='' timeout="${MLFK_REVIEW_TIMEOUT:-5400}"
  while [ $# -gt 0 ]; do
    case "$1" in
      --arc)             arc="${2:-}"; shift 2 ;;
      --round)           round="${2:-}"; shift 2 ;;
      --reviewer)        reviewer="${2:-}"; shift 2 ;;
      --role)            role="${2:-}"; shift 2 ;;
      --tier)            tier="${2:-}"; shift 2 ;;
      --prompt)          prompt="${2:-}"; shift 2 ;;
      --scope)           scope="${2:-}"; shift 2 ;;
      --fallback-basis)  basis="${2:-}"; shift 2 ;;
      --reviewer-cmd)    reviewer_cmd="${2:-}"; shift 2 ;;
      --timeout-sec)     timeout="${2:-}"; shift 2 ;;
      *) die "run: unknown argument '$1'" ;;
    esac
  done
  v_arcid "$arc"        || die "run: --arc '$arc' fails the arc-id grammar"
  v_round "$round"      || die "run: --round '$round' must be 1..999"
  reviewer_descriptor "$reviewer" >/dev/null || die "run: unknown --reviewer '$reviewer'"
  v_role "$role"        || die "run: --role must be primary|fallback|second-opinion"
  v_tier "$tier"        || die "run: --tier must be A, A+ or B"
  [ -n "$prompt" ] && [ -f "$prompt" ] && [ ! -L "$prompt" ] || die "run: --prompt must be an existing regular file"
  [ -s "$prompt" ] || die "run: --prompt file is empty"
  [ -n "$scope" ] && [ -f "$scope" ] || die "run: --scope must be an existing file listing the reviewed paths"
  printf '%s' "$timeout" | LC_ALL=C grep -qE '^[1-9][0-9]{0,5}$' || die "run: --timeout-sec must be 1..999999"

  # role/reviewer coherence, enforced at PRODUCE time so an incoherent record
  # can never reach the judge.
  if [ "$role" = "primary" ] && reviewer_is_fallback "$reviewer"; then
    die "run: reviewer '$reviewer' may not hold role 'primary' (PROCESS §11: codex is the primary reviewer)"
  fi
  if [ "$role" = "fallback" ]; then
    reviewer_is_fallback "$reviewer" \
      || die "run: role 'fallback' is for the §11 pair ($REVIEW_FALLBACK_PAIR), not '$reviewer'"
    # PROCESS §11 fallback is only available "when a codex round is PROVEN
    # failed". The proof is a recorded artifact, not an assertion, so the
    # round must name it — and the judge resolves it.
    v_base "$basis" \
      || die "run: role 'fallback' requires --fallback-basis <basename of the failed codex .verdict in this arc>"
    [ -f "$ARCDIR/$arc/$basis" ] \
      || die "run: --fallback-basis '$basis' is not a file in '$ARCDIR/$arc'"
  else
    [ -z "$basis" ] || die "run: --fallback-basis is only meaningful with --role fallback"
    basis='-'
  fi

  local descriptor cmdsha mode
  descriptor="$(reviewer_descriptor "$reviewer")"
  if [ -n "$reviewer_cmd" ]; then
    [ -f "$reviewer_cmd" ] && [ -x "$reviewer_cmd" ] || die "run: --reviewer-cmd must be an executable file"
    # A synthetic reviewer is RECORDED, not hidden: the descriptor sha will
    # not match the built-in, and arc-report.sh refuses such an artifact
    # unless it is run with --synthetic-ok (which only the teeth pass).
    cmdsha="$(sha_str "synthetic:$reviewer:$(sha_file "$reviewer_cmd")")"
    mode=synthetic
  else
    cmdsha="$(sha_str "$descriptor")"
    mode=builtin
  fi

  local rule
  case "$role" in
    primary)        rule='process3-tier-a-go' ;;
    fallback)       rule='process11-fallback-dual-go' ;;
    second-opinion) rule='process3-tier-aplus-second' ;;
  esac

  # The DECISION CHANNEL. codex writes its final message to a file only that
  # message can reach; everything else falls back to the transcript region.
  # The judge requires last-message for codex, so a codex round can never be
  # decided by mid-stream text.
  # A synthetic reviewer keeps its declared id's channel: the teeth must be
  # able to exercise the last-message path, not just the transcript one.
  local dsource
  case "$reviewer" in
    codex) dsource='last-message' ;;
    *)     dsource='transcript' ;;
  esac

  local nonce
  nonce="$(od -An -tx1 -N16 /dev/urandom | LC_ALL=C tr -d ' \n')"
  [ "${#nonce}" -eq 32 ] || die "run: could not draw a 16-byte nonce"

  local dir stamp base log art scopeman promptcopy decfile started ended
  dir="$ARCDIR/$arc"
  mkdir -p "$dir"
  if [ -L "$dir" ]; then die "run: arc directory '$dir' is a symlink"; fi
  # ONE clock reading, used for BOTH the path stamp and `started-utc`.
  # They used to be two separate `date` calls a few hundred milliseconds
  # apart, which meant the artifact's start time and the filename's stamp
  # could legitimately disagree and neither could witness the other. Drawing
  # them from one reading makes `started-utc` RE-DERIVABLE from the path a
  # reader enumerates, so a record cannot be re-labelled in place. NOTE: no
  # mechanical rule derives round ORDER from it any more — the monotonicity
  # rule that did read only `started-utc` and was measured unsound
  # (FORMAT.md §5.3); arc-report.sh prints both timestamps instead.
  started="$(utc)"
  stamp="$(printf '%s' "$started" | LC_ALL=C tr -d ':-')"
  # UNIQUE per round: a UTC-second stamp PLUS 8 nonce hex chars, so two
  # launches in the same second still get distinct paths, and reserved
  # atomically below.
  base="$(printf 'r%03d-%s-%s-%s-%s' "$round" "$reviewer" "$role" "$stamp" "$(printf '%s' "$nonce" | cut -c1-8)")"
  log="$dir/$base.log"
  art="$dir/$base.verdict"
  scopeman="$dir/$base.scope"
  promptcopy="$dir/$base.prompt"
  decfile="$dir/$base.decision"
  # VALIDATE BEFORE RESERVING. A scope error after reservation would leave
  # empty sidecars with no .verdict, and the judge's bundle-completeness rule
  # would then block the whole arc permanently on a refusal that produced no
  # evidence at all. So the manifest is built into a temp file first, and the
  # bundle is only reserved once the round is known to be launchable.
  local scopetmp scope_count
  scopetmp="$(mktemp)"
  scope_count="$(build_scope_manifest "$scope" "$scopetmp")"

  # The trap goes on BEFORE the first creation, so a partial reservation is
  # still cleaned up. Until the reviewer is launched, a refusal must leave NO
  # residue that the judge would read as an incomplete round.
  trap 'harness_abort' EXIT
  if [ "$dsource" = 'last-message' ]; then
    reserve "$log" "$scopeman" "$promptcopy" "$decfile"
  else
    reserve "$log" "$scopeman" "$promptcopy"
  fi
  if [ -e "$art" ]; then die "run: '$art' already exists"; fi
  cat "$scopetmp" > "$scopeman"
  rm -f "$scopetmp"

  # The reviewer runs on the ARCHIVED prompt envelope, not on the caller's
  # mutable file: the artifact must say what was actually reviewed. The
  # envelope carries the canonical scope manifest and its digest, so the
  # prompt the reviewer saw and the scope the artifact binds cannot drift
  # apart.
  {
    cat "$prompt"
    printf '\n\n---\n\n'
    printf '## REVIEWED SCOPE (harness-generated — this is the binding scope)\n\n'
    # DELIBERATELY reviewer/role-free: PROCESS §11 requires the two fallback
    # reviewers to see the SAME prompt, and the judge enforces that by
    # comparing prompt-sha256 across the closing round. Stamping the reviewer
    # id into the envelope would make that comparison impossible by
    # construction.
    printf 'arc-id: %s · round: %s · tier: %s\n\n' "$arc" "$round" "$tier"
    printf 'These %s files, at exactly these sha256 values, are the reviewed\n' "$scope_count"
    printf 'bytes. The harness records this manifest in its verdict artifact,\n'
    printf 'and the checker re-hashes every path against it. Arc closure is a\n'
    printf 'driver/human judgement informed by that evidence, not a tool call.\n\n'
    printf '```\n'
    cat "$scopeman"
    printf '```\n\n'
    # LAST LINE of the envelope, by construction — the judge checks it
    # positionally so a prompt that merely quotes an old scope block cannot
    # satisfy the check.
    printf 'scope-sha256: %s\n' "$(sha_file "$scopeman")"
  } > "$promptcopy"

  printf 'harness: arc=%s round=%s reviewer=%s role=%s tier=%s\n' \
    "$arc" "$round" "$reviewer" "$role" "$tier" >&2
  printf 'harness: log=%s decision=%s\n' "$log" "$dsource" >&2

  set +e
  if [ "$mode" = synthetic ]; then
    # argv mirrors what the built-ins get: the archived prompt, and (when the
    # reviewer id uses one) the decision-channel path to write.
    "$reviewer_cmd" "$promptcopy" "$decfile" > "$log" 2>&1 < /dev/null &
  else
    case "$reviewer" in
      codex) codex exec --sandbox read-only --output-last-message "$decfile" - < "$promptcopy" > "$log" 2>&1 & ;;
      grok)  grok --prompt-file "$promptcopy" --permission-mode plan > "$log" 2>&1 < /dev/null & ;;
      # stdin, never "$(cat ...)": command substitution strips trailing
      # newlines and cannot carry NULs, so two reviewers with equal
      # prompt-sha256 would not in fact have seen equal bytes.
      opus5) claude --model claude-opus-5 --permission-mode plan -p < "$promptcopy" > "$log" 2>&1 & ;;
    esac
  fi
  local pid rc timed_out hkill
  LAUNCHED=1
  pid=$!
  timed_out=0
  # WHO KILLED THE REVIEWER is recorded, not inferred. `harness-timeout` means
  # THIS loop signalled it after `timeout-sec`; `none` means the process ended
  # on its own — and a `none` round that nonetheless died of a signal was
  # killed by something OUTSIDE the harness. RECORDING that distinction is
  # the point; no rule decides on it any more (FORMAT.md §5.3 — the
  # eligibility rule that did covered 2 of 8 VOID shapes and was
  # manufacturable in zero seconds). arc-report.sh prints it.
  hkill='none'
  SECONDS=0
  while kill -0 "$pid" 2>/dev/null; do
    if [ "$SECONDS" -ge "$timeout" ]; then
      timed_out=1
      hkill='harness-timeout'
      kill -TERM "$pid" 2>/dev/null
      sleep 5
      kill -KILL "$pid" 2>/dev/null
      break
    fi
    sleep 0.25
  done
  wait "$pid"
  rc=$?
  set -e
  ended="$(utc)"
  [ -f "$log" ] || die "run: reviewer produced no log at '$log'"

  # ---- terminator. Written by the harness, nonce-bearing so that neither a
  # reviewer echoing a prompt nor a pasted transcript can forge one.
  if [ -s "$log" ] && [ "$(tail -c 1 "$log" | wc -l | tr -d ' ')" = "0" ]; then
    printf '\n' >> "$log"
  fi
  printf 'REVIEWER_RC %s %d\nHARNESS-EOR %s\n' "$nonce" "$rc" "$nonce" >> "$log"

  # ---- measure the log and the decision channel (facts only; the judgement
  # they inform is the driver's, and arc-report.sh only reports them back)
  local nulc bytes lines rc_count eor_count rc_line gos nogos verdict vline void
  local tv dv dnul decpath decsha
  nulc="$(LC_ALL=C tr -dc '\000' < "$log" | wc -c | tr -d ' ')"
  bytes="$(wc -c < "$log" | tr -d ' ')"
  lines="$(wc -l < "$log" | tr -d ' ')"
  # Count only NONCE-MATCHING terminators. A fixed token would be collidable
  # by any reviewer that quotes this project's own documentation — measured:
  # round 5 of this item's own arc voided because the reviewer echoed the
  # literal line `HARNESS-EOR nonce` out of FORMAT.md's code block. The nonce
  # is what the reviewer cannot know, so it is what the count must key on.
  rc_count="$(grepc "^REVIEWER_RC $nonce " "$log")"
  eor_count="$(grepc "^HARNESS-EOR $nonce\$" "$log")"

  void='-'
  verdict='VOID'
  vline=0
  rc_line=0
  gos=0
  nogos=0
  if [ "$timed_out" = "1" ]; then
    void='timeout'
  elif [ "$rc" != "0" ]; then
    # A failed reviewer PROCESS is a failed round, whatever it printed — and
    # it must be VOID rather than a non-VOID artifact, so that it is merely
    # excluded from closure instead of poisoning the arc, and so that it can
    # serve as §11's recorded failed-codex basis.
    void='reviewer-failed'
  elif [ "$nulc" != "0" ]; then
    void='nul-bytes'
  elif [ "$rc_count" != "1" ] || [ "$eor_count" != "1" ]; then
    # >1 means the reviewer's own output carried a terminator-shaped line
    # (a prompt quoting a previous harness log will do this). Loud VOID and
    # re-run; never an interpreted round.
    void='duplicate-terminator'
  else
    rc_line="$(LC_ALL=C grep -a -n "^REVIEWER_RC $nonce " "$log" | cut -d: -f1)"
    gos="$(grepc '^VERDICT: GO$' "$log")"
    nogos="$(grepc '^VERDICT: NO-GO$' "$log")"
    tv="$(terminal_verdict "$log" $((rc_line - 1)))"
    if [ -z "$tv" ]; then
      void='no-verdict'
      rc_line=0
    else
      vline="${tv%%:*}"
      verdict="${tv#*:}"
    fi
  fi
  # the in-region counts are what the artifact reports
  if [ "$verdict" != 'VOID' ]; then
    local region
    region="$(mktemp)"
    head -n $((rc_line - 1)) "$log" > "$region"
    gos="$(grepc '^VERDICT: GO$' "$region")"
    nogos="$(grepc '^VERDICT: NO-GO$' "$region")"
    rm -f "$region"
  else
    gos=0
    nogos=0
  fi

  # ---- the decision channel decides. The transcript must agree with it.
  #
  # The decision file is NEVER repaired or rewritten: it is the reviewer's
  # bytes, and "fixing" a truncated write could turn a partial `VERDICT: G`
  # into a valid line. The rule is instead that its LAST line — including a
  # final line with no trailing newline — must be exactly the anchored
  # verdict. That admits no trailing content and catches truncation, without
  # the harness touching a single byte.
  if [ "$dsource" = 'last-message' ]; then
    decpath="$decfile"
    if [ "$verdict" != 'VOID' ]; then
      if [ ! -s "$decfile" ]; then
        verdict='VOID'; void='decision-empty'; vline=0; rc_line=0; gos=0; nogos=0
      else
        dnul="$(LC_ALL=C tr -dc '\000' < "$decfile" | wc -c | tr -d ' ')"
        dv="$(tail -1 "$decfile")"
        if [ "$dnul" != "0" ]; then
          verdict='VOID'; void='nul-bytes'; vline=0; rc_line=0; gos=0; nogos=0
        elif [ "$dv" != 'VERDICT: GO' ] && [ "$dv" != 'VERDICT: NO-GO' ]; then
          verdict='VOID'; void='decision-malformed'; vline=0; rc_line=0; gos=0; nogos=0
        elif [ "$verdict" != "${dv#VERDICT: }" ]; then
          # the transcript and the final message disagree: exactly the shape a
          # foreign or tool-emitted verdict produces. Void, never interpreted.
          verdict='VOID'; void='decision-mismatch'; vline=0; rc_line=0; gos=0; nogos=0
        fi
      fi
    fi
    decsha="$(sha_file "$decfile")"
  else
    decpath="$log"
    decsha="$(sha_file "$log")"
    # ------------------------------------------------------------------
    # AMBIGUOUS VERDICT (failure mode #1, closed on the TRANSCRIPT path).
    #
    # Defence (a), the decision channel, exists only for codex. On the §11
    # fallback path and the Tier A+ second-opinion path — which are BY
    # DEFINITION never codex — the round is decided by the transcript's LAST
    # anchored verdict, and defence (b), the nonce terminator, closes only a
    # paste AFTER the run. A reviewer that emits its real NO-GO and then
    # QUOTES a foreign log whose last line is an anchored GO is inside the
    # region, before the terminator, and hashes correctly: it decided GO.
    # MEASURED 2026-07-31 (Tier A+ review, H2) — it closed an arc.
    #
    # The tell was already being measured and written down, and then never
    # judged: `anchored-go` AND `anchored-nogo` both non-zero. A transcript
    # carrying BOTH verdicts does not have "a terminal verdict"; it has two
    # candidates and a positional tie-break. That is not a decision, so the
    # round is VOID and gets re-run — loudly, never interpreted.
    #
    # Deliberately NOT applied to `last-message` rounds: a codex round whose
    # PROSE quotes an opposing verdict is decided by the channel no quoted
    # text can reach, and 2 of the 7 real codex rounds of this item's own arc
    # carry exactly that shape while being honest NO-GOs. Voiding those would
    # discard real reviews to close a hole that is already closed for them.
    if [ "$verdict" != 'VOID' ] && [ "$gos" != "0" ] && [ "$nogos" != "0" ]; then
      verdict='VOID'; void='ambiguous-verdict'; vline=0; rc_line=0; gos=0; nogos=0
    fi
  fi

  # ---- write the artifact atomically: a poller must never see a partial one
  local tmp="$art.tmp.$$"
  {
    # RVERDICT2, not 1: the field list changed (timeout-sec, harness-kill),
    # and a format whose grammar moves under an unchanged magic cannot read
    # its own history — the judge reported the skew as truncation. THE RULE:
    # any change to the field list BUMPS THE MAGIC, in the same edit.
    printf 'RVERDICT2\n'
    printf 'arc-id: %s\n'              "$arc"
    printf 'round: %s\n'               "$round"
    printf 'reviewer: %s\n'            "$reviewer"
    printf 'role: %s\n'                "$role"
    printf 'tier: %s\n'                "$tier"
    printf 'closure-rule: %s\n'        "$rule"
    printf 'fallback-basis: %s\n'      "$basis"
    printf 'reviewer-cmd-sha256: %s\n' "$cmdsha"
    printf 'scope-count: %s\n'         "$scope_count"
    printf 'scope-sha256: %s\n'        "$(sha_file "$scopeman")"
    printf 'scope-manifest: %s\n'      "$scopeman"
    printf 'prompt-sha256: %s\n'       "$(sha_file "$promptcopy")"
    printf 'prompt-path: %s\n'         "$promptcopy"
    printf 'log-path: %s\n'            "$log"
    printf 'log-sha256: %s\n'          "$(sha_file "$log")"
    printf 'log-bytes: %s\n'           "$bytes"
    printf 'log-lines: %s\n'           "$lines"
    printf 'log-nul-count: %s\n'       "$nulc"
    printf 'decision-source: %s\n'     "$dsource"
    printf 'decision-path: %s\n'       "$decpath"
    printf 'decision-sha256: %s\n'     "$decsha"
    printf 'eor-nonce: %s\n'           "$nonce"
    printf 'reviewer-rc: %s\n'         "$rc"
    printf 'rc-line: %s\n'             "$rc_line"
    printf 'verdict: %s\n'             "$verdict"
    printf 'verdict-line: %s\n'        "$vline"
    printf 'anchored-go: %s\n'         "$gos"
    printf 'anchored-nogo: %s\n'       "$nogos"
    printf 'void-reason: %s\n'         "$void"
    # The DEADLINE and WHO KILLED IT. `--timeout-sec` used to be a caller knob
    # recorded nowhere, while `void-reason: timeout` was TRUSTED by the judge:
    # one flag manufactured a "proven-failed" codex round that then licensed
    # §11's more permissive two-reviewer path (H3, measured 2026-07-31).
    printf 'timeout-sec: %s\n'         "$timeout"
    printf 'harness-kill: %s\n'        "$hkill"
    printf 'harness-sha256: %s\n'      "$(sha_file "$HERE/review-harness.sh")"
    printf 'started-utc: %s\n'         "$started"
    printf 'ended-utc: %s\n'           "$ended"
    printf 'END RVERDICT2\n'
  } > "$tmp"
  seal_artifact "$tmp"
  mv "$tmp" "$art"

  printf 'HARNESS ROUND WRITTEN %s round=%s reviewer=%s verdict=%s rc=%s artifact=%s\n' \
    "$arc" "$round" "$reviewer" "$verdict" "$rc" "$art"
  worktree_notice "$dir"
}

# ============================================================ cap subcommand
cmd_cap() {
  local arc='' tier='' rounds='' scope='' authby='' class=''
  while [ $# -gt 0 ]; do
    case "$1" in
      --arc)           arc="${2:-}"; shift 2 ;;
      --tier)          tier="${2:-}"; shift 2 ;;
      --rounds)        rounds="${2:-}"; shift 2 ;;
      --scope)         scope="${2:-}"; shift 2 ;;
      --authorized-by) authby="${2:-}"; shift 2 ;;
      --class)         class="${2:-}"; shift 2 ;;
      *) die "cap: unknown argument '$1'" ;;
    esac
  done
  v_arcid "$arc"   || die "cap: --arc '$arc' fails the arc-id grammar"
  v_tier "$tier"   || die "cap: --tier must be A, A+ or B"
  v_round "$rounds" || die "cap: --rounds must be 1..999"
  [ -n "$scope" ] && [ -f "$scope" ] || die "cap: --scope must be an existing file listing the reviewed paths"
  printf '%s' "$authby" | LC_ALL=C grep -qE '^[a-z0-9][a-z0-9.-]{2,63}$' \
    || die "cap: --authorized-by must be a token like owner-ruling-2026-07-30"
  # PROCESS §3: a cap must NAME the recurring objection class. An unnamed cap
  # is the thing the rule exists to prevent, so an empty/short one is refused.
  # NOTE: the length bound is a shell test, not a regex repetition count —
  # BSD grep rejects {m,n} with n > 255 ("invalid repetition count(s)"), and a
  # grammar that only works on GNU grep is a grammar that fails open on macOS.
  printf '%s' "$class" | LC_ALL=C grep -qE '^[ -~]+$' \
    || die "cap: --class must be one line of printable ASCII"
  [ "${#class}" -ge 24 ] && [ "${#class}" -le 400 ] \
    || die "cap: --class must name the recurring objection class (24..400 chars, measured ${#class})"

  local dir stamp base art scopeman
  dir="$ARCDIR/$arc"
  mkdir -p "$dir"
  if [ -L "$dir" ]; then die "cap: arc directory '$dir' is a symlink"; fi
  stamp="$(date -u +%Y%m%dT%H%M%SZ)"
  base="cap-$stamp-$(od -An -tx1 -N4 /dev/urandom | LC_ALL=C tr -d ' \n')"
  art="$dir/$base.cap"
  scopeman="$dir/$base.scope"
  # validate BEFORE reserving (same rule as cmd_run: a refusal leaves no
  # residue that the judge would read as an incomplete bundle)
  local scopetmp scope_count
  scopetmp="$(mktemp)"
  scope_count="$(build_scope_manifest "$scope" "$scopetmp")"
  trap 'harness_abort' EXIT
  reserve "$scopeman"
  if [ -e "$art" ]; then die "cap: '$art' already exists"; fi
  cat "$scopetmp" > "$scopeman"
  rm -f "$scopetmp"

  local tmp="$art.tmp.$$"
  {
    printf 'RVCAP1\n'
    printf 'arc-id: %s\n'          "$arc"
    printf 'tier: %s\n'            "$tier"
    printf 'closure-rule: %s\n'    'process3-capped'
    printf 'rounds-completed: %s\n' "$rounds"
    printf 'authorized-by: %s\n'   "$authby"
    printf 'recurring-class: %s\n' "$class"
    printf 'scope-count: %s\n'     "$scope_count"
    printf 'scope-sha256: %s\n'    "$(sha_file "$scopeman")"
    printf 'scope-manifest: %s\n'  "$scopeman"
    printf 'harness-sha256: %s\n'  "$(sha_file "$HERE/review-harness.sh")"
    printf 'written-utc: %s\n'     "$(utc)"
    printf 'END RVCAP1\n'
  } > "$tmp"
  seal_artifact "$tmp"
  mv "$tmp" "$art"
  LAUNCHED=1   # the cap bundle is complete; nothing to clean up
  printf 'HARNESS CAP WRITTEN %s rounds=%s artifact=%s\n' "$arc" "$rounds" "$art"
  worktree_notice "$dir"
}

# ===================================================== regression subcommand
# PROCESS §3's Tier A+ tier-up is TWO obligations: an independent second
# reviewer AND a byte-identity regression on archived results. The second was
# previously unrepresented, which meant the judge could call an A+ arc closed
# with half the tier-up discharged. It is representable after all — not by
# trusting an assertion, but by the harness RUNNING the lane's own regression
# command and sealing its rc and output, exactly as it does for a review
# round. The judge then requires a passing regression bound to the closing
# round's scope before any Tier A+ closure.
#
#   bash port/review/review-harness.sh regression \
#     --arc <id> --tier A+ --scope <file> --cmd <executable>
cmd_regression() {
  local arc='' tier='' scope='' cmd=''
  while [ $# -gt 0 ]; do
    case "$1" in
      --arc)   arc="${2:-}"; shift 2 ;;
      --tier)  tier="${2:-}"; shift 2 ;;
      --scope) scope="${2:-}"; shift 2 ;;
      --cmd)   cmd="${2:-}"; shift 2 ;;
      *) die "regression: unknown argument '$1'" ;;
    esac
  done
  v_arcid "$arc" || die "regression: --arc '$arc' fails the arc-id grammar"
  # The regression record exists to discharge the Tier A+ tier-up and nothing
  # else. Producing one for a Tier A or B arc would leave an artifact no
  # closure path reads — and the judge refuses any it finds outside A+, so
  # such a record could only ever be dead weight or confusion.
  [ "$tier" = 'A+' ] || die "regression: --tier must be A+ (the byte-identity regression is the Tier A+ tier-up)"
  [ -n "$scope" ] && [ -f "$scope" ] || die "regression: --scope must be an existing file listing the reviewed paths"
  [ -n "$cmd" ] && [ -f "$cmd" ] && [ -x "$cmd" ] || die "regression: --cmd must be an executable file"
  if [ -L "$cmd" ]; then die "regression: --cmd must not be a symlink"; fi

  local dir stamp base log art scopeman cmdcopy nonce
  dir="$ARCDIR/$arc"
  mkdir -p "$dir"
  if [ -L "$dir" ]; then die "regression: arc directory '$dir' is a symlink"; fi
  nonce="$(od -An -tx1 -N16 /dev/urandom | LC_ALL=C tr -d ' \n')"
  [ "${#nonce}" -eq 32 ] || die "regression: could not draw a 16-byte nonce"
  stamp="$(date -u +%Y%m%dT%H%M%SZ)"
  base="reg-$stamp-$(printf '%s' "$nonce" | cut -c1-8)"
  log="$dir/$base.log"
  art="$dir/$base.reg"
  scopeman="$dir/$base.scope"
  cmdcopy="$dir/$base.cmd"

  local scopetmp scope_count
  scopetmp="$(mktemp)"
  scope_count="$(build_scope_manifest "$scope" "$scopetmp")"
  if [ -e "$art" ]; then die "regression: '$art' already exists"; fi
  trap 'harness_abort' EXIT
  reserve "$log" "$scopeman" "$cmdcopy"
  cat "$scopetmp" > "$scopeman"
  rm -f "$scopetmp"
  # ARCHIVE the command bytes. `cmd-sha256` is only meaningful if the judge
  # can re-hash what actually ran; a bare hash of a file that has since moved
  # or changed is a claim, not evidence.
  cat "$cmd" > "$cmdcopy"
  chmod +x "$cmdcopy"

  local rc
  set +e
  LAUNCHED=1
  # Run the ARCHIVED copy, not the caller's mutable path: certifying bytes
  # that were copied but never executed would make cmd-sha256 a lie whenever
  # the original changed between the copy and the run.
  "$cmdcopy" > "$log" 2>&1 < /dev/null
  rc=$?
  set -e
  if [ -s "$log" ] && [ "$(tail -c 1 "$log" | wc -l | tr -d ' ')" = "0" ]; then
    printf '\n' >> "$log"
  fi
  printf 'REVIEWER_RC %s %d\nHARNESS-EOR %s\n' "$nonce" "$rc" "$nonce" >> "$log"

  local tmp="$art.tmp.$$"
  {
    printf 'RVREG1\n'
    printf 'arc-id: %s\n'         "$arc"
    printf 'tier: %s\n'           "$tier"
    printf 'scope-count: %s\n'    "$scope_count"
    printf 'scope-sha256: %s\n'   "$(sha_file "$scopeman")"
    printf 'scope-manifest: %s\n' "$scopeman"
    printf 'cmd-sha256: %s\n'     "$(sha_file "$cmdcopy")"
    printf 'cmd-path: %s\n'       "$cmdcopy"
    printf 'log-path: %s\n'       "$log"
    printf 'log-sha256: %s\n'     "$(sha_file "$log")"
    printf 'log-bytes: %s\n'      "$(wc -c < "$log" | tr -d ' ')"
    printf 'log-lines: %s\n'      "$(wc -l < "$log" | tr -d ' ')"
    printf 'log-nul-count: %s\n'  "$(LC_ALL=C tr -dc '\000' < "$log" | wc -c | tr -d ' ')"
    printf 'eor-nonce: %s\n'      "$nonce"
    printf 'rc: %s\n'             "$rc"
    printf 'harness-sha256: %s\n' "$(sha_file "$HERE/review-harness.sh")"
    printf 'run-utc: %s\n'        "$(utc)"
    printf 'END RVREG1\n'
  } > "$tmp"
  seal_artifact "$tmp"
  mv "$tmp" "$art"
  printf 'HARNESS REGRESSION WRITTEN %s rc=%s artifact=%s\n' "$arc" "$rc" "$art"
  worktree_notice "$dir"
}

# ===================================================================== main
[ $# -ge 1 ] || die "usage: review-harness.sh {run|cap|regression} [...]"
sub="$1"; shift
case "$sub" in
  run)        cmd_run "$@" ;;
  cap)        cmd_cap "$@" ;;
  regression) cmd_regression "$@" ;;
  *)          die "unknown subcommand '$sub' (expected run, cap or regression)" ;;
esac
