const path = require("path");
const fs = require("fs");

const {
  app,
  BrowserWindow,
  ipcMain,
  Tray,
  Menu,
  nativeImage,
  screen,
} = require("electron");

const config = require("./lib/config");
const settings = require("./lib/settings");
const progress = require("./lib/progress");
const dictionary = require("./lib/dictionary");
const romaji = require("./lib/romaji");
const startup = require("./lib/startup");
const styles = require("./lib/ui-styles");
const { ChromeBlocker } = require("./lib/chrome");
const { ForegroundBooster, windowHandle } = require("./lib/foreground");

// The user will provide pronunciation files; decoding happens in the renderer,
// so allow Chromium to start audio without an extra click.
app.commandLine.appendSwitch("autoplay-policy", "no-user-gesture-required");
app.setAppUserModelId(config.APP_USER_MODEL_ID);
app.setName(config.APP_NAME);

const AUDIO_EXTENSIONS = new Set([
  ".ogg",
  ".oga",
  ".opus",
  ".mp3",
  ".wav",
  ".m4a",
  ".aac",
  ".flac",
  ".webm",
]);

// Compact countdown card. Keep these in sync with the .overlay-card size in
// scripts/build-tailwind.js (the window and the card must match exactly).
const OVERLAY_WIDTH = 140;
const OVERLAY_HEIGHT = 40;
const OVERLAY_MARGIN = 16;

class KotobaApp {
  constructor() {
    this.style = styles.currentDefault();
    this.chrome = new ChromeBlocker();
    // Boot the PowerShell helper now (~1.5 s, once) instead of on first show.
    this.chrome.warmup();
    // Same for the window-activation helper: compile it now so the first gate
    // steals keyboard focus immediately instead of ~1.5 s after it appears.
    this.foreground = new ForegroundBooster();
    // Let the Chrome worker finish compiling first so startup is not two
    // PowerShell Add-Type compilations at once. activate() waits for READY if a
    // gate opens before this fires.
    setTimeout(() => {
      if (!this.quitting && this.foreground) this.foreground.warmup();
    }, 2500).unref?.();

    this.running = true;
    this.countdownEnabled = true;
    this.visible = false;
    this.quitting = false;
    this.cleaned = false;
    this.showing = false;

    this.nextShowAt = null;
    this.showTimer = null;
    this.overlayTimer = null;
    this.showSequence = 0;

    this.mainWindow = null;
    this.mainReady = false;
    this.pendingQuestion = null;
    this.overlayWindow = null;
    this.overlayReady = false;
    this.settingsWindow = null;
    this.tray = null;
    this.lastOverlayStyleKey = null;

    // Creates %USERPROFILE%/kotobalock/config.json on first run.
    settings.ensureFile();

    // Load the Kuromoji dictionary now so the first gate does not wait for it.
    romaji.init().catch((error) => {
      console.error("[Kotoba Lock] romaji init failed:", error);
    });

    // Honour the saved preference and repair a moved/renamed executable.
    if (settings.startWithWindows()) {
      startup.enable();
    }

    this.createMainWindow();
    this.createOverlayWindow();
    this.createTray();
    this.registerIpc();

    this.scheduleAutoShow();
  }

  // ---------------------------------------------------------------- windows

  createMainWindow() {
    const style = this.style;
    this.mainWindow = new BrowserWindow({
      width: 1280,
      height: 820,
      show: false,
      title: config.APP_NAME,
      backgroundColor: style.background || style.background_bottom,
      icon: path.join(config.ASSETS_DIR, "icon.ico"),
      autoHideMenuBar: true,
      roundedCorners: false,
      webPreferences: {
        preload: path.join(__dirname, "preload.js"),
        contextIsolation: true,
        nodeIntegration: false,
        sandbox: false,
        spellcheck: false,
      },
    });

    this.mainWindow.setMenuBarVisibility(false);
    this.mainWindow.loadFile(path.join(__dirname, "src", "index.html"));

    this.mainWindow.webContents.on("did-finish-load", () => {
      this.mainReady = true;
      this.mainWindow.webContents.send("ui:style", styles.describe(this.style));
      if (this.pendingQuestion) {
        this.mainWindow.webContents.send("ui:question", this.pendingQuestion);
      }
    });

    // Closing the window (X / Alt+F4) minimizes to the tray, like the Python app.
    this.mainWindow.on("close", (event) => {
      if (this.quitting) return;
      event.preventDefault();
      this.hide();
    });
  }

