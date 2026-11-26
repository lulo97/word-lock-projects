const audio = new Audio();
const activeButtons = new Set();
const animations = new Set();
const animationSteps = ['Speak.', 'Speak..', 'Speak...'];

const buttonClass =
  'speak-button rounded border border-gray-400 px-2 py-0.5 text-sm text-gray-700 hover:bg-gray-100';

let speechQueue = Promise.resolve();
let disposed = false;

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

async function playSpeech(text, lang, button, stopAnimation) {
  try {
    const base64 = await window.electronAPI.speak(text, lang);

    if (disposed) return;

    audio.pause();
    audio.src = `data:audio/wav;base64,${base64}`;
    await audio.play();
  } catch (error) {
    if (!disposed) console.error('TTS failed:', error);
  } finally {
    stopAnimation();

    if (!disposed) restoreButton(button);
  }
}

function speak(text, lang, button) {
  if (disposed || !button || activeButtons.has(button)) return;

  activeButtons.add(button);
  button.disabled = true;
  const stopAnimation = animate(button);

  speechQueue = speechQueue
    .then(() => playSpeech(text, lang, button, stopAnimation))
    .catch((error) => {
      if (!disposed) console.error('Speech queue error:', error);
    });
}

function createSpeakButton(text, lang) {
  const button = document.createElement('button');
  button.type = 'button';
  button.className = buttonClass;
  button.textContent = 'Speak';
  button.addEventListener('click', () => speak(text, lang, button));
  return button;
}

function bindSpeakButton(id, getText, lang) {
  const button = document.getElementById(id);

  if (button) {
    button.addEventListener('click', () => speak(getText(), lang, button));
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
