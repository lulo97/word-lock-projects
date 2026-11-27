#include "app.h"

namespace wordlock {

void RegisterWebViewBinding(ICoreWebView2Controller* controller,
                            ICoreWebView2* webView,
                            EventRegistrationToken token) {
    if (controller == nullptr || webView == nullptr) {
        return;
    }
    WebViewBinding binding;
    binding.controller = controller;
    binding.messageToken = token;
    g_webViewBindings.push_back(std::move(binding));
}

void UnregisterWebViewBinding(ICoreWebView2Controller* controller) {
    if (controller == nullptr) {
        return;
    }
    g_webViewBindings.erase(
        std::remove_if(g_webViewBindings.begin(), g_webViewBindings.end(),
                       [controller](const WebViewBinding& binding) {
                           return binding.controller.Get() == controller;
                       }),
        g_webViewBindings.end());
}

void BroadcastTheme(const std::wstring& theme) {
    const std::wstring normalized = NormalizeTheme(theme);
    const std::wstring message =
        L"{\"type\":\"theme:changed\",\"theme\":" + JsLiteral(normalized) + L"}";
    for (const WebViewBinding& binding : g_webViewBindings) {
        ComPtr<ICoreWebView2> webView;
        if (SUCCEEDED(binding.controller->get_CoreWebView2(&webView)) && webView) {
            webView->PostWebMessageAsJson(message.c_str());
            RefreshBootScript(webView.Get(), binding.controller.Get(), normalized);
        }
    }
}

void SendHostReply(ICoreWebView2* sender,
                   const std::wstring& requestId,
                   const std::wstring& resultJson,
                   const std::wstring& error) {
    if (sender == nullptr || requestId.empty()) {
        return;
    }
    std::wstring json = L"{\"type\":\"response\",\"id\":" + JsLiteral(requestId) + L",";
    if (!error.empty()) {
        json += L"\"ok\":false,\"error\":" + JsLiteral(error);
    } else {
        json += L"\"ok\":true,\"result\":" + (resultJson.empty() ? L"null" : resultJson);
    }
    json += L"}";
    sender->PostWebMessageAsJson(json.c_str());
}

void InstallWebViewBridge(ICoreWebView2* webView, ICoreWebView2Controller* controller,
                          WebViewReady onReady) {
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
    AddBootScript(webView, controller, g_settings.theme, onReady);
}

}  // namespace wordlock
