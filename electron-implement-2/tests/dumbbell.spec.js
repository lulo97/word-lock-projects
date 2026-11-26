const path = require('node:path');
const { test, expect, _electron: electron } = require('@playwright/test');

const projectDir = path.join(__dirname, '..');
const settingsPath = path.join(projectDir, 'src', 'settings', 'settings.js');
const windowPath = path.join(projectDir, 'src', 'main', 'createWindow.js');

let fakeNow = 1000;

async function launchDumbbellApp() {
  return electron.launch({
    executablePath: require('electron'),
    args: [
      path.join(projectDir, 'src', 'main.js'),
      '--use-fake-device-for-media-stream',
      '--use-fake-ui-for-media-stream',
    ],
  });
}

async function showDumbbell(app) {
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

  return app.windows().find((window) => window.url().endsWith('dumbbell.html'));
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

async function waitForPoseTracker(page) {
  await expect(page.locator('#status')).toHaveText('Pose tracker ready', { timeout: 25000 });
  await page.waitForFunction(() => Boolean(window.__dumbbellTest), null, { timeout: 5000 });
  fakeNow = 1000;
}

async function feedPose(page, leftAngle, rightAngle, advanceMs = 0) {
  fakeNow += advanceMs;
  await page.evaluate(
    ([left, right, now]) => window.__dumbbellTest.feedPose(left, right, null, now),
    [leftAngle, rightAngle, fakeNow],
  );
}

// Enough frames for the 5-frame median to settle and the 3-frame debounce to confirm.
async function holdAngle(page, angle, frames = 6) {
  for (let frame = 0; frame < frames; frame += 1) {
    await feedPose(page, angle, angle);
  }
}

async function doRep(page) {
  await holdAngle(page, 170); // down
  await holdAngle(page, 40); // up
  fakeNow += 400; // MIN_UP_MS
  await holdAngle(page, 170); // down again = one rep
  fakeNow += 900; // MIN_REP_MS before the next rep
}

test('renders two arm progress bars of 10 reps each', async () => {
  const app = await launchDumbbellApp();

  try {
    const page = await showDumbbell(app);
    await waitForPoseTracker(page);

    await expect(page.locator('#overall')).toHaveText('Total 0 / 20');
    await expect(page.locator('#left-reps')).toHaveText('0 / 10');
    await expect(page.locator('#right-reps')).toHaveText('0 / 10');
    await expect(page.locator('#left-bar span')).toHaveCount(10);
    await expect(page.locator('#right-bar span')).toHaveCount(10);
    await expect(page.locator('#left-stage')).toHaveText('ARM HIDDEN');
    await expect(page.locator('#right-stage')).toHaveText('ARM HIDDEN');
    await expect(page.locator('#done')).toBeHidden();
    await expect(page.locator('#overlay')).toBeVisible();
    await expect(page.locator('#camera')).toHaveClass(/mirror/);
    await expect(page.locator('#connection, #seen, #recent, #hold')).toHaveCount(0);

    const asideOverflow = await page.locator('aside').evaluate((element) => ({
      scrollHeight: element.scrollHeight,
      clientHeight: element.clientHeight,
    }));
    expect(asideOverflow.scrollHeight).toBeLessThanOrEqual(asideOverflow.clientHeight + 1);
  } finally {
    await restoreSettings(app);
    await quitApp(app);
  }
});

test('counts one rep per up and down cycle on each arm', async () => {
  const app = await launchDumbbellApp();

  try {
    const page = await showDumbbell(app);
    await waitForPoseTracker(page);

    await doRep(page);
    await expect(page.locator('#left-reps')).toHaveText('1 / 10');
    await expect(page.locator('#right-reps')).toHaveText('1 / 10');
    await expect(page.locator('#left-bar span.bg-green-600')).toHaveCount(1);
    await expect(page.locator('#right-bar span.bg-green-600')).toHaveCount(1);
    await expect(page.locator('#left-stage')).toContainText('DOWN');
    await expect(page.locator('#overall')).toHaveText('Total 2 / 20');

    await doRep(page);
    await expect(page.locator('#overall')).toHaveText('Total 4 / 20');

    await feedPose(page, null, null);
    await expect(page.locator('#left-stage')).toHaveText('ARM HIDDEN');
    await expect(page.locator('#right-stage')).toHaveText('ARM HIDDEN');
    await expect(page.locator('#overall')).toHaveText('Total 4 / 20');
  } finally {
    await restoreSettings(app);
    await quitApp(app);
  }
});

test('ignores angle spikes, too-fast reps and a lost arm', async () => {
  const app = await launchDumbbellApp();

  try {
    const page = await showDumbbell(app);
    await waitForPoseTracker(page);

    await holdAngle(page, 170); // stage down
    await holdAngle(page, 40); // stage up

    // A 2-frame glitch is median-filtered and never confirmed.
    await feedPose(page, 170, 170);
    await feedPose(page, 170, 170);
    await holdAngle(page, 40);
    await expect(page.locator('#left-reps')).toHaveText('0 / 10');

    // A full down swing less than 300 ms after the top must not count.
    await holdAngle(page, 170);
    await expect(page.locator('#left-reps')).toHaveText('0 / 10');

    // A proper rep: up, wait, then down.
    await holdAngle(page, 40);
    fakeNow += 400;
    await holdAngle(page, 170);
    await expect(page.locator('#left-reps')).toHaveText('1 / 10');

    // Back-to-back rep within MIN_REP_MS must not count.
    await holdAngle(page, 40);
    await holdAngle(page, 170);
    await expect(page.locator('#left-reps')).toHaveText('1 / 10');

    // Losing the arm for 10 frames resets a half-finished rep.
    await holdAngle(page, 40);
    for (let frame = 0; frame < 10; frame += 1) {
      await feedPose(page, null, null);
    }
    await holdAngle(page, 170);
    await expect(page.locator('#left-reps')).toHaveText('1 / 10');
  } finally {
    await restoreSettings(app);
    await quitApp(app);
  }
});

test('shows Training done after 10 reps per arm and returns to the countdown', async () => {
  const app = await launchDumbbellApp();

  try {
    const page = await showDumbbell(app);
    await waitForPoseTracker(page);

    for (let rep = 0; rep < 10; rep += 1) {
      await doRep(page);
    }

    await expect(page.locator('#overall')).toHaveText('Total 20 / 20');
    await expect(page.locator('#arm-left')).toHaveClass(/bg-green-100/);
    await expect(page.locator('#arm-right')).toHaveClass(/bg-green-100/);
    await expect(page.getByText('Training done')).toBeVisible();
    await expect
      .poll(() => app.windows().some((window) => window.url().endsWith('countdown.html')))
      .toBe(true);
  } finally {
    await restoreSettings(app);
    await quitApp(app);
  }
});
