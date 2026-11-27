import { DEFAULT_THEME, getThemeIds, nextThemeId } from './themes.js';
import { native } from './native.js';

// Same folder/file as the Electron build, so existing users keep their data.
// With WORDLOCK_USE_TEST_DATA=1 everything stays in memory.
const IGNORE_ERROR = "can't save ignore words";
const COUNTDOWN_SECONDS_MIN = 1;
const COUNTDOWN_SECONDS_MAX = 3600;

const defaults = {
  englishWord: true,
  multipleChoice: true,
  japaneseWord: true,
  dumbbellWorkout: true,
  masterVolume: 1,
  countdownSeconds: 60,
  chromeFreeze: false,
  autoStart: false,
  autoTheme: false,
  theme: DEFAULT_THEME,
  ignoreWords: { english: [], japanese: [] },
};

let useTestData = true;
let dataDir = null;
let settingsPath = null;
let settings = null;
let nextTaskIndex = 0;
let writeQueue = Promise.resolve();

export function normalizeVolume(value) {
  const volume = Number(value);
  if (!Number.isFinite(volume)) return defaults.masterVolume;
  return Math.min(4, Math.max(0, Math.round(volume * 2) / 2));
}

export function normalizeTheme(value) {
  const ids = getThemeIds();
  return ids.includes(value) ? value : DEFAULT_THEME;
}

export function normalizeCountdownSeconds(value) {
  const seconds = Number(value);
  if (!Number.isFinite(seconds)) return defaults.countdownSeconds;
  return Math.min(
    COUNTDOWN_SECONDS_MAX,
    Math.max(COUNTDOWN_SECONDS_MIN, Math.round(seconds)),
  );
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

export function parseIgnoreWords(value, language = 'english') {
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

async function env(key) {
  try {
    return await Neutralino.os.getEnv(key);
  } catch {
    return '';
  }
}

async function readText(path) {
  try {
    return await Neutralino.filesystem.readFile(path);
  } catch {
    return (await native.call('fs.readText', { path })).text ?? null;
  }
}

async function writeText(path, data) {
  try {
    await Neutralino.filesystem.writeFile(path, data);
  } catch {
    await native.call('fs.writeText', { path, data });
  }
}

function fromSaved(saved) {
  return {
    ...saved,
    masterVolume: normalizeVolume(saved.masterVolume),
    countdownSeconds: normalizeCountdownSeconds(saved.countdownSeconds),
    chromeFreeze: Boolean(saved.chromeFreeze),
    autoStart: Boolean(saved.autoStart),
    autoTheme: Boolean(saved.autoTheme),
    theme: normalizeTheme(saved.theme),
    ignoreWords: normalizeStoredIgnoreWords(saved.ignoreWords),
  };
}

export async function initSettings() {
  useTestData = (await env('WORDLOCK_USE_TEST_DATA')) === '1';
  const home = await env('USERPROFILE');

  if (!useTestData && home) {
    dataDir = `${home.replace(/\\/g, '/')}/pomodoro_lock`;
    settingsPath = `${dataDir}/settings.json`;
  }

  if (useTestData || !settingsPath) {
    settings = fromSaved({ ...defaults, ignoreWords: normalizeStoredIgnoreWords(defaults.ignoreWords) });
    return settings;
  }

  try {
    const text = await readText(settingsPath);
    settings = text ? fromSaved({ ...defaults, ...JSON.parse(text) }) : fromSaved(defaults);
  } catch {
    settings = fromSaved(defaults);
  }
  return settings;
}

function save() {
  if (useTestData || !settingsPath) return;
  const snapshot = JSON.stringify(settings, null, 2);
  // Serialize writes; a temp file + move keeps the last good settings intact.
  writeQueue = writeQueue
    .then(async () => {
      const tmp = `${settingsPath}.tmp`;
      await writeText(tmp, snapshot);
      try {
        await Neutralino.filesystem.move(tmp, settingsPath);
      } catch {
        await native.call('fs.move', { from: tmp, to: settingsPath });
      }
    })
    .catch(() => {});
}

export function getSettings() {
  return { ...settings, ignoreWords: getIgnoreWords() };
}

export function getIgnoreWords() {
  return {
    english: [...settings.ignoreWords.english],
    japanese: [...settings.ignoreWords.japanese],
  };
}

export function setIgnoreWords(values = {}) {
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

export function setSettings(next) {
  settings = {
    englishWord: Boolean(next.englishWord),
    multipleChoice: Boolean(next.multipleChoice),
    japaneseWord: Boolean(next.japaneseWord),
    dumbbellWorkout: Boolean(next.dumbbellWorkout),
    masterVolume:
      next.masterVolume === undefined ? settings.masterVolume : normalizeVolume(next.masterVolume),
    countdownSeconds:
      next.countdownSeconds === undefined
        ? settings.countdownSeconds
        : normalizeCountdownSeconds(next.countdownSeconds),
    chromeFreeze:
      next.chromeFreeze === undefined ? settings.chromeFreeze : Boolean(next.chromeFreeze),
    autoStart: next.autoStart === undefined ? settings.autoStart : Boolean(next.autoStart),
    autoTheme: next.autoTheme === undefined ? settings.autoTheme : Boolean(next.autoTheme),
    theme: next.theme === undefined ? settings.theme : normalizeTheme(next.theme),
    ignoreWords: getIgnoreWords(),
  };
  nextTaskIndex = 0;
  save();
}

export function advanceTheme() {
  const theme = nextThemeId(settings.theme);
  settings = { ...settings, theme };
  save();
  return theme;
}

export function getNextTaskType() {
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

export function getSettingsPath() {
  return settingsPath;
}

export { IGNORE_ERROR, COUNTDOWN_SECONDS_MIN, COUNTDOWN_SECONDS_MAX };
