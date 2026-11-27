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

  // WebView2 has no setPermissionRequestHandler. The port is fixed, so the
  // user only answers once; if they blocked it, fail with a usable message
  // instead of leaving the dumbbell page stuck on "Starting pose tracker...".
  installCameraPermissionFallback();

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

  // Reused children keep their original process args. Re-apply the window
  // state after each navigation so a parked child comes back the right size.
  ready.then(async () => {
    if (isHost || location.pathname.endsWith('childPark.html')) return;
    try {
      if (args.includes('--maximize')) {
        await Neutralino.window.maximize();
      } else {
        const width = Number(arg('width'));
        const height = Number(arg('height'));
        if (width > 0 && height > 0) {
          await Neutralino.window.setSize({ width, height });
        }
      }
      await Neutralino.window.show();
    } catch { /* window state is cosmetic */ }
  });

  const waiting = new Map();
  let seq = 0;
  // A child process keeps the same id across page navigations, so a reply to
  // an old page's `task:1` must not resolve a new page's `task:1`.
  const pageNonce = `${Date.now().toString(36)}-${Math.random().toString(36).slice(2)}`;

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
      const id = `${myId}:${pageNonce}:${++seq}`;
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

  Neutralino.events.on('app:navigate', ({ detail }) => {
    if (isHost || !detail || detail.to !== myId || !detail.url) return;
    if (detail.theme) {
      themeId = detail.theme;
      if (window.electronAPI) window.electronAPI.themeId = themeId;
    }
    window.location.replace(detail.url);
  });

  // Parking keeps the child process alive so the host can navigate it to the
  // next task instead of spawning another WebView. The bridge must also run on
  // this page so it can receive the later app:navigate broadcast.
  const PARK_URL = '/src/host/childPark.html';
  const retireChild = () => {
    if (isHost || location.pathname.endsWith('childPark.html')) return;
    Neutralino.window.hide().catch(() => {});
    setTimeout(() => window.location.replace(PARK_URL), 60);
  };

  const hideListeners = [];
  function runHideCallbacks() {
    // Both app:hide and app:close may arrive for the same retirement. Run each
    // cleanup once so the camera stream and TTS stop before navigation.
    const callbacks = hideListeners.splice(0);
    for (const cb of callbacks) {
      try { cb(); } catch { /* cleanup is best effort */ }
    }
  }

  Neutralino.events.on('app:hide', ({ detail }) => {
    if (!detail || detail.to !== myId) return;
    runHideCallbacks();
    // Give camera/TTS cleanup a chance to finish before navigation.
    setTimeout(retireChild, 60);
  });
  Neutralino.events.on('app:close', ({ detail }) => {
    if (!detail || detail.to !== myId) return;
    runHideCallbacks();
    retireChild();
  });
  Neutralino.events.on('app:focus', ({ detail }) => {
    if (detail.to === myId) Neutralino.window.focus();
  });
  Neutralino.events.on('app:quit', () => {
    // Each Neutralino window is its own process; a child only exits itself.
    if (!isHost) Neutralino.app.exit();
  });
  if (!isHost) {
    Neutralino.events.on('windowClose', () => {
      // Neutralino does not act on windowClose by itself: hide immediately so
      // the X button behaves, then let the host retire/reuse this child.
      Neutralino.window.hide().catch(() => {});
      send('win:closed', myId);
      retireChild();
    });
  }

  function installCameraPermissionFallback() {
    const media = navigator.mediaDevices;
    if (!media || typeof media.getUserMedia !== 'function') return;

    const original = media.getUserMedia.bind(media);
    const blocked = `Camera access is blocked. Allow the camera for ${window.location.origin}, then reopen this task.`;

    try {
      Object.defineProperty(media, 'getUserMedia', {
        configurable: true,
        writable: true,
        value: async (constraints) => {
          try {
            if (navigator.permissions && typeof navigator.permissions.query === 'function') {
              const status = await navigator.permissions.query({ name: 'camera' });
              if (status.state === 'denied') throw new Error(blocked);
            }
          } catch (error) {
            // Permissions API is optional; an unsupported query is not a denial.
            if (error && error.message === blocked) throw error;
          }

          try {
            return await original(constraints);
          } catch (error) {
            if (error && (error.name === 'NotAllowedError' || error.name === 'PermissionDeniedError')) {
              throw new Error(blocked);
            }
            throw error;
          }
        },
      });
    } catch {
      // If a runtime locks the property down, the original getUserMedia still
      // works; the user just sees the browser's own permission prompt.
    }
  }

  const api = {
    // Keep the host's `window.electronAPI.themeId = ...` assignment and the
    // theme:changed listener on the same underlying value.
    get themeId() { return themeId; },
    set themeId(value) { themeId = value; },
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
