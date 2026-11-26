const fs = require('node:fs');
const path = require('node:path');
const { app } = require('electron');

const dataDir = path.join(app.getPath('home'), 'pomodoro_lock');
const settingsPath = path.join(dataDir, 'settings.json');

const defaults = {
  englishWord: true,
  multipleChoice: true,
};

function load() {
  try {
    return { ...defaults, ...JSON.parse(fs.readFileSync(settingsPath, 'utf8')) };
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
  };
  nextTaskIndex = 0;
  save();
}

function getNextTaskType() {
  const enabled = [];

  if (settings.englishWord) enabled.push('word');
  if (settings.multipleChoice) enabled.push('mcq');

  if (enabled.length === 0) return 'word';

  const task = enabled[nextTaskIndex % enabled.length];
  nextTaskIndex += 1;

  return task;
}

module.exports = { getSettings, setSettings, getNextTaskType, settingsPath };
