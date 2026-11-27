use serde_json::{json, Value};
use std::env;
use winreg::enums::{HKEY_CURRENT_USER, KEY_SET_VALUE};
use winreg::RegKey;

const RUN_PATH: &str = "Software\\Microsoft\\Windows\\CurrentVersion\\Run";
const APPROVED_PATH: &str = "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\StartupApproved\\Run";
const VALUE_NAME: &str = "PomodoroLock";

fn executable() -> String {
    let base = env::var("NL_PATH").unwrap_or_else(|_| {
        std::env::current_exe()
            .ok()
            .and_then(|p| p.parent().and_then(|parent| parent.parent()).map(|d| d.to_string_lossy().to_string()))
            .unwrap_or_default()
    });
    format!("{}\\pomodorolock.exe", base.trim_end_matches(['\\', '/']))
}

pub fn get_state() -> Value {
    let hkcu = RegKey::predef(HKEY_CURRENT_USER);

    let open_at_login = hkcu
        .open_subkey(RUN_PATH)
        .ok()
        .and_then(|run| run.get_value::<String, _>(VALUE_NAME).ok())
        .is_some();

    // Windows writes 0x02/0x06 when the entry is enabled in Startup Apps.
    let executable_will_launch_at_login = hkcu
        .open_subkey(APPROVED_PATH)
        .ok()
        .and_then(|approved| approved.get_raw_value(VALUE_NAME).ok())
        .and_then(|raw| raw.bytes.first().copied())
        .map(|first| first == 0x02 || first == 0x06)
        .unwrap_or(false);

    json!({
        "ok": true,
        "state": {
            "openAtLogin": open_at_login,
            "executableWillLaunchAtLogin": executable_will_launch_at_login
        }
    })
}

pub fn set(enabled: bool) -> Value {
    let hkcu = RegKey::predef(HKEY_CURRENT_USER);
    let run = match hkcu.open_subkey_with_flags(RUN_PATH, KEY_SET_VALUE) {
        Ok(key) => key,
        Err(error) => return json!({ "ok": false, "error": error.to_string() }),
    };

    if enabled {
        if let Err(error) = run.set_value(VALUE_NAME, &executable()) {
            return json!({ "ok": false, "error": error.to_string() });
        }
    } else {
        let _ = run.delete_value(VALUE_NAME);
    }
    get_state()
}

pub fn sync(wants: bool) -> Value {
    let current = get_state();
    let open = current["state"]["openAtLogin"].as_bool().unwrap_or(false);
    let will = current["state"]["executableWillLaunchAtLogin"].as_bool().unwrap_or(false);

    if !wants {
        return if open { set(false) } else { current };
    }
    // Do not re-enable an entry that Windows itself disabled.
    if open && !will {
        return current;
    }
    set(true)
}
