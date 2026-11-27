const { app, session, protocol, net } = require('electron');
const path = require('node:path');
const { pathToFileURL } = require('node:url');

const { iconPath } = require('./appIcon');

app.setName('PomodoroLock');

// Group all windows under the same taskbar icon instead of the generic Electron one.
app.setAppUserModelId('com.pomodorolock.app');

const { createWindow, showMainWindow, hideMainWindow } = require('./main/createWindow');
const { createCountdownWindow } = require('./countdown/createCountdownWindow');
const { createTray } = require('./tray/createTray');
const { createSettingsWindow } = require('./settings/createSettingsWindow');
const {
  initializeChromeFreeze,
  unfreezeChromeOnExit,
} = require('./chromeFreeze/chromeFreezeManager');
const { initializeAutoStart } = require('./autoStart/autoStartManager');

// MediaPipe fetches its .wasm and .task files. file:// blocks fetch(), so the
// dumbbell page is served from app:// instead. Register before app is ready.
protocol.registerSchemesAsPrivileged([
  {
    scheme: 'app',
    privileges: { standard: true, secure: true, supportFetchAPI: true, stream: true },
  },
]);

const appRoot = path.resolve(__dirname, '..');

function registerAppProtocol() {
  protocol.handle('app', (request) => {
    const { pathname } = new URL(request.url);
    const target = path.resolve(appRoot, `.${decodeURIComponent(pathname)}`);

    if (target !== appRoot && !target.startsWith(appRoot + path.sep)) {
      return new Response('Forbidden', { status: 403 });
    }

    return net.fetch(pathToFileURL(target).toString());
  });
}

function startApp() {
  app.whenReady().then(() => {
    registerAppProtocol();

    session.defaultSession.setPermissionRequestHandler((_webContents, permission, callback) => {
      callback(permission === 'media');
    });

    require('./database/database');
    require('./ipc').registerIpcHandlers();

    void initializeChromeFreeze().catch(() => {});
    initializeAutoStart();

    const mainWindow = createWindow();
    mainWindow.hide();

    createCountdownWindow();
    createTray(mainWindow, showMainWindow, hideMainWindow, createSettingsWindow);
  });

  // Bring the running instance forward instead of starting a second copy that
  // would fight over the same frozen Chrome processes.
  app.on('second-instance', () => showMainWindow());

  // Exit and tray Quit must always release Chrome. The guard in the manager
  // makes these repeated calls harmless.
  app.on('before-quit', unfreezeChromeOnExit);
  app.on('will-quit', unfreezeChromeOnExit);
  app.on('quit', unfreezeChromeOnExit);
  process.on('exit', unfreezeChromeOnExit);

  // Prevent app exit when all windows are closed while running in tray
  app.on('window-all-closed', (event) => {
    event.preventDefault();
  });
}

// Tests opt into the in-memory mock, where a single-instance lock would only
// make separate test launches interfere with each other.
const useTestMock = process.env.WORDLOCK_CHROME_FREEZE_MOCK === '1';
if (useTestMock || app.requestSingleInstanceLock()) {
  startApp();
} else {
  unfreezeChromeOnExit();
  app.quit();
}
