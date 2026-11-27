// Sequential speech queue with button animation and spam-click protection.
(function () {
  'use strict';

  var audio = new Audio();
  var activeButtons = new Set();
  var animations = new Set();
  var animationSteps = ['Speak.', 'Speak..', 'Speak...'];
  var statusId = 'tts-status';
  var defaultError = 'Speech could not be played.';
  var speechQueue = Promise.resolve();
  var disposed = false;

  function getStatusEl() {
    var statusEl = document.getElementById(statusId);
    if (statusEl) return statusEl;

    statusEl = document.createElement('p');
    statusEl.id = statusId;
    statusEl.setAttribute('role', 'status');
    statusEl.setAttribute('aria-live', 'polite');
    statusEl.className = 'ui-feedback text-center';
    statusEl.dataset.state = 'bad';
    var mainEl = document.querySelector('main');
    (mainEl || document.body).prepend(statusEl);
    return statusEl;
  }

  function showTtsMessage(message) {
    if (!disposed) getStatusEl().textContent = message;
  }

  function clearTtsMessage() {
    var statusEl = document.getElementById(statusId);
    if (statusEl) statusEl.textContent = '';
  }

  function restoreButton(button) {
    if (!button) return;
    activeButtons.delete(button);
    button.textContent = 'Speak';
    button.disabled = false;
  }

  function animate(button) {
    if (!button) return function () {};
    var step = 0;
    button.textContent = animationSteps[step];
    var timer = setInterval(function () {
      step = (step + 1) % animationSteps.length;
      button.textContent = animationSteps[step];
    }, 300);
    animations.add(timer);
    return function () {
      clearInterval(timer);
      animations.delete(timer);
    };
  }

  function playSpeech(text, lang, button, stopAnimation, userInitiated) {
    return window.app.call('tts:speak', { text: text, lang: lang }).then(function (result) {
      if (disposed) return;
      var base64 = result && result.ok ? result.wav : null;
      if (!base64) {
        // Automatic speech stays quiet on failure; user clicks report the error.
        if (userInitiated) showTtsMessage((result && result.error) || defaultError);
        return;
      }

      clearTtsMessage();
      audio.pause();
      audio.src = 'data:audio/wav;base64,' + base64;
      return audio.play();
    }).catch(function (error) {
      if (disposed) return;
      console.error('TTS failed:', error);
      if (userInitiated) showTtsMessage(defaultError);
    }).then(function () {
      stopAnimation();
      if (!disposed) restoreButton(button);
    });
  }

  function speak(text, lang, button, options) {
    if (disposed || !button || activeButtons.has(button)) return;
    var userInitiated = Boolean(options && options.userInitiated);
    activeButtons.add(button);
    button.disabled = true;
    var stopAnimation = animate(button);
    speechQueue = speechQueue.then(function () {
      return playSpeech(text, lang, button, stopAnimation, userInitiated);
    }).catch(function (error) {
      if (!disposed) console.error('Speech queue error:', error);
    });
  }

  function createSpeakButton(text, lang) {
    var button = document.createElement('button');
    button.type = 'button';
    button.className = 'ui-btn ui-btn--sm';
    button.textContent = 'Speak';
    button.addEventListener('click', function () {
      speak(text, lang, button, { userInitiated: true });
    });
    return button;
  }

  function bindSpeakButton(id, getText, lang) {
    var button = document.getElementById(id);
    if (!button) return;
    button.addEventListener('click', function () {
      speak(getText(), lang, button, { userInitiated: true });
    });
  }

  function cleanup() {
    if (disposed) return;
    disposed = true;
    audio.pause();
    audio.removeAttribute('src');
    audio.load();
    animations.forEach(function (timer) { clearInterval(timer); });
    animations.clear();
    activeButtons.forEach(restoreButton);
    activeButtons.clear();
  }

  window.addEventListener('pagehide', cleanup);
  window.addEventListener('beforeunload', cleanup);
  if (window.app && window.app.on) {
    window.app.on('app:hide', cleanup);
  }

  window.speakUI = {
    speak: speak,
    createSpeakButton: createSpeakButton,
    bindSpeakButton: bindSpeakButton,
    cleanup: cleanup
  };
})();
