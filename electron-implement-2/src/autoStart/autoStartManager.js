const { getSettings } = require('../settings/settings');
const { syncAutoStart, setAutoStart } = require('./index');

function safely(fn) {
  try {
    return fn();
  } catch {
    // Auto-start is a convenience; never break startup because of registry access.
    return null;
  }
}

function initializeAutoStart() {
  return safely(() => syncAutoStart(Boolean(getSettings().autoStart)));
}

function applyAutoStartPreference(enabled) {
  return safely(() => setAutoStart(Boolean(enabled)));
}

module.exports = { initializeAutoStart, applyAutoStartPreference };
