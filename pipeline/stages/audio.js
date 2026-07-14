"use strict";
// Stage "audio" — deterministic conversion of the upstream audio content
// plus the executed-JS sound map (fix_plan §M1 task 4; format SND1,
// pipeline/FORMATS.md §5).
//
//   dist/sfx/*.wav   -> audio/sfx/<name>.pcm   22050 Hz MONO   S16LE raw
//   dist/music/*.ogg -> audio/music/<name>.pcm 22050 Hz STEREO S16LE raw
//   sounds.json      -> the SND1 sound map (Howl name -> blob + volumes +
//                       loop + music sprite windows + actionSounds)
//
// Rates/channels per the measured device verdict (PLAN §7 /
// docs/research/audio-spike.md): SFX pre-decoded to RAM mono 22050; music
// streamed from SD stereo 22050; ogg/wav never ship to the device.
//
// PROVENANCE: this content is Nintendo-derived (Melee SFX/music ripped
// upstream). PRIVATE USE ONLY — never distributed. Blobs live only in
// gitignored build output (build*/); the repo commits hashes, never bytes.
//
// DETERMINISM: raw s16le output carries no container metadata/timestamps,
// but resampler bytes are an ffmpeg-build property — so the ffmpeg VERSION
// is pinned in pipeline/expected.json (audio.tool.ffmpeg) and this stage
// HARD-FAILS on any other ffmpeg before converting a single file, rather
// than drifting bytes past the byte-stability check (which compares two
// runs of the SAME binary and cannot see version drift by itself).
// Output hashes are additionally frozen as expected.json
// audio.artifactsSha256 (aggregate over path+sha256 of every artifact).

const fs = require("fs");
const path = require("path");
const { execFileSync } = require("child_process");
const { sha256, sha256File, stableStringify } = require("../lib/manifest");
const S = require("../lib/sounds-schema");

const REPO_ROOT = path.join(__dirname, "..", "..");

// Exact conversion argv (pinned; recorded verbatim in the manifest).
// -fflags +bitexact silences any bitexact-affected decode path; raw s16le
// has no muxer metadata to strip but the flag is pinned anyway so the
// command line never silently loosens.
const FFMPEG_COMMON = ["-hide_banner", "-nostdin", "-loglevel", "error",
  "-y", "-fflags", "+bitexact"];
const SFX_OUT_ARGS = ["-map_metadata", "-1", "-f", "s16le",
  "-acodec", "pcm_s16le", "-ar", "22050", "-ac", "1"];
const MUSIC_OUT_ARGS = ["-map_metadata", "-1", "-f", "s16le",
  "-acodec", "pcm_s16le", "-ar", "22050", "-ac", "2"];

const README = `# audio/ — Nintendo-derived content — PRIVATE USE ONLY

These raw PCM blobs are converted from the upstream meleelight dist/
audio: ripped Super Smash Bros. Melee sound effects and music.

- PRIVATE USE ONLY. NEVER distribute these files, the binaries that
  embed them, or any package containing them (CLAUDE.md licensing rule;
  docs/LICENSING.md).
- Generated build output (gitignored via build*/): only manifests and
  hashes are committed, never these bytes.
- Format: headerless S16LE PCM — sfx/ 22050 Hz mono, music/ 22050 Hz
  stereo (pipeline/FORMATS.md section 5; PLAN section 7 device verdict).
`;

function ffmpegVersion() {
  const line = execFileSync("ffmpeg", ["-version"], { encoding: "utf8" }).split("\n")[0];
  const m = /^ffmpeg version (\S+)/.exec(line);
  if (!m) throw new Error(`cannot parse ffmpeg version from: ${line}`);
  return m[1];
}

function assertPinnedFfmpeg(version) {
  const expPath = path.join(REPO_ROOT, "pipeline", "expected.json");
  const exp = JSON.parse(fs.readFileSync(expPath, "utf8"));
  const pin = exp.audio && exp.audio.tool && exp.audio.tool.ffmpeg;
  if (pin === undefined) return; // first measurement; freeze afterwards
  if (version !== pin) {
    throw new Error(
      `ffmpeg ${version} != pinned ${pin} — PCM resampler output is an ` +
      `ffmpeg-build property; converting would DRIFT frozen artifact ` +
      `bytes. Install the pinned ffmpeg, or deliberately re-freeze ` +
      `pipeline/expected.json audio (tool.ffmpeg + artifactsSha256 + ` +
      `byte/sample counts) in the same change (FORMATS.md section 5).`);
  }
}

