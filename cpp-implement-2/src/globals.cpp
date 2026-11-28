#include "app.h"

namespace wordlock {

AppSettings g_settings;
sqlite3* g_db = nullptr;
int g_nextTaskIndex = 0;
TaskType g_currentTask = TaskType::English;
int g_countdownSeconds = kCountDownStartSeconds;
bool g_countdownPaused = false;
bool g_countdownSettingsPaused = false;
std::wstring g_countDownTheme = L"paper";
POINT g_countDownPosition{};
bool g_countDownPositionValid = false;

std::vector<WebViewBinding> g_webViewBindings;
std::unordered_map<DWORD, FILETIME> g_heldChromeProcesses;
bool g_chromeFreezeShutdownDone = false;

// Held for the lifetime of the winning instance; OS releases it on process exit.
HANDLE g_singleInstanceMutex = nullptr;
HINSTANCE g_hInstance = nullptr;
HWND g_mainWindow = nullptr;
HWND g_countDownWindow = nullptr;
HWND g_settingWindow = nullptr;
HWND g_ignoreWordsWindow = nullptr;
bool g_mainWebViewInitialized = false;
bool g_isQuitting = false;
NOTIFYICONDATAW g_trayIcon{};
ComPtr<ICoreWebView2Controller> g_mainViewController;
ComPtr<ICoreWebView2Controller> g_webViewController;
ComPtr<ICoreWebView2Controller> g_ignoreWordsController;

}  // namespace wordlock
