// Shared info-screen rendering. rows = [{ text, subs: [translation, ...] }].
(function () {
  'use strict';

  function setText(id, value) {
    var element = document.getElementById(id);
    if (element) element.textContent = String(value);
  }

  function renderExamples(containerId, rows, lang) {
    var container = document.getElementById(containerId);
    if (!container) return;
    container.replaceChildren();

    (rows || []).forEach(function (row) {
      var card = document.createElement('div');
      card.className = 'ui-example';

      var body = document.createElement('div');
      var main = document.createElement('div');
      main.className = 'ui-example__text';
      main.textContent = row.text;
      body.appendChild(main);

      (row.subs || []).forEach(function (sub) {
        if (!sub) return;
        var line = document.createElement('div');
        line.className = 'ui-example__sub';
        line.textContent = sub;
        body.appendChild(line);
      });

      card.appendChild(body);
      card.appendChild(window.speakUI.createSpeakButton(row.text, lang));
      container.appendChild(card);
    });
  }

  window.infoUI = { setText: setText, renderExamples: renderExamples };
})();
