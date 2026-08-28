#!/usr/bin/env bash
# port/foh/check-persist-table.sh — ticket #22 (ADR 0001): the persistence
# record is a DECLARATIVE FIELD TABLE, and this is what proves it.
#
# WHY THIS CHECK EXISTS, AND WHAT IT IS FOR
# -----------------------------------------
# The change it guards is a serialiser rewrite whose entire contract is "no
# observable difference". Nothing on screen can see a table replacing a
# hand-written parser; the ONE thing that can is the bytes. So every
# judgement here is made on bytes, against fixtures this script builds from
# the FORMAT (foh_persist.h's grammar) rather than from the C — an
# expectation derived from the code under test proves nothing.
#
# ADR 0001 names the risk it accepts: "an offset table is exactly the kind
# of thing that is wrong in a way tests must catch rather than review", and
# "the static assertion is load-bearing… a check must prove the guard still
# bites". Legs [8] and [9] are that proof, and they are NEGATIVE BUILDS: a
# perturbed COPY of the header must FAIL to compile. The tree is never
# edited (check-rebind.sh's perturb_build discipline, and the no-commit
# guard at the bottom enforces it).
#
# HOST ONLY. It builds and runs foh_persist.c and nothing else of the FOH —
# no device, no SDL, no data planes. It is the cheap standing guard; the
# expensive one is check-device-persist.sh, whose independent positional
# whitelist and migration teeth this check deliberately MIRRORS host-side
# (leg [7]) so that a persistence change cannot go to hardware blind.
#
# Prints `PERSIST TABLE OK`, exit 0. Any deviation exits nonzero.
set -euo pipefail

FOH=port/foh
BUILD=$FOH/build/persist-table
WIT=$FOH/foh_persist_witness.c
SRC=$FOH/foh_persist.c
HDR=$FOH/foh_persist.h

fail() { echo "PERSIST TABLE FAIL: $1" >&2; exit 1; }
grammar_die() { echo "PERSIST TABLE FAIL: $1" >&2; exit 2; }
relay_lines() { sed 's/^/  | /'; }

made() {
  local f
  for f in "$@"; do [ -s "$f" ] || fail "expected artifact missing/empty: $f"; done
}

LOCK=$FOH/build/persist-table.lock
mkdir -p "$FOH/build"
mkdir "$LOCK" 2>/dev/null || fail "another run holds $LOCK (remove it only if
  you have proven no other run is live)"
trap 'rmdir "$LOCK" 2>/dev/null || true' EXIT

# --- no-commit guard (every committed check carries one) ---------------------
tree_fingerprint() {
  local status diff files hashes
  status="$(git status --porcelain)" || return 1
  diff="$(git diff)" || return 1
  files="$(git ls-files -o --exclude-standard)" || return 1
  files="$(printf '%s\n' "$files" | grep -v '^\.tokensave/' || true)"
  if [ -n "$files" ]; then
    hashes="$(printf '%s\n' "$files" | tr '\n' '\0' | xargs -0 shasum -a 256)" \
      || return 1
  else
    hashes=""
  fi
  printf '%s\n%s\n%s\n' "$status" "$diff" "$hashes" | shasum -a 256 | cut -d' ' -f1
}
git_dirty_before="$(tree_fingerprint)" \
  || fail "could not fingerprint the working tree (fails CLOSED)"
[[ "$git_dirty_before" =~ ^[0-9a-f]{64}$ ]] \
  || fail "tree fingerprint '$git_dirty_before' is not a sha256 digest"

rm -rf "$BUILD"
mkdir -p "$BUILD"
made "$WIT" "$SRC" "$HDR"

teeth=0

# --- [0] the source pins -----------------------------------------------------
# The three claims this check is built on, asserted textually so that a
# refactor that removes one is loud here rather than silently making a later
# leg vacuous. A check whose subject moved is a check that proves nothing.
echo "=== [0] source pins"
pin1() { # <file> <exact line>
  local n
  n="$(grep -cxF "$2" "$1")" || true
  [ "$n" = 1 ] || grammar_die "[0] $1 has $n copies of
   |$2|
   (want exactly 1) — the mechanism this check judges has moved; re-read it"
}
pin1 "$SRC" 'static const FpField FP_TABLE[] = {FP_FIELDS(FP_ROW)};'
pin1 "$SRC" '_Static_assert(sizeof(FohPersist) =='
pin1 "$SRC" '                   FP_ALIGN_UP(FP_TABLE_BYTES + FP_UNPERSISTED_BYTES,'
pin1 "$SRC" '#define FP_UNPERSISTED_BYTES (sizeof(int) /* layoutGuard */)'
pin1 "$HDR" '  int layoutGuard;'
# ONE writer and ONE reader. The whole point of the table is that no other
# code knows the file's shape, so a second snprintf of a persisted line
# would be the defect the ticket exists to remove.
nser="$(grep -c 'fp_addf(buf, cap' "$SRC")" || true
[ "$nser" -ge 5 ] \
  || grammar_die "[0] $SRC has $nser table-driven append sites (want >= 5) —
   the writer is no longer walking the table"
# every FP_FIELDS row, counted, so a dropped row is loud
nrows="$(grep -cE '^  X\([A-Za-z]' "$SRC")" || true
[ "$nrows" = 27 ] \
  || grammar_die "[0] FP_FIELDS has $nrows rows (want 26: turbo, lcancel,
   tapjump, ctlstyle, modonr, rec, flash, walljump, blastzone, dustless,
   phantom, soundslevel, musiclevel, bind, sel, resume, then ticket #25's
   CSS machine plane — ptype, cpudiff, vsmode, hand, slider, carry, cpucarry,
   handtype — then ticket #26's target-select trio, tsscur, tsspage and
   tsshand). A row was added or removed — if that was deliberate, this number moves WITH
   the format and the fixture builder in leg [1] moves with it too"
echo "    [0] OK: table, static assertion, layout guard and 27 rows all present"

