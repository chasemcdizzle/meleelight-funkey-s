#!/usr/bin/env node
// port/gfx/judge-audio-summary.js — strict whitelist-grammar judge for
// gfx_app's audio summary line (M3 task 6; PROCESS §3 rule).
//
// PRODUCER GRAMMAR (measured from gfx_app.c's single audio fprintf —
// the paired change site; corpus: this iteration's host + device logs):
//   gfx_app audio: <cbs> callbacks, <underruns> underruns, <badlen>
//   badlen, <starts> voice starts, <stops> voice stops, <steals>
//   steals, rate=<r> samples=<s> channels=<c>
// EXACTLY ONE log line may match the anchored full-line pattern; the
// expected rate/samples/channels are PINNED INTO the pattern (a run
// whose backend granted a different spec — e.g. a headless run posing
// as a device run, or a renegotiated device — cannot even parse). All
// numeric fields are digit-bounded (1-12) before any arithmetic.
//
// Usage:
//   judge-audio-summary.js <log> --rate N --samples N --channels N
//     [--cbs-min N] [--cbs-max N] [--max-underruns N] [--max-badlen N]
//     [--expect-starts N] [--expect-stops N]
//
// Output (iter 59, review-57 H — a MANDATORY COMPLETE verdict block,
// buffered and written in ONE stdout write): key=value lines (cbs,
// underruns, badlen, starts, stops, steals), then one fail_<leg>=1
// line per violated assertion, then the INTEGRITY TERMINATOR
//   judge_complete=1
// as the LAST line, ALWAYS (both the pass and the assertion-violated
// exits). Callers MUST require that terminator: judge output whose
// last line is not exactly `judge_complete=1` is truncated/partial =
// corruption, and may never be classified (in particular never as
// "retryable"). fail_* lines are REPORTING ONLY — callers bind their
// decisions to the counters, never to fail_* presence.
// Exit codes: 0 = parsed + all given assertions hold; 2 = parsed but
// >=1 assertion violated (the gate-fail direction — the caller decides
// retry policy per leg); 1 = grammar/corruption death (never partially
// parsed, never silently skipped; NO judge_complete line is emitted on
// this path — all rc-1 deaths precede the verdict block).
'use strict';
const fs = require('fs');

function die(msg) {
  console.error('judge-audio-summary FAIL: ' + msg);
  process.exit(1);
}

const args = process.argv.slice(2);
if (args.length < 1) die('usage: judge-audio-summary.js <log> --rate N ...');
const logPath = args[0];
const opt = {};
for (let i = 1; i < args.length; i += 2) {
  const k = args[i];
  if (!/^--(rate|samples|channels|cbs-min|cbs-max|max-underruns|max-badlen|expect-starts|expect-stops)$/.test(k)) {
    die('unknown option ' + k);
  }
  if (i + 1 >= args.length) die(k + ' needs a value');
  const v = args[i + 1];
  if (!/^[0-9]{1,12}$/.test(v)) die(k + ' value not a bounded decimal: ' + v);
  opt[k.slice(2)] = Number(v);
}
for (const req of ['rate', 'samples', 'channels']) {
  if (!(req in opt)) die('--' + req + ' is required (pinned into the grammar)');
}

let text;
try {
  text = fs.readFileSync(logPath, 'utf8');
} catch (e) {
  die('cannot read log ' + logPath + ': ' + e.message);
}
const lines = text.split('\n');
const re = new RegExp(
  '^gfx_app audio: ([0-9]{1,12}) callbacks, ([0-9]{1,12}) underruns, ' +
  '([0-9]{1,12}) badlen, ([0-9]{1,12}) voice starts, ' +
  '([0-9]{1,12}) voice stops, ([0-9]{1,12}) steals, ' +
  'rate=' + String(opt.rate) + ' samples=' + String(opt.samples) +
  ' channels=' + String(opt.channels) + '$');
const matches = [];
let audioish = 0;
for (const ln of lines) {
  if (ln.startsWith('gfx_app audio:')) audioish++;
  const m = re.exec(ln);
  if (m) matches.push(m);
}
if (matches.length !== 1) {
  die('log has ' + String(matches.length) + ' lines matching the pinned ' +
      'audio grammar (want exactly 1; ' + String(audioish) +
      ' audio-prefixed lines present) — corrupt or wrong-spec evidence');
}
// resembles-but-doesn't-match = corruption (fail closed): any extra
// audio-prefixed line that did not match the full grammar is corrupt.
if (audioish !== 1) {
  die(String(audioish) + ' "gfx_app audio:" lines present but only 1 ' +
      'matches the pinned grammar — corrupt evidence');
}
const m = matches[0];
const out = {
  cbs: Number(m[1]),
  underruns: Number(m[2]),
  badlen: Number(m[3]),
  starts: Number(m[4]),
  stops: Number(m[5]),
  steals: Number(m[6]),
};
const outLines = [];
for (const k of ['cbs', 'underruns', 'badlen', 'starts', 'stops', 'steals']) {
  outLines.push(k + '=' + String(out[k]));
}
const fails = [];
if ('cbs-min' in opt && out.cbs < opt['cbs-min']) fails.push('cbs_low');
if ('cbs-max' in opt && out.cbs > opt['cbs-max']) fails.push('cbs_high');
if ('max-underruns' in opt && out.underruns > opt['max-underruns']) {
  fails.push('underruns');
}
if ('max-badlen' in opt && out.badlen > opt['max-badlen']) fails.push('badlen');
if ('expect-starts' in opt && out.starts !== opt['expect-starts']) {
  fails.push('starts');
}
if ('expect-stops' in opt && out.stops !== opt['expect-stops']) {
  fails.push('stops');
}
for (const f of fails) outLines.push('fail_' + f + '=1');
// Integrity terminator (review-57 H): ALWAYS last; the whole verdict
// block is one write so a partial flush cannot end at a plausible
// boundary without also dropping this line.
outLines.push('judge_complete=1');
process.stdout.write(outLines.join('\n') + '\n');
process.exit(fails.length ? 2 : 0);
