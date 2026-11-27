const { test, expect } = require('@playwright/test');
const {
  launchApp, closeApp, waitForWindow, windowGone,
} = require('./helpers');

const ALL_TASKS = {
  englishWord: true,
  multipleChoice: true,
  japaneseWord: true,
  dumbbellWorkout: true,
};

function countdown(app) {
  return app.host().frameLocator('#cd').locator('#countdown');
}

async function windowState(page) {
  return page.evaluate(async () => ({
    visible: await Neutralino.window.isVisible(),
    maximized: await Neutralino.window.isMaximized(),
    fullScreen: await Neutralino.window.isFullScreen(),
  }));
}

async function waitForSettingsLoaded(page) {
  await expect(page.locator('body')).toHaveAttribute('data-settings-loaded', 'true');
}

async function clickSettingsButton(page, id) {
  // Clicking Cancel/Confirm retires the child window; a normal Playwright
  // click can then report the target closed. Dispatch from inside the page.
  await page.locator(`#${id}`).evaluate((element) => element.click());
}

async function setTasks(app, values) {
  await app.call('settings.setSettings', values);
}

async function getSettings(app) {
  return app.call('settings.getSettings');
}

async function callSettingsWindow(app) {
  await app.call('windows.createSettingsWindow');
}

test('launches the app and renders the countdown window', async () => {
  const app = await launchApp();

  try {
    await expect(countdown(app)).toHaveText(/^\d+$/);
  } finally {
    await closeApp(app);
  }
});

test('closes the countdown window for the maximized main window and restores it on close', async () => {
  const app = await launchApp();

  try {
    await expect(countdown(app)).toHaveText(/^\d+$/);

    await setTasks(app, { englishWord: true, multipleChoice: false, japaneseWord: false, dumbbellWorkout: false });
    await app.call('windows.showMainWindow');

    const taskPage = await waitForWindow(app, 'english.html');
    await expect.poll(() => windowState(taskPage)).toEqual({
      visible: true,
      maximized: true,
      fullScreen: false,
    });

    await app.call('windows.hideMainWindow');
    await windowGone(app, 'english.html');
    await expect(countdown(app)).toHaveText(/^(60|59)$/);
  } finally {
    await closeApp(app);
  }
});

test('checks the word, opens InfoScreen, and Enter restarts the countdown', async () => {
  const app = await launchApp();

  try {
    await setTasks(app, { englishWord: true, multipleChoice: false, japaneseWord: false, dumbbellWorkout: false });
    await app.call('windows.showMainWindow');

    const wordPage = await waitForWindow(app, 'english.html');
    await expect(wordPage.locator('#word')).toHaveText('apple');

    await wordPage.locator('#answer').fill('apple');
    await expect(wordPage.locator('#feedback')).toHaveText('Correct');
    await expect(wordPage.locator('#feedback')).toHaveAttribute('data-state', 'ok');

    const infoPage = await waitForWindow(app, 'englishInfo.html');
    await expect(infoPage.locator('#word')).toHaveText('apple');
    await expect(infoPage.locator('#meaning_vi')).toHaveText('quả táo');
    await expect(infoPage.locator('#examples')).toContainText('an apple a day keep the doctor away');
    await expect(infoPage.locator('#examples')).toContainText('anh ấy có một quả táo');
    await expect(infoPage.locator('#speak-word')).toBeVisible();
    await expect(infoPage.locator('#examples .ui-example')).toHaveCount(3);
    await expect(infoPage.locator('#examples button')).toHaveCount(3);
    await expect(infoPage.locator('#examples_vi')).toHaveCount(0);
    await expect(infoPage.getByText('Enter to exit')).toBeVisible();

    await infoPage.keyboard.press('Enter');
    await windowGone(app, 'englishInfo.html');
    await expect(countdown(app)).toHaveText(/^(60|59)$/);
  } finally {
    await closeApp(app);
  }
});

