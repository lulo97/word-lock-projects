const { app } = require('electron');

// Portable builds extract to a temp folder; point at the real exe instead.
function executablePath() {
  return process.env.PORTABLE_EXECUTABLE_FILE || process.execPath;
}

function getState() {
  const state = app.getLoginItemSettings({ path: executablePath() });
  return {
    openAtLogin: Boolean(state.openAtLogin),
    executableWillLaunchAtLogin: Boolean(state.executableWillLaunchAtLogin),
  };
}

function setAutoStart(enabled) {
  app.setLoginItemSettings({
    openAtLogin: Boolean(enabled),
    path: executablePath(),
    args: [],
  });
  return getState();
}

// Called on every launch so a moved exe rewrites the registry with its new path.
// If Windows 11 disabled the entry in Startup Apps, leave that choice alone.
function syncAutoStart(wantsAutoStart) {
  const state = getState();

  if (!wantsAutoStart) {
    return state.openAtLogin ? setAutoStart(false) : state;
  }

  if (state.openAtLogin && !state.executableWillLaunchAtLogin) {
    return state;
  }

  return setAutoStart(true);
}

module.exports = { getState, setAutoStart, syncAutoStart, executablePath };
