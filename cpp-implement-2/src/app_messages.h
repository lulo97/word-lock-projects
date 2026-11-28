#pragma once

#include <windows.h>

namespace wordlock {

// Window messages shared by the application and the end-to-end tests.
constexpr UINT kTrayMessage = WM_APP + 1;
constexpr UINT kTestQuitMessage = WM_APP + 2;
constexpr UINT kThemeChangedMessage = WM_APP + 3;

}  // namespace wordlock
