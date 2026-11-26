function getJapaneseWord(db, id) {
  const row = db.prepare('SELECT * FROM japanese_word WHERE id = ?').get(id);
  if (!row) return null;

  return {
    ...row,
    examples: JSON.parse(row.examples),
    examples_en: JSON.parse(row.examples_en),
    examples_vi: JSON.parse(row.examples_vi),
  };
}

module.exports = { getJapaneseWord };
