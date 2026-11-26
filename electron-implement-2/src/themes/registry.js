const themes = require('./registry.json');

const DEFAULT_THEME = 'paper';
const THEME_IDS = themes.map((theme) => theme.id);

function nextThemeId(current) {
  const safe = THEME_IDS.includes(current) ? current : DEFAULT_THEME;
  return THEME_IDS[(THEME_IDS.indexOf(safe) + 1) % THEME_IDS.length];
}

module.exports = { themes, THEME_IDS, DEFAULT_THEME, nextThemeId };
