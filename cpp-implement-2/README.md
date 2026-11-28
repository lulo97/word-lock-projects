# PomodoroLockCpp

A C++/Win32 tray application that locks the active task screen between countdowns.
Task screens are hosted in WebView2 and call native services (SQLite, settings, TTS,
Chrome freeze) through an injected `window.app` bridge.

## Windows

- **MainWindow** — WebView2 client area that navigates directly to the next enabled task:
  - `english.html` → `englishInfo.html` after the typed word matches.
  - `japanese.html` → `japaneseInfo.html` after the typed romaji matches.
  - `mcq.html` → `mcqResult.html` after an option is chosen.
  - `dumbbell.html` — live camera stage, pose overlay, and two 10-rep counters.
  - Info/result screens render database rows via `info-render.js` and share `ui.css` +
    `themes.css`. **Enter** exits the task back to the countdown.
- **CountDownWindow** — frameless `100x50`, 50% alpha, bottom-right of the primary monitor's
  work area. Counts down from the configured 1–3600 second interval.
- **SettingWindows** — full Settings UI with task toggles, countdown/volume controls,
  Chrome freeze, auto-start, auto-theme, theme picker, and an Ignore Words editor.
  Hidden at startup; opened from **Tray → Settings**.
- **IgnoreWordsWindows** — English/Japanese tabs with per-tab drafts, host-side validation,
  duplicate retention, and persistence to `settings.json`.

The app is per-monitor DPI-aware; window sizes are kept in logical pixels and scaled
automatically.

## Window flow

- **First launch:** only **CountDownWindow** is visible.
- When the countdown reaches 0, MainWindow opens with the next enabled task.
- **Tray → Show App** opens MainWindow. Clicking the tray icon toggles between MainWindow
  and the countdown.
- **Closing MainWindow** hides it and restarts the countdown.
- **Tray → Settings** shows SettingWindows. Closing it only hides it and reverts any
  unconfirmed theme preview.
- **Confirm** saves settings, resets task rotation, syncs Chrome freeze and auto-start,
  rebroadcasts the theme, hides secondary windows, and hides MainWindow.
- **Cancel** rebroadcasts the saved theme and hides Settings/Ignore Words.
- **Tray → Quit** unfreezes Chrome, removes the tray icon, and exits completely.

## Task screen flow

- Database rows are seeded with English `apple`, MCQ `1+1=?` (answer `2`), and Japanese
  `桜` (`sakura`).
- **English:** type the displayed word (case-insensitive) to open its info screen after 300 ms.
- **Japanese:** type the displayed romaji to open its info screen after 300 ms.
- **Multiple choice:** choose an option once; the result screen compares the selected index
  with `correct_answer_idx`.
- **Dumbbell:** each arm needs 10 confirmed reps (20 total); completion shows `Training done`
  and exits after 300 ms. MediaPipe assets are used when installed; otherwise the tracker
  still exposes `window.__dumbbellTest.feedPose(left, right, null, now)` for synthetic poses.
- **Enter** on any info/result screen exits to the countdown, advancing the theme first when
  `autoTheme` is enabled.
- English and Japanese info screens show the word's most common part of speech
  (`noun`, `verb`, `adjective`, ...) from the `part_of_speech` column. Older databases get the
  column added automatically on startup, defaulting existing rows to `noun`.
- The `svg_icon` column stores a wordless SVG illustration rendered as a small decorative image
  on the info screens. Rows without an icon simply show no image. The column is added
  automatically on startup.

## Native services

- Settings persist to `%USERPROFILE%\pomodoro_lock_cpp\settings.json`; SQLite data lives in
  `%USERPROFILE%\pomodoro_lock_cpp\pomodoro_lock_cpp.db`. The separate folder and database
  name keep this app isolated from the Electron `PomodoroLock` app.
- Ignore words are filtered case-insensitively from random English and Japanese queries.
- Speech posts `{"input","lang","voice":"female"}` to `http://127.0.0.1:8080/tts`; when the
  server is unavailable, automatic speech stays quiet while button clicks show an error.
- Successful speech is cached in the `tts_cache` table as un-gained OGG bytes, keyed by
  `(lang, voice, trimmed text)`. On every read the cache decodes OGG back to WAV with `ffmpeg.exe`
  (PATH) and applies the current master volume, so changing volume does not serve stale loudness.
  `ffmpeg` compresses a ~46 KB WAV to ~9 KB OGG in testing; a 200 MB LRU cap prunes old rows.
  Cache hits work offline, and speech still works uncached if `ffmpeg` is missing.
