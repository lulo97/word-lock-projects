// Multiple choice result screen: reflect the chosen option and exit on Enter.
(function () {
  const params = new URLSearchParams(window.location.search);
  const isCorrect = params.get('result') === 'ok';
  const choice = params.get('choice') || '';

  const labels = {
    A: 'A. A lucky discovery',
    B: 'B. A planned routine',
    C: 'C. A sudden mistake',
    D: 'D. A quiet conversation'
  };

  const result = document.getElementById('result');
  result.textContent = isCorrect ? 'Correct!' : 'Not quite';
  result.dataset.state = isCorrect ? 'ok' : 'bad';

  document.getElementById('your-answer').textContent = labels[choice] || '—';
  document.getElementById('correct-answer').textContent = labels.A;
  document.getElementById('explanation').textContent = isCorrect
    ? 'Exactly — serendipity means finding something good by chance.'
    : 'Serendipity means finding something good by chance, usually without planning it.';

  document.addEventListener('keydown', function (event) {
    if (event.key === 'Enter') {
      event.preventDefault();
      window.location.href = 'mcq.html';
    }
  });
})();
