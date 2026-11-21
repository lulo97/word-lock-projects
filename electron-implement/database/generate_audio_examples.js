#!/usr/bin/env node
"use strict";

/**
 * Generate Japanese pronunciation audio for every example sentence in the
 * Kotoba Lock database.
 *
 * Usage:
 *   node generate_audio_examples.js
 *   node generate_audio_examples.js --word お茶
 *   node generate_audio_examples.js --verify
 *   node generate_audio_examples.js --force
 *
 * For each example in database/words/<word>/<word>.json this script:
 *   1. creates database/words/<word>/audios_example/
 *   2. asks the local llama.cpp TTS server for a WAV
 *   3. converts the WAV to OGG with ffmpeg
 *   4. stores the path relative to database/ in the example's "audio" field
 *
 * The OGG file name is the normalised romaji of the sentence, produced by
 * Kuroshiro (lib/romaji.js), e.g.
 *   "友達と魚" -> "tomodachi_to_sakana.ogg"
 * The sentence itself (example.japanese) is sent to the TTS server.
 *
 * Running it again skips examples that already have a valid audio file.
 */

const crypto = require("crypto");
const fs = require("fs");
const path = require("path");

const romaji = require("../lib/romaji");
const wordAudio = require("./generate_audio");
const {
  DATABASE_DIR,
  atomicWriteJson,
  formatHelp,
  parseArgs,
  safeConsole,
  sleep,
  toPosix,
  WORDS_DIR,
} = require("./utils");

const AUDIO_DIR_NAME = "audios_example";
const DEFAULT_N_PREDICT = 512;
const MAX_STEM_LENGTH = 100;

/**
 * Turn a romaji sentence into a safe, normalised file stem.
 *
 *   "tomodachi to sakana." -> "tomodachi_to_sakana"
 */
function normalizeFilename(romajiText, sourceText) {
  const lowered = String(romajiText || "").trim().toLowerCase();
  let stem = lowered
    .replace(/[^a-z0-9]+/g, "_")
    .replace(/^_+|_+$/g, "");

  if (!stem) {
    // Non-Japanese text or a failed conversion still needs a stable name.
    const digest = crypto
      .createHash("sha1")
      .update(String(sourceText || "example"), "utf8")
      .digest("hex")
      .slice(0, 8);
    stem = `example_${digest}`;
  }

  if (stem.length > MAX_STEM_LENGTH) {
    const digest = crypto.createHash("sha1").update(lowered, "utf8").digest("hex").slice(0, 8);
    stem = `${stem.slice(0, MAX_STEM_LENGTH).replace(/_+$/, "")}_${digest}`;
  }

  return stem;
}

function exampleText(example) {
  if (!example || typeof example !== "object") return "";
  // Japanese entries use "japanese"; "english" is accepted for legacy files.
  return String(example.japanese ?? example.english ?? "").trim();
}

function exampleAudioValue(example) {
  if (!example || typeof example !== "object") return "";
  return String(example.audio ?? "").trim();
}

function resolveAudio(value) {
  if (!value) return null;
  return path.isAbsolute(value) ? value : path.join(DATABASE_DIR, value);
}

function relativeAudioPath(filePath) {
  return toPosix(path.relative(DATABASE_DIR, filePath));
}

function saveJson(jsonPath, entry) {
  atomicWriteJson(jsonPath, entry);
}

/** Set example.audio and save the JSON when it changed. */
function setExampleAudio(jsonPath, entry, example, audioPath) {
  const relative = relativeAudioPath(audioPath);
  if (example.audio === relative) return false;
  example.audio = relative;
  saveJson(jsonPath, entry);
  return true;
}

/**
 * Build { jsonPath, entry, index, example, text, targetOgg } for every example.
 * The OGG name is the normalised romaji of the sentence.
 */
async function iterExamples(entries) {
  const jobs = [];
  for (const [jsonPath, entry] of entries) {
    const examples = entry.examples;
    if (!Array.isArray(examples)) continue;
    const audioDir = path.join(path.dirname(jsonPath), AUDIO_DIR_NAME);
    for (let index = 0; index < examples.length; index += 1) {
      const example = examples[index];
      const text = exampleText(example);
      if (!text) continue;
      const romajiText = await romaji.sentenceRomaji(text);
      const targetOgg = path.join(audioDir, `${normalizeFilename(romajiText, text)}.ogg`);
      jobs.push({ jsonPath, entry, index, example, text, romajiText, targetOgg });
    }
  }
  return jobs;
}

async function processExample(job, args, ffmpeg) {
  const { jsonPath, entry, example, text, targetOgg } = job;
  const audioDir = path.dirname(targetOgg);
  fs.mkdirSync(audioDir, { recursive: true });
  const targetWav = path.join(audioDir, `${path.basename(targetOgg, ".ogg")}.wav`);

  const existing = resolveAudio(exampleAudioValue(example));
  if (!args.force && wordAudio.isAudioFile(existing)) {
    let shown;
    try {
      shown = relativeAudioPath(existing);
    } catch {
      shown = existing;
    }
    console.log(`  SKIP ${path.basename(targetOgg)}: already has ${shown}`);
    return "skipped";
  }

  if (!args.force && wordAudio.isAudioFile(targetOgg)) {
    const changed = setExampleAudio(jsonPath, entry, example, targetOgg);
    const suffix = changed ? " (linked existing file)" : "";
    console.log(`  SKIP ${path.basename(targetOgg)}: already exists${suffix}`);
    return "skipped";
  }

  if (!args.force && wordAudio.isAudioFile(targetWav)) {
    console.log(`  Reusing existing WAV for ${path.basename(targetOgg)}`);
  } else {
    console.log(`  Generating TTS for ${path.basename(targetOgg)}...`);
    await wordAudio.generateWav(text, targetWav, args);
  }

  console.log(`  Converting ${path.basename(targetOgg)} to OGG...`);
  wordAudio.convertToOgg(targetWav, targetOgg, ffmpeg);
  setExampleAudio(jsonPath, entry, example, targetOgg);

  if (!args.keepWav) {
    try {
      fs.unlinkSync(targetWav);
    } catch {
      // ignore: the WAV is only a temporary artifact
    }
  }

  console.log(`  SAVED ${path.basename(targetOgg)}: ${relativeAudioPath(targetOgg)}`);
  return "generated";
}

