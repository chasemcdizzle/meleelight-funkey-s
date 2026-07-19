#!/usr/bin/env node
// port/sim/device/skip-attrib/correlate-skips.js — M4 task 8: host-side
// correlator/judge for the skip-stall attribution instrument. The
// device only RECORDS (gfx_app --attrib rows, sk_sampler kernel-counter
// snapshots, pre/post /proc dumps pulled by the check script); ALL
// judgment happens here (M3 convention).
//
// Usage:
//   node correlate-skips.js --timing t.txt --attrib a.txt --frames N
//     --budget-ns B --pre-dir D1 --post-dir D2 [--sampler s.txt]
//
// GRAMMARS (PROCESS §3 whitelist rule — every input is parsed through
// anchored full-line/length-prefixed patterns measured from the real
// producers; resembles-but-doesn't-match = corruption = exit 3):
//   timing:  exactly N lines  ^<sim> <render> <present> <0|1>$
//            (gfx_app --timing; judge-render-timing.js's grammar)
//   attrib:  exactly N+1 lines ^<mono> <raw> <nvcsw> <nivcsw> <minflt>
//            <majflt>$  — mono/raw strictly increasing, counters
//            non-decreasing (gfx_app --attrib; cumulative)
//   sampler: SAMPLE/FILE length-prefixed blocks + the SAMPLER DONE
//            terminator (sk_sampler.c); ANY overflow count > 0 dies
//   pidtab:  raw /proc/<pid>/stat lines (comm parsed via last ')')
//   interrupts/stat/softirqs/vmstat: the kernel's own formats
//
// EVENT (pre-registered, AGENT-LOG iter 74): a frame that skipped, OR
// whose work time (sim+render+present) exceeded the budget, OR whose
// START was > 2 ms late against the absolute pacing schedule (stalls
// landing in the pacing sleep are invisible to the work buckets).
//
// Output: strict key=value lines + detail records ("EV|", "PID|",
// "IRQ|", "SIRQ|", "VM|" prefixed) + the attrib_complete=1 integrity
// terminator (iter-62 judge pattern; consumers assert the FILE tail).
"use strict";
const fs = require("fs");

function die(msg) {
  console.error("correlate-skips: " + msg);
  process.exit(3);
}

// --- args ------------------------------------------------------------------
const args = process.argv.slice(2);
const opt = {};
for (let i = 0; i < args.length; i++) {
  const a = args[i];
  const need = () => {
    if (i + 1 >= args.length) die("missing value for " + a);
    return args[++i];
  };
  if (a === "--timing") opt.timing = need();
  else if (a === "--attrib") opt.attrib = need();
  else if (a === "--frames") opt.frames = need();
  else if (a === "--budget-ns") opt.budget = need();
  else if (a === "--pre-dir") opt.pre = need();
  else if (a === "--post-dir") opt.post = need();
  else if (a === "--sampler") opt.sampler = need();
  else die("bad argument " + a);
}
for (const k of ["timing", "attrib", "frames", "budget", "pre", "post"]) {
  if (!opt[k]) die("required argument --" + (k === "budget" ? "budget-ns" : k) + " missing");
}
if (!/^[0-9]{1,7}$/.test(opt.frames)) die("--frames not a bounded decimal");
if (!/^[0-9]{1,12}$/.test(opt.budget)) die("--budget-ns not a bounded decimal");
const FRAMES = parseInt(opt.frames, 10);
const BUDGET = parseInt(opt.budget, 10);
if (FRAMES <= 0 || BUDGET <= 0) die("frames/budget must be positive");
const LATE_START_NS = 2000000; // pre-registered event threshold (2 ms)

function readLines(path, what) {
  let raw;
  try {
    raw = fs.readFileSync(path, "utf8");
  } catch (e) {
    die("cannot read " + what + " " + path + ": " + e.message);
  }
  if (raw.length === 0) die(what + " file is empty: " + path);
  if (raw[raw.length - 1] !== "\n") die(what + " does not end with a newline: " + path);
  const lines = raw.split("\n");
  lines.pop();
  return lines;
}

