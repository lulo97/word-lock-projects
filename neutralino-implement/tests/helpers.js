const path = require('node:path');
const { spawn, spawnSync } = require('node:child_process');
const { chromium } = require('@playwright/test');

const root = path.join(__dirname, '..');
const exe = path.join(root, 'dist', 'pomodorolock', 'pomodorolock-win_x64.exe');
const cdpPort = Number(process.env.WORDLOCK_CDP_PORT || 9333);
const cdpBase = `http://127.0.0.1:${cdpPort}`;

function sleep(ms) {
  return new Promise((resolve) => setTimeout(resolve, ms));
}

// Neutralino child windows are separate processes. Killing only the host can
// leave WebView2 children behind holding the shared CDP port.
function killProcessTree(child) {
  if (!child?.pid) return;
  const result = spawnSync('taskkill', ['/PID', String(child.pid), '/T', '/F'], {
    stdio: 'ignore',
    windowsHide: true,
  });
  if (result.status !== 0) {
    try { child.kill(); } catch { /* already gone */ }
  }
}

async function waitForCdp(timeoutMs = 30000) {
  const started = Date.now();
  let lastError = null;
  while (Date.now() - started < timeoutMs) {
    try {
      const response = await fetch(`${cdpBase}/json/version`);
      if (response.ok) return response.json();
    } catch (error) {
      lastError = error;
    }
    await sleep(200);
  }
  throw new Error(`Neutralino CDP did not start on ${cdpBase}: ${lastError?.message || 'timeout'}`);
}

function browserArguments(extra = []) {
  return [
    `--remote-debugging-port=${cdpPort}`,
    '--use-fake-device-for-media-stream',
    '--use-fake-ui-for-media-stream',
    ...extra,
  ].join(' ');
}

async function launchApp({ env = {}, extraArgs = [] } = {}) {
  const child = spawn(exe, [], {
    cwd: path.dirname(exe),
    env: {
      ...process.env,
      WORDLOCK_USE_TEST_DATA: '1',
      WORDLOCK_CHROME_FREEZE_MOCK: '1',
      WORDLOCK_USE_MOCK_AUTO_START: '1',
      WEBVIEW2_ADDITIONAL_BROWSER_ARGUMENTS: browserArguments(extraArgs),
      ...env,
    },
    stdio: ['ignore', 'pipe', 'pipe'],
  });

  let logs = '';
  child.stdout.on('data', (chunk) => { logs += chunk; });
  child.stderr.on('data', (chunk) => { logs += chunk; });

  try {
    await waitForCdp();
    const browser = await chromium.connectOverCDP(cdpBase);

    const app = {
      child,
      browser,
      logs: () => logs,
      windows() {
        return browser.contexts()
          .flatMap((context) => context.pages())
          .filter((page) => {
            const url = page.url();
            return url.startsWith('http://') || url.startsWith('https://');
          });
      },
      host() {
        return this.windows().find((page) => page.url().includes('/src/host/host.html'));
      },
      async waitHost() {
        const started = Date.now();
        while (Date.now() - started < 30000) {
          const page = this.host();
          if (page) {
            try {
              await page.waitForFunction(() => Boolean(window.__host), null, { timeout: 1000 });
              return page;
            } catch {
              // Host page exists but boot() has not installed the hooks yet.
            }
          }
          await sleep(150);
        }
        throw new Error(`window.__host was not ready; app logs:\n${logs}`);
      },
      async evaluate(fn, arg) {
        const page = await this.waitHost();
        return page.evaluate(fn, arg);
      },
      async call(route, ...args) {
        const page = await this.waitHost();
        return page.evaluate(({ route, args }) => {
          const parts = route.split('.');
          let target = window.__host;
          for (const part of parts) {
            target = target?.[part];
          }
          if (typeof target !== 'function') throw new Error(`window.__host.${route} is not a function`);
          return target.apply(window.__host, args);
        }, { route, args });
      },
      async randomWords(language, times = 120) {
        const page = await this.waitHost();
        return page.evaluate(async ({ language, times }) => {
          const select = language === 'english'
            ? window.electronAPI.getRandomWord
            : window.electronAPI.getRandomJapaneseWord;
          const rows = [];
          for (let i = 0; i < times; i += 1) rows.push(await select());
          return rows;
        }, { language, times });
      },
      async quit() {
        const page = this.host();
        if (page) {
          await page.evaluate(() => window.__host?.quit()).catch(() => {});
        }
        await sleep(300);
        if (child.exitCode === null && child.signalCode === null) killProcessTree(child);
        await browser.close().catch(() => {});
        await sleep(150);
      },
    };

    await app.waitHost();
    return app;
  } catch (error) {
    killProcessTree(child);
    throw error;
  }
}

async function closeApp(app) {
  await app.quit();
}

async function waitForWindow(app, suffix, timeout = 15000) {
  const started = Date.now();
  while (Date.now() - started < timeout) {
    const page = app.windows().find((candidate) => candidate.url().endsWith(suffix));
    if (page) return page;
    await sleep(150);
  }
  throw new Error(`No window with URL ending ${suffix}`);
}

async function windowGone(app, suffix, timeout = 15000) {
  const started = Date.now();
  while (Date.now() - started < timeout) {
    if (!app.windows().some((page) => page.url().endsWith(suffix))) return;
    await sleep(150);
  }
  throw new Error(`Window with URL ending ${suffix} is still open`);
}

async function hostDbExec(app, sql) {
  return app.call('native.call', 'db.exec', { sql });
}

module.exports = {
  launchApp,
  closeApp,
  waitForWindow,
  windowGone,
  hostDbExec,
  sleep,
  cdpPort,
};
