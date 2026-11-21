const fs = require("fs");
const path = require("path");

const config = require("./config");

const CONFIG_FILE = path.join(config.DATA_DIR, "config.json");

const IGNORE_KEYS = ["IGNORE_WORDS", "ignore_words", "ignored_words"];
const MIN_SOUND_VOLUME = 0.0;
const MAX_SOUND_VOLUME = 4.0;

function defaults() {
  return {
    AUTO_SHOW_DELAY_MS: config.AUTO_SHOW_DELAY_MS,
    IGNORE_WORDS: [],
    SOUND_VOLUME: config.SOUND_VOLUME,
    START_WITH_WINDOWS: false,
  };
}

function write(data) {
  fs.mkdirSync(config.DATA_DIR, { recursive: true });
  const temp = CONFIG_FILE.replace(/\.json$/i, ".json.tmp");
  fs.writeFileSync(temp, JSON.stringify(data, null, 2) + "\n", "utf8");
  fs.renameSync(temp, CONFIG_FILE);
}

function ensureFile() {
  if (fs.existsSync(CONFIG_FILE)) return;
  try {
    write(defaults());
  } catch (error) {
    // A read-only profile should not stop the application from starting.
  }
}

function readRaw() {
  try {
    const parsed = JSON.parse(fs.readFileSync(CONFIG_FILE, "utf8"));
    return parsed && typeof parsed === "object" && !Array.isArray(parsed)
      ? parsed
      : {};
  } catch (error) {
    return {};
  }
}

function readBool(value) {
  if (typeof value === "boolean") return value;
  if (typeof value === "number") return Boolean(value);
  if (typeof value === "string") {
    return ["1", "true", "yes", "on"].includes(value.trim().toLowerCase());
  }
  return false;
}

function readIgnoreWords(data) {
  for (const key of IGNORE_KEYS) {
    const values = data[key];
    if (Array.isArray(values)) {
      return values
        .filter((value) => typeof value === "string" && value.trim())
        .map((value) => value.trim().toLowerCase());
    }
  }
  return [];
}

function load() {
  ensureFile();
  const data = readRaw();
  return {
    AUTO_SHOW_DELAY_MS: data.AUTO_SHOW_DELAY_MS,
    IGNORE_WORDS: readIgnoreWords(data),
    SOUND_VOLUME: data.SOUND_VOLUME,
    START_WITH_WINDOWS: readBool(data.START_WITH_WINDOWS),
  };
}

function autoShowDelayMs() {
  let value = load().AUTO_SHOW_DELAY_MS;
  if (typeof value === "boolean") return config.AUTO_SHOW_DELAY_MS;
  if (typeof value === "string") {
    const parsed = Number(value.trim());
    value = Number.isFinite(parsed) ? parsed : NaN;
  }
  if (typeof value === "number" && Number.isFinite(value) && value > 0) {
    return Math.trunc(value);
  }
  return config.AUTO_SHOW_DELAY_MS;
}

function ignoredWords() {
  return new Set(load().IGNORE_WORDS);
}

function clampVolume(value) {
  const volume = Number(value);
  if (!Number.isFinite(volume)) return config.SOUND_VOLUME;
  return Math.min(MAX_SOUND_VOLUME, Math.max(MIN_SOUND_VOLUME, volume));
}

function soundVolume() {
  return clampVolume(load().SOUND_VOLUME);
}

function startWithWindows() {
  return load().START_WITH_WINDOWS;
}

function addIgnoredWord(word) {
  const key = String(word || "").trim().toLowerCase();
  if (!key) return true;

  const data = load();
  const words = [...data.IGNORE_WORDS];
  if (words.includes(key)) return true;

  words.push(key);
  saveConfig(
    autoShowDelayMs(),
    words,
    clampVolume(data.SOUND_VOLUME),
    data.START_WITH_WINDOWS,
  );
  return true;
}

function parseWords(text) {
  return String(text || "")
    .trim()
    .split(/[\s,;]+/)
    .filter(Boolean);
}

function saveConfig(autoShowDelayMsValue, ignoreWords, soundVolumeValue, startWithWindowsValue) {
  ensureFile();

  // Preserve keys written by future versions or by hand.
  const data = readRaw();
  for (const key of IGNORE_KEYS) delete data[key];

  const words = [];
  const seen = new Set();
  for (const word of ignoreWords || []) {
    const key = String(word || "").trim().toLowerCase();
    if (key && !seen.has(key)) {
      seen.add(key);
      words.push(key);
    }
  }

  data.AUTO_SHOW_DELAY_MS = Math.trunc(Number(autoShowDelayMsValue));
  data.IGNORE_WORDS = words;
  data.SOUND_VOLUME = Number(
    clampVolume(
      soundVolumeValue === undefined ? config.SOUND_VOLUME : soundVolumeValue,
    ).toFixed(1),
  );
  if (startWithWindowsValue === undefined) {
    data.START_WITH_WINDOWS = readBool(data.START_WITH_WINDOWS);
  } else {
    data.START_WITH_WINDOWS = Boolean(startWithWindowsValue);
  }

  write(data);
  return data;
}

module.exports = {
  CONFIG_FILE,
  MIN_SOUND_VOLUME,
  MAX_SOUND_VOLUME,
  load,
  autoShowDelayMs,
  ignoredWords,
  soundVolume,
  startWithWindows,
  addIgnoredWord,
  saveConfig,
  parseWords,
  readBool,
  clampVolume,
  ensureFile,
};
