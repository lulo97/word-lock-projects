# **Electron app size and RAM optimization**

For your `electron-implement-2` project, I would keep Electron initially and optimize the packaging and runtime architecture before considering a rewrite.

**The key constraint:** Electron bundles its own Chromium and Node.js runtime. The Windows computer does not need a separately installed Node.js, but the app itself must carry the runtime components it needs. You can make the distribution self-contained; you cannot turn a normal Electron app into a tiny standalone native executable just by compiling its JavaScript.

There are three different targets to optimize:

- **Download size:** the installer or portable `.exe`.
- **Installed size:** the complete application folder after installation.
- **RAM usage:** the Electron main process, renderer windows, background services, and MediaPipe processing.

## **1. Biggest opportunities in your actual files**



Highest priority

**Replace the Heavy Pose Landmarker model**



Your project contains:

`assets/pose_landmarker_heavy.task`

The MediaPipe models have these approximate file sizes in current 2026 measurements:

| Model | File size |
| ----- | --------- |
| Heavy | 30.7 MB   |
| Full  | 9.4 MB    |
| Lite  | 5.8 MB    |

Potential saving: approximately 25 MB

Google provides all three model variants. For dumbbell exercise tracking, test Lite first and compare rep-counting accuracy and landmark stability against Heavy.&#x20;

