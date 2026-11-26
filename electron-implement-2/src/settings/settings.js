const fs = require('node:fs');
const path = require('node:path');
const { app } = require('electron');

const dataDir = path.join(app.getPath('home'), 'pomodoro_lock');
const settingsPath = path.join(dataDir, 'settings.json');

const IGNORE_ERROR = "can't save ignore words";

const defaults = {
  englishWord: true,
  multipleChoice: true,
  japaneseWord: true,
  dumbbellWorkout: true,
  masterVolume: 1,
  chromeFreeze: false,
  ignoreWords: { english: [], japanese: [] },
};

function normalizeVolume(value) {
  const volume = Number(value);
  if (!Number.isFinite(volume)) return defaults.masterVolume;
  return Math.min(4, Math.max(0, Math.round(volume * 2) / 2));
}

function normalizeStoredList(value) {
  if (!Array.isArray(value)) return [];
  return value
    .filter((item) => typeof item === 'string' && item.trim())
    .map((item) => item.trim());
}

function normalizeStoredIgnoreWords(value) {
  const source = value && typeof value === 'object' ? value : {};
  return {
    english: normalizeStoredList(source.english),
    japanese: normalizeStoredList(source.japanese),
  };
}

// Kanji (incl. 々 and 〆), hiragana, katakana (incl. ー). Romaji/English are rejected.
const JAPANESE_WORD_PATTERN =
  /^[\u3005\u3006\u3041-\u309F\u30A0-\u30FF\u3400-\u4DBF\u4E00-\u9FFF\uF900-\uFAFF]+$/;

// Accepts either the raw textarea string or an already-split list.
// Only "word1, word2, word3" on a single line is valid.
// Duplicates are kept on purpose: the same word in kanji/hiragana/katakana
// is a valid list entry.
function parseIgnoreWords(value, language = 'english') {
  if (language !== 'english' && language !== 'japanese') throw new Error(IGNORE_ERROR);

  let words;

  if (typeof value === 'string') {
    if (/[\r\n]/.test(value)) throw new Error(IGNORE_ERROR);
    if (!value.trim()) return [];
    words = value.split(',').map((word) => word.trim());
  } else if (Array.isArray(value)) {
    words = value.map((word) => (typeof word === 'string' ? word.trim() : ''));
  } else {
    throw new Error(IGNORE_ERROR);
  }

  if (words.some((word) => !word || /\s/.test(word))) throw new Error(IGNORE_ERROR);
  if (language === 'japanese' && words.some((word) => !JAPANESE_WORD_PATTERN.test(word))) {
    throw new Error(IGNORE_ERROR);
  }

  return words;
}

function load() {
  try {
    const saved = { ...defaults, ...JSON.parse(fs.readFileSync(settingsPath, 'utf8')) };
    return {
      ...saved,
      masterVolume: normalizeVolume(saved.masterVolume),
      chromeFreeze: Boolean(saved.chromeFreeze),
      ignoreWords: normalizeStoredIgnoreWords(saved.ignoreWords),
    };
  } catch {
    return { ...defaults, ignoreWords: normalizeStoredIgnoreWords(defaults.ignoreWords) };
  }
}

let settings = load();
let nextTaskIndex = 0;

function save() {
  fs.mkdirSync(dataDir, { recursive: true });
  fs.writeFileSync(settingsPath, JSON.stringify(settings, null, 2));
}

function getSettings() {
  return { ...settings, ignoreWords: getIgnoreWords() };
}

function getIgnoreWords() {
  return {
    english: [...settings.ignoreWords.english],
    japanese: [...settings.ignoreWords.japanese],
  };
}

function setIgnoreWords(values = {}) {
  const next = {};
  for (const language of ['english', 'japanese']) {
    next[language] =
      values[language] === undefined
        ? [...settings.ignoreWords[language]]
        : parseIgnoreWords(values[language], language);
  }

  settings = { ...settings, ignoreWords: next };
  save();
  return getIgnoreWords();
}

function setSettings(next) {
  settings = {
    englishWord: Boolean(next.englishWord),
    multipleChoice: Boolean(next.multipleChoice),
    japaneseWord: Boolean(next.japaneseWord),
    dumbbellWorkout: Boolean(next.dumbbellWorkout),
    masterVolume:
      next.masterVolume === undefined ? settings.masterVolume : normalizeVolume(next.masterVolume),
    chromeFreeze:
      next.chromeFreeze === undefined ? settings.chromeFreeze : Boolean(next.chromeFreeze),
    ignoreWords: getIgnoreWords(),
  };
  nextTaskIndex = 0;
  save();
}

function getNextTaskType() {
  const enabled = [];

  if (settings.englishWord) enabled.push('english');
  if (settings.multipleChoice) enabled.push('mcq');
  if (settings.japaneseWord) enabled.push('japanese');
  if (settings.dumbbellWorkout) enabled.push('dumbbell');

  if (enabled.length === 0) return 'english';

  const task = enabled[nextTaskIndex % enabled.length];
  nextTaskIndex += 1;

  return task;
}

module.exports = {
  getSettings,
  setSettings,
  getNextTaskType,
  getIgnoreWords,
  setIgnoreWords,
  parseIgnoreWords,
  normalizeVolume,
  IGNORE_ERROR,
  settingsPath,
};
