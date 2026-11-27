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

const alive = new Set();
let quitting = false;
let hidingTask = false;

export function isAlive(id) {
  return alive.has(id);
}

async function spawn(id, url, options = {}) {
  if (alive.has(id)) {
    emit('app:focus', { to: id });
    return;
  }
  alive.add(id);
  try {
    await Neutralino.window.create(url, {
      title: 'PomodoroLock',
      icon: '/resources/assets/icon.png',
      injectGlobals: true,
      injectClientLibrary: true,
      enableInspector: false,
      exitProcessOnClose: false,
      processArgs: `--win=${id} --theme=${getSettings().theme}`,
      ...options,
    });
  } catch (error) {
    alive.delete(id);
    throw error;
  }
}

export async function showMainWindow() {
  if (quitting) return;

  try { await Neutralino.window.hide(); } catch { /* already hidden */ }

  if (alive.has('task')) {
    emit('app:close', { to: 'task' });
    alive.delete('task');
    await sleep(150);
  }

  const task = getNextTaskType();
  await spawn('task', TASKS[task] || TASKS.english, { maximize: true });
}

export async function hideMainWindow() {
  if (quitting || !alive.has('task')) return;

  hidingTask = true;
  alive.delete('task');
  // Children stop the camera/TTS themselves, then exit only their own process.
  emit('app:hide', { to: 'task' });
  await sleep(80);
  hidingTask = false;
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
  native.call('window.setOpacity', { title: 'PomodoroLock', alpha: 0.5 });
}

export function createSettingsWindow() {
  return spawn('settings', '/src/settings/settings.html', {
    title: 'Settings',
    width: 460,
    height: 720,
    resizable: false,
  });
}

export function closeSettingsWindow() {
  emit('app:close', { to: 'settings' });
}

export function createIgnoreWordsWindow() {
  return spawn('ignore', '/src/settings/ignoreWords.html', {
    title: 'Ignore Words',
    width: 560,
    height: 460,
    resizable: false,
  });
}

export function closeIgnoreWordsWindow() {
  emit('app:close', { to: 'ignore' });
}

export function onChildClosed(id) {
  if (!alive.has(id)) return;
  alive.delete(id);

  // Closing settings cancels any live theme preview.
  if (id === 'settings') broadcastTheme(getSettings().theme);
  // Closing the task window returns to the countdown, as in Electron.
  if (id === 'task' && !hidingTask) hideMainWindow();
}

export function quitAll() {
  if (quitting) return;
  quitting = true;
  unfreezeChromeOnExit();
  emit('app:quit');
  setTimeout(() => Neutralino.app.exit(), 120);
}

export function isQuitting() {
  return quitting;
}

export function setQuitting(value) {
  quitting = Boolean(value);
}
