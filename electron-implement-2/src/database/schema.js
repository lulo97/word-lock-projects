function createSchema(db) {
  db.exec(`
    CREATE TABLE IF NOT EXISTS english_word (
      id INTEGER PRIMARY KEY,
      word TEXT NOT NULL,
      meaning_vi TEXT NOT NULL,
      examples TEXT NOT NULL
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
}

module.exports = { createSchema };
