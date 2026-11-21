/**
 * Applies the CSS custom properties that main.js computes for the current
 * style. Nothing is hard-coded here: the whole mapping lives in
 * lib/ui-styles.js and travels with the style payload.
 */
(function () {
  function applyStyle(style) {
    if (!style || !style.css) return;
    const root = document.documentElement;
    for (const [name, value] of Object.entries(style.css)) {
      root.style.setProperty(name, value);
    }
    if (document.body) document.body.dataset.style = style.key;
  }

  window.WordLockTheme = { applyStyle };
})();
