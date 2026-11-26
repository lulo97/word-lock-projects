const path = require('node:path');
const { test, expect, _electron: electron } = require('@playwright/test');

const projectDir = path.join(__dirname, '..');
const settingsPath = path.join(projectDir, 'src', 'settings', 'settings.js');
const databasePath = path.join(projectDir, 'src', 'database', 'database.js');
const settingsWindowPath = path.join(projectDir, 'src', 'settings', 'createSettingsWindow.js');
const ignoreWindowPath = path.join(projectDir, 'src', 'settings', 'createIgnoreWordsWindow.js');
const mainWindowPath = path.join(projectDir, 'src', 'main', 'createWindow.js');

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

async function callMain(app, modulePath, method, args = []) {
  return app.evaluate(
    (_electron, { modulePath, method, args }) => process.mainModule.require(modulePath)[method](...args),
    { modulePath, method, args },
  );
}

async function getIgnoreWords(app) {
  return callMain(app, settingsPath, 'getIgnoreWords');
}

async function setIgnoreWords(app, values) {
  return callMain(app, settingsPath, 'setIgnoreWords', [values]);
}

async function getSettings(app) {
  return callMain(app, settingsPath, 'getSettings');
}

async function setSettings(app, values) {
  return callMain(app, settingsPath, 'setSettings', [values]);
}

async function openSettings(app) {
  await callMain(app, settingsWindowPath, 'createSettingsWindow');
  await expect
    .poll(() => app.windows().some((window) => window.url().endsWith('settings.html')))
    .toBe(true);
  return app.windows().find((window) => window.url().endsWith('settings.html'));
}

async function openIgnoreWords(app, settingsPage) {
  await settingsPage.getByRole('button', { name: 'Ignore words…' }).click();
  await expect
    .poll(() => app.windows().some((window) => window.url().endsWith('ignoreWords.html')))
    .toBe(true);
  return app.windows().find((window) => window.url().endsWith('ignoreWords.html'));
}

async function insertRows(app) {
  await app.evaluate((_electron, modulePath) => {
    const { db } = process.mainModule.require(modulePath);

    const english = db.prepare(
      'INSERT OR REPLACE INTO english_word (id, word, meaning_vi, examples, examples_vi) VALUES (?, ?, ?, ?, ?)',
    );
    [
      [9101, 'banana', 'quả chuối', '[]', '[]'],
      [9102, 'cat', 'con mèo', '[]', '[]'],
      [9103, 'dog', 'con chó', '[]', '[]'],
      [9104, 'sun', 'mặt trời', '[]', '[]'],
    ].forEach((row) => english.run(...row));

    const japanese = db.prepare(
      `INSERT OR REPLACE INTO japanese_word
        (id, kanji, hiragana, katakana, romaji, meaning_en, examples, examples_en, examples_vi)
       VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)`,
    );
    [
      [9101, '猫', 'ねこ', 'ネコ', 'neko', 'cat', '[]', '[]', '[]'],
      [9102, '犬', 'いぬ', 'イヌ', 'inu', 'dog', '[]', '[]', '[]'],
      [9103, '鳥', 'とり', 'トリ', 'tori', 'bird', '[]', '[]', '[]'],
    ].forEach((row) => japanese.run(...row));
  }, databasePath);
}

async function deleteRows(app) {
  await app.evaluate((_electron, modulePath) => {
    const { db } = process.mainModule.require(modulePath);
    db.prepare('DELETE FROM english_word WHERE id >= 9100').run();
    db.prepare('DELETE FROM japanese_word WHERE id >= 9100').run();
  }, databasePath);
}

async function randomWords(app, language, times = 120) {
  return app.evaluate(
    (_electron, { modulePath, language, times }) => {
      const database = process.mainModule.require(modulePath);
      const select = language === 'english' ? database.getRandomWord : database.getRandomJapaneseWord;
      return Array.from({ length: times }, () => select());
    },
    { modulePath: databasePath, language, times },
  );
}

async function visibleWord(app, language) {
  await callMain(app, mainWindowPath, 'showMainWindow');
  const suffix = language === 'english' ? 'english.html' : 'japanese.html';
  await expect
    .poll(() => app.windows().some((window) => window.url().endsWith(suffix)))
    .toBe(true);

  const page = app.windows().find((window) => window.url().endsWith(suffix));
  await expect(page.locator('#word')).not.toHaveText('');
  return page.locator('#word').textContent();
}

