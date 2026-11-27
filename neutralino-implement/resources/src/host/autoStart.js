import { getSettings } from './settings.js';
import { native } from './native.js';

async function safely(method, params) {
  try {
    return await native.call(method, params);
  } catch {
    // Auto-start is a convenience; registry access must never break startup.
    return null;
  }
}

export function initializeAutoStart() {
  return safely('autostart.sync', { wants: Boolean(getSettings().autoStart) });
}

export function applyAutoStartPreference(enabled) {
  return safely('autostart.set', { enabled: Boolean(enabled) });
}
