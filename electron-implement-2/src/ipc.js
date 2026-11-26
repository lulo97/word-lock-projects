const { ipcMain } = require('electron');

const { getEnglishWord, getRandomWord } = require('./database');
const { hideMainWindow } = require('./createWindow');

function registerIpcHandlers() {
  ipcMain.handle('word:getRandom', () => getRandomWord());
  ipcMain.handle('word:get', (_event, id) => getEnglishWord(id));
  ipcMain.on('app:exitInfo', () => hideMainWindow());
}

module.exports = { registerIpcHandlers };
