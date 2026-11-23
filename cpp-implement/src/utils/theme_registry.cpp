// theme_registry.cpp
//
// Reads the embedded style list and picks the next style. See the header for
// the data-only theme convention.

#include "utils/theme_registry.h"

#include "core/utf.h"
#include "utils/word_db.h"

#include <algorithm>
#include <random>

namespace {

std::wstring trimmed(const std::wstring& s) {
    const wchar_t* ws = L" \t\r\n";
    size_t begin = s.find_first_not_of(ws);
    if (begin == std::wstring::npos)
        return {};
    size_t end = s.find_last_not_of(ws);
    return s.substr(begin, end - begin + 1);
}

} // namespace

std::string theme_asset_path(const std::wstring& theme, const wchar_t* page) {
    return "assets/themes/" + ui::wideToUtf8(theme) + "/" + ui::wideToUtf8(page);
}

std::wstring load_theme_html(const std::wstring& theme, const wchar_t* page) {
    if (theme.empty())
        return {};
    return ui::utf8ToWide(load_asset_bytes(theme_asset_path(theme, page)));
}

ThemeRegistry::ThemeRegistry() {
    const std::wstring text =
        ui::utf8ToWide(load_asset_bytes("assets/themes/registry.txt"));

    size_t start = 0;
    while (start <= text.size()) {
        size_t end = text.find_first_of(L"\r\n", start);
        if (end == std::wstring::npos)
            end = text.size();

        std::wstring line = trimmed(text.substr(start, end - start));
        if (!line.empty() && line[0] != L'#') {
            // A duplicate line would make pick() repeat itself, so keep the
            // first occurrence only.
            if (std::find(names_.begin(), names_.end(), line) == names_.end())
                names_.push_back(std::move(line));
        }

        if (end == text.size())
            break;
        start = end + 1;
    }
}

std::wstring ThemeRegistry::first() const {
    return names_.empty() ? std::wstring() : names_.front();
}

std::wstring ThemeRegistry::pick(const std::wstring& lastName) const {
    if (names_.empty())
        return {};

    std::vector<size_t> candidates;
    candidates.reserve(names_.size());
    for (size_t i = 0; i < names_.size(); ++i)
        if (names_[i] != lastName)
            candidates.push_back(i);

    // One style, or `lastName` is not in the registry: any style is fine.
    if (candidates.empty())
        return names_.front();

    static thread_local std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<size_t> dist(0, candidates.size() - 1);
    return names_[candidates[dist(rng)]];
}
