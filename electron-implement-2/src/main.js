const { app, session } = require('electron');

const { createWindow, showMainWindow, hideMainWindow } = require('./main/createWindow');
const { createCountdownWindow } = require('./countdown/createCountdownWindow');
const { createTray } = require('./tray/createTray');
const { createSettingsWindow } = require('./settings/createSettingsWindow');

app.whenReady().then(() => {
  session.defaultSession.setPermissionRequestHandler((_webContents, permission, callback) => {
    callback(permission === 'media');
  });

  require('./database/database');
  require('./ipc').registerIpcHandlers();

  const mainWindow = createWindow();
  mainWindow.hide();

  createCountdownWindow();
  createTray(mainWindow, showMainWindow, hideMainWindow, createSettingsWindow);
});

// Prevent app exit when all windows are closed while running in tray
app.on('window-all-closed', (event) => {
  event.preventDefault();
});