[image](https://www.google.com/s2/favicons?domain=https://developers.google.com\&sz=32)

Google for Developers+2



My recommendation is to use Lite as the default and avoid shipping multiple models unless users genuinely need them.

Other opportunities in your existing files:

| Your files                | Recommended change                                           |
| ------------------------- | ------------------------------------------------------------ |
| `vendor/jquery.min.js`    | Replace jQuery usage with native DOM APIs if practical       |
| `vendor/lodash.min.js`    | Import only required functions or use tree-shakeable modules |
| `assets/tailwind.css`     | Remove unused Tailwind CSS classes                           |
| `assets/themes.css`       | Minify; consider loading only the selected theme             |
| `themes/*/theme.css`      | Keep individual themes separate and load on demand           |
| `themes/*/theme.json`     | Load only the active theme configuration                     |
| `tests/*`                 | Never include in production                                  |
| `test-results/*`          | Exclude                                                      |
| `playwright.config.js`    | Exclude                                                      |
| `scripts/*`               | Exclude build scripts from runtime package                   |
| `tailwind.input.css`      | Exclude after generating production CSS                      |
| `src/**/*.mock.js`        | Exclude test mocks if not used at runtime                    |
| `vendor/mediapipe/wasm/*` | Keep only required MediaPipe runtime files                   |

One important detail: keep the MediaPipe WASM runtime and model available locally. Your app must not depend on downloading them from Google when launched on the new laptop.

## **2. Reduce packaged EXE and installer size**

### **A. Configure electron-builder carefully**

Your project appears to be a good candidate for electron-builder with an explicit production file whitelist.

Example configuration for the conventional electron-builder configuration format:

```
{
  "build": {
    "asar": true,
    "compression": "maximum",
    "electronLanguages": [
      "en-US",
      "ja"
    ],
    "files": [
      "src/**/*",
      "assets/**/*",
      "themes/**/*",
      "vendor/**/*",

      "!tests/**",
      "!test-results/**",
      "!scripts/**",
      "!playwright.config.js",
      "!tailwind.input.css",
      "!src/**/*.mock.js",
      "!**/*.map"
    ],
    "win": {
      "target": [
        {
          "target": "nsis",
          "arch": [
            "x64"
          ]
        }
      ]
    }
  }
}
```

Adjust the whitelist to match the files your HTML pages actually load. Keep the generated production CSS, theme registry, model, and WASM dependencies.

Electron-builder normally includes production dependencies and excludes development dependencies within `node_modules`. However, explicitly excluding your project's root test files is still important.&#x20;

[image](https://www.google.com/s2/favicons?domain=https://www.electron.build\&sz=32)

electron-builder+1



### **B. Remove unnecessary Electron locales**

Electron can contain many language resource files.

You only need the Electron locales actually required by your application.

```
"electronLanguages": ["en-US", "ja"]
```

This is separate from the English and Japanese vocabulary data in your database. electron-builder officially supports this option.&#x20;

[image](https://www.google.com/s2/favicons?domain=https://www.electron.build\&sz=32)

electron-builder+1



### **C. Use ASAR correctly**

Keep:

```
"asar": true
```

ASAR combines application files into an archive. It is not a compression algorithm, so don't expect it to significantly shrink the application by itself.

Keep native `.node` modules and files requiring real filesystem paths outside the archive when necessary. In particular, verify how MediaPipe accesses its model and WASM files.&#x20;

[image](https://www.google.com/s2/favicons?domain=https://www.electronjs.org\&sz=32)

Electron+1



### **D. Minify application code**

Use a production bundler such as esbuild or Rollup.

Recommended production operations:

- JavaScript minification.
- Dead-code elimination.
- Tree shaking.
- CSS minification.
- HTML minification.
- Remove unused dependencies.
- Remove unused polyfills.
- Avoid duplicate dependencies.
- Bundle frequently imported application code.

Electron's own performance guidance specifically recommends avoiding unnecessary modules, lazy loading, and bundling application code.&#x20;

[image](https://www.google.com/s2/favicons?domain=https://www.electronjs.org\&sz=32)

Electron+1



### **E. Avoid unnecessary architectures**

For your target, build only:

```
Windows x64
```

Don't package x86 and ARM64 versions together unless you actually need to support those devices.

### **F. Installer compression**

Try:

```
"compression": "maximum"
```

Compare the output against the normal setting. electron-builder notes that maximum compression does not usually make a noticeable size difference compared with normal compression, while increasing build time.&#x20;

[image](https://www.google.com/s2/favicons?domain=https://www.electron.build\&sz=32)

electron-builder



## **3. Make the app a single EXE without Node.js installed**

You have two good distribution choices.

**Option A: NSIS installer**

Recommended

[\[NSIS + HM NIS Edit\]1\~2. 기본적인 Windows용 설치 프로그램 만들기 |  수성컴전자방](https://images.openai.com/static-rsc-4/VUoaxyyJfKIXsBFueX5IbmKPKf2M0hoGriqgNv4soMsKG0ZYJYAfkxFgXDW4XVdgb7kYbQ5E8NrCjsx-xpihAvKtm9pHaQXiTf1JjWQiDn_9Jy7NSvtAGBgFS7SkpKKlfKVH2i7PvrnRhprO4lKyI84t5TR9NXDCjDhrkSs7Tmk?purpose=inline)

[sooseongcom.com](https://sooseongcom.com/post/nsis-1-hm-nis-edit?utm_source=chatgpt.com)

You distribute one setup EXE. It installs the application, Electron runtime, Chromium, application code, models and other required resources.

Advantages: normal installation, shortcuts, uninstall support and easier updates.

**Option B: Portable EXE**

[\[DEPRECATED\] Simple Classic Theme | WinClassic](https://images.openai.com/static-rsc-4/ViP0656YuurYkVkOjIu2FRlQSVAaKUyed9EoUUg7CtY559KjaKlMbw5a5dznbXuSiKW1CIHpZKV1xsuwGEC42BoW6FoB6pPpeX5bCkkRXVpDJQDrrBMLzJ3M8Yi3b1_dML-g6PRDzDGSRdM_RxKTKEzC7BLuNHJj8NPCnfFIsM8?purpose=inline)

[winclassic.net](https://winclassic.net/thread/456/deprecated-simple-classic-theme?page=33\&utm_source=chatgpt.com)

Use electron-builder's `portable` target.

Advantages: users receive a single executable and do not need a separate Node.js installation.

Disadvantages: the complete application runtime and resources still have to be carried inside the portable distribution. It does not magically remove Chromium or reduce the installed runtime footprint.

Electron's distribution documentation explains that its prebuilt binaries include the runtime needed to execute the application.&#x20;

[image](https://www.google.com/s2/favicons?domain=https://www.electronjs.org\&sz=32)

Electron+1



**My recommendation:** Build NSIS for normal users and optionally provide a portable EXE as a second distribution format.

Do not use an online-only installer if the application must install and work without internet access.

## **4. Major RAM optimization opportunities**

This is where I would focus most of your development effort.

### **A. Reduce BrowserWindow count**

Your project has several window creation modules:

```
src/main/createWindow.js
src/countdown/createCountdownWindow.js
src/dumbbell/overlay.js
src/settings/createSettingsWindow.js
src/settings/createIgnoreWordsWindow.js
src/english/englishInfo.js
src/japanese/japaneseInfo.js
src/mcq/mcqResult.js
```

Every `BrowserWindow` has its own renderer process. Multiple windows can therefore consume significantly more memory than one renderer.&#x20;

[image](https://www.google.com/s2/favicons?domain=https://www.electronjs.org\&sz=32)

Electron+1



I recommend this architecture:

**One main BrowserWindow**

One primary renderer

English

Japanese

MCQ

Settings

Themes

Ignore words

**Temporary windows**

Countdown and dumbbell overlay, only when needed

Instead of creating separate windows for English, Japanese, MCQ results, settings and information pages, consider making these sections inside one renderer.

Only create an additional window when the feature genuinely requires a separate floating or overlay window.

Destroy unused windows instead of keeping all their renderers alive invisibly.

### **B. Load MediaPipe only when necessary**

This is particularly important for your dumbbell feature.

Do not initialize MediaPipe when the application starts.

Recommended lifecycle:

```
Application starts
       |
       v
MediaPipe not initialized
       |
       v
User opens Dumbbell
       |
       v
Load WASM
       |
       v
Load Pose Landmarker Lite
       |
       v
Start webcam
       |
       v
Perform pose detection
       |
       v
User exits Dumbbell
       |
       v
Stop webcam
       |
       v
Cancel animation frames
       |
       v
Close Pose Landmarker
       |
       v
Release references
```

Important improvements:

- Initialize just one PoseLandmarker instance.
- Use `numPoses: 1`.
- Disable segmentation masks when your application doesn't need them.
- Avoid processing every camera frame.
- Discard incoming frames when the previous inference is still running.
- Use a lower camera resolution when sufficient.
- Avoid maintaining unnecessary landmark history.
- Run inference inside a Web Worker where appropriate.
- Close the MediaPipe task when the exercise feature ends.

A smaller model may also reduce inference resource requirements, but benchmark it on actual devices.

### **C. Stop background processes properly**

For the following modules:

```
src/dumbbell/pose-tracker.js
src/dumbbell/dumbbell.js
src/dumbbell/overlay.js
src/tts/tts.js
src/countdown/countdown.js
```

Check for:

- Unclosed timers.
- Endless `requestAnimationFrame` loops.
- Repeated event listeners.
- Duplicate webcam initialization.
- Unreleased video tracks.
- Unused audio objects.
- Unreleased MediaPipe instances.
- Background DOM updates.
- Duplicate IPC listeners.

Use Electron's default background throttling unless your application has a specific reason to disable it. Pause expensive work when the page is hidden.&#x20;

[image](https://www.google.com/s2/favicons?domain=https://www.electronjs.org\&sz=32)

Electron+1



For countdown functionality, calculate remaining time from an absolute end timestamp instead of relying on an interval counter. This allows background timers to be throttled without making the countdown inaccurate.

### **D. Optimize database loading**

Your database modules include:

```
src/database/database.js
src/database/getEnglishWord.js
src/database/getJapaneseWord.js
src/database/getMcq.js
src/database/getRandomJapaneseWord.js
src/database/getRandomMcq.js
src/database/getRandomWord.js
```

Recommendations:

- Avoid loading the entire word database into memory unnecessarily.
- Retrieve only the records required.
- Use prepared SQL statements where appropriate.
- Avoid repeatedly creating database connections.
- Cache only commonly used records.
- Avoid synchronous, expensive database operations on Electron's main thread.
- Store writable database files inside Electron's user-data directory, not inside ASAR.

For large CPU-intensive operations, use a Worker or an Electron utility process rather than blocking the main process.&#x20;

[image](https://www.google.com/s2/favicons?domain=https://www.electronjs.org\&sz=32)

Electron+1



### **E. Reduce idle RAM**

Your app appears to have background functionality involving:

```
src/tray/createTray.js
src/autoStart/
src/chromeFreeze/
```

When idle:

- Keep one main Electron process.
- Avoid unnecessary hidden windows.
- Avoid continuous polling.
- Avoid frequent database queries.
- Avoid unnecessary filesystem scans.
- Avoid repeated theme broadcasts.
- Avoid unnecessary network checks.
- Do not initialize TTS until needed.

Keep hardware acceleration enabled by default. Disabling it may shift rendering work onto the CPU rather than reduce overall resource usage.

## **5. Advanced options worth considering**

| Method                  | File size         | RAM                     | Recommendation            |
| ----------------------- | ----------------- | ----------------------- | ------------------------- |
| esbuild/Rollup          | Lower             | Small improvement       | Yes                       |
| Tree shaking            | Lower             | Small improvement       | Yes                       |
| Tailwind purging        | Lower             | Small improvement       | Yes                       |
| Remove jQuery           | Lower             | Small improvement       | Yes, if unused            |
| Remove unused packages  | Lower             | Potentially lower       | Yes                       |
| Reduce Electron locales | Lower             | Minimal                 | Yes                       |
| Lazy loading            | Similar           | Lower startup RAM       | Yes                       |
| Single renderer         | Similar           | Potentially much lower  | High priority             |
| MediaPipe Lite          | Lower             | Potentially lower       | Test first                |
| Web Workers             | Similar           | Usually similar overall | For responsiveness        |
| Electron Fuses          | Similar           | Negligible              | Security                  |
| UPX executable packing  | Variable          | No meaningful gain      | Avoid                     |
| Custom Electron build   | Potentially lower | Variable                | Not recommended initially |

### **Electron Fuses**

You can disable unnecessary production capabilities such as:

```
runAsNode
nodeOptions
nodeCliInspect
```

You can also enable ASAR integrity validation and restrict loading to the ASAR archive.

Important: Disabling `runAsNode` does not remove Node.js from Electron. It disables a particular execution mode. It also affects `child_process.fork()`, for which Electron recommends its Utility Process API as a replacement.&#x20;

[image](https://www.google.com/s2/favicons?domain=https://www.electronjs.org\&sz=32)

Electron+1



These changes primarily improve security rather than significantly reduce app size or RAM.

## **6. Should you replace Electron with Tauri?**

This is worth considering, especially when package size is your biggest concern.

[DJ.MODE: How to Create Your First Music Player Desktop App from Scratch Using Electron JS - DEV Community](https://images.openai.com/static-rsc-4/C6IGabdU1WOxdqCZTl4NSVFmYppJiXgzPFOpPKWanM_X7BLBPqekgxlQVV0hbcCD_UoF8CkLeuUwFdKNLzyqtfVw1p_QH_XeBT064zsVy5R00zPWt82oqDf5IQGQRr-_D2-c8I3d7V1_uPEYTuFEW7E__Ykcu3bFHzbPDZ9a71U?purpose=inline)

**Electron**

Bundles Chromium and Node.js. No separate Node installation is necessary.

[学习 | Tauri](https://images.openai.com/static-rsc-4/dsE3elsGM6KX6_9KWU8i5Y9vla-BteRmHDh85uEChNfsvb2By4MtbJpNwuA7nSrXD9YSRAFD02Nk6cYkj7rIvqunYzO5H_wIvREMWqq8It5HrM2SyRtTrOqYwSXIgDPw3LxSquUp8_Sg47fiwj7k81WlbFupT2FTc3cm2DdXWMk?purpose=inline)

**Tauri**

Uses Rust and the Windows WebView2 runtime rather than packaging its own Chromium.

Windows 11 includes the Evergreen WebView2 Runtime, making Tauri an attractive alternative when targeting Windows 11 exclusively.&#x20;

[image](https://www.google.com/s2/favicons?domain=https://learn.microsoft.com\&sz=32)

Microsoft Learn+1



However, if you want to package your own WebView2 runtime for offline operation, Tauri's documentation estimates approximately 127 MB for its offline installer option or 180 MB for its fixed-runtime option. Microsoft also describes fixed-version distributions exceeding 250 MB, depending on the distribution.&#x20;

[image](https://www.google.com/s2/favicons?domain=https://v2.tauri.app\&sz=32)

Tauri+1



Tauri also requires migrating your Electron-specific code, including:

- `ipc.js`
- `preload.js`
- `createWindow.js`
- Tray functionality
- Auto-start functionality
- Chrome freeze functionality
- Database integration
- Window management

Tauri doesn't automatically guarantee lower RAM because it still uses a Chromium-based rendering engine.

**My verdict:** Keep Electron for now. Consider Tauri only if you are prepared for a significant rewrite and can rely on the Windows 11 WebView2 runtime.

## **7. How to measure RAM accurately**

Don't rely solely on the main process's `process.memoryUsage()`. That doesn't give you the full memory usage of Chromium and all Electron subprocesses.

Use:

```
const { app } = require('electron');

function printMemoryUsage() {
    const metrics = app.getAppMetrics();

    console.table(
        metrics.map(process => ({
            pid: process.pid,
            type: process.type,
            name: process.name || '',

            workingSetMB:
                (process.memory.workingSetSize / 1024).toFixed(1),

            privateMB:
                process.memory.privateBytes == null
                    ? 'N/A'
                    : (process.memory.privateBytes / 1024).toFixed(1),

            cpu:
                process.cpu.percentCPUUsage.toFixed(1)
        }))
    );
}
```

`app.getAppMetrics()` reports metrics for the application's associated processes. Windows working-set and private-memory measurements are reported in KB.&#x20;

[image](https://www.google.com/s2/favicons?domain=https://www.electronjs.org\&sz=32)

Electron+2



Use this to compare the application in different states:

| State              | What to investigate               |
| ------------------ | --------------------------------- |
| Startup            | Main process and initial renderer |
| Idle for 5 minutes | Background CPU and RAM            |
| English open       | Renderer memory                   |
| Japanese open      | Additional memory                 |
| MCQ open           | Database loading                  |
| Dumbbell active    | MediaPipe and GPU activity        |
| Dumbbell closed    | Whether memory and CPU settle     |
| All windows closed | Remaining tray/background usage   |

## **8. Critical standalone Windows 11 testing**

Your requirement is important: the application must work on a new laptop without separately installing Node.js, Python, npm or other development environments.

Check these dependencies:

| Component             | Requirement                                   |
| --------------------- | --------------------------------------------- |
| Electron              | Bundle its runtime                            |
| Chromium              | Included with Electron                        |
| Node.js               | Must not require external installation        |
| SQLite/native modules | Bundle and rebuild for Electron if applicable |
| MediaPipe WASM        | Bundle locally                                |
| Pose model            | Bundle locally                                |
| TTS                   | Ensure no external runtime is required        |
| CSS, JS and HTML      | Bundle locally                                |
| Themes                | Bundle all supported themes                   |
| Database              | Initialize/store locally                      |
| Fonts                 | Bundle required fonts or use system fonts     |

Pay particular attention to `src/tts/tts.js` and `tests/tts-server.spec.js`. Verify whether TTS starts an external Python or Node executable. If it does, that executable and its required runtime must also be included, or you need another implementation.

I would test the final installer on a clean Windows 11 machine with Node.js absent and internet disconnected. Test the webcam, MediaPipe, TTS, SQLite, tray, auto-start and every window.

## **9. My recommended implementation order**

## **Optimization roadmap**

1. **Replace Heavy with Lite**

   High file-size impact

   Save approximately 25 MB in the raw model asset.
2. **Configure production file exclusions**

   Remove tests, test results, build scripts and unnecessary assets.
3. **Optimize dependencies and frontend builds**

   Remove unused jQuery/polyfills, minify JS/CSS and optimize Tailwind.
4. **Reduce BrowserWindow count**

   Consolidate primary screens into one renderer.
5. **Optimize MediaPipe lifecycle**

   Lazy initialization, one model instance, frame throttling and cleanup.
6. **Optimize database and background functionality**

   Lazy loading, event-driven operations and proper cleanup.
7. **Build standalone Windows x64 installer**

   Test on a clean Windows 11 computer without Node.js.
8. **Profile and repeat**

   Compare installer size, installed size, idle RAM and active-feature RAM.