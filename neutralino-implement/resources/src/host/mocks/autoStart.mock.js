// Pure-JS stand-in for src/autoStart/autoStart.mock.js in the old app.
const calls = [];

let state = { openAtLogin: false };
let disabledByWindows = false;

function getState() {
  return {
    openAtLogin: state.openAtLogin,
    executableWillLaunchAtLogin: state.openAtLogin && !disabledByWindows,
  };
}

function doSet(enabled) {
  calls.push(`set:${Boolean(enabled)}`);
  state = { openAtLogin: Boolean(enabled) };
  return getState();
}

function sync(wants) {
  calls.push(`sync:${Boolean(wants)}`);
  const current = getState();
  if (!wants) return current.openAtLogin ? doSet(false) : current;
  if (current.openAtLogin && !current.executableWillLaunchAtLogin) return current;
  return doSet(true);
}

export function call(method, params = {}) {
  switch (method) {
    case 'autostart.get': return { ok: true, state: getState() };
    case 'autostart.set': return { ok: true, state: doSet(params.enabled) };
    case 'autostart.sync': return { ok: true, state: sync(params.wants) };
    default: return { ok: true };
  }
}

export function getCalls() { return [...calls]; }
export function setState(next) { state = { openAtLogin: Boolean(next.openAtLogin) }; }
export function setDisabledByWindows(value) { disabledByWindows = Boolean(value); }
export function reset() {
  calls.length = 0;
  state = { openAtLogin: false };
  disabledByWindows = false;
}

export const mock = { call, getCalls, setState, setDisabledByWindows, reset, getState };
