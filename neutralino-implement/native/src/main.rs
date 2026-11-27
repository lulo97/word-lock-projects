mod autostart;
mod chrome;
mod db;
mod system;
mod tts;

use serde_json::{json, Map, Value};
use std::env;
use std::io::{self, BufRead};
use std::net::TcpStream;
use std::sync::atomic::{AtomicBool, AtomicU64, Ordering};
use std::sync::{Arc, Mutex};
use std::thread;
use std::time::Duration;
use tungstenite::{client::client, Message, WebSocket};

struct AppState {
    db: Option<db::Database>,
    chrome: chrome::ChromeFreeze,
    refreeze_on: bool,
}

static NEXT_ID: AtomicU64 = AtomicU64::new(1);

// Outbound WebSocket ids only need to be unique for this one connection.
fn make_id() -> String {
    NEXT_ID.fetch_add(1, Ordering::Relaxed).to_string()
}

fn test_mode() -> bool {
    env::var("WORDLOCK_USE_TEST_DATA")
        .map(|value| value == "1")
        .unwrap_or(false)
}

fn dispatch(method: &str, params: &Value, state: &mut AppState) -> Value {
    match method {
        "native.ping" => json!({ "ok": true, "pong": true }),

        "lock.acquire" => system::acquire_lock(),

        "db.open" => {
            let test = params.get("test").and_then(Value::as_bool).unwrap_or(false);
            match db::Database::open(test) {
                Ok(database) => {
                    state.db = Some(database);
                    json!({ "ok": true })
                }
                Err(error) => json!({ "ok": false, "error": error }),
            }
        }
        // Arbitrary SQL is only needed by the Playwright fixtures. Keep it
        // behind the test env so a compromised renderer cannot dispatch it.
        "db.exec" if !test_mode() => json!({ "ok": false, "error": "db.exec is disabled" }),
        "db.exec" => match state.db.as_ref() {
            Some(database) => database
                .call(method, params)
                .unwrap_or_else(|error| json!({ "ok": false, "error": error })),
            None => json!({ "ok": false, "error": "database not open" }),
        },
        m if m.starts_with("db.") => match state.db.as_ref() {
            Some(database) => database
                .call(m, params)
                .unwrap_or_else(|error| json!({ "ok": false, "error": error })),
            None => json!({ "ok": false, "error": "database not open" }),
        },

        "chrome.freeze" => state.chrome.freeze_all(),
        "chrome.unfreeze" => state.chrome.unfreeze_all(),
        "chrome.resumeAll" => state.chrome.resume_all(),
        "chrome.refreeze" => {
            state.refreeze_on = params.get("on").and_then(Value::as_bool).unwrap_or(false);
            json!({ "ok": true })
        }

        "autostart.get" => autostart::get_state(),
        "autostart.set" => autostart::set(
            params
                .get("enabled")
                .and_then(Value::as_bool)
                .unwrap_or(false),
        ),
        "autostart.sync" => autostart::sync(
            params
                .get("wants")
                .and_then(Value::as_bool)
                .unwrap_or(false),
        ),

        "screen.workArea" => system::work_area(),
        "window.setOpacity" => {
            let title = params
                .get("title")
                .and_then(Value::as_str)
                .unwrap_or("PomodoroLock Countdown");
            let alpha = params.get("alpha").and_then(Value::as_f64).unwrap_or(0.5);
            system::set_opacity(title, alpha)
        }

        _ => json!({ "ok": false, "error": format!("unknown method: {}", method) }),
    }
}

fn handle_text(
    text: String,
    state: &Arc<Mutex<AppState>>,
    socket: &mut WebSocket<TcpStream>,
    token: &str,
) {
    let message: Value = match serde_json::from_str(&text) {
        Ok(value) => value,
        Err(_) => return,
    };
    if message.get("event").and_then(Value::as_str) != Some("rpc") {
        return;
    }

    let data = message.get("data").cloned().unwrap_or_else(|| json!({}));
    let rpc_id = data.get("id").cloned().unwrap_or(Value::Null);
    let method = data.get("method").and_then(Value::as_str).unwrap_or("");
    let params = data.get("params").cloned().unwrap_or_else(|| json!({}));

    // TTS can take minutes. Never hold the AppState mutex while it runs:
    // Chrome's re-freeze loop and the parent watchdog must stay responsive.
    let mut result = if method == "tts.synthesize" {
        let text = params.get("text").and_then(Value::as_str).unwrap_or("");
        let lang = params.get("lang").and_then(Value::as_str).unwrap_or("en");
        let gain = params.get("gain").and_then(Value::as_f64).unwrap_or(1.0);
        tts::synthesize(text, lang, gain)
    } else {
        let mut locked = match state.lock() {
            Ok(guard) => guard,
            Err(poisoned) => poisoned.into_inner(),
        };
        dispatch(method, &params, &mut locked)
    };

    if result.get("ok").is_none() {
        let mut object = Map::new();
        object.insert("ok".to_string(), json!(true));
        if let Value::Object(existing) = result {
            for (key, value) in existing {
                object.insert(key, value);
            }
        }
        result = Value::Object(object);
    }
    if let Value::Object(ref mut object) = result {
        object.insert("id".to_string(), rpc_id);
    }

    let response = json!({
        "id": make_id(),
        "method": "app.broadcast",
        "accessToken": token,
        "data": {
            "event": "native:res",
            "data": result
        }
    });

    let _ = socket.send(Message::Text(response.to_string().into()));
}

