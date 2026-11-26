const path = require('node:path');
const { test, expect, _electron: electron } = require('@playwright/test');

const projectDir = path.join(__dirname, '..');

async function launchApp() {
  return electron.launch({
    executablePath: require('electron'),
    args: [path.join(projectDir, 'main.js')],
  });
}

async function closeApp(app) {
  // The app keeps running in the tray, so let its close handlers allow shutdown.
  await app.evaluate(({ app }) => {
    app.isQuitting = true;
  });
  await app.close();
}

async function callMainWindow(app, method) {
  await app.evaluate(
    (_electron, { modulePath, name }) => {
      process.mainModule.require(modulePath)[name]();
    },
    { modulePath: path.join(projectDir, 'src', 'createWindow.js'), name: method },
  );
}

test('launches the app and renders the countdown window', async () => {
  const app = await launchApp();

  try {
    await expect
      .poll(() => app.windows().some((window) => window.url().endsWith('countdown.html')))
      .toBe(true);

    const countdownWindow = app.windows().find((window) => window.url().endsWith('countdown.html'));
    await expect(countdownWindow.locator('#countdown')).toHaveText(/^\d+$/);
  } finally {
    await closeApp(app);
  }
});

test('closes the countdown window for the maximized main window and restores it on close', async () => {
  const app = await launchApp();

  try {
    await expect
      .poll(() => app.windows().some((window) => window.url().endsWith('countdown.html')))
      .toBe(true);

    await callMainWindow(app, 'showMainWindow');

    await expect
      .poll(() => app.windows().some((window) => window.url().endsWith('countdown.html')))
      .toBe(false);

    await expect
      .poll(() =>
        app.evaluate(({ BrowserWindow }) => {
          const win = BrowserWindow.getAllWindows().find((w) => w.getURL().endsWith('index.html'));
          return {
            visible: win.isVisible(),
            maximized: win.isMaximized(),
            fullScreen: win.isFullScreen(),
          };
        }),
      )
      .toEqual({ visible: true, maximized: true, fullScreen: false });

    await callMainWindow(app, 'hideMainWindow');

    await expect
      .poll(() => app.windows().some((window) => window.url().endsWith('countdown.html')))
      .toBe(true);

    const countdownWindow = app.windows().find((window) => window.url().endsWith('countdown.html'));
    await expect(countdownWindow.locator('#countdown')).toHaveText(/^(60|59)$/);
  } finally {
    await closeApp(app);
  }
});
