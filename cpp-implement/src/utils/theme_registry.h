#pragma once

#include <string>
#include <vector>

// UI styles (M7).
//
// A style is data only: assets/themes/<name>/{input.html,info.html}. The
// ordered list of enabled styles lives in assets/themes/registry.txt (one
// name per line, blank lines and # comments ignored). Adding a style means
// adding a folder and one line here - no C++ change.
//
// The element ids every style must provide are documented in
// assets/themes/_contract.txt; the `theme_contract` feature checks them.
class ThemeRegistry {
public:
    ThemeRegistry();   // reads assets/themes/registry.txt (embedded)

    const std::vector<std::wstring>& names() const { return names_; }
    bool empty() const { return names_.empty(); }

    // First enabled style (used when config.randomUi is false).
    std::wstring first() const;

    // Random style, never the same as `lastName` when at least two are
    // enabled. With one style, returns it. With none, returns empty.
    std::wstring pick(const std::wstring& lastName) const;

private:
    std::vector<std::wstring> names_;
};

// Embedded HTML for one theme page ("input.html" / "info.html").
// Returns empty when the theme or file is missing.
std::wstring load_theme_html(const std::wstring& theme, const wchar_t* page);

// Asset path of a theme page, e.g. "assets/themes/ink/input.html".
std::string theme_asset_path(const std::wstring& theme, const wchar_t* page);
