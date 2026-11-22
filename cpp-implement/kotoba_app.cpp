// kotoba_app.cpp
//
// The Kotoba Lock tray app, extracted from app.cpp so headless feature tests
// (app_test) can drive it without a message loop. See kotoba_app.h for the
// test seams (init/tick/window/isVisible/stateJson/hooks).

#include "kotoba_app.h"

#include "app/script.h"
#include "core/utf.h"
#include "utils/debug_log.h"
#include "utils/set_tray.h"
#include "utils/window_helpers.h"

#include <fstream>

namespace {

constexpr UINT_PTR TIMER_COUNTDOWN = 100;
constexpr UINT_PTR TIMER_TRANSITION = 101;
constexpr UINT_PTR TIMER_INPUT_DONE = 102;

} // namespace

KotobaCppApp::KotobaCppApp() {
    if (const wchar_t* env = _wgetenv(L"KOTOBA_DEBUG"))
        debug_ = env[0] != L'0';
}

KotobaCppApp::~KotobaCppApp() {
    if (countdown_ && countdown_->hwnd())
        remove_tray(countdown_->hwnd());
}

void KotobaCppApp::setDebug(bool d) { debug_ = d; }

bool KotobaCppApp::init() {
    ui::WindowHost::enableDpiAwareness();

    if (!create_countdown())
        return false;

    if (!create_tray_menu())
        return false;

    if (!create_settings())
        return false;

    set_tray(countdown_->hwnd(), nullptr, L"Kotoba Lock");

    set_tray_click_callback(
        countdown_->hwnd(),
        [this](HWND, LPARAM lParam) {
            on_tray_click(lParam);
        }
    );

    start_countdown();

    if (debug_) {
        // Poll debug_cmd.txt every 250 ms while the app runs.
        countdown_->setTimer(200, 250, [this] { poll_debug_commands(); });
    }

    return true;
}

int KotobaCppApp::run() {
    if (!init())
        return 1;

    MSG message{};

    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    return 0;
}

void KotobaCppApp::tick(int n) {
    for (int i = 0; i < n; ++i)
        // Only fire if the countdown timer is pending: a dead timer would
        // never fire in the message pump, so ticking after a stop/skip must
        // not advance the countdown.
        if (countdown_ && countdown_->hasTimer(TIMER_COUNTDOWN))
            on_countdown_tick();

    // Simulate one pass of the message pump: fire any one-shot timers that
    // became pending during the ticks (transition -> show_input,
    // input-done -> show_info). Repeating timers (countdown, debug poll) are
    // driven by the ticks above, so they are not fired here.
    if (countdown_) countdown_->fireOneShotTimers();
    if (trayMenu_)   trayMenu_->fireOneShotTimers();
    if (input_)      input_->fireOneShotTimers();
    if (info_)       info_->fireOneShotTimers();
    if (settings_)   settings_->fireOneShotTimers();
}

// Test seam: deliver a tray-icon event through the exact same path as the
// OS callback (set_tray_click_callback -> on_tray_click(lParam), event in
// the low word of lParam).
void KotobaCppApp::trayEvent(UINT event) {
    on_tray_click(event);
}

ui::WindowHost* KotobaCppApp::window(const std::wstring& name) {
    if (name == L"countdown") return countdown_.get();
    if (name == L"tray")      return trayMenu_.get();
    if (name == L"input")     return input_.get();
    if (name == L"info")      return info_.get();
    if (name == L"settings")  return settings_.get();
    return nullptr;
}

bool KotobaCppApp::isVisible(const std::wstring& name) {
    if (ui::WindowHost* h = window(name))
        return h->hwnd() && IsWindowVisible(h->hwnd());
    return false;
}

nlohmann::json KotobaCppApp::stateJson() {
    nlohmann::json j;
    j["countdown"] = countdownSeconds_;
    j["inputDone"] = inputDone_;
    j["stopped"] = stopped_;
    j["word"] = ui::wideToUtf8(currentWord_.romaji);
    j["inputWin"] = !!input_;
    j["infoWin"] = !!info_;
    j["timers"] = pendingTimerCount();
    j["freezeCalls"] = hooks.freezeCalls;
    j["resumeCalls"] = hooks.resumeCalls;
    return j;
}

void KotobaCppApp::setWord(const std::string& json) {
    currentWord_ = parse_word_json(json);
    reset_word_windows();
}

