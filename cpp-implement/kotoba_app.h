#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <ostream>
#include "document.h"
#include "app/script.h"
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
        std::function<WordData()> pickWord = pick_random_word;  // fixed word in tests
        std::function<void()> freezeChrome = [] {};             // no-op default
        std::function<void()> resumeChrome = [] {};             // no-op default
        int freezeCalls = 0;
        int resumeCalls = 0;
    } hooks;

    KotobaCppApp();
    ~KotobaCppApp();

    void setDebug(bool d);

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

    int countdownSeconds_ = 30;
    bool inputDone_ = false;
    bool debug_ = false;
    bool stopped_ = false;
    WordData currentWord_{};
    ui::ScriptRunner current_;          // bound to a window via `window` command

    // ----- window creation -----
    bool create_countdown();
    bool create_tray_menu();
    bool create_settings();

    // ----- word windows -----
    void reset_word_windows();
    bool ensure_input();
    bool ensure_info();

    // ----- countdown -----
    void start_countdown();
    void stop_app();
    void on_countdown_tick();

    // ----- input / info -----
    void show_input();
    void on_answer_changed(const std::wstring& value);
    void show_info();

    // ----- settings / tray -----
    void show_settings();
    void on_tray_click(LPARAM lParam);
    void show_tray_menu();
    void toggle_tray_menu();
    void hide_tray_menu();
    void quit();

    // ----- debug command file (agent-driven) -----
    void poll_debug_commands();
    void app_command(const std::vector<std::wstring>& t, std::wostream& out);

    static std::string read_file(const std::wstring& path);
    int pendingTimerCount() const;
};
