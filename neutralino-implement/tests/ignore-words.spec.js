const { test, expect } = require('@playwright/test');
const {
  launchApp, closeApp, waitForWindow, windowGone, hostDbExec,
} = require('./helpers');

async function openSettings(app) {
  await app.call('windows.createSettingsWindow');
  const settingsPage = await waitForWindow(app, 'settings.html');
  await expect(settingsPage.locator('body')).toHaveAttribute('data-settings-loaded', 'true');
  return settingsPage;
}

async function openIgnoreWords(app, settingsPage) {
  await settingsPage.getByRole('button', { name: 'Ignore words…' }).click();
  return waitForWindow(app, 'ignoreWords.html');
}

async function insertRows(app) {
  await hostDbExec(app, `
    INSERT OR REPLACE INTO english_word (id, word, meaning_vi, examples, examples_vi) VALUES
      (9101, 'banana', 'quả chuối', '[]', '[]'),
      (9102, 'cat', 'con mèo', '[]', '[]'),
      (9103, 'dog', 'con chó', '[]', '[]'),
      (9104, 'sun', 'mặt trời', '[]', '[]');

    INSERT OR REPLACE INTO japanese_word
      (id, kanji, hiragana, katakana, romaji, meaning_en, examples, examples_en, examples_vi)
    VALUES
      (9101, '猫', 'ねこ', 'ネコ', 'neko', 'cat', '[]', '[]', '[]'),
      (9102, '犬', 'いぬ', 'イヌ', 'inu', 'dog', '[]', '[]', '[]'),
      (9103, '鳥', 'とり', 'トリ', 'tori', 'bird', '[]', '[]', '[]');
  `);
}

async function deleteRows(app) {
  await hostDbExec(app, `
    DELETE FROM english_word WHERE id >= 9100;
    DELETE FROM japanese_word WHERE id >= 9100;
  `);
}

async function visibleWord(app, language) {
  await app.call('windows.showMainWindow');
  const suffix = language === 'english' ? 'english.html' : 'japanese.html';
  const page = await waitForWindow(app, suffix);
  await expect(page.locator('#word')).not.toHaveText('');
  return page.locator('#word').textContent();
}

test('Open Ignore Words validates comma-separated input and saves both tabs', async () => {
  const app = await launchApp();
  let original = { english: [], japanese: [] };

  try {
    original = await app.call('settings.getIgnoreWords');
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
    expect(await app.call('settings.getIgnoreWords')).toEqual(original);

    // A trailing comma (empty item) is invalid.
    await ignorePage.locator('#words').fill('apple, banana,');
    await ignorePage.getByRole('button', { name: 'Save' }).click();
    await expect(ignorePage.getByText("can't save ignore words")).toBeVisible();
    expect(await app.call('settings.getIgnoreWords')).toEqual(original);

    // Valid English list, duplicates included.
    await ignorePage.locator('#words').fill('abc, xyz, 123, abc');
    await ignorePage.getByRole('button', { name: 'Save' }).click();
    await windowGone(app, 'ignoreWords.html');
    expect((await app.call('settings.getIgnoreWords')).english).toEqual(['abc', 'xyz', '123', 'abc']);

    // Japanese tab rejects romaji/English but accepts kanji, hiragana and katakana.
    const japanesePage = await openIgnoreWords(app, settingsPage);
    await japanesePage.getByRole('tab', { name: 'Japanese' }).click();
    await japanesePage.locator('#words').fill('sakura');
    await japanesePage.getByRole('button', { name: 'Save' }).click();
    await expect(japanesePage.getByText("can't save ignore words")).toBeVisible();

    await japanesePage.locator('#words').fill('こんにちは, コンニチハ, 今日は');
    await japanesePage.getByRole('button', { name: 'Save' }).click();
    await windowGone(app, 'ignoreWords.html');
    expect((await app.call('settings.getIgnoreWords')).japanese).toEqual(['こんにちは', 'コンニチハ', '今日は']);
  } finally {
    await app.call('settings.setIgnoreWords', original);
    await closeApp(app);
  }
});

test('new database rows with ignored words never appear in the app', async () => {
  const app = await launchApp();
  let originalIgnore = { english: [], japanese: [] };
  let originalSettings = null;

  try {
    originalIgnore = await app.call('settings.getIgnoreWords');
    originalSettings = await app.call('settings.getSettings');

    await insertRows(app);
    await app.call('settings.setIgnoreWords', {
      english: 'banana, cat',
      japanese: '猫, いぬ, ネコ',
    });

    const ignoredEnglish = ['banana', 'cat'];
    const ignoredJapanese = ['猫', 'いぬ', 'ネコ'];

    const englishRows = await app.randomWords('english');
    expect(englishRows).not.toHaveLength(0);
    for (const row of englishRows) {
      expect(ignoredEnglish).not.toContain(row.word);
      expect(['apple', 'dog', 'sun']).toContain(row.word);
    }

    const japaneseRows = await app.randomWords('japanese');
    expect(japaneseRows).not.toHaveLength(0);
    for (const row of japaneseRows) {
      for (const field of ['kanji', 'hiragana', 'katakana', 'romaji']) {
        expect(ignoredJapanese).not.toContain(row[field]);
      }
      expect(['桜', '犬', '鳥']).toContain(row.kanji);
    }

    await app.call('settings.setSettings', {
      englishWord: true,
      multipleChoice: false,
      japaneseWord: false,
      dumbbellWorkout: false,
    });
    for (let i = 0; i < 5; i += 1) {
      const word = await visibleWord(app, 'english');
      expect(ignoredEnglish).not.toContain(word);
      await app.call('windows.hideMainWindow');
    }

    await app.call('settings.setSettings', {
      englishWord: false,
      multipleChoice: false,
      japaneseWord: true,
      dumbbellWorkout: false,
    });
    for (let i = 0; i < 5; i += 1) {
      const word = await visibleWord(app, 'japanese');
      expect(ignoredJapanese).not.toContain(word);
      await app.call('windows.hideMainWindow');
    }
  } finally {
    if (originalSettings) await app.call('settings.setSettings', originalSettings);
    await app.call('settings.setIgnoreWords', originalIgnore);
    await deleteRows(app);
    await closeApp(app);
  }
});
