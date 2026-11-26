function getEnglishWord(db, id) {
  const row = db.prepare('SELECT * FROM english_word WHERE id = ?').get(id);
  if (!row) return null;

  return {
    ...row,
    examples: JSON.parse(row.examples),
    examples_vi: JSON.parse(row.examples_vi),
  };
}

module.exports = { getEnglishWord };
