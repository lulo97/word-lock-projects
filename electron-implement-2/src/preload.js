const { contextBridge, ipcRenderer } = require('electron');

contextBridge.exposeInMainWorld('electronAPI', {
  getRandomWord: () => ipcRenderer.invoke('word:getRandom'),
  getWord: (id) => ipcRenderer.invoke('word:get', id),
  exitInfo: () => ipcRenderer.send('app:exitInfo'),
});
