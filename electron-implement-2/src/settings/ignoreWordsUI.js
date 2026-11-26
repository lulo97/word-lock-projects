const tabEnglishEl = document.getElementById('tabEnglish');
const tabJapaneseEl = document.getElementById('tabJapanese');
const hintEl = document.getElementById('hint');
const wordsEl = document.getElementById('words');
const errorEl = document.getElementById('error');
const cancelEl = document.getElementById('cancel');
const saveEl = document.getElementById('save');

const hints = {
  english: 'Comma-separated words only, e.g. abc, xyz, 123',
  japanese:
    'Comma-separated kanji, hiragana or katakana only, e.g. こんにちは, コンニチハ, 今日は',
};

let activeLanguage = 'english';
let drafts = { english: '', japanese: '' };

function hideError() {
  errorEl.classList.add('hidden');
}

function render() {
  tabEnglishEl.setAttribute('aria-selected', String(activeLanguage === 'english'));
  tabJapaneseEl.setAttribute('aria-selected', String(activeLanguage === 'japanese'));
  hintEl.textContent = hints[activeLanguage];
  wordsEl.value = drafts[activeLanguage];
  hideError();
}

function switchLanguage(language) {
  drafts[activeLanguage] = wordsEl.value;
  activeLanguage = language;
  render();
}

tabEnglishEl.addEventListener('click', () => switchLanguage('english'));
tabJapaneseEl.addEventListener('click', () => switchLanguage('japanese'));

wordsEl.addEventListener('input', hideError);

cancelEl.addEventListener('click', () => {
  window.electronAPI.closeIgnoreWords();
});

saveEl.addEventListener('click', async () => {
  drafts[activeLanguage] = wordsEl.value;

  const result = await window.electronAPI.saveIgnoreWords(drafts);
  if (!result.ok) {
    errorEl.classList.remove('hidden');
    return;
  }

  drafts = {
    english: result.words.english.join(', '),
    japanese: result.words.japanese.join(', '),
  };
  render();
  window.electronAPI.closeIgnoreWords();
});

async function loadIgnoreWords() {
  const words = await window.electronAPI.getIgnoreWords();
  drafts = { english: words.english.join(', '), japanese: words.japanese.join(', ') };
  render();
}

loadIgnoreWords();