  createOverlayWindow() {
    this.overlayWindow = new BrowserWindow({
      width: OVERLAY_WIDTH,
      height: OVERLAY_HEIGHT,
      show: false,
      frame: false,
      transparent: true,
      resizable: false,
      movable: false,
      minimizable: false,
      maximizable: false,
      fullscreenable: false,
      skipTaskbar: true,
      focusable: false,
      hasShadow: false,
      backgroundColor: "#00000000",
      webPreferences: {
        preload: path.join(__dirname, "preload.js"),
        contextIsolation: true,
        nodeIntegration: false,
        sandbox: false,
      },
    });

    this.overlayWindow.setAlwaysOnTop(true, "screen-saver");
    this.overlayWindow.setVisibleOnAllWorkspaces(true, {
      visibleOnFullScreen: true,
    });
    this.overlayWindow.loadFile(path.join(__dirname, "src", "overlay.html"));
    this.overlayWindow.webContents.on("did-finish-load", () => {
      this.overlayReady = true;
      this.lastOverlayStyleKey = null;
      this.paintOverlay();
    });
  }

  createTray() {
    let image = nativeImage.createFromPath(
      path.join(config.ASSETS_DIR, "icon.png"),
    );
    if (image.isEmpty()) {
      image = nativeImage.createEmpty();
    }
    this.tray = new Tray(image);
    this.tray.setToolTip(config.APP_NAME);
    this.tray.on("double-click", () => this.showSafely());
    this.rebuildTrayMenu();
  }

  rebuildTrayMenu() {
    if (!this.tray) return;

    const menu = Menu.buildFromTemplate([
      {
        label: "Open app",
        enabled: this.running,
        click: () => this.showSafely(),
      },
      {
        label: "Next UI style",
        click: () => this.cycleStyle(),
      },
      {
        label: "Settings",
        click: () => this.openSettings(),
      },
      {
        label: this.running ? "Stop app" : "Start app",
        click: () => this.toggleRunning(),
      },
      {
        label: "Show countdown",
        type: "checkbox",
        checked: this.countdownEnabled,
        click: () => this.toggleCountdown(),
      },
      { type: "separator" },
      {
        label: "Exit",
        click: () => this.quit(),
      },
    ]);

    this.tray.setContextMenu(menu);
  }

  // -------------------------------------------------------------- countdown

  cancelAutoShow() {
    if (this.showTimer !== null) {
      clearTimeout(this.showTimer);
      this.showTimer = null;
    }
    this.nextShowAt = null;
    this.stopOverlayClock();
  }

  stopOverlayClock() {
    if (this.overlayTimer !== null) {
      clearInterval(this.overlayTimer);
      this.overlayTimer = null;
    }
    this.hideOverlay();
  }

  scheduleAutoShow() {
    this.cancelAutoShow();
    if (!this.running || this.settingsWindow) return;

    const delayMs = settings.autoShowDelayMs();
    this.nextShowAt = Date.now() + delayMs;
    this.showTimer = setTimeout(() => {
      this.showTimer = null;
      if (this.running) this.showSafely();
    }, delayMs);
    this.updateOverlay();
  }

  updateOverlay() {
    if (this.overlayTimer !== null) {
      clearInterval(this.overlayTimer);
      this.overlayTimer = null;
    }
    if (
      !this.running ||
      this.settingsWindow ||
      !this.countdownEnabled ||
      this.visible ||
      this.nextShowAt === null
    ) {
      this.hideOverlay();
      return;
    }

    this.paintOverlay();
    this.showOverlay();
    this.overlayTimer = setInterval(() => this.updateOverlay(), 250);
  }