function run(ctx) {
  const version = ffmpegVersion();
  assertPinnedFfmpeg(version);

  const { sounds, tables, srcSha256 } = S.loadSounds(ctx.distRoot);
  const model = S.buildSoundModel(sounds, tables);

  const upstreamHead = execFileSync("git",
    ["-C", ctx.distRoot, "rev-parse", "HEAD"], { encoding: "utf8" }).trim();

  const artifacts = [];
  const sources = [
    { path: "dist/js/extractor.js", sha256: srcSha256 },
    { path: "pipeline/extractor/extractor.entry.js",
      sha256: sha256File(path.join(REPO_ROOT, "pipeline/extractor/extractor.entry.js")) },
    { path: "pipeline/extractor/extractor.config.js",
      sha256: sha256File(path.join(REPO_ROOT, "pipeline/extractor/extractor.config.js")) },
    { path: "src/main/sfx.js",
      sha256: sha256File(path.join(ctx.distRoot, "src/main/sfx.js")) },
    { path: "src/main/music.js",
      sha256: sha256File(path.join(ctx.distRoot, "src/main/music.js")) },
  ];

  // Convert one file; returns its manifest artifact entry (blob shape
  // hard-asserted here AND re-checked from the manifest by
  // lib/check-expected.js).
  const convert = (inRel, outRel, outArgs, channels) => {
    const inAbs = path.join(ctx.distRoot, "dist", inRel);
    const outAbs = path.join(ctx.outDir, outRel);
    fs.mkdirSync(path.dirname(outAbs), { recursive: true });
    execFileSync("ffmpeg",
      [...FFMPEG_COMMON, "-i", inAbs, ...outArgs, outAbs],
      { stdio: ["ignore", "ignore", "inherit"] });
    const buf = fs.readFileSync(outAbs);
    const frameSize = 2 * channels;
    if (buf.length === 0 || buf.length % frameSize !== 0) {
      throw new Error(`${outRel}: ${buf.length} bytes !== 0 mod frame size ${frameSize}`);
    }
    sources.push({ path: "dist/" + inRel, sha256: sha256File(inAbs) });
    const entry = { path: outRel, sha256: sha256(buf), bytes: buf.length,
      samples: buf.length / frameSize, channels, rate: 22050 };
    artifacts.push(entry);
    return entry;
  };

  // -- sfx: every wav in dist/sfx (mapped or not — the 24-ish unmapped
  // wavs are upstream content too; the map records which 180 names the
  // sim can reach) --
  const wavs = fs.readdirSync(path.join(ctx.distRoot, "dist", "sfx"))
    .filter((f) => f.endsWith(".wav")).sort();
  let sfxBytes = 0, sfxSamples = 0;
  for (const w of wavs) {
    const e = convert(path.join("sfx", w),
      path.join("audio", "sfx", w.replace(/\.wav$/, ".pcm")), SFX_OUT_ARGS, 1);
    sfxBytes += e.bytes; sfxSamples += e.samples;
  }
  ctx.log(`  audio/sfx: ${wavs.length} wavs -> mono 22050 S16LE pcm (${sfxBytes} bytes)`);

  // -- music: every ogg in dist/music --
  const oggs = fs.readdirSync(path.join(ctx.distRoot, "dist", "music"))
    .filter((f) => f.endsWith(".ogg")).sort();
  let musicBytes = 0, musicSamples = 0;
  for (const o of oggs) {
    const e = convert(path.join("music", o),
      path.join("audio", "music", o.replace(/\.ogg$/, ".pcm")), MUSIC_OUT_ARGS, 2);
    musicBytes += e.bytes; musicSamples += e.samples;
  }
  ctx.log(`  audio/music: ${oggs.length} oggs -> stereo 22050 S16LE pcm (${musicBytes} bytes)`);

  // Every mapped blob must exist among the converted artifacts.
  const blobPaths = new Set(artifacts.map((a) => a.path));
  for (const [name, s] of Object.entries(model.sfx)) {
    if (!blobPaths.has(s.blob)) throw new Error(`sfx.${name}: blob ${s.blob} was not converted`);
  }
  for (const [name, m] of Object.entries(model.music)) {
    if (!blobPaths.has(m.blob)) throw new Error(`music.${name}: blob ${m.blob} was not converted`);
  }

  const write = (name, content) => {
    const buf = Buffer.from(content);
    const abs = path.join(ctx.outDir, name);
    fs.mkdirSync(path.dirname(abs), { recursive: true });
    fs.writeFileSync(abs, buf);
    artifacts.push({ path: name, sha256: sha256(buf), bytes: buf.length });
    ctx.log(`  ${name}: ${buf.length} bytes`);
  };
  write("sounds.json", stableStringify(model));
  write("audio/README.md", README);

  // Coverage (exact executed/converted counts; pinned in expected.json).
  const mappedWavs = new Set(Object.values(model.sfx).map((s) => s.file));
  const actionStates = Object.values(model.actionSounds)
    .reduce((n, c) => n + Object.keys(c).length, 0);
  const actionEvents = Object.values(model.actionSounds)
    .reduce((n, c) => n + Object.values(c).reduce((k, l) => k + l.length, 0), 0);
  const cov = {
    sfxBlobs: wavs.length,
    mappedSounds: Object.keys(model.sfx).length,
    mappedWavs: mappedWavs.size,
    unmappedWavs: wavs.length - mappedWavs.size,
    loopingSounds: Object.values(model.sfx).filter((s) => s.loop === 1).length,
    musicTracks: oggs.length,
    actionSoundChars: Object.keys(model.actionSounds).length,
    actionSoundStates: actionStates,
    actionSoundEvents: actionEvents,
    sfxBytes, sfxSamples, musicBytes, musicSamples,
  };

  artifacts.sort((a, b) => (a.path < b.path ? -1 : 1));
  sources.sort((a, b) => (a.path < b.path ? -1 : 1));
  return {
    format: "SND1",
    tool: {
      ffmpeg: version,
      sfxArgs: [...FFMPEG_COMMON, "-i", "<in.wav>", ...SFX_OUT_ARGS, "<out.pcm>"],
      musicArgs: [...FFMPEG_COMMON, "-i", "<in.ogg>", ...MUSIC_OUT_ARGS, "<out.pcm>"],
    },
    provenance: {
      contentOrigin: "Nintendo-derived (Super Smash Bros. Melee SFX/music, ripped upstream)",
      use: "PRIVATE USE ONLY - never distributed",
    },
    artifactsSha256: sha256(artifacts.map((a) => `${a.path} ${a.sha256}\n`).join("")),
    sources,
    coverage: cov,
    artifacts,
  };
}

module.exports = { name: "audio", run };
