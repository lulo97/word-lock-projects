async function loadDetails() {
  const id = Number(new URLSearchParams(window.location.search).get('id'));
  const row = await window.electronAPI.getWord(id);

  if (!row) {
    window.infoUI.setText('word', 'Word not found');
    return;
  }

  window.infoUI.setText('id', row.id);
  window.infoUI.setText('word', row.word);
  window.infoUI.setText('meaning_vi', row.meaning_vi);

  const rows = (row.examples ?? []).map((text, i) => ({
    text,
    subs: [row.examples_vi?.[i]],
  }));
  window.infoUI.renderExamples('examples', rows, 'en');

  window.speakUI.speak(row.word, 'en', document.getElementById('speak-word'));
}

window.speakUI.bindSpeakButton('speak-word', () => document.getElementById('word').textContent, 'en');

window.addEventListener('keydown', (event) => {
  if (event.key === 'Enter') window.electronAPI.exitInfo();
});

loadDetails();