// --- timing ----------------------------------------------------------------
const timingLines = readLines(opt.timing, "timing");
if (timingLines.length !== FRAMES) {
  die("timing has " + timingLines.length + " lines, expected " + FRAMES);
}
const TROW = /^([0-9]+) ([0-9]+) ([0-9]+) ([01])$/;
const tim = new Array(FRAMES);
let skips = 0;
for (let i = 0; i < FRAMES; i++) {
  const m = TROW.exec(timingLines[i]);
  if (!m) die("timing line " + (i + 1) + " malformed: " + JSON.stringify(timingLines[i]));
  const s = parseInt(m[1], 10), r = parseInt(m[2], 10), p = parseInt(m[3], 10);
  if (!Number.isSafeInteger(s) || !Number.isSafeInteger(r) || !Number.isSafeInteger(p)) {
    die("timing line " + (i + 1) + " exceeds safe integer range");
  }
  const skipped = m[4] === "1";
  if (skipped && (r !== 0 || p !== 0)) die("timing line " + (i + 1) + " skipped but render/present nonzero");
  if (skipped) skips++;
  tim[i] = { sim: s, render: r, present: p, full: s + r + p, skipped };
}

// --- attrib ----------------------------------------------------------------
const attribLines = readLines(opt.attrib, "attrib");
if (attribLines.length !== FRAMES + 1) {
  die("attrib has " + attribLines.length + " lines, expected " + (FRAMES + 1));
}
const AROW = /^([0-9]+) ([0-9]+) ([0-9]+) ([0-9]+) ([0-9]+) ([0-9]+)$/;
const att = new Array(FRAMES + 1);
for (let i = 0; i <= FRAMES; i++) {
  const m = AROW.exec(attribLines[i]);
  if (!m) die("attrib line " + (i + 1) + " malformed: " + JSON.stringify(attribLines[i]));
  const row = {
    mono: Number(m[1]), raw: Number(m[2]), nvcsw: Number(m[3]),
    nivcsw: Number(m[4]), minflt: Number(m[5]), majflt: Number(m[6]),
  };
  for (const k of Object.keys(row)) {
    if (!Number.isSafeInteger(row[k])) die("attrib line " + (i + 1) + " field " + k + " exceeds safe integer range");
  }
  if (i > 0) {
    const p = att[i - 1];
    if (row.mono <= p.mono || row.raw <= p.raw) die("attrib clocks not strictly increasing at line " + (i + 1));
    for (const k of ["nvcsw", "nivcsw", "minflt", "majflt"]) {
      if (row[k] < p[k]) die("attrib cumulative counter " + k + " decreased at line " + (i + 1));
    }
  }
  att[i] = row;
}

// --- sampler (optional) ----------------------------------------------------
// Block grammar over RAW BYTES (length-prefixed): SAMPLE header, three
// FILE blocks in pinned order, terminator with matching count.
let samples = null;
if (opt.sampler) {
  let buf;
  try {
    buf = fs.readFileSync(opt.sampler);
  } catch (e) {
    die("cannot read sampler " + opt.sampler + ": " + e.message);
  }
  samples = [];
  let off = 0;
  const nextLine = () => {
    const nl = buf.indexOf(0x0a, off);
    if (nl < 0) die("sampler: unterminated line at offset " + off);
    const line = buf.toString("utf8", off, nl);
    off = nl + 1;
    return line;
  };
  const FILE_ORDER = ["stat", "interrupts", "softirqs"];
  let done = false, doneCount = -1, doneOverflows = -1;
  while (off < buf.length) {
    const line = nextLine();
    let m;
    if ((m = /^SAMPLE ([0-9]+) ([0-9]+)$/.exec(line))) {
      const idx = parseInt(m[1], 10);
      if (idx !== samples.length) die("sampler: SAMPLE index " + idx + " out of order (expected " + samples.length + ")");
      const sample = { t: Number(m[2]), files: {} };
      if (!Number.isSafeInteger(sample.t)) die("sampler stamp exceeds safe integer range");
      for (const fname of FILE_ORDER) {
        const fl = nextLine();
        const fm = /^FILE ([a-z]+) ([0-9]+)$/.exec(fl);
        if (!fm || fm[1] !== fname) die("sampler: expected 'FILE " + fname + " <bytes>', got " + JSON.stringify(fl));
        const n = parseInt(fm[2], 10);
        if (off + n > buf.length) die("sampler: FILE block runs past EOF");
        sample.files[fname] = buf.toString("utf8", off, off + n);
        off += n;
      }
      samples.push(sample);
    } else if ((m = /^SAMPLER DONE ([0-9]+) ([0-9]+)$/.exec(line))) {
      done = true;
      doneCount = parseInt(m[1], 10);
      doneOverflows = parseInt(m[2], 10);
      if (off !== buf.length) die("sampler: bytes after the SAMPLER DONE terminator");
    } else {
      die("sampler: unrecognized line " + JSON.stringify(line));
    }
  }
  if (!done) die("sampler: missing SAMPLER DONE terminator (truncated capture)");
  if (doneCount !== samples.length) die("sampler: DONE count " + doneCount + " != parsed blocks " + samples.length);
  if (doneOverflows !== 0) die("sampler: " + doneOverflows + " overflowed reads — clipped counter tables cannot be judged");
  if (samples.length < 2) die("sampler: fewer than 2 samples — no window deltas possible");
  for (let i = 1; i < samples.length; i++) {
    if (samples[i].t <= samples[i - 1].t) die("sampler stamps not strictly increasing at block " + i);
  }
  // iter 76 (review-73 M): validate EVERY payload UNCONDITIONALLY —
  // previously raw payloads were parsed only when an event happened to
  // be bracketed, so a zero-event (or sparse-event) run could carry
  // malformed length-valid payloads all the way to the terminator.
  // Every /proc snapshot the sampler recorded must satisfy the same
  // strict kernel-format grammars the window lookups use.
  for (let i = 0; i < samples.length; i++) {
    parseStat(samples[i].files.stat, "sampler[" + i + "]");
    parseInterrupts(samples[i].files.interrupts, "sampler[" + i + "]");
    parseSoftirqs(samples[i].files.softirqs, "sampler[" + i + "]");
  }
}

