const path = require('node:path');
const { app, Menu, Tray, nativeImage } = require('electron');

let tray = null;

function createTray(mainWindow, openApp, closeApp) {
  const iconPath = path.join(__dirname, '..', 'assets', 'icon.ico');
  const icon = nativeImage.createFromPath(iconPath);

  tray = new Tray(icon);
  tray.setToolTip('My Electron App');

  const contextMenu = Menu.buildFromTemplate([
    {
      label: 'Show App',
      click: openApp,
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
