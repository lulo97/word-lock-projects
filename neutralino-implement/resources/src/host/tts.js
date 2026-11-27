import { getSettings } from './settings.js';
import { native } from './native.js';

const TTS_UNAVAILABLE_MESSAGE = 'TTS server is not available at 127.0.0.1:8080.';

function toBytes(base64) {
  const binary = atob(base64);
  const bytes = new Uint8Array(binary.length);
  for (let i = 0; i < binary.length; i += 1) bytes[i] = binary.charCodeAt(i);
  return bytes;
}

function toBase64(bytes) {
  let binary = '';
  for (let i = 0; i < bytes.length; i += 1) binary += String.fromCharCode(bytes[i]);
  return btoa(binary);
}

function ascii(bytes, start, end) {
  let out = '';
  for (let i = start; i < end; i += 1) out += String.fromCharCode(bytes[i]);
  return out;
}

// Port of the old Node Buffer implementation: scale 16-bit PCM in place,
// clipped to the Int16 range. Unknown/non-PCM WAVs pass through untouched.
export function applyGain(base64, gain) {
  const volume = Number(gain);
  if (!Number.isFinite(volume) || volume === 1) return base64;

  const wav = toBytes(base64);
  if (
    wav.length < 12 ||
    ascii(wav, 0, 4) !== 'RIFF' ||
    ascii(wav, 8, 12) !== 'WAVE'
  ) {
    return base64;
  }

  const view = new DataView(wav.buffer, wav.byteOffset, wav.byteLength);
  let format = null;
  let bitsPerSample = null;
  let dataOffset = null;
  let dataSize = null;

  let offset = 12;
  while (offset + 8 <= wav.length) {
    const id = ascii(wav, offset, offset + 4);
    const size = view.getUint32(offset + 4, true);
    const start = offset + 8;

    if (id === 'fmt ' && size >= 16 && start + 16 <= wav.length) {
      format = view.getUint16(start, true);
      bitsPerSample = view.getUint16(start + 14, true);
    }
    if (id === 'data') {
      dataOffset = start;
      dataSize = size;
      break;
    }

    offset = start + size + (size % 2);
  }

  if (format !== 1 || bitsPerSample !== 16 || dataOffset === null) return base64;

  const end = Math.min(dataOffset + dataSize, wav.length);
  for (let i = dataOffset; i + 1 < end; i += 2) {
    const sample = Math.max(-32768, Math.min(32767, Math.round(view.getInt16(i, true) * volume)));
    view.setInt16(i, sample, true);
  }

  return toBase64(wav);
}

export async function synthesize(text, lang = 'en') {
  const gain = getSettings().masterVolume;
  const result = await native.call('tts.synthesize', { text, lang }, 300000);
  const base64 = typeof result === 'string' ? result : result?.ok ? result.wav : null;
  if (!base64) {
    const detail = result?.error || '';
    if (/refused|10061|ECONNREFUSED/i.test(detail)) {
      throw new Error(`${TTS_UNAVAILABLE_MESSAGE} (ECONNREFUSED 127.0.0.1:8080)`);
    }
    throw new Error(detail || TTS_UNAVAILABLE_MESSAGE);
  }
  return applyGain(base64, gain);
}

export { TTS_UNAVAILABLE_MESSAGE };
