const { test, expect } = require('@playwright/test');
const { launchApp, closeApp } = require('./helpers');

function makeWav(samples) {
  const sampleRate = 8000;
  const dataSize = samples.length * 2;
  const buffer = Buffer.alloc(44 + dataSize);

  buffer.write('RIFF', 0);
  buffer.writeUInt32LE(36 + dataSize, 4);
  buffer.write('WAVE', 8);
  buffer.write('fmt ', 12);
  buffer.writeUInt32LE(16, 16);
  buffer.writeUInt16LE(1, 20);
  buffer.writeUInt16LE(1, 22);
  buffer.writeUInt32LE(sampleRate, 24);
  buffer.writeUInt32LE(sampleRate * 2, 28);
  buffer.writeUInt16LE(2, 32);
  buffer.writeUInt16LE(16, 34);
  buffer.write('data', 36);
  buffer.writeUInt32LE(dataSize, 40);

  samples.forEach((sample, index) => buffer.writeInt16LE(sample, 44 + index * 2));
  return buffer.toString('base64');
}

test('master volume scales PCM samples, mutes at 0 and clips at 4', async () => {
  const app = await launchApp();

  try {
    const wav = makeWav([1000, -1000, 10000]);

    const result = await app.evaluate(async ({ base64 }) => {
      const samplesFor = async (gain) => {
        const changed = await window.__host.tts.applyGain(base64, gain);
        const binary = atob(changed);
        const view = new DataView(
          Uint8Array.from(binary, (char) => char.charCodeAt(0)).buffer,
        );
        return [0, 1, 2].map((index) => view.getInt16(44 + index * 2, true));
      };

      return {
        original: await samplesFor(1),
        muted: await samplesFor(0),
        doubled: await samplesFor(2),
        maxed: await samplesFor(4),
        passthrough: await window.__host.tts.applyGain('not a wav', 2),
      };
    }, { base64: wav });

    expect(result).toEqual({
      original: [1000, -1000, 10000],
      muted: [0, 0, 0],
      doubled: [2000, -2000, 20000],
      maxed: [4000, -4000, 32767],
      passthrough: 'not a wav',
    });
  } finally {
    await closeApp(app);
  }
});

test('master volume is clamped and rounded to 0.5 steps', async () => {
  const app = await launchApp();

  try {
    const values = [-1, 5, 1.3, 2.2, 'bad'];
    const result = [];
    for (const value of values) {
      result.push(await app.call('settings.normalizeVolume', value));
    }
    expect(result).toEqual([0, 4, 1.5, 2, 1]);
  } finally {
    await closeApp(app);
  }
});
