const path = require('node:path');
const { test, expect, _electron: electron } = require('@playwright/test');

const projectDir = path.join(__dirname, '..');
const settingsPath = path.join(projectDir, 'src', 'settings', 'settings.js');
const windowPath = path.join(projectDir, 'src', 'main', 'createWindow.js');
const mockPath = path.join(projectDir, 'src', 'dumbbell', 'vision.mock.js');

async function launchDumbbellApp() {
  return electron.launch({
    executablePath: require('electron'),
    env: { ...process.env, DUMBBELL_VISION: 'mock', DUMBBELL_FRAME_MS: '10' },
    args: [
      path.join(projectDir, 'src', 'main.js'),
      '--use-fake-device-for-media-stream',
      '--use-fake-ui-for-media-stream',
    ],
  });
}

async function setOutputs(app, outputs) {
  await app.evaluate(
    (_electron, { modulePath, values }) => process.mainModule.require(modulePath).setOutputs(values),
    { modulePath: mockPath, values: outputs },
  );
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

test('renders a single dumbbell screen with 5 sets of 3 reps', async () => {
  const app = await launchDumbbellApp();

  try {
    const page = await showDumbbell(app);

    await expect(page.locator('#overall')).toHaveText('Step 1 / 15');
    await expect(page.locator('#required')).toHaveText(/Required: hold dumbbell/);
    await expect(page.locator('#seen')).toContainText('Last seen:');
    await expect(page.locator('#connection')).toHaveText('llama.cpp 8080 status: connected');
    await expect.poll(() => page.locator('#requests').textContent()).toMatch(/Requests sent: [1-9]/);
    await expect(page.locator('#avg')).toContainText('Avg request:');
    await expect.poll(() => page.locator('#recent li').count()).toBe(5);
    await expect(page.getByText('Set 1 / 5')).toBeVisible();
    await expect(page.getByText('Set 5 / 5')).toBeVisible();
    await expect(page.locator('#sets span')).toHaveCount(15);
    await expect(page.locator('#done')).toBeHidden();

    const asideOverflow = await page.locator('aside').evaluate((element) => ({
      scrollHeight: element.scrollHeight,
      clientHeight: element.clientHeight,
    }));
    expect(asideOverflow.scrollHeight).toBeLessThanOrEqual(asideOverflow.clientHeight + 1);

    const repTops = await page
      .locator('#sets > div')
      .first()
      .locator('span')
      .evaluateAll((elements) => elements.map((element) => Math.round(element.getBoundingClientRect().top)));
    expect(new Set(repTops).size).toBe(1);
  } finally {
    await restoreSettings(app);
    await quitApp(app);
  }
});

test('advances one rep after three continuous matching frames', async () => {
  const app = await launchDumbbellApp();

  try {
    await setOutputs(app, [
      'hold dumbbell',
      'hold dumbbell',
      'hold dumbbell',
      'up dumbbell',
      'up dumbbell',
      'up dumbbell',
      'down dumbbell',
      'down dumbbell',
      'down dumbbell',
    ]);

    const page = await showDumbbell(app);

    await expect(page.locator('#overall')).toHaveText('Step 4 / 15');
    await expect(page.locator('#required')).toHaveText(/Required: hold dumbbell/);
    await expect(page.locator('#seen')).toContainText('Last seen:');
  } finally {
    await restoreSettings(app);
    await quitApp(app);
  }
});

test('non-matching frames do not advance the rep', async () => {
  const app = await launchDumbbellApp();

  try {
    await setOutputs(app, ['no hold dumbbell', 'hold dumbbell', 'no hold dumbbell']);
    const page = await showDumbbell(app);

    await expect(page.locator('#overall')).toHaveText('Step 1 / 15');
    await expect(page.locator('#required')).toContainText('hold dumbbell');
    await expect(page.locator('#seen')).toContainText('no hold dumbbell');
  } finally {
    await restoreSettings(app);
    await quitApp(app);
  }
});

test('shows Trainning done after 15 reps and returns to the countdown', async () => {
  const app = await launchDumbbellApp();

  try {
    const stages = ['hold dumbbell', 'up dumbbell', 'down dumbbell'];
    const outputs = [];
    for (let step = 0; step < 15; step += 1) {
      outputs.push(stages[step % stages.length], stages[step % stages.length], stages[step % stages.length]);
    }
    await setOutputs(app, outputs);

    const page = await showDumbbell(app);

    await expect(page.getByText('Trainning done')).toBeVisible({ timeout: 15000 });
    await expect
      .poll(() => app.windows().some((window) => window.url().endsWith('countdown.html')))
      .toBe(true);
  } finally {
    await restoreSettings(app);
    await quitApp(app);
  }
});
