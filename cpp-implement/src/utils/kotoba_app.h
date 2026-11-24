#pragma once

#include <deque>
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <ostream>
#include "document.h"
#include "app/script.h"
#include "utils/config.h"
#include "utils/progress.h"
#include "utils/startup_registry.h"
#include "utils/theme_registry.h"
#include "utils/word_db.h"
#include "json.hpp"

// The Kotoba Lock tray app, extracted from app.cpp so headless feature tests
// (app_test) can drive it without a message loop:
//   - init()        creates the real (hidden) windows, no GetMessage
//   - tick(n)       simulates n countdown ticks + any pending one-shot timers
//   - window(name)  the WindowHost for "countdown"/"tray"/"input"/"info"/"settings"
//   - isVisible()   IsWindowVisible for the named window
//   - stateJson()   all observable state, for `app assert <key> == <value>`
//   - hooks         deterministic fakes (fixed word, no real Chrome suspend)
class KotobaCppApp {
public:
    // Test seams: swap app internals for deterministic fakes.
    struct Hooks {
        // M9 Chrome freeze backend. Null -> the real freeze_chrome() /
        // resume_chrome(); app_test installs fakes that only count calls,
        // so feature tests never touch a real Chrome process.
        std::function<int()> freezeChrome;
        std::function<int()> resumeChrome;
        int freezeCalls = 0;
        int resumeCalls = 0;
        // Config/progress file paths. Empty -> default %USERPROFILE%\kotoba
        // (the real app); app_test points these at the feature-test folder
        // so each test is self-contained and deterministic.
        std::wstring configPath;
        std::wstring progressPath;
        // M5 audio backend. app_test installs a fake that only records the
        // request, so feature tests never produce sound. Empty -> real
        // play_audio()/stop_audio().
        std::function<bool(const std::wstring&, float)> playAudio;
        std::function<void()> stopAudio;
        // M8 "start with Windows" backend. Empty -> the real HKCU Run key;
        // app_test installs a FakeStartupRegistry so tests never touch it.
        std::shared_ptr<StartupRegistry> startupRegistry;
    } hooks;

    KotobaCppApp();
    ~KotobaCppApp();

    void setDebug(bool d);

    // Login launch (--minimized): the first countdown starts in the tray
    // without showing its overlay; later starts behave normally.
    void setMinimized(bool m) { minimized_ = m; }

    // Create all windows (countdown, tray, settings) and start the countdown,
    // WITHOUT entering a message loop. Tests call this, then drive the app
    // directly. Returns false if window creation failed.
    bool init();

    // Simulate time: call on_countdown_tick() n times, then fire any pending
    // one-shot timers (transition, input-done) once. No real timers, no sleep.
    // The countdown tick only fires while its timer is pending, matching
    // the message pump (a dead timer never fires).
    void tick(int n = 1);

    // Simulate a real tray-icon event: the OS delivers these to
    // on_tray_click via set_tray_click_callback. Test seam for headless
    // tests (WM_LBUTTONDBLCLK = double click, WM_LBUTTONUP = left click,
    // WM_RBUTTONUP = right click).
    void trayEvent(UINT event);

    // Register the `app` and `window` commands on a ScriptRunner, shared by
    // app_test, ui_test and the debug command-file poller, so there is one
    // implementation of the app command vocabulary (no drift).
    void registerCommands(ui::ScriptRunner& runner);

    // Window host by name: "countdown", "tray", "input", "info", "settings".
    ui::WindowHost* window(const std::wstring& name);

    // IsWindowVisible for the named window (false if not created).
    bool isVisible(const std::wstring& name);

    // All observable state as JSON (for `app assert <key> == <value>`).
    nlohmann::json stateJson();

    // Set the current word from a JSON string and reset the word windows.
    void setWord(const std::string& json);

    // init() + message loop. The real app entry point.
    int run();

private:
    std::unique_ptr<ui::WindowHost> countdown_;
    std::unique_ptr<ui::WindowHost> trayMenu_;
    std::unique_ptr<ui::WindowHost> settings_;
    std::unique_ptr<ui::WindowHost> input_;
    std::unique_ptr<ui::WindowHost> info_;

    // M9 crash fix: hosts retired by reset_word_windows() while a handler of
    // theirs may still be on the stack. They are parked here and destroyed at
    // a safe point (ensure_input / ensure_info / dtor) where no handler of the
    // retired window can be running. This prevents the use-after-free in
    // EventState::onMouseUp / ScriptRunner::settle / Document::handleMessage
    // that caused the intermittent 0xC0000005 in chrome_freeze.
    std::vector<std::unique_ptr<ui::WindowHost>> graveyard_;
    void reap_windows() { graveyard_.clear(); }
    