fn main() {
    // Neutralino writes the connection details as one JSON line on stdin.
    let mut handshake = String::new();
    if io::stdin().lock().read_line(&mut handshake).unwrap_or(0) == 0 {
        return;
    }
    let info: Value = match serde_json::from_str(handshake.trim()) {
        Ok(value) => value,
        Err(error) => {
            eprintln!("wordlock-native: bad handshake: {}", error);
            return;
        }
    };

    // Neutralino sends nlPort as a string in some releases and a number in others.
    let port = info
        .get("nlPort")
        .and_then(|value| {
            value
                .as_u64()
                .or_else(|| value.as_str().and_then(|text| text.parse::<u64>().ok()))
        })
        .unwrap_or(0) as u16;
    let token = info
        .get("nlToken")
        .and_then(Value::as_str)
        .unwrap_or("")
        .to_string();
    let connect_token = info
        .get("nlConnectToken")
        .and_then(Value::as_str)
        .unwrap_or("")
        .to_string();
    let extension_id = info
        .get("nlExtensionId")
        .and_then(Value::as_str)
        .unwrap_or("")
        .to_string();

    if port == 0 || token.is_empty() || connect_token.is_empty() {
        eprintln!("wordlock-native: incomplete handshake");
        return;
    }

    let stream = match TcpStream::connect(("127.0.0.1", port)) {
        Ok(stream) => stream,
        Err(error) => {
            eprintln!("wordlock-native: cannot connect: {}", error);
            return;
        }
    };
    let url = format!(
        "ws://127.0.0.1:{}/?extensionId={}&connectToken={}",
        port, extension_id, connect_token
    );
    let (mut socket, _) = match client(url, stream) {
        Ok(pair) => pair,
        Err(error) => {
            eprintln!("wordlock-native: websocket handshake failed: {}", error);
            return;
        }
    };

    // Apply the poll timeout only after the handshake succeeded. Setting it
    // earlier can turn a slow-but-valid upgrade into an Interrupted error.
    if let Err(error) = socket
        .get_mut()
        .set_read_timeout(Some(Duration::from_millis(200)))
    {
        eprintln!("wordlock-native: set_read_timeout failed: {}", error);
        return;
    }

    // Each thread gets its own waitable handle to the same parent process.
    let parent_pid = system::parent_pid();
    let watchdog_parent = Arc::new(system::ParentProcess::open(parent_pid));
    let loop_parent = Arc::new(system::ParentProcess::open(parent_pid));

    let eof = Arc::new(AtomicBool::new(false));
    let state = Arc::new(Mutex::new(AppState {
        db: None,
        chrome: chrome::ChromeFreeze::new(),
        refreeze_on: false,
    }));

    // Any further stdin data is irrelevant, but EOF means the host is gone.
    let stdin_eof = eof.clone();
    thread::spawn(move || {
        let mut line = String::new();
        let mut input = io::stdin().lock();
        loop {
            line.clear();
            match input.read_line(&mut line) {
                Ok(0) | Err(_) => {
                    stdin_eof.store(true, Ordering::Relaxed);
                    break;
                }
                Ok(_) => {}
            }
        }
    });

    let watchdog_state = state.clone();
    let watchdog_eof = eof.clone();
    thread::spawn(move || loop {
        thread::sleep(Duration::from_secs(1));
        if watchdog_eof.load(Ordering::Relaxed) || (parent_pid != 0 && !watchdog_parent.is_alive())
        {
            let mut locked = watchdog_state.lock().unwrap_or_else(|e| e.into_inner());
            let _ = locked.chrome.unfreeze_all();
            std::process::exit(0);
        }
    });

    let refreeze_state = state.clone();
    let refreeze_eof = eof.clone();
    thread::spawn(move || loop {
        thread::sleep(Duration::from_secs(2));
        if refreeze_eof.load(Ordering::Relaxed) {
            break;
        }
        let mut locked = refreeze_state.lock().unwrap_or_else(|e| e.into_inner());
        if locked.refreeze_on {
            let _ = locked.chrome.freeze_all();
        }
    });

    loop {
        if eof.load(Ordering::Relaxed) || (parent_pid != 0 && !loop_parent.is_alive()) {
            break;
        }

        match socket.read() {
            Ok(Message::Text(text)) => handle_text(text, &state, &mut socket, &token),
            Ok(Message::Ping(payload)) => {
                let _ = socket.send(Message::Pong(payload));
            }
            Ok(Message::Close(_)) => break,
            Ok(_) => {}
            Err(tungstenite::Error::Io(ref error))
                if error.kind() == io::ErrorKind::WouldBlock
                    || error.kind() == io::ErrorKind::TimedOut => {}
            Err(error) => {
                eprintln!("wordlock-native: websocket read failed: {}", error);
                break;
            }
        }
    }

    // Hard-kill recovery: always give Chrome back before exiting.
    let mut locked = state.lock().unwrap_or_else(|e| e.into_inner());
    let _ = locked.chrome.unfreeze_all();
}