# --- [1] the fixture builder (INDEPENDENT of the C under test) ---------------
# Builds a canonical MLFKPERSIST7 file from the FORMAT — the grammar written
# out in foh_persist.h — and reseals it. Everything downstream compares the
# product's bytes against THIS, never against the product's own last output,
# which is how a serialiser bug that is stable would otherwise pass forever.
echo "=== [1] independent fixture builder"
cat > "$BUILD/fixture.js" <<'EOF'
// Canonical MLFKPERSIST7 bytes, written from the FORMAT (foh_persist.h),
// not from foh_persist.c. `mode` picks the record: `seed` must match
// foh_persist_witness.c's seed() field for field, `defaults` must match
// foh_persist_defaults().
const crypto = require("crypto");
const bits = (x) => {
  const b = Buffer.alloc(8);
  b.writeDoubleBE(x);
  return b.toString("hex");
};
function record(mode) {
  const seeded = mode === "seed";
  const rec = [];
  for (let c = 0; c < 5; c++) {
    for (let s = 0; s < 10; s++) {
      rec.push(seeded ? (c === 2 && s === 7 ? -1 : c * 10 + s + 0.5) : -1);
    }
  }
  return {
    turbo: seeded ? 1 : 0,
    lcancel: seeded ? 2 : 0,
    tapjump: seeded ? [1, 0, 1, 1] : [0, 0, 0, 0],
    ctlstyle: seeded ? 1 : 2, // fresh install is CTL_STYLE_NATURAL
    modonr: seeded ? 0 : 1,   // fresh install is D30's swapped shoulder
    rec,
    flash: seeded ? 1 : 0,
    walljump: seeded ? 1 : 0,
    blastzone: 0,
    dustless: seeded ? 1 : 0,
    phantom: seeded ? 0.02 : 0.01,
    soundslevel: seeded ? 0.7 : 0.5,
    musiclevel: seeded ? 0.4 : 0.3,
    bind: [
      seeded ? [3, 1, 0, 2, 5, 4, 7, 6] : [0, 1, 2, 3, 4, 5, 6, 7],
      [0, 1, 2, 3, 4, 5, 6, 7],
      [0, 1, 2, 3, 4, 5, 6, 7],
      [0, 1, 2, 3, 4, 5, 6, 7],
    ],
    sel: seeded ? [1, 2, 3, 4] : [0, 0, 0, 0],
    resume: seeded ? 14 : 0, // FOH_TSS / FOH_STARTUP
    // ticket #25's CSS machine plane. The DEFAULTS side is restated from
    // foh.h's CSS COLD-START PLANE — the formulas, not the numbers they
    // happen to produce today — because that is what makes this an
    // INDEPENDENT expectation: if foh_persist_defaults() ever stops asking
    // foh.h and starts carrying its own copy, the two drift and leg [3]
    // says so. RAST_W/RAST_H are 240 (port/gfx/raster.h), the CSS panel is
    // at 1 + 60k with its rail at +11 and 36 long (foh.h).
    ptype: seeded ? [0, 1, -1, 1] : [0, -1, -1, -1],
    cpudiff: seeded ? [3, 1, 4, 2] : [3, 3, 3, 3],
    vsmode: seeded ? 1 : 0,
    hand: seeded ? [33.25, 240.0]
                 : [(140.0 * 240) / 1200.0, (700.0 * 240) / 750.0],
    slider: seeded
      ? [10.5, 17.75, 25.0, 32.25]
      : [0, 1, 2, 3].map((k) => 1 + 60 * k + 11 + (116 / 166) * 36),
    // both grabs set in the seed — a state the screen cannot produce (a held
    // knob pins the hand to the rail), which is the point: the file's domain
    // is per row, so two adjacent one-digit rows must be distinguishable by
    // value or a swap between them would round-trip unnoticed.
    carry: seeded ? 2 : -1,
    cpucarry: seeded ? 3 : -1,
    handtype: seeded ? 2 : 0,
    // ticket #26. The seeded cursor is 10 — the ELEVENTH value, which is
    // the whole reason `tsscur` is a two-digit row: a fixture that only
    // ever wrote 0..9 would agree with a row mistyped as a one-digit flag.
    tsscur: seeded ? 10 : 0,
    tsspage: seeded ? 1 : 0,
    // the DEFAULTS side is foh.h's FOH_TSS_HOME_{X,Y} restated as its own
    // formula — slot 0's centre, x = 8 + 100/2, y = 30 + 19/2 — for the
    // reason the CSS block's defaults are formulas: an expectation that
    // copied the numbers could not notice the definition moving.
    tsshand: seeded ? [12.5, 205.75] : [8 + 100 / 2, 30 + 19 / 2],
  };
}
// The WIRE BIAS, restated from the format (foh_persist.h) rather than read
// out of the table: the file column is an unsigned digit and three of these
// fields are not, so the column is `value + bias`. A bias applied on only
// one side of foh_persist.c would round-trip its own file perfectly and
// still be wrong; this is the side that notices.
const BIAS = { ptype: 1, cpudiff: -1, carry: 1, cpucarry: 1 };
function lines(r) {
  const L = ["MLFKPERSIST7"];
  L.push("turbo " + r.turbo);
  L.push("lcancel " + r.lcancel);
  L.push("tapjump " + r.tapjump.join(" "));
  L.push("ctlstyle " + r.ctlstyle);
  L.push("modonr " + r.modonr);
  for (let c = 0; c < 5; c++) {
    for (let s = 0; s < 10; s++) {
      L.push("rec " + c + " " + s + " " + bits(r.rec[c * 10 + s]));
    }
  }
  L.push("flash " + r.flash);
  L.push("walljump " + r.walljump);
  L.push("blastzone " + r.blastzone);
  L.push("dustless " + r.dustless);
  L.push("phantom " + bits(r.phantom));
  L.push("soundslevel " + bits(r.soundslevel));
  L.push("musiclevel " + bits(r.musiclevel));
  for (let k = 0; k < 4; k++) L.push("bind " + k + " " + r.bind[k].join(" "));
  L.push("sel " + r.sel.join(" "));
  L.push("resume " + String(r.resume).padStart(2, "0"));
  L.push("ptype " + r.ptype.map((t) => t + BIAS.ptype).join(" "));
  L.push("cpudiff " + r.cpudiff.map((d) => d + BIAS.cpudiff).join(" "));
  L.push("vsmode " + r.vsmode);
  L.push("hand " + r.hand.map(bits).join(" "));
  L.push("slider " + r.slider.map(bits).join(" "));
  L.push("carry " + (r.carry + BIAS.carry));
  L.push("cpucarry " + (r.cpucarry + BIAS.cpucarry));
  L.push("handtype " + r.handtype);
  L.push("tsscur " + String(r.tsscur).padStart(2, "0")); // TWO digits
  L.push("tsspage " + r.tsspage);
  L.push("tsshand " + r.tsshand.map(bits).join(" "));
  return L;
}
// The record dump foh_persist_witness.c prints, derived from the SAME
// record — so "every field survived the round trip" is judged against the
// format, not against whatever the loader happened to produce.
function dump(r) {
  const D = [];
  D.push("turbo " + r.turbo);
  D.push("lcancel " + r.lcancel);
  for (let k = 0; k < 4; k++) D.push("tapjump " + k + " " + r.tapjump[k]);
  D.push("ctlstyle " + r.ctlstyle);
  D.push("modonr " + r.modonr);
  for (let c = 0; c < 5; c++) {
    for (let s = 0; s < 10; s++) {
      D.push("rec " + c + " " + s + " " + bits(r.rec[c * 10 + s]));
    }
  }
  D.push("flash " + r.flash);
  D.push("walljump " + r.walljump);
  D.push("blastzone " + r.blastzone);
  D.push("dustless " + r.dustless);
  D.push("phantom " + bits(r.phantom));
  D.push("soundslevel " + bits(r.soundslevel));
  D.push("musiclevel " + bits(r.musiclevel));
  for (let k = 0; k < 4; k++) {
    for (let i = 0; i < 8; i++) D.push("bind " + k + " " + i + " " + r.bind[k][i]);
  }
  for (let k = 0; k < 4; k++) D.push("sel " + k + " " + r.sel[k]);
  D.push("resume " + r.resume);
  // UNBIASED here, biased in lines() above: the dump is the FIELD's value
  // and the file is the COLUMN. Printing the same number in both would let
  // a bias that is applied on neither side, or on both, pass unnoticed.
  for (let k = 0; k < 4; k++) D.push("ptype " + k + " " + r.ptype[k]);
  for (let k = 0; k < 4; k++) D.push("cpudiff " + k + " " + r.cpudiff[k]);
  D.push("vsmode " + r.vsmode);
  D.push("hand " + r.hand.map(bits).join(" "));
  for (let k = 0; k < 4; k++) D.push("slider " + k + " " + bits(r.slider[k]));
  D.push("carry " + r.carry);
  D.push("cpucarry " + r.cpucarry);
  D.push("handtype " + r.handtype);
  // the DUMP prints the field's value, not the file's column: `tsscur` is
  // padded to two digits on the wire and is a plain integer here.
  D.push("tsscur " + r.tsscur);
  D.push("tsspage " + r.tsspage);
  D.push("tsshand " + r.tsshand.map(bits).join(" "));
  return D.join("\n") + "\n";
}
function seal(L) {
  const body = L.join("\n") + "\n";
  return body + "SUM " + crypto.createHash("sha256").update(body).digest("hex") + "\n";
}
module.exports = { record, lines, dump, seal, bits };
if (require.main === module) {
  const fs = require("fs");
  const [what, mode, out] = process.argv.slice(2);
  const r = record(mode);
  fs.writeFileSync(out, what === "dump" ? dump(r) : seal(lines(r)));
}
EOF
node "$BUILD/fixture.js" file seed "$BUILD/seed.dat"
node "$BUILD/fixture.js" dump seed "$BUILD/seed.dump"
node "$BUILD/fixture.js" file defaults "$BUILD/defaults.dat"
node "$BUILD/fixture.js" dump defaults "$BUILD/defaults.dump"
made "$BUILD/seed.dat" "$BUILD/seed.dump" "$BUILD/defaults.dat" "$BUILD/defaults.dump"
[ "$(grep -c "" "$BUILD/seed.dat")" = 81 ] \
  || grammar_die "[1] the independently built fixture is not 81 LF lines —
   fixture construction is broken, so every leg below would be vacuous"
