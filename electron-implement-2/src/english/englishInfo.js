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
  const row = await window.electronAPI.getWord(id);

  if (!row) {
    setText('word', 'Word not found');
    return;
  }

  setText('id', row.id);
  setText('word', row.word);
  setText('meaning_vi', row.meaning_vi);
  renderList('examples', row.examples, 'en');
  renderList('examples_vi', row.examples_vi);
  window.speakUI.speak(row.word, 'en', document.getElementById('speak-word'));
}

window.speakUI.bindSpeakButton('speak-word', () => document.getElementById('word').textContent, 'en');

window.addEventListener('keydown', (event) => {
  if (event.key === 'Enter') {
    window.electronAPI.exitInfo();
  }
});

loadDetails();
