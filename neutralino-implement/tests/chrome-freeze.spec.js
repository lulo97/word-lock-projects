const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const { test, expect } = require('@playwright/test');
const { launchApp, closeApp, waitForWindow } = require('./helpers');

function makeLogPath() {
  return path.join(os.tmpdir(), `pomodorolock-chrome-freeze-${process.pid}-${Date.now()}.log`);
}

function readLog(logPath) {
  if (!fs.existsSync(logPath)) return [];
  return fs.readFileSync(logPath, 'utf8').trim().split(/\r?\n/).filter(Boolean);
}

async function openSettings(app) {
  await app.call('windows.createSettingsWindow');
  const settingsPage = await waitForWindow(app, 'settings.html');
  await expect(settingsPage.locator('body')).toHaveAttribute('data-settings-loaded', 'true');
  return settingsPage;
}

test('first launch unfreezes Chrome as a cleanup check', async () => {
  const logPath = makeLogPath();
  const app = await launchApp({ env: { WORDLOCK_CHROME_FREEZE_LOG: logPath } });

  try {
    await expect.poll(() => readLog(logPath)).toEqual(['unfreezeChrome']);
    expect(await app.call('nativeMock.chrome.getCalls')).toEqual(['unfreezeChrome']);
    expect(await app.call('nativeMock.chrome.isFrozen')).toBe(false);
    expect((await app.call('settings.getSettings')).chromeFreeze).toBe(false);
  } finally {
    await closeApp(app);
    fs.rmSync(logPath, { force: true });
  }
});

test('the settings checkbox freezes and unfreezes Chrome', async () => {
  const logPath = makeLogPath();
  const app = await launchApp({ env: { WORDLOCK_CHROME_FREEZE_LOG: logPath } });
  let original = null;

  try {
    original = await app.call('settings.getSettings');

    let settingsPage = await openSettings(app);
    await expect(settingsPage.locator('#chromeFreeze')).toBeVisible();
    await expect(settingsPage.locator('#chromeFreeze')).not.toBeChecked();

    await settingsPage.locator('#chromeFreeze').check();
    await settingsPage.getByRole('button', { name: 'Confirm' }).click();

    await expect.poll(async () => (await app.call('settings.getSettings')).chromeFreeze).toBe(true);
    expect(await app.call('nativeMock.chrome.getCalls')).toContain('freezeChrome');
    expect(await app.call('nativeMock.chrome.isFrozen')).toBe(true);

    settingsPage = await openSettings(app);
    await expect(settingsPage.locator('#chromeFreeze')).toBeChecked();

    await settingsPage.locator('#chromeFreeze').uncheck();
    await settingsPage.getByRole('button', { name: 'Confirm' }).click();

    await expect.poll(async () => (await app.call('settings.getSettings')).chromeFreeze).toBe(false);
    expect(await app.call('nativeMock.chrome.getCalls')).toContain('unfreezeChrome');
    expect(await app.call('nativeMock.chrome.isFrozen')).toBe(false);
  } finally {
    if (original) await app.call('settings.setSettings', original);
    await closeApp(app);
    fs.rmSync(logPath, { force: true });
  }
});

test('exiting the app unfreezes Chrome again', async () => {
  const logPath = makeLogPath();
  const app = await launchApp({ env: { WORDLOCK_CHROME_FREEZE_LOG: logPath } });

  try {
    await expect.poll(() => readLog(logPath)).toEqual(['unfreezeChrome']);
  } finally {
    await closeApp(app);
  }

  expect(readLog(logPath)).toEqual(['unfreezeChrome', 'unfreezeChrome']);
  fs.rmSync(logPath, { force: true });
});
