const calls = [];

let state = { openAtLogin: false };
let disabledByWindows = false;

function getState() {
  return {
    openAtLogin: state.openAtLogin,
    executableWillLaunchAtLogin: state.openAtLogin && !disabledByWindows,
  };
}

function setAutoStart(enabled) {
  calls.push(`set:${Boolean(enabled)}`);
  state = { openAtLogin: Boolean(enabled) };
  return getState();
}

function syncAutoStart(wantsAutoStart) {
  calls.push(`sync:${Boolean(wantsAutoStart)}`);
  const current = getState();

  if (!wantsAutoStart) {
    return current.openAtLogin ? setAutoStart(false) : current;
  }

  if (current.openAtLogin && !current.executableWillLaunchAtLogin) {
    return current;
  }

  return setAutoStart(true);
}

function getCalls() {
  return [...calls];
}

function setDisabledByWindows(value) {
  disabledByWindows = Boolean(value);
}

function setState(next) {
  state = { openAtLogin: Boolean(next.openAtLogin) };
}

function reset() {
  calls.length = 0;
  state = { openAtLogin: false };
  disabledByWindows = false;
}

module.exports = {
  getState,
  setAutoStart,
  syncAutoStart,
  getCalls,
  setDisabledByWindows,
  setState,
  reset,
};
