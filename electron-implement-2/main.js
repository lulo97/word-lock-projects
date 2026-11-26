const { app } = require('electron');

const { createWindow, showMainWindow, hideMainWindow } = require('./src/createWindow');
const { createCountdownWindow } = require('./src/createCountdownWindow');
const { createTray } = require('./src/createTray');

app.whenReady().then(() => {
  const mainWindow = createWindow();
  mainWindow.hide();

  createCountdownWindow();
  createTray(mainWindow, showMainWindow, hideMainWindow);
});

// Prevent app exit when all windows are closed while running in tray
app.on('window-all-closed', (event) => {
  event.preventDefault();
});