const { test, expect } = require('@playwright/test');
const { launchApp, closeApp, waitForWindow } = require('./helpers');

async function openSettings(app) {
  await app.call('windows.createSettingsWindow');
  const settingsPage = await waitForWindow(app, 'settings.html');
  await expect(settingsPage.locator('body')).toHaveAttribute('data-settings-loaded', 'true');
  return settingsPage;
}

test('auto-start is off by default and syncs on launch', async () => {
  const app = await launchApp();

  try {
    expect((await app.call('settings.getSettings')).autoStart).toBe(false);
    expect(await app.call('nativeMock.autostart.getCalls')).toEqual(['sync:false']);
    expect(await app.call('nativeMock.autostart.getState')).toEqual({
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
    original = await app.call('settings.getSettings');
    await app.call('nativeMock.autostart.reset');

    let settingsPage = await openSettings(app);
    await expect(settingsPage.locator('#autoStart')).toBeVisible();
    await expect(settingsPage.locator('#autoStart')).not.toBeChecked();

    await settingsPage.locator('#autoStart').check();
    await settingsPage.getByRole('button', { name: 'Confirm' }).click();

    await expect.poll(async () => (await app.call('settings.getSettings')).autoStart).toBe(true);
    expect(await app.call('nativeMock.autostart.getCalls')).toContain('set:true');
    expect((await app.call('nativeMock.autostart.getState')).openAtLogin).toBe(true);

    settingsPage = await openSettings(app);
    await expect(settingsPage.locator('#autoStart')).toBeChecked();

    await settingsPage.locator('#autoStart').uncheck();
    await settingsPage.getByRole('button', { name: 'Confirm' }).click();

    await expect.poll(async () => (await app.call('settings.getSettings')).autoStart).toBe(false);
    expect(await app.call('nativeMock.autostart.getCalls')).toContain('set:false');
    expect((await app.call('nativeMock.autostart.getState')).openAtLogin).toBe(false);
  } finally {
    if (original) await app.call('settings.setSettings', original);
    await closeApp(app);
  }
});

test('launch-time sync re-writes the login item when auto-start is enabled', async () => {
  const app = await launchApp();
  let original = null;

  try {
    original = await app.call('settings.getSettings');
    await app.call('nativeMock.autostart.reset');
    await app.call('settings.setSettings', { ...original, autoStart: true });

    // Simulate the startup sync that a real relaunch would perform. Settings
    // are in memory, so no cross-process persistence is involved.
    await app.call('initializeAutoStart');

    expect(await app.call('nativeMock.autostart.getCalls')).toEqual(['sync:true', 'set:true']);
    expect((await app.call('nativeMock.autostart.getState')).openAtLogin).toBe(true);
  } finally {
    if (original) await app.call('settings.setSettings', { ...original, autoStart: false });
    await closeApp(app);
  }
});

test('a login item disabled by Windows is left alone', async () => {
  const app = await launchApp();
  let original = null;

  try {
    original = await app.call('settings.getSettings');
    await app.call('nativeMock.autostart.reset');
    await app.call('settings.setSettings', { ...original, autoStart: true });
    await app.call('nativeMock.autostart.setState', { openAtLogin: true });
    await app.call('nativeMock.autostart.setDisabledByWindows', true);

    await app.call('initializeAutoStart');

    const calls = await app.call('nativeMock.autostart.getCalls');
    expect(calls).toContain('sync:true');
    expect(calls).not.toContain('set:true');
    expect((await app.call('nativeMock.autostart.getState')).openAtLogin).toBe(true);
  } finally {
    if (original) await app.call('settings.setSettings', { ...original, autoStart: false });
    await closeApp(app);
  }
});
