function getMcq(db, id) {
  const row = db.prepare('SELECT * FROM mcq WHERE id = ?').get(id);
  if (!row) return null;

  return { ...row, answers: JSON.parse(row.answers) };
}

module.exports = { getMcq };
