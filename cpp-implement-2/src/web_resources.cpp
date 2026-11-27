#include "app.h"

namespace wordlock {

std::wstring ContentTypeForFile(const std::filesystem::path& path) {
    std::wstring extension = path.extension().wstring();
    std::transform(extension.begin(), extension.end(), extension.begin(), [](wchar_t ch) {
        return static_cast<wchar_t>(std::tolower(static_cast<unsigned char>(ch)));
    });

    if (extension == L".html" || extension == L".htm") return L"text/html; charset=utf-8";
    if (extension == L".js" || extension == L".mjs") return L"application/javascript; charset=utf-8";
    if (extension == L".css") return L"text/css; charset=utf-8";
    if (extension == L".json") return L"application/json; charset=utf-8";
    if (extension == L".wasm") return L"application/wasm";
    if (extension == L".svg") return L"image/svg+xml";
    if (extension == L".png") return L"image/png";
    if (extension == L".jpg" || extension == L".jpeg") return L"image/jpeg";
    if (extension == L".gif") return L"image/gif";
    if (extension == L".mp4") return L"video/mp4";
    if (extension == L".wav") return L"audio/wav";
    return L"application/octet-stream";
}

ComPtr<ICoreWebView2WebResourceResponse> CreateFileWebResourceResponse(
    ICoreWebView2Environment2* environment,
    const std::filesystem::path& file) {
    if (environment == nullptr) {
        return nullptr;
    }

    std::ifstream input(file, std::ios::binary);
    if (!input) {
        return nullptr;
    }
    const std::vector<char> bytes((std::istreambuf_iterator<char>(input)),
                                  std::istreambuf_iterator<char>());
    IStream* rawStream = nullptr;
    if (FAILED(CreateStreamOnHGlobal(nullptr, TRUE, &rawStream)) || rawStream == nullptr) {
        return nullptr;
    }
    ComPtr<IStream> stream;
    stream.Attach(rawStream);

    if (!bytes.empty()) {
        ULONG written = 0;
        if (FAILED(stream->Write(bytes.data(), static_cast<ULONG>(bytes.size()), &written))) {
            return nullptr;
        }
    }
    LARGE_INTEGER start{};
    stream->Seek(start, STREAM_SEEK_SET, nullptr);

    const std::wstring headers = L"Content-Type: " + ContentTypeForFile(file) + L"\r\n";
    ComPtr<ICoreWebView2WebResourceResponse> response;
    if (FAILED(environment->CreateWebResourceResponse(
            stream.Get(), 200, L"OK", headers.c_str(), &response))) {
        return nullptr;
    }
    return response;
}

}  // namespace wordlock
