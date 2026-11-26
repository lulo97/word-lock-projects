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

const dataDir = path.join(app.getPath('home'), 'pomodoro_lock');
const dbPath = path.join(dataDir, 'pomodoro_lock.db');

fs.mkdirSync(dataDir, { recursive: true });

const db = new Database(dbPath);

createSchema(db);
seed(db);

function getEnglishWord(id) {
  return selectEnglishWord(db, id);
}

function getRandomWord() {
  return selectRandomWord(db);
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
  return selectRandomJapaneseWord(db);
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
