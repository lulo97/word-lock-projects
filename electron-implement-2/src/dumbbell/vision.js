const http = require('node:http');

const VISION_HOST = '127.0.0.1';
const VISION_PORT = 8080;
const VISION_PATH = '/v1/chat/completions';

const LABELS = {
  hold: 'hold dumbbell',
  up: 'up dumbbell',
  down: 'down dumbbell',
  none: 'no hold dumbbell',
};

const SYSTEM_PROMPT = [
  'You classify a dumbbell workout webcam frame.',
  'Reply with exactly one of these labels and nothing else:',
  '"hold dumbbell", "up dumbbell", "down dumbbell", "no hold dumbbell".',
  '"hold dumbbell" = the person holds a dumbbell down at the starting position.',
  '"up dumbbell" = the dumbbell is lifted up.',
  '"down dumbbell" = the dumbbell is lowered back down while still held.',
  '"no hold dumbbell" = the person is not holding a dumbbell.',
].join(' ');

function normalizeLabel(content) {
  const text = String(content).toLowerCase().trim();

  if (text.includes('no hold') || text.includes('not hold') || text.includes('without dumbbell')) {
    return LABELS.none;
  }
  if (text.includes('down')) return LABELS.down;
  if (text.includes('up')) return LABELS.up;
  if (text.includes('hold') || text.includes('dumbbell')) return LABELS.hold;

  return '';
}

function analyzeFrame(dataUrl) {
  const body = JSON.stringify({
    model: 'LFM3-Omni-2B-Q8_0.gguf',
    messages: [
      { role: 'system', content: SYSTEM_PROMPT },
      {
        role: 'user',
        content: [
          { type: 'text', text: 'Classify this frame.' },
          { type: 'image_url', image_url: { url: dataUrl } },
        ],
      },
    ],
    max_tokens: 32,
    temperature: 0,
  });

  return new Promise((resolve, reject) => {
    const request = http.request(
      {
        hostname: VISION_HOST,
        port: VISION_PORT,
        path: VISION_PATH,
        method: 'POST',
        headers: {
          'Content-Type': 'application/json',
          'Content-Length': Buffer.byteLength(body),
        },
      },
      (response) => {
        if (response.statusCode < 200 || response.statusCode >= 300) {
          response.resume();
          reject(new Error(`Vision request failed with HTTP ${response.statusCode}`));
          return;
        }

        const chunks = [];
        response.on('data', (chunk) => chunks.push(chunk));
        response.on('end', () => {
          try {
            const payload = JSON.parse(Buffer.concat(chunks).toString('utf8'));
            const raw = payload.choices?.[0]?.message?.content ?? '';
            resolve({ raw, label: normalizeLabel(raw) });
          } catch (error) {
            reject(error);
          }
        });
      },
    );

    request.on('error', reject);
    request.setTimeout(300000, () => request.destroy(new Error('Vision request timed out')));
    request.end(body);
  });
}

function getStatus() {
  return new Promise((resolve) => {
    const request = http.get(
      { hostname: VISION_HOST, port: VISION_PORT, path: '/health', timeout: 3000 },
      (response) => {
        response.resume();
        resolve(response.statusCode >= 200 && response.statusCode < 300);
      },
    );

    request.on('timeout', () => request.destroy());
    request.on('error', () => resolve(false));
  });
}

module.exports = { analyzeFrame, getStatus, LABELS, normalizeLabel };
