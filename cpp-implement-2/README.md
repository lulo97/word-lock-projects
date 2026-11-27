# Win32 Hello World

A minimal C++ Win32 API program with a Windows 11 tray icon and three windows.

## Windows

- **MainWindow** — `1280x720` white WebView2 client area. Its content is rebuilt from the
  confirmed task checkboxes: `English`, `japanese`, `mcq`, and/or `dumbbell` divs, one per
  enabled task.
- **CountDownWindow** — frameless `100x50`, bottom-right of the primary monitor's work
  area, counts `60 → 0` once per second.
- **SettingWindows** — white background, hosted via WebView2. It loads
  `src/settings/settings.html`, which contains the full Settings UI structure (HTML/CSS
  only, no logic yet). It is hidden at startup and can only be opened from
  **Tray → Setting**.
- **IgnoreWordsWindows** — hosted via WebView2, loads
  `src/settings/ignoreWords.html`. It opens when the **Ignore words…** button in
  SettingWindows is clicked. Its UI is markup/CSS only; `ignoreWordsUI.js` is a stub.

The app is per-monitor DPI-aware, so text stays crisp at Windows zoom levels such as
125% and 150%. Window sizes are kept in logical pixels and scaled automatically.

## Window flow

- **First launch:** only **CountDownWindow** is visible; MainWindow and SettingWindows are hidden.
- When the countdown reaches **0**, MainWindow opens and the countdown hides.
- **Tray → Open App** (or double-click the tray icon): opens MainWindow and hides the countdown.
- **Closing MainWindow:** hides MainWindow and restarts CountDownWindow from `60`.
- **Tray → Setting:** shows SettingWindows. Closing it only hides it — the tray menu is the
  only way to show it again.
- **Ignore words…** in SettingWindows opens IgnoreWordsWindows. Its tabs, textarea, error
  banner, and footer are UI-only for now; no validation/save logic is wired yet.
- **Confirm** in SettingWindows saves the four task checkbox drafts, rebuilds MainWindow's
  HTML content, hides Settings/Ignore Words, and shows MainWindow.
- **Cancel** restores the last confirmed checkbox values and hides Settings/Ignore Words.
- **Tray → Exit:** removes the tray icon and closes the app completely.

## Build

Dependencies are declared in `vcpkg.json` (`webview2`). Configure with the vcpkg toolchain:

```powershell
cmake -S . -B build -A x64 `
  "-DCMAKE_TOOLCHAIN_FILE=C:/Program Files/Microsoft Visual Studio/18/Community/VC/vcpkg/scripts/buildsystems/vcpkg.cmake"
cmake --build build --config Release
```

CMake copies `settings.html` and `WebView2Loader.dll` next to the executable.

## Run

```powershell
.\build\Release\Win32HelloWorld.exe
```

The WebView2 Runtime is normally preinstalled on Windows 11. If it is missing, install the
Microsoft WebView2 Runtime before launching.
