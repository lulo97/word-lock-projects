// Load the selected word, render examples, auto-speak the word, and exit on Enter.
(function () {
  'use strict';

  async function loadDetails() {
    var id = Number(new URLSearchParams(window.location.search).get('id'));
    var row = null;
    try {
      row = await window.app.call('word:get', { value: String(id) });
    } catch (error) {
      row = null;
    }

    if (!row) {
      window.infoUI.setText('word', 'Word not found');
      return;
    }

    window.infoUI.setText('id', row.id);
    window.infoUI.setText('word', row.word);
    window.infoUI.setText('meaning_vi', row.meaning_vi);

    var examples = row.examples || [];
    var translations = row.examples_vi || [];
    var rows = examples.map(function (text, index) {
      return { text: text, subs: [translations[index]] };
    });
    window.infoUI.renderExamples('examples', rows, 'en');

    window.speakUI.speak(row.word, 'en', document.getElementById('speak-word'),
      { userInitiated: false });
  }

  window.speakUI.bindSpeakButton('speak-word', function () {
    return document.getElementById('word').textContent;
  }, 'en');

  window.addEventListener('keydown', function (event) {
    if (event.key === 'Enter') {
      event.preventDefault();
      window.app.send('app:exitInfo');
    }
  });

  loadDetails();
})();
