use serde_json::{json, Value};
use std::env;
use std::path::Path;
use windows_sys::Win32::Foundation::CloseHandle;
use windows_sys::Win32::System::Threading::{
    OpenProcess, QueryFullProcessImageNameW, PROCESS_QUERY_LIMITED_INFORMATION,
};
use winreg::enums::{HKEY_CURRENT_USER, KEY_SET_VALUE};
use winreg::RegKey;

use crate::system;

const RUN_PATH: &str = "Software\\Microsoft\\Windows\\CurrentVersion\\Run";
const APPROVED_PATH: &str =
    "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\StartupApproved\\Run";
const VALUE_NAME: &str = "PomodoroLock";
const RELEASE_EXE: &str = "pomodorolock-win_x64.exe";

fn file_name(path: &str) -> String {
    path.rsplit(['\\', '/'])
        .next()
        .unwrap_or(path)
        .to_ascii_lowercase()
}

/// Ask Windows for the host process image instead of guessing from NL_PATH.
/// Neutralino can spawn extensions through a helper, so only accept a known
/// host binary name; otherwise fall back to the app directory below.
fn parent_executable() -> Option<String> {
    let pid = system::parent_pid();
    if pid == 0 {
        return None;
    }

    unsafe {
        let handle = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, 0, pid);
        if handle.is_null() {
            return None;
        }

        let mut buffer = vec![0u16; 32_768];
        let mut size = buffer.len() as u32;
        let ok = QueryFullProcessImageNameW(handle, 0, buffer.as_mut_ptr(), &mut size);
        CloseHandle(handle);

        if ok == 0 || size == 0 || size as usize > buffer.len() {
            return None;
        }

        let path = String::from_utf16_lossy(&buffer[..size as usize]);
        let name = file_name(&path);
        if name == RELEASE_EXE || name == "pomodorolock.exe" {
            Some(path)
        } else {
            None
        }
    }
}

fn executable() -> String {
    if let Some(path) = parent_executable() {
        return path;
    }

    let base = env::var("NL_PATH").unwrap_or_default();
    if base.is_empty() {
        RELEASE_EXE.to_string()
    } else {
        format!("{}\\{}", base.trim_end_matches(['\\', '/']), RELEASE_EXE)
    }
}

/// Run entries execute with System32 as the working directory. Launch through
/// cmd.exe so Neutralino can find resources.neu next to the binary. The whole
/// command is quoted, including a Program Files style directory.
fn desired_value() -> String {
    let exe = executable();
    let dir = Path::new(&exe)
        .parent()
        .and_then(|parent| parent.to_str())
        .unwrap_or("");

    if dir.is_empty() {
        format!("\"{}\"", exe)
    } else {
        format!("cmd.exe /c start \"\" /B /D \"{}\" \"{}\"", dir, exe)
    }
}

fn stored_value() -> Option<String> {
    RegKey::predef(HKEY_CURRENT_USER)
        .open_subkey(RUN_PATH)
        .ok()
        .and_then(|run| run.get_value::<String, _>(VALUE_NAME).ok())
}

pub fn get_state() -> Value {
    let hkcu = RegKey::predef(HKEY_CURRENT_USER);

    let open_at_login = stored_value().is_some();

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
        let value = desired_value();
        // Electron rewrote the path every launch; only do the registry write
        // when a moved/renamed executable actually changed the command.
        if stored_value().as_deref() == Some(value.as_str()) {
            return get_state();
        }
        if let Err(error) = run.set_value(VALUE_NAME, &value) {
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
    let will = current["state"]["executableWillLaunchAtLogin"]
        .as_bool()
        .unwrap_or(false);

    if !wants {
        return if open { set(false) } else { current };
    }
    // Do not re-enable an entry that Windows itself disabled.
    if open && !will {
        return current;
    }
    if stored_value().as_deref() == Some(desired_value().as_str()) {
        return current;
    }
    set(true)
}