test('shows a multiple choice task, its result screen, and Enter restarts the countdown', async () => {
  const app = await launchApp();

  try {
    await setTasks(app, { englishWord: false, multipleChoice: true, japaneseWord: false, dumbbellWorkout: false });
    await app.call('windows.showMainWindow');

    const mcqPage = await waitForWindow(app, 'mcq.html');
    await expect(mcqPage.locator('#question')).toHaveText('1+1=?');

    await mcqPage.getByRole('button', { name: '2', exact: true }).click();

    const resultPage = await waitForWindow(app, 'mcqResult.html');
    await expect(resultPage.locator('#result')).toHaveText('Correct');
    await expect(resultPage.locator('#details')).toContainText('1+1=2');
    await expect(resultPage.getByText('Enter to exit')).toBeVisible();

    await resultPage.keyboard.press('Enter');
    await windowGone(app, 'mcqResult.html');
    await expect(countdown(app)).toHaveText(/^(60|59)$/);

    await setTasks(app, ALL_TASKS);
  } finally {
    await closeApp(app);
  }
});

test('opens the settings window with all task checkboxes', async () => {
  const app = await launchApp();

  try {
    await callSettingsWindow(app);

    const settingsPage = await waitForWindow(app, 'settings.html');
    await waitForSettingsLoaded(settingsPage);
    await expect(settingsPage.getByRole('heading', { name: 'Tasks' })).toBeVisible();
    await expect(settingsPage.locator('#englishWord')).toBeVisible();
    await expect(settingsPage.locator('#multipleChoice')).toBeVisible();
    await expect(settingsPage.locator('#japaneseWord')).toBeVisible();
    await expect(settingsPage.locator('#dumbbellWorkout')).toBeVisible();
    await expect(settingsPage.locator('#chromeFreeze')).toBeVisible();
    await expect(settingsPage.locator('#autoStart')).toBeVisible();
    await expect(settingsPage.locator('#masterVolume')).toBeVisible();
    await expect(settingsPage.locator('#masterVolume')).toHaveAttribute('min', '0');
    await expect(settingsPage.locator('#masterVolume')).toHaveAttribute('max', '4');
    await expect(settingsPage.locator('#masterVolume')).toHaveAttribute('step', '0.5');
    await expect(settingsPage.locator('#autoTheme')).toBeVisible();
    await expect(settingsPage.locator('#autoTheme')).not.toBeChecked();
    await expect(settingsPage.getByRole('button', { name: 'Ignore words…' })).toBeVisible();
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

    await app.call('windows.showMainWindow');
    await waitForWindow(app, 'english.html');

    await app.call('windows.hideMainWindow');
    await app.call('windows.showMainWindow');
    await waitForWindow(app, 'mcq.html');

    await app.call('windows.hideMainWindow');
    await app.call('windows.showMainWindow');
    await waitForWindow(app, 'japanese.html');

    await app.call('windows.hideMainWindow');
    await app.call('windows.showMainWindow');
    await waitForWindow(app, 'english.html');
  } finally {
    await closeApp(app);
  }
});

test('checks the japanese word, opens its info screen, and Enter restarts the countdown', async () => {
  const app = await launchApp();

  try {
    await setTasks(app, { englishWord: false, multipleChoice: false, japaneseWord: true, dumbbellWorkout: false });
    await app.call('windows.showMainWindow');

    const wordPage = await waitForWindow(app, 'japanese.html');
    await expect(wordPage.locator('#word')).toHaveText('桜');
    await expect(wordPage.locator('#hiragana')).toHaveText('さくら');
    await expect(wordPage.locator('#katakana')).toHaveText('サクラ');
    await expect(wordPage.locator('#romaji')).toHaveText('sakura');

    await wordPage.locator('#answer').fill('sakura');
    await expect(wordPage.locator('#feedback')).toHaveText('Correct');

    const infoPage = await waitForWindow(app, 'japaneseInfo.html');
    await expect(infoPage.locator('#kanji')).toHaveText('桜');
    await expect(infoPage.locator('#hiragana')).toHaveText('さくら');
    await expect(infoPage.locator('#katakana')).toHaveText('サクラ');
    await expect(infoPage.locator('#romaji')).toHaveText('sakura');
    await expect(infoPage.locator('#meaning_en')).toHaveText('cherry blossom');
    await expect(infoPage.locator('#examples')).toContainText('桜が咲きました。');
    await expect(infoPage.locator('#examples')).toContainText('The cherry blossoms bloomed.');
    await expect(infoPage.locator('#examples')).toContainText('Hoa anh đào đã nở.');
    await expect(infoPage.locator('#speak-kanji')).toBeVisible();
    await expect(infoPage.locator('#speak-hiragana')).toBeVisible();
    await expect(infoPage.locator('#speak-katakana')).toBeVisible();
    await expect(infoPage.locator('#examples .ui-example')).toHaveCount(2);
    await expect(infoPage.locator('#examples button')).toHaveCount(2);
    await expect(infoPage.locator('#examples_en')).toHaveCount(0);
    await expect(infoPage.locator('#examples_vi')).toHaveCount(0);
    await expect(infoPage.getByText('Enter to exit')).toBeVisible();

    await infoPage.keyboard.press('Enter');
    await windowGone(app, 'japaneseInfo.html');
    await expect(countdown(app)).toHaveText(/^(60|59)$/);

    await setTasks(app, ALL_TASKS);
  } finally {
    await closeApp(app);
  }
});

