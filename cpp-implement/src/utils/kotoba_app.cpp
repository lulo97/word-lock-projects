// kotoba_app.cpp
//
// The Kotoba Lock tray app, extracted from app.cpp so headless feature tests
// (app_test) can drive it without a message loop. See kotoba_app.h for the
// test seams (init/tick/window/isVisible/stateJson/hooks).

#include "utils/kotoba_app.h"

#include "app/script.h"
#include "controls/checkbox.h"
#include "core/utf.h"
#include "dom/tree_ops.h"
#include "utils/debug_log.h"
#include "utils/play_audio.h"
#include "utils/set_tray.h"
#include "utils/window_helpers.h"

#include <fstream>

namespace {

constexpr UINT_PTR TIMER_COUNTDOWN = 100;
constexpr UINT_PTR TIMER_TRANSITION = 101;
constexpr UINT_PTR TIMER_INPUT_DONE = 102;
constexpr UINT_PTR TIMER_AUDIO_ANIM = 103;
constexpr UINT AudioAnimIntervalMs = 350;

// Greyed button styling for a clip that is missing from the database.
const wchar_t* kSpeakDisabledClasses =
    L"px-6 py-1.5 bg-[#0f172a] border border-[#1e293b] text-gray-600 text-sm rounded-sm";
const wchar_t* kListenDisabledClasses =
    L"px-4 py-1 bg-[#0f172a] border border-[#1e293b] text-gray-600 text-xs "
    L"font-semibold rounded-sm mt-1";

ui::Element* find_by_class(ui::Element& root, const std::wstring& cls) {
    ui::Element* found = nullptr;
    ui::walk(root, [&](ui::Element& el) {
        if (found)
            return true;
        for (const auto& name : el.classList) {
            if (name == cls) {
                found = &el;
                return false;
            }
        }
        return true;
    });
    return found;
}

std::wstring first_non_empty(const std::wstring& a, const std::wstring& b) {
    return a.empty() ? b : a;
}

} // namespace

KotobaCppApp::KotobaCppApp() {
    if (const wchar_t* env = _wgetenv(L"KOTOBA_DEBUG"))
        debug_ = env[0] != L'0';
    // The debug poller's runner gets the same `app`/`window` commands as
    // app_test and ui_test: one vocabulary, one implementation.
    registerCommands(current_);
}

KotobaCppApp::~KotobaCppApp() {
    stopAudio();
    if (countdown_ && countdown_->hwnd())
        remove_tray(countdown_->hwnd());
}

void KotobaCppApp::setDebug(bool d) { debug_ = d; }

