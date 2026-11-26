const path = require('node:path');
const { test, expect, _electron: electron } = require('@playwright/test');

const projectDir = path.join(__dirname, '..');
const settingsPath = path.join(projectDir, 'src', 'settings', 'settings.js');
const settingsWindowPath = path.join(projectDir, 'src', 'settings', 'createSettingsWindow.js');
const managerPath = path.join(projectDir, 'src', 'autoStart', 'autoStartManager.js');
const mockPath = path.join(projectDir, 'src', 'autoStart', 'autoStart.mock.js');

async function launchApp() {
  return electron.launch({
    executablePath: require('electron'),
    args: [path.join(projectDir, 'src', 'main.js')],
    env: {
      ...process.env,
      WORDLOCK_CHROME_FREEZE_MOCK: '1',
      WORDLOCK_USE_MOCK_AUTO_START: '1',
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

async function setSettings(app, next) {
  await app.evaluate(
    (_electron, { modulePath, values }) => {
      process.mainModule.require(modulePath).setSettings(values);
    },
    { modulePath: settingsPath, values: next },
  );
}

async function mock(app, method, arg) {
  return app.evaluate(
    (_electron, { modulePath, method, arg }) => {
      const module = process.mainModule.require(modulePath);
      return module[method](arg);
    },
    { modulePath: mockPath, method, arg },
  );
}

async function callManager(app, method) {
  return app.evaluate((_electron, { modulePath, method }) => {
    return process.mainModule.require(modulePath)[method]();
  }, { modulePath: managerPath, method });
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

test('auto-start is off by default and syncs on launch', async () => {
  const app = await launchApp();

  try {
    expect((await getSettings(app)).autoStart).toBe(false);
    expect(await mock(app, 'getCalls')).toEqual(['sync:false']);
    expect(await mock(app, 'getState')).toEqual({
      openAtLogin: false,
      executableWillLaunchAtLogin: false,
    });
  } finally {
    await closeApp(app);
  }
});

test('the Start with Windows checkbox enables and disables auto-start', async () => {
  const app = await launchApp();
  let original = null;

  try {
    original = await getSettings(app);
    await mock(app, 'reset');

    let settingsPage = await openSettings(app);
    await expect(settingsPage.locator('#autoStart')).toBeVisible();
    await expect(settingsPage.locator('#autoStart')).not.toBeChecked();

    await settingsPage.locator('#autoStart').check();
    await settingsPage.getByRole('button', { name: 'Confirm' }).click();

    await expect.poll(async () => (await getSettings(app)).autoStart).toBe(true);
    expect(await mock(app, 'getCalls')).toContain('set:true');
    expect((await mock(app, 'getState')).openAtLogin).toBe(true);

    settingsPage = await openSettings(app);
    await expect(settingsPage.locator('#autoStart')).toBeChecked();

    await settingsPage.locator('#autoStart').uncheck();
    await settingsPage.getByRole('button', { name: 'Confirm' }).click();

    await expect.poll(async () => (await getSettings(app)).autoStart).toBe(false);
    expect(await mock(app, 'getCalls')).toContain('set:false');
    expect((await mock(app, 'getState')).openAtLogin).toBe(false);
  } finally {
    if (original) await setSettings(app, original);
    await closeApp(app);
  }
});

test('launching again re-writes the login item with the current exe', async () => {
  let app = await launchApp();
  let original = null;

  try {
    original = await getSettings(app);
    await setSettings(app, { ...original, autoStart: true });
  } finally {
    await closeApp(app);
  }

  app = await launchApp();
  try {
    expect(await mock(app, 'getCalls')).toEqual(['sync:true', 'set:true']);
    expect((await mock(app, 'getState')).openAtLogin).toBe(true);
  } finally {
    await setSettings(app, { ...original, autoStart: false });
    await closeApp(app);
  }
});

test('a login item disabled by Windows is left alone', async () => {
  const app = await launchApp();
  let original = null;

  try {
    original = await getSettings(app);
    await mock(app, 'reset');
    await setSettings(app, { ...original, autoStart: true });
    await mock(app, 'setState', { openAtLogin: true });
    await mock(app, 'setDisabledByWindows', true);

    await callManager(app, 'initializeAutoStart');

    const calls = await mock(app, 'getCalls');
    expect(calls).toContain('sync:true');
    expect(calls).not.toContain('set:true');
    expect((await mock(app, 'getState')).openAtLogin).toBe(true);
  } finally {
    if (original) await setSettings(app, { ...original, autoStart: false });
    await closeApp(app);
  }
});
