#!/usr/bin/env python3
"""Derive port/sim/device/m4-closure-ledger.txt from the freeze manifest.

WHY THIS EXISTS (fix_plan C25/C26/C27). The freeze manifest's `cite` column
is free text. verify_m4.sh's cite check (iter 134) proves a cited log has a
TERMINAL anchored `VERDICT: GO`, which closes the adorned/non-terminal class
— but NOT that the GO is about THAT producer, NOT that the GO is
semantically live rather than pasted, and NOT that a non-arc cite ASSERTS
its gate rather than denying it. This tool migrates the free-text corpus to
a structured closure record that a machine can verify by identity instead of
by prose.

DERIVATION ONLY — NEVER INVENTION. Every field is read off the existing
corpus (the manifest row, the cited `.loop/` artifacts, docs/AGENT-LOG.md).
A row whose fields cannot ALL be derived is NOT emitted: it is printed under
`CANNOT DERIVE` for a driver ruling. Guessing here would launder exactly the
approval this item exists to stop laundering.

Usage:
  python3 port/sim/device/gen-closure-ledger.py [--cap <path>=<YYYY-MM-DD>]...
  python3 port/sim/device/gen-closure-ledger.py --write [--cap ...]

A `--cap` names a producer the DRIVER has capped per PROCESS §3 (bounded
convergence) rather than closed at a VERDICT: GO. It is an explicit assertion
on the command line precisely so that no cite prose can ever produce one; the
AGENT-LOG binding is still verified independently before the record is
emitted. Live corpus: `--cap port/sim/device/adbsh.sh=2026-07-17`.
"""
import hashlib
import os
import re
import stat
import subprocess
import sys
import tempfile

MANIFEST = "port/sim/device/m4-freeze-manifest.txt"
LEDGER = "port/sim/device/m4-closure-ledger.txt"
# r8 [L]: the installed mode of the derived ledger, stated once. See the
# os.chmod call at the atomic install for why this is explicit and not
# inherited from mkstemp (0600) or from whatever is already on disk.
LEDGER_MODE = 0o644
AGENT_LOG = "docs/AGENT-LOG.md"

# Manifest record grammar, anchored exactly as verify_m4.sh parses it.
# EXACT character class, identical to check-cite-closure.sh (r3 [M]):
# `\S`/`[^ ]` accepted TAB and CR, so a tab-separated fifth field or a
# trailing CR could pass one parser and not the other.
ROW = re.compile(r"^([0-9a-f]{64}) ([^ \t\r]+) ([^ \t\r]+) ([^ \t\r]+)\Z")
# Cite artifact grammar, ported verbatim from verify_m4.sh cite_refs().
REF = re.compile(r"\.loop/[A-Za-z0-9._{},-]+\.(?:log|txt)")
# MEASURED over all 21 cited terminal-GO logs (2026-07-29): the only thing
# that ever follows a terminal GO is a single reviewer exit-code marker.
# MEASURED over all 18 pinned closure logs: every RC marker present is `=0`
# (CODEX_RC x4, GROK_RC, CODEX3_RC, OPUS_RC; 11 logs carry none). A NONZERO
# RC means the reviewer PROCESS failed, and a failed transcript that happens
# to end in a foreign approval line is the C26 shape. Zero false rejections.
RC_MARK = re.compile(r"^[A-Z][A-Z0-9]*_RC=0$")

CLOSED_ARC = ("reviewed-go",)
# STATUS -> gate token. Derived from the manifest's ANCHORED status field,
# never from cite prose (r2 [H]): substring-matching the cite for a gate name
# converts `...NOT-proven-by-SIM-CONFORMS...` into an affirmative ledger field,
# which is C27 laundered one file further along instead of closed. The status
# token IS the machine-readable gate claim; the prose added nothing. The
# checker recomputes this same mapping and refuses any disagreement, so a
# hand-edited token cannot survive either.
CLOSED_GATE = {
    "oracle-frozen": "PROVEN-BY-HARD-RULE-3",
    "grandfathered-m1": "PROVEN-BY-M1-EXIT-GATE",
    "grandfathered-m2": "PROVEN-BY-M2-EXIT-GATE",
}
OPEN = ("arc-in-flight", "arc-pending")