cmp -s "$BUILD/seed.dat" "$BUILD/defaults.dat" \
  && fail "[1] the seeded and default fixtures are identical (dead tooth: a
   loader that dropped every field would still satisfy the round trip)"
echo "    [1] OK: 70-line seeded fixture and a distinct defaults fixture built
    from the format"

# The variant tool: derives a fixture from another by named edits, then
# reseals (or deliberately does not). Used by every negative leg below.
cat > "$BUILD/variant.js" <<'EOF'
const fs = require("fs");
const crypto = require("crypto");
const [src, dst, ...ops] = process.argv.slice(2);
let L = fs.readFileSync(src, "utf8").split("\n");
if (L.pop() !== "") throw new Error("fixture has no final LF");
const at = (k) => L.findIndex((l) => l === k || l.startsWith(k + " "));
let reseal = true, trunc = 0;
for (const op of ops) {
  const eq = op.indexOf("=");
  const kind = eq < 0 ? op : op.slice(0, eq);
  const arg = eq < 0 ? "" : op.slice(eq + 1);
  if (kind === "hdr") L[0] = "MLFKPERSIST" + arg;
  else if (kind === "drop") {
    const i = at(arg);
    if (i < 0) throw new Error("drop: no row " + arg);
    L.splice(i, 1);
  } else if (kind === "dropall") {
    const keep = L.filter((l) => !l.startsWith(arg + " "));
    if (keep.length === L.length) throw new Error("dropall: no rows " + arg);
    L = keep;
  } else if (kind === "set") {
    const c = arg.indexOf(":");
    const k = arg.slice(0, c), v = arg.slice(c + 1);
    const i = at(k);
    if (i < 0) throw new Error("set: no row " + k);
    if (L[i] === v) throw new Error("set: no-op on " + k);
    L[i] = v;
  } else if (kind === "before") {
    const at2 = arg.lastIndexOf("@");
    const i = at(arg.slice(at2 + 1));
    if (i < 0) throw new Error("before: no anchor");
    L.splice(i, 0, arg.slice(0, at2));
  } else if (kind === "append") {
    L.splice(L.findIndex((l) => l.startsWith("SUM ")), 0, arg);
  } else if (kind === "swap") {
    const [a, b] = arg.split(",");
    const ia = at(a), ib = at(b);
    if (ia < 0 || ib < 0) throw new Error("swap: missing row");
    const t = L[ia]; L[ia] = L[ib]; L[ib] = t;
  } else if (kind === "stale-sum") reseal = false;
  else if (kind === "trunc") trunc = parseInt(arg, 10);
  else throw new Error("unknown op " + kind);
}
if (reseal) {
  const s = L.findIndex((l) => l.startsWith("SUM "));
  if (s < 0) throw new Error("no SUM line");
  const body = L.slice(0, s).join("\n") + "\n";
  L[s] = "SUM " + crypto.createHash("sha256").update(body).digest("hex");
}
let out = L.join("\n") + "\n";
if (trunc) out = out.slice(0, trunc);
fs.writeFileSync(dst, out);
EOF

# every older version, derived from the seeded v7 file by REMOVING exactly
# the rows that version did not have. Defined once; used by legs [7].
variant() { # <name> <ops...>
  local n="$1"; shift
  node "$BUILD/variant.js" "$BUILD/seed.dat" "$BUILD/v/$n.dat" "$@" \
    || fail "could not derive the '$n' fixture"
  made "$BUILD/v/$n.dat"
  cmp -s "$BUILD/v/$n.dat" "$BUILD/seed.dat" \
    && fail "$n: the derived fixture is byte-identical to the seed (dead tooth)"
  return 0
}
mkdir -p "$BUILD/v"
V4_ROWS=(drop=flash drop=walljump drop=blastzone drop=dustless drop=phantom
         drop=soundslevel drop=musiclevel)
# EVERY row whose `since` is 7 — the hibernate row plus ticket #25's eight CSS
# rows. Named once, because "the rows a v6 file cannot have" is one fact and
# six call sites: the previous shape spelt `drop=resume` inline six times, and
# adding a v7 row would then have needed six edits with nothing failing if one
# were missed (the v6 fixture would simply have carried a row no v6 writer
# could produce, and the migration leg would have passed on it).
V7_ROWS=(drop=resume drop=ptype drop=cpudiff drop=vsmode drop=hand
         drop=slider drop=carry drop=cpucarry drop=handtype
         drop=tsscur drop=tsspage drop=tsshand)
variant v6 hdr=6 "${V7_ROWS[@]}"
variant v5 hdr=5 "${V7_ROWS[@]}" drop=sel
variant v4 hdr=4 "${V7_ROWS[@]}" drop=sel dropall=bind
variant v3 hdr=3 "${V7_ROWS[@]}" drop=sel dropall=bind "${V4_ROWS[@]}"
variant v2 hdr=2 "${V7_ROWS[@]}" drop=sel dropall=bind "${V4_ROWS[@]}" \
  drop=modonr
variant v1 hdr=1 "${V7_ROWS[@]}" drop=sel dropall=bind "${V4_ROWS[@]}" \
  drop=modonr drop=ctlstyle

# --- [2] the witness, built against the REAL tree ----------------------------
echo "=== [2] the persist witness against the real tree"
CFLAGS=(-O2 -ffp-contract=off -Wall -Wextra -Werror -Iport/ryu -Iport/sim
        -Ioracle/qjs)
LINK=(port/sim/ml_ser.c port/sim/ml_fmt.c oracle/qjs/sha256.c)
cc "${CFLAGS[@]}" -o "$BUILD/pw" "$WIT" "$SRC" "${LINK[@]}" -lm \
  || fail "[2] the persist witness did not build against the real tree"
made "$BUILD/pw"
echo "    [2] OK: built"

# run <cmd> <fixture-or-'-'> <outdir> -> $D holds the dir, $EV the stderr
run() { # <cmd> <fixture|-> <name>
  local cmd="$1" fx="$2" nm="$3"
  D=$BUILD/run/$nm
  rm -rf "$D"; mkdir -p "$D"
  [ "$fx" = - ] || cp "$fx" "$D/mlfk-persist.dat"
  set +e
  MLFK_PERSIST_DIR="$PWD/$D" "$BUILD/pw" "$cmd" > "$D/out.txt" 2> "$D/err.txt"
  RC=$?
  set -e
  EV="$D/err.txt"
}
# exactly one TERMINAL event per boot, and it must be the expected one
# (check-device-persist.sh's review-ctl r4 rule, restated here).
want_event() { # <name> <exact stderr line>
  local n nterm
  n="$(grep -cxF "$2" "$EV")" || true
  [ "$n" = 1 ] || { relay_lines < "$EV"
    grammar_die "$1: expected exactly one '$2' (saw $n)"; }
  nterm="$(grep -c '^foh_persist: \(loaded\|reset cause=\)' "$EV")" || true
  [ "$nterm" = 1 ] \
    || grammar_die "$1: expected exactly ONE terminal loaded/reset event, saw $nterm"
}

# --- [3] the writer emits the documented format ------------------------------
# The product saves a record it was GIVEN; the bytes must equal the ones the
# format says, built by leg [1] without reading foh_persist.c.
echo "=== [3] the table WRITER emits the documented bytes"
run seed - w-seed
[ "$RC" = 0 ] || { relay_lines < "$EV"; fail "[3] the seeded save failed"; }
cmp "$D/mlfk-persist.dat" "$BUILD/seed.dat" \
  || fail "[3] the table writer's bytes differ from the format-derived
   fixture — the serialiser and foh_persist.h's documented grammar disagree"
