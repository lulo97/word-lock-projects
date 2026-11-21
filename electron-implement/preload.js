const { contextBridge, ipcRenderer } = require("electron");

contextBridge.exposeInMainWorld("wordlock", {
  // Bootstrap / data
  bootstrap: () => ipcRenderer.invoke("app:bootstrap"),
  recordSuccess: (word) => ipcRenderer.invoke("progress:record", word),
  ignoreWord: (word) => ipcRenderer.invoke("settings:ignoreWord", word),
  readAudio: (filePath) => ipcRenderer.invoke("audio:read", filePath),
  getVolume: () => ipcRenderer.invoke("audio:volume"),

  // Window actions
  hide: () => ipcRenderer.send("ui:hide"),
  exitCard: () => ipcRenderer.send("ui:exitCard"),

  // Settings window
  settingsInit: () => ipcRenderer.invoke("settings:init"),
  saveSettings: (payload) => ipcRenderer.invoke("settings:save", payload),
  closeSettings: () => ipcRenderer.send("settings:close"),

  // Main renderer events
  onStyle: (callback) =>
    ipcRenderer.on("ui:style", (event, style) => callback(style)),
  onQuestion: (callback) =>
    ipcRenderer.on("ui:question", (event, entry) => callback(entry)),
  onFocusAnswer: (callback) =>
    ipcRenderer.on("ui:focusAnswer", () => callback()),
  onStopAudio: (callback) =>
    ipcRenderer.on("ui:stopAudio", () => callback()),
  onCancelFocus: (callback) =>
    ipcRenderer.on("ui:cancelFocus", () => callback()),

  // Countdown overlay
  onOverlaySet: (callback) =>
    ipcRenderer.on("overlay:set", (event, payload) => callback(payload)),
});
