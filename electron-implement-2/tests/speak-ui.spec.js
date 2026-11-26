const path = require('node:path');
const { test, expect, _electron: electron } = require('@playwright/test');

const projectDir = path.join(__dirname, '..');
const settingsPath = path.join(projectDir, 'src', 'settings', 'settings.js');
const windowPath = path.join(projectDir, 'src', 'main', 'createWindow.js');

function makeWav() {
  const sampleRate = 8000;
  const dataSize = 800 * 2;
  const buffer = Buffer.alloc(44 + dataSize);

  buffer.write('RIFF', 0);
  buffer.writeUInt32LE(36 + dataSize, 4);
  buffer.write('WAVE', 8);
  buffer.write('fmt ', 12);
  buffer.writeUInt32LE(16, 16);
  buffer.writeUInt16LE(1, 20);
  buffer.writeUInt16LE(1, 22);
  buffer.writeUInt32LE(sampleRate, 24);
  buffer.writeUInt32LE(sampleRate * 2, 28);
  buffer.writeUInt16LE(2, 32);
  buffer.writeUInt16LE(16, 34);
  buffer.write('data', 36);
  buffer.writeUInt32LE(dataSize, 40);

  return buffer.toString('base64');
}

async function stubTts(app, delayMs) {
  await app.evaluate(
    (_electron, { wav, delay }) => {
      const { ipcMain } = process.mainModule.require('electron');

      ipcMain.removeHandler('tts:speak');
      global.__ttsCalls = [];
      ipcMain.handle('tts:speak', async (_event, text, lang) => {
        global.__ttsCalls.push(`${text}:${lang}`);
        await new Promise((resolve) => setTimeout(resolve, delay));
        return wav;
      });
    },
    { wav: makeWav(), delay: delayMs },
  );
}

async function stubTtsUnavailable(app) {
  await app.evaluate((_electron) => {
    const { ipcMain } = process.mainModule.require('electron');

    ipcMain.removeHandler('tts:speak');
    ipcMain.handle('tts:speak', async () => ({
      ok: false,
      error: 'TTS server is not available at 127.0.0.1:8080.',
    }));
  });
}

async function launchApp(stubDelayMs) {
  const app = await electron.launch({
    executablePath: require('electron'),
    args: [path.join(projectDir, 'src', 'main.js')],
  });

  if (stubDelayMs !== undefined) {
    await stubTts(app, stubDelayMs);
  }

  return app;
}

async function showEnglishInfo(app) {
  await app.evaluate(
    (_electron, modulePath) =>
      process.mainModule
        .require(modulePath)
        .setSettings({ englishWord: true, multipleChoice: false, japaneseWord: false, dumbbellWorkout: false }),
    settingsPath,
  );
  await app.evaluate((_electron, modulePath) => process.mainModule.require(modulePath).showMainWindow(), windowPath);
  await expect
    .poll(() => app.windows().some((window) => window.url().endsWith('english.html')))
    .toBe(true);

  const page = app.windows().find((window) => window.url().endsWith('english.html'));
  await expect(page.locator('#word')).toHaveText('apple');
  await page.locator('#answer').fill('apple');
  await expect
    .poll(() => app.windows().some((window) => window.url().includes('englishInfo.html')))
    .toBe(true);

  return app.windows().find((window) => window.url().includes('englishInfo.html'));
}

async function showJapaneseInfo(app) {
  await app.evaluate(
    (_electron, modulePath) =>
      process.mainModule
        .require(modulePath)
        .setSettings({ englishWord: false, multipleChoice: false, japaneseWord: true, dumbbellWorkout: false }),
    settingsPath,
  );
  await app.evaluate((_electron, modulePath) => process.mainModule.require(modulePath).showMainWindow(), windowPath);
  await expect
    .poll(() => app.windows().some((window) => window.url().endsWith('japanese.html')))
    .toBe(true);

  const page = app.windows().find((window) => window.url().endsWith('japanese.html'));
  await expect(page.locator('#word')).toHaveText('桜');
  await page.locator('#answer').fill('sakura');
  await expect
    .poll(() => app.windows().some((window) => window.url().includes('japaneseInfo.html')))
    .toBe(true);

  return app.windows().find((window) => window.url().includes('japaneseInfo.html'));
}

async function restoreSettings(app) {
  await app.evaluate(
    (_electron, modulePath) =>
      process.mainModule
        .require(modulePath)
        .setSettings({ englishWord: true, multipleChoice: true, japaneseWord: true, dumbbellWorkout: true }),
    settingsPath,
  );
}

