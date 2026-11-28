#include "app.h"

namespace wordlock {

int TestIntervalMs(const wchar_t* name, int fallbackMs) {
    if (!IsTestModeEnabled()) {
        return fallbackMs;
    }
    const std::wstring value = GetEnvironmentValue(name);
    if (value.empty()) {
        return fallbackMs;
    }
    try {
        const int milliseconds = std::stoi(value);
        return milliseconds > 0 ? milliseconds : fallbackMs;
    } catch (...) {
        return fallbackMs;
    }
}

}  // namespace wordlock
