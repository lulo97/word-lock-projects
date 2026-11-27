const { test, expect } = require('@playwright/test');
const { launchApp, closeApp, waitForWindow } = require('./helpers');

test('real MediaPipe model loads from the local HTTP origin', async () => {
  const app = await launchApp();

  try {
    await app.call('settings.setSettings', {
      englishWord: false,
      multipleChoice: false,
      japaneseWord: false,
      dumbbellWorkout: true,
    });
    await app.call('windows.showMainWindow');
    const page = await waitForWindow(app, 'dumbbell.html');

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
    await closeApp(app);
  }
});