# RC-marker prefix -> reviewer identity (PROCESS §4 identity pins, §11 roster).
RC_REVIEWER = {"CODEX": "codex", "CODEX3": "codex", "GROK": "grok",
               "OPUS": "opus5"}


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def safe_path(p, what):
    """r6 [H]: the CHECKER refuses symlinked evidence, symlinked parents, and
    `.`/`..` components (its dir_ok); the GENERATOR followed all of them. Two
    consequences, both real: it could DERIVE a ledger from bytes that live
    outside this worktree, and `--write` could truncate whatever a symlinked
    ledger pointed at. A generator that can emit a ledger its own checker
    refuses is a broken tool, so the rule is reproduced here EXACTLY — every
    component, plus the final one."""
    if p == "" or p.startswith("/"):
        raise SystemExit("REFUSING: %s '%s' is not a repo-relative path" % (what, p))
    parts = p.split("/")
    for i, c in enumerate(parts):
        if c in ("", ".", ".."):
            raise SystemExit(
                "REFUSING: %s '%s' contains a '%s' component — evidence paths "
                "must be plain repo-relative paths that cannot escape the "
                "worktree" % (what, p, c))
        acc = "/".join(parts[:i + 1])
        if os.path.islink(acc):
            raise SystemExit(
                "REFUSING: path component '%s' of %s '%s' is a SYMLINK — "
                "evidence must live in this worktree, not wherever a link "
                "points" % (acc, what, p))
        if i < len(parts) - 1 and not os.path.isdir(acc):
            raise SystemExit(
                "REFUSING: path component '%s' of %s '%s' is not a directory"
                % (acc, what, p))
    return p


def read_bytes_lines(path, what):
    """r6 [M]: Python TEXT mode performs universal-newline translation, so a
    CRLF or bare-CR line separator vanished here while the checker's awk
    refused those bytes — the two parsers disagreed on the very corruption
    class round 3 hardened. Read BYTES and split on LF only, exactly as awk
    does.

    r8 [M]: the decode was `errors="replace"`, which SILENTLY rewrote every
    invalid byte to U+FFFD. That is the same two-parser divergence one layer
    down: this tool would derive a row from the NORMALIZED text and write it
    into the ledger, while the checker's awk scans the ORIGINAL bytes and can
    never match the row it is asked to verify — `gen --write` succeeds and
    `check` then refuses its own output. Corrupt input is corruption, not a
    character to be invented: decode STRICTLY and refuse, so both layers reach
    the same verdict on the same bytes."""
    safe_path(path, what)
    with open(path, "rb") as f:
        raw = f.read()
    try:
        text = raw.decode("utf-8")
    except UnicodeDecodeError as e:
        # Report the offending byte offset and the line it falls on, so a real
        # corruption is diagnosable without a hex dump.
        lineno = raw.count(b"\n", 0, e.start) + 1
        raise SystemExit(
            "REFUSING: %s (%s) is not valid UTF-8 — byte offset %d (line %d): "
            "%s\n  The checker scans the RAW bytes; normalizing them here "
            "would derive records it can never match."
            % (path, what, e.start, lineno, e.reason))
    return text.split("\n")


def manifest_rows():
    """WHOLE-FILE whitelist grammar (r2 [M]): every line must be a comment, a
    blank, or a well-formed record. Collecting only the matching lines let an
    indented / CR-tainted / truncated row vanish silently, taking its closure
    obligation with it. Measured over the live manifest: 0 non-conforming
    lines, so this refuses nothing genuine."""
    rows = []
    for n, line in enumerate(read_bytes_lines(MANIFEST, "manifest"), 1):
        # BLANK means the checker's blank: only spaces and tabs. `strip()`
        # accepts every Unicode whitespace character (r4 [M]), so the
        # generator could write a ledger from a manifest the checker
        # considers corrupt.
        if line.startswith("#") or line.strip(" \t") == "":
            continue
        m = ROW.match(line)
        if not m:
            raise SystemExit(
                "REFUSING: %s:%d is neither a comment, a blank, nor a "
                "well-formed record:\n  %r" % (MANIFEST, n, line))
        rows.append(m.groups())
    seen = {}
    for _, path, _, _ in rows:
        seen[path] = seen.get(path, 0) + 1
    dupes = sorted(k for k, v in seen.items() if v != 1)
    if dupes:
        raise SystemExit("REFUSING: duplicate manifest row(s): %s" % dupes)
    return rows


