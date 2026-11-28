#include "app.h"

namespace wordlock {

const std::vector<std::wstring>& ThemeIds() {
    static const std::vector<std::wstring> themes = {
        L"paper", L"brutal", L"candy", L"glass", L"midnight",
        L"notebook", L"sakura", L"synthwave", L"terminal", L"zen"};
    return themes;
}

int NormalizeCountdownSeconds(double value) {
    if (!std::isfinite(value)) {
        return kCountDownStartSeconds;
    }
    return std::clamp(static_cast<int>(std::lround(value)), 1, 3600);
}

double NormalizeVolume(double value) {
    if (!std::isfinite(value)) {
        return 1.0;
    }
    return std::clamp(std::round(value * 2.0) / 2.0, 0.0, 4.0);
}

std::wstring NormalizeTheme(const std::wstring& value) {
    const auto& themes = ThemeIds();
    return std::find(themes.begin(), themes.end(), value) != themes.end() ? value : L"paper";
}

std::wstring NextThemeId(const std::wstring& current) {
    const auto& themes = ThemeIds();
    const auto it = std::find(themes.begin(), themes.end(), current);
    const size_t index = it == themes.end() ? 0 : static_cast<size_t>(it - themes.begin());
    return themes[(index + 1) % themes.size()];
}

std::wstring Trim(const std::wstring& value) {
    size_t begin = 0;
    while (begin < value.size() && std::iswspace(value[begin])) {
        ++begin;
    }
    size_t end = value.size();
    while (end > begin && std::iswspace(value[end - 1])) {
        --end;
    }
    return value.substr(begin, end - begin);
}

}  // namespace wordlock
