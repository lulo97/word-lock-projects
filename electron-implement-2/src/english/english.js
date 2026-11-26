const wordEl = document.getElementById('word');
const answerEl = document.getElementById('answer');
const feedbackEl = document.getElementById('feedback');

let currentWord = null;
let solved = false;

async function loadWord() {
  currentWord = await window.electronAPI.getRandomWord();

  if (!currentWord) {
    wordEl.textContent = 'No words available';
    return;
  }

  wordEl.textContent = currentWord.word;
  answerEl.focus();
}

answerEl.addEventListener('input', () => {
  if (!currentWord || solved) return;

  if (answerEl.value.trim().toLowerCase() === currentWord.word.toLowerCase()) {
    solved = true;
    answerEl.disabled = true;
    feedbackEl.textContent = 'Correct';
    feedbackEl.dataset.state = 'ok';

    setTimeout(() => {
      window.location.href = `englishInfo.html?id=${encodeURIComponent(currentWord.id)}`;
    }, 300);
  } else {
    feedbackEl.textContent = '';
    delete feedbackEl.dataset.state;
  }
});

loadWord();