def cite_refs(cite):
    """Boundary-anchored artifact extraction — verify_m4.sh cite_refs().

    r8 [M]: the brace-grammar refusal used to live ONLY in cite_expand(), which
    never sees a reference the boundary test drops. The checker's `brbad`
    pre-pass has no such filter: it validates the brace group of EVERY
    ref-shaped occurrence in the cite, boundary or not. So a malformed group
    sitting at a bad boundary was silently skipped HERE and refused THERE —
    `gen --write` installed a ledger that its own checker rejects. The two
    layers must reach the same verdict on the same bytes, so validate every
    occurrence FIRST, in the checker's order, and only then apply the boundary
    filter that decides which references are actually cited.
    """
    out, s = [], cite
    while True:
        m = REF.search(s)
        if not m:
            break
        ref, rest = m.group(0), s[m.end():]
        # Validate unconditionally — mirrors the checker's brbad pre-pass,
        # which runs before any boundary test. Raises on a malformed group.
        check_brace_grammar(ref)
        if rest[:1] in ("", "+", "-"):
            out.append(ref)
        s = rest
    return out


def check_brace_grammar(ref):
    """The brace-group grammar, shared by cite_refs() and cite_expand() so the
    validation point and the expansion point can never drift apart again."""
    if "{" not in ref and "}" not in ref:
        return
    if ref.count("{") != 1 or ref.count("}") != 1 or ref.index("{") > ref.index("}"):
        raise SystemExit(
            "REFUSING: cite reference %r has a malformed brace group — exactly "
            "one '{...}' group is representable" % ref)
    alts = ref.split("{", 1)[1].split("}", 1)[0]
    if not alts or any(a == "" for a in alts.split(",")):
        raise SystemExit(
            "REFUSING: cite reference %r has an empty brace alternative" % ref)


def cite_expand(ref):
    """r6 [M]: this and the checker's awk `emit()` disagreed on MALFORMED brace
    groups, and neither refused. Measured divergences: an EMPTY group
    (`.loop/a{}.log`) expands to one alternative here and to ZERO there, so the
    generator could select an artifact the checker's membership set can never
    contain; an UNMATCHED `{` is passed through verbatim here and mangled into
    a concatenation there. MEASURED over the live manifest: all 3 brace refs
    are well-formed with non-empty alternatives, so refusing the malformed
    forms outright — in BOTH layers — costs zero real rows and replaces two
    silent, divergent behaviours with one refusal.

    r8 [M]: the validation itself now lives in check_brace_grammar(), called
    from cite_refs() on EVERY ref-shaped occurrence, so a reference dropped by
    the boundary test is still validated exactly as the checker validates it.
    This function keeps calling it too — expansion must never run on a group
    that was not checked, whatever the caller does."""
    check_brace_grammar(ref)
    if "{" not in ref:
        return [ref]
    pre, rest = ref.split("{", 1)
    alts, post = rest.split("}", 1)
    return [pre + a + post for a in alts.split(",")]


def log_verdicts(path):
    """-> (lines, [(lineno, 'GO'|'NO-GO'), ...]) with CR stripped."""
    # EXACTLY ONE trailing CR, matching awk's sub(/\r$/, "") (r3 [M]):
    # rstrip() removed a RUN, so a double-CR line parsed here and not there.
    lines = [x[:-1] if x.endswith(b"\r") else x
             for x in open(path, "rb").read().split(b"\n")]
    vs = []
    for i, s in enumerate(lines, 1):
        if s == b"VERDICT: GO":
            vs.append((i, "GO"))
        elif s == b"VERDICT: NO-GO":
            vs.append((i, "NO-GO"))
    return lines, vs


