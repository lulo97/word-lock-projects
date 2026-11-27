// English word screen: accept "serendipity" and open the info screen.
(function () {
  const input = document.getElementById('answer');
  const feedback = document.getElementById('feedback');
  const expected = 'serendipity';

  function normalize(value) {
    return value.trim().normalize('NFKC').toLocaleLowerCase();
  }

  function checkAnswer() {
    if (normalize(input.value) === expected) {
      feedback.dataset.state = 'ok';
      feedback.textContent = 'Correct! Opening word details…';
      window.location.href = 'englishInfo.html';
      return;
    }

    feedback.dataset.state = 'bad';
    feedback.textContent = 'Not quite — try again.';
    input.focus();
    input.select();
  }

  input.addEventListener('keydown', function (event) {
    if (event.key === 'Enter') {
      event.preventDefault();
      checkAnswer();
    }
  });

  // Move on as soon as the correct word has been typed.
  input.addEventListener('input', function () {
    if (normalize(input.value) === expected) {
      checkAnswer();
    }
  });

  input.focus();
})();
