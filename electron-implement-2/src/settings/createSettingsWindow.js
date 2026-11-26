const { BrowserWindow } = require('electron');
const path = require('node:path');

let settingsWindow = null;

function createSettingsWindow() {
  if (settingsWindow && !settingsWindow.isDestroyed()) {
    settingsWindow.focus();
    return settingsWindow;
  }

  settingsWindow = new BrowserWindow({
    width: 420,
    height: 260,
    resizable: false,
    webPreferences: {
      preload: path.join(__dirname, '..', 'preload.js'),
    },
  });

  settingsWindow.setTitle('Settings');
  settingsWindow.loadFile(path.join(__dirname, 'settings.html'));

  settingsWindow.on('closed', () => {
    settingsWindow = null;
  });

  return settingsWindow;
}

module.exports = { createSettingsWindow };
