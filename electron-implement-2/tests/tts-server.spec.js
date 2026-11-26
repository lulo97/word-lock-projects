const path = require('node:path');
const { test, expect, _electron: electron } = require('@playwright/test');

const projectDir = path.join(__dirname, '..');
const ttsPath = path.join(projectDir, 'src', 'tts', 'tts.js');

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

test('synthesize reports an unavailable llama.cpp server instead of throwing a raw error', async () => {
  const app = await launchApp();

  try {
    const result = await app.evaluate(async (_electron, modulePath) => {
      const http = process.mainModule.require('node:http');
      const { EventEmitter } = process.mainModule.require('node:events');
      const originalRequest = http.request;

      http.request = () => {
        const request = new EventEmitter();
        request.setTimeout = () => request;
        request.end = () => {
          process.nextTick(() => {
            const error = new Error('connect ECONNREFUSED 127.0.0.1:8080');
            error.code = 'ECONNREFUSED';
            request.emit('error', error);
          });
        };
        return request;
      };

      try {
        const wav = await process.mainModule.require(modulePath).synthesize('hello');
        return { ok: true, wav };
      } catch (error) {
        return { ok: false, error: error.message };
      } finally {
        http.request = originalRequest;
      }
    }, ttsPath);

    expect(result.ok).toBe(false);
    expect(result.error).toContain('TTS server is not available at 127.0.0.1:8080');
    expect(result.error).toContain('ECONNREFUSED');
  } finally {
    await closeApp(app);
  }
});
