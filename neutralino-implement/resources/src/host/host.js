import { native, waitReady, configureMocks, getMocks } from './native.js';
import { initSettings, getSettings as readSettings } from './settings.js';
import { initThemes } from './themes.js';
import { createRouter } from './ipc.js';
import { showCountdown, showMainWindow, hideMainWindow, createSettingsWindow, quitAll } from './windows.js';
import { createTray } from './tray.js';
import { initializeChromeFreeze, unfreezeChromeOnExit } from './chromeFreeze.js';
import { initializeAutoStart } from './autoStart.js';

async function env(key) {
  try { return await Neutralino.os.getEnv(key); } catch { return ''; }
}

async function boot() {
  // The bridge already called Neutralino.init() before this module ran.
  await window.__bridgeReady;

  const testData = (await env('WORDLOCK_USE_TEST_DATA')) === '1';

  if ((await env('WORDLOCK_CHROME_FREEZE_MOCK')) === '1') {
    const mock = await import('./mocks/chromeFreeze.mock.js');
    mock.setLogPath(await env('WORDLOCK_CHROME_FREEZE_LOG'));
    configureMocks({ chrome: mock.mock });
  }
  if ((await env('WORDLOCK_USE_MOCK_AUTO_START')) === '1') {
    const mock = await import('./mocks/autoStart.mock.js');
    configureMocks({ autostart: mock.mock });
  }

  // The extension is a separate process; wait for its ping before any RPC.
  await waitReady();

  // Single instance: a second process must never fight over frozen Chrome.
  // Test runs use in-memory state and deliberately skip the named mutex.
  if (!testData) {
    const lock = await native.call('lock.acquire');
    if (lock && lock.ok === false && lock.error === 'already-running') {
      Neutralino.app.broadcast('second-instance', {});
      Neutralino.app.exit();
      return;
    }
    // A timeout or RPC error is not proof of a second instance: keep starting.
  }

  await initThemes();
  await initSettings();
  // The iframe's theme-boot.js reads this before its first paint.
  window.electronAPI.themeId = readSettings().theme;
  await native.call('db.open', { test: testData });

  const router = createRouter();

  Neutralino.events.on('second-instance', () => { void showMainWindow(); });
  Neutralino.events.on('windowClose', () => { /* host lives in the tray */ });

  await Neutralino.window.setDraggableRegion('drag');
  await initializeChromeFreeze();
  void initializeAutoStart();

  await showCountdown();
  // Headless tests must not get a native modal if tray setup is unavailable.
  await createTray({ visibleFallback: !testData });

  if (testData) {
    const settings = await import('./settings.js');
    const windows = await import('./windows.js');
    const tts = await import('./tts.js');
    window.__host = {
      settings: {
        getSettings: settings.getSettings,
        setSettings: settings.setSettings,
        getIgnoreWords: settings.getIgnoreWords,
        setIgnoreWords: settings.setIgnoreWords,
        parseIgnoreWords: settings.parseIgnoreWords,
        normalizeVolume: settings.normalizeVolume,
        advanceTheme: settings.advanceTheme,
      },
      windows: {
        showMainWindow,
        hideMainWindow,
        showCountdown,
        createSettingsWindow,
        isAlive: windows.isAlive,
      },
      tts: {
        applyGain: tts.applyGain,
        synthesize: tts.synthesize,
      },
      native,
      nativeMock: getMocks(),
      router,
      initializeAutoStart,
      initializeChromeFreeze,
      quit: quitAll,
      unfreezeChromeOnExit,
      call: (method, params) => native.call(method, params),
    };
  }
}

boot().catch(async (error) => {
  console.error('PomodoroLock host failed to start:', error);
  await unfreezeChromeOnExit();
});
