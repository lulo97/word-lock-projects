const themes = require('./registry.json');

const DEFAULT_THEME = 'paper';
const THEME_IDS = themes.map((theme) => theme.id);

module.exports = { themes, THEME_IDS, DEFAULT_THEME };
