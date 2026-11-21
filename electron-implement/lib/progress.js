const fs = require("fs");
const path = require("path");

const config = require("./config");

const PROGRESS_DIR = config.DATA_DIR;
const PROGRESS_FILE = path.join(PROGRESS_DIR, "progress.json");

function wordKey(word) {
  return String(word || "").trim().toLowerCase();
}

function read() {
  let data;
  try {
    data = JSON.parse(fs.readFileSync(PROGRESS_FILE, "utf8"));
  } catch (error) {
    return { version: 1, attempts: {} };
  }

  if (!data || typeof data !== "object" || Array.isArray(data)) {
    return { version: 1, attempts: {} };
  }

  const rawAttempts =
    data.attempts && typeof data.attempts === "object" && !Array.isArray(data.attempts)
      ? data.attempts
      : {};

  // Keep only well-formed counters so a corrupted file cannot break the app.
  const attempts = {};
  for (const [word, count] of Object.entries(rawAttempts)) {
    if (
      typeof word === "string" &&
      Number.isInteger(count) &&
      count >= 0
    ) {
      attempts[word] = count;
    }
  }

  return { version: 1, attempts };
}

function write(data) {
  fs.mkdirSync(PROGRESS_DIR, { recursive: true });
  const temp = PROGRESS_FILE.replace(/\.json$/i, ".json.tmp");
  fs.writeFileSync(temp, JSON.stringify(data, null, 2) + "\n", "utf8");
  fs.renameSync(temp, PROGRESS_FILE);
}

function loadAttempts() {
  return { ...read().attempts };
}

function recordSuccess(word) {
  const key = wordKey(word);
  if (!key) return 0;

  const data = read();
  const count = (data.attempts[key] || 0) + 1;
  data.attempts[key] = count;
  try {
    write(data);
  } catch (error) {
    // Progress is a nice-to-have; never block the word gate on disk errors.
  }
  return count;
}

function attemptCount(word, attempts) {
  const source = attempts || loadAttempts();
  return source[wordKey(word)] || 0;
}

module.exports = {
  PROGRESS_DIR,
  PROGRESS_FILE,
  wordKey,
  loadAttempts,
  recordSuccess,
  attemptCount,
};
