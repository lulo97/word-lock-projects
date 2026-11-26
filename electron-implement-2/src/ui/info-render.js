// src/ui/info-render.js
// rows = [{ text: 'sentence', subs: ['translation 1', 'translation 2'] }]
window.infoUI = {
  setText(id, value) {
    document.getElementById(id).textContent = String(value);
  },

  renderExamples(containerId, rows, lang) {
    const container = document.getElementById(containerId);
    container.replaceChildren();

    for (const row of rows) {
      const card = document.createElement('div');
      card.className = 'ui-example';

      const body = document.createElement('div');
      const main = document.createElement('div');
      main.className = 'ui-example__text';
      main.textContent = row.text;
      body.appendChild(main);

      for (const sub of row.subs ?? []) {
        if (!sub) continue;
        const line = document.createElement('div');
        line.className = 'ui-example__sub';
        line.textContent = sub;
        body.appendChild(line);
      }

      card.appendChild(body);
      card.appendChild(window.speakUI.createSpeakButton(row.text, lang));
      container.appendChild(card);
    }
  },
};
