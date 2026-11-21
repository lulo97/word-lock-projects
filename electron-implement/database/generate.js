#!/usr/bin/env node
"use strict";

/**
 * Generate missing Kotoba Lock dictionary entries with a local llama.cpp server.
 *
 * Japanese -> Vietnamese entries are saved as:
 *   database/words/<word>/<word>.json
 *
 * Usage:
 *   node generate.js --input words.txt
 *
 * `words.txt` contains one Japanese word per line (kanji, hiragana or
 * katakana). Existing words are skipped; missing words are sent to llama.cpp.
 *
 * Run this from the project root or from the database folder:
 *   node database/generate.js --input database/words.txt
 *   node generate.js --input words.txt
 *
 * The llama.cpp server must be listening on http://127.0.0.1:9931 (configurable
 * with --endpoint).
 */

const fs = require("fs");
const path = require("path");

const {
  DATABASE_DIR,
  WORDS_DIR,
  atomicWriteJson,
  formatHelp,
  isFile,
  parseArgs,
  resolveInput,
  safeConsole,
  sleep,
  toPosix,
  walkJsonFiles,
} = require("./utils");

const REQUIRED_KEYS = [
  "word",
  "language",
  "pronunciation",
  "part_of_speech",
  "definitions",
  "examples",
  "synonyms",
  "antonyms",
  "tags",
];

