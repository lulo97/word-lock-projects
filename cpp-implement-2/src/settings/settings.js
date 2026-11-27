// Settings screen logic.
// - Tracks a draft of the four task checkboxes.
// - Confirm sends the draft to the native host and applies it to MainWindow.
// - Cancel restores the last saved values and closes the window.
(function () {
  const checkboxIds = [
    'englishWord',
    'multipleChoice',
    'japaneseWord',
    'dumbbellWorkout'
  ];

  const checkboxes = {
    englishWord: document.getElementById('englishWord'),
    multipleChoice: document.getElementById('multipleChoice'),
    japaneseWord: document.getElementById('japaneseWord'),
    dumbbellWorkout: document.getElementById('dumbbellWorkout')
  };

  let saved = {
    englishWord: true,
    multipleChoice: true,
    japaneseWord: true,
    dumbbellWorkout: false
  };

  function post(type, tasks) {
    if (!window.chrome || !window.chrome.webview) {
      return;
    }
    const message = { type: type };
    if (tasks) {
      message.tasks = tasks;
    }
    window.chrome.webview.postMessage(message);
  }

  function readDraft() {
    const draft = {};
    for (const id of checkboxIds) {
      draft[id] = !!checkboxes[id].checked;
    }
    return draft;
  }

  function applyToCheckboxes(state) {
    for (const id of checkboxIds) {
      checkboxes[id].checked = !!state[id];
    }
  }

  if (window.chrome && window.chrome.webview) {
    window.chrome.webview.addEventListener('message', function (event) {
      const data = event.data;
      if (data && data.type === 'applySettings' && data.tasks) {
        saved = {
          englishWord: !!data.tasks.englishWord,
          multipleChoice: !!data.tasks.multipleChoice,
          japaneseWord: !!data.tasks.japaneseWord,
          dumbbellWorkout: !!data.tasks.dumbbellWorkout
        };
        applyToCheckboxes(saved);
      }
    });
  }

  document.getElementById('openIgnoreWords').addEventListener('click', function () {
    post('openIgnoreWords');
  });

  document.getElementById('confirm').addEventListener('click', function () {
    saved = readDraft();
    post('confirmSettings', saved);
  });

  document.getElementById('cancel').addEventListener('click', function () {
    applyToCheckboxes(saved);
    post('closeSettings');
  });
})();