def tail_marks(lines, vline):
    """MEASURED C26 tail rule, implemented EXACTLY (r2 [M] corrected an
    over-claiming first draft). Everything strictly after the pinned verdict
    must be an EMPTY line or a reviewer RC marker, and there must be AT MOST
    ONE marker — the comment used to say "one" while the code accepted many
    and ignored all but the first. No whitespace is stripped beyond a trailing
    CR: the checker does not strip either, and the two must agree exactly or
    the generator can emit rows the checker rejects.
    -> (ok, [markers])"""
    marks = []
    for s in lines[vline:]:
        t = s.decode("utf-8", "replace")
        if t == "":
            continue
        if RC_MARK.match(t):
            marks.append(t.split("_RC=")[0])
            continue
        return False, marks
    return len(marks) <= 1, marks


def reviewer_of(lines, vline, evbase):
    """DERIVED, never defaulted (r2 [M]). The RC marker binds identity; a
    filename token is the documented second source; anything else is
    `unattributed` — silently calling those `codex` was an invention.

    r8 [L]: this comment said "10 of the 17" and had gone stale — a measured
    fact asserted in a comment is a claim the code must still satisfy, so it
    is re-measured rather than adjusted by eye. CURRENT, measured over the
    live ledger with THIS module's own log_verdicts()/tail_marks():
    18 distinct pinned closure logs, 7 carry an RC marker, **11 carry none**;
    of those 11, 2 resolve by filename token (grok/opus) and 9 end as
    `unattributed`. Reproduce:
        python3 - <<'EOF'
        import importlib.util
        s = importlib.util.spec_from_file_location(
            "g", "port/sim/device/gen-closure-ledger.py")
        g = importlib.util.module_from_spec(s); s.loader.exec_module(g)
        ev = {f[4]: int(f[6]) for f in
              (l.split() for l in open("port/sim/device/m4-closure-ledger.txt"))
              if len(f) == 12 and f[0] == "CLOSURE" and f[4] != "-"}
        m = [bool(g.tail_marks(g.log_verdicts(e)[0], v)[1])
             for e, v in ev.items()]
        print(len(m), m.count(True), m.count(False))
        EOF
    """
    ok, marks = tail_marks(lines, vline)
    if not ok:
        return ""
    if marks:
        return RC_REVIEWER.get(marks[0], "")
    if "grok" in evbase:
        return "grok"
    if "opus" in evbase:
        return "opus5"
    return "unattributed"


def contains(path, needle):
    return subprocess.call(["grep", "-qF", "--", needle, path],
                           stdout=subprocess.DEVNULL,
                           stderr=subprocess.DEVNULL) == 0


# r6 [M]: the cap arm bound on `index(header, date) > 0` — ANY H2 whose text
# happens to contain the date, including one whose PROSE mentions it
# ("re-examine the 2026-07-17 cap"), and on a bare `index(line, base) > 0`, so
# `xadbsh.shy` counted as a mention. Both are now anchored. MEASURED over
# docs/AGENT-LOG.md: 179 `## ` headers, 177 carry a date, and every one of
# those 177 carries it as a delimited FIELD in one of exactly two shapes —
# `— <date> — ` (176) and `(<date>, ` (1, the LOOP STOP marker). So the
# anchored grammar refuses a date buried in prose at zero cost to real rows.
SEC_DATE = (r"^## .* — %s — ", r"^## .*\(%s, ")
# Word-bounded basename: the mention may not be part of a longer token.
BASE_TOK = r"(?:^|[^A-Za-z0-9._-])%s(?:[^A-Za-z0-9._-]|$)"


def agent_log_sections(date, base):
    """-> number of DISTINCT dated sections that mention the producer.

    r6 [M]: this returned a count of MENTION LINES across every section whose
    header contained the date, so several unrelated sections could each
    contribute. The cap must bind to ONE section, and the caller requires
    exactly one."""
    if not os.path.isfile(AGENT_LOG):
        return 0
    hdr = [re.compile(p % re.escape(date)) for p in SEC_DATE]
    tok = re.compile(BASE_TOK % re.escape(base))
    secs, insec, hit = 0, False, False
    for line in read_bytes_lines(AGENT_LOG, "ledger of record"):
        if line.startswith("## "):
            if insec and hit:
                secs += 1
            insec, hit = any(h.match(line) for h in hdr), False
        if insec and tok.search(line):
            hit = True
    if insec and hit:
        secs += 1
    return secs


