// app.cpp
//
// Build:
//   build.bat --input app.cpp --output build/app.exe
//
// Flow:
//   CountDownUI (small, bottom-right, frameless)
//     -> after 3, 2, 1, 0 seconds
//   InputUI (maximized, normal frame)
//     -> type "apple"
//     -> green "Correct!" for 300 ms
//   InfoUI (maximized, normal frame)
//     -> listen to apple.ogg
//     -> ENTER returns to CountDownUI

#include "ui/ui_framework.h"
#include "utils/play_audio.h"

#include <string>

namespace {

constexpr UINT_PTR TIMER_COUNTDOWN = 100;
constexpr UINT_PTR TIMER_TRANSITION = 101;

class CountdownApp {
public:
    CountdownApp()
        : window_(L"Apple Countdown", 360, 120) {
    }

    void run() {
        bind_events();

        window_.set_tray();

        start_countdown();

        window_.run();

        stop_audio();
    }

private:
    enum class Screen {
        Countdown,
        Input,
        Info
    };

    UIWindow window_;

    Screen screen_ = Screen::Countdown;
    int countdown_ = 3;
    bool transitioning_ = false;

    void bind_events() {
        window_.bind_input(
            L"answer",
            [this](const std::wstring& value) {
                on_answer_changed(value);
            }
        );

        window_.bind_click(
            L"btn-listen",
            [] {
                play_audio(L"apple.ogg");
            }
        );

        window_.set_key_handler(
            [this](WPARAM key) {
                if (key == VK_RETURN &&
                    screen_ == Screen::Info) {
                    start_countdown();
                    return true;
                }

                return false;
            }
        );
    }

    static std::wstring countdown_text(int seconds) {
        return L"Word appear in " +
               std::to_wstring(seconds) +
               L" s";
    }

    static std::wstring countdown_html(int seconds) {
        std::wstring html =
            L"<div class=\"bg-white p-4 rounded-lg\">"
            L"<p id=\"countdown\" "
            L"class=\"text-xl text-black\">";

        html += countdown_text(seconds);
        html += L"</p></div>";

        return html;
    }

    std::wstring input_html() const {
        return LR"HTML(
<div class="h-screen bg-gray-100 p-6 flex items-center justify-center">
    <div class="bg-white p-6 rounded-lg w-[480px]">
        <p class="text-3xl text-black mb-4">Apple</p>

        <input id="answer"
               class="bg-white text-black text-lg p-4 rounded-lg"
               placeholder="Type apple..." />

        <p id="feedback"
           class="text-lg text-green-600 mt-4"></p>
    </div>
</div>
)HTML";
    }

    std::wstring info_html() const {
        const wchar_t* english =
            L"English: Apple is a round fruit with red, green, "
            L"or yellow skin and firm white flesh.";

        const wchar_t* vietnamese =
            L"Vietnamese: T\u00E1o l\u00E0 lo\u1EA1i qu\u1EA3 "
            L"tr\u00F2n, v\u1ECF c\u00F3 th\u1EC3 m\u00E0u "
            L"\u0111\u1ECF, xanh ho\u1EB7c v\u00E0ng, "
            L"th\u1ECBt tr\u1EAFng v\u00E0 gi\u00F2n.";

        std::wstring html = LR"HTML(
<div class="h-screen bg-gray-100 p-6 flex flex-col justify-between">
    <div>
        <p class="text-3xl text-black mb-4">Apple</p>
)HTML";

        html += L"<p class=\"text-lg text-black mb-2\">";
        html += english;
        html += L"</p>";

        html += L"<p class=\"text-lg text-black mb-4\">";
        html += vietnamese;
        html += L"</p>";

        html += LR"HTML(
        <div class="flex justify-center mt-4">
            <button id="btn-listen"
                    class="bg-blue-600 text-white text-base p-4 rounded-lg">
                Listen
            </button>
        </div>
    </div>

    <div class="flex justify-center">
        <p class="text-base text-gray-600">Hit ENTER to close</p>
    </div>
</div>
)HTML";

        return html;
    }

    void position_bottom_right() {
        RECT work{};

        if (!SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0)) {
            work.left = 0;
            work.top = 0;
            work.right = GetSystemMetrics(SM_CXSCREEN);
            work.bottom = GetSystemMetrics(SM_CYSCREEN);
        }

        const int width = 360;
        const int height = 120;
        const int margin = 20;

        window_.set_bounds(
            work.right - width - margin,
            work.bottom - height - margin,
            width,
            height
        );
    }

    void start_countdown() {
        screen_ = Screen::Countdown;
        countdown_ = 3;
        transitioning_ = false;

        stop_audio();

        window_.kill_timer(TIMER_COUNTDOWN);
        window_.kill_timer(TIMER_TRANSITION);

        window_.restore();
        window_.set_frameless();

        window_.load_html(countdown_html(countdown_));

        position_bottom_right();

        window_.set_timer(
            TIMER_COUNTDOWN,
            1000,
            [this] {
                on_countdown_tick();
            }
        );
    }

    void on_countdown_tick() {
        if (screen_ != Screen::Countdown)
            return;

        if (countdown_ <= 0)
            return;

        --countdown_;

        window_.set_text(
            L"countdown",
            countdown_text(countdown_)
        );

        if (countdown_ == 0) {
            window_.kill_timer(TIMER_COUNTDOWN);

            window_.set_timer(
                TIMER_TRANSITION,
                300,
                [this] {
                    window_.kill_timer(TIMER_TRANSITION);
                    show_input();
                }
            );
        }
    }

    void show_input() {
        screen_ = Screen::Input;
        transitioning_ = false;

        window_.set_framed();

        window_.load_html(input_html());

        window_.set_maximize();
        window_.focus_input(L"answer");
    }

    void on_answer_changed(const std::wstring& value) {
        if (screen_ != Screen::Input)
            return;

        if (transitioning_)
            return;

        if (value != L"apple")
            return;

        transitioning_ = true;

        window_.set_text(L"feedback", L"Correct!");

        window_.set_timer(
            TIMER_TRANSITION,
            300,
            [this] {
                window_.kill_timer(TIMER_TRANSITION);
                show_info();
            }
        );
    }

    void show_info() {
        screen_ = Screen::Info;

        window_.set_framed();

        window_.load_html(info_html());

        window_.set_maximize();
    }
};

} // namespace

int WINAPI wWinMain(HINSTANCE, HINSTANCE, LPWSTR, int) {
    CountdownApp app;
    app.run();
    return 0;
}
