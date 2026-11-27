#include "app.h"

namespace wordlock {

void InstallTestDomDump(ICoreWebView2* webView, const wchar_t* htmlFile) {
    if (!IsTestDomDumpEnabled() || wcscmp(htmlFile, L"settings.html") != 0) {
        return;
    }

    webView->add_NavigationCompleted(
        Callback<ICoreWebView2NavigationCompletedEventHandler>(
            [](ICoreWebView2* sender,
               ICoreWebView2NavigationCompletedEventArgs* args) -> HRESULT {
                BOOL success = FALSE;
                if (args == nullptr || FAILED(args->get_IsSuccess(&success))) {
                    WriteTestDomDump(L"{\"navigationResultFailed\":true}");
                    return S_OK;
                }
                if (!success) {
                    WriteTestDomDump(L"{\"navigationFailed\":true}");
                    return S_OK;
                }

                ComPtr<ICoreWebView2> view = sender;
                const HRESULT hr = view->ExecuteScript(
                    L"document.body ? document.body.innerText : ''",
                    Callback<ICoreWebView2ExecuteScriptCompletedHandler>(
                        [view](HRESULT result, LPCWSTR resultJson) -> HRESULT {
                            if (FAILED(result) || resultJson == nullptr) {
                                WriteTestDomDump(L"{\"domScriptFailed\":true}");
                            } else {
                                WriteTestDomDump(resultJson);
                                ExecuteTestCommand(view.Get());
                            }
                            return S_OK;
                        }).Get());
                if (FAILED(hr)) {
                    WriteTestDomDump(L"{\"domScriptStartFailed\":true}");
                }
                return S_OK;
            }).Get(),
        nullptr);
}

void InitializeWebView(HWND hwnd,
                       const wchar_t* htmlFile,
                       ComPtr<ICoreWebView2Controller>* controllerOut) {
    const std::wstring htmlPath = AppAssetUrl(htmlFile);
    const std::wstring htmlFileValue(htmlFile);
    CreateWebView(hwnd, controllerOut,
                  [htmlPath, htmlFileValue](ComPtr<ICoreWebView2>& webView) {
                      InstallTestDomDump(webView.Get(), htmlFileValue.c_str());
                      webView->Navigate(htmlPath.c_str());
                  });
}

void InitializeMainWebView(HWND hwnd) {
    CreateWebView(hwnd, &g_mainViewController,
                  [](ComPtr<ICoreWebView2>&) {
                      NavigateMainToTask(g_currentTask);
                  });
}

}  // namespace wordlock
