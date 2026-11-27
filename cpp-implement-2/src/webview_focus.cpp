#include "app.h"

namespace wordlock {

void InstallTaskInputFocus(ICoreWebView2* webView, ICoreWebView2Controller* controller) {
    if (webView == nullptr || controller == nullptr) {
        return;
    }

    ComPtr<ICoreWebView2Controller> controllerRef(controller);
    const std::wstring focusScript =
        L"(function(){var el=document.getElementById('answer');"
        L"if(el&&!el.disabled){try{el.focus();}catch(e){}}})()";

    webView->add_NavigationCompleted(
        Callback<ICoreWebView2NavigationCompletedEventHandler>(
            [controllerRef, focusScript](ICoreWebView2* sender,
                                         ICoreWebView2NavigationCompletedEventArgs* args)
                -> HRESULT {
                BOOL success = FALSE;
                if (args == nullptr || FAILED(args->get_IsSuccess(&success)) || !success) {
                    return S_OK;
                }
                if (g_mainWindow && IsWindowVisible(g_mainWindow)) {
                    controllerRef->MoveFocus(COREWEBVIEW2_MOVE_FOCUS_REASON_PROGRAMMATIC);
                }
                ComPtr<ICoreWebView2> view = sender;
                view->ExecuteScript(
                    focusScript.c_str(),
                    Callback<ICoreWebView2ExecuteScriptCompletedHandler>(
                        [](HRESULT, LPCWSTR) -> HRESULT { return S_OK; }).Get());
                return S_OK;
            }).Get(),
        nullptr);
}

}  // namespace wordlock
