#include "app.h"

namespace wordlock {

int HexDigit(wchar_t ch) {
    if (ch >= L'0' && ch <= L'9') return ch - L'0';
    if (ch >= L'a' && ch <= L'f') return ch - L'a' + 10;
    if (ch >= L'A' && ch <= L'F') return ch - L'A' + 10;
    return -1;
}

std::wstring UrlDecode(const std::wstring& value) {
    std::wstring out;
    for (size_t i = 0; i < value.size(); ++i) {
        if (value[i] == L'%' && i + 2 < value.size()) {
            const int high = HexDigit(value[i + 1]);
            const int low = HexDigit(value[i + 2]);
            if (high >= 0 && low >= 0) {
                out.push_back(static_cast<wchar_t>((high << 4) | low));
                i += 2;
                continue;
            }
        }
        if (value[i] == L'+') {
            out.push_back(L' ');
        } else {
            out.push_back(value[i]);
        }
    }
    return out;
}

bool IsAppResourceUri(const std::wstring& uri) {
    return uri.rfind(kAppUriPrefix, 0) == 0;
}

std::wstring AppRelativePathFromUri(const std::wstring& uri) {
    if (!IsAppResourceUri(uri)) {
        return std::wstring();
    }

    std::wstring relative = UrlDecode(uri.substr(std::size(kAppUriPrefix) - 1));
    const size_t query = relative.find_first_of(L"?#");
    if (query != std::wstring::npos) {
        relative.resize(query);
    }
    std::replace(relative.begin(), relative.end(), L'/', L'\\');
    return relative;
}

std::filesystem::path ResolveAppResourcePath(const std::wstring& relative) {
    return std::filesystem::path(GetExecutableDirectory()) / relative;
}

bool IsAppResourceUriAllowed(const std::wstring& uri) {
    if (!IsAppResourceUri(uri)) {
        return true;  // Not an app resource request; WebView2 may serve it normally.
    }

    std::error_code error;
    const std::filesystem::path root = std::filesystem::weakly_canonical(GetExecutableDirectory(), error);
    const std::filesystem::path target =
        std::filesystem::weakly_canonical(ResolveAppResourcePath(AppRelativePathFromUri(uri)), error);
    if (error) {
        return false;
    }

    std::wstring rootText = root.wstring();
    while (!rootText.empty() && (rootText.back() == L'\\' || rootText.back() == L'/')) {
        rootText.pop_back();
    }
    const std::wstring targetText = target.wstring();
    if (targetText.rfind(rootText, 0) != 0) {
        return false;
    }
    return targetText.size() == rootText.size() || targetText[rootText.size()] == L'\\';
}

}  // namespace wordlock