int KotobaCppApp::pendingTimerCount() const {
    int n = 0;
    if (countdown_) n += countdown_->pendingTimers();
    if (trayMenu_)   n += trayMenu_->pendingTimers();
    if (input_)      n += input_->pendingTimers();
    if (info_)       n += info_->pendingTimers();
    if (settings_)   n += settings_->pendingTimers();
    return n;
}

// ----- window creation -----

bool KotobaCppApp::create_countdown() {
    ui::WindowOptions options;
    options.title = L"Countdown";
    options.autoSizeToContent = true;
    options.popup = true;
    options.topmost = true;
    options.toolWindow = true;
    options.noActivate = true;
    options.centered = false;
    options.visible = true;

    if (!create_host(countdown_, options, L"countdown.html"))
        return false;

    position_bottom_right(*countdown_, 20);
    return true;
}

bool KotobaCppApp::create_tray_menu() {
    ui::WindowOptions options;
    options.title = L"Kotoba Lock";
    options.autoSizeToContent = true;
    options.popup = true;
    options.topmost = true;
    options.toolWindow = true;
    options.centered = false;
    options.visible = false;

    if (!create_host(trayMenu_, options, L"tray_menu.html"))
        return false;

    ui::Document& document = trayMenu_->document();

    document.onClick(L"tray-settings", [this] {
        hide_tray_menu();
        show_settings();
    });

    document.onClick(L"tray-open", [this] {
        hide_tray_menu();
        currentWord_ = hooks.pickWord();
        reset_word_windows();
        show_input();
    });

    document.onClick(L"tray-show", [this] {
        hide_tray_menu();
        start_countdown();
    });

    document.onClick(L"tray-stop", [this] {
        hide_tray_menu();
        // Toggle: "Stop app" while running, "Start app" while stopped.
        if (stopped_) start_countdown();
        else stop_app();
    });

    document.onClick(L"tray-next", [this] {
        hide_tray_menu();
    });

    document.onClick(L"tray-exit", [this] {
        hide_tray_menu();
        quit();
    });

    trayMenu_->onDeactivate = [this] {
        hide_tray_menu();
    };

    return true;
}

bool KotobaCppApp::create_settings() {
    ui::WindowOptions options;
    options.title = L"Kotoba Lock Settings";
    options.width = 560;
    options.height = 680;
    options.centered = true;
    options.visible = false;

    return create_host(settings_, options, L"settings.html");
}

// ----- word windows -----

// Destroy cached input/info windows so they are recreated with
// the new currentWord_ on next show.
void KotobaCppApp::reset_word_windows() {
    UI_LOG("app", L"reset_word_windows word=%s", currentWord_.romaji.c_str());
    if (input_) {
        input_->show(false);
        input_.reset();
    }
    if (info_) {
        info_->show(false);
        info_.reset();
    }
}

bool KotobaCppApp::ensure_input() {
    if (input_)
        return true;

    ui::WindowOptions options;
    options.title = L"Kotoba Lock";
    options.width = 1200;
    options.height = 800;
    options.centered = true;
    options.visible = false;

    if (!create_host(input_, options, L"input.html", &currentWord_))
        return false;

    input_->document().onInput(
        L"answer",
        [this](const std::wstring& value) {
            on_answer_changed(value);
        }
    );

    // Re-show countdown when input window is closed via X.
    input_->onClose = [this] {
        input_->show(false);
        start_countdown();
    };

    return true;
}

bool KotobaCppApp::ensure_info() {
    if (info_)
        return true;

    ui::WindowOptions options;
    options.title = L"Kotoba Lock";
    options.width = 1200;
    options.height = 800;
    options.centered = true;
    options.visible = false;

    if (!create_host(info_, options, L"info.html", &currentWord_))
        return false;

    info_->onKeyDown = [this](UINT key) {
        if (key == VK_RETURN) {
            info_->show(false);
            start_countdown();
            return true;
        }
        return false;
    };

    return true;
}

// ----- countdown -----

