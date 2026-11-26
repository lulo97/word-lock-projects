const englishWordEl = document.getElementById('englishWord');
const multipleChoiceEl = document.getElementById('multipleChoice');
const japaneseWordEl = document.getElementById('japaneseWord');
const dumbbellWorkoutEl = document.getElementById('dumbbellWorkout');
const chromeFreezeEl = document.getElementById('chromeFreeze');
const autoStartEl = document.getElementById('autoStart');
const masterVolumeEl = document.getElementById('masterVolume');
const volumeValueEl = document.getElementById('volumeValue');
const countdownSecondsEl = document.getElementById('countdownSeconds');
const openIgnoreWordsEl = document.getElementById('openIgnoreWords');
const cancelEl = document.getElementById('cancel');
const confirmEl = document.getElementById('confirm');

function readSettings() {
  return {
    englishWord: englishWordEl.checked,
    multipleChoice: multipleChoiceEl.checked,
    japaneseWord: japaneseWordEl.checked,
    dumbbellWorkout: dumbbellWorkoutEl.checked,
    chromeFreeze: chromeFreezeEl.checked,
    autoStart: autoStartEl.checked,
    masterVolume: Number(masterVolumeEl.value),
    countdownSeconds: Number(countdownSecondsEl.value),
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
  chromeFreezeEl.checked = settings.chromeFreeze;
  autoStartEl.checked = settings.autoStart;
  masterVolumeEl.value = settings.masterVolume;
  countdownSecondsEl.value = settings.countdownSeconds;
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
