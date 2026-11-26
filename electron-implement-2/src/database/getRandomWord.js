function getRandomWord(db) {
  const row = db.prepare('SELECT * FROM english_word ORDER BY RANDOM() LIMIT 1').get();
  if (!row) return null;

  return { ...row, examples: JSON.parse(row.examples) };
}

module.exports = { getRandomWord };
