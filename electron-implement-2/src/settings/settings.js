const fs = require('node:fs');
const path = require('node:path');
const { app } = require('electron');

const dataDir = path.join(app.getPath('home'), 'pomodoro_lock');
const settingsPath = path.join(dataDir, 'settings.json');

const defaults = {
  englishWord: true,
  multipleChoice: true,
  japaneseWord: true,
  dumbbellWorkout: true,
  masterVolume: 1,
};

function normalizeVolume(value) {
  const volume = Number(value);
  if (!Number.isFinite(volume)) return defaults.masterVolume;
  return Math.min(4, Math.max(0, Math.round(volume * 2) / 2));
}

function load() {
  try {
    const saved = { ...defaults, ...JSON.parse(fs.readFileSync(settingsPath, 'utf8')) };
    return { ...saved, masterVolume: normalizeVolume(saved.masterVolume) };
  } catch {
    return { ...defaults };
  }
}

let settings = load();
let nextTaskIndex = 0;

function save() {
  fs.mkdirSync(dataDir, { recursive: true });
  fs.writeFileSync(settingsPath, JSON.stringify(settings, null, 2));
}

function getSettings() {
  return { ...settings };
}

function setSettings(next) {
  settings = {
    englishWord: Boolean(next.englishWord),
    multipleChoice: Boolean(next.multipleChoice),
    japaneseWord: Boolean(next.japaneseWord),
    dumbbellWorkout: Boolean(next.dumbbellWorkout),
    masterVolume:
      next.masterVolume === undefined ? settings.masterVolume : normalizeVolume(next.masterVolume),
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

module.exports = { getSettings, setSettings, getNextTaskType, normalizeVolume, settingsPath };