  paintOverlay() {
    if (!this.overlayReady || !this.overlayWindow) return;
    const remaining =
      this.nextShowAt === null
        ? 0
        : Math.max(0, (this.nextShowAt - Date.now()) / 1000);
    this.overlayWindow.webContents.send("overlay:set", {
      seconds: remaining,
      style: styles.describe(this.style),
    });
    this.lastOverlayStyleKey = this.style.key;
  }

  positionOverlay() {
    if (!this.overlayWindow) return;
    const { workArea } = screen.getPrimaryDisplay();
    const width = OVERLAY_WIDTH;
    const height = OVERLAY_HEIGHT;
    const margin = OVERLAY_MARGIN;
    const x = Math.max(
      workArea.x + margin,
      workArea.x + workArea.width - width - margin,
    );
    const y = Math.max(
      workArea.y + margin,
      workArea.y + workArea.height - height - margin,
    );
    this.overlayWindow.setBounds({ x, y, width, height });
  }

  showOverlay() {
    if (!this.overlayWindow || !this.overlayReady) return;
    this.positionOverlay();
    try {
      this.overlayWindow.setOpacity(this.style.overlay_alpha);
    } catch (error) {
      // Ignore opacity failures on unsupported compositors.
    }
    this.overlayWindow.showInactive();
  }

  hideOverlay() {
    if (this.overlayWindow && this.overlayWindow.isVisible()) {
      this.overlayWindow.hide();
    }
  }

  // ------------------------------------------------------------- show/hide

  showSafely() {
    this.show().catch((error) => {
      console.error("[Kotoba Lock] show failed:", error);
    });
  }

  async show() {
    if (this.settingsWindow) {
      this.focusSettings();
      return;
    }

    this.cancelAutoShow();
    if (!this.running) return;

    // A second invocation while romaji is being computed must not stack.
    if (this.showing) return;
    this.showing = true;

    // Freeze Chrome immediately, before the (async) word preparation and
    // before the window is shown, so no audio slips through like it does when
    // the freeze waits behind PowerShell startup.
    this.chrome.freeze();

    try {
      if (!this.visible) {
        this.stopAudio();
        const entry = dictionary.loadNextEntry(
          progress.loadAttempts(),
          settings.ignoredWords(),
        );
        const question = await dictionary.presentEntry(entry);
        if (!this.running || this.quitting || this.settingsWindow) {
          this.chrome.unfreeze();
          return;
        }

        this.pendingQuestion = question;
        if (this.mainReady && this.mainWindow && !this.mainWindow.isDestroyed()) {
          this.mainWindow.webContents.send("ui:question", question);
        }
      }

      this.reveal();
    } finally {
      this.showing = false;
      // Never leave Chrome frozen if the round failed before becoming visible.
      if (!this.visible) this.chrome.unfreeze();
    }
  }

  reveal() {
    const win = this.mainWindow;
    if (!win || win.isDestroyed()) return;

    win.show();
    win.maximize();
    win.setAlwaysOnTop(true);
    this.takeFocus(win);
    win.moveTop();

    const sequence = ++this.showSequence;
    setTimeout(() => this.releaseTopmost(sequence), 600);

    this.visible = true;

    // Windows only lets the current foreground process activate another window,
    // so a tray app showing itself from a timer is usually visible but still
    // *not* the keyboard owner. The native helper attaches to the foreground
    // thread and forces the activation; afterwards the page gets focus again so
    // the answer input is ready for typing without a mouse click.
    this.raiseForeground(win, sequence);

    for (const delay of [0, 120, 400, 900]) {
      setTimeout(() => this.activate(sequence), delay);
    }
  }