    int countdownSeconds_ = 30;
    bool inputDone_ = false;
    bool debug_ = false;
    bool stopped_ = false;
    bool minimized_ = false;                 // --minimized, consumed at first start
    WordData currentWord_{};
    ui::ScriptRunner current_;          // bound to a window via `window` command
    ConfigStore config_;
    ProgressStore progress_;
    std::shared_ptr<StartupRegistry> startupRegistry_;   // set in init()
    ThemeRegistry themes_;                   // embedded assets/themes/registry.txt
    std::wstring currentTheme_;              // active style for this word
    std::wstring pendingTheme_;              // forced style for the next reset
    bool themeChanged_ = false;              // last reset picked a different style
    // Style each chrome window was built with, so a theme change can rebuild
    // it lazily without touching a window whose handler is on the stack.
    std::wstring countdownTheme_;
    std::wstring trayTheme_;
    std::wstring settingsTheme_;
    std::deque<std::wstring> recentPicks_;   // wordKey of each pick (newest last)
    std::vector<WordData> extraWords_;       // `app add-word` test seam (M4)
    int dbRejected_ = 0;                     // words rejected by the light check
    bool chromeFrozen_ = false;               // M9: Chrome suspended by us

    // ----- audio (M5) -----
    std::wstring audioLast_;                 // JSON path of the last clip
    bool audioStarted_ = false;              // a backend accepted the last play
    int audioReady_ = 0;                     // playable buttons on the info screen
    int audioDisabled_ = 0;                  // greyed buttons (missing audio)
    std::wstring audioAnimId_;               // button currently showing "Speaking..."
    std::wstring audioAnimLabel_;            // its normal label to restore
    int audioAnimStep_ = 0;                  // 0..3, dots shown

    bool audioPlaying() const;
    void playAudioClip(const std::wstring& relative, const std::wstring& buttonId);
    void stopAudio();
    // Build the info screen's audio buttons from currentWord_ (speak + one
    // listen per example). Rows are cloned from <template id="example-row">.
    void populateInfoAudio();
    // Numbers the tag chips from currentWord_.tags, or hides #tags when empty.
    void populateInfoTags();
    // "Speaking." / ".." / "..." animation on the button being played.
    void startAudioAnimation(const std::wstring& id);
    void onAudioAnimationTick();

    // Pick the next word with the even picker (M1) and record the pick in
    // progress. start_countdown() and tray-open both go through here.
    void pick_word();

    // ----- window creation -----
    bool create_countdown();
    bool create_tray_menu();
    bool create_settings();
    void register_countdown();
    void wire_tray_menu();
    void wire_settings();

    // Rebuild a chrome window when the active style changed. The tray menu is
    // only rebuilt when it is shown (a click handler may still be running);
    // settings likewise. The countdown is rebuilt before a new countdown
    // starts, where its timers are re-armed anyway.
    void ensure_countdown_theme();
    void ensure_tray_theme();
    void ensure_settings_theme();

    // ----- word windows -----
    // Theme page for the current style, or the legacy assets/htmls page when
    // no theme is active. Used by all five windows, so a style can restyle
    // the countdown, settings and tray menu as well as input/info.
    std::wstring themed_page_html(const wchar_t* legacyName) const;
    void reset_word_windows();
    bool ensure_input();
    bool ensure_info();

    // ----- countdown -----
    void start_countdown();
    void stop_app();
    void on_countdown_tick();

    // ----- chrome freeze (M9) -----
    // Idempotent, gated on config.freezeChrome. resume clears the flag
    // before calling the backend so it never double-resumes.
    void chrome_freeze();
    void chrome_resume();

    // ----- input / info -----
    void show_input();
    void on_answer_changed(const std::wstring& value);
    void show_info();

    // ----- settings / tray -----
    void show_settings();
    void apply_settings();
    void next_theme();
    // Make the Run key match `enabled`, rewriting a moved/renamed exe path.
    void syncStartupRegistry(bool enabled);
    void applyStartup(bool enabled);
    void on_tray_click(LPARAM lParam);
    void show_tray_menu();
    void toggle_tray_menu();
    void hide_tray_menu();
    void quit();

    // ----- debug command file (agent-driven) -----
    void poll_debug_commands();
    // `app <subcommand>` handler, registered via registerCommands().
    void appCmd(const std::vector<std::wstring>& t, ui::ScriptRunner& runner);

    static std::string read_file(const std::wstring& path);
    int pendingTimerCount() const;
};
