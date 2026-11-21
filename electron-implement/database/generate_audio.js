#!/usr/bin/env node
"use strict";

/**
 * Generate Japanese pronunciation audio for every word in the Kotoba Lock
 * database.
 *
 * Usage:
 *   node generate_audio.js
 *   node generate_audio.js --word お茶
 *   node generate_audio.js --force
 *
 * For each database/words/<word>/<word>.json this script:
 *   1. creates database/words/<word>/audios/
 *   2. asks the local llama.cpp TTS server for a WAV
 *   3. converts the WAV to OGG with ffmpeg
 *   4. stores the path relative to database/ in pronunciation.audio
 *
 * Running it again skips words that already have a valid audio file.
 */

const { spawnSync } = require("child_process");
const fs = require("fs");
const path = require("path");

const {
  DATABASE_DIR,
  WORDS_DIR,
  atomicWriteJson,
  formatHelp,
  isFile,
  parseArgs,
  safeConsole,
  sleep,
  toPosix,
  walkJsonFiles,
} = require("./utils");

const DEFAULT_ENDPOINT = "http://127.0.0.1:8080";
const AUDIO_DIR_NAME = "audios";

class AudioError extends Error {}

/** Yield [jsonPath, entry] for every word file in the database. */
function iterEntries() {
  const entries = [];
  for (const jsonPath of walkJsonFiles(WORDS_DIR)) {
    let entry;
    try {
      entry = JSON.parse(fs.readFileSync(jsonPath, "utf8"));
    } catch (error) {
      console.log(`  SKIP ${path.basename(jsonPath)}: cannot read JSON (${error.message})`);
      continue;
    }
    if (!entry || typeof entry !== "object" || Array.isArray(entry)) {
      console.log(`  SKIP ${path.basename(jsonPath)}: not a word entry`);
      continue;
    }
    if (!String(entry.word ?? "").trim()) {
      console.log(`  SKIP ${path.basename(jsonPath)}: not a word entry`);
      continue;
    }
    entries.push([jsonPath, entry]);
  }
  return entries;
}

function wordOf(entry, jsonPath) {
  const word = String(entry.word ?? "").trim();
  return word || path.basename(jsonPath, path.extname(jsonPath)).trim();
}

function audioValue(entry) {
  const pronunciation = entry.pronunciation;
  if (!pronunciation || typeof pronunciation !== "object") return "";
  return String(pronunciation.audio ?? "").trim();
}

function resolveAudio(value) {
  if (!value) return null;
  return path.isAbsolute(value) ? value : path.join(DATABASE_DIR, value);
}

function isAudioFile(filePath) {
  return isFile(filePath) && fs.statSync(filePath).size > 0;
}

function relativeAudioPath(filePath) {
  return toPosix(path.relative(DATABASE_DIR, filePath));
}

function saveJson(jsonPath, entry) {
  atomicWriteJson(jsonPath, entry);
}

/** Set pronunciation.audio if it changed, and save the JSON. */
function setAudioField(jsonPath, entry, audioPath) {
  const relative = relativeAudioPath(audioPath);
  let pronunciation = entry.pronunciation;
  if (!pronunciation || typeof pronunciation !== "object" || Array.isArray(pronunciation)) {
    pronunciation = {};
    entry.pronunciation = pronunciation;
  }
  if (pronunciation.audio === relative) return false;
  pronunciation.audio = relative;
  saveJson(jsonPath, entry);
  return true;
}

function ttsUrl(endpoint) {
  const base = String(endpoint).replace(/\/+$/, "");
  return base.endsWith("/tts") ? base : `${base}/tts`;
}

function propsUrl(endpoint) {
  let base = String(endpoint).replace(/\/+$/, "");
  if (base.endsWith("/tts")) base = base.slice(0, -4);
  if (base.endsWith("/v1")) base = base.slice(0, -3);
  return `${base}/props`;
}

