#include "app.h"

namespace wordlock {

void ConfigureWebViewPermissions(ICoreWebView2* webView) {
    ComPtr<ICoreWebView2_4> webView4;
    if (SUCCEEDED(webView->QueryInterface(IID_PPV_ARGS(&webView4))) && webView4) {
        webView4->add_PermissionRequested(
            Callback<ICoreWebView2PermissionRequestedEventHandler>(
                [](ICoreWebView2*, ICoreWebView2PermissionRequestedEventArgs* args) -> HRESULT {
                    COREWEBVIEW2_PERMISSION_KIND kind{};
                    args->get_PermissionKind(&kind);
                    args->put_State(kind == COREWEBVIEW2_PERMISSION_KIND_CAMERA
                                        ? COREWEBVIEW2_PERMISSION_STATE_ALLOW
                                        : COREWEBVIEW2_PERMISSION_STATE_DENY);
                    return S_OK;
                }).Get(),
            nullptr);
    }
}

void ConfigureControllerDefaults(ICoreWebView2Controller* controller) {
    ComPtr<ICoreWebView2Controller2> controller2;
    if (SUCCEEDED(controller->QueryInterface(IID_PPV_ARGS(&controller2))) && controller2) {
        COREWEBVIEW2_COLOR white{};
        white.A = 255;
        white.R = 255;
        white.G = 255;
        white.B = 255;
        controller2->put_DefaultBackgroundColor(white);
    }
}

void ConfigureWebViewResources(ICoreWebView2* webView,
                               ComPtr<ICoreWebView2Environment> environmentRef,
                               ComPtr<ICoreWebView2Environment2> environment2,
                               std::wstring assetFolder, bool hasVirtualHostMapping) {
    webView->AddWebResourceRequestedFilter(L"https://app/*",
                                           COREWEBVIEW2_WEB_RESOURCE_CONTEXT_ALL);
    webView->add_WebResourceRequested(
        Callback<ICoreWebView2WebResourceRequestedEventHandler>(
            [environmentRef, environment2, assetFolder, hasVirtualHostMapping](
                ICoreWebView2*, ICoreWebView2WebResourceRequestedEventArgs* args) -> HRESULT {
                return HandleAppWebResourceRequest(environment2, environmentRef, assetFolder,
                                                   hasVirtualHostMapping, args);
            }).Get(),
        nullptr);
}

void InstallWebViewBridge(ICoreWebView2* webView, ICoreWebView2Controller* controller,
                          WebViewReady onReady) {
    const std::wstring bootScript = BuildBootScript();
    webView->AddScriptToExecuteOnDocumentCreated(
        bootScript.c_str(),
        Callback<ICoreWebView2AddScriptToExecuteOnDocumentCreatedCompletedHandler>(
            [onReady, webView](HRESULT scriptResult, LPCWSTR error) mutable -> HRESULT {
                if (FAILED(scriptResult) && IsTestModeEnabled()) {
                    AppendTestMessageLog(L"boot-script-add-failed:" +
                                         (error != nullptr ? std::wstring(error)
                                                           : std::wstring(L"unknown")));
                }
                ComPtr<ICoreWebView2> readyView = webView;
                onReady(readyView);
                return S_OK;
            }).Get());

    EventRegistrationToken messageToken{};
    webView->add_WebMessageReceived(
        Callback<ICoreWebView2WebMessageReceivedEventHandler>(
            [webView](ICoreWebView2*, ICoreWebView2WebMessageReceivedEventArgs* args) -> HRESULT {
                LPWSTR json = nullptr;
                if (SUCCEEDED(args->get_WebMessageAsJson(&json)) && json != nullptr) {
                    const std::wstring message(json);
                    CoTaskMemFree(json);
                    HandleHostMessage(webView, message);
                }
                return S_OK;
            }).Get(),
        &messageToken);
    RegisterWebViewBinding(controller, webView, messageToken);
}

}  // namespace wordlock
