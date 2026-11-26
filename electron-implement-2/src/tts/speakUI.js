const audio = new Audio();
const activeButtons = new Set();
const animations = new Set();
const animationSteps = ['Speak.', 'Speak..', 'Speak...'];

const buttonClass = 'ui-btn ui-btn--sm';

const TTS_STATUS_ID = 'tts-status';
const DEFAULT_ERROR = 'Speech could not be played.';

let speechQueue = Promise.resolve();
let disposed = false;

function getStatusEl() {
  let statusEl = document.getElementById(TTS_STATUS_ID);
  if (statusEl) return statusEl;

  statusEl = document.createElement('p');
  statusEl.id = TTS_STATUS_ID;
  statusEl.setAttribute('role', 'status');
  statusEl.setAttribute('aria-live', 'polite');
  statusEl.className = 'ui-feedback text-center';
  statusEl.dataset.state = 'bad';

  const mainEl = document.querySelector('main');
  (mainEl || document.body).prepend(statusEl);
  return statusEl;
}

function showTtsMessage(message) {
  if (disposed) return;
  getStatusEl().textContent = message;
}

function clearTtsMessage() {
  const statusEl = document.getElementById(TTS_STATUS_ID);
  if (statusEl) statusEl.textContent = '';
}

function restoreButton(button) {
  activeButtons.delete(button);
  button.textContent = 'Speak';
  button.disabled = false;
}

function animate(button) {
  let step = 0;
  button.textContent = animationSteps[step];

  const timer = setInterval(() => {
    step = (step + 1) % animationSteps.length;
    button.textContent = animationSteps[step];
  }, 300);

  animations.add(timer);

  return () => {
    clearInterval(timer);
    animations.delete(timer);
  };
}

async function playSpeech(text, lang, button, stopAnimation, userInitiated) {
  try {
    const result = await window.electronAPI.speak(text, lang);

    if (disposed) return;

    // The real IPC handler returns { ok, wav/error }. Older/simple stubs may
    // still return the base64 WAV string directly.
    const base64 = typeof result === 'string' ? result : result?.ok ? result.wav : null;

    if (!base64) {
      // Automatic speech on page load fails quietly; only show the error when
      // the user explicitly pressed a Speak button.
      if (userInitiated) showTtsMessage(result?.error || DEFAULT_ERROR);
      return;
    }

    clearTtsMessage();

    audio.pause();
    audio.src = `data:audio/wav;base64,${base64}`;
    await audio.play();
  } catch (error) {
    if (!disposed) {
      console.error('TTS failed:', error);
      if (userInitiated) showTtsMessage(DEFAULT_ERROR);
    }
  } finally {
    stopAnimation();

    if (!disposed) restoreButton(button);
  }
}

function speak(text, lang, button, options = {}) {
  if (disposed || !button || activeButtons.has(button)) return;

  const userInitiated = Boolean(options.userInitiated);
  activeButtons.add(button);
  button.disabled = true;
  const stopAnimation = animate(button);

  speechQueue = speechQueue
    .then(() => playSpeech(text, lang, button, stopAnimation, userInitiated))
    .catch((error) => {
      if (!disposed) console.error('Speech queue error:', error);
    });
}

function createSpeakButton(text, lang) {
  const button = document.createElement('button');
  button.type = 'button';
  button.className = buttonClass;
  button.textContent = 'Speak';
  button.addEventListener('click', () => speak(text, lang, button, { userInitiated: true }));
  return button;
}

function bindSpeakButton(id, getText, lang) {
  const button = document.getElementById(id);

  if (button) {
    button.addEventListener('click', () => speak(getText(), lang, button, { userInitiated: true }));
  }
}

function cleanup() {
  if (disposed) return;
  disposed = true;

  audio.pause();
  audio.removeAttribute('src');
  audio.load();

  for (const timer of animations) clearInterval(timer);
  animations.clear();

  for (const button of activeButtons) {
    button.textContent = 'Speak';
    button.disabled = false;
  }
  activeButtons.clear();
}

window.addEventListener('pagehide', cleanup);
window.addEventListener('beforeunload', cleanup);

if (window.electronAPI && window.electronAPI.onHide) {
  window.electronAPI.onHide(cleanup);
}

window.speakUI = { speak, createSpeakButton, bindSpeakButton, cleanup };
