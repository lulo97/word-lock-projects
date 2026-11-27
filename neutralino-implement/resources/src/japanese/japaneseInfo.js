async function loadDetails() {
  const id = Number(new URLSearchParams(window.location.search).get('id'));
  const row = await window.electronAPI.getJapaneseWord(id);

  if (!row) {
    window.infoUI.setText('kanji', 'Word not found');
    return;
  }

  for (const key of ['id', 'kanji', 'hiragana', 'katakana', 'romaji', 'meaning_en']) {
    window.infoUI.setText(key, row[key]);
  }

  const rows = (row.examples ?? []).map((text, i) => ({
    text,
    subs: [row.examples_en?.[i], row.examples_vi?.[i]],
  }));
  window.infoUI.renderExamples('examples', rows, 'ja');

  window.speakUI.speak(row.kanji, 'ja', document.getElementById('speak-kanji'));
}

window.speakUI.bindSpeakButton('speak-kanji', () => document.getElementById('kanji').textContent, 'ja');
window.speakUI.bindSpeakButton('speak-hiragana', () => document.getElementById('hiragana').textContent, 'ja');
window.speakUI.bindSpeakButton('speak-katakana', () => document.getElementById('katakana').textContent, 'ja');

window.addEventListener('keydown', (event) => {
  if (event.key === 'Enter') {
    window.electronAPI.exitInfo();
  }
});

loadDetails();
