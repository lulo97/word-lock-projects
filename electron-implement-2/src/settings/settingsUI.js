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
const themeGridEl = document.getElementById('themeGrid');
let selectedTheme = 'paper';

function renderThemeCards(themes) {
  themeGridEl.replaceChildren();

  for (const theme of themes) {
    const card = document.createElement('button');
    card.type = 'button';
    card.className = 'ui-theme-card';
    card.dataset.theme = theme.id;   // makes the card wear its own skin
    card.dataset.id = theme.id;
    card.title = theme.description;
    card.setAttribute('aria-pressed', String(theme.id === selectedTheme));

    const name = document.createElement('span');
    name.className = 'font-semibold';
    name.textContent = theme.name;

    const dots = document.createElement('span');
    dots.className = 'ui-theme-card__dots';
    dots.append(document.createElement('i'), document.createElement('i'), document.createElement('i'));

    card.append(name, dots);
    card.addEventListener('click', () => selectTheme(theme.id));
    themeGridEl.appendChild(card);
  }
}

function selectTheme(id) {
  selectedTheme = id;
  for (const card of themeGridEl.children) {
    card.setAttribute('aria-pressed', String(card.dataset.id === id));
  }
  window.electronAPI.previewTheme(id); // live preview in every window, not saved yet
}

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
    theme: selectedTheme,
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

  selectedTheme = settings.theme;
  renderThemeCards(await window.electronAPI.listThemes());
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
