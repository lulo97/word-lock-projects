const http = require('node:http');

const { getSettings } = require('../settings/settings');

const TTS_HOST = '127.0.0.1';
const TTS_PORT = 8080;
const TTS_PATH = '/tts';

// Gain is applied to the 16-bit PCM samples, so values above 1 can amplify.
// Samples are clipped to the Int16 range to avoid wrap-around distortion.
function applyGain(base64, gain) {
  const volume = Number(gain);
  if (!Number.isFinite(volume) || volume === 1) return base64;

  const wav = Buffer.from(base64, 'base64');
  if (
    wav.length < 12 ||
    wav.toString('ascii', 0, 4) !== 'RIFF' ||
    wav.toString('ascii', 8, 12) !== 'WAVE'
  ) {
    return base64;
  }

  let format = null;
  let bitsPerSample = null;
  let dataOffset = null;
  let dataSize = null;

  let offset = 12;
  while (offset + 8 <= wav.length) {
    const id = wav.toString('ascii', offset, offset + 4);
    const size = wav.readUInt32LE(offset + 4);
    const start = offset + 8;

    if (id === 'fmt ' && size >= 16 && start + 16 <= wav.length) {
      format = wav.readUInt16LE(start);
      bitsPerSample = wav.readUInt16LE(start + 14);
    }
    if (id === 'data') {
      dataOffset = start;
      dataSize = size;
      break;
    }

    offset = start + size + (size % 2);
  }

  // Unknown/non-PCM WAV layouts are passed through untouched.
  if (format !== 1 || bitsPerSample !== 16 || dataOffset === null) return base64;

  const end = Math.min(dataOffset + dataSize, wav.length);
  for (let i = dataOffset; i + 1 < end; i += 2) {
    const sample = Math.max(-32768, Math.min(32767, Math.round(wav.readInt16LE(i) * volume)));
    wav.writeInt16LE(sample, i);
  }

  return wav.toString('base64');
}

function synthesize(text, lang = 'en') {
  const gain = getSettings().masterVolume;
  const body = JSON.stringify({
    input: text,
    lang,
    voice: 'female',
  });

  return new Promise((resolve, reject) => {
    const request = http.request(
      {
        hostname: TTS_HOST,
        port: TTS_PORT,
        path: TTS_PATH,
        method: 'POST',
        headers: {
          'Content-Type': 'application/json',
          'Content-Length': Buffer.byteLength(body),
        },
      },
      (response) => {
        if (response.statusCode < 200 || response.statusCode >= 300) {
          response.resume();
          reject(new Error(`TTS request failed with HTTP ${response.statusCode}`));
          return;
        }

        const chunks = [];
        response.on('data', (chunk) => chunks.push(chunk));
        response.on('end', () => {
          try {
            const wav = Buffer.concat(chunks).toString('base64');
            resolve(applyGain(wav, gain));
          } catch (error) {
            reject(error);
          }
        });
      },
    );

    request.on('error', reject);
    request.setTimeout(300000, () => request.destroy(new Error('TTS request timed out')));
    request.end(body);
  });
}

module.exports = { synthesize, applyGain };
