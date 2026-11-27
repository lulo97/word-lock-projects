#include "app.h"

namespace wordlock {

void CreateWebView(HWND hwnd, ComPtr<ICoreWebView2Controller>* controllerOut,
                   WebViewReady onReady) {
    HRESULT hr = CreateCoreWebView2EnvironmentWithOptions(
        nullptr, nullptr, nullptr,
        Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
            [hwnd, controllerOut, onReady](HRESULT result,
                                           ICoreWebView2Environment* environment) -> HRESULT {
                if (FAILED(result) || environment == nullptr) {
                    if (IsTestModeEnabled()) {
                        WriteTestDomDump(L"{\"webviewEnvironmentFailed\":true}");
                        return result;
                    }
                    MessageBoxW(hwnd, L"WebView2 environment initialization failed.",
                                L"Error", MB_ICONERROR | MB_OK);
                    return result;
                }

                std::wstring assetFolder = GetExecutableDirectory();
                if (!assetFolder.empty() && assetFolder.back() == L'\\') {
                    assetFolder.pop_back();
                }
                bool hasVirtualHostMapping = false;
                ComPtr<ICoreWebView2_3> environment3;
                if (SUCCEEDED(environment->QueryInterface(IID_PPV_ARGS(&environment3))) &&
                    environment3) {
                    hasVirtualHostMapping = SUCCEEDED(environment3->SetVirtualHostNameToFolderMapping(
                        L"app", assetFolder.c_str(),
                        COREWEBVIEW2_HOST_RESOURCE_ACCESS_KIND_ALLOW));
                }

                ComPtr<ICoreWebView2Environment2> environment2;
                environment->QueryInterface(IID_PPV_ARGS(&environment2));
                ComPtr<ICoreWebView2Environment> environmentRef = environment;
                return environment->CreateCoreWebView2Controller(
                    hwnd,
                    Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                        [hwnd, controllerOut, onReady, environmentRef, environment2,
                         assetFolder, hasVirtualHostMapping](
                            HRESULT controllerResult, ICoreWebView2Controller* controllerRaw)
                            -> HRESULT {
                            if (FAILED(controllerResult) || controllerRaw == nullptr) {
                                if (IsTestModeEnabled()) {
                                    WriteTestDomDump(L"{\"webviewControllerFailed\":true}");
                                    return controllerResult;
                                }
                                MessageBoxW(hwnd, L"WebView2 controller initialization failed.",
                                            L"Error", MB_ICONERROR | MB_OK);
                                return controllerResult;
                            }
                            *controllerOut = controllerRaw;
                            ConfigureControllerDefaults(controllerRaw);
                            UpdateWebViewBounds(hwnd, *controllerOut);

                            ComPtr<ICoreWebView2> webView;
                            if (SUCCEEDED((*controllerOut)->get_CoreWebView2(&webView)) && webView) {
                                ConfigureWebViewPermissions(webView.Get());
                                ConfigureWebViewResources(webView.Get(), environmentRef,
                                                          environment2, assetFolder,
                                                          hasVirtualHostMapping);
                                InstallWebViewBridge(webView.Get(), controllerOut->Get(), onReady);
                            }
                            return S_OK;
                        }).Get());
            }).Get());

    if (FAILED(hr)) {
        if (IsTestModeEnabled()) {
            WriteTestDomDump(L"{\"webviewStartFailed\":true}");
        } else {
            MessageBoxW(hwnd, L"Failed to start WebView2. Is the WebView2 runtime installed?",
                        L"Error", MB_ICONERROR | MB_OK);
        }
    }
}

}  // namespace wordlock
