const { test, expect } = require('@playwright/test');
const { launchApp, closeApp } = require('./helpers');

test('synthesize reports an unavailable local TTS server instead of throwing a raw error', async () => {
  const app = await launchApp();

  try {
    const result = await app.evaluate(async () => {
      try {
        const wav = await window.__host.tts.synthesize('hello');
        return { ok: true, wav };
      } catch (error) {
        return { ok: false, error: error.message };
      }
    });

    expect(result.ok).toBe(false);
    expect(result.error).toContain('TTS server is not available at 127.0.0.1:8080');
    expect(result.error).toContain('ECONNREFUSED');
  } finally {
    await closeApp(app);
  }
});
