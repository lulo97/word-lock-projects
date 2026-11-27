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
  async call(method, params = {}, timeout = 5000) {
    if (method.startsWith('chrome.') && mocks.chrome) {
      const result = await mocks.chrome.call(method, params);
      // Mock log writes are queued; a quitting host must not exit before the
      // final unfreeze line has been flushed to disk.
      if (typeof mocks.chrome.flush === 'function') await mocks.chrome.flush();
      return result;
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

let readyProbe = null;

// The extension is spawned by Neutralino, so it may not have connected by the
// time host.js runs. Probe with a short timeout instead of losing the first
// real RPC to the 5-second default timeout.
export function waitReady(timeout = 15000) {
  if (readyProbe) return readyProbe;

  readyProbe = (async () => {
    const deadline = Date.now() + timeout;
    let lastError = 'extension did not answer native.ping';

    while (Date.now() < deadline) {
      try {
        const result = await native.call('native.ping', {}, 800);
        if (result && result.ok && result.pong) return result;
        lastError = result?.error || lastError;
      } catch (error) {
        lastError = error?.message || String(error);
      }
      await new Promise((resolve) => setTimeout(resolve, 150));
    }

    throw new Error(`wordlock-native extension did not become ready: ${lastError}`);
  })();

  return readyProbe;
}
