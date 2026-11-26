function getRandomJapaneseWord(db) {
  const row = db.prepare('SELECT * FROM japanese_word ORDER BY RANDOM() LIMIT 1').get();
  if (!row) return null;

  return {
    ...row,
    examples: JSON.parse(row.examples),
    examples_en: JSON.parse(row.examples_en),
    examples_vi: JSON.parse(row.examples_vi),
  };
}

module.exports = { getRandomJapaneseWord };