// A word must contain Japanese script and be usable as a Windows folder name.
const JAPANESE_RE = /[\u3000-\u303f\u3040-\u309f\u30a0-\u30ff\u3400-\u4dbf\u4e00-\u9fff\uf900-\ufaff]/u;
const INVALID_FILENAME_RE = /[\\/:*?"<>|\u0000-\u001f]/u;

const FALLBACK_PROMPT = `You generate one Japanese-to-Vietnamese dictionary entry.

Return exactly one valid JSON object and nothing else: no markdown fences, no
comments, no explanation before or after it.

Schema:
{
  "word": string,
  "language": {"source": "ja", "target": "vi"},
  "pronunciation": {"kana": string, "audio": ""},
  "part_of_speech": string,
  "definitions": [{"japanese": string, "vietnamese": string}],
  "examples": [{"japanese": string, "vietnamese": string, "audio": ""}],
  "synonyms": [string],
  "antonyms": [string],
  "tags": [string]
}

Rules:
- \`word\` must be exactly the requested Japanese word, unchanged.
- Put the full hiragana reading in \`pronunciation.kana\` (katakana words are
  written in hiragana). Never store romaji.
- Give at least one clear Japanese definition and its natural Vietnamese
  translation. Definitions and examples are written in Japanese, not English.
- Give two natural Japanese example sentences and their Vietnamese translations.
- Do not add an \`audio\` key to definitions. Examples keep "audio": "".
- Use empty arrays when there are no synonyms, antonyms, or tags.
- Only use Japanese synonyms/antonyms (kanji, hiragana or katakana).
- Do not add extra top-level keys. All strings must be non-empty except
  \`pronunciation.audio\`, \`examples[].audio\` and intentionally empty lists.`;

class GenerateError extends Error {}

/** Normalize a Japanese word without changing its script. */
function normalizeWord(value) {
  return String(value).normalize("NFC").trim();
}

function isValidWord(word) {
  return Boolean(word) && !INVALID_FILENAME_RE.test(word) && JAPANESE_RE.test(word);
}

function readWords(filePath) {
  /** Read, normalize, and de-duplicate words while preserving file order. */
  const words = [];
  const seen = new Set();
  const text = fs.readFileSync(filePath, "utf8").replace(/^\uFEFF/, "");
  const lines = text.split(/\r?\n/);

  lines.forEach((line, index) => {
    const lineNumber = index + 1;
    const word = normalizeWord(line);
    if (!word || word.startsWith("#")) return;
    if (!isValidWord(word)) {
      console.log(`  SKIP line ${lineNumber}: invalid Japanese word ${JSON.stringify(line.trim())}`);
      return;
    }
    if (!seen.has(word)) {
      seen.add(word);
      words.push(word);
    }
  });

  return words;
}

function loadExistingWords() {
  /** Collect words already present anywhere in the database. */
  const existing = new Set();
  for (const filePath of walkJsonFiles(WORDS_DIR)) {
    let word = "";
    try {
      const data = JSON.parse(fs.readFileSync(filePath, "utf8"));
      if (data && typeof data === "object" && !Array.isArray(data)) {
        word = normalizeWord(data.word ?? "");
      }
    } catch {
      // ignore unreadable or invalid JSON, fall back to the folder/file name
    }
    const stem = normalizeWord(path.basename(filePath, path.extname(filePath)));
    existing.add(word || stem);
  }
  return existing;
}

function buildMessages(prompt, word, previousError) {
  const messages = [
    { role: "system", content: prompt },
    { role: "user", content: `Generate the JSON for the Japanese word: ${word}` },
  ];
  if (previousError) {
    messages.push({
      role: "user",
      content:
        `The previous reply was invalid: ${previousError}. ` +
        "Return only corrected JSON for the word.",
    });
  }
  return messages;
}

function chatUrl(endpoint) {
  let base = String(endpoint).replace(/\/+$/, "");
  if (base.endsWith("/v1/chat/completions")) return base;
  if (base.endsWith("/v1")) return `${base}/chat/completions`;
  return `${base}/v1/chat/completions`;
}

function timeoutSignal(seconds) {
  if (typeof AbortSignal !== "undefined" && typeof AbortSignal.timeout === "function") {
    return AbortSignal.timeout(Math.max(1, Math.round(seconds * 1000)));
  }
  const controller = new AbortController();
  setTimeout(() => controller.abort(), Math.max(1, seconds * 1000)).unref?.();
  return controller.signal;
}

async function requestLlm(endpoint, model, messages, timeout, maxTokens) {
  const payload = {
    model,
    messages,
    temperature: 0.0,
    max_tokens: maxTokens,
    stream: false,
  };

  let response;
  try {
    response = await fetch(chatUrl(endpoint), {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(payload),
      signal: timeoutSignal(timeout),
    });
  } catch (error) {
    const cause = error && error.cause ? ` (${error.cause.message || error.cause})` : "";
    throw new GenerateError(`cannot reach llama.cpp: ${error.message}${cause}`);
  }

  const body = Buffer.from(await response.arrayBuffer()).toString("utf8");

  if (!response.ok) {
    throw new GenerateError(`HTTP ${response.status}: ${body.slice(0, 500)}`);
  }

  let data;
  try {
    data = JSON.parse(body);
  } catch {
    throw new GenerateError(`unexpected llama.cpp response: ${body.slice(0, 300)}`);
  }

  const content =
    data && data.choices && data.choices[0] && data.choices[0].message
      ? data.choices[0].message.content
      : undefined;
  if (typeof content !== "string" || !content.trim()) {
    throw new GenerateError("llama.cpp returned an empty response");
  }
  return content;
}

function extractJson(text) {
  /** Parse JSON, tolerating markdown fences or surrounding prose. */
  let source = String(text).trim();

  const fence = source.match(/```(?:json)?\s*([\s\S]*?)```/i);
  if (fence) source = fence[1].trim();

  try {
    return JSON.parse(source);
  } catch {
    // fall through to brace extraction
  }

  const start = source.indexOf("{");
  const end = source.lastIndexOf("}");
  if (start === -1 || end <= start) {
    throw new GenerateError("response did not contain a JSON object");
  }
  try {
    return JSON.parse(source.slice(start, end + 1));
  } catch (error) {
    throw new GenerateError(`invalid JSON in response: ${error.message}`);
  }
}

function validateEntry(entry, word, targetPath) {
  /** Check the generated object before it is written to disk. */
  if (!entry || typeof entry !== "object" || Array.isArray(entry)) {
    throw new GenerateError("LLM output is not a JSON object");
  }

  const returned = normalizeWord(entry.word ?? "");
  if (returned !== word) {
    throw new GenerateError(
      `LLM returned word ${JSON.stringify(returned)}, expected ${JSON.stringify(word)}`,
    );
  }
  entry.word = word;

  const missing = REQUIRED_KEYS.filter((key) => !Object.prototype.hasOwnProperty.call(entry, key));
  if (missing.length) {
    throw new GenerateError(`missing required keys: ${missing.join(", ")}`);
  }

  // Keep generated entries in the exact Japanese -> Vietnamese shape the app reads.
  if (!entry.language || typeof entry.language !== "object") {
    throw new GenerateError("language must be an object");
  }
  entry.language.source = "ja";
  entry.language.target = "vi";

  const pronunciation = entry.pronunciation;
  if (!pronunciation || typeof pronunciation !== "object") {
    throw new GenerateError("pronunciation must be an object");
  }
  if (typeof pronunciation.audio !== "string") pronunciation.audio = "";

  if (Array.isArray(entry.examples)) {
    for (const example of entry.examples) {
      if (example && typeof example === "object" && typeof example.audio !== "string") {
        example.audio = "";
      }
    }
  }

  let validator;
  try {
    validator = require("./validate_database");
  } catch {
    return;
  }

  const { errors } = validator.validateEntry(entry, targetPath);
  if (errors.length) {
    throw new GenerateError(errors.slice(0, 5).join("; "));
  }
}

async function generateEntry(word, targetPath, prompt, args) {
  /** Ask the LLM until it returns a valid entry or retries run out. */
  let lastError = null;
  for (let attempt = 1; attempt <= args.retries + 1; attempt += 1) {
    if (attempt > 1) {
      console.log(`  Retry ${attempt - 1}/${args.retries} for ${JSON.stringify(word)}...`);
    }
    try {
      const content = await requestLlm(
        args.endpoint,
        args.model,
        buildMessages(prompt, word, lastError),
        args.timeout,
        args.maxTokens,
      );
      const entry = extractJson(content);
      validateEntry(entry, word, targetPath);
      return entry;
    } catch (error) {
      if (!(error instanceof GenerateError)) throw error;
      lastError = error.message;
      console.log(`  Invalid response: ${lastError}`);
    }
  }
  throw new GenerateError(lastError || "generation failed");
}

function saveEntry(entry, targetPath) {
  atomicWriteJson(targetPath, entry);
}

function targetPathFor(word) {
  return path.join(WORDS_DIR, word, `${word}.json`);
}

async function run(args) {
  const inputPath = resolveInput(args.input);
  if (inputPath === null) {
    console.log(`ERROR: input file not found: ${args.input}`);
    return 1;
  }

  const prompt = FALLBACK_PROMPT;

  const words = readWords(inputPath);
  const existing = loadExistingWords();
  console.log(`Database: ${DATABASE_DIR}`);
  console.log(`Words folder: ${WORDS_DIR}`);
  console.log(`Words file: ${inputPath}`);
  console.log(`Words found: ${words.length} | already in database: ${existing.size}`);
  console.log("-".repeat(72));

  let generated = 0;
  let skipped = 0;
  const failed = [];

  for (let index = 0; index < words.length; index += 1) {
    const word = words[index];
    const prefix = `[${index + 1}/${words.length}] ${word}`;
    const targetPath = targetPathFor(word);

    if (existing.has(word) || isFile(targetPath)) {
      console.log(`${prefix}: already exists, skipping`);
      skipped += 1;
      continue;
    }

    console.log(`${prefix}: generating...`);
    try {
      const entry = await generateEntry(word, targetPath, prompt, args);
      saveEntry(entry, targetPath);
    } catch (error) {
      if (!(error instanceof GenerateError) && !(error && error.code)) throw error;
      console.log(`${prefix}: FAILED - ${error.message}`);
      failed.push([word, error.message]);
      continue;
    }

    existing.add(word);
    generated += 1;
    console.log(`${prefix}: saved ${toPosix(path.relative(DATABASE_DIR, targetPath))}`);

    if (args.delay > 0 && index < words.length - 1) {
      await sleep(args.delay);
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

const SPEC = {
  input: {
    type: "string",
    required: true,
    help: 'Text file containing one Japanese word per line (e.g. "words.txt").',
  },
  endpoint: {
    type: "string",
    default: "http://127.0.0.1:9931",
    help: "llama.cpp server base URL (default: http://127.0.0.1:9931).",
  },
  model: {
    type: "string",
    default: "local-model",
    help: "Model name sent to llama.cpp (default: local-model).",
  },
  timeout: {
    type: "float",
    default: 300.0,
    help: "Request timeout in seconds (default: 300).",
  },
  retries: {
    type: "int",
    default: 2,
    help: "Extra attempts after an invalid response (default: 2).",
  },
  "max-tokens": {
    type: "int",
    default: 2048,
    help: "Maximum tokens per response (default: 2048).",
  },
  delay: {
    type: "float",
    default: 0.0,
    help: "Seconds to wait between generated words (default: 0).",
  },
};

const USAGE = "node generate.js --input <words.txt> [options]";

function toCamelCaseOptions(parsed) {
  return {
    input: parsed.input,
    endpoint: parsed.endpoint,
    model: parsed.model,
    timeout: parsed.timeout,
    retries: parsed.retries,
    maxTokens: parsed["max-tokens"],
    delay: parsed.delay,
  };
}

function parseCli(argv) {
  const parsed = parseArgs(argv, SPEC, USAGE);
  if (parsed.help) {
    console.log(
      formatHelp("Generate missing Japanese -> Vietnamese Kotoba Lock entries with llama.cpp.", SPEC, USAGE),
    );
    return null;
  }
  return toCamelCaseOptions(parsed.options);
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
  FALLBACK_PROMPT,
  REQUIRED_KEYS,
  GenerateError,
  buildMessages,
  chatUrl,
  extractJson,
  generateEntry,
  isValidWord,
  loadExistingWords,
  main,
  normalizeWord,
  readWords,
  requestLlm,
  run,
  saveEntry,
  targetPathFor,
  validateEntry,
};
