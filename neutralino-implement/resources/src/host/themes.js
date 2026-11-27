const DEFAULT_THEME = 'paper';
let themes = [];

export async function initThemes() {
  const response = await fetch('/src/themes/registry.json');
  themes = await response.json();
  return themes;
}

export function getThemes() {
  return themes;
}

export function getThemeIds() {
  return themes.map((theme) => theme.id);
}

export function nextThemeId(current) {
  const ids = getThemeIds();
  if (ids.length === 0) return DEFAULT_THEME;
  const safe = ids.includes(current) ? current : DEFAULT_THEME;
  return ids[(ids.indexOf(safe) + 1) % ids.length];
}

// The old broadcast.js walked BrowserWindow.getAllWindows(); Neutralino's
// app.broadcast reaches every WebView (host, children and the countdown iframe).
export function broadcastTheme(id) {
  return Neutralino.app.broadcast('theme:changed', id);
}
