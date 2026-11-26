const { contextBridge, ipcRenderer } = require('electron');

const themeId = ipcRenderer.sendSync('theme:get');

contextBridge.exposeInMainWorld('electronAPI', {
  themeId,
  listThemes: () => ipcRenderer.invoke('theme:list'),
  previewTheme: (id) => ipcRenderer.send('theme:preview', id),
  onThemeChanged: (callback) => ipcRenderer.on('theme:changed', (_event, id) => callback(id)),
  getRandomWord: () => ipcRenderer.invoke('word:getRandom'),
  getWord: (id) => ipcRenderer.invoke('word:get', id),
  getRandomMcq: () => ipcRenderer.invoke('mcq:getRandom'),
  getMcq: (id) => ipcRenderer.invoke('mcq:get', id),
  getRandomJapaneseWord: () => ipcRenderer.invoke('japanese:getRandom'),
  getJapaneseWord: (id) => ipcRenderer.invoke('japanese:get', id),
  getSettings: () => ipcRenderer.invoke('settings:get'),
  setSettings: (values) => ipcRenderer.invoke('settings:set', values),
  confirmSettings: (values) => ipcRenderer.invoke('settings:confirm', values),
  closeSettings: () => ipcRenderer.send('settings:close'),
  openIgnoreWords: () => ipcRenderer.invoke('ignoreWords:open'),
  getIgnoreWords: () => ipcRenderer.invoke('ignoreWords:get'),
  saveIgnoreWords: (values) => ipcRenderer.invoke('ignoreWords:save', values),
  closeIgnoreWords: () => ipcRenderer.send('ignoreWords:close'),
  countdownFinished: () => ipcRenderer.send('countdown:finished'),
  exitInfo: () => ipcRenderer.send('app:exitInfo'),
  speak: (text, lang) => ipcRenderer.invoke('tts:speak', text, lang),
  onHide: (callback) => ipcRenderer.on('app:hide', callback),
});
