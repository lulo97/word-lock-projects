const { BrowserWindow } = require('electron');
const path = require('node:path');

let ignoreWordsWindow = null;

function createIgnoreWordsWindow() {
  if (ignoreWordsWindow && !ignoreWordsWindow.isDestroyed()) {
    ignoreWordsWindow.focus();
    return ignoreWordsWindow;
  }

  ignoreWordsWindow = new BrowserWindow({
    width: 560,
    height: 460,
    resizable: false,
    webPreferences: {
      preload: path.join(__dirname, '..', 'preload.js'),
    },
  });

  ignoreWordsWindow.setTitle('Ignore Words');
  ignoreWordsWindow.loadFile(path.join(__dirname, 'ignoreWords.html'));

  ignoreWordsWindow.on('closed', () => {
    ignoreWordsWindow = null;
  });

  return ignoreWordsWindow;
}

function closeIgnoreWordsWindow() {
  if (ignoreWordsWindow && !ignoreWordsWindow.isDestroyed()) {
    ignoreWordsWindow.close();
  }
}

module.exports = { createIgnoreWordsWindow, closeIgnoreWordsWindow };
