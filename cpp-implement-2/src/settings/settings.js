// Settings screen: load persisted values from the host, preview themes live,
// and confirm or cancel through host messages.
(function () {
  'use strict';

  var checkboxIds = [
    'englishWord', 'multipleChoice', 'japaneseWord', 'dumbbellWorkout',
    'chromeFreeze', 'autoStart', 'autoTheme'
  ];
  var checkboxes = {};
  checkboxIds.forEach(function (id) { checkboxes[id] = document.getElementById(id); });

  var masterVolumeEl = document.getElementById('masterVolume');
  var volumeValueEl = document.getElementById('volumeValue');
  var countdownSecondsEl = document.getElementById('countdownSeconds');
  var themeGridEl = document.getElementById('themeGrid');
  var selectedTheme = 'paper';

  function renderThemeCards(themes) {
    themeGridEl.replaceChildren();
    (themes || []).forEach(function (theme) {
      var card = document.createElement('button');
      card.type = 'button';
      card.className = 'theme-card';
      card.dataset.theme = theme.id;
      card.dataset.id = theme.id;
      card.title = theme.description || '';
      card.setAttribute('aria-pressed', String(theme.id === selectedTheme));
      card.textContent = theme.name;
      card.addEventListener('click', function () { selectTheme(theme.id); });
      themeGridEl.appendChild(card);
    });
  }

  function selectTheme(id) {
    selectedTheme = id;
    Array.prototype.forEach.call(themeGridEl.children, function (card) {
      card.setAttribute('aria-pressed', String(card.dataset.id === id));
    });
    window.app.send('theme:preview', { value: id });
  }

  function renderVolume() {
    volumeValueEl.textContent = Number(masterVolumeEl.value) + '×';
  }

  function readSettings() {
    var values = {};
    checkboxIds.forEach(function (id) { values[id] = !!checkboxes[id].checked; });
    values.masterVolume = Number(masterVolumeEl.value);
    values.countdownSeconds = Number(countdownSecondsEl.value);
    values.theme = selectedTheme;
    return values;
  }

  async function loadSettings() {
    var settings = null;
    var themes = [];
    try {
      settings = await window.app.call('settings:get');
      themes = await window.app.call('theme:list');
    } catch (error) {
      settings = null;
    }

    if (!settings) return;

    checkboxIds.forEach(function (id) { checkboxes[id].checked = !!settings[id]; });
    masterVolumeEl.value = settings.masterVolume;
    countdownSecondsEl.value = settings.countdownSeconds;
    selectedTheme = settings.theme || 'paper';
    renderVolume();
    renderThemeCards(themes);
    document.body.dataset.settingsLoaded = 'true';
  }

  masterVolumeEl.addEventListener('input', renderVolume);

  document.getElementById('openIgnoreWords').addEventListener('click', function () {
    window.app.call('ignoreWords:open');
  });

  document.getElementById('cancel').addEventListener('click', function () {
    window.app.send('settings:close');
  });

  document.getElementById('confirm').addEventListener('click', function () {
    window.app.call('settings:confirm', readSettings());
  });

  loadSettings();
})();
