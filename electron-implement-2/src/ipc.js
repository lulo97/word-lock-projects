const { ipcMain } = require('electron');

const {
  getEnglishWord,
  getRandomWord,
  getMcq,
  getRandomMcq,
} = require('./database/database');
const { showMainWindow, hideMainWindow } = require('./main/createWindow');
const { getSettings, setSettings } = require('./settings/settings');

function registerIpcHandlers() {
  ipcMain.handle('word:getRandom', () => getRandomWord());
  ipcMain.handle('word:get', (_event, id) => getEnglishWord(id));
  ipcMain.handle('mcq:getRandom', () => getRandomMcq());
  ipcMain.handle('mcq:get', (_event, id) => getMcq(id));
  ipcMain.handle('settings:get', () => getSettings());
  ipcMain.handle('settings:set', (_event, values) => setSettings(values));
  ipcMain.on('countdown:finished', () => showMainWindow());
  ipcMain.on('app:exitInfo', () => hideMainWindow());
}

module.exports = { registerIpcHandlers };
