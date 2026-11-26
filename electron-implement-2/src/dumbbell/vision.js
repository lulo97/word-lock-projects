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

const SYSTEM_PROMPT_HOLD = `
Analyze the input webcam frame.
1. Check if a person is visible in the frame.
2. If a person is visible, check if they are holding a dumbbell.

Respond with a valid JSON object only, with no markdown or extra text:
{"status": "NONE" | "HOLD", "message": "string"}

Examples:
- {"status": "NONE", "message": "No person visible in the frame."}
- {"status": "NONE", "message": "Person is visible, but not holding a dumbbell."}
- {"status": "HOLD", "message": "Person is holding a dumbbell."}
`;

const SYSTEM_PROMPT_UP = `
Analyze the input webcam frame.
1. Check if a person is visible in the frame.
2. If a person is visible, check if they are holding a dumbbell.
3. If holding a dumbbell, determine its vertical position:
   - "UP": The dumbbell is raised to shoulder height or higher (near the shoulders or head).
   - "DOWN": The dumbbell is lowered below shoulder height (near the chest, waist, hips, or legs).

Respond with a valid JSON object only, with no markdown or extra text:
{"status": "NONE" | "UP" | "DOWN", "message": "string"}

Examples:
- {"status": "NONE", "message": "No person visible in the frame."}
- {"status": "NONE", "message": "Person is visible, but not holding a dumbbell."}
- {"status": "DOWN", "message": "Person is holding a dumbbell in the low position."}
- {"status": "UP", "message": "Person is holding a dumbbell in the high position."}
`;

const SYSTEM_PROMPT_DOWN = `
Analyze the input webcam frame.
1. Check if a person is visible in the frame.
2. If a person is visible, check if they are holding a dumbbell.
3. If holding a dumbbell, determine its vertical position:
   - "UP": The dumbbell is raised to shoulder height or higher (near the shoulders or head).
   - "DOWN": The dumbbell is lowered below shoulder height (near the chest, waist, hips, or legs).

Respond with a valid JSON object only, with no markdown or extra text:
{"status": "NONE" | "UP" | "DOWN", "message": "string"}

Examples:
- {"status": "NONE", "message": "No person visible in the frame."}
- {"status": "NONE", "message": "Person is visible, but not holding a dumbbell."}
- {"status": "DOWN", "message": "Person is holding a dumbbell in the low position."}
- {"status": "UP", "message": "Person is holding a dumbbell in the high position."}
`;

const PROMPTS = {
  hold: SYSTEM_PROMPT_HOLD,
  up: SYSTEM_PROMPT_UP,
  down: SYSTEM_PROMPT_DOWN,
};

const STATUS_LABELS = {
  NONE: LABELS.none,
  HOLD: LABELS.hold,
  UP: LABELS.up,
  DOWN: LABELS.down,
};

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

function parseJsonContent(content) {
  const text = String(content ?? '').trim();
  const candidates = [text];

  const fenced = text.match(/```(?:json)?\s*([\s\S]*?)```/i);
  if (fenced) candidates.push(fenced[1].trim());

  const firstBrace = text.indexOf('{');
  const lastBrace = text.lastIndexOf('}');
  if (firstBrace !== -1 && lastBrace > firstBrace) {
    candidates.push(text.slice(firstBrace, lastBrace + 1));
  }

  for (const candidate of candidates) {
    if (!candidate) continue;
    try {
      const parsed = JSON.parse(candidate);
      if (parsed && typeof parsed === 'object' && !Array.isArray(parsed)) {
        return parsed;
      }
    } catch {
      // Try the next candidate.
    }
  }

  return null;
}

function parseVisionResponse(content) {
  const text = String(content ?? '').trim();
  const json = parseJsonContent(text);

  if (json) {
    const status = String(json.status ?? '').trim().toUpperCase();
    const message = String(json.message ?? json.status ?? '').trim();

    return {
      status,
      message,
      label: STATUS_LABELS[status] ?? normalizeLabel(message || text),
    };
  }

  return { status: '', message: text, label: normalizeLabel(text) };
}

function analyzeFrame(dataUrl, stage = 'hold') {
  const systemPrompt = PROMPTS[stage] ?? SYSTEM_PROMPT_HOLD;
  const body = JSON.stringify({
    model: 'LFM3-Omni-2B-Q8_0.gguf',
    messages: [
      { role: 'system', content: systemPrompt },
      {
        role: 'user',
        content: [
          { type: 'text', text: 'Classify this frame.' },
          { type: 'image_url', image_url: { url: dataUrl } },
        ],
      },
    ],
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
            const content = payload.choices?.[0]?.message?.content ?? '';
            const { message, label, status } = parseVisionResponse(content);
            resolve({ raw: message, label, status });
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

module.exports = { analyzeFrame, getStatus, LABELS, normalizeLabel, parseVisionResponse };