  raiseForeground(win, sequence) {
    if (!this.foreground) return;
    this.foreground.activate(windowHandle(win)).then((ok) => {
      if (sequence !== this.showSequence || !this.visible || !this.mainWindow) {
        return;
      }
      if (ok) this.takeFocus(this.mainWindow);
      this.activate(sequence);
    });

    // A second attempt once the window is fully painted; harmless when the
    // first one already succeeded because it is skipped while focused.
    setTimeout(() => {
      if (sequence !== this.showSequence || !this.visible || !this.mainWindow) {
        return;
      }
      if (this.mainWindow.isFocused()) return;
      this.foreground.activate(windowHandle(this.mainWindow)).then((ok) => {
        if (!ok || sequence !== this.showSequence || !this.visible) return;
        this.takeFocus(this.mainWindow);
        this.activate(sequence);
      });
    }, 700);
  }

  takeFocus(win) {
    if (!win || win.isDestroyed()) return;
    win.focus();
    try {
      app.focus({ steal: true });
    } catch (error) {
      // app.focus is best-effort on Windows.
    }
    // Keyboard events are routed to the webContents, not just the OS window.
    // After hide()/show() Chromium can keep the last focused window handle but
    // lose the renderer focus, which is why typing did nothing until a click.
    if (win.webContents && !win.webContents.isDestroyed()) {
      win.webContents.focus();
    }
  }

  activate(sequence) {
    if (sequence !== this.showSequence || !this.visible || !this.mainWindow) {
      return;
    }
    const win = this.mainWindow;
    if (win.isDestroyed()) return;
    if (!win.isFocused()) {
      this.takeFocus(win);
      win.moveTop();
    }
    if (win.webContents && !win.webContents.isDestroyed()) {
      win.webContents.focus();
      win.webContents.send("ui:focusAnswer");
    }
  }

  releaseTopmost(sequence) {
    if (sequence !== this.showSequence) return;
    if (this.mainWindow) this.mainWindow.setAlwaysOnTop(false);
  }

  hide() {
    this.showSequence += 1;
    if (this.mainWindow) {
      this.mainWindow.setAlwaysOnTop(false);
      this.stopAudio();
      if (this.mainReady && !this.mainWindow.isDestroyed()) {
        this.mainWindow.webContents.send("ui:cancelFocus");
      }
      this.mainWindow.hide();
    }
    this.visible = false;
    this.chrome.unfreeze();
    if (this.running) this.scheduleAutoShow();
  }

  stopAudio() {
    if (this.mainWindow && !this.mainWindow.isDestroyed() && this.mainReady) {
      this.mainWindow.webContents.send("ui:stopAudio");
    }
  }

  // ----------------------------------------------------------------- styles

  cycleStyle() {
    this.style = styles.randomStyle(this.style);
    if (this.mainWindow && this.mainReady) {
      this.mainWindow.webContents.send("ui:style", styles.describe(this.style));
    }
    if (this.overlayWindow && this.overlayReady) {
      this.paintOverlay();
      try {
        this.overlayWindow.setOpacity(this.style.overlay_alpha);
      } catch (error) {
        // ignore
      }
    }
    this.rebuildTrayMenu();
  }

  // ------------------------------------------------------------ tray toggles

  toggleRunning() {
    this.running = !this.running;
    if (this.running) {
      this.scheduleAutoShow();
    } else {
      this.cancelAutoShow();
      if (this.visible) this.hide();
    }
    this.rebuildTrayMenu();
  }

  toggleCountdown() {
    this.countdownEnabled = !this.countdownEnabled;
    if (this.countdownEnabled) {
      this.updateOverlay();
    } else {
      this.stopOverlayClock();
    }
    this.rebuildTrayMenu();
  }

  // --------------------------------------------------------------- settings

