function seed(db) {
  db.prepare(`
    INSERT INTO english_word (id, word, meaning_vi, examples, examples_vi)
    VALUES (@id, @word, @meaning_vi, @examples, @examples_vi)
    ON CONFLICT(id) DO UPDATE SET
      word = excluded.word,
      meaning_vi = excluded.meaning_vi,
      examples = excluded.examples,
      examples_vi = excluded.examples_vi
  `).run({
    id: 0,
    word: 'apple',
    meaning_vi: 'quả táo',
    examples: JSON.stringify([
      'he has an apple',
      'apple is a popular company',
      'an apple a day keep the doctor away',
    ]),
    examples_vi: JSON.stringify([
      'anh ấy có một quả táo',
      'Apple là một công ty nổi tiếng',
      'một quả táo mỗi ngày giúp không cần gặp bác sĩ',
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

  db.prepare(`
    INSERT INTO japanese_word
      (id, kanji, hiragana, katakana, romaji, meaning_en, examples, examples_en, examples_vi)
    VALUES
      (@id, @kanji, @hiragana, @katakana, @romaji, @meaning_en,
       @examples, @examples_en, @examples_vi)
    ON CONFLICT(id) DO UPDATE SET
      kanji = excluded.kanji,
      hiragana = excluded.hiragana,
      katakana = excluded.katakana,
      romaji = excluded.romaji,
      meaning_en = excluded.meaning_en,
      examples = excluded.examples,
      examples_en = excluded.examples_en,
      examples_vi = excluded.examples_vi
  `).run({
    id: 0,
    kanji: '桜',
    hiragana: 'さくら',
    katakana: 'サクラ',
    romaji: 'sakura',
    meaning_en: 'cherry blossom',
    examples: JSON.stringify(['桜が咲きました。', '春に桜を見ます。']),
    examples_en: JSON.stringify(['The cherry blossoms bloomed.', 'I see cherry blossoms in spring.']),
    examples_vi: JSON.stringify(['Hoa anh đào đã nở.', 'Tôi nhìn hoa anh đào vào mùa xuân.']),
  });
}

module.exports = { seed };
