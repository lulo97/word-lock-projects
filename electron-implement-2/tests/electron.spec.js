const path = require('node:path');
const { test, expect, _electron: electron } = require('@playwright/test');

const projectDir = path.join(__dirname, '..');

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

    await setTasks(app, { englishWord: true, multipleChoice: false });
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

test('checks the word, opens InfoScreen, and Enter restarts the countdown', async () => {
  const app = await launchApp();

  try {
    await setTasks(app, { englishWord: true, multipleChoice: false });
    await callMainWindow(app, 'showMainWindow');

    const wordPage = app.windows().find((window) => window.url().endsWith('index.html'));
    await expect(wordPage.locator('#word')).toHaveText('apple');

    await wordPage.locator('#answer').fill('apple');
    await expect(wordPage.locator('#feedback')).toHaveText('Correct');
    await expect(wordPage.locator('#feedback')).toHaveClass(/text-green-600/);

    await expect
      .poll(() => app.windows().some((window) => window.url().includes('info.html')))
      .toBe(true);

    const infoPage = app.windows().find((window) => window.url().includes('info.html'));
    await expect(infoPage.locator('#details')).toContainText('apple');
    await expect(infoPage.locator('#details')).toContainText('quả táo');
    await expect(infoPage.locator('#details')).toContainText('an apple a day keep the doctor away');
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
    await setTasks(app, { englishWord: false, multipleChoice: true });
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

    await setTasks(app, { englishWord: true, multipleChoice: true });
  } finally {
    await closeApp(app);
  }
});

test('opens the settings window with both task checkboxes', async () => {
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
  } finally {
    await closeApp(app);
  }
});

test('alternates between word and mcq tasks with round robin', async () => {
  const app = await launchApp();

  try {
    await setTasks(app, { englishWord: true, multipleChoice: true });

    await callMainWindow(app, 'showMainWindow');
    await expect
      .poll(() => app.windows().some((window) => window.url().endsWith('index.html')))
      .toBe(true);

    await callMainWindow(app, 'hideMainWindow');
    await callMainWindow(app, 'showMainWindow');
    await expect
      .poll(() => app.windows().some((window) => window.url().endsWith('mcq.html')))
      .toBe(true);

    await callMainWindow(app, 'hideMainWindow');
    await callMainWindow(app, 'showMainWindow');
    await expect
      .poll(() => app.windows().some((window) => window.url().endsWith('index.html')))
      .toBe(true);
  } finally {
    await closeApp(app);
  }
});
