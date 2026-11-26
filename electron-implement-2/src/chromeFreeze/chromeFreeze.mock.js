const fs = require('node:fs');

const calls = [];
let frozen = false;

function record(name) {
  calls.push(name);

  const logPath = process.env.WORDLOCK_CHROME_FREEZE_LOG;
  if (logPath) {
    try {
      fs.appendFileSync(logPath, `${name}\n`);
    } catch {
      // Logging is only for tests.
    }
  }
}

function freezeChrome() {
  record('freezeChrome');
  frozen = true;
  return 1;
}

function unfreezeChrome() {
  record('unfreezeChrome');
  frozen = false;
  return 1;
}

function getCalls() {
  return [...calls];
}

function isFrozen() {
  return frozen;
}

function reset() {
  calls.length = 0;
  frozen = false;

  const logPath = process.env.WORDLOCK_CHROME_FREEZE_LOG;
  if (logPath) {
    try {
      fs.writeFileSync(logPath, '');
    } catch {
      // Logging is only for tests.
    }
  }
}

module.exports = { freezeChrome, unfreezeChrome, getCalls, isFrozen, reset };