CAPS = {}
CONSUMED_CAPS = set()


def parse_caps(argv):
    """--cap <producer-path>=<YYYY-MM-DD>, repeatable. An explicit driver
    assertion; never inferred from cite prose."""
    out, i = {}, 0
    while i < len(argv):
        if argv[i] == "--cap":
            if i + 1 >= len(argv) or "=" not in argv[i + 1]:
                raise SystemExit("--cap needs <path>=<YYYY-MM-DD>")
            k, v = argv[i + 1].split("=", 1)
            if k in out:
                raise SystemExit("--cap given twice for %s" % k)
            out[k] = v
            i += 2
            continue
        if argv[i] not in ("--write",):
            raise SystemExit("unknown argument %r" % argv[i])
        i += 1
    return out


def main():
    global CAPS
    CAPS = parse_caps(sys.argv[1:])
    rows = manifest_rows()
    bases = [p.rsplit("/", 1)[-1] for _, p, _, _ in rows]
    records, residue, skipped = [], [], []

    for psha, path, status, cite in rows:
        base = path.rsplit("/", 1)[-1]
        # r7b [L]: this used to sit inside the reviewed-go arm only, so the 24
        # gate rows and the cap row bypassed it entirely — the exact
        # generator/checker parity this function exists to guarantee.
        safe_path(path, "producer")
        if status in OPEN:
            skipped.append((path, status))
            continue

        # ---- non-arc rows: a NAMED EXIT GATE, as a positive token (C27) ----
        if status in CLOSED_GATE:
            records.append([path, psha, CLOSED_GATE[status], "-", "-", "-",
                            "gate", "-", "-", "gate", "-"])
            continue

        if status not in CLOSED_ARC:
            residue.append((path, status, "unknown status token"))
            continue

        # ---- reviewed-go: resolve the arc's artifacts ----------------------
        refs = [p for r in cite_refs(cite) for p in cite_expand(r)]
        # r6 [H]: every artifact this tool will HASH must satisfy the same
        # containment rule the checker applies, or the ledger can be derived
        # from bytes that live outside the worktree.
        for p in refs:
            safe_path(p, "cited artifact")
        missing = [p for p in refs if not os.path.isfile(p)]
        if missing:
            residue.append((path, status, "cited artifact absent: %s" % missing[0]))
            continue

        golog = []
        for p in refs:
            if not p.endswith(".log"):
                continue
            lines, vs = log_verdicts(p)
            if vs and vs[-1][1] == "GO":
                golog.append((p, lines, vs[-1][0]))

        if golog:
            # r4 [M]: the closing log used to be fixed FIRST (last cited GO) and
            # the needle ranked only within it, so six rows took a weaker
            # binding while a stronger one existed in another cited GO log.
            # Rank the (artifact x needle) product jointly instead: a `self-sha`
            # bind — the producer's EXACT current sha256 inside the very log
            # that carries the verdict — is the most selective evidence the
            # corpus can offer, and is preferred over every path/basename
            # mention, which any log that merely quoted the manifest contains.
            probes = [("sha", psha), ("path", path)]
            if bases.count(base) == 1:
                probes.append(("base", base))
            pick = None
            for kind, needle in probes:
                for g in golog:
                    if contains(g[0], needle):
                        pick = (g, "self-" + kind, g[0])
                        break
                if pick:
                    break
            if pick is None:
                for kind, needle in probes:
                    for q in refs:
                        if q != golog[-1][0] and contains(q, needle):
                            pick = (golog[-1], "x-" + kind, q)
                            break
                    if pick:
                        break
            if pick is None:
                residue.append((path, status,
                                "no cited artifact names the producer "
                                "(path/basename/sha) - arc coverage unprovable"))
                continue
            (evp, evlines, evline), cov, covref = pick
            # r6 [M]: the checker's art_ok accepts `.log` OR `.txt` for both
            # `ev` and `covref`, but this tool only ever considers `.log` for a
            # VERDICT (the `.endswith(".log")` filter above). MEASURED: all 59
            # emitted `ev` are `.log`, all 7 `.txt` artifacts appear only as
            # `covref`. Assert the invariant here and enforce it there, so the
            # two layers agree on what may carry a verdict.
            assert evp.endswith(".log"), evp
            ok, _marks = tail_marks(evlines, evline)
            if not ok:
                residue.append((path, status,
                                "the terminal GO in %s is followed by content "
                                "that is not a single reviewer RC marker" % evp))
                continue
            evbase = evp.rsplit("/", 1)[-1]
            rev = reviewer_of(evlines, evline, evbase)
            if not rev:
                residue.append((path, status,
                                "reviewer identity not derivable from %s" % evp))
                continue
            rnd = evbase[:-len(".log")]
            if rnd.startswith("review-"):
                rnd = rnd[len("review-"):]

            records.append([path, psha, "GO", evp, sha256(evp), str(evline),
                            cov, covref, sha256(covref), rev, rnd])
            continue

        # ---- driver cap ---------------------------------------------------
        # NEVER inferred from prose (r2 [H]): the cite grammar hyphen-separates
        # every word, so `NOT-CAPPED-CLOSED` contains `CAPPED-CLOSED`. A cap is
        # a DRIVER ASSERTION and must be made explicitly on the command line:
        #   --cap port/sim/device/adbsh.sh=2026-07-17
        # It is still only accepted if the AGENT-LOG binding independently
        # verifies, so the flag asserts intent, not evidence.
        if path in CAPS:
            CONSUMED_CAPS.add(path)
            m = re.match(r"^([0-9]{4}-[0-9]{2}-[0-9]{2})$", CAPS[path])
            if not m:
                residue.append((path, status,
                                "--cap date %r is not YYYY-MM-DD" % CAPS[path]))
                continue
            if bases.count(base) != 1:
                residue.append((path, status,
                                "CAPPED-CLOSED but basename not unique"))
                continue
            nsec = agent_log_sections(m.group(1), base)
            if nsec != 1:
                residue.append((path, status,
                                "CAPPED-CLOSED date %s binds %d dated ledger "
                                "section(s) mentioning '%s'; exactly one is "
                                "required" % (m.group(1), nsec, base)))
                continue
            records.append([path, psha, "CAPPED-CLOSED", "-", "-", "-",
                            "ledger", m.group(1), "-", "driver", "-"])
            continue

        residue.append((path, status,
                        "no terminal-GO log and no --cap <path>=<YYYY-MM-DD> "
                        "assertion for this producer"))

    # r3 [M]: a --cap for an open, gate-proven or already-GO-backed row used to
    # be silently ignored (and counted "used" merely because the path existed),
    # so a malformed or contradictory driver assertion could pass unnoticed.
    # Every cap must be CONSUMED by the cap arm; anything else refuses.
    for k in sorted(CAPS):
        if k not in CONSUMED_CAPS:
            residue.append((k, "-",
                            "--cap was not consumed: the path is not a "
                            "reviewed-go row without a terminal-GO log"))

    records.sort(key=lambda r: r[0])
    body = []
    for r in records:
        body.append("CLOSURE " + " ".join(r))

    print("== derived closure records ==")
    print("  manifest rows      : %d" % len(rows))
    print("  emitted records    : %d" % len(records))
    print("  open (no record)   : %d" % len(skipped))
    print("  CANNOT DERIVE      : %d" % len(residue))
    by = {}
    for r in records:
        by[r[2] if r[2] != "GO" else "GO/" + r[6]] = \
            by.get(r[2] if r[2] != "GO" else "GO/" + r[6], 0) + 1
    for k in sorted(by):
        print("      %-28s %d" % (k, by[k]))
    if residue:
        print("\n== CANNOT DERIVE (driver ruling required) ==")
        for p, s, why in residue:
            print("  %-56s %-14s %s" % (p, s, why))
    if skipped:
        print("\n== open rows, deliberately unrecorded ==")
        for p, s in skipped:
            print("  %-56s %s" % (p, s))


    if "--write" in sys.argv:
        if residue:
            print("\nREFUSING to write: %d row(s) cannot be derived. A partial "
                  "ledger would be a silent coverage gap." % len(residue))
            return 1
        head = HEADER % (len(records), len(skipped))
        # An OPEN row has no closure record by construction, so deleting one
        # would otherwise be invisible to cardinality (r5 [M]). Pin the count.
        text = (head + "LEDGER-OPEN-ROWS %d\n\n" % len(skipped)
                + "\n".join(body) + "\n")
        # r6 [H]: a plain `open(LEDGER, "w")` FOLLOWS a symlink and truncates
        # whatever it points at — a destructive write to a file this tool does
        # not own, done before the checker (which refuses a symlinked ledger)
        # ever runs. Refuse the symlink here too, and write via a temp file in
        # the SAME directory + os.replace, so a crash mid-write can never leave
        # a half-ledger that later scans would read as authoritative.
        safe_path(LEDGER, "ledger")
        if os.path.islink(LEDGER):
            raise SystemExit("REFUSING: %s is a SYMLINK, not evidence" % LEDGER)
        # r7b [H]: `LEDGER + ".tmp"` is a DETERMINISTIC name, so two concurrent
        # generators share one inode: A can os.replace it into the
        # authoritative ledger while B still holds it open, and B's remaining
        # writes then land in the INSTALLED ledger. This is not theoretical —
        # two writers ran in this one worktree while this arc was live. Create
        # the temp exclusively (O_CREAT|O_EXCL, unique suffix) in the ledger's
        # OWN directory so os.replace stays atomic on one filesystem, and
        # remove it if anything below fails.
        fd, tmp = tempfile.mkstemp(dir=os.path.dirname(LEDGER) or ".",
                                   prefix=os.path.basename(LEDGER) + ".",
                                   suffix=".tmp")
        try:
            with os.fdopen(fd, "w", encoding="utf-8") as f:
                f.write(text)
            # r8 [L]: mkstemp creates 0600 and os.replace PRESERVES that mode
            # onto the installed file, so the r7b atomicity fix silently
            # changed the tracked ledger from 0644 to 0600 — invisible to every
            # check here, but a tracked evidence file only its owner can read
            # is a trap for any cross-UID consumer (CI, a review checkout).
            # Install the mode EXPLICITLY rather than preserving whatever is
            # already there: the on-disk file had already been damaged to 0600
            # by the time this was found, so "preserve the previous mode" would
            # have carried the damage forward silently. 0644 is what the peer
            # artifacts (checker, generator) carry, and umask gets no vote —
            # the mode of a derived, tracked file is part of its definition.
            os.chmod(tmp, LEDGER_MODE)
            os.replace(tmp, LEDGER)
        except BaseException:
            try:
                os.unlink(tmp)
            except OSError:
                pass
            raise
        print("\nwrote %s (%d records)" % (LEDGER, len(records)))
        print("LEDGER_SHA256=%s" % sha256(LEDGER))
    return 0


