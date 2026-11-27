// Press Enter to leave the English word info screen.
document.addEventListener('keydown', function (event) {
  if (event.key === 'Enter') {
    event.preventDefault();
    window.location.href = 'english.html';
  }
});
