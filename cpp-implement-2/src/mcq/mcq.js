// Multiple choice screen: open the result screen with the chosen option.
document.querySelectorAll('[data-choice]').forEach(function (button) {
  button.addEventListener('click', function () {
    window.location.href =
      'mcqResult.html?result=' + button.dataset.result + '&choice=' + button.dataset.choice;
  });
});