// --- kernel-format parsers -------------------------------------------------
function parseInterrupts(text, what) {
  const map = new Map();
  const lines = text.split("\n").filter((l) => l.length > 0);
  if (lines.length < 2) die(what + ": interrupts table too short");
  if (!/^\s+CPU0\s*$/.test(lines[0])) die(what + ": interrupts header is not the single-CPU form: " + JSON.stringify(lines[0]));
  for (let i = 1; i < lines.length; i++) {
    const m = /^\s*([A-Za-z0-9]+):\s+([0-9]+)(?:\s+(.*))?$/.exec(lines[i]);
    if (!m) die(what + ": interrupts line malformed: " + JSON.stringify(lines[i]));
    const label = m[3] ? m[1] + ":" + m[3].trim().replace(/\s+/g, "_") : m[1];
    if (map.has(label)) die(what + ": duplicate interrupts label " + label);
    map.set(label, parseInt(m[2], 10));
  }
  return map;
}

function parseStat(text, what) {
  // FULL-LINE whitelist grammar (iter 76, review-73 M — the old intr
  // rule was a PREFIX match and cpuN/btime/softirq were prefix-skipped;
  // duplicates silently overwrote). Measured producer corpus: kernel
  // 4.14.14-funkey single-core /proc/stat (pre/post snapshots + every
  // sk_sampler payload): 'cpu' aggregate = 'cpu' + TWO spaces + exactly
  // 10 single-spaced counters; exactly one 'cpu0' mirror (10 counters);
  // 'intr' = total + per-source list; 'softirq' = total + exactly 10
  // class counters; singletons ctxt/btime/processes/procs_running/
  // procs_blocked. Anything else — including any other cpuN (this is a
  // pinned single-core device) or a duplicate of any class — is
  // corruption -> loud death.
  const out = { cpu: null, ctxt: null, processes: null, procs_running: null, procs_blocked: null, intr: null };
  const seen = new Set();
  const once = (k) => {
    if (seen.has(k)) die(what + ": duplicate stat line class '" + k + "' (concatenated/corrupt snapshot)");
    seen.add(k);
  };
  for (const line of text.split("\n")) {
    if (line.length === 0) continue;
    let m;
    if ((m = /^cpu {2}([0-9]+(?: [0-9]+){9})$/.exec(line))) {
      once("cpu");
      const f = m[1].split(" ").map((x) => parseInt(x, 10));
      out.cpu = { user: f[0], nice: f[1], system: f[2], idle: f[3], iowait: f[4], irq: f[5], softirq: f[6], steal: f[7] };
    } else if (/^cpu0 [0-9]+(?: [0-9]+){9}$/.test(line)) once("cpu0");
    else if ((m = /^ctxt ([0-9]+)$/.exec(line))) { once("ctxt"); out.ctxt = parseInt(m[1], 10); }
    else if (/^btime [0-9]+$/.test(line)) once("btime");
    else if ((m = /^processes ([0-9]+)$/.exec(line))) { once("processes"); out.processes = parseInt(m[1], 10); }
    else if ((m = /^procs_running ([0-9]+)$/.exec(line))) { once("procs_running"); out.procs_running = parseInt(m[1], 10); }
    else if ((m = /^procs_blocked ([0-9]+)$/.exec(line))) { once("procs_blocked"); out.procs_blocked = parseInt(m[1], 10); }
    else if ((m = /^intr ([0-9]+)((?: [0-9]+)+)$/.exec(line))) { once("intr"); out.intr = parseInt(m[1], 10); }
    else if (/^softirq [0-9]+(?: [0-9]+){10}$/.test(line)) once("softirq");
    else die(what + ": stat line unrecognized: " + JSON.stringify(line));
  }
  // iter 78 (review-76 M2 — REQUIRED-SET completion): the fixed table
  // is ALL NINE line classes on this pinned single-core kernel
  // (4.14.14-funkey, measured corpus) — a clipped snapshot missing ANY
  // of them (cpu0/btime/softirq/procs_running/procs_blocked included)
  // is corruption, never a pass. The old required set (cpu/ctxt/
  // processes/intr) let a truncated snapshot ride.
  for (const k of ["cpu", "cpu0", "ctxt", "btime", "processes",
                   "procs_running", "procs_blocked", "intr", "softirq"]) {
    if (!seen.has(k)) die(what + ": stat missing required line class '" + k + "' (clipped snapshot)");
  }
  return out;
}

