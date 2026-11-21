#pragma once
#include <string>
#include <vector>

namespace ui {

// Trim ASCII whitespace from both ends. In: s. Out: trimmed copy.
std::wstring trim(const std::wstring& s);

// Lowercase ASCII/Unicode letters. In: s. Out: lowered copy.
std::wstring toLower(const std::wstring& s);

// Split on whitespace. In: s. Out: tokens (no empties).
std::vector<std::wstring> splitWhitespace(const std::wstring& s);

// Split on a delimiter char. In: s, delim. Out: pieces (empties kept).
std::vector<std::wstring> split(const std::wstring& s, wchar_t delim);

// Collapse whitespace runs to one space (CSS white-space: normal).
// In: s. Out: collapsed string (no trim).
std::wstring collapseWhitespace(const std::wstring& s);

// Decode &amp; &lt; &gt; &quot; &nbsp; &#NNN; &#xHH;. In: s. Out: decoded.
std::wstring decodeEntities(const std::wstring& s);

// True if code point is CJK (kana, kanji, hangul, fullwidth). Used for line breaking.
bool isCjk(wchar_t c);

// True if c may NOT start a line (CJK closing punctuation like 。、」).
bool isLineStartForbidden(wchar_t c);

// Parse a float from text. In: s, out. Out: true on success.
bool parseFloat(const std::wstring& s, float& out);

// UTF-8 <-> UTF-16 for loading html files. In: string. Out: converted string.
std::wstring utf8ToWide(const std::string& s);
std::string wideToUtf8(const std::wstring& s);

} // namespace ui
