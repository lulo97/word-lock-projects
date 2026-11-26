const { BrowserWindow } = require('electron');

// Tell every open window (main, countdown, settings...) to switch skin now.
function broadcastTheme(id) {
  for (const win of BrowserWindow.getAllWindows()) {
    if (!win.isDestroyed()) win.webContents.send('theme:changed', id);
  }
}

module.exports = { broadcastTheme };
