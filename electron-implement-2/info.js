const detailsEl = document.getElementById('details');

function addField(label, value) {
  const row = document.createElement('div');

  const labelEl = document.createElement('span');
  labelEl.className = 'font-semibold';
  labelEl.textContent = `${label}: `;

  row.append(labelEl, document.createTextNode(String(value)));
  detailsEl.appendChild(row);
}

function addExamples(examples) {
  const label = document.createElement('div');
  label.className = 'font-semibold';
  label.textContent = 'examples:';
  detailsEl.appendChild(label);

  const list = document.createElement('ol');
  list.className = 'list-inside list-decimal';

  for (const example of examples) {
    const item = document.createElement('li');
    item.textContent = example;
    list.appendChild(item);
  }

  detailsEl.appendChild(list);
}

async function loadDetails() {
  const id = Number(new URLSearchParams(window.location.search).get('id'));
  const row = await window.electronAPI.getWord(id);

  if (!row) {
    detailsEl.textContent = 'Word not found';
    return;
  }

  addField('id', row.id);
  addField('word', row.word);
  addField('meaning_vi', row.meaning_vi);
  addExamples(row.examples);
}

window.addEventListener('keydown', (event) => {
  if (event.key === 'Enter') {
    window.electronAPI.exitInfo();
  }
});

loadDetails();
