// Load a random question and navigate to its result screen after one selection.
(function () {
  'use strict';

  var questionEl = document.getElementById('question');
  var optionsEl = document.getElementById('options');
  var currentMcq = null;
  var answered = false;

  async function loadMcq() {
    try {
      currentMcq = await window.app.call('mcq:getRandom');
    } catch (error) {
      currentMcq = null;
    }

    if (!currentMcq) {
      questionEl.textContent = 'No questions available';
      return;
    }

    questionEl.textContent = currentMcq.question;
    (currentMcq.answers || []).forEach(function (answer, index) {
      var button = document.createElement('button');
      button.type = 'button';
      button.className = 'ui-btn ui-btn--choice';
      button.textContent = answer;
      button.addEventListener('click', function () { choose(index); });
      optionsEl.appendChild(button);
    });
  }

  function choose(index) {
    if (answered || !currentMcq) return;
    answered = true;
    setTimeout(function () {
      window.location.href = 'mcqResult.html?id=' + encodeURIComponent(currentMcq.id) +
        '&selected=' + index;
    }, 300);
  }

  loadMcq();
})();
