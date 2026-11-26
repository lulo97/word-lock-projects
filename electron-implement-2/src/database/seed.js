function seed(db) {
  db.prepare(`
    INSERT OR IGNORE INTO english_word (id, word, meaning_vi, examples)
    VALUES (@id, @word, @meaning_vi, @examples)
  `).run({
    id: 0,
    word: 'apple',
    meaning_vi: 'quả táo',
    examples: JSON.stringify([
      'he has an apple',
      'apple is a popular company',
      'an apple a day keep the doctor away',
    ]),
  });

  db.prepare(`
    INSERT OR IGNORE INTO mcq (id, question, answers, correct_answer_idx, explanation)
    VALUES (@id, @question, @answers, @correct_answer_idx, @explanation)
  `).run({
    id: 0,
    question: '1+1=?',
    answers: JSON.stringify(['1', '2', '3', '4']),
    correct_answer_idx: 1,
    explanation: '1+1=2',
  });
}

module.exports = { seed };
