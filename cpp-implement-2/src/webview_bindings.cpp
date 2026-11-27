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
    const std::wstring message =
        L"{\"type\":\"theme:changed\",\"theme\":" + JsLiteral(NormalizeTheme(theme)) + L"}";
    for (const WebViewBinding& binding : g_webViewBindings) {
        ComPtr<ICoreWebView2> webView;
        if (SUCCEEDED(binding.controller->get_CoreWebView2(&webView)) && webView) {
            webView->PostWebMessageAsJson(message.c_str());
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

}  // namespace wordlock
