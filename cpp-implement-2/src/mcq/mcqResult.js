// Evaluate the selected answer against the stored question and exit on Enter.
(function () {
  'use strict';

  var resultEl = document.getElementById('result');
  var detailsEl = document.getElementById('details');

  function addField(label, value) {
    var dt = document.createElement('dt');
    dt.textContent = label;
    var dd = document.createElement('dd');
    dd.textContent = String(value == null ? '' : value);
    detailsEl.append(dt, dd);
  }

  async function loadResult() {
    var params = new URLSearchParams(window.location.search);
    var id = Number(params.get('id'));
    var selected = Number(params.get('selected'));
    var mcq = null;
    try {
      mcq = await window.app.call('mcq:get', { value: String(id) });
    } catch (error) {
      mcq = null;
    }

    if (!mcq) {
      resultEl.textContent = 'Question not found';
      return;
    }

    var correct = selected === mcq.correct_answer_idx;
    resultEl.textContent = correct ? 'Correct' : 'Wrong';
    resultEl.dataset.state = correct ? 'ok' : 'bad';

    addField('question', mcq.question);
    addField('your answer', mcq.answers[selected]);
    addField('correct answer', mcq.answers[mcq.correct_answer_idx]);
    addField('explanation', mcq.explanation);
  }

  window.addEventListener('keydown', function (event) {
    if (event.key === 'Enter') {
      event.preventDefault();
      window.app.send('app:exitInfo');
    }
  });

  loadResult();
})();