test('Open Ignore Words validates comma-separated input and saves both tabs', async () => {
  const app = await launchApp();
  let original = { english: [], japanese: [] };

  try {
    original = await getIgnoreWords(app);
    const settingsPage = await openSettings(app);
    const ignorePage = await openIgnoreWords(app, settingsPage);

    await expect(ignorePage.getByRole('tab', { name: 'English' })).toBeVisible();
    await expect(ignorePage.getByRole('tab', { name: 'Japanese' })).toBeVisible();
    await expect(ignorePage.locator('#words')).toBeVisible();
    await expect(ignorePage.getByRole('button', { name: 'Save' })).toBeVisible();
    await expect(ignorePage.getByRole('button', { name: 'Cancel' })).toBeVisible();

    // One word per line is invalid.
    await ignorePage.locator('#words').fill('apple\nbanana');
    await ignorePage.getByRole('button', { name: 'Save' }).click();
    await expect(ignorePage.getByText("can't save ignore words")).toBeVisible();
    expect(await getIgnoreWords(app)).toEqual(original);

    // A trailing comma (empty item) is invalid.
    await ignorePage.locator('#words').fill('apple, banana,');
    await ignorePage.getByRole('button', { name: 'Save' }).click();
    await expect(ignorePage.getByText("can't save ignore words")).toBeVisible();
    expect(await getIgnoreWords(app)).toEqual(original);

    // Valid English list, duplicates included.
    await ignorePage.locator('#words').fill('abc, xyz, 123, abc');
    await ignorePage.getByRole('button', { name: 'Save' }).click();
    await expect
      .poll(() => app.windows().some((window) => window.url().endsWith('ignoreWords.html')))
      .toBe(false);
    expect((await getIgnoreWords(app)).english).toEqual(['abc', 'xyz', '123', 'abc']);

    // Japanese tab rejects romaji/English but accepts kanji, hiragana and katakana.
    const japanesePage = await openIgnoreWords(app, settingsPage);
    await japanesePage.getByRole('tab', { name: 'Japanese' }).click();
    await japanesePage.locator('#words').fill('sakura');
    await japanesePage.getByRole('button', { name: 'Save' }).click();
    await expect(japanesePage.getByText("can't save ignore words")).toBeVisible();

    await japanesePage.locator('#words').fill('こんにちは, コンニチハ, 今日は');
    await japanesePage.getByRole('button', { name: 'Save' }).click();
    await expect
      .poll(() => app.windows().some((window) => window.url().endsWith('ignoreWords.html')))
      .toBe(false);
    expect((await getIgnoreWords(app)).japanese).toEqual(['こんにちは', 'コンニチハ', '今日は']);
  } finally {
    await setIgnoreWords(app, original);
    await closeApp(app);
  }
});

test('new database rows with ignored words never appear in the app', async () => {
  const app = await launchApp();
  let originalIgnore = { english: [], japanese: [] };
  let originalSettings = null;

  try {
    originalIgnore = await getIgnoreWords(app);
    originalSettings = await getSettings(app);

    await insertRows(app);
    await setIgnoreWords(app, {
      english: 'banana, cat',
      japanese: '猫, いぬ, ネコ',
    });

    const ignoredEnglish = ['banana', 'cat'];
    const ignoredJapanese = ['猫', 'いぬ', 'ネコ'];

    const englishRows = await randomWords(app, 'english');
    expect(englishRows).not.toHaveLength(0);
    for (const row of englishRows) {
      expect(ignoredEnglish).not.toContain(row.word);
      expect(['apple', 'dog', 'sun']).toContain(row.word);
    }

    const japaneseRows = await randomWords(app, 'japanese');
    expect(japaneseRows).not.toHaveLength(0);
    for (const row of japaneseRows) {
      for (const field of ['kanji', 'hiragana', 'katakana', 'romaji']) {
        expect(ignoredJapanese).not.toContain(row[field]);
      }
      expect(['桜', '犬', '鳥']).toContain(row.kanji);
    }

    await setSettings(app, {
      englishWord: true,
      multipleChoice: false,
      japaneseWord: false,
      dumbbellWorkout: false,
    });
    for (let i = 0; i < 5; i += 1) {
      const word = await visibleWord(app, 'english');
      expect(ignoredEnglish).not.toContain(word);
      await callMain(app, mainWindowPath, 'hideMainWindow');
    }

    await setSettings(app, {
      englishWord: false,
      multipleChoice: false,
      japaneseWord: true,
      dumbbellWorkout: false,
    });
    for (let i = 0; i < 5; i += 1) {
      const word = await visibleWord(app, 'japanese');
      expect(ignoredJapanese).not.toContain(word);
      await callMain(app, mainWindowPath, 'hideMainWindow');
    }
  } finally {
    if (originalSettings) await setSettings(app, originalSettings);
    await setIgnoreWords(app, originalIgnore);
    await deleteRows(app);
    await closeApp(app);
  }
});
