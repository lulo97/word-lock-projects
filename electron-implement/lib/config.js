const path = require("path");
const os = require("os");
const { app } = require("electron");

const APP_NAME = "Kotoba Lock";
const APP_ID = "KotobaLock";
const APP_USER_MODEL_ID = "com.kotobalock.app";

// Milliseconds between the word gate closing and opening again.
const AUTO_SHOW_DELAY_MS = 60000 * 1;

// Playback gain relative to the Windows master volume. 2.0 = twice as loud.
const SOUND_VOLUME = 2.0;

// Bundled data lives beside the source in development and in resources/ when
// packaged, mirroring the PyInstaller layout of the Python app.
const RESOURCE_ROOT = app.isPackaged
  ? process.resourcesPath
  : path.resolve(__dirname, "..");

const DATABASE_DIR = path.join(RESOURCE_ROOT, "database");
const ASSETS_DIR = path.join(RESOURCE_ROOT, "assets");

// Settings and progress are deliberately kept apart from the Python app's
// %USERPROFILE%/wordlock folder so both applications can be used side by side.
const DATA_DIR = path.join(os.homedir(), "kotobalock");

module.exports = {
  APP_NAME,
  APP_ID,
  APP_USER_MODEL_ID,
  AUTO_SHOW_DELAY_MS,
  SOUND_VOLUME,
  RESOURCE_ROOT,
  DATABASE_DIR,
  ASSETS_DIR,
  DATA_DIR,
};
