const englishWordEl = document.getElementById('englishWord');
const multipleChoiceEl = document.getElementById('multipleChoice');

function saveSettings() {
  window.electronAPI.setSettings({
    englishWord: englishWordEl.checked,
    multipleChoice: multipleChoiceEl.checked,
  });
}

async function loadSettings() {
  const settings = await window.electronAPI.getSettings();

  englishWordEl.checked = settings.englishWord;
  multipleChoiceEl.checked = settings.multipleChoice;
}

englishWordEl.addEventListener('change', saveSettings);
multipleChoiceEl.addEventListener('change', saveSettings);

loadSettings();