HEADER = """\
# port/sim/device/m4-closure-ledger.txt - structured closure records for the
# M4 freeze manifest (fix_plan C25/C26/C27). DERIVED, never hand-written:
#   python3 port/sim/device/gen-closure-ledger.py --write \\
#       --cap port/sim/device/adbsh.sh=2026-07-17
# Verified by: bash port/sim/device/check-cite-closure.sh -> CITE CLOSURE OK
#
# WHY. The manifest's `cite` column is free text, so a `reviewed-go` row can
# cite a real terminal GO that belongs to a DIFFERENT arc (C25), a GO that is
# positionally terminal but semantically inert (C26), or, for a non-arc row,
# prose that DENIES the gate it must assert (C27). Nothing in prose binds a
# verdict to a producer. These records do, by identity.
#
# RECORD GRAMMAR - anchored, fixed arity, fail closed. Exactly 12
# whitespace-separated fields; any `CLOSURE` line with a different arity is a
# refusal, never a partial parse (PROCESS 3 whitelist-grammar rule). The file
# must end with a newline: an unterminated final record is counted by every
# awk scan but skipped by a `while read` loop, so the checker refuses that too.
#
#   CLOSURE <path> <psha> <verdict> <ev> <evsha> <vline> <cov> <covref> <covsha> <reviewer> <round>
#
#   path     producer path; must be a manifest row, exactly once
#   psha     producer sha256; must equal the manifest row AND the bytes on disk
#   verdict  GO | CAPPED-CLOSED | PROVEN-BY-HARD-RULE-3
#            | PROVEN-BY-M1-EXIT-GATE | PROVEN-BY-M2-EXIT-GATE
#            Closed vocabulary matched WHOLE-FIELD, so C27's denial form
#            ("...NOT-proven-by-SIM-CONFORMS...") is unrepresentable. The
#            PROVEN-BY-* token is DERIVED FROM THE MANIFEST'S ANCHORED STATUS
#            FIELD, never from cite prose, and the checker RECOMPUTES it from
#            the status and refuses any disagreement.
#   ev       the closure log carrying the verdict (`-` when not arc-closed)
#   evsha    sha256 of that log - evidence is now tamper-evident: appending a
#            transcript to a cited log changes this and the gate refuses
#   vline    1-based line number of the verdict INSIDE ev. The gate reads THAT
#            line, requires it to be exactly `VERDICT: GO`, requires it to be
#            the LAST anchored verdict in the file, and requires everything
#            after it to be blank or AT MOST ONE reviewer `<TOOL>_RC=<n>`
#            marker (C26; measured over every cited closure log).
#   cov      how this GO is bound to THIS producer:
#              self-sha | self-path | self-base  the SAME sha-pinned artifact
#                        carries the verdict AND names the producer (by its
#                        exact sha256, its path, or its unique basename), so no
#                        cross-artifact arc relation is assumed. STATED
#                        PRECISELY (r5 [M]): a substring match proves the
#                        closing artifact MENTIONS this producer, NOT that the
#                        review's SCOPE was this producer. C25 is NARROWED for
#                        such a row, not closed. Only a harness-emitted scope
#                        record would close it.
#              x-sha | x-path | x-base  the binding lives in a DIFFERENT cited
#                        artifact, so the row also rests on those two artifacts
#                        being rounds of ONE arc - which nothing in the corpus
#                        makes machine-checkable. GRADED and COUNTED, never
#                        silently equated with a self-* row. Registered
#                        residual; the durable fix is an arc id emitted by the
#                        review harness (a PROCESS change).
#              ledger    a driver cap, bound to a dated AGENT-LOG section
#              gate      a named EXIT GATE, derived from the status
#   covref   the artifact (or AGENT-LOG date) carrying that binding
#   covsha   sha256 of covref (`-` for ledger/gate)
#   reviewer codex | grok | opus5 | unattributed | driver | gate. DERIVED from
#            the RC marker, then from a filename token, and otherwise
#            `unattributed` - never defaulted to codex, because 11 of the 18
#            pinned closure logs carry no marker and calling those codex would
#            be an invention. A marker, when present, must read `=0`: a nonzero
#            reviewer exit code means the round FAILED.
#   round    the arc round; DERIVED from ev's basename (drop `.log`, drop a
#            leading `review-`) and compared for EQUALITY, not as a substring
#
# COVERAGE IS AN ARC-LEVEL RELATION, MEASURED NOT ASSUMED. Over the live corpus
# only 37 of 59 GO-backed rows have a terminal-GO log naming their own path, so
# the naive "the GO log must name the producer" rule false-rejects 22 real rows:
# the arc's EARLY rounds name the producer and the closure round often does not
# (riglib.sh closes at review-113-7 while its binding is review-109-9). Hence
# the self-/x- grading rather than a rule the corpus cannot satisfy.
#
# %d records; %d manifest rows are deliberately UNRECORDED because their
# status is arc-in-flight/arc-pending - an open row must have no closure
# record, and the checker refuses one that does.
#
# INTEGRITY ANCHOR: this file's sha256 is pinned as LEDGER_SHA256= inside
# port/sim/device/check-cite-closure.sh. ANY edit here - record or comment -
# must update that literal IN THE SAME COMMIT:
#   shasum -a 256 port/sim/device/m4-closure-ledger.txt
# Regenerating (`--write`) prints the new value.
#
"""

if __name__ == "__main__":
    sys.exit(main())
