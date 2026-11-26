const { ipcMain } = require('electron');

const {
  getEnglishWord,
  getRandomWord,
  getMcq,
  getRandomMcq,
  getJapaneseWord,
  getRandomJapaneseWord,
} = require('./database/database');
const { showMainWindow, hideMainWindow } = require('./main/createWindow');
const { getSettings, setSettings } = require('./settings/settings');
const { closeSettingsWindow } = require('./settings/createSettingsWindow');
const { synthesize } = require('./tts/tts');

const useMockVision = process.env.DUMBBELL_VISION === 'mock';
const vision = require(useMockVision ? './dumbbell/vision.mock' : './dumbbell/vision');

function registerIpcHandlers() {
  ipcMain.handle('word:getRandom', () => getRandomWord());
  ipcMain.handle('word:get', (_event, id) => getEnglishWord(id));
  ipcMain.handle('mcq:getRandom', () => getRandomMcq());
  ipcMain.handle('mcq:get', (_event, id) => getMcq(id));
  ipcMain.handle('japanese:getRandom', () => getRandomJapaneseWord());
  ipcMain.handle('japanese:get', (_event, id) => getJapaneseWord(id));
  ipcMain.handle('settings:get', () => getSettings());
  ipcMain.handle('settings:set', (_event, values) => setSettings(values));
  ipcMain.handle('settings:confirm', (_event, values) => {
    setSettings(values);
    hideMainWindow();
    closeSettingsWindow();
  });
  ipcMain.on('settings:close', () => closeSettingsWindow());
  ipcMain.on('countdown:finished', () => showMainWindow());
  ipcMain.on('app:exitInfo', () => hideMainWindow());
  ipcMain.handle('tts:speak', (_event, text, lang) => synthesize(text, lang));
  ipcMain.handle('vision:analyze', (_event, dataUrl) => vision.analyzeFrame(dataUrl));
  ipcMain.handle('vision:status', () => vision.getStatus());
  ipcMain.handle('vision:config', () => ({
    frameIntervalMs: Number(process.env.DUMBBELL_FRAME_MS) || 1000,
    mock: useMockVision,
  }));
}

module.exports = { registerIpcHandlers };