bool KotobaCppApp::init() {
    ui::WindowHost::enableDpiAwareness();

    // Config/progress live in %USERPROFILE%\kotoba by default; tests point
    // the hooks at their own folder (self-contained, deterministic).
    config_ = ConfigStore(hooks.configPath.empty() ? ConfigStore::defaultPath()
                                                   : hooks.configPath);
    progress_ = ProgressStore(hooks.progressPath.empty() ? ProgressStore::defaultPath()
                                                          : hooks.progressPath);
    if (!config_.load() || !progress_.load())
        return false;

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

void KotobaCppApp::registerCommands(ui::ScriptRunner& runner) {
    runner.addCommand(L"window", 1, L"window <countdown|tray|input|info|settings>",
        [this, &runner](const auto& t, const auto&) {
            if (ui::WindowHost* h = window(t[1])) {
                RECT rc; GetClientRect(h->hwnd(), &rc);
                runner.bind(h->document(), rc.right, rc.bottom, h->dpiScale());
            } else {
                // Unbind so later commands fail clearly instead of running
                // against a stale document.
                runner.unbind();
                runner.fail(L"window not created: " + t[1]);
            }
        });
    runner.addCommand(L"app", 1,
        L"app <init|tick [n]|word <file>|add-word <file>|assert <key> == <value>|state|tray-event <event>>",
        [this, &runner](const auto& t, const auto&) { appCmd(t, runner); });
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
    j["picked"] = (int)recentPicks_.size();
    j["wordCount"] = count_words() + (int)extraWords_.size();
    j["dbRejected"] = dbRejected_;
    j["audio"] = audioPlaying() ? "playing" : "idle";
    j["audio_last"] = audioLast_.empty() ? "none" : ui::wideToUtf8(audioLast_);
    j["audio_ready"] = audioReady_;
    j["audio_disabled"] = audioDisabled_;
    return j;
}

// ----- audio (M5) -----

bool KotobaCppApp::audioPlaying() const {
    if (hooks.playAudio)
        return audioStarted_;
    return audioStarted_ && is_audio_playing();
}

void KotobaCppApp::stopAudio() {
    if (info_) {
        info_->killTimer(TIMER_AUDIO_ANIM);
        if (!audioAnimId_.empty()) {
            info_->document().setText(audioAnimId_, audioAnimLabel_);
            info_->document().invalidatePaint();
        }
    }
    audioAnimId_.clear();
    audioAnimLabel_.clear();
    audioAnimStep_ = 0;

    if (hooks.stopAudio)
        hooks.stopAudio();
    else if (!hooks.playAudio)
        stop_audio();
    audioStarted_ = false;
}

void KotobaCppApp::startAudioAnimation(const std::wstring& id) {
    if (!info_ || id.empty())
        return;

    ui::Element* button = info_->document().byId(id);
    if (!button)
        return;

    // Remember the normal label ("Listen" / "Speak") so stopAudio() can
    // restore it, then start the dot animation immediately.
    audioAnimId_ = id;
    audioAnimLabel_ = ui::textContent(*button);
    audioAnimStep_ = 0;

    onAudioAnimationTick();
}

void KotobaCppApp::onAudioAnimationTick() {
    if (!info_ || audioAnimId_.empty())
        return;

    // The clip may have finished naturally since the last tick.
    if (!audioPlaying()) {
        stopAudio();
        return;
    }

    audioAnimStep_ = audioAnimStep_ % 3 + 1;
    info_->document().setText(
        audioAnimId_,
        L"Speaking" + std::wstring(static_cast<size_t>(audioAnimStep_), L'.')
    );
    info_->document().invalidatePaint();

    // One-shot chain (not a repeating timer): app_test's `app tick` advances
    // the animation deterministically, one step per simulated pump pass.
    info_->setTimer(
        TIMER_AUDIO_ANIM,
        AudioAnimIntervalMs,
        [this] { onAudioAnimationTick(); },
        /*oneShot=*/true
    );
}

void KotobaCppApp::playAudioClip(const std::wstring& relative,
                                 const std::wstring& buttonId) {
    if (relative.empty() || !audio_asset_exists(relative))
        return;

    // One clip at a time: a new button press stops (and un-animates) the
    // previous one. Rapid clicks just restart; nothing stacks up.
    stopAudio();

    if (hooks.playAudio) {
        if (hooks.playAudio(relative, config_.get().volume)) {
            audioLast_ = relative;
            audioStarted_ = true;
            startAudioAnimation(buttonId);
        }
        return;
    }

    const std::wstring file = prepare_audio_asset(relative);
    if (file.empty()) {
        UI_LOG("app", L"audio missing at runtime: %s", relative.c_str());
        return;
    }

    if (play_audio(file, false, config_.get().volume)) {
        audioLast_ = relative;
        audioStarted_ = true;
        startAudioAnimation(buttonId);
    } else {
        UI_LOG("app", L"audio failed to play: %s", relative.c_str());
    }
}

void KotobaCppApp::populateInfoAudio() {
    audioReady_ = 0;
    audioDisabled_ = 0;
    if (!info_)
        return;

    ui::Document& document = info_->document();

    auto wire = [&](const std::wstring& id, const std::wstring& relative,
                    const wchar_t* disabledClasses) {
        ui::Element* button = document.byId(id);
        if (!button)
            return;
        if (relative.empty() || !audio_asset_exists(relative)) {
            // Missing file: grey the button out, bind nothing, never error.
            document.setClass(id, disabledClasses);
            ++audioDisabled_;
            return;
        }
        document.onClick(id, [this, relative, id] {
            playAudioClip(relative, id);
        });
        ++audioReady_;
    };

    wire(L"speak", currentWord_.audio, kSpeakDisabledClasses);

    document.clearChildren(L"examples");

    int index = 0;
    for (const auto& example : currentWord_.examples) {
        ++index;
        ui::Element* row = document.cloneTemplate(L"example-row", L"examples", {});
        if (!row)
            break;

        if (ui::Element* el = find_by_class(*row, L"jp"))
            ui::setText(*el, L"• " + first_non_empty(example.jp, example.ja));
        if (ui::Element* el = find_by_class(*row, L"romaji"))
            ui::setText(*el, first_non_empty(example.romaji, example.reading));
        if (ui::Element* el = find_by_class(*row, L"en"))
            ui::setText(*el, first_non_empty(example.en, example.vi));

        ui::Element* listen = find_by_class(*row, L"listen");
        if (!listen)
            continue;

        const std::wstring id = L"listen-" + std::to_wstring(index);
        listen->id = id;
        wire(id, example.audio, kListenDisabledClasses);
    }
}

void KotobaCppApp::populateInfoTags() {
    if (!info_)
        return;

    ui::Document& document = info_->document();
    document.clearChildren(L"tags-list");

    if (currentWord_.tags.empty()) {
        // No tags: hide the whole row instead of leaving literal placeholders.
        document.setVisible(L"tags", false);
        return;
    }

    document.setVisible(L"tags", true);

    int index = 0;
    for (const auto& tag : currentWord_.tags) {
        ++index;
        ui::Element* chip = document.cloneTemplate(L"tag-chip", L"tags-list", {});
        if (!chip)
            break;
        chip->id = L"tag-" + std::to_wstring(index);
        ui::setText(*chip, tag);
    }
}

void KotobaCppApp::pick_word() {
    currentWord_ = pick_even_word(config_.get().ignoreWords,
                                  progress_.get().attempts,
                                  extraWords_);
    // Empty: no eligible word (empty or fully ignored database). Nothing to
    // record; the UI will show blank fields rather than a bogus "" key.
    if (currentWord_.kanji.empty() && currentWord_.word.empty())
        return;
    std::wstring key = wordKey(currentWord_);
    recentPicks_.push_back(key);
    progress_.record(key);
    progress_.save();
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
        pick_word();
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

    if (!create_host(settings_, options, L"settings.html"))
        return false;

    // M6: every control persists immediately through Config::save().
    ui::Document& doc = settings_->document();
    const Config& c = config_.get();

    doc.onInput(L"countdown", [this](const std::wstring& value) {
        int seconds = _wtoi(value.c_str());
        if (seconds < 1 || seconds > 359999)
            return;   // ignore garbage; keep the last valid value
        Config nc = config_.get();
        nc.countdownSeconds = seconds;
        config_.set(nc);
        config_.save();
    });

    doc.onInput(L"ignore-words", [this](const std::wstring& value) {
        std::vector<std::wstring> words;
        std::wstring current;
        for (wchar_t ch : value) {
            if (ch == L'\n' || ch == L'\r' || ch == L',') {
                std::wstring w = ui::trim(current);
                if (!w.empty())
                    words.push_back(std::move(w));
                current.clear();
            } else {
                current += ch;
            }
        }
        std::wstring w = ui::trim(current);
        if (!w.empty())
            words.push_back(std::move(w));
        Config nc = config_.get();
        nc.ignoreWords = std::move(words);
        config_.set(nc);
        config_.save();
    });

    doc.onChange(L"volume", [this](float value) {
        Config nc = config_.get();
        nc.volume = value;
        config_.set(nc);
        config_.save();
        wchar_t buf[32];
        swprintf(buf, 32, L"x%g", value);
        settings_->document().setText(L"volume-value", buf);
    });

    auto wire_check = [this, &doc](const std::wstring& id, bool Config::*flag) {
        if (ui::Element* el = doc.byId(id))
            if (auto* check = dynamic_cast<ui::Checkbox*>(el->control.get())) {
                check->onChange = [this, flag](bool v) {
                    Config nc = config_.get();
                    nc.*flag = v;
                    config_.set(nc);
                    config_.save();
                };
                // Fires onChange only when the HTML attr differs from config.
                check->setChecked(config_.get().*flag);
            }
    };
    wire_check(L"random-ui", &Config::randomUi);
    wire_check(L"start-with-windows", &Config::startWithWindows);
    wire_check(L"freeze-chrome", &Config::freezeChrome);

    doc.onClick(L"close", [this] { settings_->show(false); });

    // Initial values. setValue fires onChange, which re-persists the same
    // value (idempotent); the volume handler also refreshes the label.
    doc.setValue(L"countdown", std::to_wstring(c.countdownSeconds));
    doc.setValue(L"volume", std::to_wstring(c.volume));

    return true;
}

// ----- word windows -----

// Destroy cached input/info windows so they are recreated with
// the new currentWord_ on next show.
void KotobaCppApp::reset_word_windows() {
    UI_LOG("app", L"reset_word_windows word=%s", currentWord_.romaji.c_str());
    stopAudio();
    if (input_) {
        input_->show(false);
        input_.reset();
    }
    if (info_) {
        info_->show(false);
        info_.reset();
    }
    audioReady_ = 0;
    audioDisabled_ = 0;
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

    // Wire the word + example buttons before the screen is first shown.
    populateInfoTags();
    populateInfoAudio();

    info_->onClose = [this] {
        stopAudio();
    };

    info_->onKeyDown = [this](UINT key) {
        if (key == VK_RETURN) {
            stopAudio();
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
    countdownSeconds_ = config_.get().countdownSeconds;
    inputDone_ = false;
    stopped_ = false;
    pick_word();
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
    stopAudio();
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
    // Defensive: the tray commands can be driven before init() in a test
    // script; countdown_ is only guaranteed by init().
    if (!ensure_input() || !countdown_)
        return;

    UI_LOG("app", L"show_input word=%s", currentWord_.romaji.c_str());
    stopAudio();
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
    stopAudio();
    if (input_) input_->show(false);

    info_->show(true);
    ShowWindow(info_->hwnd(), SW_MAXIMIZE);
}

// ----- settings / tray -----

void KotobaCppApp::show_settings() {
    if (!settings_)
        return;

    // Refresh the fields from config every time the window opens, so the
    // controls always show the current values (setValue fires onChange,
    // which re-persists the same value; harmless).
    ui::Document& doc = settings_->document();
    const Config& c = config_.get();
    doc.setValue(L"countdown", std::to_wstring(c.countdownSeconds));

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
    stopAudio();
    if (countdown_ && countdown_->hwnd())
        remove_tray(countdown_->hwnd());

    PostQuitMessage(0);
}

// ----- debug command file (agent-driven) -----

// Reads debug_cmd.txt, runs its commands through the shared ScriptRunner
// (same vocabulary as app_test/ui_test), and writes the report to
// debug_out.txt. The command file is consumed (deleted) after each poll.
void KotobaCppApp::poll_debug_commands() {
    // One report per poll: the runner is long-lived, so drop the previous
    // poll's notes/failures instead of re-printing them every time.
    current_.resetReport();

    std::ifstream in("debug_cmd.txt");
    if (!in)
        return;
    std::vector<std::string> lines;
    for (std::string l; std::getline(in, l);) lines.push_back(l);
    in.close();
    DeleteFileW(L"debug_cmd.txt");

    std::wofstream out("debug_out.txt");
    int line = 0;
    for (auto& raw : lines) {
        ++line;
        current_.setLocation(L"debug_cmd.txt", line);
        current_.run(ui::utf8ToWide(raw));   // click/type/dump/shot/assert/app/window
    }
    out << current_.summary();
    InvalidateRect(countdown_->hwnd(), nullptr, FALSE);
}

void KotobaCppApp::appCmd(const std::vector<std::wstring>& t, ui::ScriptRunner& runner) {
    const std::wstring& sub = t[1];
    if (sub == L"init") {
        if (!init()) runner.fail(L"app init failed");
    }
    else if (sub == L"tick") {
        // Same tick() as app_test: gated on the pending countdown timer,
        // then fires one-shot timers. (The old poller called
        // on_countdown_tick() directly and drifted from this.)
        tick(t.size() > 2 ? _wtoi(t[2].c_str()) : 1);
    }
    else if (sub == L"word") {
        if (t.size() < 3) { runner.fail(L"app word: need <file>"); return; }
        std::string j = read_file(t[2]);
        if (j.empty()) { runner.fail(L"app word: cannot open " + t[2]); return; }
        setWord(j);
    }
    else if (sub == L"state") {
        // Note (not failure) so it lands in summary()/debug_out.txt.
        runner.note(L"state " + ui::utf8ToWide(stateJson().dump()));
    }
    else if (sub == L"add-word") {
        // Test/extension seam (M4): add one word file to the picker pool.
        // The same light check as startup applies; a bad file is counted
        // and logged, never added.
        if (t.size() < 3) { runner.fail(L"app add-word: need <file>"); return; }
        std::wstring path = runner.resolvePath(t[2]);
        std::string json = read_file(path);
        if (json.empty()) { runner.fail(L"app add-word: cannot open " + path); return; }
        WordData w = parse_word_json(json);
        if (const std::wstring issue = wordIssue(w); !issue.empty()) {
            ++dbRejected_;
            dbg::log("db", L"rejected %ls: %ls", path.c_str(), issue.c_str());
            return;
        }
        extraWords_.push_back(std::move(w));
    }
    else if (sub == L"config") {
        if (t.size() >= 3 && t[2] == L"reload") { config_.load(); return; }
        if (t.size() < 4) { runner.fail(L"app config: need <key> <value> or reload"); return; }
        Config c = config_.get();
        const std::wstring& key = t[2];
        const std::wstring& value = t[3];
        if (key == L"countdownSeconds") c.countdownSeconds = _wtoi(value.c_str());
        else if (key == L"volume") c.volume = (float)_wtof(value.c_str());
        else if (key == L"randomUi") c.randomUi = value == L"true" || value == L"1";
        else if (key == L"startWithWindows") c.startWithWindows = value == L"true" || value == L"1";
        else if (key == L"freezeChrome") c.freezeChrome = value == L"true" || value == L"1";
        else if (key == L"maxFreezeMinutes") c.maxFreezeMinutes = _wtoi(value.c_str());
        else { runner.fail(L"app config: unknown key " + key); return; }
        config_.set(c);
        if (!config_.save()) runner.fail(L"app config: save failed");
    }
    else if (sub == L"progress") {
        if (t.size() < 3) { runner.fail(L"app progress: need <reset>"); return; }
        if (t[2] == L"reset") { progress_.reset(); progress_.save(); return; }
        runner.fail(L"unknown app progress command " + t[2]);
    }
    else if (sub == L"pick") {
        // Test seam: run the even picker n times (simulates n countdown
        // restarts) so picker behaviour is testable without UI ceremony.
        int n = t.size() > 2 ? _wtoi(t[2].c_str()) : 1;
        for (int i = 0; i < n; ++i)
            pick_word();
    }
    else if (sub == L"tray-event") {
        if (t.size() < 3) { runner.fail(L"app tray-event: need <double-click|left-click|right-click>"); return; }
        UINT ev = t[2] == L"double-click" ? WM_LBUTTONDBLCLK
                 : t[2] == L"left-click"   ? WM_LBUTTONUP
                 : t[2] == L"right-click"  ? WM_RBUTTONUP
                 : 0;
        if (!ev) { runner.fail(L"app tray-event: unknown event " + t[2]); return; }
        trayEvent(ev);
    }
    else if (sub == L"assert") {
        if (t.size() < 4) { runner.fail(L"app assert: need <key> == <value>"); return; }
        const std::wstring& key = t[2];
        const std::wstring& expected = t.back();
        std::wstring actual;
        if (key == L"visible") {
            if (t.size() < 5) { runner.fail(L"app assert visible: need <window>"); return; }
            actual = isVisible(t[3]) ? L"true" : L"false";
        }
        else if (key == L"config") {
            if (t.size() < 5) { runner.fail(L"app assert config: need <key> == <value>"); return; }
            const Config& c = config_.get();
            const std::wstring& ck = t[3];
            if (ck == L"countdownSeconds") actual = std::to_wstring(c.countdownSeconds);
            else if (ck == L"volume") {
                wchar_t buf[32];
                swprintf(buf, 32, L"%g", c.volume);   // 1.5, not 1.500000
                actual = buf;
            }
            else if (ck == L"randomUi") actual = c.randomUi ? L"true" : L"false";
            else if (ck == L"startWithWindows") actual = c.startWithWindows ? L"true" : L"false";
            else if (ck == L"freezeChrome") actual = c.freezeChrome ? L"true" : L"false";
            else if (ck == L"maxFreezeMinutes") actual = std::to_wstring(c.maxFreezeMinutes);
            else { runner.fail(L"app assert config: unknown key " + ck); return; }
        }
        else if (key == L"attempts") {
            if (t.size() < 5) { runner.fail(L"app assert attempts: need <word> == <N>"); return; }
            actual = std::to_wstring(progress_.attempts(t[3]));
        }
        else if (key == L"attemptsTotal") {
            int total = 0;
            for (const auto& [k, v] : progress_.get().attempts) total += v;
            actual = std::to_wstring(total);
        }
        else if (key == L"attemptsMax") {
            int max = 0;
            for (const auto& [k, v] : progress_.get().attempts) max = std::max(max, v);
            actual = std::to_wstring(max);
        }
        else if (key == L"ignored") {
            if (t.size() < 5) { runner.fail(L"app assert ignored: need <word> == <value>"); return; }
            bool found = false;
            for (const auto& w : config_.get().ignoreWords)
                if (w == t[3]) { found = true; break; }
            actual = found ? L"true" : L"false";
        }
        else if (key == L"picked_unique") {
            // The last n recorded picks must be pairwise distinct. The even
            // picker guarantees this up to the word count; beyond it this
            // assertion fails (pigeonhole) - useful with expect-fail.
            int n = t.size() > 3 ? _wtoi(t[3].c_str()) : 0;
            if ((int)recentPicks_.size() < n) {
                runner.fail(L"app assert picked_unique: only " +
                             std::to_wstring(recentPicks_.size()) + L" picks recorded");
                return;
            }
            auto begin = recentPicks_.end() - n;
            bool unique = true;
            for (auto a = begin; a != recentPicks_.end() && unique; ++a)
                for (auto b = begin; b != a; ++b)   // within the window only
                    if (*a == *b) { unique = false; break; }
            actual = unique ? L"true" : L"false";
        }
        else {
            if (t[3] != L"==") { runner.fail(L"app assert: operator must be ==, got " + t[3]); return; }
            auto state = stateJson();
            std::string k = ui::wideToUtf8(key);
            if (!state.contains(k)) { runner.fail(L"app assert: unknown key " + key); return; }
            auto& v = state[k];
            // String state (word) compares unquoted; everything else dumps
            // (bools -> true/false, ints -> digits).
            actual = v.is_string() ? ui::utf8ToWide(v.get<std::string>())
                                    : ui::utf8ToWide(v.dump());
        }
        if (actual != expected) {
            runner.fail(L"app assert " + key + L" expected " + expected + L" got " + actual);
            // Print the full state on failure: most timer/state bugs are
            // diagnosed from this, and it saves a debugging round trip.
            runner.note(L"state " + ui::utf8ToWide(stateJson().dump()));
        }
    }
    else runner.fail(L"unknown app command " + sub);
}

std::string KotobaCppApp::read_file(const std::wstring& path) {
    std::ifstream f(std::string(ui::wideToUtf8(path)), std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(f)), {});
}
