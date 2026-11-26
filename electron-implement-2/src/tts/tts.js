const http = require('node:http');

const TTS_HOST = '127.0.0.1';
const TTS_PORT = 8080;
const TTS_PATH = '/tts';

function synthesize(text, lang = 'en') {
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
        response.on('end', () => resolve(Buffer.concat(chunks).toString('base64')));
      },
    );

    request.on('error', reject);
    request.setTimeout(300000, () => request.destroy(new Error('TTS request timed out')));
    request.end(body);
  });
}

module.exports = { synthesize };
