const fs = require('node:fs');
const path = require('node:path');
const { app } = require('electron');
const Database = require('better-sqlite3');

const { createSchema } = require('./schema');
const { seed } = require('./seed');
const { getEnglishWord: selectEnglishWord } = require('./getEnglishWord');
const { getRandomWord: selectRandomWord } = require('./getRandomWord');
const { getMcq: selectMcq } = require('./getMcq');
const { getRandomMcq: selectRandomMcq } = require('./getRandomMcq');
const { getJapaneseWord: selectJapaneseWord } = require('./getJapaneseWord');
const { getRandomJapaneseWord: selectRandomJapaneseWord } = require('./getRandomJapaneseWord');
const { getIgnoreWords } = require('../settings/settings');

// Playwright runs with WORDLOCK_USE_TEST_DATA=1. In that mode the database is
// in memory and seeded with deterministic fake rows, so tests never touch the
// user's real pomodoro_lock.db in their home folder.
const USE_TEST_DATA = process.env.WORDLOCK_USE_TEST_DATA === '1';
const dataDir = USE_TEST_DATA ? null : path.join(app.getPath('home'), 'pomodoro_lock');
const dbPath = dataDir ? path.join(dataDir, 'pomodoro_lock.db') : ':memory:';

if (dataDir) fs.mkdirSync(dataDir, { recursive: true });

const db = new Database(dbPath);

createSchema(db);
seed(db);

function getEnglishWord(id) {
  return selectEnglishWord(db, id);
}

function getRandomWord() {
  return selectRandomWord(db, getIgnoreWords().english);
}

function getMcq(id) {
  return selectMcq(db, id);
}

function getRandomMcq() {
  return selectRandomMcq(db);
}

function getJapaneseWord(id) {
  return selectJapaneseWord(db, id);
}

function getRandomJapaneseWord() {
  return selectRandomJapaneseWord(db, getIgnoreWords().japanese);
}

module.exports = {
  db,
  dbPath,
  getEnglishWord,
  getRandomWord,
  getMcq,
  getRandomMcq,
  getJapaneseWord,
  getRandomJapaneseWord,
};