async function quitApp(app) {
  await app.evaluate(({ app }) => {
    app.isQuitting = true;
  });
  await app.close();
}

test('auto-speaks the English word once when the info screen opens', async () => {
  const app = await launchApp(1000);

  try {
    const infoPage = await showEnglishInfo(app);

    await expect.poll(() => app.evaluate(() => global.__ttsCalls.length)).toBe(1);
    await expect(infoPage.locator('#speak-word')).toHaveText(/^Speak\.{1,3}$/);
    await expect(infoPage.locator('#speak-word')).toBeDisabled();

    await expect(infoPage.locator('#speak-word')).toHaveText('Speak', { timeout: 15000 });
    await expect(infoPage.locator('#speak-word')).toBeEnabled();
    expect(await app.evaluate(() => global.__ttsCalls)).toEqual(['apple:en']);
  } finally {
    await restoreSettings(app);
    await quitApp(app);
  }
});

test('auto-speaks the Japanese kanji once when the info screen opens', async () => {
  const app = await launchApp(1000);

  try {
    const infoPage = await showJapaneseInfo(app);

    await expect.poll(() => app.evaluate(() => global.__ttsCalls.length)).toBe(1);
    await expect(infoPage.locator('#speak-kanji')).toHaveText(/^Speak\.{1,3}$/);
    await expect(infoPage.locator('#speak-kanji')).toBeDisabled();

    await expect(infoPage.locator('#speak-kanji')).toHaveText('Speak', { timeout: 15000 });
    await expect(infoPage.locator('#speak-kanji')).toBeEnabled();
    expect(await app.evaluate(() => global.__ttsCalls)).toEqual(['桜:ja']);
  } finally {
    await restoreSettings(app);
    await quitApp(app);
  }
});

test('shows a message when the TTS server is unavailable and keeps the app usable', async () => {
  const app = await launchApp();

  try {
    await stubTtsUnavailable(app);

    const infoPage = await showEnglishInfo(app);

    await expect(infoPage.locator('#tts-status')).toHaveText(
      'TTS server is not available at 127.0.0.1:8080.',
    );
    await expect(infoPage.locator('#speak-word')).toHaveText('Speak');
    await expect(infoPage.locator('#speak-word')).toBeEnabled();

    // The app is still responsive after the failed speech request.
    await infoPage.keyboard.press('Enter');
    await expect
      .poll(() => app.windows().some((window) => window.url().endsWith('countdown.html')))
      .toBe(true);
  } finally {
    await restoreSettings(app);
    await quitApp(app);
  }
});

test('animates speech, ignores spam clicks, and queues other speak buttons', async () => {
  const app = await launchApp(1000);

  try {
    const infoPage = await showEnglishInfo(app);

    await expect.poll(() => app.evaluate(() => global.__ttsCalls.length)).toBe(1);
    await expect(infoPage.locator('#speak-word')).toHaveText('Speak', { timeout: 15000 });
    await expect(infoPage.locator('#speak-word')).toBeEnabled();

    await infoPage.locator('#speak-word').click();
    await expect(infoPage.locator('#speak-word')).toHaveText(/^Speak\.{1,3}$/);
    await expect(infoPage.locator('#speak-word')).toBeDisabled();

    await infoPage.locator('#speak-word').click({ force: true });
    await infoPage.locator('#examples .speak-button').first().click();
    await expect(infoPage.locator('#examples .speak-button').first()).toHaveText(/^Speak\.{1,3}$/);
    await expect(infoPage.locator('#examples .speak-button').first()).toBeDisabled();
    await expect.poll(() => app.evaluate(() => global.__ttsCalls.length)).toBe(2);

    await expect(infoPage.locator('#speak-word')).toHaveText('Speak', { timeout: 15000 });
    await expect(infoPage.locator('#speak-word')).toBeEnabled();
    await expect(infoPage.locator('#examples .speak-button').first()).toHaveText('Speak', { timeout: 15000 });
    await expect(infoPage.locator('#examples .speak-button').first()).toBeEnabled();

    expect(await app.evaluate(() => global.__ttsCalls)).toEqual([
      'apple:en',
      'apple:en',
      'he has an apple:en',
    ]);
  } finally {
    await restoreSettings(app);
    await quitApp(app);
  }
});

test('closing the app while speech is playing does not hang', async () => {
  const app = await launchApp(5000);
  const infoPage = await showEnglishInfo(app);

  await expect(infoPage.locator('#speak-word')).toHaveText(/^Speak\.{1,3}$/);

  const startedAt = Date.now();
  await quitApp(app);

  expect(Date.now() - startedAt).toBeLessThan(4000);
});
