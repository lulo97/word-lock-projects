function ensureColumn(db, table, column, definition) {
  const columns = db.prepare(`PRAGMA table_info(${table})`).all().map((info) => info.name);

  if (!columns.includes(column)) {
    db.exec(`ALTER TABLE ${table} ADD COLUMN ${column} ${definition}`);
  }
}

function createSchema(db) {
  db.exec(`
    CREATE TABLE IF NOT EXISTS english_word (
      id INTEGER PRIMARY KEY,
      word TEXT NOT NULL,
      meaning_vi TEXT NOT NULL,
      examples TEXT NOT NULL,
      examples_vi TEXT NOT NULL DEFAULT '[]'
    )
  `);

  db.exec(`
    CREATE TABLE IF NOT EXISTS mcq (
      id INTEGER PRIMARY KEY,
      question TEXT NOT NULL,
      answers TEXT NOT NULL,
      correct_answer_idx INTEGER NOT NULL,
      explanation TEXT NOT NULL
    )
  `);

  db.exec(`
    CREATE TABLE IF NOT EXISTS japanese_word (
      id INTEGER PRIMARY KEY,
      kanji TEXT NOT NULL,
      hiragana TEXT NOT NULL,
      katakana TEXT NOT NULL,
      romaji TEXT NOT NULL DEFAULT '',
      meaning_en TEXT NOT NULL,
      examples TEXT NOT NULL,
      examples_en TEXT NOT NULL DEFAULT '[]',
      examples_vi TEXT NOT NULL DEFAULT '[]'
    )
  `);

  ensureColumn(db, 'english_word', 'examples_vi', "TEXT NOT NULL DEFAULT '[]'");
  ensureColumn(db, 'japanese_word', 'romaji', "TEXT NOT NULL DEFAULT ''");
  ensureColumn(db, 'japanese_word', 'examples_en', "TEXT NOT NULL DEFAULT '[]'");
  ensureColumn(db, 'japanese_word', 'examples_vi', "TEXT NOT NULL DEFAULT '[]'");
}

module.exports = { createSchema };
