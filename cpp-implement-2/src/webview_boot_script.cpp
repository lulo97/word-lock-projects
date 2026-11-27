#include "app.h"

namespace wordlock {

namespace {

WebViewBinding* FindBinding(ICoreWebView2Controller* controller) {
    for (WebViewBinding& binding : g_webViewBindings) {
        if (binding.controller.Get() == controller) {
            return &binding;
        }
    }
    return nullptr;
}

}  // namespace

void RememberBootScript(ICoreWebView2Controller* controller, LPCWSTR id) {
    WebViewBinding* binding = FindBinding(controller);
    if (binding != nullptr) {
        binding->bootScriptId = id != nullptr ? id : L"";
        binding->bootScriptReady = id != nullptr;
    }
}

void AddBootScript(ICoreWebView2* webView, ICoreWebView2Controller* controller,
                   const std::wstring& theme, WebViewReady onReady) {
    ComPtr<ICoreWebView2> view = webView;
    ComPtr<ICoreWebView2Controller> controllerRef(controller);
    webView->AddScriptToExecuteOnDocumentCreated(
        BuildBootScript(theme).c_str(),
        Callback<ICoreWebView2AddScriptToExecuteOnDocumentCreatedCompletedHandler>(
            [view, controllerRef, onReady](HRESULT, LPCWSTR id) -> HRESULT {
                RememberBootScript(controllerRef.Get(), id);
                if (onReady) {
                    ComPtr<ICoreWebView2> readyView = view;
                    onReady(readyView);
                }
                return S_OK;
            }).Get());
}

void RefreshBootScript(ICoreWebView2* webView, ICoreWebView2Controller* controller,
                       const std::wstring& theme) {
    WebViewBinding* binding = FindBinding(controller);
    if (binding != nullptr && !binding->bootScriptId.empty()) {
        webView->RemoveScriptToExecuteOnDocumentCreated(binding->bootScriptId.c_str());
        binding->bootScriptId.clear();
        binding->bootScriptReady = false;
    }
    AddBootScript(webView, controller, theme, WebViewReady{});
}

}  // namespace wordlock
