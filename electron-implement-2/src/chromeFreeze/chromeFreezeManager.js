const { getSettings } = require('../settings/settings');
const { freezeChrome, unfreezeChrome } = require('./index');

function safely(fn) {
  try {
    return fn();
  } catch {
    // Chrome freeze is a convenience feature; never let it break the app.
    return 0;
  }
}

// First open: clear any freezes left over from a crash or forced exit.
function initializeChromeFreeze() {
  safely(() => unfreezeChrome());
  if (getSettings().chromeFreeze) {
    safely(() => freezeChrome());
  }
}

// Called when the setting is confirmed/changed.
function syncChromeFreeze() {
  if (getSettings().chromeFreeze) {
    safely(() => freezeChrome());
  } else {
    safely(() => unfreezeChrome());
  }
}

let didUnfreezeOnExit = false;

// Exit / kill: always give Chrome back so videos and work are not blocked.
function unfreezeChromeOnExit() {
  if (didUnfreezeOnExit) return;
  didUnfreezeOnExit = true;
  safely(() => unfreezeChrome());
}

module.exports = { initializeChromeFreeze, syncChromeFreeze, unfreezeChromeOnExit };
