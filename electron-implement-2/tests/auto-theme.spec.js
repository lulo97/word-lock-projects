const path = require('node:path');
const { test, expect, _electron: electron } = require('@playwright/test');

const registry = require('../src/themes/registry.json');

const projectDir = path.join(__dirname, '..');
const settingsPath = path.join(projectDir, 'src', 'settings', 'settings.js');
const windowPath = path.join(projectDir, 'src', 'main', 'createWindow.js');

function nextThemeId(id) {
  const index = registry.findIndex((theme) => theme.id === id);
  return registry[(index + 1) % registry.length].id;
}

async function launchApp() {
  return electron.launch({
    executablePath: require('electron'),
    args: [path.join(projectDir, 'src', 'main.js')],
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

async function showMainWindow(app) {
  await app.evaluate((_electron, modulePath) => {
    process.mainModule.require(modulePath).showMainWindow();
  }, windowPath);
}

async function finishTask(app) {
  await showMainWindow(app);

  await expect
    .poll(() => app.windows().some((window) => window.url().endsWith('english.html')))
    .toBe(true);
  const taskPage = app.windows().find((window) => window.url().endsWith('english.html'));
  await expect(taskPage.locator('#word')).toHaveText('apple');
  await taskPage.locator('#answer').fill('apple');

  await expect
    .poll(() => app.windows().some((window) => window.url().includes('englishInfo.html')))
    .toBe(true);
  const infoPage = app.windows().find((window) => window.url().includes('englishInfo.html'));
  // The URL can change before englishInfo.js attaches its Enter listener.
  await expect(infoPage.locator('#word')).toHaveText('apple');
  await expect(infoPage.locator('#examples .ui-example')).toHaveCount(3);
  await infoPage.keyboard.press('Enter');

  await expect
    .poll(() => app.windows().some((window) => window.url().endsWith('countdown.html')))
    .toBe(true);
  return app.windows().find((window) => window.url().endsWith('countdown.html'));
}

async function countdownTheme(countdownPage) {
  return countdownPage.evaluate(() => document.documentElement.dataset.theme);
}

test('advances the skin round robin when a task finishes and auto-change is on', async () => {
  const app = await launchApp();

  try {
    await setSettings(app, {
      englishWord: true,
      multipleChoice: false,
      japaneseWord: false,
      dumbbellWorkout: false,
      autoTheme: true,
      theme: 'paper',
    });

    const firstExpected = nextThemeId('paper');
    const firstCountdown = await finishTask(app);

    expect((await getSettings(app)).theme).toBe(firstExpected);
    await expect.poll(() => countdownTheme(firstCountdown)).toBe(firstExpected);

    const secondExpected = nextThemeId(firstExpected);
    const secondCountdown = await finishTask(app);

    expect((await getSettings(app)).theme).toBe(secondExpected);
    await expect.poll(() => countdownTheme(secondCountdown)).toBe(secondExpected);
  } finally {
    await closeApp(app);
  }
});

test('keeps the skin when a task finishes and auto-change is off', async () => {
  const app = await launchApp();

  try {
    await setSettings(app, {
      englishWord: true,
      multipleChoice: false,
      japaneseWord: false,
      dumbbellWorkout: false,
      autoTheme: false,
      theme: 'paper',
    });

    const countdown = await finishTask(app);

    expect((await getSettings(app)).theme).toBe('paper');
    await expect.poll(() => countdownTheme(countdown)).toBe('paper');
  } finally {
    await closeApp(app);
  }
});