function timeoutSignal(seconds) {
  if (typeof AbortSignal !== "undefined" && typeof AbortSignal.timeout === "function") {
    return AbortSignal.timeout(Math.max(1, Math.round(seconds * 1000)));
  }
  const controller = new AbortController();
  setTimeout(() => controller.abort(), Math.max(1, seconds * 1000)).unref?.();
  return controller.signal;
}

/** Fail early with a helpful message when the server lacks audio support. */
async function checkServer(endpoint, timeout) {
  let data;
  try {
    const response = await fetch(propsUrl(endpoint), {
      method: "GET",
      signal: timeoutSignal(timeout),
    });
    const body = Buffer.from(await response.arrayBuffer()).toString("utf8");
    if (!response.ok) {
      throw new AudioError(`HTTP ${response.status}: ${body.slice(0, 300)}`);
    }
    data = JSON.parse(body);
  } catch (error) {
    if (error instanceof AudioError) throw error;
    const cause = error && error.cause ? ` (${error.cause.message || error.cause})` : "";
    throw new AudioError(`cannot reach llama.cpp at ${endpoint}: ${error.message}${cause}`);
  }

  const modalities = data && typeof data === "object" ? data.modalities : null;
  if (!modalities || typeof modalities !== "object" || !modalities.audio) {
    throw new AudioError("llama.cpp has no audio modality. Start llama-server with --mmproj.");
  }
}

async function requestTts(text, args) {
  const payload = {
    input: text,
    lang: args.lang,
    voice: args.voice,
    n_predict: args.nPredict,
  };

  let response;
  try {
    response = await fetch(ttsUrl(args.endpoint), {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(payload),
      signal: timeoutSignal(args.timeout),
    });
  } catch (error) {
    const cause = error && error.cause ? ` (${error.cause.message || error.cause})` : "";
    throw new AudioError(`TTS request failed: ${error.message}${cause}`);
  }

  const body = Buffer.from(await response.arrayBuffer());

  if (!response.ok) {
    throw new AudioError(`HTTP ${response.status}: ${body.toString("utf8").slice(0, 300)}`);
  }

  const isWav =
    body.length >= 100 &&
    body.subarray(0, 4).toString("latin1") === "RIFF" &&
    body.subarray(0, 16).includes("WAVE");
  if (!isWav) {
    throw new AudioError(
      `server did not return WAV audio: ${JSON.stringify(body.toString("utf8").slice(0, 200))}`,
    );
  }
  return body;
}

/** Write a WAV for `text`, retrying failed requests. */
async function generateWav(text, wavPath, args) {
  let lastError = null;
  for (let attempt = 1; attempt <= args.retries + 1; attempt += 1) {
    if (attempt > 1) {
      console.log(`  Retry ${attempt - 1}/${args.retries} for ${JSON.stringify(text)}...`);
    }
    try {
      fs.mkdirSync(path.dirname(wavPath), { recursive: true });
      fs.writeFileSync(wavPath, await requestTts(text, args));
      return;
    } catch (error) {
      if (!(error instanceof AudioError)) throw error;
      lastError = error.message;
      console.log(`  TTS error: ${lastError}`);
    }
  }
  throw new AudioError(lastError || "TTS generation failed");
}

function convertToOgg(wavPath, oggPath, ffmpeg) {
  const result = spawnSync(
    ffmpeg,
    [
      "-y",
      "-hide_banner",
      "-loglevel",
      "error",
      "-i",
      wavPath,
      "-c:a",
      "libvorbis",
      "-q:a",
      "4",
      oggPath,
    ],
    { encoding: "utf8", windowsHide: true },
  );

  if (result.error) {
    throw new AudioError(`ffmpeg failed: ${result.error.message}`);
  }
  if (result.status !== 0) {
    const detail = String(result.stderr || "").trim() || `exit code ${result.status}`;
    throw new AudioError(`ffmpeg failed: ${detail}`);
  }
  if (!isAudioFile(oggPath)) {
    throw new AudioError("ffmpeg did not create a usable OGG file");
  }
}

