#include "app.h"

namespace wordlock {

bool TryGetStringField(const std::wstring& json, const std::wstring& key, std::wstring& out) {
    const std::wstring needle = L"\"" + key + L"\":";
    const size_t start = json.find(needle);
    if (start == std::wstring::npos) {
        return false;
    }
    size_t index = start + needle.size();
    while (index < json.size() && std::isspace(static_cast<unsigned char>(json[index]))) {
        ++index;
    }
    if (index >= json.size() || json[index] != L'"') {
        return false;
    }
    ++index;
    out.clear();
    while (index < json.size()) {
        wchar_t ch = json[index++];
        if (ch == L'"') {
            return true;
        }
        if (ch == L'\\' && index < json.size()) {
            ch = json[index++];
            switch (ch) {
            case L'n': ch = L'\n'; break;
            case L'r': ch = L'\r'; break;
            case L't': ch = L'\t'; break;
            case L'"': break;
            case L'\\': break;
            default: break;
            }
        }
        out.push_back(ch);
    }
    return false;
}

bool TryGetNumberField(const std::wstring& json, const std::wstring& key, double& out) {
    const std::wstring needle = L"\"" + key + L"\":";
    const size_t start = json.find(needle);
    if (start == std::wstring::npos) {
        return false;
    }
    size_t index = start + needle.size();
    while (index < json.size() && std::isspace(static_cast<unsigned char>(json[index]))) {
        ++index;
    }
    size_t end = index;
    while (end < json.size()) {
        const wchar_t ch = json[end];
        if (!(ch == L'-' || ch == L'+' || ch == L'.' || (ch >= L'0' && ch <= L'9'))) {
            break;
        }
        ++end;
    }
    if (end == index) {
        return false;
    }
    out = wcstod(json.substr(index, end - index).c_str(), nullptr);
    return true;
}

bool TryGetBoolField(const std::wstring& json,
                     const std::wstring& key,
                     bool defaultValue) {
    const std::wstring needle = L"\"" + key + L"\":";
    const size_t start = json.find(needle);
    if (start == std::wstring::npos) {
        return defaultValue;
    }
    size_t index = start + needle.size();
    while (index < json.size() && std::isspace(static_cast<unsigned char>(json[index]))) {
        ++index;
    }
    if (json.compare(index, 4, L"true") == 0) {
        return true;
    }
    if (json.compare(index, 5, L"false") == 0) {
        return false;
    }
    return defaultValue;
}

}  // namespace wordlock
