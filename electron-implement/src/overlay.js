/** Bottom-right countdown card, rewritten with jQuery. */

const $ = window.jQuery;
let styleKey = null;

function formatRemaining(seconds) {
  let value = Math.max(0, Math.ceil(Number(seconds) || 0));
  const hours = Math.floor(value / 3600);
  value -= hours * 3600;
  const minutes = Math.floor(value / 60);
  const secs = value - minutes * 60;

  if (hours) {
    return `${hours}:${String(minutes).padStart(2, "0")}:${String(secs).padStart(2, "0")}`;
  }
  if (minutes) {
    return `${String(minutes).padStart(2, "0")}:${String(secs).padStart(2, "0")}`;
  }
  return `${secs}s`;
}

function render(payload) {
  if (!payload) return;

  if (payload.style && payload.style.key !== styleKey) {
    styleKey = payload.style.key;
    window.WordLockTheme.applyStyle(payload.style);
  }

  $("#overlayTime").text(formatRemaining(payload.seconds));
}

window.wordlock.onOverlaySet(render);