run defaults - w-def
[ "$RC" = 0 ] || fail "[3] the defaults dump failed"
cmp "$D/out.txt" "$BUILD/defaults.dump" \
  || fail "[3] foh_persist_defaults() no longer matches the authored defaults"
echo "    [3] OK: seeded save byte-equal to the format-derived fixture;
    defaults unchanged"

# --- [4] BYTE-IDENTITY: an existing file loads and re-saves byte-identical ---
# Ticket #22's first acceptance criterion, and the one the whole rewrite
# stands on. The fixture is the format's, not the product's last output.
echo "=== [4] byte-identity round trip (acceptance 1)"
run roundtrip "$BUILD/seed.dat" rt
[ "$RC" = 0 ] || { relay_lines < "$EV"; fail "[4] the round trip failed"; }
want_event "[4]" "foh_persist: loaded"
cmp "$D/mlfk-persist.dat" "$BUILD/seed.dat" \
  || fail "[4] an existing MLFKPERSIST7 file did NOT re-save byte-identical
   through the field table — the rewrite is not behaviour-preserving"
# …and every FIELD came back, judged value by value rather than in bulk: a
# writer and reader that dropped the same field would still cmp equal.
run dump "$BUILD/seed.dat" dump
tail -n +2 "$D/out.txt" > "$D/fields.txt"
cmp "$D/fields.txt" "$BUILD/seed.dump" \
  || fail "[4] a loaded field value differs from the format-derived record —
   a table row is pointed at the wrong member, or silently dropped one"
echo "    [4] OK: re-saved byte-identical, and every field value matches"

# --- [5] forward compatible: an UNKNOWN row is skipped (acceptance 4) --------
echo "=== [5] an unknown row loads and is ignored"
variant unknown-tail 'append=zzfuture 1'
variant unknown-mid 'before=zzmid 7@sel'
for nm in unknown-tail unknown-mid; do
  run roundtrip "$BUILD/v/$nm.dat" "$nm"
  [ "$RC" = 0 ] || { relay_lines < "$EV"; fail "[5] $nm: the run failed"; }
  want_event "[5] $nm" "foh_persist: loaded"
  # loaded AND unchanged: the known rows all survived, and the row this
  # build cannot know is simply not re-emitted.
  cmp "$D/mlfk-persist.dat" "$BUILD/seed.dat" \
    || fail "[5] $nm: a file carrying one unknown row did not re-save as the
   same record — the skip is not clean"
done
# NOT permissive: a line that is not shaped like a row is still corruption.
variant garbage 'append=GARBAGE'
run roundtrip "$BUILD/v/garbage.dat" garbage
want_event "[5] garbage" "foh_persist: reset cause=corrupt detail=grammar"
teeth=$((teeth + 1))
echo "    [5] OK: unknown rows (tail and mid-file) skipped, record intact;
    a non-row line still dies on grammar"

# --- [6] backward compatible: a MISSING row defaults (acceptance 5) ---------
# The same mechanism the retired migration arms used to hand-write, now the
# table's `since`/absent columns. Each case names the field it drops and the
# value it must come back with.
echo "=== [6] a missing row loads and takes its default"
miss() { # <name> <row-key> <dump-line-regex> <what> [pre-ops...]
  local nm="$1" k="$2" want="$3" what="$4"; shift 4
  local base="$BUILD/seed.dat"
  if [ "$#" -gt 0 ]; then
    node "$BUILD/variant.js" "$BUILD/seed.dat" "$BUILD/v/$nm-base.dat" "$@" \
      || fail "[6] $nm: could not derive the base fixture"
    base="$BUILD/v/$nm-base.dat"
  fi
  # DEAD-TOOTH GUARD, and it is not decorative: `ctlstyle`'s absent value is
  # BOX, which a file may perfectly well already hold. With the row STILL
  # PRESENT the record must NOT already show what the absence is supposed to
  # produce, or the assertion below is satisfied by a loader that ignores
  # the row entirely.
  run dump "$base" "$nm-ctl"
  want_event "[6] $nm control" "foh_persist: loaded"
  if grep -qE "$want" "$D/out.txt"; then
    fail "[6] $nm: the fixture ALREADY carries the value the missing row is
   supposed to default to (dead tooth)"
  fi
  node "$BUILD/variant.js" "$base" "$BUILD/v/$nm.dat" "drop=$k" \
    || fail "[6] $nm: could not drop the '$k' row"
  run dump "$BUILD/v/$nm.dat" "$nm"
  want_event "[6] $nm" "foh_persist: loaded"
  grep -qE "$want" "$D/out.txt" \
    || { relay_lines < "$D/out.txt"
         fail "[6] a file missing its '$k' row loaded, but $what"; }
  return 0
}
miss miss-resume resume '^resume 0$' \
  "resumeScreen is not FOH_STARTUP (nothing armed)"
miss miss-modonr modonr '^modonr 1$' "modOnR is not the ratified D30 default"
miss miss-turbo turbo '^turbo 0$' "turbo is not the authored default"
miss miss-ctlstyle ctlstyle '^ctlstyle 1$' \
  "ctlStyle is not BOX — the absent-value ruling for a file with no style" \
  'set=ctlstyle:ctlstyle 0'
# ticket #25: a v7 file written before the CSS rows existed is the case the
# owner's own device will present on the first boot after this ships, so it is
# checked as itself and not left to leg [11]'s synthetic row. `vsmode` and
# `ptype` are picked because they are the two the player would SEE come back
# wrong: the mode ribbon and whether a port is switched on.
miss miss-vsmode vsmode '^vsmode 0$' "versusMode is not stock"
miss miss-carry carry '^carry -1$' "cssCarry is not 'holding nothing'"
# ticket #26: same case, one ticket later. The owner's device will present a
# file with the CSS rows and WITHOUT these two on the first boot after this
# ships, and the answer has to be the screen an entry produces: slot 0 on the
# authored grid (menu.js:77-84's targetPointerPos reset).
miss miss-tsscur tsscur '^tsscur 0$' "tssCursor is not slot 0"
miss miss-tsspage tsspage '^tsspage 0$' "tssPage is not the AUTHORED grid"
# ...and the hand with them, at slot 0's centre — 58.0 (4834000000000000) and
# 39.5 (4043c00000000000). A migrated file whose hand and cursor disagreed
# would correct itself on tick one, so this is not cosmetic.
miss miss-tsshand tsshand '^tsshand 404d000000000000 4043c00000000000$' \
  "the target-select hand is not at slot 0's centre"
# a whole multi-line block may go missing too
variant miss-rec dropall=rec
run dump "$BUILD/v/miss-rec.dat" miss-rec
want_event "[6] rec" "foh_persist: loaded"
[ "$(grep -c '^rec [0-4] [0-9] bff0000000000000$' "$D/out.txt")" = 50 ] \
  || fail "[6] a file with no 'rec' rows did not come back with 50 fresh
   -1.0 records"
echo "    [6] OK: absent rows (scalar and whole blocks) take their documented
    values, neighbours untouched"

