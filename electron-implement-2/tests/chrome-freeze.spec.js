const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const { test, expect, _electron: electron } = require('@playwright/test');

const projectDir = path.join(__dirname, '..');
const settingsPath = path.join(projectDir, 'src', 'settings', 'settings.js');
const settingsWindowPath = path.join(projectDir, 'src', 'settings', 'createSettingsWindow.js');
const mockPath = path.join(projectDir, 'src', 'chromeFreeze', 'chromeFreeze.mock.js');

function makeLogPath() {
  return path.join(os.tmpdir(), `wordlock-chrome-freeze-${process.pid}-${Date.now()}.log`);
}

function readLog(logPath) {
  if (!fs.existsSync(logPath)) return [];
  return fs.readFileSync(logPath, 'utf8').trim().split(/\r?\n/).filter(Boolean);
}

async function launchApp(logPath) {
  return electron.launch({
    executablePath: require('electron'),
    args: [path.join(projectDir, 'src', 'main.js')],
    env: {
      ...process.env,
      WORDLOCK_CHROME_FREEZE_MOCK: '1',
      WORDLOCK_CHROME_FREEZE_LOG: logPath,
    },
  });
}

async function closeApp(app) {
  await app.evaluate(({ app }) => {
    app.isQuitting = true;
  });
  await app.close();
}

async function getSettings(app) {
  return app.evaluate((_electron, modulePath) => {
    return process.mainModule.require(modulePath).getSettings();
  }, settingsPath);
}

async function restoreSettings(app, values) {
  await app.evaluate(
    (_electron, { modulePath, next }) => {
      process.mainModule.require(modulePath).setSettings(next);
    },
    { modulePath: settingsPath, next: values },
  );
}

async function mockCalls(app) {
  return app.evaluate((_electron, modulePath) => {
    return process.mainModule.require(modulePath).getCalls();
  }, mockPath);
}

async function mockFrozen(app) {
  return app.evaluate((_electron, modulePath) => {
    return process.mainModule.require(modulePath).isFrozen();
  }, mockPath);
}

async function openSettings(app) {
  await app.evaluate((_electron, modulePath) => {
    process.mainModule.require(modulePath).createSettingsWindow();
  }, settingsWindowPath);

  await expect
    .poll(() => app.windows().some((window) => window.url().endsWith('settings.html')))
    .toBe(true);

  return app.windows().find((window) => window.url().endsWith('settings.html'));
}

test('first launch unfreezes Chrome as a cleanup check', async () => {
  const logPath = makeLogPath();
  const app = await launchApp(logPath);

  try {
    await expect.poll(() => readLog(logPath)).toEqual(['unfreezeChrome']);
    expect(await mockCalls(app)).toEqual(['unfreezeChrome']);
    expect(await mockFrozen(app)).toBe(false);
    expect((await getSettings(app)).chromeFreeze).toBe(false);
  } finally {
    await closeApp(app);
    fs.rmSync(logPath, { force: true });
  }
});

test('the settings checkbox freezes and unfreezes Chrome', async () => {
  const logPath = makeLogPath();
  const app = await launchApp(logPath);
  let original = null;

  try {
    original = await getSettings(app);

    let settingsPage = await openSettings(app);
    await expect(settingsPage.locator('#chromeFreeze')).toBeVisible();
    await expect(settingsPage.locator('#chromeFreeze')).not.toBeChecked();

    await settingsPage.locator('#chromeFreeze').check();
    await settingsPage.getByRole('button', { name: 'Confirm' }).click();

    await expect.poll(async () => (await getSettings(app)).chromeFreeze).toBe(true);
    expect(await mockCalls(app)).toContain('freezeChrome');
    expect(await mockFrozen(app)).toBe(true);

    settingsPage = await openSettings(app);
    await expect(settingsPage.locator('#chromeFreeze')).toBeChecked();

    await settingsPage.locator('#chromeFreeze').uncheck();
    await settingsPage.getByRole('button', { name: 'Confirm' }).click();

    await expect.poll(async () => (await getSettings(app)).chromeFreeze).toBe(false);
    expect(await mockCalls(app)).toContain('unfreezeChrome');
    expect(await mockFrozen(app)).toBe(false);
  } finally {
    if (original) await restoreSettings(app, original);
    await closeApp(app);
    fs.rmSync(logPath, { force: true });
  }
});

test('exiting the app unfreezes Chrome again', async () => {
  const logPath = makeLogPath();
  const app = await launchApp(logPath);

  try {
    await expect.poll(() => readLog(logPath)).toEqual(['unfreezeChrome']);
  } finally {
    await closeApp(app);
  }

  expect(readLog(logPath)).toEqual(['unfreezeChrome', 'unfreezeChrome']);
  fs.rmSync(logPath, { force: true });
});