void KotobaCppApp::start_countdown() {
    countdownSeconds_ = 30;
    inputDone_ = false;
    stopped_ = false;
    currentWord_ = hooks.pickWord();
    UI_LOG("app", L"start_countdown word=%s seconds=%d",
           currentWord_.romaji.c_str(), countdownSeconds_);
    reset_word_windows();

    if (trayMenu_)
        trayMenu_->document().setText(L"tray-stop", L"Stop app");

    if (input_) input_->show(false);
    if (info_) info_->show(false);

    countdown_->document().setText(
        L"countdown",
        std::to_wstring(countdownSeconds_) + L"s"
    );

    countdown_->killTimer(TIMER_COUNTDOWN);
    countdown_->killTimer(TIMER_TRANSITION);

    InvalidateRect(countdown_->hwnd(), nullptr, TRUE);
    countdown_->show(true);
    position_bottom_right(*countdown_, 20);

    const int kTickMs = debug_ ? 10 : 1000;   // fast-forwarded countdown in debug
    countdown_->setTimer(
        TIMER_COUNTDOWN,
        (UINT)kTickMs,
        [this] {
            on_countdown_tick();
        }
    );
}

// Pause the app: stop the countdown and hide the countdown/input/info
// windows, but keep the tray icon alive. The tray-stop button becomes
// "Start app"; clicking it again runs start_countdown() as usual.
void KotobaCppApp::stop_app() {
    stopped_ = true;
    countdown_->killTimer(TIMER_COUNTDOWN);
    countdown_->killTimer(TIMER_TRANSITION);
    if (input_) input_->killTimer(TIMER_INPUT_DONE);
    countdown_->show(false);
    if (input_) input_->show(false);
    if (info_) info_->show(false);
    if (trayMenu_)
        trayMenu_->document().setText(L"tray-stop", L"Start app");
    UI_LOG("app", L"stop_app word=%s", currentWord_.romaji.c_str());
}

void KotobaCppApp::on_countdown_tick() {
    if (countdownSeconds_ <= 0)
        return;

    --countdownSeconds_;

    countdown_->document().setText(
        L"countdown",
        std::to_wstring(countdownSeconds_) + L"s"
    );

    if (countdownSeconds_ != 0)
        return;

    countdown_->killTimer(TIMER_COUNTDOWN);

    countdown_->setTimer(
        TIMER_TRANSITION,
        400,
        [this] {
            countdown_->killTimer(TIMER_TRANSITION);
            show_input();
        },
        /*oneShot=*/true
    );
}

// ----- input / info -----

void KotobaCppApp::show_input() {
    if (!ensure_input())
        return;

    UI_LOG("app", L"show_input word=%s", currentWord_.romaji.c_str());
    // Opening input cancels any in-flight countdown: kill its timers so a
    // background tick can't reset the answer mid-typing. A real double-click
    // delivers UP, DBLCLK, UP, and the trailing UP would re-toggle the tray
    // menu, so close it now to keep the outcome deterministic.
    hide_tray_menu();
    countdown_->killTimer(TIMER_COUNTDOWN);
    countdown_->killTimer(TIMER_TRANSITION);
    countdown_->show(false);
    if (info_) info_->show(false);

    inputDone_ = false;
    input_->document().setValue(L"answer", L"");
    input_->document().setText(L"feedback", L"");

    input_->show(true);
    ShowWindow(input_->hwnd(), SW_MAXIMIZE);

    if (ui::Element* answer = input_->document().byId(L"answer"))
        input_->document().events().setFocus(answer);
}

void KotobaCppApp::on_answer_changed(const std::wstring& value) {
    if (inputDone_ || currentWord_.romaji.empty())
        return;

    if (ui::toLower(value) != ui::toLower(currentWord_.romaji))
        return;

    inputDone_ = true;
    UI_LOG("app", L"input complete word=%s", currentWord_.romaji.c_str());
    input_->document().setText(L"feedback", L"Correct!");

    input_->setTimer(
        TIMER_INPUT_DONE,
        300,
        [this] {
            input_->killTimer(TIMER_INPUT_DONE);
            show_info();
        },
        /*oneShot=*/true
    );
}

void KotobaCppApp::show_info() {
    // Guard: a stop that raced with a pending input-done timer must not
    // resurrect the info window.
    if (stopped_)
        return;

    if (!ensure_info())
        return;

    UI_LOG("app", L"show_info word=%s", currentWord_.romaji.c_str());
    if (input_) input_->show(false);

    info_->show(true);
    ShowWindow(info_->hwnd(), SW_MAXIMIZE);
}

