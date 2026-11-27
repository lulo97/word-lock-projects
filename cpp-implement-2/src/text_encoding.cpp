#include "app.h"

namespace wordlock {

std::wstring Utf8ToWide(const std::string& text) {
    if (text.empty()) {
        return std::wstring();
    }
    const int length = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
    if (length <= 0) {
        return std::wstring();
    }
    std::vector<wchar_t> buffer(static_cast<size_t>(length));
    MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, buffer.data(), length);
    return std::wstring(buffer.data(), static_cast<size_t>(length - 1));
}

std::string WideToUtf8(const std::wstring& text) {
    if (text.empty()) {
        return std::string();
    }
    const int length = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (length <= 0) {
        return std::string();
    }
    std::vector<char> buffer(static_cast<size_t>(length));
    WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, buffer.data(), length, nullptr, nullptr);
    return std::string(buffer.data(), static_cast<size_t>(length - 1));
}

std::wstring JsonEscape(const std::wstring& value) {
    std::wstring out;
    for (wchar_t ch : value) {
        switch (ch) {
        case L'"': out += L"\\\""; break;
        case L'\\': out += L"\\\\"; break;
        case L'\n': out += L"\\n"; break;
        case L'\r': out += L"\\r"; break;
        case L'\t': out += L"\\t"; break;
        default:
            if (ch < 0x20) {
                static const wchar_t* digits = L"0123456789abcdef";
                out += L"\\u00";
                out.push_back(digits[(ch >> 4) & 0xF]);
                out.push_back(digits[ch & 0xF]);
            } else {
                out.push_back(ch);
            }
            break;
        }
    }
    return out;
}

std::wstring JsLiteral(const std::wstring& value) {
    return L"\"" + JsonEscape(value) + L"\"";
}

const wchar_t* BoolLiteral(bool value) {
    return value ? L"true" : L"false";
}

}  // namespace wordlock
