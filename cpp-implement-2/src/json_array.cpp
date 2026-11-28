#include "app.h"

namespace wordlock {

bool TryGetStringArrayField(const std::wstring& json,
                            const std::wstring& key,
                            std::vector<std::wstring>& out) {
    const std::wstring needle = L"\"" + key + L"\":";
    const size_t start = json.find(needle);
    if (start == std::wstring::npos) {
        return false;
    }
    size_t index = json.find(L'[', start + needle.size());
    if (index == std::wstring::npos) {
        return false;
    }
    ++index;
    out.clear();
    while (index < json.size()) {
        while (index < json.size() && (json[index] == L' ' || json[index] == L',')) {
            ++index;
        }
        if (index < json.size() && json[index] == L']') {
            return true;
        }
        if (index >= json.size() || json[index] != L'"') {
            return false;
        }
        ++index;
        std::wstring value;
        bool closed = false;
        while (index < json.size()) {
            wchar_t ch = json[index++];
            if (ch == L'"') {
                closed = true;
                break;
            }
            if (ch == L'\\' && index < json.size()) {
                ch = json[index++];
                switch (ch) {
                case L'n': ch = L'\n'; break;
                case L'r': ch = L'\r'; break;
                case L't': ch = L'\t'; break;
                case L'"': break;
                case L'\\': break;
                case L'/': break;
                default: break;
                }
            }
            value.push_back(ch);
        }
        if (!closed) {
            return false;
        }
        out.push_back(value);
    }
    return false;
}

}  // namespace wordlock
