// Ignore Words editor: language tabs preserve per-language drafts, validation
// happens in the host, and duplicates are retained by the host save path.
(function () {
  'use strict';

  var tabEnglishEl = document.getElementById('tabEnglish');
  var tabJapaneseEl = document.getElementById('tabJapanese');
  var hintEl = document.getElementById('hint');
  var wordsEl = document.getElementById('words');
  var errorEl = document.getElementById('error');
  var cancelEl = document.getElementById('cancel');
  var saveEl = document.getElementById('save');

  var hints = {
    english: 'Comma-separated words only, e.g. abc, xyz, 123',
    japanese: 'Comma-separated kanji, hiragana or katakana only, e.g. こんにちは, コンニチハ, 今日は'
  };

  var activeLanguage = 'english';
  var drafts = { english: '', japanese: '' };

  function hideError() {
    errorEl.hidden = true;
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

  tabEnglishEl.addEventListener('click', function () { switchLanguage('english'); });
  tabJapaneseEl.addEventListener('click', function () { switchLanguage('japanese'); });
  wordsEl.addEventListener('input', hideError);

  cancelEl.addEventListener('click', function () {
    window.app.send('ignoreWords:close');
  });

  saveEl.addEventListener('click', async function () {
    drafts[activeLanguage] = wordsEl.value;
    var result = null;
    try {
      result = await window.app.call('ignoreWords:save', {
        english: drafts.english,
        japanese: drafts.japanese
      });
    } catch (error) {
      result = null;
    }

    if (!result || !result.ok) {
      errorEl.hidden = false;
      return;
    }

    drafts = {
      english: result.words.english.join(', '),
      japanese: result.words.japanese.join(', ')
    };
    render();
    window.app.send('ignoreWords:close');
  });

  async function loadIgnoreWords() {
    var words = null;
    try {
      words = await window.app.call('ignoreWords:get');
    } catch (error) {
      words = null;
    }
    if (!words) return;
    drafts = {
      english: (words.english || []).join(', '),
      japanese: (words.japanese || []).join(', ')
    };
    render();
  }

  loadIgnoreWords();
})();
