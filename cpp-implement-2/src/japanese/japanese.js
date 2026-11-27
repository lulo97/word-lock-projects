// Random non-ignored Japanese word; romaji input comparison with the same
// solved/navigation behaviour as the English screen.
(function () {
  'use strict';

  var wordEl = document.getElementById('word');
  var hiraganaEl = document.getElementById('hiragana');
  var katakanaEl = document.getElementById('katakana');
  var romajiEl = document.getElementById('romaji');
  var answerEl = document.getElementById('answer');
  var feedbackEl = document.getElementById('feedback');
  var currentWord = null;
  var solved = false;

  async function loadWord() {
    try {
      currentWord = await window.app.call('japanese:getRandom');
    } catch (error) {
      currentWord = null;
    }

    if (!currentWord) {
      wordEl.textContent = 'No words available';
      return;
    }

    wordEl.textContent = currentWord.kanji;
    wordEl.dataset.id = String(currentWord.id);
    hiraganaEl.textContent = currentWord.hiragana;
    katakanaEl.textContent = currentWord.katakana;
    romajiEl.textContent = currentWord.romaji;
    answerEl.focus();
  }

  answerEl.addEventListener('input', async function () {
    if (!currentWord || solved) return;

    if (answerEl.value.trim().toLowerCase() === String(currentWord.romaji).toLowerCase()) {
      solved = true;
      answerEl.disabled = true;
      try {
        await window.app.call('japanese:correct', { value: String(currentWord.id) });
      } catch (error) {
        // Keep the UI responsive if the host reply is lost.
      }
      feedbackEl.textContent = 'Correct';
      feedbackEl.dataset.state = 'ok';
      setTimeout(function () {
        window.location.href = 'japaneseInfo.html?id=' + encodeURIComponent(currentWord.id);
      }, 300);
    } else {
      feedbackEl.textContent = '';
      delete feedbackEl.dataset.state;
    }
  });

  loadWord();
})();
