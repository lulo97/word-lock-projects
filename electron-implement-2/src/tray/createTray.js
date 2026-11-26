const { app, Menu, Tray } = require('electron');
const { icon } = require('../appIcon');

let tray = null;

function createTray(mainWindow, openApp, closeApp, openSettings) {
  tray = new Tray(icon);
  tray.setToolTip('PomodoroLock');

  const contextMenu = Menu.buildFromTemplate([
    {
      label: 'Show App',
      click: openApp,
    },
    { type: 'separator' },
    {
      label: 'Settings',
      click: openSettings,
    },
    { type: 'separator' },
    {
      label: 'Quit',
      click: () => {
        app.isQuitting = true;
        app.quit();
      },
    },
  ]);

  tray.setContextMenu(contextMenu);

  // Restore window on left-click or double-click in Windows 11
  tray.on('click', () => {
    if (mainWindow) {
      mainWindow.isVisible() ? closeApp() : openApp();
    }
  });
}

module.exports = { createTray };
