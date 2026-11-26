const { contextBridge, ipcRenderer } = require('electron');

contextBridge.exposeInMainWorld('electronAPI', {
  getRandomWord: () => ipcRenderer.invoke('word:getRandom'),
  getWord: (id) => ipcRenderer.invoke('word:get', id),
  getRandomMcq: () => ipcRenderer.invoke('mcq:getRandom'),
  getMcq: (id) => ipcRenderer.invoke('mcq:get', id),
  getSettings: () => ipcRenderer.invoke('settings:get'),
  setSettings: (values) => ipcRenderer.invoke('settings:set', values),
  countdownFinished: () => ipcRenderer.send('countdown:finished'),
  exitInfo: () => ipcRenderer.send('app:exitInfo'),
});
