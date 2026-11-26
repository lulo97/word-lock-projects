function setText(id, value) {
  document.getElementById(id).textContent = String(value);
}

function renderList(id, values, lang) {
  const list = document.getElementById(id);
  list.replaceChildren();

  for (const value of values ?? []) {
    const item = document.createElement('li');
    item.className = 'flex items-center gap-2';

    const text = document.createElement('span');
    text.textContent = value;
    item.appendChild(text);

    if (lang) {
      item.appendChild(window.speakUI.createSpeakButton(value, lang));
    }

    list.appendChild(item);
  }
}

async function loadDetails() {
  const id = Number(new URLSearchParams(window.location.search).get('id'));
  const row = await window.electronAPI.getJapaneseWord(id);

  if (!row) {
    setText('kanji', 'Word not found');
    return;
  }

  setText('id', row.id);
  setText('kanji', row.kanji);
  setText('hiragana', row.hiragana);
  setText('katakana', row.katakana);
  setText('romaji', row.romaji);
  setText('meaning_en', row.meaning_en);
  renderList('examples', row.examples, 'ja');
  renderList('examples_en', row.examples_en);
  renderList('examples_vi', row.examples_vi);
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
