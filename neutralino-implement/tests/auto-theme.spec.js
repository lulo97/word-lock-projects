const { test, expect } = require('@playwright/test');
const registry = require('../resources/src/themes/registry.json');
const { launchApp, closeApp, waitForWindow, windowGone, sleep } = require('./helpers');

function nextThemeId(id) {
  const index = registry.findIndex((theme) => theme.id === id);
  return registry[(index + 1) % registry.length].id;
}

function countdownFrame(app) {
  return app.host().frameLocator('#cd');
}

async function countdownTheme(app) {
  return countdownFrame(app).locator('html').evaluate((el) => el.dataset.theme);
}

async function finishTask(app) {
  await app.call('windows.showMainWindow');

  const taskPage = await waitForWindow(app, 'english.html');
  await expect(taskPage.locator('#word')).toHaveText('apple');
  await taskPage.locator('#answer').fill('apple');

  const infoPage = await waitForWindow(app, 'englishInfo.html');
  // The URL can change before englishInfo.js attaches its Enter listener.
  await expect(infoPage.locator('#word')).toHaveText('apple');
  await expect(infoPage.locator('#examples .ui-example')).toHaveCount(3);
  await infoPage.keyboard.press('Enter');
  await windowGone(app, 'englishInfo.html');
  await sleep(150);
  return app;
}

const TASK_SETTINGS = {
  englishWord: true,
  multipleChoice: false,
  japaneseWord: false,
  dumbbellWorkout: false,
};

test('advances the skin round robin when a task finishes and auto-change is on', async () => {
  const app = await launchApp();

  try {
    await app.call('settings.setSettings', { ...TASK_SETTINGS, autoTheme: true, theme: 'paper' });

    const firstExpected = nextThemeId('paper');
    await finishTask(app);

    expect((await app.call('settings.getSettings')).theme).toBe(firstExpected);
    await expect.poll(() => countdownTheme(app)).toBe(firstExpected);

    const secondExpected = nextThemeId(firstExpected);
    await finishTask(app);

    expect((await app.call('settings.getSettings')).theme).toBe(secondExpected);
    await expect.poll(() => countdownTheme(app)).toBe(secondExpected);
  } finally {
    await closeApp(app);
  }
});

test('keeps the skin when a task finishes and auto-change is off', async () => {
  const app = await launchApp();

  try {
    await app.call('settings.setSettings', { ...TASK_SETTINGS, autoTheme: false, theme: 'paper' });

    await finishTask(app);

    expect((await app.call('settings.getSettings')).theme).toBe('paper');
    await expect.poll(() => countdownTheme(app)).toBe('paper');
  } finally {
    await closeApp(app);
  }
});