// ----- settings / tray -----

void KotobaCppApp::show_settings() {
    if (!settings_)
        return;

    settings_->show(true);
    SetForegroundWindow(settings_->hwnd());
}

void KotobaCppApp::on_tray_click(LPARAM lParam) {
    UINT event = LOWORD(lParam);

    switch (event) {
    // Right click: always open the menu, never toggle it closed.
    case WM_CONTEXTMENU:
    case WM_RBUTTONDOWN:
    case WM_RBUTTONUP:
    case WM_RBUTTONDBLCLK:
        show_tray_menu();
        break;

    // Double click: open the input screen right away, skipping the
    // countdown.
    case WM_LBUTTONDBLCLK:
        show_input();
        break;

    // Left click: toggle.
    case NIN_SELECT:
    case NIN_KEYSELECT:
    case WM_LBUTTONUP:
        toggle_tray_menu();
        break;

    default:
        break;
    }
}

void KotobaCppApp::show_tray_menu() {
    if (!trayMenu_)
        return;

    trayMenu_->show(true);
    position_near_cursor(*trayMenu_, 8);
    SetForegroundWindow(trayMenu_->hwnd());
}

void KotobaCppApp::toggle_tray_menu() {
    if (!trayMenu_)
        return;

    if (IsWindowVisible(trayMenu_->hwnd())) {
        hide_tray_menu();
        return;
    }

    show_tray_menu();
}

void KotobaCppApp::hide_tray_menu() {
    if (!trayMenu_)
        return;

    if (trayMenu_->hwnd() && IsWindowVisible(trayMenu_->hwnd()))
        trayMenu_->show(false);
}

void KotobaCppApp::quit() {
    if (countdown_ && countdown_->hwnd())
        remove_tray(countdown_->hwnd());

    PostQuitMessage(0);
}

// ----- debug command file (agent-driven) -----

// Reads debug_cmd.txt, runs its commands against the bound window, and
// writes the report to debug_out.txt. The command file is consumed
// (deleted) after each poll.
void KotobaCppApp::poll_debug_commands() {
    std::ifstream in("debug_cmd.txt");
    if (!in)
        return;
    std::vector<std::string> lines;
    for (std::string l; std::getline(in, l);) lines.push_back(l);
    in.close();
    DeleteFileW(L"debug_cmd.txt");

    std::wofstream out("debug_out.txt");
    for (auto& raw : lines) {
        auto t = ui::ScriptRunner::tokens(ui::utf8ToWide(raw));
        if (t.empty()) continue;

        if (t[0] == L"window") {
            // window input|info|countdown|tray|settings
            ui::WindowHost* h = t[1] == L"input" ? input_.get()
                : t[1] == L"info" ? info_.get()
                : t[1] == L"tray" ? trayMenu_.get()
                : t[1] == L"settings" ? settings_.get()
                : countdown_.get();
            if (!h) { out << L"window not created: " << t[1] << L"\n"; continue; }
            RECT rc; GetClientRect(h->hwnd(), &rc);
            current_.bind(h->document(), rc.right, rc.bottom, h->dpiScale());
            continue;
        }
        if (t[0] == L"app") { app_command(t, out); continue; }
        current_.run(ui::utf8ToWide(raw));   // click/type/dump/shot/assert...
    }
    out << current_.summary();
    InvalidateRect(countdown_->hwnd(), nullptr, FALSE);
}

void KotobaCppApp::app_command(const std::vector<std::wstring>& t, std::wostream& out) {
    if (t[1] == L"state")
        out << L"countdown=" << countdownSeconds_ << L" inputDone=" << inputDone_
            << L" word=" << currentWord_.romaji << L" stopped=" << (int)stopped_
            << L" inputWin=" << !!input_ << L"\n";
    else if (t[1] == L"tick")
        for (int i = 0, n = _wtoi(t[2].c_str()); i < n; ++i) on_countdown_tick();
    else if (t[1] == L"show_input") show_input();
    else if (t[1] == L"show_info") show_info();
    else if (t[1] == L"word") {
        currentWord_ = parse_word_json(read_file(t[2]));
        reset_word_windows();
    }
}

std::string KotobaCppApp::read_file(const std::wstring& path) {
    std::ifstream f(std::string(ui::wideToUtf8(path)), std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(f)), {});
}
