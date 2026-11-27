const resultEl = document.getElementById('result');
const detailsEl = document.getElementById('details');

function addField(label, value) {
  const dt = document.createElement('dt');
  dt.textContent = label;
  const dd = document.createElement('dd');
  dd.textContent = String(value);
  detailsEl.append(dt, dd);
}

async function loadResult() {
  const params = new URLSearchParams(window.location.search);
  const id = Number(params.get('id'));
  const selected = Number(params.get('selected'));
  const mcq = await window.electronAPI.getMcq(id);

  if (!mcq) {
    resultEl.textContent = 'Question not found';
    return;
  }

  const correct = selected === mcq.correct_answer_idx;

  resultEl.textContent = correct ? 'Correct' : 'Wrong';
  resultEl.dataset.state = correct ? 'ok' : 'bad';

  addField('question', mcq.question);
  addField('your answer', mcq.answers[selected]);
  addField('correct answer', mcq.answers[mcq.correct_answer_idx]);
  addField('explanation', mcq.explanation);
}

window.addEventListener('keydown', (event) => {
  if (event.key === 'Enter') {
    window.electronAPI.exitInfo();
  }
});

loadResult();
