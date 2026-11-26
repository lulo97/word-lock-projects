const countdownEl = document.getElementById('countdown');

let seconds = 60;

const intervalId = setInterval(() => {
  seconds -= 1;
  countdownEl.textContent = String(seconds);

  if (seconds === 0) {
    clearInterval(intervalId);
  }
}, 1000);
