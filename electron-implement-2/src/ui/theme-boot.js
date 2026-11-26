// src/ui/theme-boot.js
(function () {
  const root = document.documentElement;
  const api = window.electronAPI;

  root.dataset.theme = (api && api.themeId) || 'paper';

  // Live swap: main process tells every window when the theme changes.
  if (api && api.onThemeChanged) {
    api.onThemeChanged((id) => {
      root.dataset.theme = id;
    });
  }
})();
