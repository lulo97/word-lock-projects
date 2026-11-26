function getRandomJapaneseWord(db, ignoredWords = []) {
  const ignored = (ignoredWords || [])
    .map((word) => String(word).toLowerCase())
    .filter(Boolean);

  let row;
  if (ignored.length) {
    const conditions = [
      'LOWER(kanji) NOT IN (PLACEHOLDERS)',
      'LOWER(hiragana) NOT IN (PLACEHOLDERS)',
      'LOWER(katakana) NOT IN (PLACEHOLDERS)',
      'LOWER(romaji) NOT IN (PLACEHOLDERS)',
    ].join(' AND ');
    const sql = `SELECT * FROM japanese_word WHERE ${conditions} ORDER BY RANDOM() LIMIT 1`;
    const placeholders = ignored.map(() => '?').join(', ');
    row = db
      .prepare(sql.replaceAll('PLACEHOLDERS', placeholders))
      .get(...ignored, ...ignored, ...ignored, ...ignored);
  } else {
    row = db.prepare('SELECT * FROM japanese_word ORDER BY RANDOM() LIMIT 1').get();
  }

  if (!row) return null;

  return {
    ...row,
    examples: JSON.parse(row.examples),
    examples_en: JSON.parse(row.examples_en),
    examples_vi: JSON.parse(row.examples_vi),
  };
}

module.exports = { getRandomJapaneseWord };
