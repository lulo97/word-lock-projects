const path = require('node:path');
const { test, expect, _electron: electron } = require('@playwright/test');

const projectDir = path.join(__dirname, '..');
const settingsPath = path.join(projectDir, 'src', 'settings', 'settings.js');

async function launchApp() {
  return electron.launch({
    executablePath: require('electron'),
    args: [path.join(projectDir, 'src', 'main.js')],
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
    { modulePath: path.join(projectDir, 'src', 'main', 'createWindow.js'), name: method },
  );
}

async function setTasks(app, values) {
  await app.evaluate(
    (_electron, { modulePath, next }) => {
      process.mainModule.require(modulePath).setSettings(next);
    },
    {
      modulePath: path.join(projectDir, 'src', 'settings', 'settings.js'),
      next: values,
    },
  );
}

async function getSettings(app) {
  return app.evaluate((_electron, modulePath) => {
    return process.mainModule.require(modulePath).getSettings();
  }, path.join(projectDir, 'src', 'settings', 'settings.js'));
}

async function callSettingsWindow(app) {
  await app.evaluate((_electron, modulePath) => {
    process.mainModule.require(modulePath).createSettingsWindow();
  }, path.join(projectDir, 'src', 'settings', 'createSettingsWindow.js'));
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

    await setTasks(app, { englishWord: true, multipleChoice: false, japaneseWord: false, dumbbellWorkout: false });
    await callMainWindow(app, 'showMainWindow');

    await expect
      .poll(() => app.windows().some((window) => window.url().endsWith('countdown.html')))
      .toBe(false);

    await expect
      .poll(() =>
        app.evaluate(({ BrowserWindow }) => {
          const win = BrowserWindow.getAllWindows().find((w) => w.getURL().endsWith('english.html'));
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

test('checks the word, opens InfoScreen, and Enter restarts the countdown', async () => {
  const app = await launchApp();

  try {
    await setTasks(app, { englishWord: true, multipleChoice: false, japaneseWord: false, dumbbellWorkout: false });
    await callMainWindow(app, 'showMainWindow');

    const wordPage = app.windows().find((window) => window.url().endsWith('english.html'));
    await expect(wordPage.locator('#word')).toHaveText('apple');

    await wordPage.locator('#answer').fill('apple');
    await expect(wordPage.locator('#feedback')).toHaveText('Correct');
    await expect(wordPage.locator('#feedback')).toHaveClass(/text-green-600/);

    await expect
      .poll(() => app.windows().some((window) => window.url().includes('englishInfo.html')))
      .toBe(true);

    const infoPage = app.windows().find((window) => window.url().includes('englishInfo.html'));
    await expect(infoPage.locator('#word')).toHaveText('apple');
    await expect(infoPage.locator('#meaning_vi')).toHaveText('quả táo');
    await expect(infoPage.locator('#examples')).toContainText('an apple a day keep the doctor away');
    await expect(infoPage.locator('#examples_vi')).toContainText('anh ấy có một quả táo');
    await expect(infoPage.locator('#speak-word')).toBeVisible();
    await expect(infoPage.locator('#examples .speak-button')).toHaveCount(3);
    await expect(infoPage.locator('#examples_vi .speak-button')).toHaveCount(0);
    await expect(infoPage.getByText('Enter to exit')).toBeVisible();

    await infoPage.keyboard.press('Enter');

    await expect
      .poll(() => app.windows().some((window) => window.url().endsWith('countdown.html')))
      .toBe(true);

    const countdownWindow = app.windows().find((window) => window.url().endsWith('countdown.html'));
    await expect(countdownWindow.locator('#countdown')).toHaveText(/^(60|59)$/);
  } finally {
    await closeApp(app);
  }
});

test('shows a multiple choice task, its result screen, and Enter restarts the countdown', async () => {
  const app = await launchApp();

  try {
    await setTasks(app, { englishWord: false, multipleChoice: true, japaneseWord: false, dumbbellWorkout: false });
    await callMainWindow(app, 'showMainWindow');

    await expect
      .poll(() => app.windows().some((window) => window.url().endsWith('mcq.html')))
      .toBe(true);

    const mcqPage = app.windows().find((window) => window.url().endsWith('mcq.html'));
    await expect(mcqPage.locator('#question')).toHaveText('1+1=?');

    await mcqPage.getByRole('button', { name: '2', exact: true }).click();

    await expect
      .poll(() => app.windows().some((window) => window.url().includes('mcqResult.html')))
      .toBe(true);

    const resultPage = app.windows().find((window) => window.url().includes('mcqResult.html'));
    await expect(resultPage.locator('#result')).toHaveText('Correct');
    await expect(resultPage.locator('#details')).toContainText('1+1=2');
    await expect(resultPage.getByText('Enter to exit')).toBeVisible();

    await resultPage.keyboard.press('Enter');

    await expect
      .poll(() => app.windows().some((window) => window.url().endsWith('countdown.html')))
      .toBe(true);

    const countdownWindow = app.windows().find((window) => window.url().endsWith('countdown.html'));
    await expect(countdownWindow.locator('#countdown')).toHaveText(/^(60|59)$/);

    await setTasks(app, { englishWord: true, multipleChoice: true, japaneseWord: true, dumbbellWorkout: true });
  } finally {
    await closeApp(app);
  }
});

test('opens the settings window with all task checkboxes', async () => {
  const app = await launchApp();

  try {
    await callSettingsWindow(app);

    await expect
      .poll(() => app.windows().some((window) => window.url().endsWith('settings.html')))
      .toBe(true);

    const settingsPage = app.windows().find((window) => window.url().endsWith('settings.html'));
    await expect(settingsPage.getByText('Randomly show these tasks')).toBeVisible();
    await expect(settingsPage.locator('#englishWord')).toBeVisible();
    await expect(settingsPage.locator('#multipleChoice')).toBeVisible();
    await expect(settingsPage.locator('#japaneseWord')).toBeVisible();
    await expect(settingsPage.locator('#dumbbellWorkout')).toBeVisible();
    await expect(settingsPage.locator('#masterVolume')).toBeVisible();
    await expect(settingsPage.locator('#masterVolume')).toHaveAttribute('min', '0');
    await expect(settingsPage.locator('#masterVolume')).toHaveAttribute('max', '4');
    await expect(settingsPage.locator('#masterVolume')).toHaveAttribute('step', '0.5');
    await expect(settingsPage.getByRole('button', { name: 'Cancel' })).toBeVisible();
    await expect(settingsPage.getByRole('button', { name: 'Confirm' })).toBeVisible();
  } finally {
    await closeApp(app);
  }
});

test('cycles word, mcq, and japanese tasks with round robin', async () => {
  const app = await launchApp();

  try {
    await setTasks(app, { englishWord: true, multipleChoice: true, japaneseWord: true, dumbbellWorkout: false });

    await callMainWindow(app, 'showMainWindow');
    await expect
      .poll(() => app.windows().some((window) => window.url().endsWith('english.html')))
      .toBe(true);

    await callMainWindow(app, 'hideMainWindow');
    await callMainWindow(app, 'showMainWindow');
    await expect
      .poll(() => app.windows().some((window) => window.url().endsWith('mcq.html')))
      .toBe(true);

    await callMainWindow(app, 'hideMainWindow');
    await callMainWindow(app, 'showMainWindow');
    await expect
      .poll(() => app.windows().some((window) => window.url().endsWith('japanese.html')))
      .toBe(true);

    await callMainWindow(app, 'hideMainWindow');
    await callMainWindow(app, 'showMainWindow');
    await expect
      .poll(() => app.windows().some((window) => window.url().endsWith('english.html')))
      .toBe(true);
  } finally {
    await closeApp(app);
  }
});

test('checks the japanese word, opens its info screen, and Enter restarts the countdown', async () => {
  const app = await launchApp();

  try {
    await setTasks(app, { englishWord: false, multipleChoice: false, japaneseWord: true, dumbbellWorkout: false });
    await callMainWindow(app, 'showMainWindow');

    await expect
      .poll(() => app.windows().some((window) => window.url().endsWith('japanese.html')))
      .toBe(true);

    const wordPage = app.windows().find((window) => window.url().endsWith('japanese.html'));
    await expect(wordPage.locator('#word')).toHaveText('桜');
    await expect(wordPage.locator('#hiragana')).toHaveText('さくら');
    await expect(wordPage.locator('#katakana')).toHaveText('サクラ');
    await expect(wordPage.locator('#romaji')).toHaveText('sakura');

    await wordPage.locator('#answer').fill('sakura');
    await expect(wordPage.locator('#feedback')).toHaveText('Correct');

    await expect
      .poll(() => app.windows().some((window) => window.url().includes('japaneseInfo.html')))
      .toBe(true);

    const infoPage = app.windows().find((window) => window.url().includes('japaneseInfo.html'));
    await expect(infoPage.locator('#kanji')).toHaveText('桜');
    await expect(infoPage.locator('#hiragana')).toHaveText('さくら');
    await expect(infoPage.locator('#katakana')).toHaveText('サクラ');
    await expect(infoPage.locator('#romaji')).toHaveText('sakura');
    await expect(infoPage.locator('#meaning_en')).toHaveText('cherry blossom');
    await expect(infoPage.locator('#examples')).toContainText('桜が咲きました。');
    await expect(infoPage.locator('#examples_en')).toContainText('The cherry blossoms bloomed.');
    await expect(infoPage.locator('#examples_vi')).toContainText('Hoa anh đào đã nở.');
    await expect(infoPage.locator('#speak-kanji')).toBeVisible();
    await expect(infoPage.locator('#speak-hiragana')).toBeVisible();
    await expect(infoPage.locator('#speak-katakana')).toBeVisible();
    await expect(infoPage.locator('#examples .speak-button')).toHaveCount(2);
    await expect(infoPage.locator('#examples_en .speak-button')).toHaveCount(0);
    await expect(infoPage.locator('#examples_vi .speak-button')).toHaveCount(0);
    await expect(infoPage.getByText('Enter to exit')).toBeVisible();

    await infoPage.keyboard.press('Enter');

    await expect
      .poll(() => app.windows().some((window) => window.url().endsWith('countdown.html')))
      .toBe(true);

    const countdownWindow = app.windows().find((window) => window.url().endsWith('countdown.html'));
    await expect(countdownWindow.locator('#countdown')).toHaveText(/^(60|59)$/);

    await setTasks(app, { englishWord: true, multipleChoice: true, japaneseWord: true, dumbbellWorkout: true });
  } finally {
    await closeApp(app);
  }
});

test('confirm applies settings and restarts the countdown', async () => {
  const app = await launchApp();

  try {
    await setTasks(app, { englishWord: true, multipleChoice: true, japaneseWord: true, dumbbellWorkout: true });
    await callSettingsWindow(app);

    await expect
      .poll(() => app.windows().some((window) => window.url().endsWith('settings.html')))
      .toBe(true);

    const settingsPage = app.windows().find((window) => window.url().endsWith('settings.html'));
    await settingsPage.locator('#englishWord').uncheck();
    await settingsPage.getByRole('button', { name: 'Confirm' }).click();

    await expect.poll(async () => (await getSettings(app)).englishWord).toBe(false);
    await expect
      .poll(() => app.windows().some((window) => window.url().endsWith('countdown.html')))
      .toBe(true);

    const countdownWindow = app.windows().find((window) => window.url().endsWith('countdown.html'));
    await expect(countdownWindow.locator('#countdown')).toHaveText(/^(60|59)$/);
    await expect
      .poll(() => app.windows().some((window) => window.url().endsWith('settings.html')))
      .toBe(false);
  } finally {
    await setTasks(app, { englishWord: true, multipleChoice: true, japaneseWord: true, dumbbellWorkout: true });
    await closeApp(app);
  }
});

test('saves master volume from the settings slider', async () => {
  const app = await launchApp();
  let initialVolume = 1;

  try {
    await callSettingsWindow(app);

    await expect
      .poll(() => app.windows().some((window) => window.url().endsWith('settings.html')))
      .toBe(true);

    let settingsPage = app.windows().find((window) => window.url().endsWith('settings.html'));
    initialVolume = Number(await settingsPage.locator('#masterVolume').inputValue());
    const target = initialVolume === 0.5 ? 2.5 : 0.5;

    await settingsPage.locator('#masterVolume').evaluate((element, value) => {
      element.value = value;
      element.dispatchEvent(new Event('input', { bubbles: true }));
    }, String(target));
    await expect(settingsPage.locator('#volumeValue')).toHaveText(`${target}×`);

    await settingsPage.getByRole('button', { name: 'Confirm' }).click();
    await expect.poll(async () => (await getSettings(app)).masterVolume).toBe(target);

    await callSettingsWindow(app);
    await expect
      .poll(async () => {
        const pages = app.windows().filter((window) => window.url().endsWith('settings.html'));
        if (!pages.length) return null;
        return pages[0].locator('#masterVolume').inputValue().catch(() => null);
      })
      .toBe(String(target));
  } finally {
    await app.evaluate(
      (_electron, { modulePath, volume }) => {
        const settings = process.mainModule.require(modulePath);
        settings.setSettings({ ...settings.getSettings(), masterVolume: volume });
      },
      { modulePath: settingsPath, volume: initialVolume },
    );
    await closeApp(app);
  }
});

test('cancel discards settings changes', async () => {
  const app = await launchApp();

  try {
    await setTasks(app, { englishWord: true, multipleChoice: true, japaneseWord: true, dumbbellWorkout: true });
    await callSettingsWindow(app);

    await expect
      .poll(() => app.windows().some((window) => window.url().endsWith('settings.html')))
      .toBe(true);

    const settingsPage = app.windows().find((window) => window.url().endsWith('settings.html'));
    await settingsPage.locator('#englishWord').uncheck();
    await settingsPage.getByRole('button', { name: 'Cancel' }).click();

    await expect.poll(async () => (await getSettings(app)).englishWord).toBe(true);
    await expect
      .poll(() => app.windows().some((window) => window.url().endsWith('settings.html')))
      .toBe(false);
  } finally {
    await closeApp(app);
  }
});
