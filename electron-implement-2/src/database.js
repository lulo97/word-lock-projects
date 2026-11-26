const fs = require('node:fs');
const path = require('node:path');
const { app } = require('electron');
const Database = require('better-sqlite3');

const dataDir = path.join(app.getPath('home'), 'pomodoro_lock');
const dbPath = path.join(dataDir, 'pomodoro_lock.db');

fs.mkdirSync(dataDir, { recursive: true });

const db = new Database(dbPath);

db.exec(`
  CREATE TABLE IF NOT EXISTS english_word (
    id INTEGER PRIMARY KEY,
    word TEXT NOT NULL,
    meaning_vi TEXT NOT NULL,
    examples TEXT NOT NULL
  )
`);

db.prepare(`
  INSERT OR IGNORE INTO english_word (id, word, meaning_vi, examples)
  VALUES (@id, @word, @meaning_vi, @examples)
`).run({
  id: 0,
  word: 'apple',
  meaning_vi: 'quả táo',
  examples: JSON.stringify([
    'he has an apple',
    'apple is a popular company',
    'an apple a day keep the doctor away',
  ]),
});

function getEnglishWord(id) {
  const row = db.prepare('SELECT * FROM english_word WHERE id = ?').get(id);
  if (!row) return null;

  return { ...row, examples: JSON.parse(row.examples) };
}

function getRandomWord() {
  const row = db.prepare('SELECT * FROM english_word ORDER BY RANDOM() LIMIT 1').get();
  if (!row) return null;

  return { ...row, examples: JSON.parse(row.examples) };
}

module.exports = { db, dbPath, getEnglishWord, getRandomWord };
