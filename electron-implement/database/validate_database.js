#!/usr/bin/env node
"use strict";

/**
 * Validate every Kotoba Lock (Japanese -> Vietnamese) .json entry.
 *
 * Word files live one folder per word:
 *   database/words/<word>/<word>.json
 * so this script searches subfolders too.
 *
 * Standalone: uses only the Node.js standard library.
 *
 * Usage:
 *   node database/validate_database.js
 *
 * Exit code 0 = all files valid, 1 = at least one invalid file.
 * A detailed report is written to database/validation_errors.log.
 */

const fs = require("fs");
const path = require("path");

const {
  DATABASE_DIR,
  WORDS_DIR,
  preview,
  safeConsole,
  typeName,
  walkJsonFiles,
} = require("./utils");

const LOG_PATH = path.join(DATABASE_DIR, "validation_errors.log");

const REQUIRED_TOP_LEVEL = [
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

const STRING_LISTS = ["synonyms", "antonyms", "tags"];

// Legacy English -> Vietnamese keys that the app no longer reads.
const LEGACY_KEYS = new Set(["english", "ipa"]);

function has(object, key) {
  return Object.prototype.hasOwnProperty.call(object, key);
}

function isPlainObject(value) {
  return value !== null && typeof value === "object" && !Array.isArray(value);
}

function isString(value) {
  return typeof value === "string";
}

function checkString(value, valuePath, allowEmpty = false) {
  /** Return an error message, or null if the value is a valid string. */
  if (!isString(value)) {
    return `${valuePath}: expected string, got ${typeName(value)} (${preview(value)})`;
  }
  if (!allowEmpty && !value.trim()) {
    return `${valuePath}: must not be an empty string`;
  }
  return null;
}

function checkStringList(value, valuePath) {
  const errors = [];
  if (!Array.isArray(value)) {
    return [`${valuePath}: expected list of strings, got ${typeName(value)}`];
  }
  value.forEach((item, index) => {
    const message = checkString(item, `${valuePath}[${index}]`);
    if (message) errors.push(message);
  });
  return errors;
}

function checkObject(value, valuePath, requiredKeys) {
  /** Validate an object and its required keys, returning [errors, object|null]. */
  if (!isPlainObject(value)) {
    return [[`${valuePath}: expected object, got ${typeName(value)}`], null];
  }

  const errors = [];
  for (const key of requiredKeys) {
    if (!has(value, key)) {
      errors.push(`${valuePath}.${key}: missing required key`);
    }
  }
  return [errors, value];
}

function validateEntry(data, filename) {
  /** Validate one parsed JSON entry. Returns { errors, warnings }. */
  const errors = [];
  const warnings = [];

  if (!isPlainObject(data)) {
    return {
      errors: [`$: top-level must be a JSON object, got ${typeName(data)}`],
      warnings: [],
    };
  }

  const fileName = typeof filename === "string" ? path.basename(filename) : String(filename);
  const fileStem = fileName.replace(/\.json$/i, "");

  for (const key of REQUIRED_TOP_LEVEL) {
    if (!has(data, key)) {
      errors.push(`$.${key}: missing required key`);
    }
  }

  const extraKeys = Object.keys(data)
    .filter((key) => !REQUIRED_TOP_LEVEL.includes(key))
    .sort();
  for (const key of extraKeys) {
    warnings.push(`$.${key}: unknown key (ignored by the app)`);
  }

  if (has(data, "word")) {
    const message = checkString(data.word, "$.word");
    if (message) {
      errors.push(message);
    } else {
      if (fileStem.normalize("NFC") !== data.word.normalize("NFC")) {
        warnings.push(`$.word: '${data.word}' does not match filename '${fileName}'`);
      }
      const folder = path.basename(path.dirname(String(filename)));
      if (folder && folder !== "." && folder !== fileStem) {
        warnings.push(`$.word: file is not inside a folder named '${data.word}'`);
      }
    }
  }

  if (has(data, "language")) {
    const [languageErrors, language] = checkObject(data.language, "$.language", [
      "source",
      "target",
    ]);
    errors.push(...languageErrors);
    if (language !== null) {
      for (const key of ["source", "target"]) {
        if (has(language, key)) {
          const message = checkString(language[key], `$.language.${key}`, true);
          if (message) errors.push(message);
        }
      }
      if (language.source !== "ja") {
        warnings.push(`$.language.source: expected 'ja', got '${language.source}'`);
      }
      if (language.target !== "vi") {
        warnings.push(`$.language.target: expected 'vi', got '${language.target}'`);
      }
    }
  }

  if (has(data, "pronunciation")) {
    const [pronunciationErrors, pronunciation] = checkObject(data.pronunciation, "$.pronunciation", [
      "kana",
      "audio",
    ]);
    errors.push(...pronunciationErrors);
    if (pronunciation !== null) {
      for (const key of ["kana", "audio"]) {
        if (has(pronunciation, key)) {
          const message = checkString(pronunciation[key], `$.pronunciation.${key}`, true);
          if (message) errors.push(message);
        }
      }
      if (has(pronunciation, "romaji")) {
        warnings.push("$.pronunciation.romaji: romaji is computed by the app and should not be stored");
      }
      if (has(pronunciation, "ipa")) {
        warnings.push("$.pronunciation.ipa: legacy English field; use 'kana' instead");
      }
    }
  }

  if (has(data, "part_of_speech")) {
    const message = checkString(data.part_of_speech, "$.part_of_speech", true);
    if (message) errors.push(message);
  }

  if (has(data, "definitions")) {
    const definitions = data.definitions;
    if (!Array.isArray(definitions)) {
      errors.push(`$.definitions: expected list of objects, got ${typeName(definitions)}`);
    } else if (definitions.length === 0) {
      errors.push("$.definitions: must contain at least one definition");
    } else {
      definitions.forEach((rawDefinition, index) => {
        const valuePath = `$.definitions[${index}]`;
        const [definitionErrors, definition] = checkObject(rawDefinition, valuePath, [
          "japanese",
          "vietnamese",
        ]);
        errors.push(...definitionErrors);
        if (definition !== null) {
          for (const key of ["japanese", "vietnamese"]) {
            if (has(definition, key)) {
              const message = checkString(definition[key], `${valuePath}.${key}`);
              if (message) errors.push(message);
            }
          }
          for (const legacy of LEGACY_KEYS) {
            if (has(definition, legacy)) {
              warnings.push(`${valuePath}.${legacy}: legacy field; use 'japanese' instead`);
            }
          }
        }
      });
    }
  }

  if (has(data, "examples")) {
    const examples = data.examples;
    if (!Array.isArray(examples)) {
      errors.push(`$.examples: expected list of objects, got ${typeName(examples)}`);
    } else {
      examples.forEach((rawExample, index) => {
        const valuePath = `$.examples[${index}]`;
        const [exampleErrors, example] = checkObject(rawExample, valuePath, [
          "japanese",
          "vietnamese",
        ]);
        errors.push(...exampleErrors);
        if (example !== null) {
          for (const key of ["japanese", "vietnamese"]) {
            if (has(example, key)) {
              const message = checkString(example[key], `${valuePath}.${key}`);
              if (message) errors.push(message);
            }
          }
          if (has(example, "audio")) {
            const message = checkString(example.audio, `${valuePath}.audio`, true);
            if (message) errors.push(message);
          }
          if (has(example, "english")) {
            warnings.push(`${valuePath}.english: legacy field; use 'japanese' instead`);
          }
        }
      });
    }
  }

  for (const key of STRING_LISTS) {
    if (has(data, key)) {
      errors.push(...checkStringList(data[key], `$.${key}`));
    }
  }

  return { errors, warnings };
}

function validateFile(filePath) {
  /** Read and validate one file. Returns { errors, warnings }. */
  let text;
  try {
    const buffer = fs.readFileSync(filePath);
    try {
      text = new TextDecoder("utf-8", { fatal: true }).decode(buffer);
    } catch (error) {
      return { errors: [`file is not valid UTF-8: ${error.message}`], warnings: [] };
    }
  } catch (error) {
    return { errors: [`cannot read file: ${error.message}`], warnings: [] };
  }

  let data;
  try {
    data = JSON.parse(text);
  } catch (error) {
    return { errors: [`invalid JSON: ${error.message}`], warnings: [] };
  }

  return validateEntry(data, filePath);
}

function timestamp() {
  const now = new Date();
  const pad = (value) => String(value).padStart(2, "0");
  return (
    `${now.getFullYear()}-${pad(now.getMonth() + 1)}-${pad(now.getDate())} ` +
    `${pad(now.getHours())}:${pad(now.getMinutes())}:${pad(now.getSeconds())}`
  );
}

function main() {
  safeConsole();
  const started = timestamp();
  const files = walkJsonFiles(WORDS_DIR);

  const lines = [
    `[${started}] Kotoba Lock database validation (Japanese -> Vietnamese)`,
    `Directory: ${WORDS_DIR}`,
    `Files found: ${files.length}`,
    "-".repeat(72),
  ];
  console.log(lines[0]);
  console.log(lines[1]);
  console.log(lines[2]);

  let invalidFiles = 0;
  let warningCount = 0;

  for (const filePath of files) {
    const { errors, warnings } = validateFile(filePath);
    warningCount += warnings.length;
    const name = path.basename(filePath);

    if (errors.length) {
      invalidFiles += 1;
      lines.push(`INVALID ${name} (${errors.length} error(s))`);
      console.log(`INVALID ${name} (${errors.length} error(s))`);
      for (const message of errors) {
        const line = `  ERROR ${name}: ${message}`;
        lines.push(line);
        console.log(line);
      }
    } else {
      lines.push(`OK      ${name}`);
      console.log(`OK      ${name}`);
    }

    for (const message of warnings) {
      const line = `  WARN  ${name}: ${message}`;
      lines.push(line);
      console.log(line);
    }
  }

  const summary =
    `${files.length} file(s) checked: ` +
    `${files.length - invalidFiles} valid, ` +
    `${invalidFiles} invalid, ` +
    `${warningCount} warning(s)`;
  lines.push("-".repeat(72));
  lines.push(summary);
  console.log("-".repeat(72));
  console.log(summary);

  try {
    fs.writeFileSync(LOG_PATH, `${lines.join("\n")}\n`, "utf8");
    console.log(`Report written to ${LOG_PATH}`);
  } catch (error) {
    console.log(`Could not write log file: ${error.message}`);
  }

  return invalidFiles ? 1 : 0;
}

if (require.main === module) {
  process.exit(main());
}

module.exports = {
  LOG_PATH,
  REQUIRED_TOP_LEVEL,
  STRING_LISTS,
  checkObject,
  checkString,
  checkStringList,
  main,
  validateEntry,
  validateFile,
};
