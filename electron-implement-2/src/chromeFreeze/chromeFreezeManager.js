const { getSettings } = require('../settings/settings');
const {
  initializeChromeFreeze: warmUpChromeFreeze,
  freezeChrome,
  unfreezeChrome,
  shutdownChromeFreeze,
} = require('./index');

const REFREEZE_INTERVAL_MS = 2000;

async function safely(fn) {
  try {
    return await fn();
  } catch {
    // Chrome freeze is a convenience feature; never let it break the app.
    return 0;
  }
}

// Begin the one-time PowerShell startup at require time so its ~0.5s Add-Type
// cost overlaps with Electron starting up. Test mocks have no helper to warm.
if (typeof warmUpChromeFreeze === 'function') warmUpChromeFreeze();

let refreezeTimer = null;

function stopRefreeze() {
  if (refreezeTimer) {
    clearInterval(refreezeTimer);
    refreezeTimer = null;
  }
}

// Chrome can spawn child processes after a freeze, so re-run the idempotent
// freeze while the setting is on. The helper skips PIDs it already holds.
function startRefreeze() {
  if (refreezeTimer) return;
  refreezeTimer = setInterval(() => {
    safely(() => freezeChrome());
  }, REFREEZE_INTERVAL_MS);
  refreezeTimer.unref?.();
}

// First open: clear any freezes left over from a crash or forced exit.
async function initializeChromeFreeze() {
  await safely(() => unfreezeChrome());

  if (getSettings().chromeFreeze) {
    await safely(() => freezeChrome());
    startRefreeze();
  }
}

// Called when the setting is confirmed/changed.
async function syncChromeFreeze() {
  if (getSettings().chromeFreeze) {
    await safely(() => freezeChrome());
    startRefreeze();
  } else {
    stopRefreeze();
    await safely(() => unfreezeChrome());
  }
}

let didUnfreezeOnExit = false;

// Exit / kill: always give Chrome back so videos and work are not blocked.
function unfreezeChromeOnExit() {
  if (didUnfreezeOnExit) return;
  didUnfreezeOnExit = true;

  stopRefreeze();

  if (typeof shutdownChromeFreeze === 'function') {
    // Sends one last 'U', then closes stdin. PowerShell resumes Chrome on EOF
    // even if Electron dies before the command is answered.
    shutdownChromeFreeze();
  } else {
    // Mock path: keep the recorded call for tests.
    safely(() => unfreezeChrome());
  }
}

module.exports = { initializeChromeFreeze, syncChromeFreeze, unfreezeChromeOnExit };
