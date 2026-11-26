const path = require('node:path');
const { test, expect, _electron: electron } = require('@playwright/test');

test('launches the app and renders the countdown window', async () => {
  const app = await electron.launch({
    executablePath: require('electron'),
    args: [path.join(__dirname, '..', 'main.js')],
  });

  try {
    await expect
      .poll(() => app.windows().some((window) => window.url().endsWith('countdown.html')))
      .toBe(true);

    const countdownWindow = app.windows().find((window) => window.url().endsWith('countdown.html'));
    await expect(countdownWindow.locator('#countdown')).toHaveText(/^\d+$/);
  } finally {
    // The app keeps running in the tray, so let its close handlers allow shutdown.
    await app.evaluate(({ app }) => {
      app.isQuitting = true;
    });
    await app.close();
  }
});
