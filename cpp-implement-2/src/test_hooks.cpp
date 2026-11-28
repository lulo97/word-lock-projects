#include "app.h"

namespace wordlock {

bool IsTestDomDumpEnabled() {
    return IsTestModeEnabled() && !GetEnvironmentValue(kTestDomDumpFileEnv).empty();
}

void WriteTestDomDump(const std::wstring& contents) {
    if (!IsTestDomDumpEnabled()) {
        return;
    }

    static bool written = false;
    if (written) {
        return;
    }
    written = true;

    const std::filesystem::path path(GetEnvironmentValue(kTestDomDumpFileEnv));
    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path());
    }
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (output) {
        output << WideToUtf8(contents);
    }
}

void AppendTestMessageLog(const std::wstring& type) {
    if (!IsTestModeEnabled()) {
        return;
    }
    const std::wstring pathValue = GetEnvironmentValue(kTestMessageLogFileEnv);
    if (pathValue.empty()) {
        return;
    }
    std::ofstream output(std::filesystem::path(pathValue), std::ios::binary | std::ios::app);
    if (output) {
        output << WideToUtf8(type) << "\n";
    }
}

void ExecuteTestCommand(ICoreWebView2* webView, const wchar_t* htmlFile) {
    if (!IsTestModeEnabled() || webView == nullptr) {
        return;
    }
    const bool ignoreView = htmlFile != nullptr && wcscmp(htmlFile, L"ignoreWords.html") == 0;
    const std::wstring pathValue = GetEnvironmentValue(
        ignoreView ? kTestIgnoreCommandFileEnv : kTestCommandFileEnv);
    if (pathValue.empty()) {
        return;
    }
    std::ifstream input(std::filesystem::path(pathValue), std::ios::binary);
    if (!input) {
        return;
    }
    const std::string scriptUtf8((std::istreambuf_iterator<char>(input)),
                                 std::istreambuf_iterator<char>());
    const std::wstring script = Utf8ToWide(scriptUtf8);
    if (!script.empty()) {
        AppendTestMessageLog(L"test:command");
        webView->ExecuteScript(
            script.c_str(),
            Callback<ICoreWebView2ExecuteScriptCompletedHandler>(
                [](HRESULT resultCode, LPCWSTR resultJson) -> HRESULT {
                    if (FAILED(resultCode)) {
                        AppendTestMessageLog(L"test:script-error");
                    } else {
                        AppendTestMessageLog(std::wstring(L"test:script:") +
                                             (resultJson != nullptr ? resultJson : L"null"));
                    }
                    return S_OK;
                }).Get());
    }
}

}  // namespace wordlock