# --- [7] the FROZEN historical grammar, mirrored from the device check ------
# check-device-persist.sh owns these as T-H2/T-H9..T-H17 and cannot run on a
# host. They are restated here because a persistence change that quietly
# made old formats permissive would otherwise only be caught on hardware —
# which is exactly the blindness the A49 SUM-index defect got through.
echo "=== [7] historical versions keep their own frozen grammar"
hist_ok() { # <name> <from-version>
  run roundtrip "$BUILD/v/$1.dat" "h-$1"
  want_event "[7] $1" "foh_persist: loaded"
  [ "$(grep -cxF "foh_persist: migrated from=$2" "$EV")" = 1 ] \
    || { relay_lines < "$EV"
         grammar_die "[7] $1: expected exactly one 'migrated from=$2' prelude"; }
  # THE DATA-LOSS TOOTH (check-device-persist.sh T-H9's rule): a migration
  # is proven BY BYTES. A loader that discarded every setting and record
  # would still print `migrated` and `loaded`.
  cmp "$D/mlfk-persist.dat" "$BUILD/expect-$1.dat" \
    || fail "[7] $1: the migrated+saved file is not the independently built
   expectation — a setting, a binding or one of the 50 target records was
   lost carrying it forward"
  teeth=$((teeth + 1))
}
# The expectations: the seeded record with exactly the fields that version
# could not carry replaced by their documented absent values. Built by
# editing the FIXTURE, so they are independent of the loader.
#
# Ticket #25's eight rows take their FRESH-INSTALL values on a migration, and
# those are lifted from the DEFAULTS fixture rather than typed here — the
# defaults fixture builds them from foh.h's formulas (leg [1]), so there is
# still exactly one independent statement of what a fresh CSS looks like and
# this leg cannot quietly disagree with leg [3] about it.
# ticket #26's two rows join the same list for the same reason: a migrated
# file must come back on the screen an ENTRY would have opened.
V7_DEFAULT_OPS=()
for k in ptype cpudiff vsmode hand slider carry cpucarry handtype \
         tsscur tsspage tsshand; do
  dline="$(grep -m1 "^$k " "$BUILD/defaults.dat")" \
    || fail "[7] the defaults fixture carries no '$k' row"
  V7_DEFAULT_OPS+=("set=$k:$dline")
done
node "$BUILD/variant.js" "$BUILD/seed.dat" "$BUILD/expect-v6.dat" \
  'set=resume:resume 00' "${V7_DEFAULT_OPS[@]}"
node "$BUILD/variant.js" "$BUILD/expect-v6.dat" "$BUILD/expect-v5.dat" \
  'set=sel:sel 0 0 0 0'
node "$BUILD/variant.js" "$BUILD/expect-v5.dat" "$BUILD/expect-v4.dat" \
  'set=bind 0:bind 0 0 1 2 3 4 5 6 7'
node "$BUILD/variant.js" "$BUILD/expect-v4.dat" "$BUILD/expect-v3.dat" \
  'set=flash:flash 0' 'set=walljump:walljump 0' 'set=dustless:dustless 0' \
  'set=phantom:phantom 3f847ae147ae147b' \
  'set=soundslevel:soundslevel 3fe0000000000000' \
  'set=musiclevel:musiclevel 3fd3333333333333'
node "$BUILD/variant.js" "$BUILD/expect-v3.dat" "$BUILD/expect-v2.dat" \
  'set=modonr:modonr 1'
cp "$BUILD/expect-v2.dat" "$BUILD/expect-v1.dat" # v1 already carries ctlstyle 1 (BOX)
made "$BUILD/expect-v1.dat" "$BUILD/expect-v6.dat"
hist_ok v6 6
hist_ok v5 5
hist_ok v4 4
hist_ok v3 3
hist_ok v2 2
hist_ok v1 1
echo "    [7a] OK: all six older formats migrate BY BYTES (settings, all four
    bindings and all 50 target records carried forward)"
# …and they are NOT permissive. Each of these is a check-device-persist.sh
# tooth, restated: an old header carrying a NEWER row, or missing one of its
# OWN rows, is corruption — not a row to skip and not a default to fill.
hist_grammar() { # <name> <ops...>
  local n="$1"; shift
  variant "$n" "$@"
  run roundtrip "$BUILD/v/$n.dat" "hg-$n"
  want_event "[7] $n" "foh_persist: reset cause=corrupt detail=grammar"
  teeth=$((teeth + 1))
}
# EVERY case below drops "${V7_ROWS[@]}", not just `drop=resume`. Each tooth
# names ONE deviation and requires the refusal to come from it; leaving a v7
# row in a v3 fixture would make the file corrupt for a second reason, the
# refusal would arrive anyway, and the tooth would have stopped biting while
# still going green. That is exactly the disarmed-tooth failure CONTEXT.md
# records twice.
# T-H10: a v1 header WITH the newer lines is not a v1 file
hist_grammar v1-plus-ctlstyle hdr=1 "${V7_ROWS[@]}" drop=sel dropall=bind \
  "${V4_ROWS[@]}" drop=modonr
# T-H15: a v2 header WITH a modonr line
hist_grammar v2-plus-modonr hdr=2 "${V7_ROWS[@]}" drop=sel dropall=bind \
  "${V4_ROWS[@]}"
# T-H17: a v3 header WITH the v4 options block
hist_grammar v3-plus-v4 hdr=3 "${V7_ROWS[@]}" drop=sel dropall=bind
# T-H11: a v3 header WITHOUT its ctlstyle line
hist_grammar v3-no-ctlstyle hdr=3 "${V7_ROWS[@]}" drop=sel dropall=bind \
  "${V4_ROWS[@]}" drop=ctlstyle
# T-H12: a v3 header WITHOUT its modonr line
hist_grammar v3-no-modonr hdr=3 "${V7_ROWS[@]}" drop=sel dropall=bind \
  "${V4_ROWS[@]}" drop=modonr
# T-H14 (review-ctl n1): a v2 file may not claim the v3-era ctlstyle 2
hist_grammar v2-style2 hdr=2 "${V7_ROWS[@]}" drop=sel dropall=bind \
  "${V4_ROWS[@]}" drop=modonr 'set=ctlstyle:ctlstyle 2'
# ticket #25: a v6 header carrying one of the NEW v7 rows is corrupt for the
# same reason a v3 header carrying a v4 row is. Without this, "the CSS rows
# are v7-only" would be true only of the writer.
hist_grammar v6-plus-ptype hdr=6 drop=resume drop=cpudiff drop=vsmode \
  drop=hand drop=slider drop=carry drop=cpucarry drop=handtype \
  drop=tsscur drop=tsspage
# ticket #26, same rule aimed at THIS ticket's own rows: keep `tsspage` and
# strip everything else v7. Sharing the leg above would have let both new
# rows ride on `ptype` being the one actually refused.
hist_grammar v6-plus-tsspage hdr=6 drop=resume drop=ptype drop=cpudiff \
  drop=vsmode drop=hand drop=slider drop=carry drop=cpucarry \
  drop=handtype drop=tsscur drop=tsshand
echo "    [7b] OK: eight frozen-grammar refusals (a historical version neither
    skips a newer row nor defaults one of its own)"

# --- [8] the guards that are still guards -----------------------------------
# Order, domain, seal and version, restated after the rewrite. These are the
# arms the table now drives, and a table that parsed but stopped judging
# would be the worst outcome available here.
echo "=== [8] the refusals the table still makes"
refuse() { # <name> <expected event> <ops...>
  local n="$1" ev="$2"; shift 2
  variant "$n" "$@"
  run roundtrip "$BUILD/v/$n.dat" "r-$n"
  want_event "[8] $n" "foh_persist: $ev"
  teeth=$((teeth + 1))
}
refuse rec-order    "reset cause=corrupt detail=order"   swap='rec 0 3','rec 0 4'
refuse bind-order   "reset cause=corrupt detail=order"   swap='bind 0','bind 1'
refuse dup-turbo    "reset cause=corrupt detail=order"   'before=turbo 0@lcancel'
refuse rec-nan      "reset cause=corrupt detail=domain"  'set=rec 0 3:rec 0 3 7ff8000000000000'
refuse rec-cap      "reset cause=corrupt detail=domain"  'set=rec 0 3:rec 0 3 40b7700000000000'
refuse phantom-big  "reset cause=corrupt detail=domain"  'set=phantom:phantom 408f400000000001'
refuse level-big    "reset cause=corrupt detail=domain"  'set=soundslevel:soundslevel 3ff0000000000001'
refuse sel-bad      "reset cause=corrupt detail=domain"  'set=sel:sel 1 2 3 5'
refuse bind-dup     "reset cause=corrupt detail=domain"  'set=bind 0:bind 0 3 1 0 2 5 4 7 7'
refuse resume-match "reset cause=corrupt detail=domain"  'set=resume:resume 13'
refuse bind-slot8   "reset cause=corrupt detail=grammar" 'set=bind 0:bind 0 3 1 0 2 5 4 7 8'
refuse turbo-2      "reset cause=corrupt detail=grammar" 'set=turbo:turbo 2'
refuse lcancel-3    "reset cause=corrupt detail=grammar" 'set=lcancel:lcancel 3'
refuse stale-sum    "reset cause=corrupt detail=sum" \
  'set=rec 0 3:rec 0 3 402e000000000000' stale-sum