function collectEntries(wanted) {
  return wordAudio
    .iterEntries()
    .filter(([jsonPath, entry]) => !wanted.size || wanted.has(wordAudio.wordOf(entry, jsonPath)));
}

/** Report examples that have no usable OGG file. Returns the missing list. */
async function verify(entries) {
  let total = 0;
  const missing = [];
  for (const job of await iterExamples(entries)) {
    total += 1;
    if (wordAudio.isAudioFile(job.targetOgg)) continue;
    missing.push(job);
  }
  console.log("-".repeat(72));
  if (!missing.length) {
    console.log(`CHECK OK: all ${total} examples have audio files.`);
    return missing;
  }
  console.log(`CHECK FAILED: ${missing.length} of ${total} examples have no audio file:`);
  for (const job of missing) {
    const word = wordAudio.wordOf(job.entry, job.jsonPath);
    console.log(`  - ${word} example[${job.index}] -> ${relativeAudioPath(job.targetOgg)}`);
    console.log(`      ${job.text}`);
  }
  return missing;
}

const SPEC = {
  endpoint: {
    type: "string",
    default: wordAudio.DEFAULT_ENDPOINT,
    help: `llama.cpp server base URL (default: ${wordAudio.DEFAULT_ENDPOINT}).`,
  },
  voice: { type: "string", default: "female", help: "TTS voice (default: female)." },
  lang: { type: "string", default: "ja", help: "TTS language (default: ja)." },
  "n-predict": {
    type: "int",
    default: DEFAULT_N_PREDICT,
    help: `Maximum audio tokens per sentence (default: ${DEFAULT_N_PREDICT}).`,
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
    help: "Only process examples of this Japanese word (can be repeated).",
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
    help: "Seconds to wait between generated examples (default: 0).",
  },
  verify: {
    type: "boolean",
    default: false,
    help: "Only check that every example has an audio file; generate nothing.",
  },
};

const USAGE = "node generate_audio_examples.js [options]";

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
    verify: parsed.verify,
  };
}

function parseCli(argv) {
  const parsed = parseArgs(argv, SPEC, USAGE);
  if (parsed.help) {
    console.log(
      formatHelp(
        "Generate OGG audio for Kotoba Lock Japanese example sentences.",
        SPEC,
        USAGE,
      ),
    );
    return null;
  }
  return toOptions(parsed.options);
}

async function run(args) {
  await romaji.init();

  const wanted = new Set(args.word.map((word) => String(word).trim()).filter(Boolean));
  const entries = collectEntries(wanted);

  if (wanted.size) {
    const found = new Set(entries.map(([jsonPath, entry]) => wordAudio.wordOf(entry, jsonPath)));
    for (const missing of [...wanted].filter((word) => !found.has(word)).sort()) {
      console.log(`WARNING: word not found in database: ${missing}`);
    }
  }

  if (args.verify) {
    console.log(`Database: ${WORDS_DIR}`);
    console.log(`Words checked: ${entries.length}`);
    return (await verify(entries)).length ? 1 : 0;
  }

  const ffmpeg = wordAudio.findFfmpeg();
  if (ffmpeg === null) {
    console.log("ERROR: ffmpeg was not found in PATH.");
    return 1;
  }

  try {
    await wordAudio.checkServer(args.endpoint, Math.min(args.timeout, 15.0));
  } catch (error) {
    if (!(error instanceof wordAudio.AudioError)) throw error;
    console.log(`ERROR: ${error.message}`);
    return 1;
  }

  const jobs = await iterExamples(entries);
  console.log(`Database: ${WORDS_DIR}`);
  console.log(`Words: ${entries.length}  Examples to process: ${jobs.length}`);
  console.log("-".repeat(72));

  let generated = 0;
  let skipped = 0;
  const failed = [];

  for (let index = 0; index < jobs.length; index += 1) {
    const job = jobs[index];
    const word = wordAudio.wordOf(job.entry, job.jsonPath);
    console.log(`[${index + 1}/${jobs.length}] ${word} example[${job.index}]`);
    let result;
    try {
      result = await processExample(job, args, ffmpeg);
    } catch (error) {
      if (!(error instanceof wordAudio.AudioError) && !(error && error.code)) throw error;
      console.log(`  FAILED ${path.basename(job.targetOgg)}: ${error.message}`);
      failed.push([word, path.basename(job.targetOgg), error.message]);
      continue;
    }

    if (result === "generated") {
      generated += 1;
      if (args.delay > 0 && index < jobs.length - 1) {
        await sleep(args.delay);
      }
    } else {
      skipped += 1;
    }
  }

  console.log("-".repeat(72));
  console.log(`Done: ${generated} generated, ${skipped} skipped, ${failed.length} failed`);
  if (failed.length) {
    console.log("Failed examples:");
    for (const [word, name, error] of failed) {
      console.log(`  - ${word} / ${name}: ${error}`);
    }
  }

  const missing = await verify(entries);
  return failed.length || missing.length ? 1 : 0;
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
  collectEntries,
  exampleAudioValue,
  exampleText,
  iterExamples,
  main,
  normalizeFilename,
  processExample,
  run,
  setExampleAudio,
  verify,
};
