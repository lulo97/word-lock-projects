#include "app.h"

namespace wordlock {

HRESULT HandleAppWebResourceRequest(
    ComPtr<ICoreWebView2Environment2> environment2,
    ComPtr<ICoreWebView2Environment> environmentRef,
    std::wstring assetFolder, bool hasVirtualHostMapping,
    ICoreWebView2WebResourceRequestedEventArgs* args) {
    ComPtr<ICoreWebView2WebResourceRequest> request;
    if (FAILED(args->get_Request(&request)) || !request) {
        return S_OK;
    }
    LPWSTR uri = nullptr;
    request->get_Uri(&uri);
    if (uri != nullptr) {
        const std::wstring uriText(uri);
        ComPtr<ICoreWebView2Environment2> responseEnvironment = environment2;
        if (!responseEnvironment && environmentRef) {
            environmentRef->QueryInterface(IID_PPV_ARGS(&responseEnvironment));
        }
        ComPtr<ICoreWebView2WebResourceResponse> response;
        if (!IsAppResourceUriAllowed(uriText)) {
            if (responseEnvironment) {
                responseEnvironment->CreateWebResourceResponse(
                    nullptr, 403, L"Forbidden", nullptr, &response);
            }
        } else if (IsAppResourceUri(uriText) && !hasVirtualHostMapping) {
            const std::filesystem::path file =
                std::filesystem::path(assetFolder) / AppRelativePathFromUri(uriText);
            response = CreateFileWebResourceResponse(responseEnvironment.Get(), file);
        }
        if (response) {
            args->put_Response(response.Get());
        }
        CoTaskMemFree(uri);
    }
    return S_OK;
}

}  // namespace wordlock
