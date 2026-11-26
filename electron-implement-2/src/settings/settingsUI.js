const englishWordEl = document.getElementById('englishWord');
const multipleChoiceEl = document.getElementById('multipleChoice');
const japaneseWordEl = document.getElementById('japaneseWord');
const dumbbellWorkoutEl = document.getElementById('dumbbellWorkout');
const masterVolumeEl = document.getElementById('masterVolume');
const volumeValueEl = document.getElementById('volumeValue');
const openIgnoreWordsEl = document.getElementById('openIgnoreWords');
const cancelEl = document.getElementById('cancel');
const confirmEl = document.getElementById('confirm');

function readSettings() {
  return {
    englishWord: englishWordEl.checked,
    multipleChoice: multipleChoiceEl.checked,
    japaneseWord: japaneseWordEl.checked,
    dumbbellWorkout: dumbbellWorkoutEl.checked,
    masterVolume: Number(masterVolumeEl.value),
  };
}

function renderVolume() {
  volumeValueEl.textContent = `${Number(masterVolumeEl.value)}×`;
}

masterVolumeEl.addEventListener('input', renderVolume);

async function loadSettings() {
  const settings = await window.electronAPI.getSettings();

  englishWordEl.checked = settings.englishWord;
  multipleChoiceEl.checked = settings.multipleChoice;
  japaneseWordEl.checked = settings.japaneseWord;
  dumbbellWorkoutEl.checked = settings.dumbbellWorkout;
  masterVolumeEl.value = settings.masterVolume;
  renderVolume();
}

openIgnoreWordsEl.addEventListener('click', () => {
  window.electronAPI.openIgnoreWords();
});

cancelEl.addEventListener('click', () => {
  window.electronAPI.closeSettings();
});

confirmEl.addEventListener('click', () => {
  window.electronAPI.confirmSettings(readSettings());
});

loadSettings();
