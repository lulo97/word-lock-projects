use serde_json::{json, Value};
use std::env;
use std::ffi::c_void;
use windows_sys::Win32::Foundation::{
    CloseHandle, GetLastError, SetLastError, ERROR_ALREADY_EXISTS, HANDLE, INVALID_HANDLE_VALUE,
    RECT, WAIT_TIMEOUT,
};
use windows_sys::Win32::System::Diagnostics::ToolHelp::{
    CreateToolhelp32Snapshot, Process32FirstW, Process32NextW, PROCESSENTRY32W, TH32CS_SNAPPROCESS,
};
use windows_sys::Win32::System::Threading::{
    CreateMutexW, GetCurrentProcessId, OpenProcess, WaitForSingleObject,
    PROCESS_QUERY_LIMITED_INFORMATION, PROCESS_SYNCHRONIZE,
};
use windows_sys::Win32::UI::WindowsAndMessaging::{
    FindWindowW, GetWindowLongPtrW, SetLayeredWindowAttributes, SetWindowLongPtrW,
    SystemParametersInfoW, GWL_EXSTYLE, LWA_ALPHA, SPI_GETWORKAREA, WS_EX_LAYERED,
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
    // Neutralino passes its own pid to extensions in these env vars on recent
    // releases. Prefer them over a process snapshot.
    for key in ["NL_PID", "NEUTRALINO_PID"] {
        if let Ok(value) = env::var(key) {
            if let Ok(pid) = value.trim().parse::<u32>() {
                if pid != 0 {
                    return pid;
                }
            }
        }
    }

    let me = current_pid();
    for (pid, parent) in processes() {
        if pid == me {
            return parent;
        }
    }
    0
}

/// A waitable handle to the Neutralino parent process. A process handle stays
/// signaled only when that exact process exits, so recycled pids are harmless.
pub struct ParentProcess {
    handle: HANDLE,
}

unsafe impl Send for ParentProcess {}
unsafe impl Sync for ParentProcess {}

impl ParentProcess {
    pub fn open(pid: u32) -> Self {
        let handle = if pid == 0 {
            std::ptr::null_mut()
        } else {
            unsafe {
                OpenProcess(
                    PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_SYNCHRONIZE,
                    0,
                    pid,
                )
            }
        };
        Self { handle }
    }

    pub fn is_alive(&self) -> bool {
        if self.handle.is_null() {
            // If OpenProcess was denied, do not invent a parent death: the
            // stdin-EOF watcher is still authoritative.
            return true;
        }
        unsafe { WaitForSingleObject(self.handle, 0) == WAIT_TIMEOUT }
    }
}

impl Drop for ParentProcess {
    fn drop(&mut self) {
        if !self.handle.is_null() {
            unsafe { CloseHandle(self.handle) };
        }
    }
}

pub fn acquire_lock() -> Value {
    unsafe {
        let name = wide("Local\\PomodoroLock");
        SetLastError(0);
        let handle = CreateMutexW(std::ptr::null(), 1, name.as_ptr());
        if handle.is_null() {
            return json!({ "ok": false, "error": format!("CreateMutexW failed: {}", GetLastError()) });
        }
        if GetLastError() == ERROR_ALREADY_EXISTS {
            return json!({ "ok": false, "error": "already-running" });
        }
        // The mutex stays open for the process lifetime; Windows closes it at exit.
        json!({ "ok": true })
    }
}

pub fn work_area() -> Value {
    unsafe {
        let mut rect: RECT = std::mem::zeroed();
        let ok =
            SystemParametersInfoW(SPI_GETWORKAREA, 0, &mut rect as *mut RECT as *mut c_void, 0);
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