refuse truncated    "reset cause=corrupt detail=truncated" trunc=900
refuse v8           "reset cause=version" hdr=8
refuse v0           "reset cause=version" hdr=0
refuse v01          "reset cause=version" hdr=01
refuse bad-header   "reset cause=corrupt detail=header" hdr=7x
# ticket #25's rows, each judged in its OWN column. `ptype`'s digit is
# playerType + 1 with three legal values, so a 3 is out of column; `cpudiff`'s
# is the level - 1 with four, so a 4 is; `carry` has five (nothing, or one of
# four ports). The two `hand` cases matter most: a NaN or an off-canvas cursor
# hit-tests nothing at all, which is a CSS the player cannot use, so they are
# corruption rather than something to clamp back onto the screen.
refuse ptype-bad    "reset cause=corrupt detail=grammar" 'set=ptype:ptype 1 3 0 2'
refuse cpudiff-bad  "reset cause=corrupt detail=grammar" 'set=cpudiff:cpudiff 2 4 3 1'
refuse carry-bad    "reset cause=corrupt detail=grammar" 'set=carry:carry 5'
refuse handtype-bad "reset cause=corrupt detail=grammar" 'set=handtype:handtype 3'
# ticket #26's two rows, each judged in ITS OWN column and by its own rule.
#   * `tsscur 11` is GRAMMATICAL — two digits is two digits — and outside the
#     eleven slots the screen has, so it is `domain`. That distinction is the
#     point of giving FP_U2 a bound at all: without one, 11 would have been
#     accepted and the resumed screen would ring a slot that is not there.
#   * `tsscur 5` is the WIDTH failing: a one-digit value makes the line short
#     and the anchored parse never reaches a domain question. It is the tooth
#     that would notice `tsscur` being demoted to a one-digit flag row.
#   * `tsspage 2` is a flag outside its digit domain, i.e. `grammar`, exactly
#     like `handtype 3` above.
refuse tsscur-bad "reset cause=corrupt detail=domain" 'set=tsscur:tsscur 11'
refuse tsscur-narrow "reset cause=corrupt detail=grammar" 'set=tsscur:tsscur 5'
refuse tsspage-bad "reset cause=corrupt detail=grammar" 'set=tsspage:tsspage 2'
# and the hand takes FP_DOM_SCREEN like the CSS one: a NaN or an off-canvas
# cursor hit-tests nothing, and on THIS screen that also freezes the
# selection wherever it last was.
refuse tsshand-nan "reset cause=corrupt detail=domain" \
  'set=tsshand:tsshand 7ff8000000000000 4043c00000000000'
refuse tsshand-offscreen "reset cause=corrupt detail=domain" \
  'set=tsshand:tsshand 404d000000000000 406e200000000000'
refuse hand-nan     "reset cause=corrupt detail=domain" \
  'set=hand:hand 7ff8000000000000 406e000000000000'
refuse hand-offscreen "reset cause=corrupt detail=domain" \
  'set=hand:hand 406e200000000000 406e000000000000'
refuse slider-neg   "reset cause=corrupt detail=domain" \
  'set=slider:slider bff0000000000000 4031c00000000000 4039000000000000 4040200000000000'
run roundtrip - missing
want_event "[8] missing" "foh_persist: reset cause=missing"
teeth=$((teeth + 1))
# INCLUSIVE bounds are still inclusive: a value exactly at the cap is VALID,
# and a gate that rejects a legitimate save is a defect of its own
# (review-r12's rule, restated for the table).
refuse_not() { # <name> <ops...>
  local n="$1"; shift
  variant "$n" "$@"
  run roundtrip "$BUILD/v/$n.dat" "ok-$n"
  want_event "[8] $n" "foh_persist: loaded"
}
refuse_not phantom-cap 'set=phantom:phantom 408f400000000000'
refuse_not level-one   'set=soundslevel:soundslevel 3ff0000000000000'
# the hand's own inclusive ends — foh_hand_step clamps TO 0 and TO the canvas
# width, so both are positions the player can actually park the cursor at and
# a gate that rejected either would reset a legitimate save.
refuse_not hand-zero   'set=hand:hand 0000000000000000 0000000000000000'
refuse_not hand-cap    'set=hand:hand 406e000000000000 406e000000000000'
echo "    [8] OK: order, domain, permutation, seal, version and header
    refusals all still bite; the inclusive caps still load"

# --- [9] THE STATIC ASSERTION (acceptance 3) --------------------------------
# ADR 0001: "the guard asserts an outcome, and a check must prove the guard
# still bites". A COPY of the header gains one field and the build must
# FAIL. The tree is untouched.
echo "=== [9] adding a field without a decision must FAIL THE BUILD"
mkcopy() { # <dir> — a private copy of the two persist TUs
  rm -rf "$BUILD/$1"; mkdir -p "$BUILD/$1"
  cp "$HDR" "$SRC" "$WIT" "$BUILD/$1/"
}
build_copy() { # <dir> -> RC, log at $BUILD/<dir>/cc.log
  set +e
  cc "${CFLAGS[@]}" -I"$BUILD/$1" -Iport/foh -o "$BUILD/$1/pw" \
    "$BUILD/$1/foh_persist_witness.c" "$BUILD/$1/foh_persist.c" \
    "${LINK[@]}" -lm > "$BUILD/$1/cc.log" 2>&1
  RC=$?
  set -e
}
# the copy must build UNPERTURBED, or every "did not build" below is a lie
mkcopy c-base
build_copy c-base
[ "$RC" = 0 ] || { relay_lines < "$BUILD/c-base/cc.log"
  fail "[9] an UNPERTURBED copy did not build (dead tooth: the negative
   builds below would pass for the wrong reason)"; }
# the copy must also behave: same bytes as the real tree
D=$BUILD/c-base/persist; rm -rf "$D"; mkdir -p "$D"
MLFK_PERSIST_DIR="$PWD/$D" "$BUILD/c-base/pw" seed 2>/dev/null
cmp "$D/mlfk-persist.dat" "$BUILD/seed.dat" \
  || fail "[9] the copy does not produce the same bytes as the real tree"

add_field() { # <dir> <declaration>
  mkcopy "$1"
  node -e '
    const fs = require("fs");
    const [f, decl] = process.argv.slice(1);
    const raw = fs.readFileSync(f, "utf8");
    const anchor = "  int layoutGuard;\n";
    if (raw.split(anchor).length - 1 !== 1) {
      console.error("anchor |" + anchor.trim() + "| is not unique");
      process.exit(1);
    }
    fs.writeFileSync(f, raw.replace(anchor, anchor + "  " + decl + "\n"));
  ' "$BUILD/$1/foh_persist.h" "$2" || fail "[9] $1: could not add the field"
  cmp -s "$BUILD/$1/foh_persist.h" "$HDR" \
    && fail "[9] $1: the perturbed header is byte-identical (dead tooth)"
  return 0
}
add_field c-newint 'int newFieldNobodyDecidedAbout;'
build_copy c-newint
[ "$RC" != 0 ] \
  || fail "[9] adding an int to FohPersist COMPILED. The static assertion is
   dead: a new field would be silently unpersisted, which is the exact defect
   ADR 0001 exists to prevent."
