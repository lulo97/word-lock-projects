const path = require('node:path');
const { test, expect, _electron: electron } = require('@playwright/test');

const projectDir = path.join(__dirname, '..');
const ttsPath = path.join(projectDir, 'src', 'tts', 'tts.js');
const settingsPath = path.join(projectDir, 'src', 'settings', 'settings.js');

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

test('master volume scales PCM samples, mutes at 0 and clips at 4', async () => {
  const app = await launchApp();

  try {
    const wav = makeWav([1000, -1000, 10000]);

    const result = await app.evaluate(
      (_electron, { modulePath, base64 }) => {
        const { applyGain } = process.mainModule.require(modulePath);
        const samplesFor = (gain) => {
          const buffer = Buffer.from(applyGain(base64, gain), 'base64');
          return [0, 1, 2].map((index) => buffer.readInt16LE(44 + index * 2));
        };

        return {
          original: samplesFor(1),
          muted: samplesFor(0),
          doubled: samplesFor(2),
          maxed: samplesFor(4),
          passthrough: applyGain('not a wav', 2),
        };
      },
      { modulePath: ttsPath, base64: wav },
    );

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
    const result = await app.evaluate((_electron, modulePath) => {
      const { normalizeVolume } = process.mainModule.require(modulePath);
      return [-1, 5, 1.3, 2.2, 'bad'].map(normalizeVolume);
    }, settingsPath);

    expect(result).toEqual([0, 4, 1.5, 2, 1]);
  } finally {
    await closeApp(app);
  }
});
