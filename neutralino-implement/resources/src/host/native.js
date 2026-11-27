/*
 * Thin RPC client for the Rust extension. Every native capability that used to
 * live in Electron's main process (SQLite, Chrome freeze, registry, named
 * mutex, work area, window opacity, TTS) is a method on js.wordlock.native.
 */
const pending = new Map();
let seq = 0;

Neutralino.events.on('native:res', ({ detail }) => {
  const entry = pending.get(detail.id);
  if (!entry) return;
  pending.delete(detail.id);
  clearTimeout(entry.timer);
  entry.resolve(detail);
});

// Test doubles are installed by host.js when the matching env vars are set.
let mocks = { chrome: null, autostart: null };

export function configureMocks(next) {
  mocks = { ...mocks, ...next };
}

export function getMocks() {
  return mocks;
}

export const native = {
  call(method, params = {}, timeout = 5000) {
    if (method.startsWith('chrome.') && mocks.chrome) {
      return Promise.resolve(mocks.chrome.call(method, params));
    }
    if (method.startsWith('autostart.') && mocks.autostart) {
      return Promise.resolve(mocks.autostart.call(method, params));
    }

    return new Promise((resolve) => {
      const id = ++seq;
      const timer = setTimeout(() => {
        if (pending.delete(id)) resolve({ ok: false, error: 'timeout' });
      }, timeout);
      pending.set(id, { resolve, timer });
      Neutralino.extensions.dispatch('js.wordlock.native', 'rpc', { id, method, params });
    });
  },
};
