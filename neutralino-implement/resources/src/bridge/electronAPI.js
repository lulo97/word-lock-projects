/*
 * bridge/electronAPI.js
 *
 * Drop-in replacement for Electron's preload.js. Renderer files are unchanged:
 * they still call window.electronAPI.* and listen for the same events.
 *
 * The host window talks to its in-page router directly. Child windows send
 * ipc:req / receive ipc:res over Neutralino's app.broadcast event bus.
 */
(() => {
  // The countdown iframe shares the host's API (same http origin).
  if (window.parent !== window && window.parent.electronAPI) {
    window.electronAPI = window.parent.electronAPI;
    return;
  }

  const args = Array.isArray(window.NL_ARGS) ? window.NL_ARGS : [];
  const arg = (key) => {
    const prefix = `--${key}=`;
    const found = args.find((a) => a.startsWith(prefix));
    return found ? found.slice(prefix.length) : '';
  };

  // The host is launched without --win; every child is spawned with one.
  const isHost = !args.some((a) => a.startsWith('--win='));
  const myId = isHost ? 'host' : arg('win') || 'task';
  let themeId = arg('theme') || 'paper';

  let markReady;
  const ready = new Promise((resolve) => { markReady = resolve; });
  window.__bridgeReady = ready;
  Neutralino.init();
  Neutralino.events.on('ready', () => markReady());

  const waiting = new Map();
  let seq = 0;

  Neutralino.events.on('ipc:res', ({ detail }) => {
    // Broadcasts reach every WebView. Only accept replies addressed to this id.
    if (!detail || detail.to !== myId) return;
    const pending = waiting.get(detail.id);
    if (!pending) return;
    waiting.delete(detail.id);
    clearTimeout(pending.timer);
    pending.resolve(detail.result);
  });

  const invoke = (channel, ...values) => {
    if (isHost) {
      try {
        return Promise.resolve(window.__router ? window.__router.handle(channel, values) : undefined);
      } catch (error) {
        return Promise.reject(error);
      }
    }

    return ready.then(() => new Promise((resolve) => {
      const id = `${myId}:${++seq}`;
      const timer = setTimeout(() => {
        if (waiting.delete(id)) resolve(undefined);
      }, 8000);
      waiting.set(id, { resolve, timer });
      Neutralino.app.broadcast('ipc:req', {
        id,
        from: myId,
        to: 'host',
        channel,
        args: values,
      });
    }));
  };

  const send = (channel, ...values) => {
    if (isHost) {
      if (window.__router) window.__router.handle(channel, values);
      return;
    }
    ready.then(() => Neutralino.app.broadcast('ipc:req', {
      from: myId,
      to: 'host',
      channel,
      args: values,
    }));
  };

  const themeListeners = [];
  Neutralino.events.on('theme:changed', ({ detail }) => {
    themeId = detail;
    if (api) api.themeId = detail;
    for (const cb of themeListeners) {
      try { cb(detail); } catch { /* a bad listener must not break the theme */ }
    }
  });

  // Neutralino has no API to close a single child window, and app.exit() would
  // terminate the whole application. Hide the WebView and navigate it away:
  // the host stays alive in the tray, and tests no longer see the old URL.
  const retireChild = () => {
    Neutralino.window.hide().catch(() => {});
    setTimeout(() => window.location.replace('about:blank'), 60);
  };

  const hideListeners = [];
  Neutralino.events.on('app:hide', ({ detail }) => {
    if (detail.to !== myId) return;
    for (const cb of hideListeners) {
      try { cb(); } catch { /* cleanup is best effort */ }
    }
    // Give camera/TTS cleanup a chance to finish before navigation.
    setTimeout(retireChild, 60);
  });
  Neutralino.events.on('app:close', ({ detail }) => {
    if (detail.to === myId) retireChild();
  });
  Neutralino.events.on('app:focus', ({ detail }) => {
    if (detail.to === myId) Neutralino.window.focus();
  });
  Neutralino.events.on('app:quit', () => {
    // The host exits the whole Neutralino process; children need no action.
  });
  if (!isHost) {
    Neutralino.events.on('windowClose', () => {
      send('win:closed', myId);
    });
  }

  const api = {
    themeId,
    listThemes: () => invoke('theme:list'),
    previewTheme: (id) => send('theme:preview', id),
    onThemeChanged: (cb) => themeListeners.push(cb),
    getRandomWord: () => invoke('word:getRandom'),
    getWord: (id) => invoke('word:get', id),
    getRandomMcq: () => invoke('mcq:getRandom'),
    getMcq: (id) => invoke('mcq:get', id),
    getRandomJapaneseWord: () => invoke('japanese:getRandom'),
    getJapaneseWord: (id) => invoke('japanese:get', id),
    getSettings: () => invoke('settings:get'),
    setSettings: (values) => invoke('settings:set', values),
    confirmSettings: (values) => invoke('settings:confirm', values),
    closeSettings: () => send('settings:close'),
    openIgnoreWords: () => invoke('ignoreWords:open'),
    getIgnoreWords: () => invoke('ignoreWords:get'),
    saveIgnoreWords: (values) => invoke('ignoreWords:save', values),
    closeIgnoreWords: () => send('ignoreWords:close'),
    countdownFinished: () => send('countdown:finished'),
    exitInfo: () => send('app:exitInfo'),
    speak: (text, lang) => invoke('tts:speak', text, lang),
    onHide: (cb) => hideListeners.push(cb),
  };

  window.electronAPI = api;
})();
