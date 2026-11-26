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

async function launchInfoApp() {
  const app = await electron.launch({
    executablePath: require('electron'),
    args: [path.join(projectDir, 'src', 'main.js')],
  });

  await app.evaluate(
    (_electron, modulePath) =>
      process.mainModule
        .require(modulePath)
        .setSettings({ englishWord: true, multipleChoice: false, japaneseWord: false }),
    settingsPath,
  );
  await app.evaluate((_electron, modulePath) => process.mainModule.require(modulePath).showMainWindow(), windowPath);

  const page = app.windows().find((window) => window.url().endsWith('english.html'));
  await expect(page.locator('#word')).toHaveText('apple');
  await page.locator('#answer').fill('apple');
  await expect
    .poll(() => app.windows().some((window) => window.url().includes('englishInfo.html')))
    .toBe(true);

  const infoPage = app.windows().find((window) => window.url().includes('englishInfo.html'));
  return { app, infoPage };
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

async function restoreSettings(app) {
  await app.evaluate(
    (_electron, modulePath) =>
      process.mainModule
        .require(modulePath)
        .setSettings({ englishWord: true, multipleChoice: true, japaneseWord: true }),
    settingsPath,
  );
}

test('animates speech, ignores spam clicks, and queues other speak buttons', async () => {
  const { app, infoPage } = await launchInfoApp();

  try {
    await stubTts(app, 1000);

    await infoPage.locator('#speak-word').click();
    await expect(infoPage.locator('#speak-word')).toHaveText(/^Speak\.{1,3}$/);
    await expect(infoPage.locator('#speak-word')).toBeDisabled();

    await infoPage.locator('#speak-word').click({ force: true });
    await infoPage.locator('#examples .speak-button').first().click();
    await expect(infoPage.locator('#examples .speak-button').first()).toHaveText(/^Speak\.{1,3}$/);
    await expect(infoPage.locator('#examples .speak-button').first()).toBeDisabled();
    await expect
      .poll(() => app.evaluate(() => global.__ttsCalls.length))
      .toBe(1);

    await expect(infoPage.locator('#speak-word')).toHaveText('Speak', { timeout: 15000 });
    await expect(infoPage.locator('#speak-word')).toBeEnabled();
    await expect(infoPage.locator('#examples .speak-button').first()).toHaveText('Speak', { timeout: 15000 });
    await expect(infoPage.locator('#examples .speak-button').first()).toBeEnabled();

    expect(await app.evaluate(() => global.__ttsCalls)).toEqual([
      'apple:en',
      'he has an apple:en',
    ]);
  } finally {
    await restoreSettings(app);
    await app.evaluate(({ app }) => {
      app.isQuitting = true;
    });
    await app.close();
  }
});

test('closing the app while speech is playing does not hang', async () => {
  const { app, infoPage } = await launchInfoApp();

  await stubTts(app, 5000);
  await infoPage.locator('#speak-word').click();
  await expect(infoPage.locator('#speak-word')).toHaveText(/^Speak\.{1,3}$/);

  const startedAt = Date.now();
  await app.evaluate(({ app }) => {
    app.isQuitting = true;
  });
  await app.close();

  expect(Date.now() - startedAt).toBeLessThan(4000);
});
