/**
 * Start Kotoba Lock with Windows using the per-user registry Run key.
 *
 *     HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\Run
 *
 * No administrator rights are needed. The registry is the source of truth for
 * whether the entry exists; config.json stores the user's preference.
 */

const { execFileSync } = require("child_process");
const { app } = require("electron");

const APP_RUN_NAME = "KotobaLock";
const RUN_KEY = "HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\Run";

function supported() {
  return process.platform === "win32";
}

function launchCommand() {
  if (app.isPackaged) {
    return `"${process.execPath}"`;
  }
  // Development: electron.exe + the application directory.
  return `"${process.execPath}" "${app.getAppPath()}"`;
}

function read() {
  if (!supported()) return null;
  try {
    const output = execFileSync(
      "reg.exe",
      ["query", RUN_KEY, "/v", APP_RUN_NAME],
      { encoding: "utf8", windowsHide: true },
    );
    const match = output.match(/REG_SZ\s+(.+)$/m);
    return match ? match[1].trim() : null;
  } catch (error) {
    return null;
  }
}

function isEnabled() {
  return read() !== null;
}

function enable() {
  if (!supported()) return false;
  const command = launchCommand();
  if (read() === command) return true;
  try {
    execFileSync(
      "reg.exe",
      [
        "add",
        RUN_KEY,
        "/v",
        APP_RUN_NAME,
        "/t",
        "REG_SZ",
        "/d",
        command,
        "/f",
      ],
      { windowsHide: true },
    );
    return true;
  } catch (error) {
    return false;
  }
}

function disable() {
  if (!supported()) return false;
  try {
    execFileSync(
      "reg.exe",
      ["delete", RUN_KEY, "/v", APP_RUN_NAME, "/f"],
      { windowsHide: true },
    );
    return true;
  } catch (error) {
    const message = `${error.stdout || ""} ${error.stderr || ""}`.toLowerCase();
    if (message.includes("unable to find") || message.includes("not found")) {
      return true;
    }
    // Exit code 1 with no value means it is already gone.
    return read() === null;
  }
}

function syncPath() {
  const current = read();
  if (current === null) return false;
  if (current !== launchCommand()) return enable();
  return true;
}

module.exports = {
  APP_RUN_NAME,
  RUN_KEY,
  supported,
  launchCommand,
  read,
  isEnabled,
  enable,
  disable,
  syncPath,
};
