use serde_json::{json, Value};
use std::collections::HashMap;
use windows_sys::Win32::Foundation::{CloseHandle, FILETIME, HANDLE, INVALID_HANDLE_VALUE};
use windows_sys::Win32::System::Diagnostics::ToolHelp::{
    CreateToolhelp32Snapshot, Process32FirstW, Process32NextW, PROCESSENTRY32W, TH32CS_SNAPPROCESS,
};
use windows_sys::Win32::System::LibraryLoader::{GetModuleHandleW, GetProcAddress};
use windows_sys::Win32::System::Threading::{GetProcessTimes, OpenProcess};

const PROCESS_QUERY_LIMITED_INFORMATION: u32 = 0x1000;
const PROCESS_SUSPEND_RESUME: u32 = 0x0800;

type NtProcessFn = unsafe extern "system" fn(HANDLE) -> i32;

fn wide(text: &str) -> Vec<u16> {
    text.encode_utf16().chain(std::iter::once(0)).collect()
}

fn nt_process_fn(name: &[u8]) -> Option<NtProcessFn> {
    unsafe {
        let module = GetModuleHandleW(wide("ntdll.dll").as_ptr());
        if module.is_null() {
            return None;
        }
        let address = GetProcAddress(module, name.as_ptr())?;
        Some(std::mem::transmute::<*const (), NtProcessFn>(address as *const ()))
    }
}

unsafe fn processes() -> Vec<(u32, String)> {
    let snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if snapshot == INVALID_HANDLE_VALUE {
        return Vec::new();
    }

    let mut entry: PROCESSENTRY32W = std::mem::zeroed();
    entry.dwSize = std::mem::size_of::<PROCESSENTRY32W>() as u32;

    let mut found = Vec::new();
    if Process32FirstW(snapshot, &mut entry) != 0 {
        loop {
            let raw = String::from_utf16_lossy(&entry.szExeFile);
            let name = raw.split('\0').next().unwrap_or("").to_lowercase();
            found.push((entry.th32ProcessID, name));
            if Process32NextW(snapshot, &mut entry) == 0 {
                break;
            }
        }
    }
    CloseHandle(snapshot);
    found
}

unsafe fn creation_ticks(pid: u32) -> Option<u64> {
    let handle = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, 0, pid);
    if handle.is_null() {
        return None;
    }
    let mut creation: FILETIME = std::mem::zeroed();
    let mut exit: FILETIME = std::mem::zeroed();
    let mut kernel: FILETIME = std::mem::zeroed();
    let mut user: FILETIME = std::mem::zeroed();
    let ok = GetProcessTimes(handle, &mut creation, &mut exit, &mut kernel, &mut user);
    CloseHandle(handle);
    if ok == 0 {
        return None;
    }
    Some(((creation.dwHighDateTime as u64) << 32) | creation.dwLowDateTime as u64)
}

pub struct ChromeFreeze {
    // pid -> creation time. A recycled pid is never resumed or frozen twice.
    held: HashMap<u32, u64>,
}

impl Default for ChromeFreeze {
    fn default() -> Self {
        Self::new()
    }
}

impl ChromeFreeze {
    pub fn new() -> Self {
        Self { held: HashMap::new() }
    }

    pub fn freeze_all(&mut self) -> Value {
        let suspend = match nt_process_fn(b"NtSuspendProcess\0") {
            Some(f) => f,
            None => return json!({ "ok": false, "error": "NtSuspendProcess unavailable" }),
        };
        let mut count = 0;
        unsafe {
            for (pid, name) in processes() {
                if name != "chrome.exe" {
                    continue;
                }
                let ticks = match creation_ticks(pid) {
                    Some(t) => t,
                    None => continue,
                };
                if self.held.get(&pid) == Some(&ticks) {
                    continue;
                }
                let handle = OpenProcess(PROCESS_SUSPEND_RESUME, 0, pid);
                if handle.is_null() {
                    continue;
                }
                let status = suspend(handle);
                CloseHandle(handle);
                if status >= 0 {
                    self.held.insert(pid, ticks);
                    count += 1;
                }
            }
        }
        json!({ "ok": true, "count": count })
    }

    pub fn unfreeze_all(&mut self) -> Value {
        let resume = match nt_process_fn(b"NtResumeProcess\0") {
            Some(f) => f,
            None => return json!({ "ok": false, "error": "NtResumeProcess unavailable" }),
        };
        let current = unsafe { processes() };
        let mut count = 0;
        unsafe {
            for (pid, ticks) in self.held.iter() {
                let still_match = current.iter().any(|(id, name)| {
                    id == pid
                        && name == "chrome.exe"
                        && creation_ticks(*pid) == Some(*ticks)
                });
                if !still_match {
                    continue;
                }
                let handle = OpenProcess(PROCESS_SUSPEND_RESUME, 0, *pid);
                if handle.is_null() {
                    continue;
                }
                let status = resume(handle);
                CloseHandle(handle);
                if status >= 0 {
                    count += 1;
                }
            }
        }
        self.held.clear();
        json!({ "ok": true, "count": count })
    }

    // Startup recovery: resume every chrome process, even ones we did not freeze.
    pub fn resume_all(&mut self) -> Value {
        let resume = match nt_process_fn(b"NtResumeProcess\0") {
            Some(f) => f,
            None => return json!({ "ok": false, "error": "NtResumeProcess unavailable" }),
        };
        let mut count = 0;
        unsafe {
            for (pid, name) in processes() {
                if name != "chrome.exe" {
                    continue;
                }
                let handle = OpenProcess(PROCESS_SUSPEND_RESUME, 0, pid);
                if handle.is_null() {
                    continue;
                }
                let status = resume(handle);
                CloseHandle(handle);
                if status >= 0 {
                    count += 1;
                }
            }
        }
        self.held.clear();
        json!({ "ok": true, "count": count })
    }
}