function parseSoftirqs(text, what) {
  const map = new Map();
  const lines = text.split("\n").filter((l) => l.length > 0);
  if (!/^\s+CPU0\s*$/.test(lines[0])) die(what + ": softirqs header is not the single-CPU form");
  for (let i = 1; i < lines.length; i++) {
    const m = /^\s*([A-Z_]+):\s+([0-9]+)\s*$/.exec(lines[i]);
    if (!m) die(what + ": softirqs line malformed: " + JSON.stringify(lines[i]));
    if (map.has(m[1])) die(what + ": duplicate softirq " + m[1]);
    map.set(m[1], parseInt(m[2], 10));
  }
  return map;
}

function parsePidTable(text, what) {
  // Raw /proc/<pid>/stat lines. comm may hold spaces/parens: split at
  // the LAST ')'. Fields after it are space-separated; utime/stime are
  // overall fields 14/15, i.e. post-comm fields 12/13 (state = 1).
  const map = new Map();
  for (const line of text.split("\n")) {
    if (line.length === 0) continue;
    const close = line.lastIndexOf(")");
    const open = line.indexOf("(");
    if (open < 0 || close < 0 || close < open) die(what + ": pid stat line malformed: " + JSON.stringify(line));
    const pid = line.slice(0, open).trim();
    if (!/^[0-9]{1,7}$/.test(pid)) die(what + ": pid field malformed: " + JSON.stringify(line));
    const comm = line.slice(open + 1, close);
    const rest = line.slice(close + 1).trim().split(/ +/);
    if (rest.length < 15) die(what + ": pid stat line too short: " + JSON.stringify(line));
    const state = rest[0];
    const utime = parseInt(rest[11], 10);
    const stime = parseInt(rest[12], 10);
    if (!Number.isSafeInteger(utime) || !Number.isSafeInteger(stime)) die(what + ": pid stat utime/stime malformed");
    if (map.has(pid)) die(what + ": duplicate pid " + pid);
    map.set(pid, { comm, state, utime, stime });
  }
  if (map.size < 10) die(what + ": pid table implausibly small (" + map.size + " rows)");
  return map;
}

function parseVmstat(text, what) {
  const map = new Map();
  for (const line of text.split("\n")) {
    if (line.length === 0) continue;
    const m = /^([a-z_0-9]+) ([0-9]+)$/.exec(line);
    if (!m) die(what + ": vmstat line malformed: " + JSON.stringify(line));
    // iter 76 (review-73 M): duplicates previously overwrote silently —
    // a concatenated snapshot could shadow real deltas.
    if (map.has(m[1])) die(what + ": duplicate vmstat key " + m[1] + " (concatenated/corrupt snapshot)");
    map.set(m[1], parseInt(m[2], 10));
  }
  if (map.size < 10) die(what + ": vmstat implausibly small");
  // iter 76: the keys the judgment reports on must be PRESENT (a
  // truncated snapshot that drops pswpin can no longer read as
  // "no swap activity").
  for (const k of ["pgpgin", "pgpgout", "pswpin", "pswpout", "pgfault", "pgmajfault"]) {
    if (!map.has(k)) die(what + ": vmstat missing required key " + k + " (truncated snapshot)");
  }
  return map;
}

