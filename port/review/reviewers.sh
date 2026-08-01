#!/usr/bin/env bash
# port/review/reviewers.sh — the ONE reviewer table, sourced by BOTH the
# producer (review-harness.sh) and the diagnostic reader (arc-report.sh).
#
# Why one file: the reader refuses any artifact whose recorded
# `reviewer-cmd-sha256` is not the sha256 of the descriptor string for its
# declared reviewer id. If producer and reader carried separate copies of the
# table, a drift between them would either false-reject every genuine arc or
# silently accept a synthetic one. One table, one sha, no drift.
#
# The descriptor is a FIXED STRING, not the resolved binary: reviewer CLIs
# self-update constantly and hashing their bytes would invalidate every
# archived artifact on every reviewer upgrade. What the descriptor pins is
# "this round was run by the built-in <id> invocation", which is exactly the
# property the judge needs to distinguish a real round from a synthetic one.

# reviewer_descriptor <reviewer-id>  -> prints descriptor, rc 0
#                                    -> rc 1 for an unknown id
# These strings must be kept BYTE-FOR-BYTE in step with the invocations in
# review-harness.sh cmd_run — including security-critical flags such as
# codex's --output-last-message, which is what makes the decision channel a
# channel. A descriptor that omits a flag is a descriptor that certifies the
# wrong invocation.
reviewer_descriptor() {
  case "$1" in
    codex) printf 'builtin:codex:codex exec --sandbox read-only --output-last-message <DECISION> - <PROMPT' ;;
    grok)  printf 'builtin:grok:grok --prompt-file <PROMPT> --permission-mode plan' ;;
    opus5) printf 'builtin:opus5:claude --model claude-opus-5 --permission-mode plan -p <PROMPT' ;;
    *)     return 1 ;;
  esac
}

# reviewer_is_fallback <reviewer-id> -> rc 0 if this id may only appear in a
# PROCESS §11 codex-failure fallback pair (or as a Tier A+ second opinion).
reviewer_is_fallback() {
  case "$1" in
    grok|opus5) return 0 ;;
    *)          return 1 ;;
  esac
}

# The two reviewer ids that PROCESS §11's codex-failure fallback requires
# TOGETHER. Both must end GO for the round to count as GO.
REVIEW_FALLBACK_PAIR="grok opus5"

# ---------------------------------------------------------------------------
# §11 FALLBACK-BASIS ELIGIBILITY: THERE IS NO LONGER A MECHANICAL RULE HERE,
# DELIBERATELY (owner ruling 2026-07-31).
#
# A rule lived here from 2026-07-31 morning: a VOID codex round was INELIGIBLE
# as a §11 basis if `void-reason: timeout` with `timeout-sec < 1800`, or if it
# exited in the signal range 129..192 with `harness-kill: none`. It closed the
# two shapes that had been measured (`--timeout-sec 1`; killing the adverse
# in-flight reviewer) and left the class: it was a TWO-ITEM BLACKLIST over
# EIGHT VOID reasons, and the second adversarial review manufactured two fresh
# eligible bases in zero seconds with no repo write —
#   (a) run the harness with codex off PATH: rc 127, `reviewer-failed`;
#   (b) one sentence of prompt ("after the verdict, append a summary"): rc 0,
#       `decision-malformed`, because the decision channel requires the
#       verdict to be the final message's LAST line while grok and opus5 are
#       decided by the last ANCHORED line and are unaffected by the same
#       instruction.
# Both then licensed the more permissive two-reviewer path.
#
# Whether a codex round is PROVEN failed under PROCESS §11 is therefore a
# driver/human judgement. `port/review/arc-report.sh` prints the evidence for
# it — void-reason, reviewer-rc, timeout-sec, harness-kill, started/ended —
# and names the question as open. Do not re-add a blacklist: the measured
# lesson (three passes, HARD RULE 8) is that this surface admits new shapes
# faster than point fixes close them.
#
# The parameters themselves are still RECORDED by the producer in every
# artifact, which is the part that was worth keeping.
