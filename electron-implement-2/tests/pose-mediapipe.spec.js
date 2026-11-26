const path = require('node:path');
const { test, expect, _electron: electron } = require('@playwright/test');

const projectDir = path.join(__dirname, '..');
const settingsPath = path.join(projectDir, 'src', 'settings', 'settings.js');
const windowPath = path.join(projectDir, 'src', 'main', 'createWindow.js');

test('real MediaPipe model loads from app://', async () => {
  const app = await electron.launch({
    executablePath: require('electron'),
    args: [
      path.join(projectDir, 'src', 'main.js'),
      '--use-fake-device-for-media-stream',
      '--use-fake-ui-for-media-stream',
    ],
  });

  try {
    await app.evaluate(
      (_electron, modulePath) =>
        process.mainModule
          .require(modulePath)
          .setSettings({ englishWord: false, multipleChoice: false, japaneseWord: false, dumbbellWorkout: true }),
      settingsPath,
    );
    await app.evaluate((_electron, modulePath) => process.mainModule.require(modulePath).showMainWindow(), windowPath);
    await expect
      .poll(() => app.windows().some((window) => window.url().endsWith('dumbbell.html')))
      .toBe(true);
    const page = app.windows().find((window) => window.url().endsWith('dumbbell.html'));

    await page.waitForFunction(() => document.getElementById('camera').videoWidth > 0, null, { timeout: 10000 });

    const result = await page.evaluate(async () => {
      try {
        const { createPoseTracker } = await import('./pose-tracker.js');
        const tracker = await createPoseTracker();
        const video = document.getElementById('camera');
        const detection = tracker.detect(video, 1);
        tracker.close();
        return { ok: true, detection };
      } catch (error) {
        return { ok: false, error: String(error && error.stack ? error.stack : error) };
      }
    });

    expect(result, result.error).toMatchObject({ ok: true });
    expect(result.detection).toHaveProperty('left');
    expect(result.detection).toHaveProperty('right');
  } finally {
    await app.evaluate(({ app }) => {
      app.isQuitting = true;
    });
    await app.close();
  }
});
