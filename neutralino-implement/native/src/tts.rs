use serde_json::{json, Value};
use std::env;
use std::fs;
use std::io::Read;
use std::path::PathBuf;
use std::time::{Duration, SystemTime, UNIX_EPOCH};

fn read_u16(bytes: &[u8], offset: usize) -> Option<u16> {
    let slice = bytes.get(offset..offset + 2)?;
    Some(u16::from_le_bytes([slice[0], slice[1]]))
}

fn read_u32(bytes: &[u8], offset: usize) -> Option<u32> {
    let slice = bytes.get(offset..offset + 4)?;
    Some(u32::from_le_bytes([slice[0], slice[1], slice[2], slice[3]]))
}

// Same rule as the old Node applyGain(): scale 16-bit PCM in place and clip to
// Int16. Unknown/non-PCM WAVs pass through untouched.
fn apply_gain(mut bytes: Vec<u8>, gain: f64) -> Vec<u8> {
    if !gain.is_finite() || gain == 1.0 {
        return bytes;
    }
    if bytes.len() < 12 || &bytes[0..4] != b"RIFF" || &bytes[8..12] != b"WAVE" {
        return bytes;
    }

    let mut format = None;
    let mut bits_per_sample = None;
    let mut data_offset = None;
    let mut data_size = 0usize;

    let mut offset = 12usize;
    while offset + 8 <= bytes.len() {
        let size = match read_u32(&bytes, offset + 4) {
            Some(size) => size as usize,
            None => break,
        };
        let start = offset + 8;

        if &bytes[offset..offset + 4] == b"fmt " && size >= 16 && start + 16 <= bytes.len() {
            format = read_u16(&bytes, start);
            bits_per_sample = read_u16(&bytes, start + 14);
        }
        if &bytes[offset..offset + 4] == b"data" {
            data_offset = Some(start);
            data_size = size;
            break;
        }

        let next = start + size + (size % 2);
        if next <= offset {
            break;
        }
        offset = next;
    }

    if format != Some(1) || bits_per_sample != Some(16) {
        return bytes;
    }
    let start = match data_offset {
        Some(start) => start,
        None => return bytes,
    };
    let end = start.saturating_add(data_size).min(bytes.len());

    for index in (start..end).step_by(2) {
        if index + 1 >= end {
            break;
        }
        let sample = i16::from_le_bytes([bytes[index], bytes[index + 1]]);
        let scaled = (sample as f64 * gain).round().clamp(-32768.0, 32767.0) as i16;
        let [low, high] = scaled.to_le_bytes();
        bytes[index] = low;
        bytes[index + 1] = high;
    }

    bytes
}

fn temp_path() -> PathBuf {
    let nanos = SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .unwrap_or_default()
        .as_nanos();
    env::temp_dir().join(format!("wordlock-tts-{}-{}.wav", std::process::id(), nanos))
}

pub fn synthesize(text: &str, lang: &str, gain: f64) -> Value {
    let body = json!({ "input": text, "lang": lang, "voice": "female" }).to_string();
    let agent = ureq::AgentBuilder::new()
        .timeout(Duration::from_secs(300))
        .build();

    let response = agent
        .post("http://127.0.0.1:8080/tts")
        .set("Content-Type", "application/json")
        .send_string(&body);

    match response {
        Ok(response) => {
            let mut bytes = Vec::new();
            if let Err(error) = response.into_reader().read_to_end(&mut bytes) {
                return json!({ "ok": false, "error": error.to_string() });
            }

            let bytes = apply_gain(bytes, gain);
            let path = temp_path();
            match fs::write(&path, &bytes) {
                Ok(()) => json!({ "ok": true, "path": path.to_string_lossy().into_owned() }),
                Err(error) => json!({ "ok": false, "error": error.to_string() }),
            }
        }
        Err(ureq::Error::Status(code, response)) => {
            let mut body = String::new();
            let _ = response.into_reader().read_to_string(&mut body);
            json!({ "ok": false, "error": format!("TTS server returned HTTP {}", code) })
        }
        Err(error) => json!({ "ok": false, "error": error.to_string() }),
    }
}
