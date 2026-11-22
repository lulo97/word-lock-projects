// app.cpp
//
// Build:
//   build.bat --input app.cpp --output build\app.exe
//
// KotobaCpp: tray app that shows a countdown, then an input screen,
// then an info screen for a randomly picked Japanese word.

#include "document.h"
#include "core/utf.h"
#include "utils/debug_log.h"
#include "utils/set_tray.h"
#include "utils/word_db.h"
#include "utils/window_helpers.h"

#include <memory>
#include <string>

namespace {

constexpr UINT_PTR TIMER_COUNTDOWN = 100;
constexpr UINT_PTR TIMER_TRANSITION = 101;
constexpr UINT_PTR TIMER_INPUT_DONE = 102;

class KotobaCppApp {
public:
    ~KotobaCppApp() {
        if (countdown_ && countdown_->hwnd())
            remove_tray(countdown_->hwnd());
    }

    int run() {
        ui::WindowHost::enableDpiAwareness();

        if (!create_countdown())
            return 1;

        if (!create_tray_menu())
            return 1;

        if (!create_settings())
            return 1;

        set_tray(countdown_->hwnd(), nullptr, L"Kotoba Lock");

        set_tray_click_callback(
            countdown_->hwnd(),
            [this](HWND, LPARAM lParam) {
                on_tray_click(lParam);
            }
        );

        start_countdown();

        MSG message{};

        while (GetMessageW(&message, nullptr, 0, 0) > 0) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }

        return 0;
    }

private:
    std::unique_ptr<ui::WindowHost> countdown_;
    std::unique_ptr<ui::WindowHost> trayMenu_;
    std::unique_ptr<ui::WindowHost> settings_;
    std::unique_ptr<ui::WindowHost> input_;
    std::unique_ptr<ui::WindowHost> info_;

    int countdownSeconds_ = 30;
    bool inputDone_ = false;
    WordData currentWord_{};

    // ----- window creation -----

    bool create_countdown() {
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

    bool create_tray_menu() {
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
            currentWord_ = pick_random_word();
            reset_word_windows();
            show_input();
        });

        document.onClick(L"tray-show", [this] {
            hide_tray_menu();
            start_countdown();
        });

        document.onClick(L"tray-stop", [this] {
            hide_tray_menu();
            countdown_->show(false);
            if (input_) input_->show(false);
            if (info_) info_->show(false);
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

    bool create_settings() {
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
    void reset_word_windows() {
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

    bool ensure_input() {
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

    bool ensure_info() {
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

    void start_countdown() {
        countdownSeconds_ = 30;
        inputDone_ = false;
        currentWord_ = pick_random_word();
        UI_LOG("app", L"start_countdown word=%s seconds=%d",
               currentWord_.romaji.c_str(), countdownSeconds_);
        reset_word_windows();

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

        countdown_->setTimer(
            TIMER_COUNTDOWN,
            1000,
            [this] {
                on_countdown_tick();
            }
        );
    }

    void on_countdown_tick() {
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
            }
        );
    }

    // ----- input / info -----

    void show_input() {
        if (!ensure_input())
            return;

        UI_LOG("app", L"show_input word=%s", currentWord_.romaji.c_str());
        countdown_->show(false);

        inputDone_ = false;
        input_->document().setValue(L"answer", L"");
        input_->document().setText(L"feedback", L"");

        input_->show(true);
        ShowWindow(input_->hwnd(), SW_MAXIMIZE);

        if (ui::Element* answer = input_->document().byId(L"answer"))
            input_->document().events().setFocus(answer);
    }

    void on_answer_changed(const std::wstring& value) {
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
            }
        );
    }

    void show_info() {
        if (!ensure_info())
            return;

        UI_LOG("app", L"show_info word=%s", currentWord_.romaji.c_str());
        if (input_) input_->show(false);

        info_->show(true);
        ShowWindow(info_->hwnd(), SW_MAXIMIZE);
    }

    // ----- settings / tray -----

    void show_settings() {
        if (!settings_)
            return;

        settings_->show(true);
        SetForegroundWindow(settings_->hwnd());
    }

    void on_tray_click(LPARAM lParam) {
        UINT event = LOWORD(lParam);

        switch (event) {
        // Right click: always open the menu, never toggle it closed.
        case WM_CONTEXTMENU:
        case WM_RBUTTONDOWN:
        case WM_RBUTTONUP:
        case WM_RBUTTONDBLCLK:
            show_tray_menu();
            break;

        // Left click: toggle.
        case NIN_SELECT:
        case NIN_KEYSELECT:
        case WM_LBUTTONUP:
        case WM_LBUTTONDBLCLK:
            toggle_tray_menu();
            break;

        default:
            break;
        }
    }

    void show_tray_menu() {
        if (!trayMenu_)
            return;

        trayMenu_->show(true);
        position_near_cursor(*trayMenu_, 8);
        SetForegroundWindow(trayMenu_->hwnd());
    }

    void toggle_tray_menu() {
        if (!trayMenu_)
            return;

        if (IsWindowVisible(trayMenu_->hwnd())) {
            hide_tray_menu();
            return;
        }

        show_tray_menu();
    }

    void hide_tray_menu() {
        if (!trayMenu_)
            return;

        if (trayMenu_->hwnd() && IsWindowVisible(trayMenu_->hwnd()))
            trayMenu_->show(false);
    }

    void quit() {
        if (countdown_ && countdown_->hwnd())
            remove_tray(countdown_->hwnd());

        PostQuitMessage(0);
    }
};

} // namespace

int WINAPI wWinMain(HINSTANCE, HINSTANCE, LPWSTR, int) {
    KotobaCppApp app;
    return app.run();
}