async function processEntry(jsonPath, entry, args, ffmpeg) {
  const word = wordOf(entry, jsonPath);
  const audioDir = path.join(path.dirname(jsonPath), AUDIO_DIR_NAME);
  fs.mkdirSync(audioDir, { recursive: true });

  const targetOgg = path.join(audioDir, `${word}.ogg`);
  const targetWav = path.join(audioDir, `${word}.wav`);

  const existing = resolveAudio(audioValue(entry));
  if (!args.force && isAudioFile(existing)) {
    let shown;
    try {
      shown = relativeAudioPath(existing);
    } catch {
      shown = existing;
    }
    console.log(`  SKIP ${word}: already has ${shown}`);
    return "skipped";
  }

  if (!args.force && isAudioFile(targetOgg)) {
    const changed = setAudioField(jsonPath, entry, targetOgg);
    const suffix = changed ? " (linked existing file)" : "";
    console.log(`  SKIP ${word}: already has ${relativeAudioPath(targetOgg)}${suffix}`);
    return "skipped";
  }

  if (!args.force && isAudioFile(targetWav)) {
    console.log(`  Reusing existing WAV for ${word}`);
  } else {
    console.log(`  Generating TTS for ${word}...`);
    await generateWav(word, targetWav, args);
  }

  console.log(`  Converting ${word} to OGG...`);
  convertToOgg(targetWav, targetOgg, ffmpeg);
  setAudioField(jsonPath, entry, targetOgg);

  if (!args.keepWav) {
    try {
      fs.unlinkSync(targetWav);
    } catch {
      // ignore: the WAV is only a temporary artifact
    }
  }

  console.log(`  SAVED ${word}: ${relativeAudioPath(targetOgg)}`);
  return "generated";
}

function findFfmpeg() {
  const names = process.platform === "win32" ? ["ffmpeg.exe", "ffmpeg"] : ["ffmpeg"];
  const pathValue = process.env.PATH || "";
  const separator = process.platform === "win32" ? ";" : ":";
  for (const directory of pathValue.split(separator)) {
    if (!directory) continue;
    for (const name of names) {
      const candidate = path.join(directory, name);
      if (isFile(candidate)) return candidate;
    }
  }
  for (const name of names) {
    const candidate = path.resolve(name);
    if (isFile(candidate)) return candidate;
  }
  return null;
}

const SPEC = {
  endpoint: {
    type: "string",
    default: DEFAULT_ENDPOINT,
    help: `llama.cpp server base URL (default: ${DEFAULT_ENDPOINT}).`,
  },
  voice: { type: "string", default: "female", help: "TTS voice (default: female)." },
  lang: { type: "string", default: "ja", help: "TTS language (default: ja)." },
  "n-predict": {
    type: "int",
    default: 128,
    help: "Maximum audio tokens per word (default: 128).",
  },
  timeout: {
    type: "float",
    default: 300.0,
    help: "Request timeout in seconds (default: 300).",
  },
  retries: {
    type: "int",
    default: 1,
    help: "Extra TTS attempts after a failed request (default: 1).",
  },
  word: {
    type: "string",
    action: "append",
    default: [],
    help: "Only process this Japanese word (can be repeated).",
  },
  force: {
    type: "boolean",
    default: false,
    help: "Regenerate audio even when files already exist.",
  },
  "keep-wav": {
    type: "boolean",
    default: false,
    help: "Keep the intermediate WAV files next to the OGG files.",
  },
  delay: {
    type: "float",
    default: 0.0,
    help: "Seconds to wait between generated words (default: 0).",
  },
};

const USAGE = "node generate_audio.js [options]";

function toOptions(parsed) {
  return {
    endpoint: parsed.endpoint,
    voice: parsed.voice,
    lang: parsed.lang,
    nPredict: parsed["n-predict"],
    timeout: parsed.timeout,
    retries: parsed.retries,
    word: parsed.word,
    force: parsed.force,
    keepWav: parsed["keep-wav"],
    delay: parsed.delay,
  };
}

