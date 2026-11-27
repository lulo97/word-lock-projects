use serde_json::json;
use serde_json::Value;
use std::io::Read;
use std::time::Duration;

const TABLE: &[u8; 64] = b"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

fn base64(data: &[u8]) -> String {
    let mut out = String::with_capacity((data.len() + 2) / 3 * 4);
    for chunk in data.chunks(3) {
        let b0 = chunk[0] as usize;
        let b1 = chunk.get(1).copied().unwrap_or(0) as usize;
        let b2 = chunk.get(2).copied().unwrap_or(0) as usize;
        out.push(TABLE[b0 >> 2] as char);
        out.push(TABLE[((b0 & 0x03) << 4) | (b1 >> 4)] as char);
        if chunk.len() > 1 {
            out.push(TABLE[((b1 & 0x0f) << 2) | (b2 >> 6)] as char);
        } else {
            out.push('=');
        }
        if chunk.len() > 2 {
            out.push(TABLE[b2 & 0x3f] as char);
        } else {
            out.push('=');
        }
    }
    out
}

pub fn synthesize(text: &str, lang: &str) -> Value {
    let body = json!({ "input": text, "lang": lang, "voice": "female" }).to_string();
    let agent = ureq::AgentBuilder::new().timeout(Duration::from_secs(300)).build();

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
            json!({ "ok": true, "wav": base64(&bytes) })
        }
        Err(ureq::Error::Status(code, response)) => {
            let mut body = String::new();
            let _ = response.into_reader().read_to_string(&mut body);
            json!({ "ok": false, "error": format!("TTS server returned HTTP {}", code) })
        }
        Err(error) => json!({ "ok": false, "error": error.to_string() }),
    }
}
