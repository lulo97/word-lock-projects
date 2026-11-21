"use strict";

/**
 * Shared helpers for the Kotoba Lock database scripts.
 *
 * This is the Node.js counterpart of the small utility functions that used to
 * live inside the Python scripts. It contains no business logic.
 *
 * Database layout (Japanese -> Vietnamese):
 *   database/words/<word>/<word>.json
 *   database/words/<word>/audios/<word>.ogg
 *   database/words/<word>/audios_example/<romaji_sentence>.ogg
 *
 * Example audio names are the normalised romaji of the sentence, produced by
 * lib/romaji.js (Kuroshiro), e.g. 友達と魚 -> tomodachi_to_sakana.ogg.
 *
 * Audio paths stored inside a JSON entry are relative to DATABASE_DIR, which
 * matches lib/config.js in the Electron app.
 */

const fs = require("fs");
const os = require("os");
const path = require("path");

const DATABASE_DIR = __dirname;
const WORDS_DIR = path.join(DATABASE_DIR, "words");

/** No-op kept for parity with the Python scripts (safe_console). */
function safeConsole() {}

function sleep(seconds) {
  return new Promise((resolve) => setTimeout(resolve, seconds * 1000));
}

function toPosix(value) {
  return String(value).split(path.sep).join("/");
}

function isFile(filePath) {
  try {
    return fs.statSync(filePath).isFile();
  } catch {
    return false;
  }
}

function fileExists(filePath) {
  try {
    return fs.existsSync(filePath);
  } catch {
    return false;
  }
}

/** Return every *.json file under `dir`, sorted like Python's sorted(Path). */
function walkJsonFiles(dir) {
  const results = [];
  let entries;
  try {
    entries = fs.readdirSync(dir, { withFileTypes: true });
  } catch {
    return results;
  }
  for (const entry of entries) {
    const full = path.join(dir, entry.name);
    if (entry.isDirectory()) {
      results.push(...walkJsonFiles(full));
    } else if (entry.isFile() && entry.name.toLowerCase().endsWith(".json")) {
      results.push(full);
    }
  }
  results.sort();
  return results;
}

/** Write JSON atomically: write `<file>.tmp`, then rename over the target. */
function atomicWriteJson(filePath, data) {
  const temp = `${filePath}.tmp`;
  fs.mkdirSync(path.dirname(filePath), { recursive: true });
  fs.writeFileSync(temp, `${JSON.stringify(data, null, 2)}\n`, "utf8");
  fs.renameSync(temp, filePath);
}

/** Python-style type name, used in validation messages. */
function typeName(value) {
  if (value === null) return "null";
  if (Array.isArray(value)) return "list";
  switch (typeof value) {
    case "string":
      return "str";
    case "boolean":
      return "bool";
    case "number":
      return Number.isInteger(value) ? "int" : "float";
    case "object":
      return "dict";
    default:
      return typeof value;
  }
}

/** Short, Python-repr-like preview of a value. */
function preview(value, limit = 60) {
  let text;
  try {
    text = JSON.stringify(value);
  } catch {
    text = String(value);
  }
  if (text === undefined) text = String(value);
  return text.length <= limit ? text : `${text.slice(0, limit - 3)}...`;
}

/**
 * Minimal argparse-like parser.
 *
 * spec: {
 *   input: { type: "string", required: true, help: "..." },
 *   retries: { type: "int", default: 2, help: "..." },
 *   word: { type: "string", action: "append", default: [], help: "..." },
 *   force: { type: "boolean", default: false, help: "..." },
 * }
 *
 * Supported forms: `--name value`, `--name=value`, `--flag`.
 * Returns { options, positionals, help } where help is true for --help/-h.
 */
function parseArgs(argv, spec, usage = "") {
  const options = {};
  for (const [name, config] of Object.entries(spec)) {
    if (config.action === "append") {
      options[name] = [...(config.default || [])];
    } else if (config.type === "boolean") {
      options[name] = config.default === true;
    } else {
      options[name] = config.default;
    }
  }

  const positionals = [];
  let showHelp = false;

  for (let index = 0; index < argv.length; index += 1) {
    const token = argv[index];

    if (token === "-h" || token === "--help") {
      showHelp = true;
      continue;
    }

    if (!token.startsWith("--")) {
      positionals.push(token);
      continue;
    }

    let name = token.slice(2);
    let rawValue;
    let hasValue = false;

    const equals = name.indexOf("=");
    if (equals !== -1) {
      rawValue = name.slice(equals + 1);
      name = name.slice(0, equals);
      hasValue = true;
    }

    const config = spec[name];
    if (!config) {
      throw new Error(`unknown option: --${name}`);
    }

    if (config.type === "boolean") {
      if (hasValue) {
        options[name] = !["0", "false", "no", ""].includes(rawValue.toLowerCase());
      } else {
        options[name] = true;
      }
      continue;
    }

    if (!hasValue) {
      index += 1;
      if (index >= argv.length) {
        throw new Error(`option --${name} requires a value`);
      }
      rawValue = argv[index];
    }

    if (config.type === "int" || config.type === "float") {
      const number =
        config.type === "int" ? Number.parseInt(rawValue, 10) : Number.parseFloat(rawValue);
      if (Number.isNaN(number)) {
        throw new Error(`invalid value for --${name}: ${rawValue}`);
      }
      if (config.action === "append") {
        options[name].push(number);
      } else {
        options[name] = number;
      }
      continue;
    }

    if (config.action === "append") {
      options[name].push(rawValue);
    } else {
      options[name] = rawValue;
    }
  }

  if (!showHelp) {
    for (const [name, config] of Object.entries(spec)) {
      if (config.required && (options[name] === undefined || options[name] === null)) {
        throw new Error(`the following argument is required: --${name}`);
      }
    }
  }

  return { options, positionals, help: showHelp };
}

/** Render a simple help page from the spec. */
function formatHelp(description, spec, usage) {
  const lines = [description, "", `Usage: ${usage}`, ""];
  for (const [name, config] of Object.entries(spec)) {
    const type = config.type === "boolean" ? "" : " VALUE";
    const fallback =
      config.default !== undefined && config.default !== "" && !Array.isArray(config.default)
        ? ` (default: ${config.default})`
        : "";
    lines.push(`  --${name}${type}${fallback}`);
    if (config.help) lines.push(`      ${config.help}`);
  }
  return lines.join("\n");
}

/** Resolve an input file: absolute path, or relative to cwd, then DATABASE_DIR. */
function resolveInput(raw) {
  const expanded = String(raw).replace(/^~(?=$|[\\/])/, os.homedir());
  if (path.isAbsolute(expanded)) {
    return isFile(expanded) ? expanded : null;
  }
  for (const base of [process.cwd(), DATABASE_DIR]) {
    const full = path.resolve(base, expanded);
    if (isFile(full)) return full;
  }
  return null;
}

module.exports = {
  DATABASE_DIR,
  WORDS_DIR,
  safeConsole,
  sleep,
  toPosix,
  isFile,
  fileExists,
  walkJsonFiles,
  atomicWriteJson,
  typeName,
  preview,
  parseArgs,
  formatHelp,
  resolveInput,
};
