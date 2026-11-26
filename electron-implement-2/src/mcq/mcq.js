const questionEl = document.getElementById('question');
const optionsEl = document.getElementById('options');

let currentMcq = null;
let answered = false;

async function loadMcq() {
  currentMcq = await window.electronAPI.getRandomMcq();

  if (!currentMcq) {
    questionEl.textContent = 'No questions available';
    return;
  }

  questionEl.textContent = currentMcq.question;

  currentMcq.answers.forEach((answer, index) => {
    const button = document.createElement('button');
    button.type = 'button';
    button.className = 'rounded border border-black px-6 py-2 text-lg hover:bg-gray-100';
    button.textContent = answer;
    button.addEventListener('click', () => choose(index));
    optionsEl.appendChild(button);
  });
}

function choose(index) {
  if (answered || !currentMcq) return;

  answered = true;

  setTimeout(() => {
    window.location.href = `mcqResult.html?id=${encodeURIComponent(currentMcq.id)}&selected=${index}`;
  }, 300);
}

loadMcq();
