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
const { getSettings, setSettings, getIgnoreWords, setIgnoreWords, IGNORE_ERROR } = require('./settings/settings');
const { closeSettingsWindow } = require('./settings/createSettingsWindow');
const { closeCountdownWindow } = require('./countdown/createCountdownWindow');
const {
  createIgnoreWordsWindow,
  closeIgnoreWordsWindow,
} = require('./settings/createIgnoreWordsWindow');
const { synthesize, TTS_UNAVAILABLE_MESSAGE } = require('./tts/tts');
const { syncChromeFreeze } = require('./chromeFreeze/chromeFreezeManager');
const { applyAutoStartPreference } = require('./autoStart/autoStartManager');

function registerIpcHandlers() {
  ipcMain.handle('word:getRandom', () => getRandomWord());
  ipcMain.handle('word:get', (_event, id) => getEnglishWord(id));
  ipcMain.handle('mcq:getRandom', () => getRandomMcq());
  ipcMain.handle('mcq:get', (_event, id) => getMcq(id));
  ipcMain.handle('japanese:getRandom', () => getRandomJapaneseWord());
  ipcMain.handle('japanese:get', (_event, id) => getJapaneseWord(id));
  ipcMain.handle('settings:get', () => getSettings());
  ipcMain.handle('settings:set', (_event, values) => {
    setSettings(values);
    syncChromeFreeze();
    if (values.autoStart !== undefined) applyAutoStartPreference(values.autoStart);
  });
  ipcMain.handle('settings:confirm', (_event, values) => {
    setSettings(values);
    syncChromeFreeze();
    if (values.autoStart !== undefined) applyAutoStartPreference(values.autoStart);
    closeCountdownWindow();
    hideMainWindow();
    closeSettingsWindow();
  });
  ipcMain.on('settings:close', () => closeSettingsWindow());
  ipcMain.handle('ignoreWords:open', () => createIgnoreWordsWindow());
  ipcMain.handle('ignoreWords:get', () => getIgnoreWords());
  ipcMain.handle('ignoreWords:save', (_event, values) => {
    try {
      return { ok: true, words: setIgnoreWords(values) };
    } catch {
      return { ok: false, error: IGNORE_ERROR };
    }
  });
  ipcMain.on('ignoreWords:close', () => closeIgnoreWordsWindow());
  ipcMain.on('countdown:finished', () => showMainWindow());
  ipcMain.on('app:exitInfo', () => hideMainWindow());
  ipcMain.handle('tts:speak', async (_event, text, lang) => {
    try {
      return { ok: true, wav: await synthesize(text, lang) };
    } catch (error) {
      return {
        ok: false,
        error: error?.message || TTS_UNAVAILABLE_MESSAGE,
      };
    }
  });
}

module.exports = { registerIpcHandlers };