function snap(dir, what) {
  const rd = (f) => {
    const p = dir + "/" + f;
    let raw;
    try {
      raw = fs.readFileSync(p, "utf8");
    } catch (e) {
      die("cannot read " + what + " snapshot " + p + ": " + e.message);
    }
    if (raw.length === 0) die(what + " snapshot " + p + " is empty");
    return raw;
  };
  return {
    interrupts: parseInterrupts(rd("interrupts.txt"), what),
    stat: parseStat(rd("stat.txt"), what),
    softirqs: parseSoftirqs(rd("softirqs.txt"), what),
    pids: parsePidTable(rd("pidstat.txt"), what),
    vmstat: parseVmstat(rd("vmstat.txt"), what),
  };
}
const pre = snap(opt.pre, "pre");
const post = snap(opt.post, "post");

// --- event extraction ------------------------------------------------------
// Late start: measured against the absolute pacing schedule anchored at
// the frame-0 attrib row (captured directly after tStart in gfx_app).
const anchor = att[0].mono;
const events = [];
let overBudget = 0, lateStarts = 0;
for (let f = 0; f < FRAMES; f++) {
  const late = f === 0 ? 0 : att[f].mono - (anchor + f * BUDGET);
  const isOver = tim[f].full > BUDGET;
  const isLate = late > LATE_START_NS;
  if (isOver) overBudget++;
  if (isLate) lateStarts++;
  if (tim[f].skipped || isOver || isLate) {
    events.push({ f, late: Math.max(late, 0) });
  }
}

// Baseline per-frame deltas (medians over all frames) for contrast.
function median(arr) {
  if (arr.length === 0) return 0;
  const a = arr.slice().sort((x, y) => x - y);
  return a[Math.floor(a.length / 2)];
}
const dNivcswAll = [], dMinfltAll = [];
for (let f = 0; f < FRAMES; f++) {
  dNivcswAll.push(att[f + 1].nivcsw - att[f].nivcsw);
  dMinfltAll.push(att[f + 1].minflt - att[f].minflt);
}

// Sampler window lookup: bracketing samples for a device-mono stamp.
function samplerWindow(t) {
  if (!samples) return null;
  let lo = null, hi = null;
  for (let i = 0; i + 1 < samples.length; i++) {
    if (samples[i].t <= t && t < samples[i + 1].t) { lo = samples[i]; hi = samples[i + 1]; break; }
  }
  if (!lo) return null;
  const preI = parseInterrupts(lo.files.interrupts, "sampler-lo");
  const postI = parseInterrupts(hi.files.interrupts, "sampler-hi");
  const preS = parseStat(lo.files.stat, "sampler-lo");
  const postS = parseStat(hi.files.stat, "sampler-hi");
  const preQ = parseSoftirqs(lo.files.softirqs, "sampler-lo");
  const postQ = parseSoftirqs(hi.files.softirqs, "sampler-hi");
  const irqD = [];
  for (const [k, v] of postI) {
    const d = v - (preI.has(k) ? preI.get(k) : 0);
    if (d > 0) irqD.push(k + "=" + d);
  }
  const sirqD = [];
  for (const [k, v] of postQ) {
    const d = v - (preQ.has(k) ? preQ.get(k) : 0);
    if (d > 0) sirqD.push(k + "=" + d);
  }
  return {
    winNs: hi.t - lo.t,
    ctxt: postS.ctxt - preS.ctxt,
    procs: postS.processes - preS.processes,
    cpu: ["user", "nice", "system", "idle", "iowait", "irq", "softirq", "steal"]
      .map((k) => k + "=" + (postS.cpu[k] - preS.cpu[k])).join(","),
    irq: irqD.join(","),
    sirq: sirqD.join(","),
  };
}

