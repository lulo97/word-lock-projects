#include "core/utf.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <algorithm>
#include <cwctype>
#include <cwchar>
#include <cstdlib>

namespace ui {

namespace {

bool isSpace(wchar_t c) {
    return c == L' ' || c == L'\t' || c == L'\r' || c == L'\n' ||
           c == L'\f' || c == L'\v';
}

int hexValue(wchar_t c) {
    if (c >= L'0' && c <= L'9') return c - L'0';
    if (c >= L'a' && c <= L'f') return c - L'a' + 10;
    if (c >= L'A' && c <= L'F') return c - L'A' + 10;
    return -1;
}

} // namespace

std::wstring trim(const std::wstring& s) {
    size_t a = 0;
    size_t b = s.size();

    while (a < b && isSpace(s[a]))
        ++a;

    while (b > a && isSpace(s[b - 1]))
        --b;

    return s.substr(a, b - a);
}

std::wstring toLower(const std::wstring& s) {
    std::wstring out = s;

    for (wchar_t& c : out)
        c = static_cast<wchar_t>(towlower(c));

    return out;
}

std::vector<std::wstring> splitWhitespace(const std::wstring& s) {
    std::vector<std::wstring> out;
    size_t i = 0;

    while (i < s.size()) {
        while (i < s.size() && isSpace(s[i]))
            ++i;

        if (i >= s.size())
            break;

        size_t start = i;

        while (i < s.size() && !isSpace(s[i]))
            ++i;

        out.push_back(s.substr(start, i - start));
    }

    return out;
}

std::vector<std::wstring> split(const std::wstring& s, wchar_t delim) {
    std::vector<std::wstring> out;
    size_t start = 0;

    for (;;) {
        size_t pos = s.find(delim, start);

        if (pos == std::wstring::npos) {
            out.push_back(s.substr(start));
            break;
        }

        out.push_back(s.substr(start, pos - start));
        start = pos + 1;
    }

    return out;
}

std::wstring collapseWhitespace(const std::wstring& s) {
    std::wstring out;
    bool pending = false;

    for (wchar_t c : s) {
        if (isSpace(c)) {
            if (!out.empty())
                pending = true;
        }
        else {
            if (pending) {
                out.push_back(L' ');
                pending = false;
            }

            out.push_back(c);
        }
    }

    return out;
}

std::wstring decodeEntities(const std::wstring& s) {
    std::wstring out;
    out.reserve(s.size());

    size_t i = 0;

    while (i < s.size()) {
        if (s[i] != L'&') {
            out.push_back(s[i++]);
            continue;
        }

        size_t semi = s.find(L';', i + 1);

        if (semi == std::wstring::npos || semi - i > 12) {
            out.push_back(s[i++]);
            continue;
        }

        std::wstring name = s.substr(i + 1, semi - i - 1);
        wchar_t decoded = 0;
        bool ok = true;

        if (name == L"amp") decoded = L'&';
        else if (name == L"lt") decoded = L'<';
        else if (name == L"gt") decoded = L'>';
        else if (name == L"quot") decoded = L'"';
        else if (name == L"apos") decoded = L'\'';
        else if (name == L"nbsp") decoded = 0x00A0;
        else if (!name.empty() && name[0] == L'#') {
            long value = 0;

            if (name.size() > 2 && (name[1] == L'x' || name[1] == L'X')) {
                for (size_t k = 2; k < name.size(); ++k) {
                    int h = hexValue(name[k]);

                    if (h < 0) {
                        ok = false;
                        break;
                    }

                    value = value * 16 + h;
                }
            }
            else {
                for (size_t k = 1; k < name.size(); ++k) {
                    if (name[k] < L'0' || name[k] > L'9') {
                        ok = false;
                        break;
                    }

                    value = value * 10 + (name[k] - L'0');
                }
            }

            decoded = static_cast<wchar_t>(value);
        }
        else {
            ok = false;
        }

        if (!ok) {
            out.push_back(s[i++]);
            continue;
        }

        out.push_back(decoded);
        i = semi + 1;
    }

    return out;
}

bool isCjk(wchar_t c) {
    unsigned int u = static_cast<unsigned int>(c);

    return (u >= 0x2E80 && u <= 0x9FFF) ||
           (u >= 0xAC00 && u <= 0xD7AF) ||
           (u >= 0xF900 && u <= 0xFAFF) ||
           (u >= 0xFF00 && u <= 0xFFEF);
}

bool isLineStartForbidden(wchar_t c) {
    switch (c) {
    case 0x3001: // 、
    case 0x3002: // 。
    case 0x3005: // 々
    case 0x3009: // 〉
    case 0x300B: // 》
    case 0x300D: // 」
    case 0x300F: // 』
    case 0x3011: // 】
    case 0x3015: // 〕
    case 0x3019: // 】
    case 0x301F: // 〟
    case 0xFF01: // ！
    case 0xFF09: // ）
    case 0xFF0C: // ，
    case 0xFF0E: // ．
    case 0xFF1F: // ？
    case 0xFF3D: // ］
    case 0xFF5D: // ｝
    case 0x30FB: // ・
        return true;
    default:
        return false;
    }
}

bool parseFloat(const std::wstring& s, float& out) {
    std::wstring t = trim(s);

    if (t.empty())
        return false;

    wchar_t* end = nullptr;
    double value = std::wcstod(t.c_str(), &end);

    if (!end || end == t.c_str())
        return false;

    while (*end && isSpace(*end))
        ++end;

    if (*end != 0)
        return false;

    out = static_cast<float>(value);
    return true;
}

std::wstring utf8ToWide(const std::string& s) {
    if (s.empty())
        return L"";

    int size = MultiByteToWideChar(
        CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0
    );

    if (size <= 0)
        return L"";

    std::wstring out(static_cast<size_t>(size), L'\0');

    MultiByteToWideChar(
        CP_UTF8, 0, s.data(), static_cast<int>(s.size()),
        out.data(), size
    );

    return out;
}

std::string wideToUtf8(const std::wstring& s) {
    if (s.empty())
        return "";

    int size = WideCharToMultiByte(
        CP_UTF8, 0, s.data(), static_cast<int>(s.size()),
        nullptr, 0, nullptr, nullptr
    );

    if (size <= 0)
        return "";

    std::string out(static_cast<size_t>(size), '\0');

    WideCharToMultiByte(
        CP_UTF8, 0, s.data(), static_cast<int>(s.size()),
        out.data(), size, nullptr, nullptr
    );

    return out;
}

} // namespace ui
