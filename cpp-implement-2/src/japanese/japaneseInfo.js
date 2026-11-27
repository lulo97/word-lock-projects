// Render readings/translations, auto-speak kanji, bind the three reading Speak
// buttons, and exit on Enter.
(function () {
  'use strict';

  async function loadDetails() {
    var id = Number(new URLSearchParams(window.location.search).get('id'));
    var row = null;
    try {
      row = await window.app.call('japanese:get', { value: String(id) });
    } catch (error) {
      row = null;
    }

    if (!row) {
      window.infoUI.setText('kanji', 'Word not found');
      return;
    }

    ['id', 'kanji', 'hiragana', 'katakana', 'romaji', 'meaning_en'].forEach(function (key) {
      window.infoUI.setText(key, row[key]);
    });

    var examples = row.examples || [];
    var english = row.examples_en || [];
    var vietnamese = row.examples_vi || [];
    var rows = examples.map(function (text, index) {
      return { text: text, subs: [english[index], vietnamese[index]] };
    });
    window.infoUI.renderExamples('examples', rows, 'ja');

    window.speakUI.speak(row.kanji, 'ja', document.getElementById('speak-kanji'),
      { userInitiated: false });
  }

  window.speakUI.bindSpeakButton('speak-kanji', function () {
    return document.getElementById('kanji').textContent;
  }, 'ja');
  window.speakUI.bindSpeakButton('speak-hiragana', function () {
    return document.getElementById('hiragana').textContent;
  }, 'ja');
  window.speakUI.bindSpeakButton('speak-katakana', function () {
    return document.getElementById('katakana').textContent;
  }, 'ja');

  window.addEventListener('keydown', function (event) {
    if (event.key === 'Enter') {
      event.preventDefault();
      window.app.send('app:exitInfo');
    }
  });

  loadDetails();
})();
