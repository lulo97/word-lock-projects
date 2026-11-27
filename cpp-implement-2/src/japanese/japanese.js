// Japanese word screen: accept any listed reading and open the info screen.
(function () {
  const input = document.getElementById('answer');
  const feedback = document.getElementById('feedback');

  function normalize(value) {
    return value.trim().normalize('NFKC').toLocaleLowerCase();
  }

  const accepted = ['こんにちは', 'コンニチハ', 'konnichiwa', '今日は'].map(normalize);

  function checkAnswer() {
    if (accepted.includes(normalize(input.value))) {
      feedback.dataset.state = 'ok';
      feedback.textContent = 'Correct! Opening word details…';
      window.location.href = 'japaneseInfo.html';
      return;
    }

    feedback.dataset.state = 'bad';
    feedback.textContent = 'Not quite — try the hiragana, katakana, or romaji reading.';
    input.focus();
    input.select();
  }

  input.addEventListener('keydown', function (event) {
    if (event.key === 'Enter') {
      event.preventDefault();
      checkAnswer();
    }
  });

  // Move on as soon as one of the accepted readings has been typed.
  input.addEventListener('input', function () {
    if (accepted.includes(normalize(input.value))) {
      checkAnswer();
    }
  });

  input.focus();
})();
