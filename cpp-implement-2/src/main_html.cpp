#include "app.h"

namespace wordlock {

std::string BuildMainHtml() {
    std::string html = R"(<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<style>
  html, body { height: 100%; margin: 0; background: #fff; }
  .screen-frame { display: block; width: 100%; height: 100vh; border: 0; }
</style>
</head>
<body>
)";

    if (g_settings.tasks.englishWord) {
        html += R"(<iframe class="screen-frame" title="English word" src="english.html"></iframe>
)";
    }
    if (g_settings.tasks.japaneseWord) {
        html += R"(<iframe class="screen-frame" title="Japanese word" src="japanese.html"></iframe>
)";
    }
    if (g_settings.tasks.multipleChoice) {
        html += R"(<iframe class="screen-frame" title="Multiple choice" src="mcq.html"></iframe>
)";
    }
    if (g_settings.tasks.dumbbellWorkout) {
        html += R"(<iframe class="screen-frame" title="Dumbbell workout" src="dumbbell.html"></iframe>
)";
    }

    html += "</body>\n</html>";
    return html;
}

void UpdateMainContent() {
    if (!g_mainViewController) {
        return;
    }

    const std::filesystem::path shellPath =
        std::filesystem::path(GetExecutableDirectory()) / L"mainScreens.html";
    std::ofstream shell(shellPath, std::ios::binary | std::ios::trunc);
    if (!shell) {
        MessageBoxW(g_mainWindow, L"Failed to write mainScreens.html.",
                    L"Error", MB_ICONERROR | MB_OK);
        return;
    }
    shell << BuildMainHtml();
    shell.close();

    ComPtr<ICoreWebView2> webView;
    if (SUCCEEDED(g_mainViewController->get_CoreWebView2(&webView)) && webView) {
        webView->Navigate(shellPath.wstring().c_str());
    }
}

}  // namespace wordlock
