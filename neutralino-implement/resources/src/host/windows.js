import { native } from './native.js';
import { emit, sleep } from './bus.js';
import { getSettings, getNextTaskType } from './settings.js';
import { broadcastTheme } from './themes.js';
import { unfreezeChromeOnExit } from './chromeFreeze.js';

const TASKS = {
  english: '/src/english/english.html',
  mcq: '/src/mcq/mcq.html',
  japanese: '/src/japanese/japanese.html',
  dumbbell: '/src/dumbbell/dumbbell.html',
};

const COUNTDOWN_W = 100;
const COUNTDOWN_H = 50;

// Neutralino child windows are separate processes. Keep each one alive and
// navigate it between pages instead of spawning a new process every cycle.
const spawned = new Set();
const active = new Set();
let quitting = false;

export function isAlive(id) {
  return active.has(id);
}

// Stop the countdown iframe while a task is visible. Otherwise its interval
// keeps ticking and can fire countdown:finished in the middle of the task.
function stopCountdown() {
  const frame = document.getElementById('cd');
  if (!frame) return;
  try { frame.contentWindow?.stop(); } catch { /* already detached */ }
  delete frame.dataset.loaded;
  frame.src = 'about:blank';
}

function buildProcessArgs(id, options, theme) {
  const parts = [`--win=${id}`, `--theme=${theme}`];
  if (options.maximize) parts.push('--maximize');
  if (options.width) parts.push(`--width=${options.width}`);
  if (options.height) parts.push(`--height=${options.height}`);
  return parts.join(' ');
}

async function spawn(id, url, options = {}) {
  const theme = getSettings().theme;

  if (spawned.has(id)) {
    active.add(id);
    // A retired child parks on a bridge-enabled blank page, so it can still
    // receive this navigation broadcast without another process being created.
    emit('app:navigate', { to: id, url, theme });
    emit('app:focus', { to: id });
    return;
  }

  spawned.add(id);
  active.add(id);
  try {
    await Neutralino.window.create(url, {
      title: 'PomodoroLock Task',
      icon: '/resources/assets/icon.png',
      injectGlobals: true,
      injectClientLibrary: true,
      enableInspector: false,
      processArgs: buildProcessArgs(id, options, theme),
      ...options,
      // Closing a child must never take down the host or its Chrome unfreeze path.
      exitProcessOnClose: false,
    });
  } catch (error) {
    spawned.delete(id);
    active.delete(id);
    throw error;
  }
}

// Hide the child and park it on a same-origin page that still loads the bridge.
// The process is reused by the next spawn() for the same id.
async function retire(id) {
  if (!active.has(id)) return;
  active.delete(id);
  emit('app:hide', { to: id });
  await sleep(180);
}

export async function showMainWindow() {
  if (quitting) return;

  stopCountdown();
  try { await Neutralino.window.hide(); } catch { /* already hidden */ }

  await retire('task');

  const task = getNextTaskType();
  await spawn('task', TASKS[task] || TASKS.english, { maximize: true });
}

export async function hideMainWindow() {
  if (quitting) return;

  await retire('task');
  await showCountdown();
}

export async function showCountdown() {
  const frame = document.getElementById('cd');
  if (frame) {
    // The iframe is left empty in host.html so the saved theme can be applied
    // before its first paint. Load it on the first countdown, reload after that.
    if (!frame.dataset.loaded) {
      frame.dataset.loaded = '1';
      frame.src = '/src/countdown/countdown.html';
    } else if (frame.contentWindow) {
      frame.contentWindow.location.reload();
    }
  }

  const result = await native.call('screen.workArea');
  const rect = result.rect || { x: 0, y: 0, w: 1920, h: 1080 };

  await Neutralino.window.setSize({ width: COUNTDOWN_W, height: COUNTDOWN_H });
  await Neutralino.window.move(
    rect.x + rect.w - COUNTDOWN_W - 10,
    rect.y + rect.h - COUNTDOWN_H - 10,
  );
  await Neutralino.window.show();
  await Neutralino.window.setAlwaysOnTop(true);
  native.call('window.setOpacity', { title: 'PomodoroLock Countdown', alpha: 0.5 });
}

export function createSettingsWindow() {
  return spawn('settings', '/src/settings/settings.html', {
    title: 'PomodoroLock Settings',
    width: 460,
    height: 720,
    resizable: false,
  });
}

export async function closeSettingsWindow() {
  await retire('settings');
  // Closing settings cancels any live theme preview.
  broadcastTheme(getSettings().theme);
}

export function createIgnoreWordsWindow() {
  return spawn('ignore', '/src/settings/ignoreWords.html', {
    title: 'PomodoroLock Ignore Words',
    width: 560,
    height: 460,
    resizable: false,
  });
}

export function closeIgnoreWordsWindow() {
  return retire('ignore');
}

export function onChildClosed(id) {
  if (!spawned.has(id)) return;
  active.delete(id);

  if (id === 'settings') broadcastTheme(getSettings().theme);
  // Closing the task window returns to the countdown, as in Electron.
  if (id === 'task') hideMainWindow();
}

export async function quitAll() {
  if (quitting) return;
  quitting = true;
  // Wait for Chrome to be released before the process disappears.
  await unfreezeChromeOnExit();
  emit('app:quit');
  await sleep(180);
  await Neutralino.app.exit();
}

export function isQuitting() {
  return quitting;
}

export function setQuitting(value) {
  quitting = Boolean(value);
}