- Chrome freeze records each PID's start time before `NtSuspendProcess`, refreezes every
  2 seconds, resumes on disable/exit, and resumes leftovers on the next startup.
- Auto-start writes `HKCU\Software\Microsoft\Windows\CurrentVersion\Run\PomodoroLockCpp`,
  preferring `PORTABLE_EXECUTABLE_FILE`, and respects `StartupApproved\Run`. A legacy C++
  `PomodoroLock` entry is removed only when it points at this executable.

## Environment flags

- `WORDLOCK_USE_TEST_DATA=1` — in-memory SQLite and in-memory settings defaults.
- `WORDLOCK_CHROME_FREEZE_MOCK=1` — disables the single-instance mutex and Chrome freeze.
- `WORDLOCK_TTS_MOCK=1` — replaces the network call with a short synthetic WAV; `fail` simulates an
  offline TTS server.
- `WORDLOCK_TTS_TEST_FORCE_MOCK=1` — forces the TTS cache integration test down its mock branch
  even when port 8080 is listening.
- `WORDLOCK_TEST_MODE=1` — enables test hooks (message log, DOM dump, command script, and
  per-process window class suffixes). The harness in `tests/support/app_harness.h` sets these for
  every runtime test:
  - `WORDLOCK_TEST_MESSAGE_LOG_FILE` — appends each host message type to this file.
  - `WORDLOCK_TEST_DOM_DUMP_FILE` / `WORDLOCK_TEST_COMMAND_FILE` — writes a one-shot DOM snapshot
    and runs the supplied JavaScript inside the WebView.
  - `WORDLOCK_TEST_HOME` — data directory used when `WORDLOCK_USE_TEST_DATA` is off, which makes
    settings/database persistence testable.
  - `WORDLOCK_TEST_FREEZE_INTERVAL_MS` / `WORDLOCK_TEST_COUNTDOWN_TICK_MS` — shorten the 2 s
    Chrome-freeze and 1 s countdown timers so negative checks stay fast.
  - `WORDLOCK_TEST_SINGLE_INSTANCE=1` — opts into the single-instance mutex when test mode is on;
    `WORDLOCK_TEST_SINGLE_INSTANCE_NAME` gives each test pair its own mutex name.

## Build

Dependencies are declared in `vcpkg.json` (`webview2`, `sqlite3`). Build with one command from
this folder:

```bat
build.bat
```

`build.bat` configures `build` with the vcpkg toolchain when needed, then builds Release. The
equivalent manual commands are:

```powershell
cmake -S . -B build -A x64 `
  "-DCMAKE_TOOLCHAIN_FILE=C:/Program Files/Microsoft Visual Studio/18/Community/VC/vcpkg/scripts/buildsystems/vcpkg.cmake" `
  -DVCPKG_TARGET_TRIPLET=x64-windows
cmake --build build --config Release
```

CMake copies all HTML/CSS/JS assets, `sqlite3.dll`, `WebView2Loader.dll`, and the executable
into `build\Release`.

## Run

```powershell
.\build\Release\PomodoroLockCpp.exe
```

The WebView2 Runtime is normally preinstalled on Windows 11. If it is missing, install the
Microsoft WebView2 Runtime before launching.

## Tests

Tests use Catch2 and CTest. Each `.cpp` file directly in `tests/` is registered as its own test
case. Build the app and run every test with:

```bat
test.bat
```

Run one test file with:

```bat
test.bat tests\settings_window_shows_settings_text.cpp
```

Every test case that launches the real executable carries the `[runtime]` tag. CTest gives those
a 30s timeout and a shared `desktop` resource lock so window/focus tests never run in parallel;
all other tests get 10s. The pure-logic checks live in `unit_*.cpp` and can be run alone with:

```bat
ctest --test-dir build -C Release -R "^unit_" --output-on-failure
```

Runtime tests share `tests/support/app_harness.h`, which sets the test environment, launches the
app in a job object, polls for messages/windows instead of sleeping, and removes its temp directory
on exit.

Runtime tests launch `PomodoroLockCpp.exe` in isolated test mode and exercise the real windows and
WebView2 views: settings confirm, theme swapping, input focus, word progress, Chrome freeze scope,
ignore-word persistence, single-instance behaviour, countdown dragging/foreground/theme, and the
`https://app/` 403 traversal guard.