  openSettings() {
    if (this.settingsWindow) {
      this.focusSettings();
      return;
    }

    this.cancelAutoShow();
    if (this.mainWindow && !this.mainWindow.isDestroyed()) {
      this.mainWindow.setEnabled(false);
    }

    this.settingsWindow = new BrowserWindow({
      width: 580,
      height: 730,
      show: false,
      resizable: false,
      title: `${config.APP_NAME} Settings`,
      backgroundColor: this.style.background || this.style.background_bottom,
      icon: path.join(config.ASSETS_DIR, "icon.ico"),
      autoHideMenuBar: true,
      roundedCorners: false,
      webPreferences: {
        preload: path.join(__dirname, "preload.js"),
        contextIsolation: true,
        nodeIntegration: false,
        sandbox: false,
        spellcheck: false,
      },
    });

    this.settingsWindow.setMenuBarVisibility(false);
    this.settingsWindow.loadFile(
      path.join(__dirname, "src", "settings.html"),
    );

    this.settingsWindow.once("ready-to-show", () => {
      if (!this.settingsWindow) return;
      this.centerSettings();
      this.settingsWindow.show();
      this.settingsWindow.focus();
      this.settingsWindow.setAlwaysOnTop(true);
      setTimeout(() => {
        if (this.settingsWindow && !this.settingsWindow.isDestroyed()) {
          this.settingsWindow.setAlwaysOnTop(false);
        }
      }, 700);
    });

    this.settingsWindow.on("closed", () => {
      this.settingsWindow = null;
      if (this.mainWindow && !this.mainWindow.isDestroyed()) {
        this.mainWindow.setEnabled(true);
      }
      if (this.running && !this.visible && !this.quitting) {
        this.scheduleAutoShow();
      }
    });
  }

  centerSettings() {
    if (!this.settingsWindow) return;
    const bounds = this.settingsWindow.getBounds();
    const display = screen.getPrimaryDisplay();
    const area = display.workArea;
    const x = Math.max(area.x, area.x + Math.floor((area.width - bounds.width) / 2));
    const y = Math.max(
      area.y,
      area.y + Math.floor((area.height - bounds.height) / 2) - 40,
    );
    this.settingsWindow.setPosition(x, y);
  }

  focusSettings() {
    if (!this.settingsWindow) return;
    if (this.settingsWindow.isMinimized()) this.settingsWindow.restore();
    this.settingsWindow.show();
    this.settingsWindow.focus();
    this.settingsWindow.setAlwaysOnTop(true);
    setTimeout(() => {
      if (this.settingsWindow && !this.settingsWindow.isDestroyed()) {
        this.settingsWindow.setAlwaysOnTop(false);
      }
    }, 700);
  }

  closeSettings() {
    if (this.settingsWindow && !this.settingsWindow.isDestroyed()) {
      this.settingsWindow.close();
    }
  }

  // ------------------------------------------------------------------- quit

  quit() {
    if (this.quitting) return;
    this.quitting = true;
    this.cleanup();
    app.quit();
  }

  cleanup() {
    if (this.cleaned) return;
    this.cleaned = true;

    if (this.showTimer !== null) {
      clearTimeout(this.showTimer);
      this.showTimer = null;
    }
    if (this.overlayTimer !== null) {
      clearInterval(this.overlayTimer);
      this.overlayTimer = null;
    }

    this.stopAudio();

    try {
      if (this.overlayWindow && !this.overlayWindow.isDestroyed()) {
        this.overlayWindow.destroy();
      }
    } catch (error) {
      // ignore
    }
    this.overlayWindow = null;

    try {
      if (this.tray) this.tray.destroy();
    } catch (error) {
      // ignore
    }
    this.tray = null;

    this.chrome.shutdown();
    if (this.foreground) this.foreground.shutdown();

    try {
      if (this.settingsWindow && !this.settingsWindow.isDestroyed()) {
        this.settingsWindow.destroy();
      }
    } catch (error) {
      // ignore
    }
    this.settingsWindow = null;

    try {
      if (this.mainWindow && !this.mainWindow.isDestroyed()) {
        this.mainWindow.destroy();
      }
    } catch (error) {
      // ignore
    }
    this.mainWindow = null;
  }

  // -------------------------------------------------------------------- ipc

