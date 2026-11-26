const { app, BrowserWindow } = require('electron');
const path = require('node:path');

const { createCountdownWindow, closeCountdownWindow } = require('../countdown/createCountdownWindow');
const { getNextTaskType } = require('../settings/settings');

const taskFiles = {
  english: path.join(__dirname, '..', 'english', 'english.html'),
  mcq: path.join(__dirname, '..', 'mcq', 'mcq.html'),
  japanese: path.join(__dirname, '..', 'japanese', 'japanese.html'),
};

let mainWindow = null;

function createWindow() {
  mainWindow = new BrowserWindow({
    width: 900,
    height: 600,
    show: false,
    frame: true,
    fullscreenable: false,
    webPreferences: {
      preload: path.join(__dirname, '..', 'preload.js'),
    },
  });

  mainWindow.loadFile(taskFiles.english);

  mainWindow.on('close', (event) => {
    if (!app.isQuitting) {
      event.preventDefault();
      hideMainWindow();
    }
  });

  return mainWindow;
}

function showMainWindow() {
  if (!mainWindow) return;

  closeCountdownWindow();
  mainWindow.loadFile(taskFiles[getNextTaskType()]);
  mainWindow.show();
  mainWindow.maximize();
  mainWindow.focus();
}

function hideMainWindow() {
  if (!mainWindow) return;

  if (!mainWindow.isDestroyed()) {
    mainWindow.webContents.send('app:hide');
  }

  mainWindow.hide();

  if (!app.isQuitting) {
    createCountdownWindow();
  }
}

module.exports = { createWindow, showMainWindow, hideMainWindow };