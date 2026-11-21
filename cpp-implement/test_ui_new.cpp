// test_ui_new.cpp
//
// Renderer smoke test for ui_new/.
//
// Build:
//   build.bat --input test_ui_new.cpp --output build\test_ui_new.exe
//
// Run:
//   build\test_ui_new.exe imgs\4.html
//   build\test_ui_new.exe imgs\3.html 760 900
//
// The optional width/height arguments are in logical pixels.

#include "document.h"

#include "core/utf.h"

#include <memory>
#include <string>

int WINAPI wWinMain(HINSTANCE, HINSTANCE, LPWSTR commandLine, int) {
    std::vector<std::wstring> args = ui::splitWhitespace(
        commandLine ? commandLine : L""
    );

    std::wstring path = args.size() > 0 ? args[0] : L"imgs\\4.html";
    int width = 1200;
    int height = 800;

    if (args.size() > 1) {
        float value = 0;

        if (ui::parseFloat(args[1], value))
            width = static_cast<int>(value);
    }

    if (args.size() > 2) {
        float value = 0;

        if (ui::parseFloat(args[2], value))
            height = static_cast<int>(value);
    }

    if (path.size() > 9 && path.substr(path.size() - 5) == L"1.html") {
        width = args.size() > 1 ? width : 560;
        height = args.size() > 2 ? height : 260;
    }
    else if (path.size() > 9 && path.substr(path.size() - 5) == L"2.html") {
        width = args.size() > 1 ? width : 420;
        height = args.size() > 2 ? height : 620;
    }
    else if (path.size() > 9 && path.substr(path.size() - 5) == L"3.html") {
        width = args.size() > 1 ? width : 760;
        height = args.size() > 2 ? height : 900;
    }

    auto document = std::make_shared<ui::Document>();

    if (!document->loadFile(path)) {
        document->load(
            L"<body class=\"bg-white p-8\">"
            L"<p id=\"error\" class=\"text-3xl text-black\">"
            L"Could not load " + path +
            L"</p></body>"
        );
    }

    ui::WindowOptions options;
    options.title = L"ui_new renderer - " + path;
    options.width = width;
    options.height = height;
    options.centered = true;

    ui::WindowHost host;

    if (!host.create(options, document))
        return 1;

    MSG message{};

    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    return 0;
}