  registerIpc() {
    ipcMain.handle("app:bootstrap", () => ({
      appName: config.APP_NAME,
      configFile: settings.CONFIG_FILE,
      style: styles.describe(this.style),
      volume: settings.soundVolume(),
    }));

    ipcMain.handle("progress:record", (event, word) => {
      return progress.recordSuccess(word);
    });

    ipcMain.handle("settings:ignoreWord", (event, word) => {
      try {
        settings.addIgnoredWord(word);
        return { ok: true };
      } catch (error) {
        return { ok: false, error: String(error.message || error) };
      }
    });

    ipcMain.handle("audio:read", async (event, filePath) => {
      if (typeof filePath !== "string" || !filePath) {
        throw new Error("invalid audio path");
      }
      const resolved = path.resolve(filePath);
      const extension = path.extname(resolved).toLowerCase();
      if (!AUDIO_EXTENSIONS.has(extension)) {
        throw new Error("unsupported audio format");
      }
      let stat;
      try {
        stat = await fs.promises.stat(resolved);
      } catch (error) {
        throw new Error("audio file not found");
      }
      if (!stat.isFile()) throw new Error("audio file not found");
      return fs.promises.readFile(resolved);
    });

    ipcMain.handle("audio:volume", () => settings.soundVolume());

    ipcMain.on("ui:hide", () => this.hide());

    ipcMain.on("ui:exitCard", () => {
      this.hide();
      setTimeout(() => {
        if (!this.quitting) this.cycleStyle();
      }, 60);
    });

    ipcMain.handle("settings:init", () => {
      const data = settings.load();
      return {
        configFile: settings.CONFIG_FILE,
        style: styles.describe(this.style),
        styleName: this.style.name,
        delayMs: settings.autoShowDelayMs(),
        volume: settings.soundVolume(),
        minVolume: settings.MIN_SOUND_VOLUME,
        maxVolume: settings.MAX_SOUND_VOLUME,
        ignoreWords: data.IGNORE_WORDS,
        startWithWindows: data.START_WITH_WINDOWS,
      };
    });

    ipcMain.handle("settings:save", (event, payload) => {
      const data = payload && typeof payload === "object" ? payload : {};

      const rawDelay = String(data.delayMs === undefined ? "" : data.delayMs).trim();
      const delay = Number(rawDelay);
      if (!rawDelay || !Number.isFinite(delay) || Math.trunc(delay) <= 0) {
        return {
          ok: false,
          error: "AUTO_SHOW_DELAY_MS must be a number in milliseconds.",
        };
      }

      const volume = Number(data.volume);
      if (!Number.isFinite(volume)) {
        return { ok: false, error: "SOUND_VOLUME must be a number." };
      }

      const words = Array.isArray(data.ignoreWords)
        ? data.ignoreWords
        : settings.parseWords(data.ignoreWords);

      const wantStartup = Boolean(data.startWithWindows);
      if (wantStartup) {
        if (!startup.enable()) {
          return {
            ok: false,
            error:
              "Could not add Kotoba Lock to Windows startup (registry write failed).",
          };
        }
      } else if (!startup.disable()) {
        return {
          ok: false,
          error:
            "Could not remove Kotoba Lock from Windows startup (registry write failed).",
        };
      }

      try {
        settings.saveConfig(Math.trunc(delay), words, volume, wantStartup);
      } catch (error) {
        return {
          ok: false,
          error: `Could not save config.json: ${error.message || error}`,
        };
      }

      return { ok: true };
    });

    ipcMain.on("settings:close", () => this.closeSettings());
  }
}

// ------------------------------------------------------------------ bootstrap

const hasLock = app.requestSingleInstanceLock();
let instance = null;

if (!hasLock) {
  app.quit();
} else {
  app.on("second-instance", () => {
    if (instance) {
      instance.showSafely();
    }
  });

  app.whenReady().then(() => {
    instance = new KotobaApp();

    app.on("activate", () => {
      if (instance) instance.showSafely();
    });
  });
}

app.on("before-quit", () => {
  if (instance) {
    instance.quitting = true;
    instance.cleanup();
  }
});

app.on("window-all-closed", () => {
  // Kotoba Lock lives in the tray; windows are hidden, not closed.
});

process.on("exit", () => {
  if (instance) instance.chrome.shutdown();
});

// Keep a reference for debugging/inspection and allow the tray app to survive
// unhandled renderer errors instead of exiting silently.
process.on("uncaughtException", (error) => {
  console.error("[Kotoba Lock]", error);
});
