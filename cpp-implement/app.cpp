// app.cpp
//
// Build:
//   build.bat --input app.cpp --output build\app.exe

#include "document.h"
#include "core/utf.h"
#include "utils/set_tray.h"
#include "embedded_assets.h"
#include "json.hpp"

#include <memory>
#include <random>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace {

constexpr UINT_PTR TIMER_COUNTDOWN = 100;
constexpr UINT_PTR TIMER_TRANSITION = 101;
constexpr UINT_PTR TIMER_INPUT_DONE = 102;

// Load an embedded asset by its relative path (e.g. "htmls/info.html").
std::string load_asset_bytes(const std::string& path) {
    if (const assets::EmbeddedAsset* asset = assets::find_asset(path)) {
        return std::string(reinterpret_cast<const char*>(asset->data), asset->size);
    }
    return {};
}

std::wstring load_embedded_html(const wchar_t* name) {
    // Convert wide name to UTF-8 path "htmls/<name>".
    std::string utf8Name = ui::wideToUtf8(name);
    std::string path = "htmls/" + utf8Name;
    std::string bytes = load_asset_bytes(path);

    if (bytes.empty())
        return L"";

    return ui::utf8ToWide(bytes);
}

// ----- Word database ----- 

struct WordData {
    std::wstring kanji;
    std::wstring hiragana;
    std::wstring romaji;
    std::wstring pos;
    std::wstring meaning_ja;
    std::wstring meaning_ja_romaji;
    std::wstring meaning_vi;
    std::wstring example1_ja;
    std::wstring example1_romaji;
    std::wstring example1_vi;
    std::wstring example2_ja;
    std::wstring example2_romaji;
    std::wstring example2_vi;
    std::wstring tag1;
    std::wstring tag2;
    std::wstring audio;
};

WordData parse_word_json(const std::string& json) {
    WordData w{};
    auto j = nlohmann::json::parse(json, nullptr, false);
    if (j.is_discarded())
        return w;

    auto get = [&](const char* key) -> std::wstring {
        if (j.contains(key))
            return ui::utf8ToWide(j[key].get<std::string>());
        return L"";
    };

    w.kanji = get("kanji");
    w.hiragana = get("hiragana");
    w.romaji = get("romaji");
    w.pos = get("pos");
    w.meaning_ja = get("meaning_ja");
    w.meaning_ja_romaji = get("meaning_ja_romaji");
    w.meaning_vi = get("meaning_vi");
    w.audio = get("audio");

    if (j.contains("examples") && j["examples"].is_array()) {
        auto& ex = j["examples"];
        if (ex.size() > 0) {
            w.example1_ja = ui::utf8ToWide(ex[0]["ja"].get<std::string>());
            w.example1_romaji = ui::utf8ToWide(ex[0]["romaji"].get<std::string>());
            w.example1_vi = ui::utf8ToWide(ex[0]["vi"].get<std::string>());
        }
        if (ex.size() > 1) {
            w.example2_ja = ui::utf8ToWide(ex[1]["ja"].get<std::string>());
            w.example2_romaji = ui::utf8ToWide(ex[1]["romaji"].get<std::string>());
            w.example2_vi = ui::utf8ToWide(ex[1]["vi"].get<std::string>());
        }
    }

    if (j.contains("tags") && j["tags"].is_array()) {
        auto& tags = j["tags"];
        if (tags.size() > 0)
            w.tag1 = ui::utf8ToWide(tags[0].get<std::string>());
        if (tags.size() > 1)
            w.tag2 = ui::utf8ToWide(tags[1].get<std::string>());
    }

    return w;
}

// Fill {{key}} placeholders in HTML with word data.
std::wstring fill_template(const std::wstring& html, const WordData& w) {
    std::unordered_map<std::wstring, std::wstring> values = {
        {L"kanji", w.kanji},
        {L"hiragana", w.hiragana},
        {L"romaji", w.romaji},
        {L"pos", w.pos},
        {L"meaning_ja", w.meaning_ja},
        {L"meaning_ja_romaji", w.meaning_ja_romaji},
        {L"meaning_vi", w.meaning_vi},
        {L"example1_ja", w.example1_ja},
        {L"example1_romaji", w.example1_romaji},
        {L"example1_vi", w.example1_vi},
        {L"example2_ja", w.example2_ja},
        {L"example2_romaji", w.example2_romaji},
        {L"example2_vi", w.example2_vi},
        {L"tag1", w.tag1},
        {L"tag2", w.tag2},
        {L"audio", w.audio},
    };

    std::wstring result;
    result.reserve(html.size() * 2);

    for (std::wstring::size_type i = 0; i < html.size(); ) {
        if (html[i] == L'{' && i + 1 < html.size() && html[i + 1] == L'{') {
            // Find closing }}.
            std::wstring::size_type close = html.find(L"}}", i + 2);
            if (close != std::wstring::npos) {
                std::wstring key = html.substr(i + 2, close - i - 2);
                auto it = values.find(key);
                if (it != values.end()) {
                    result += it->second;
                    i = close + 2;
                    continue;
                }
            }
        }
        result += html[i];
        ++i;
    }

    return result;
}

// Pick a random word from embedded database/words/*.json.
WordData pick_random_word() {
    std::vector<std::string> wordPaths;

    for (std::size_t i = 0; i < assets::kAssetCount; ++i) {
        const auto& asset = assets::kAssets[i];
        std::string_view path = asset.relative_path;
        // Match "database/words/" prefix and ".json" suffix.
        if (path.size() > 20 &&
            path.substr(0, 17) == "database/words/" &&
            path.size() >= 5 &&
            path.substr(path.size() - 5) == ".json") {
            wordPaths.push_back(std::string(path));
        }
    }

    if (wordPaths.empty())
        return {};

    static std::mt19937 rng{std::random_device{}()};
    std::uniform_int_distribution<std::size_t> dist(0, wordPaths.size() - 1);
    const std::string& chosen = wordPaths[dist(rng)];

    std::string bytes = load_asset_bytes(chosen);
    return parse_word_json(bytes);
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
    const wchar_t* htmlName,
    const WordData* word = nullptr
) {
    std::wstring html = load_embedded_html(htmlName);

    if (html.empty())
        html = missing_html_page(htmlName);

    if (word)
        html = fill_template(html, *word);

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
    WordData currentWord_{};

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

        position_countdown();

        // Dump UI tree for debugging.
        wchar_t tempDir[MAX_PATH];
        GetTempPathW(MAX_PATH, tempDir);
        std::wstring dumpFile = std::wstring(tempDir) + L"\\countdown_dump.txt";
        countdown_->document().dumpTree(dumpFile);

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

        if (!create_host(input_, options, L"input.html", &currentWord_))
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

    void start_countdown() {
        countdownSeconds_ = 30;
        inputDone_ = false;
        currentWord_ = pick_random_word();

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