function parseCli(argv) {
  const parsed = parseArgs(argv, SPEC, USAGE);
  if (parsed.help) {
    console.log(
      formatHelp("Generate OGG pronunciation audio for Kotoba Lock Japanese words.", SPEC, USAGE),
    );
    return null;
  }
  return toOptions(parsed.options);
}

async function run(args) {
  const ffmpeg = findFfmpeg();
  if (ffmpeg === null) {
    console.log("ERROR: ffmpeg was not found in PATH.");
    return 1;
  }

  try {
    await checkServer(args.endpoint, Math.min(args.timeout, 15.0));
  } catch (error) {
    if (!(error instanceof AudioError)) throw error;
    console.log(`ERROR: ${error.message}`);
    return 1;
  }

  const wanted = new Set(args.word.map((word) => String(word).trim()).filter(Boolean));
  const entries = iterEntries().filter(
    ([jsonPath, entry]) => wanted.size === 0 || wanted.has(wordOf(entry, jsonPath)),
  );

  if (wanted.size) {
    const found = new Set(entries.map(([jsonPath, entry]) => wordOf(entry, jsonPath)));
    for (const missing of [...wanted].filter((word) => !found.has(word)).sort()) {
      console.log(`WARNING: word not found in database: ${missing}`);
    }
  }

  console.log(`Database: ${DATABASE_DIR}`);
  console.log(`Words to process: ${entries.length}`);
  console.log("-".repeat(72));

  let generated = 0;
  let skipped = 0;
  const failed = [];

  for (let index = 0; index < entries.length; index += 1) {
    const [jsonPath, entry] = entries[index];
    const word = wordOf(entry, jsonPath);
    console.log(`[${index + 1}/${entries.length}] ${word}`);
    let result;
    try {
      result = await processEntry(jsonPath, entry, args, ffmpeg);
    } catch (error) {
      if (!(error instanceof AudioError) && !(error && error.code)) throw error;
      console.log(`  FAILED ${word}: ${error.message}`);
      failed.push([word, error.message]);
      continue;
    }

    if (result === "generated") {
      generated += 1;
      if (args.delay > 0 && index < entries.length - 1) {
        await sleep(args.delay);
      }
    } else {
      skipped += 1;
    }
  }

  console.log("-".repeat(72));
  console.log(`Done: ${generated} generated, ${skipped} skipped, ${failed.length} failed`);
  if (failed.length) {
    console.log("Failed words:");
    for (const [word, error] of failed) {
      console.log(`  - ${word}: ${error}`);
    }
    return 1;
  }
  return 0;
}

async function main(argv = process.argv.slice(2)) {
  safeConsole();
  let args;
  try {
    args = parseCli(argv);
  } catch (error) {
    console.error(`ERROR: ${error.message}`);
    return 2;
  }
  if (args === null) return 0;

  if (args.retries < 0) args.retries = 0;

  try {
    return await run(args);
  } catch (error) {
    if (error && (error.name === "AbortError" || error.name === "CanceledError")) {
      console.log("\nInterrupted.");
      return 130;
    }
    throw error;
  }
}

if (require.main === module) {
  main()
    .then((code) => process.exit(code))
    .catch((error) => {
      console.error(error && error.stack ? error.stack : error);
      process.exit(1);
    });
}

module.exports = {
  AUDIO_DIR_NAME,
  AudioError,
  DEFAULT_ENDPOINT,
  checkServer,
  convertToOgg,
  findFfmpeg,
  generateWav,
  isAudioFile,
  iterEntries,
  main,
  processEntry,
  relativeAudioPath,
  requestTts,
  resolveAudio,
  run,
  saveJson,
  setAudioField,
  ttsUrl,
  propsUrl,
  wordOf,
};
