function getRandomMcq(db) {
  const row = db.prepare('SELECT * FROM mcq ORDER BY RANDOM() LIMIT 1').get();
  if (!row) return null;

  return { ...row, answers: JSON.parse(row.answers) };
}

module.exports = { getRandomMcq };
