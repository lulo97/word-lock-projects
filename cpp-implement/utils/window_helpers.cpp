// window_helpers.cpp
//
// Window creation and positioning helpers.

#include "window_helpers.h"
#include "core/utf.h"
#include "debug_log.h"

#include <windows.h>

std::wstring load_embedded_html(const wchar_t* name) {
    std::string utf8Name = ui::wideToUtf8(name);
    std::string path = "htmls/" + utf8Name;
    std::string bytes = load_asset_bytes(path);

    if (bytes.empty())
        return L"";

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
    const wchar_t* htmlName,
    const WordData* word
) {
    std::wstring html = load_embedded_html(htmlName);

    if (html.empty()) {
        UI_LOG("app", L"MISSING HTML %s", htmlName);
        html = missing_html_page(htmlName);
    }

    if (word)
        html = fill_template(html, *word);

    auto document = std::make_shared<ui::Document>();

    if (!document->load(html))
        return false;

    host = std::make_unique<ui::WindowHost>();
    return host->create(options, document);
}

void position_bottom_right(ui::WindowHost& host, float marginLogical) {
    if (!host.hwnd())
        return;

    RECT work{};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);

    RECT window{};
    GetWindowRect(host.hwnd(), &window);

    int width = window.right - window.left;
    int height = window.bottom - window.top;
    int margin = static_cast<int>(marginLogical * host.dpiScale());

    SetWindowPos(
        host.hwnd(),
        HWND_TOPMOST,
        work.right - width - margin,
        work.bottom - height - margin,
        width,
        height,
        SWP_NOACTIVATE
    );
}

void position_near_cursor(ui::WindowHost& host, float gapLogical) {
    if (!host.hwnd())
        return;

    RECT work{};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);

    RECT window{};
    GetWindowRect(host.hwnd(), &window);

    int width = window.right - window.left;
    int height = window.bottom - window.top;

    POINT cursor{};
    GetCursorPos(&cursor);

    int gap = static_cast<int>(gapLogical * host.dpiScale());
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
        host.hwnd(),
        HWND_TOPMOST,
        x,
        y,
        width,
        height,
        SWP_NOACTIVATE
    );
}
