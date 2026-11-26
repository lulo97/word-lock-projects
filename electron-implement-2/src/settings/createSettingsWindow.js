const { BrowserWindow } = require('electron');
const path = require('node:path');
const { broadcastTheme } = require('../themes/broadcast');
const { getSettings } = require('./settings');

let settingsWindow = null;

function createSettingsWindow() {
  if (settingsWindow && !settingsWindow.isDestroyed()) {
    settingsWindow.focus();
    return settingsWindow;
  }

  settingsWindow = new BrowserWindow({
    width: 460,
    height: 720,
    resizable: false,
    webPreferences: {
      preload: path.join(__dirname, '..', 'preload.js'),
    },
  });

  settingsWindow.setTitle('Settings');
  settingsWindow.loadFile(path.join(__dirname, 'settings.html'));

  settingsWindow.on('closed', () => {
    settingsWindow = null;
    broadcastTheme(getSettings().theme); // back to the saved skin (no-op after Confirm)
  });

  return settingsWindow;
}

function closeSettingsWindow() {
  if (settingsWindow && !settingsWindow.isDestroyed()) {
    settingsWindow.close();
  }
}

module.exports = { createSettingsWindow, closeSettingsWindow };
