import { native } from './native.js';
import {
  getSettings, setSettings, advanceTheme, getIgnoreWords, setIgnoreWords,
  normalizeTheme, IGNORE_ERROR,
} from './settings.js';
import { getThemes, broadcastTheme } from './themes.js';
import { emit } from './bus.js';
import {
  showMainWindow, hideMainWindow, createSettingsWindow, closeSettingsWindow,
  createIgnoreWordsWindow, closeIgnoreWordsWindow, onChildClosed,
} from './windows.js';
import { syncChromeFreeze } from './chromeFreeze.js';
import { applyAutoStartPreference } from './autoStart.js';
import { synthesize, TTS_UNAVAILABLE_MESSAGE } from './tts.js';

function applySettings(values) {
  const before = getSettings();
  setSettings(values);
  const after = getSettings();

  // Fire-and-forget, just like the old stdin-based helper: renderer calls
  // must not wait on a DB/RPC round trip. Only touch native state that changed.
  if (before.chromeFreeze !== after.chromeFreeze) void syncChromeFreeze();
  if (before.autoStart !== after.autoStart) void applyAutoStartPreference(after.autoStart);
  broadcastTheme(after.theme);
}

const handlers = {
  'word:getRandom': () =>
    native.call('db.randomEnglishWord', { ignore: getIgnoreWords().english }).then((r) => r.row),
  'word:get': (id) => native.call('db.englishWord', { id }).then((r) => r.row),
  'mcq:getRandom': () => native.call('db.randomMcq').then((r) => r.row),
  'mcq:get': (id) => native.call('db.mcq', { id }).then((r) => r.row),
  'japanese:getRandom': () =>
    native.call('db.randomJapaneseWord', { ignore: getIgnoreWords().japanese }).then((r) => r.row),
  'japanese:get': (id) => native.call('db.japaneseWord', { id }).then((r) => r.row),

  'settings:get': () => getSettings(),
  'settings:set': (values) => { applySettings(values); },
  'settings:confirm': async (values) => {
    applySettings(values);
    // hideMainWindow always restarts the countdown, even when no task is open.
    await hideMainWindow();
    await closeSettingsWindow();
  },
  'settings:close': () => closeSettingsWindow(),

  'theme:list': () => getThemes(),
  'theme:preview': (id) => broadcastTheme(normalizeTheme(id)),

  'ignoreWords:open': () => createIgnoreWordsWindow(),
  'ignoreWords:get': () => getIgnoreWords(),
  'ignoreWords:save': (values) => {
    try {
      return { ok: true, words: setIgnoreWords(values) };
    } catch {
      return { ok: false, error: IGNORE_ERROR };
    }
  },
  'ignoreWords:close': () => closeIgnoreWordsWindow(),

  'countdown:finished': () => showMainWindow(),
  'app:exitInfo': () => {
    if (getSettings().autoTheme) broadcastTheme(advanceTheme());
    hideMainWindow();
  },
  'win:closed': (id) => onChildClosed(id),

  'tts:speak': async (text, lang) => {
    try {
      return { ok: true, wav: await synthesize(text, lang) };
    } catch (error) {
      return { ok: false, error: error?.message || TTS_UNAVAILABLE_MESSAGE };
    }
  },
};

const overrides = new Map();
let listening = false;

async function handleIpcRequest({ detail }) {
  // app.broadcast reaches the sender too. Only handle requests that a child
  // explicitly addressed to the host, and never handle our own broadcasts.
  if (!detail || detail.to !== 'host' || detail.from === 'host') return;

  const { id, from, channel, args } = detail;
  const router = window.__router;
  let result;
  try {
    result = router ? await router.handle(channel, args || []) : undefined;
  } catch (error) {
    result = { ok: false, error: error?.message || String(error) };
  }
  if (id) emit('ipc:res', { id, to: from, result });
}

export function createRouter() {
  const router = {
    handle(channel, args = []) {
      const fn = overrides.get(channel) || handlers[channel];
      return fn ? fn(...args) : undefined;
    },
    override(channel, fn) {
      overrides.set(channel, fn);
    },
    clearOverride(channel) {
      if (channel) overrides.delete(channel);
      else overrides.clear();
    },
  };
  window.__router = router;
  if (!listening) {
    Neutralino.events.on('ipc:req', handleIpcRequest);
    listening = true;
  }
  return router;
}