// --- output ----------------------------------------------------------------
console.log("frames=" + FRAMES);
console.log("budget_ns=" + BUDGET);
console.log("skips=" + skips);
console.log("over_budget_frames=" + overBudget);
console.log("late_start_frames=" + lateStarts);
console.log("events=" + events.length);
console.log("nivcsw_total=" + (att[FRAMES].nivcsw - att[0].nivcsw));
console.log("nvcsw_total=" + (att[FRAMES].nvcsw - att[0].nvcsw));
console.log("minflt_total=" + (att[FRAMES].minflt - att[0].minflt));
console.log("majflt_total=" + (att[FRAMES].majflt - att[0].majflt));
console.log("nivcsw_per_frame_median=" + median(dNivcswAll));
console.log("minflt_per_frame_median=" + median(dMinfltAll));
// mono-vs-raw drift over the run (clock adjustment artifact meter)
const skew0 = att[0].mono - att[0].raw;
const skewN = att[FRAMES].mono - att[FRAMES].raw;
console.log("mono_raw_drift_ns=" + (skewN - skew0));
console.log("sampler_samples=" + (samples ? samples.length : 0));

for (const ev of events) {
  const f = ev.f;
  const dN = att[f + 1].nivcsw - att[f].nivcsw;
  const dV = att[f + 1].nvcsw - att[f].nvcsw;
  const dMi = att[f + 1].minflt - att[f].minflt;
  const dMa = att[f + 1].majflt - att[f].majflt;
  let line =
    "EV|frame=" + (f + 1) + "|skipped=" + (tim[f].skipped ? 1 : 0) +
    "|sim_ns=" + tim[f].sim + "|render_ns=" + tim[f].render +
    "|present_ns=" + tim[f].present + "|full_ns=" + tim[f].full +
    "|late_start_ns=" + ev.late + "|d_nivcsw=" + dN + "|d_nvcsw=" + dV +
    "|d_minflt=" + dMi + "|d_majflt=" + dMa;
  const win = samplerWindow(att[f].mono);
  if (win) {
    line += "|win_ns=" + win.winNs + "|d_ctxt=" + win.ctxt +
      "|d_procs=" + win.procs + "|cpu{" + win.cpu + "}|irq{" + win.irq +
      "}|sirq{" + win.sirq + "}";
  } else if (samples) {
    line += "|win=none";
  }
  console.log(line);
}

// Run-level per-pid CPU deltas (jiffies; a single 10 ms stall consumed
// by a process = >= 1 jiffy in exactly that process).
for (const [pid, p] of post.pids) {
  const b = pre.pids.get(pid);
  const du = p.utime - (b ? b.utime : 0);
  const ds = p.stime - (b ? b.stime : 0);
  if (du + ds > 0 || !b) {
    console.log("PID|pid=" + pid + "|comm=" + p.comm.replace(/\|/g, "_") +
      "|new=" + (b ? 0 : 1) + "|d_utime=" + du + "|d_stime=" + ds);
  }
}
for (const [pid, p] of pre.pids) {
  if (!post.pids.has(pid)) console.log("PIDGONE|pid=" + pid + "|comm=" + p.comm.replace(/\|/g, "_"));
}
// Run-level IRQ / softirq / cpu-line / vmstat deltas (pre -> post).
for (const [k, v] of post.interrupts) {
  const d = v - (pre.interrupts.has(k) ? pre.interrupts.get(k) : 0);
  if (d !== 0) console.log("IRQ|" + k + "|d=" + d);
}
for (const [k, v] of post.softirqs) {
  const d = v - (pre.softirqs.has(k) ? pre.softirqs.get(k) : 0);
  if (d !== 0) console.log("SIRQ|" + k + "|d=" + d);
}
{
  const c = pre.stat.cpu, d = post.stat.cpu;
  console.log("CPU|" + ["user", "nice", "system", "idle", "iowait", "irq", "softirq", "steal"]
    .map((k) => k + "=" + (d[k] - c[k])).join("|"));
  console.log("SYS|d_ctxt=" + (post.stat.ctxt - pre.stat.ctxt) +
    "|d_processes=" + (post.stat.processes - pre.stat.processes) +
    "|d_intr=" + (post.stat.intr - pre.stat.intr));
}
for (const k of ["pgpgin", "pgpgout", "pswpin", "pswpout", "pgfault", "pgmajfault"]) {
  if (pre.vmstat.has(k) && post.vmstat.has(k)) {
    console.log("VM|" + k + "|d=" + (post.vmstat.get(k) - pre.vmstat.get(k)));
  }
}

// Integrity terminator — emitted ONLY after every record above.
console.log("attrib_complete=1");
