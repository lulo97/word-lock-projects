const englishWordEl = document.getElementById('englishWord');
const multipleChoiceEl = document.getElementById('multipleChoice');
const japaneseWordEl = document.getElementById('japaneseWord');
const cancelEl = document.getElementById('cancel');
const confirmEl = document.getElementById('confirm');

function readSettings() {
  return {
    englishWord: englishWordEl.checked,
    multipleChoice: multipleChoiceEl.checked,
    japaneseWord: japaneseWordEl.checked,
  };
}

async function loadSettings() {
  const settings = await window.electronAPI.getSettings();

  englishWordEl.checked = settings.englishWord;
  multipleChoiceEl.checked = settings.multipleChoice;
  japaneseWordEl.checked = settings.japaneseWord;
}

cancelEl.addEventListener('click', () => {
  window.electronAPI.closeSettings();
});

confirmEl.addEventListener('click', () => {
  window.electronAPI.confirmSettings(readSettings());
});

loadSettings();
