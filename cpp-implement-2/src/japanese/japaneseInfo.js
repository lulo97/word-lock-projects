// Press Enter to leave the Japanese word info screen.
document.addEventListener('keydown', function (event) {
  if (event.key === 'Enter') {
    event.preventDefault();
    window.location.href = 'japanese.html';
  }
});