test('confirm applies settings and restarts the countdown', async () => {
  const app = await launchApp();

  try {
    await setTasks(app, ALL_TASKS);
    await callSettingsWindow(app);

    const settingsPage = await waitForWindow(app, 'settings.html');
    await waitForSettingsLoaded(settingsPage);
    await settingsPage.locator('#englishWord').uncheck();
    await clickSettingsButton(settingsPage, 'confirm');

    await expect.poll(async () => (await getSettings(app)).englishWord).toBe(false);
    await expect(countdown(app)).toHaveText(/^(60|59)$/);
    await windowGone(app, 'settings.html');
  } finally {
    await setTasks(app, ALL_TASKS);
    await closeApp(app);
  }
});

test('saves master volume from the settings slider', async () => {
  const app = await launchApp();
  let initialVolume = 1;

  try {
    await callSettingsWindow(app);

    let settingsPage = await waitForWindow(app, 'settings.html');
    await waitForSettingsLoaded(settingsPage);
    initialVolume = Number(await settingsPage.locator('#masterVolume').inputValue());
    const target = initialVolume === 0.5 ? 2.5 : 0.5;

    await settingsPage.locator('#masterVolume').evaluate((element, value) => {
      element.value = value;
      element.dispatchEvent(new Event('input', { bubbles: true }));
    }, String(target));
    await expect(settingsPage.locator('#volumeValue')).toHaveText(`${target}×`);

    await clickSettingsButton(settingsPage, 'confirm');
    await expect.poll(async () => (await getSettings(app)).masterVolume).toBe(target);
    await windowGone(app, 'settings.html');

    await callSettingsWindow(app);
    settingsPage = await waitForWindow(app, 'settings.html');
    await waitForSettingsLoaded(settingsPage);
    await expect(settingsPage.locator('#masterVolume')).toHaveValue(String(target));
  } finally {
    await app.call('settings.setSettings', { ...(await getSettings(app)), masterVolume: initialVolume });
    await closeApp(app);
  }
});

test('saves a custom countdown and applies it when settings are confirmed', async () => {
  const app = await launchApp();
  let initialCountdown = 60;

  try {
    await callSettingsWindow(app);

    const settingsPage = await waitForWindow(app, 'settings.html');
    await waitForSettingsLoaded(settingsPage);
    initialCountdown = Number(await settingsPage.locator('#countdownSeconds').inputValue());
    await settingsPage.locator('#countdownSeconds').fill('120');
    await clickSettingsButton(settingsPage, 'confirm');

    await expect.poll(async () => (await getSettings(app)).countdownSeconds).toBe(120);
    await expect(countdown(app)).toHaveText(/^(120|119)$/);
  } finally {
    await app.call('settings.setSettings', { ...(await getSettings(app)), countdownSeconds: initialCountdown });
    await closeApp(app);
  }
});

test('cancel discards settings changes', async () => {
  const app = await launchApp();

  try {
    await setTasks(app, ALL_TASKS);
    await callSettingsWindow(app);

    const settingsPage = await waitForWindow(app, 'settings.html');
    await waitForSettingsLoaded(settingsPage);
    await settingsPage.locator('#englishWord').uncheck();
    await clickSettingsButton(settingsPage, 'cancel');

    await expect.poll(async () => (await getSettings(app)).englishWord).toBe(true);
    await windowGone(app, 'settings.html');
  } finally {
    await closeApp(app);
  }
});
