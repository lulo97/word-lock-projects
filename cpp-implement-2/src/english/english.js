// Random non-ignored English word, case-insensitive input matching, and a
// 300 ms navigation to the info screen after a correct answer.
(function () {
  'use strict';

  var wordEl = document.getElementById('word');
  var answerEl = document.getElementById('answer');
  var feedbackEl = document.getElementById('feedback');
  var currentWord = null;
  var solved = false;

  async function loadWord() {
    try {
      currentWord = await window.app.call('word:getRandom');
    } catch (error) {
      currentWord = null;
    }

    if (!currentWord) {
      wordEl.textContent = 'No words available';
      return;
    }

    wordEl.textContent = currentWord.word;
    answerEl.focus();
  }

  answerEl.addEventListener('input', function () {
    if (!currentWord || solved) return;

    if (answerEl.value.trim().toLowerCase() === String(currentWord.word).toLowerCase()) {
      solved = true;
      answerEl.disabled = true;
      feedbackEl.textContent = 'Correct';
      feedbackEl.dataset.state = 'ok';
      setTimeout(function () {
        window.location.href = 'englishInfo.html?id=' + encodeURIComponent(currentWord.id);
      }, 300);
    } else {
      feedbackEl.textContent = '';
      delete feedbackEl.dataset.state;
    }
  });

  loadWord();
})();
