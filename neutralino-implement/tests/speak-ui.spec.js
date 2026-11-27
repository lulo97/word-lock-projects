const { test, expect } = require('@playwright/test');
const { launchApp, closeApp, waitForWindow } = require('./helpers');

const ALL_TASKS = {
  englishWord: true,
  multipleChoice: true,
  japaneseWord: true,
  dumbbellWorkout: true,
};

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
  await app.evaluate(({ wav, delay }) => {
    window.__ttsCalls = [];
    window.__host.router.override('tts:speak', async (text, lang) => {
      window.__ttsCalls.push(`${text}:${lang}`);
      await new Promise((resolve) => setTimeout(resolve, delay));
      return { ok: true, wav };
    });
  }, { wav: makeWav(), delay: delayMs });
}

async function stubTtsUnavailable(app) {
  await app.evaluate(() => {
    window.__ttsCalls = [];
    window.__host.router.override('tts:speak', async () => ({
      ok: false,
      error: 'TTS server is not available at 127.0.0.1:8080.',
    }));
  });
}

async function ttsCalls(app) {
  return app.evaluate(() => window.__ttsCalls || []);
}

async function launchWithStub(stubDelayMs) {
  const app = await launchApp();
  if (stubDelayMs !== undefined) await stubTts(app, stubDelayMs);
  return app;
}

async function showEnglishInfo(app) {
  await app.call('settings.setSettings', {
    englishWord: true,
    multipleChoice: false,
    japaneseWord: false,
    dumbbellWorkout: false,
  });
  await app.call('windows.showMainWindow');

  const page = await waitForWindow(app, 'english.html');
  await expect(page.locator('#word')).toHaveText('apple');
  await page.locator('#answer').fill('apple');
  return waitForWindow(app, 'englishInfo.html');
}

async function showJapaneseInfo(app) {
  await app.call('settings.setSettings', {
    englishWord: false,
    multipleChoice: false,
    japaneseWord: true,
    dumbbellWorkout: false,
  });
  await app.call('windows.showMainWindow');

  const page = await waitForWindow(app, 'japanese.html');
  await expect(page.locator('#word')).toHaveText('桜');
  await page.locator('#answer').fill('sakura');
  return waitForWindow(app, 'japaneseInfo.html');
}

test('auto-speaks the English word once when the info screen opens', async () => {
  const app = await launchWithStub(1000);

  try {
    const infoPage = await showEnglishInfo(app);

    await expect.poll(async () => (await ttsCalls(app)).length).toBe(1);
    await expect(infoPage.locator('#speak-word')).toHaveText(/^Speak\.{1,3}$/);
    await expect(infoPage.locator('#speak-word')).toBeDisabled();

    await expect(infoPage.locator('#speak-word')).toHaveText('Speak', { timeout: 15000 });
    await expect(infoPage.locator('#speak-word')).toBeEnabled();
    expect(await ttsCalls(app)).toEqual(['apple:en']);
  } finally {
    await app.call('settings.setSettings', ALL_TASKS);
    await closeApp(app);
  }
});

test('auto-speaks the Japanese kanji once when the info screen opens', async () => {
  const app = await launchWithStub(1000);

  try {
    const infoPage = await showJapaneseInfo(app);

    await expect.poll(async () => (await ttsCalls(app)).length).toBe(1);
    await expect(infoPage.locator('#speak-kanji')).toHaveText(/^Speak\.{1,3}$/);
    await expect(infoPage.locator('#speak-kanji')).toBeDisabled();

    await expect(infoPage.locator('#speak-kanji')).toHaveText('Speak', { timeout: 15000 });
    await expect(infoPage.locator('#speak-kanji')).toBeEnabled();
    expect(await ttsCalls(app)).toEqual(['桜:ja']);
  } finally {
    await app.call('settings.setSettings', ALL_TASKS);
    await closeApp(app);
  }
});

test('shows the TTS error only after a Speak button is clicked', async () => {
  const app = await launchApp();

  try {
    await stubTtsUnavailable(app);

    const infoPage = await showEnglishInfo(app);

    // Automatic speech on page load fails quietly.
    await expect(infoPage.locator('#speak-word')).toHaveText('Speak', { timeout: 15000 });
    await expect(infoPage.locator('#speak-word')).toBeEnabled();
    await expect(infoPage.locator('#tts-status')).toHaveCount(0);

    // A user pressing Speak gets the visible explanation.
    await infoPage.locator('#speak-word').click();
    await expect(infoPage.locator('#tts-status')).toHaveText(
      'TTS server is not available at 127.0.0.1:8080.',
    );
    await expect(infoPage.locator('#speak-word')).toHaveText('Speak');
    await expect(infoPage.locator('#speak-word')).toBeEnabled();

    // The app is still responsive after the failed speech request.
    await infoPage.keyboard.press('Enter');
    await expect.poll(async () =>
      app.windows().some((page) => page.url().endsWith('englishInfo.html'))).toBe(false);
  } finally {
    await app.call('settings.setSettings', ALL_TASKS);
    await closeApp(app);
  }
});

test('animates speech, ignores spam clicks, and queues other speak buttons', async () => {
  const app = await launchWithStub(1000);

  try {
    const infoPage = await showEnglishInfo(app);

    await expect.poll(async () => (await ttsCalls(app)).length).toBe(1);
    await expect(infoPage.locator('#speak-word')).toHaveText('Speak', { timeout: 15000 });
    await expect(infoPage.locator('#speak-word')).toBeEnabled();

    await infoPage.locator('#speak-word').click();
    await expect(infoPage.locator('#speak-word')).toHaveText(/^Speak\.{1,3}$/);
    await expect(infoPage.locator('#speak-word')).toBeDisabled();

    await infoPage.locator('#speak-word').click({ force: true });
    await infoPage.locator('#examples button').first().click();
    await expect(infoPage.locator('#examples button').first()).toHaveText(/^Speak\.{1,3}$/);
    await expect(infoPage.locator('#examples button').first()).toBeDisabled();
    await expect.poll(async () => (await ttsCalls(app)).length).toBe(2);

    await expect(infoPage.locator('#speak-word')).toHaveText('Speak', { timeout: 15000 });
    await expect(infoPage.locator('#speak-word')).toBeEnabled();
    await expect(infoPage.locator('#examples button').first()).toHaveText('Speak', { timeout: 15000 });
    await expect(infoPage.locator('#examples button').first()).toBeEnabled();

    expect(await ttsCalls(app)).toEqual([
      'apple:en',
      'apple:en',
      'he has an apple:en',
    ]);
  } finally {
    await app.call('settings.setSettings', ALL_TASKS);
    await closeApp(app);
  }
});

test('closing the app while speech is playing does not hang', async () => {
  const app = await launchWithStub(5000);
  const infoPage = await showEnglishInfo(app);

  await expect(infoPage.locator('#speak-word')).toHaveText(/^Speak\.{1,3}$/);

  const startedAt = Date.now();
  await closeApp(app);

  expect(Date.now() - startedAt).toBeLessThan(4000);
});
