import { getSettings } from './settings.js';
import { native } from './native.js';

async function safely(method, params = {}) {
  try {
    return await native.call(method, params);
  } catch {
    // Chrome freeze is a convenience; never let it break startup.
    return { ok: false };
  }
}

// The 2-second re-freeze loop lives in Rust, so a throttled WebView cannot
// miss it. The JS side only turns it on/off.
export async function initializeChromeFreeze() {
  await safely('chrome.resumeAll'); // clear freezes left over from a crash
  const on = Boolean(getSettings().chromeFreeze);
  await safely('chrome.refreeze', { on });
  if (on) await safely('chrome.freeze');
}

export async function syncChromeFreeze() {
  const on = Boolean(getSettings().chromeFreeze);
  await safely('chrome.refreeze', { on });
  if (on) await safely('chrome.freeze');
  else await safely('chrome.unfreeze');
}

let didUnfreezeOnExit = false;

export function unfreezeChromeOnExit() {
  if (didUnfreezeOnExit) return;
  didUnfreezeOnExit = true;
  void safely('chrome.refreeze', { on: false });
  void safely('chrome.unfreeze');
}
