# Bug and issue review: Neutralino port

I read through the code without running it, so each item is marked **Certain** (follows from the code) or **Likely** (depends on Neutralino behavior I'd want you to verify). Ordered by severity.

---

## Critical (the app breaks or misbehaves)

### 1. `ipc:req` listener registered in the wrong window. **Certain**
`ipc.js` registers `Neutralino.events.on('ipc:req', ...)` at module load, but it uses `window.__router`, which only exists after `createRouter()`. That's fine in the host. The real problem is that **the host also receives its own broadcasts**, and the bridge's `send()` in the host calls the router directly. A child's `broadcast('ipc:req')` is handled once, which is correct. Check that the host does not double-handle: if `app.broadcast` also fires locally in the sender, a child never gets a duplicate, but the host's own `emit('ipc:res')` reaches the host listener too and is ignored. This is harmless, but see #2.

### 2. Every child window also handles `ipc:res` from other children. **Certain**
`ipc:res` is broadcast to **all** windows. Ids are `"task:1"`, `"settings:1"`, so collisions are unlikely. But the task window navigates between pages (`english.html` → `englishInfo.html`), and the **sequence counter resets to 0 each page load** while the process id stays `task`. A late reply to `task:1` from the old page can resolve the new page's `task:1` request with the wrong data. Fix: include a random page nonce in the id (`${myId}:${nonce}:${++seq}`).

### 3. `retireChild()` never actually closes the window, and the process keeps running. **Certain**
The bridge hides the WebView and navigates to `about:blank`. The Neutralino child process stays alive (running WebView2, ~50+ MB each) and is never reaped. After many task cycles you accumulate hidden `about:blank` processes. This defeats the RAM goal and leaks memory every cycle. Also `alive.delete('task')` in the host means the next `spawn('task')` creates yet another process. Options:
- Reuse the same child: navigate it to the next task URL instead of spawning.
- Or have the child call `Neutralino.app.exit()` after verifying in M0 that it only exits that child's process. Your comment says it terminates everything, so test this explicitly before relying on either option.

### 4. `windowClose` in children only fires if the user closes a visible window. **Likely**
`retireChild` hides, not closes, so `windowClose` doesn't fire. That's intended. But when the **user clicks X** on the task window, `windowClose` sends `win:closed`, the host calls `hideMainWindow()`, which does `emit('app:hide')`, and the child then runs `retireChild()`. By default Neutralino's `windowClose` handler does nothing unless you call `exit`/`hide`, so the window may stay open and the X appears dead. Add `Neutralino.window.hide()` in the `windowClose` handler as well.

### 5. Host `windowClose` is a no-op, so the user cannot close the countdown, which is fine. But `exitProcessOnClose: false` is only set for children via `spawn`. **Certain**
The host window's config has `exitProcessOnClose: false` already, so this is okay. The risk is in children: if `exitProcessOnClose` isn't honored by `window.create` options in your version, closing a child may kill the whole app (including the Chrome-unfreeze path). Verify, and note that the extension watchdog only unfreezes **after** the host dies, which can leave Chrome frozen for up to 1 s.

### 6. `showMainWindow()` hides the host's countdown window, but `window.hide()` is called on the **host itself** while `spawn` runs from the host. **Certain**
This works, but then `showCountdown()` calls `setAlwaysOnTop(true)` and `window.setOpacity` with `FindWindowW(title = "PomodoroLock")`. The **task window is also titled "PomodoroLock"** (see `spawn` default `title`). `FindWindowW` returns the first match by Z-order, so opacity 0.5 can be applied to the **maximized task window**, or to the wrong window. Give the host a distinct title (for example `PomodoroLock Countdown`) and update both the config and the `setOpacity` call.

### 7. `chrome.freeze` mock vs real on `settings:confirm`. **Certain**
`applySettings` calls `syncChromeFreeze()`, which calls `chrome.refreeze` then `chrome.freeze`. With the mock, `chrome.refreeze` returns `{ok:true}` without recording, good. But the `chrome-freeze.spec` expects the first-launch log to equal exactly `['unfreezeChrome']`. `initializeChromeFreeze` calls `chrome.resumeAll`, which the mock maps to `unfreeze()`. That works. However `settings:confirm` calls `applySettings` on **every** Confirm, so unrelated Confirms (for example in `auto-start.spec`) call `unfreezeChrome` too. Fine for the tests, but on a real machine this sends an unneeded `NtResumeProcess` on every Confirm. Only call sync when `chromeFreeze` actually changed.

### 8. `exitInfo` and `hideMainWindow` return early if the task isn't alive. **Certain**
`hideMainWindow()` has `if (quitting || !alive.has('task')) return;`. At first launch this is correct. But `electron.spec` "restores countdown on close" calls `hideMainWindow` after `showMainWindow`, which is fine. A real bug: `settings:confirm` calls `hideMainWindow()` even when no task is open, so it returns early and **never calls `showCountdown()`**. After Confirm, the old Electron app restarted the countdown (`closeCountdownWindow; hideMainWindow` recreated it). Test "confirm applies settings and restarts the countdown" expects `60|59` after Confirm. Here the countdown iframe is not reloaded, so the countdown continues from wherever it was and the test fails or flakes. Fix: in `settings:confirm`, call `showCountdown()` explicitly (reload the iframe).

---

## High

### 9. `countdown.js` calls `countdownFinished()` and a stale interval keeps running when the host is hidden. **Certain**
The countdown iframe keeps ticking while the task window is open, because the host WebView is only hidden. When it hits 0 it fires `countdown:finished` → `showMainWindow()` again, which **kills the current task and spawns a new one mid-task**. In Electron the countdown window was destroyed. Fix: in `showMainWindow`, blank the iframe (`frame.src = 'about:blank'`), or add a stop message. Reloading on `showCountdown` is already there.

### 10. `window.parent.electronAPI` access in the iframe. **Certain**
`electronAPI.js` returns early in the iframe only if `window.parent.electronAPI` is already set. The host script order in `host.html` is: bridge first, then the module, so it is set. But `countdown.html` also contains `<script src="../bridge/electronAPI.js">` **first in head** with `theme-boot.js` later. Inside the iframe, `themeId` comes from the shared object. It is read **once** at `theme-boot` time: `api.themeId`. The host sets `window.electronAPI.themeId` in `boot()` **before** the iframe's `src` is set, so that works. But `api.themeId` in the host's `api` object is a plain property assigned from `readSettings().theme`, while the theme listener updates `api.themeId` only through `theme:changed`. This is fine, just fragile.

### 11. `host.html` CSP blocks `style` inline needed by the iframe and drag overlay. **Likely**
`style-src 'self'` blocks the inline `style="..."` attributes on `<iframe>` and `<div id="drag">`. Result: the iframe gets default size (300x150) and the drag overlay has no `position:fixed`. Move these styles into a class in `base.css`, or add `'unsafe-inline'` to `style-src`.

### 12. The drag overlay blocks the iframe entirely. **Certain**
`#drag` covers the whole window (`inset:0`) over the iframe, which is intended. But Neutralino's draggable region swallows clicks, so there is no way to interact with the countdown (fine). Just note that `-webkit-app-region: drag` in `base.css` is ignored by WebView2 under Neutralino.

### 13. Camera permission prompt. **Likely**
`--use-fake-ui-for-media-stream` is only set in tests. In production the user gets the WebView2 prompt on every new origin/profile. Since each task is a **new process with the same user-data folder**, WebView2 should remember the decision, but only if the origin is stable. The port is fixed in config, which is good. Verify that the "ask again" prompt doesn't show each time.

### 14. `native.call` timeout of 5 s is used for DB. **Certain**
`tts.synthesize` passes 300 000, good. But the **Rust main loop is single-threaded and synchronous**: `tts::synthesize` blocks the whole extension (up to 300 s) while holding the `AppState` mutex. During that time `db.*`, `chrome.*` and `screen.workArea` calls all time out at 5 s. The watchdog and refreeze threads block on the same mutex too. Spawn TTS on its own thread and release the lock, or don't hold the mutex while synthesizing. Also, `tts.rs` is dispatched while the mutex is held in `handle_text`.

### 15. Extension replies are **broadcast** to all windows. **Certain**
`native:res` goes to every WebView. The host is the only one that listens in `native.js`, so it works, but child windows receive and discard every DB row, including full TTS WAV payloads (base64, possibly MBs). That wastes memory and IPC bandwidth for every window. Keep TTS bytes off the broadcast path: write the WAV to a temp file and return the path, or add a targeted dispatch.

---

## Medium

### 16. `Date`-free race: `initSettings()` runs before `native.call('db.open')`, but `lock.acquire` uses the extension before the host has verified the extension is ready. **Likely**
`Neutralino.extensions.dispatch` fails if the extension hasn't connected yet. There's no retry, so `lock.acquire` returns `{ok:false, error:'timeout'}` after 5 s and the app **quits as a "second instance"** (`!lock.ok` → `app.exit()`). Distinguish timeout/error from `ok:false` with an actual "already running" signal, and retry on startup until the extension reports ready. Use `Neutralino.extensions.getStats()` or poll.

### 17. `lock.acquire` is in the mock path? **Certain (not mocked)**
`WORDLOCK_USE_TEST_DATA` runs still call the real `lock.acquire`. Running two test processes, or a real app plus a test run, makes the test instance quit. Skip the lock when `testData` is true (the old code skipped it for the mock).

### 18. `chromeFreeze.mock.js` appends with read-then-write, racing itself. **Certain**
Two quick `record()` calls both read the same old content, so one line is lost. `chrome-freeze.spec` expects exact logs (`['unfreezeChrome','unfreezeChrome']`). Chain the writes through a promise queue. Also on exit, `unfreezeChromeOnExit` fires `void safely('chrome.unfreeze')` then `quitAll` waits only 120 ms before `Neutralino.app.exit()`. That final log write may never finish, so the "exiting the app unfreezes Chrome again" test can fail. Await the unfreeze before exiting.

### 19. `quitAll` race with real Chrome unfreeze. **Certain**
Same 120 ms window: `unfreezeChromeOnExit` sends the RPC and exits shortly after. The Rust side unfreezes on parent death (`process_alive`), so you recover within ~1 s, but until then Chrome stays frozen. Await the `chrome.unfreeze` response (with a short timeout) before `app.exit()`.

### 20. `parent_pid()` is the wrong "parent" if the extension is spawned by a helper. **Likely**
The watchdog trusts `th32ParentProcessID`. If Neutralino spawns extensions through an intermediate process, you watch the wrong pid. If that pid is dead (PID reuse is also possible), you exit immediately, or never. Compare against the pid from `NL_PID`-like info if available, and verify in the spike.

### 21. `process_alive` uses a full process snapshot every second plus every 200 ms loop. **Certain**
The main loop calls `process_alive(parent)` on **every iteration** (every 200 ms timeout) and the watchdog does it every second. Each call enumerates all system processes. It's wasteful (CPU wakeups plus allocations). Use `OpenProcess(SYNCHRONIZE)` + `WaitForSingleObject(handle, 0)` once, and keep the handle.

### 22. PID reuse in `process_alive`. **Certain**
It checks only that the pid exists. A recycled pid keeps the extension alive forever. The handle approach in #21 fixes this too.

### 23. Autostart `executable()` path is wrong. **Likely**
It uses `NL_PATH` or `current_exe().parent().parent()`. The extension lives at `<app>\extensions\wordlock-native.exe`, so parent×2 is `<app>`, which is right. But the binary name from `neu build` is `pomodorolock-win_x64.exe` (see `tests/helpers.js`), not `pomodorolock.exe`. The registry entry will point to a **non-existent file**. Use the real exe name, ideally from the parent process image path. Also the value isn't quoted: a path with spaces (for example `C:\Program Files\...`) will not launch. Quote it, and add the working directory handling since Neutralino needs to find `resources.neu`.

### 24. `autostart::sync` ignores a changed path. **Certain**
The Electron version re-wrote the path on each launch. Here, `sync(true)` calls `set(true)` only when not enabled or when the entry is approved. A moved exe with a stale `Run` value gets rewritten only in the "approved" branch, so that works. But if `open && !will` (disabled in Startup Apps), the stale path stays. This matches the Electron intent, fine, but compare the stored value to the current path rather than always rewriting.

### 25. DB: `db.exec` is exposed to the renderer-facing RPC. **Certain**
`db.exec` runs arbitrary SQL. Only the host calls `native.call`, but any page in any window can call `Neutralino.extensions.dispatch` directly (`nativeAllowList` includes `extensions.*`, and `injectGlobals` is on). Combine that with the broad allow-list, and a compromised page gets arbitrary SQL, file read/write (`fs.readText`/`fs.writeText`) and registry writes. Restrict: gate `db.exec`, `fs.*` behind the test env, drop `fs.*` entirely (Neutralino's own `filesystem` already works), and narrow `nativeAllowList` (children don't need `os.*`, `filesystem.*`, `computer.*`).

### 26. `ignored()` lowercases in Rust, and so does SQLite `LOWER`, but **`LOWER` is ASCII-only in SQLite**. **Certain**
Rust's `to_lowercase()` handles Unicode (katakana/kanji unaffected, but any cased non-ASCII text differs). Japanese ignore lists work because kana/kanji have no case. Works as before, but note it behaves the same way the old code did, so low risk.

### 27. `db.rs` `json_cols` parse failure silently becomes `null`. **Certain**
`unwrap_or(Value::Null)` for malformed JSON means `row.examples` becomes `null` and the UI shows no examples with no error. The Electron code threw. Return `[]` or surface an error.

### 28. `db.rs` blob → `json!(b)`. Minor: it serializes as an array of numbers, not base64. Unused today.

### 29. Seed overwrites user edits. **Certain, parity with the old code**
`english_word` id 0 and `japanese_word` id 0 are `ON CONFLICT DO UPDATE` on every launch. Same as Electron, so a user who edited row 0 loses the change. Not a regression, just noting it.

### 30. `settings.js`: `readText` fallback masks errors, and a missing file takes the slow path. **Certain**
On a first run, `Neutralino.filesystem.readFile` throws (no file), then it calls `native.fs.readText`, which also fails after a round trip. Harmless but adds startup latency. Also `Neutralino.filesystem.move` must exist (it's `filesystem.move` in v5). The `catch` falls back to Rust `fs.move`, but **`fs::rename` fails on Windows if the destination exists**. Rust's `std::fs::rename` does replace on Windows, so that is OK. `Neutralino.filesystem.move` may fail when the target exists, so the fallback usually runs. Consider a single `fs.writeText` plus `fs.move` path, and make the parent directory (`dataDir`) exist before writing the temp file (Neutralino's `writeFile` doesn't create directories; the Rust one does).

### 31. `settings.js` `useTestData = true` initial value, with a settings file path of `null`. **Certain**
If `getSettings()` is called before `initSettings()` completes, `settings` is `null` and it throws on `settings.ignoreWords`. The `ipc:req` listener is registered at import time, so a child's early request can reach `getSettings()` before `initSettings()` finishes. It's guarded only by the router being created late (`window.__router` set in `createRouter()` after init), which returns `undefined` and then the child's 8 s timeout resolves `undefined`. `countdown.js` has a fallback, but `settingsUI.js` does `settings.englishWord` on `undefined` and **throws**. Fine in practice since children only spawn after boot, but the iframe loads after init too. Low risk.

### 32. `setSettings` always resets `nextTaskIndex = 0` and also coerces missing task flags to `false`. **Certain, parity**
`setSettings({autoTheme:true, theme:'paper'})` with no task keys turns **all four tasks off** (`Boolean(undefined)`). The old tests did this too (and `getNextTaskType` falls back to English), so it matches, but it's a latent bug in the original design.

### 33. `showMainWindow` sleeps 150 ms and spawns again without waiting for the old child to actually go away. **Certain**
Pairs with #3. The old task page can still hold the camera if its `app:close` handler (which calls `retireChild`, **not** the `onHide` cleanup callbacks) runs. `app:close` skips `hideCbs`, so **the camera stream and TTS audio aren't stopped** when the task is replaced via `app:close`. Only `pagehide` covers it, and navigating to `about:blank` does fire `pagehide`, so it's mostly OK, but the order isn't guaranteed. Run `hideCbs` in the `app:close` handler too.

### 34. Camera is on while the window is hidden. **Likely**
Between `hide` and `about:blank` navigation (60 ms + 60 ms), cleanup runs via `onHide` → `stop()`. OK. But if the user closes the task window with X, `windowClose` → `win:closed` → host emits `app:hide` → same path. Fine, assuming the host is still alive.

---

## Low / cleanup

35. **Dead Electron files**: `resources/src/countdown/createCountdownWindow.js` still `require('electron')`. It's not loaded, but it ships in the package. Delete it.
36. **`host.html` CSP** has no `media-src`, fine, but `connect-src` should include `http://127.0.0.1:8080` only if the host `fetch`es TTS. It doesn't, so skip.
37. **`tailwind.input.css`** has no `@source` for `resources/src/host/host.html` (the HTML glob lists don't include `host/*.html`). Add it, or classes like `h-full overflow-hidden` on `<html>` won't be generated.
38. **Codemod ordering**: it inserts the bridge as the first element in `<head>`, **before** `<meta charset>` and before the CSP meta. A script before the CSP `<meta>` isn't governed by the policy, and charset must be within the first 1024 bytes. It's harmless here but should come after the CSP meta.
39. **Codemod `replace(/;$/, '')`** only strips a trailing semicolon from the policy. Policies in this repo don't end with `;`, so it works, but a trailing space breaks it.
40. **`mcq` CSP** and others use `connect-src 'self' ws://127.0.0.1:*`. That lets any page connect to any local WebSocket port. Pin the actual Neutralino port (47831) rather than `*`.
41. **`tray.js`**: `Neutralino.os.setTray` throwing is swallowed, and the tray stays missing. Then there's no way to quit (the window is borderless and hidden). Log the failure, and show a visible fallback.
42. **`window.setOpacity` is called before the window is visible**, and `SetLayeredWindowAttributes` can be reset when WebView2 or Neutralino re-applies window styles. Reapply it after each `show()` (you do, in `showCountdown`), but not after the 2 s refreeze. Fine.
43. **`GWL_EXSTYLE as i32`** in `windows-sys 0.59`: `GWL_EXSTYLE` is already `WINDOW_LONG_PTR_INDEX` (i32). Fine. `LWA_ALPHA as u32` is also fine. Just double-check that `SetWindowLongPtrW` compiles for your target.
44. **`tungstenite` with `default-features = false, features = ["handshake"]`** lacks `url`/TLS features, and `client(url, stream)` needs `IntoClientRequest` for `String`, which exists under the base crate. OK. But `Message::Text(text.into())` with `tungstenite 0.24` takes `String` (0.26 changed to `Utf8Bytes`). Pin the version, since your `handle_text` signature takes `String` from `Message::Text(text)`. It compiles on 0.24 only.
45. **`uuid` v4** pulls in `getrandom`, adding size. Use a counter-based id, since the id only needs to be unique per connection.
46. **`ureq` with `default-features = false`**: `ureq 2` without `tls` is plain HTTP only, which is fine for `127.0.0.1`. But `AgentBuilder` plus `timeout` works without extra features. OK.
47. **`tts.rs` has custom base64**, and `host/tts.js` base64-decodes then re-encodes (`atob` loop plus `btoa` loop) on every call. For multi-MB WAVs the per-byte string concatenation is slow. Do the gain in Rust (you already read the bytes), or use chunked `String.fromCharCode.apply`.
48. **`speakUI.js` `cleanup` on `onHide`** sets `disposed = true` permanently. Since each page load re-evaluates the script, that's fine.
49. **`tests/helpers.js`**: `windows()` filters by `http(s)` URLs, but the host iframe is not a page, so it is excluded, good. However `retireChild` leaves `about:blank` pages, which are excluded as well, so `windowGone` passes even though the process leaks (it hides bug #3 from the test suite).
50. **`helpers.js` kills only `child`**, not the child windows' processes. Orphaned Neutralino children survive across tests, and a fixed CDP port 9333 is shared by every WebView2 process. Two WebView2 instances with the same `--remote-debugging-port` can't both bind, so **only the first window gets CDP** and later child windows may be invisible to Playwright. Per-window remote debugging is a likely cause of flaky or failing `waitForWindow`. Since all windows share one browser process under one user-data folder, they should appear as pages in one browser. Verify this, because if each `window.create` spawns its own process with its own WebView2 environment, only one process can own the port.
51. **`electron.spec.js` filename and name** are stale ("electron"). Rename.
52. **`playwright.config.js` `maxFailures: 1`** stops the whole suite at the first failure, which hides the other bugs. Remove it while you stabilize.
53. **`package.json` scripts** aren't included, so I couldn't verify `neu build` wiring, `rcedit`, or the clean step.

---

## Suggested fix order

1. **#3, #9, #8, #6**: window lifecycle (leaked processes, countdown ticking behind the task, Confirm not restarting the countdown, wrong window gets opacity). These break core behavior.
2. **#14, #15, #16, #17, #25**: extension reliability and security (blocking TTS, broadcast of large payloads, startup race, ungated `db.exec`/`fs.*`).
3. **#18, #19, #23**: shutdown and auto-start correctness.
4. **#11, #38, #37**: CSP and static-asset fixes.
5. Everything else.

If you want, I can write patches for the top eight, starting with a reworked `windows.js` and bridge that reuse a single task child, and a non-blocking Rust dispatcher.