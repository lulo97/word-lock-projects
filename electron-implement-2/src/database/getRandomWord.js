function getRandomWord(db, ignoredWords = []) {
  const ignored = (ignoredWords || [])
    .map((word) => String(word).toLowerCase())
    .filter(Boolean);

  let row;
  if (ignored.length) {
    const placeholders = ignored.map(() => '?').join(', ');
    row = db
      .prepare(`SELECT * FROM english_word WHERE LOWER(word) NOT IN (${placeholders}) ORDER BY RANDOM() LIMIT 1`)
      .get(...ignored);
  } else {
    row = db.prepare('SELECT * FROM english_word ORDER BY RANDOM() LIMIT 1').get();
  }

  if (!row) return null;

  return {
    ...row,
    examples: JSON.parse(row.examples),
    examples_vi: JSON.parse(row.examples_vi),
  };
}

module.exports = { getRandomWord };
