// app.cpp
//
// Build:
//   build.bat --input app.cpp --output build\app.exe

#include "document.h"
#include "core/utf.h"
#include "utils/set_tray.h"

#include <fstream>
#include <memory>
#include <string>
#include <vector>

namespace {

constexpr UINT_PTR TIMER_COUNTDOWN = 100;
constexpr UINT_PTR TIMER_TRANSITION = 101;
constexpr UINT_PTR TIMER_INPUT_DONE = 102;

std::wstring module_directory() {
    std::vector<wchar_t> buffer(MAX_PATH);

    for (;;) {
        DWORD length = GetModuleFileNameW(
            nullptr, buffer.data(), static_cast<DWORD>(buffer.size())
        );

        if (length == 0)
            return L"";

        if (length < buffer.size() - 1) {
            std::wstring path(buffer.data(), length);
            std::wstring::size_type slash = path.find_last_of(L"\/");

            if (slash == std::wstring::npos)
                return L".";

            return path.substr(0, slash);
        }

        buffer.resize(buffer.size() * 2);
    }
}

std::wstring parent_directory(const std::wstring& directory) {
    std::wstring::size_type slash = directory.find_last_of(L"\/");

    if (slash == std::wstring::npos)
        return directory;

    if (slash == 2 && directory.size() >= 3 && directory[1] == L':')
        return directory.substr(0, 3);

    return directory.substr(0, slash);
}

std::wstring current_directory() {
    DWORD length = GetCurrentDirectoryW(0, nullptr);

    if (length == 0)
        return L"";

    std::vector<wchar_t> buffer(length);
    length = GetCurrentDirectoryW(length, buffer.data());

    if (length == 0)
        return L"";

    return std::wstring(buffer.data(), length);
}

bool file_exists(const std::wstring& path) {
    DWORD attributes = GetFileAttributesW(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES &&
           (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

std::wstring html_path(const std::wstring& name) {
    std::wstring exeDir = module_directory();
    std::wstring parent = parent_directory(exeDir);
    std::wstring cwd = current_directory();

    std::vector<std::wstring> candidates = {
        exeDir + L"\\htmls\\" + name,
        parent + L"\\htmls\\" + name,
        cwd + L"\\htmls\\" + name,
        exeDir + L"\\" + name,
        parent + L"\\" + name,
        cwd + L"\\" + name
    };

    for (const std::wstring& candidate : candidates) {
        if (file_exists(candidate))
            return candidate;
    }

    return L"";
}

std::wstring load_html_file(const wchar_t* name) {
    std::wstring path = html_path(name);

    if (path.empty())
        return L"";

    std::ifstream file(path, std::ios::binary);

    if (!file)
        return L"";

    std::string bytes(
        (std::istreambuf_iterator<char>(file)),
        std::istreambuf_iterator<char>()
    );

    return ui::utf8ToWide(bytes);
}

std::wstring missing_html_page(const wchar_t* name) {
    std::wstring html =
        L"<body class=\"min-h-screen flex items-center justify-center bg-white\">"
        L"<p class=\"text-2xl text-black\">Could not load htmls\\";
    html += name;
    html += L"</p></body>";
    return html;
}

bool create_host(
    std::unique_ptr<ui::WindowHost>& host,
    const ui::WindowOptions& options,
    const wchar_t* htmlName
) {
    std::wstring html = load_html_file(htmlName);

    if (html.empty())
        html = missing_html_page(htmlName);

    auto document = std::make_shared<ui::Document>();

    if (!document->load(html))
        return false;

    host = std::make_unique<ui::WindowHost>();
    return host->create(options, document);
}

class KotobaApp {
public:
    ~KotobaApp() {
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

    bool create_countdown() {
        ui::WindowOptions options;
        options.title = L"Countdown";
        options.width = 180;
        options.height = 51;
        options.popup = true;
        options.topmost = true;
        options.toolWindow = true;
        options.noActivate = true;
        options.centered = false;
        options.visible = true;

        if (!create_host(countdown_, options, L"countdown.html"))
            return false;

        position_countdown();
        return true;
    }

    bool create_tray_menu() {
        ui::WindowOptions options;
        options.title = L"Kotoba Lock";
        options.width = 320;
        options.height = 430;
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
            start_countdown();
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

    bool ensure_input() {
        if (input_)
            return true;

        ui::WindowOptions options;
        options.title = L"Kotoba Lock";
        options.width = 1200;
        options.height = 800;
        options.centered = true;
        options.visible = false;

        if (!create_host(input_, options, L"input.html"))
            return false;

        input_->document().onInput(
            L"answer",
            [this](const std::wstring& value) {
                on_answer_changed(value);
            }
        );

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

        if (!create_host(info_, options, L"info.html"))
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

    void start_countdown() {
        countdownSeconds_ = 30;
        inputDone_ = false;

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
        position_countdown();

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

    void show_input() {
        if (!ensure_input())
            return;

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
        if (inputDone_ || ui::toLower(value) != L"ashita")
            return;

        inputDone_ = true;
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

        if (input_) input_->show(false);

        info_->show(true);
        ShowWindow(info_->hwnd(), SW_MAXIMIZE);
    }

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
        position_tray_menu();
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

    void position_countdown() {
        if (!countdown_ || !countdown_->hwnd())
            return;

        RECT work{};
        SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);

        RECT window{};
        GetWindowRect(countdown_->hwnd(), &window);

        int width = window.right - window.left;
        int height = window.bottom - window.top;
        int margin = static_cast<int>(20 * countdown_->dpiScale());

        SetWindowPos(
            countdown_->hwnd(),
            HWND_TOPMOST,
            work.right - width - margin,
            work.bottom - height - margin,
            width,
            height,
            SWP_NOACTIVATE
        );
    }

    void position_tray_menu() {
        if (!trayMenu_ || !trayMenu_->hwnd())
            return;

        RECT work{};
        SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);

        RECT window{};
        GetWindowRect(trayMenu_->hwnd(), &window);

        int width = window.right - window.left;
        int height = window.bottom - window.top;

        POINT cursor{};
        GetCursorPos(&cursor);

        int gap = static_cast<int>(8 * trayMenu_->dpiScale());
        int x = cursor.x - width + gap;
        int y = cursor.y - height - gap;

        if (y < work.top)
            y = cursor.y + gap;

        if (x < work.left)
            x = work.left;

        if (x + width > work.right)
            x = work.right - width;

        if (y + height > work.bottom)
            y = work.bottom - height;

        SetWindowPos(
            trayMenu_->hwnd(),
            HWND_TOPMOST,
            x,
            y,
            width,
            height,
            SWP_NOACTIVATE
        );
    }

    void quit() {
        if (countdown_ && countdown_->hwnd())
            remove_tray(countdown_->hwnd());

        PostQuitMessage(0);
    }
};

} // namespace

int WINAPI wWinMain(HINSTANCE, HINSTANCE, LPWSTR, int) {
    KotobaApp app;
    return app.run();
}
