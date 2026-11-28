#include "app.h"

namespace wordlock {

bool IsJapaneseScriptWord(const std::wstring& word) {
    if (word.empty()) {
        return false;
    }
    for (wchar_t ch : word) {
        const bool ok =
            ch == 0x3005 || ch == 0x3006 ||
            (ch >= 0x3041 && ch <= 0x309F) ||
            (ch >= 0x30A0 && ch <= 0x30FF) ||
            (ch >= 0x3400 && ch <= 0x4DBF) ||
            (ch >= 0x4E00 && ch <= 0x9FFF) ||
            (ch >= 0xF900 && ch <= 0xFAFF);
        if (!ok) {
            return false;
        }
    }
    return true;
}

std::vector<std::wstring> ParseIgnoreWords(const std::wstring& value, bool japanese) {
    if (value.find(L'\r') != std::wstring::npos ||
        value.find(L'\n') != std::wstring::npos) {
        throw std::runtime_error("invalid ignore words");
    }
    std::vector<std::wstring> words;
    if (Trim(value).empty()) {
        return words;
    }

    size_t start = 0;
    while (true) {
        const size_t comma = value.find(L',', start);
        const std::wstring token = Trim(value.substr(start, comma == std::wstring::npos
                                                              ? std::wstring::npos
                                                              : comma - start));
        if (token.empty()) {
            throw std::runtime_error("invalid ignore words");
        }
        for (wchar_t ch : token) {
            if (std::iswspace(ch)) {
                throw std::runtime_error("invalid ignore words");
            }
        }
        if (japanese && !IsJapaneseScriptWord(token)) {
            throw std::runtime_error("invalid ignore words");
        }
        words.push_back(token);  // Duplicates are intentionally retained.
        if (comma == std::wstring::npos) {
            break;
        }
        start = comma + 1;
    }
    return words;
}

}  // namespace wordlock