grep -q 'FohPersist changed size' "$BUILD/c-newint/cc.log" \
  || { relay_lines < "$BUILD/c-newint/cc.log"
       fail "[9] the build failed for some OTHER reason than the static
   assertion — an unrelated failure is being credited as the guard biting"; }
teeth=$((teeth + 1))
add_field c-newdouble 'double newDoubleNobodyDecidedAbout;'
build_copy c-newdouble
[ "$RC" != 0 ] && grep -q 'FohPersist changed size' "$BUILD/c-newdouble/cc.log" \
  || fail "[9] adding a double to FohPersist did not trip the static assertion"
teeth=$((teeth + 1))
# THE GUARD MEMBER EARNS ITS PLACE. Remove `layoutGuard` (and the four bytes
# it accounts for) and the SAME added int becomes invisible: it lands in the
# tail alignment padding, sizeof does not move, and the assertion says
# nothing. This is the counterfactual that makes leg [9] a proof rather than
# a coincidence — and the reason foh_persist.h's guard member is not cargo.
mkcopy c-nogaurd
node -e '
  const fs = require("fs");
  const [h, c] = process.argv.slice(1);
  let H = fs.readFileSync(h, "utf8");
  const decl = "  int layoutGuard;\n";
  if (H.split(decl).length - 1 !== 1) { console.error("no guard decl"); process.exit(1); }
  H = H.replace(decl, "");
  fs.writeFileSync(h, H);
  let C = fs.readFileSync(c, "utf8");
  const u = "#define FP_UNPERSISTED_BYTES (sizeof(int) /* layoutGuard */)";
  if (C.split(u).length - 1 !== 1) { console.error("no FP_UNPERSISTED_BYTES"); process.exit(1); }
  C = C.replace(u, "#define FP_UNPERSISTED_BYTES (sizeof(int) /* tail pad */)");
  fs.writeFileSync(c, C);
' "$BUILD/c-nogaurd/foh_persist.h" "$BUILD/c-nogaurd/foh_persist.c" \
  || fail "[9] could not build the no-guard counterfactual"
build_copy c-nogaurd
[ "$RC" = 0 ] || { relay_lines < "$BUILD/c-nogaurd/cc.log"
  fail "[9] the no-guard counterfactual did not build — it must, or the
   comparison below is meaningless"; }
node -e '
  const fs = require("fs");
  const f = process.argv[1];
  const raw = fs.readFileSync(f, "utf8");
  const a = "  int resumeScreen;\n";
  if (raw.split(a).length - 1 !== 1) { console.error("no anchor"); process.exit(1); }
  fs.writeFileSync(f, raw.replace(a, a + "  int newFieldNobodyDecidedAbout;\n"));
' "$BUILD/c-nogaurd/foh_persist.h" || fail "[9] counterfactual edit failed"
build_copy c-nogaurd
[ "$RC" = 0 ] \
  || fail "[9] the no-guard counterfactual REFUSED the added int, so removing
   layoutGuard costs nothing and the guard member is cargo — re-derive it"
echo "    [9] OK: a new int and a new double each fail the build with the
    table's message; without layoutGuard the same int is INVISIBLE, which is
    what that member is for"

# --- [10] a pointer field is RECONSTRUCTED, never copied (acceptance 6) -----
# The table has no kind that reads a pointer out of a file, so a pointer
# field cannot be persisted by accident: it must be given the FP_RECON kind,
# which emits nothing and parses nothing. Proven by adding one.
echo "=== [10] a pointer field takes FP_RECON and changes no bytes"
mkcopy c-ptr
cat > "$BUILD/addrow.js" <<'EOF'
// Adds ONE member to FohPersist and ONE FP_FIELDS row, each as exactly one
// physical line, so "the cost of a persisted field is one row" can be
// judged by counting diff lines rather than by eye.
const fs = require("fs");
const [h, c, decl, row] = process.argv.slice(2);
const H = fs.readFileSync(h, "utf8").split("\n");
const hi = H.findIndex((l) => l === "  int layoutGuard;");
if (hi < 0) { console.error("addrow: no layoutGuard anchor"); process.exit(1); }
// AFTER the guard, not before: a member that raises the struct's alignment
// would otherwise open an internal padding hole, which FP_UNPERSISTED_BYTES
// would then have to declare — a real cost, but not the one being measured
// here.
H.splice(hi + 1, 0, "  " + decl);
fs.writeFileSync(h, H.join("\n"));
if (row) {
  const C = fs.readFileSync(c, "utf8").split("\n");
  const ci = C.findIndex((l) => l.startsWith("  X(resumeScreen,"));
  if (ci < 0) { console.error("addrow: no FP_FIELDS anchor"); process.exit(1); }
  C.splice(ci, 0, "  " + row + " \\");
  fs.writeFileSync(c, C.join("\n"));
}
EOF
node "$BUILD/addrow.js" "$BUILD/c-ptr/foh_persist.h" "$BUILD/c-ptr/foh_persist.c" \
  'const char *reconstructedThing;' \
  'X(reconstructedThing, .key = "recon", .kind = FP_RECON, .since = 7)' \
  || fail "[10] could not derive the pointer-field copy"
build_copy c-ptr
[ "$RC" = 0 ] || { relay_lines < "$BUILD/c-ptr/cc.log"
  fail "[10] a pointer field with an FP_RECON row did not build — the
   reconstructed kind does not actually account for it"; }
D=$BUILD/c-ptr/persist; rm -rf "$D"; mkdir -p "$D"
MLFK_PERSIST_DIR="$PWD/$D" "$BUILD/c-ptr/pw" seed 2>/dev/null
cmp "$D/mlfk-persist.dat" "$BUILD/seed.dat" \
  || fail "[10] a build carrying a pointer field wrote DIFFERENT bytes — the
   pointer reached the file, which is the stale-address trap ADR 0001 refuses"
cp "$BUILD/seed.dat" "$D/mlfk-persist.dat"
MLFK_PERSIST_DIR="$PWD/$D" "$BUILD/c-ptr/pw" roundtrip 2>/dev/null >/dev/null
cmp "$D/mlfk-persist.dat" "$BUILD/seed.dat" \
  || fail "[10] the pointer build's round trip is not byte-identical"
teeth=$((teeth + 1))
# …and WITHOUT the row it does not build, so the pointer cannot be forgotten.
mkcopy c-ptr-norow
node "$BUILD/addrow.js" "$BUILD/c-ptr-norow/foh_persist.h" \
  "$BUILD/c-ptr-norow/foh_persist.c" 'const char *reconstructedThing;' '' \
  || fail "[10] could not derive the no-row copy"
build_copy c-ptr-norow
[ "$RC" != 0 ] && grep -q 'FohPersist changed size' "$BUILD/c-ptr-norow/cc.log" \
  || fail "[10] a pointer field with NO table row compiled — it would be
   silently unpersisted, which is the gap this ticket closes"
teeth=$((teeth + 1))
echo "    [10] OK: FP_RECON accounts for a pointer and writes nothing;
    omitting the row fails the build"

# --- [11] adding a persisted field costs ONE ROW (acceptance 2) -------------
# Done by doing it: a copy gains one field and exactly one FP_FIELDS row —
# no parser edit, no writer edit, no migration arm, no version bump — and
# the new row round-trips while an OLDER file (which lacks it) defaults.
echo "=== [11] a new persisted field costs exactly one table row"
mkcopy c-newrow
node "$BUILD/addrow.js" "$BUILD/c-newrow/foh_persist.h" \
  "$BUILD/c-newrow/foh_persist.c" 'int brandNewSetting;' \
  'X(brandNewSetting, .key = "brandnew", .kind = FP_FLAG, .vals = 1, .dmax = 2, .since = 7)' \
  || fail "[11] could not derive the one-row copy"
# EXACTLY ONE line added and NOTHING removed. This is the ticket's premise,
# counted rather than asserted: no parser arm, no writer arm, no migration
# arm, no version bump — one row.
nadd="$(diff "$SRC" "$BUILD/c-newrow/foh_persist.c" | grep -c '^>')" || true
ndel="$(diff "$SRC" "$BUILD/c-newrow/foh_persist.c" | grep -c '^<')" || true
[ "$nadd" = 1 ] && [ "$ndel" = 0 ] \
  || fail "[11] persisting the new field took $nadd added and $ndel removed
   lines of foh_persist.c (want 1 and 0). If a field costs more than one
   row, the ticket's premise is not met"
build_copy c-newrow
[ "$RC" = 0 ] || { relay_lines < "$BUILD/c-newrow/cc.log"
  fail "[11] adding one field and one row did not build"; }
D=$BUILD/c-newrow/persist; rm -rf "$D"; mkdir -p "$D"
# the new build writes the new row…
MLFK_PERSIST_DIR="$PWD/$D" "$BUILD/c-newrow/pw" seed 2>/dev/null
grep -qx 'brandnew 0' "$D/mlfk-persist.dat" \
  || { relay_lines < "$D/mlfk-persist.dat"; fail "[11] the new row was not written"; }
# …and an OLD file (this tree's own fixture, which lacks it) still loads and
# takes the default — no migration arm, no version bump.
cp "$BUILD/seed.dat" "$D/mlfk-persist.dat"
set +e
MLFK_PERSIST_DIR="$PWD/$D" "$BUILD/c-newrow/pw" roundtrip \
  > "$D/out.txt" 2> "$D/err.txt"
RC=$?
set -e
[ "$RC" = 0 ] || { relay_lines < "$D/err.txt"; fail "[11] the old file did not load"; }
[ "$(grep -cxF 'foh_persist: loaded' "$D/err.txt")" = 1 ] \
  || { relay_lines < "$D/err.txt"
       fail "[11] a file predating the new row did not simply LOAD — the
   version bump is not actually retired"; }
[ "$(grep -c '^foh_persist: migrated' "$D/err.txt")" = 0 ] \
  || fail "[11] the old file needed a MIGRATION — the whole point is that it
   does not"
grep -qx 'brandnew 0' "$D/mlfk-persist.dat" \
  || fail "[11] the re-saved file does not carry the new row at its default"
# and THIS tree still reads THAT file, ignoring the row it cannot know
D2=$BUILD/run/newrow-back; rm -rf "$D2"; mkdir -p "$D2"
cp "$D/mlfk-persist.dat" "$D2/mlfk-persist.dat"
MLFK_PERSIST_DIR="$PWD/$D2" "$BUILD/pw" roundtrip > /dev/null 2> "$D2/err.txt"
[ "$(grep -cxF 'foh_persist: loaded' "$D2/err.txt")" = 1 ] \
  || { relay_lines < "$D2/err.txt"
       fail "[11] this build could not read a file written by the build with
   one more row — forward compatibility is not real"; }
cmp "$D2/mlfk-persist.dat" "$BUILD/seed.dat" \
  || fail "[11] reading the newer file lost or changed a known field"
teeth=$((teeth + 1))
echo "    [11] OK: one field + one row, both directions read both files, no
    migration arm and no version bump"

# --- [12] a NEW SCREEN must decide about the RESUME HOOK (ticket #26) -------
#
# ADR 0001 keeps one half of the rejected per-screen interface: a screen may
# carry a HOOK that re-derives what a field table cannot hold. The whole
# argument for putting it on foh_persist_resume_plan()'s switch rather than
# in a registry of its own is that THAT switch already fails the build when a
# screen arrives without a decision — "the one place that must think about
# it", and its exhaustiveness has already caught a real gap when two lanes
# merged. An inherited property is worth exactly as much as the check that
# proves it still holds, so this leg adds a screen to a COPY of foh.h and
# requires the build to fail naming that screen. The tree is untouched.
echo "=== [12] adding a screen without deciding about the hook must FAIL"
FOHH=port/foh/foh.h
made "$FOHH"
mkcopy_screen() { # <dir> — the persist pair PLUS a private copy of foh.h
  mkcopy "$1"
  cp "$FOHH" "$BUILD/$1/"
}
# The control: the same copy set, unperturbed, must build. Without this every
# "did not build" below could be the copied foh.h failing to resolve its own
# relative includes.
mkcopy_screen c-screen-base
build_copy c-screen-base
[ "$RC" = 0 ] || { relay_lines < "$BUILD/c-screen-base/cc.log"
  fail "[12] the UNPERTURBED copy (with foh.h copied in) did not build — the
   negative below would pass for the wrong reason"; }
mkcopy_screen c-screen
node -e '
  const fs = require("fs");
  const f = process.argv[1];
  const raw = fs.readFileSync(f, "utf8");
  const anchor = "  FOH_SCREEN_COUNT\n";
  if (raw.split(anchor).length - 1 !== 1) {
    console.error("anchor |" + anchor.trim() + "| is not unique in foh.h");
    process.exit(1);
  }
  fs.writeFileSync(f, raw.replace(anchor, "  FOH_SCREEN_NOBODY_DECIDED,\n" + anchor));
' "$BUILD/c-screen/foh.h" || fail "[12] could not add a screen to the copy"
cmp -s "$BUILD/c-screen/foh.h" "$FOHH" \
  && fail "[12] the perturbed foh.h is byte-identical (dead tooth)"
build_copy c-screen
[ "$RC" != 0 ] \
  || fail "[12] adding a screen to FohScreen COMPILED. The resume map is no
   longer exhaustive, so a new screen would silently take a default target
   and NO hook — the gap ADR 0001 puts the hook here to prevent."
grep -q 'FOH_SCREEN_NOBODY_DECIDED' "$BUILD/c-screen/cc.log" \
  || { relay_lines < "$BUILD/c-screen/cc.log"
       fail "[12] the build failed for some OTHER reason than the unhandled
   screen — an unrelated failure is being credited as the guard biting"; }
teeth=$((teeth + 1))
# THE COUNTERFACTUAL: it is the ENUMERATED arms that bite, not the compiler
# being clever. Give the switch a `default:` and the same added screen becomes
# invisible — which is precisely the shape a per-screen registry would have
# had, and the reason the hook is a column here instead.
mkcopy_screen c-screen-default
node -e '
  const fs = require("fs");
  const [h, c] = process.argv.slice(1);
  let H = fs.readFileSync(h, "utf8");
  const anchor = "  FOH_SCREEN_COUNT\n";
  if (H.split(anchor).length - 1 !== 1) { console.error("no enum anchor"); process.exit(1); }
  fs.writeFileSync(h, H.replace(anchor, "  FOH_SCREEN_NOBODY_DECIDED,\n" + anchor));
  let C = fs.readFileSync(c, "utf8");
  const a = "    case FOH_SCREEN_COUNT: break;\n";
  if (C.split(a).length - 1 !== 1) { console.error("no switch anchor"); process.exit(1); }
  fs.writeFileSync(c, C.replace(a, a + "    default: break;\n"));
' "$BUILD/c-screen-default/foh.h" "$BUILD/c-screen-default/foh_persist.c" \
  || fail "[12] could not build the defaulted counterfactual"
build_copy c-screen-default
[ "$RC" = 0 ] \
  || { relay_lines < "$BUILD/c-screen-default/cc.log"
       fail "[12] the defaulted counterfactual REFUSED the added screen, so
   the enumerated arms are not what makes the map bite — re-derive this leg"; }
teeth=$((teeth + 1))
echo "    [12] OK: a new screen fails the build in the resume map; with a
    default arm the same screen is INVISIBLE, which is what the enumeration
    is for"

# --- no-commit guard ---------------------------------------------------------
git_dirty_after="$(tree_fingerprint)" || fail "post-run fingerprint failed"
[ "$git_dirty_before" = "$git_dirty_after" ] \
  || fail "this check modified tracked files — must not"

echo "PERSIST TABLE OK ($teeth teeth fired)"
