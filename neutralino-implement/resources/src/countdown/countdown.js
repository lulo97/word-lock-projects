const countdownEl = document.getElementById('countdown');

const DEFAULT_SECONDS = 60;

async function getCountdownSeconds() {
  try {
    const settings = await window.electronAPI.getSettings();
    const seconds = Number(settings.countdownSeconds);
    return Number.isFinite(seconds) && seconds > 0 ? Math.max(1, Math.round(seconds)) : DEFAULT_SECONDS;
  } catch {
    return DEFAULT_SECONDS;
  }
}

async function startCountdown() {
  let seconds = await getCountdownSeconds();

  countdownEl.textContent = String(seconds);

  const intervalId = setInterval(() => {
    seconds -= 1;
    countdownEl.textContent = String(seconds);

    if (seconds <= 0) {
      clearInterval(intervalId);
      window.electronAPI.countdownFinished();
    }
  }, 1000);
}

startCountdown();
