use serde_json::{json, Value};
use std::ffi::c_void;
use std::fs;
use std::path::Path;
use windows_sys::Win32::Foundation::{
    CloseHandle, GetLastError, SetLastError, ERROR_ALREADY_EXISTS, INVALID_HANDLE_VALUE, RECT,
};
use windows_sys::Win32::System::Diagnostics::ToolHelp::{
    CreateToolhelp32Snapshot, Process32FirstW, Process32NextW, PROCESSENTRY32W, TH32CS_SNAPPROCESS,
};
use windows_sys::Win32::System::Threading::{CreateMutexW, GetCurrentProcessId};
use windows_sys::Win32::UI::WindowsAndMessaging::{
    FindWindowW, GetWindowLongPtrW, LWA_ALPHA, SetLayeredWindowAttributes, SetWindowLongPtrW,
    SystemParametersInfoW, GWL_EXSTYLE, SPI_GETWORKAREA, WS_EX_LAYERED,
};

fn wide(text: &str) -> Vec<u16> {
    text.encode_utf16().chain(std::iter::once(0)).collect()
}

fn processes() -> Vec<(u32, u32)> {
    unsafe {
        let snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if snapshot == INVALID_HANDLE_VALUE {
            return Vec::new();
        }
        let mut entry: PROCESSENTRY32W = std::mem::zeroed();
        entry.dwSize = std::mem::size_of::<PROCESSENTRY32W>() as u32;
        let mut found = Vec::new();
        if Process32FirstW(snapshot, &mut entry) != 0 {
            loop {
                found.push((entry.th32ProcessID, entry.th32ParentProcessID));
                if Process32NextW(snapshot, &mut entry) == 0 {
                    break;
                }
            }
        }
        CloseHandle(snapshot);
        found
    }
}

pub fn current_pid() -> u32 {
    unsafe { GetCurrentProcessId() }
}

pub fn parent_pid() -> u32 {
    let me = current_pid();
    for (pid, parent) in processes() {
        if pid == me {
            return parent;
        }
    }
    0
}

pub fn process_alive(pid: u32) -> bool {
    pid != 0 && processes().iter().any(|(id, _)| *id == pid)
}

pub fn acquire_lock() -> bool {
    unsafe {
        let name = wide("Local\\PomodoroLock");
        SetLastError(0);
        let handle = CreateMutexW(std::ptr::null(), 1, name.as_ptr());
        if handle.is_null() || GetLastError() == ERROR_ALREADY_EXISTS {
            return false;
        }
        // The mutex stays open for the process lifetime; Windows closes it at exit.
        true
    }
}

pub fn work_area() -> Value {
    unsafe {
        let mut rect: RECT = std::mem::zeroed();
        let ok = SystemParametersInfoW(
            SPI_GETWORKAREA,
            0,
            &mut rect as *mut RECT as *mut c_void,
            0,
        );
        if ok == 0 {
            return json!({ "ok": false, "error": "SPI_GETWORKAREA failed" });
        }
        json!({
            "ok": true,
            "rect": {
                "x": rect.left,
                "y": rect.top,
                "w": rect.right - rect.left,
                "h": rect.bottom - rect.top
            }
        })
    }
}

pub fn set_opacity(title: &str, alpha: f64) -> Value {
    unsafe {
        let handle = FindWindowW(std::ptr::null(), wide(title).as_ptr());
        if handle.is_null() {
            return json!({ "ok": false, "error": "window not found" });
        }
        let style = GetWindowLongPtrW(handle, GWL_EXSTYLE as i32);
        if style == 0 {
            return json!({ "ok": false, "error": "GetWindowLongPtrW failed" });
        }
        SetWindowLongPtrW(handle, GWL_EXSTYLE as i32, style | (WS_EX_LAYERED as isize));
        let value = (alpha.clamp(0.0, 1.0) * 255.0).round() as u8;
        let ok = SetLayeredWindowAttributes(handle, 0, value, LWA_ALPHA as u32);
        json!({ "ok": ok != 0 })
    }
}

pub fn read_text(path: &str) -> Value {
    match fs::read_to_string(path) {
        Ok(text) => json!({ "ok": true, "text": text }),
        Err(error) => json!({ "ok": false, "error": error.to_string() }),
    }
}

pub fn write_text(path: &str, data: &str) -> Value {
    if let Some(parent) = Path::new(path).parent() {
        let _ = fs::create_dir_all(parent);
    }
    match fs::write(path, data) {
        Ok(()) => json!({ "ok": true }),
        Err(error) => json!({ "ok": false, "error": error.to_string() }),
    }
}

pub fn move_file(from: &str, to: &str) -> Value {
    match fs::rename(from, to) {
        Ok(()) => json!({ "ok": true }),
        Err(error) => json!({ "ok": false, "error": error.to_string() }),
    }
}
