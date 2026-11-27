#define NOMINMAX
#include <windows.h>
#include <shellapi.h>
#include <objbase.h>
#include <tlhelp32.h>
#include <winhttp.h>
#include <wrl.h>
#include <wrl/client.h>
#include <WebView2.h>
#include <sqlite3.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <cwctype>
#include <filesystem>
#include <fstream>
#include <cstdint>
#include <functional>
#include <iterator>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

using Microsoft::WRL::Callback;
using Microsoft::WRL::ComPtr;

constexpr int kClientWidth = 1280;
constexpr int kClientHeight = 720;
constexpr wchar_t kWindowClass[] = L"MainWindow";
constexpr wchar_t kCountDownClass[] = L"CountDownWindow";
constexpr wchar_t kSettingClass[] = L"SettingWindows";
constexpr wchar_t kIgnoreClass[] = L"IgnoreWordsWindows";
constexpr wchar_t kWindowTitle[] = L"MainWindow";
constexpr wchar_t kCountDownTitle[] = L"CountDownWindow";
constexpr wchar_t kSettingTitle[] = L"SettingWindows";
constexpr wchar_t kIgnoreTitle[] = L"IgnoreWordsWindows";
constexpr wchar_t kTrayTip[] = L"PomodoroLock";

constexpr wchar_t kSingleInstanceMutexName[] = L"Local\\PomodoroLockSingleInstance";
constexpr wchar_t kChromeProcessName[] = L"chrome.exe";
constexpr wchar_t kChromeFreezeMockEnv[] = L"WORDLOCK_CHROME_FREEZE_MOCK";
constexpr wchar_t kUseTestDataEnv[] = L"WORDLOCK_USE_TEST_DATA";
constexpr wchar_t kIgnoreError[] = L"can't save ignore words";
constexpr wchar_t kPortableExecutableEnv[] = L"PORTABLE_EXECUTABLE_FILE";
constexpr wchar_t kTtsHost[] = L"127.0.0.1";
constexpr unsigned short kTtsPort = 8080;
constexpr wchar_t kTtsPath[] = L"/tts";
constexpr wchar_t kTtsUnavailableMessage[] = L"TTS server is not available at 127.0.0.1:8080.";

constexpr int kCountDownWidth = 100;
constexpr int kCountDownHeight = 50;
constexpr int kCountDownMargin = 10;
constexpr BYTE kCountDownOpacity = 128;  // 0.5 * 255
constexpr int kCountDownFontHeight = 28;
constexpr int kCountDownStartSeconds = 60;
constexpr UINT_PTR kCountDownTimerId = 1;
constexpr UINT_PTR kChromeFreezeTimerId = 2;
constexpr int kSettingWindowWidth = 780;
constexpr int kSettingWindowHeight = 780;
constexpr int kIgnoreWindowWidth = 560;
constexpr int kIgnoreWindowHeight = 620;

constexpr UINT kTrayMessage = WM_APP + 1;
constexpr UINT kTrayIconId = 1;

constexpr UINT ID_TRAY_OPEN = 1;
constexpr UINT ID_TRAY_SETTING = 2;
constexpr UINT ID_TRAY_EXIT = 3;

struct TaskSettings {
    bool englishWord = true;
    bool multipleChoice = true;
    bool japaneseWord = true;
    bool dumbbellWorkout = true;
};

enum class TaskType { English, Mcq, Japanese, Dumbbell };

struct IgnoreWords {
    std::vector<std::wstring> english;
    std::vector<std::wstring> japanese;
};

struct AppSettings {
    TaskSettings tasks;
    int countdownSeconds = kCountDownStartSeconds;
    double masterVolume = 1.0;
    bool chromeFreeze = false;
    bool autoStart = false;
    bool autoTheme = false;
    std::wstring theme = L"paper";
    IgnoreWords ignoreWords;
};

AppSettings g_settings;
sqlite3* g_db = nullptr;
int g_nextTaskIndex = 0;
TaskType g_currentTask = TaskType::English;
int g_countdownSeconds = kCountDownStartSeconds;

struct WebViewBinding {
    ComPtr<ICoreWebView2Controller> controller;
    EventRegistrationToken messageToken{};
};

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

LRESULT CALLBACK CountDownWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK SettingWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK IgnoreWordsWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
void OpenIgnoreWordsWindow();
void HideSettingsWindows();
void InitializeMainWebView(HWND hwnd);
void UpdateMainContent();
void ShowMainWindow();
void StartCountdown();
void NotifyActiveViewHide();
std::wstring AppAssetUrl(const wchar_t* file);
void BroadcastTheme(const std::wstring& theme);
void SyncChromeFreeze();
void ResumeAllChromeProcesses();
void UnfreezeChromeOnExit();
void ApplyAutoStartPreference(bool enabled);
void SyncAutoStartOnStartup();
void HandleHostMessage(ICoreWebView2* sender, const std::wstring& message);
std::wstring BuildSettingsDataJson();

int ScaleForDpi(int value, UINT dpi) {
    return MulDiv(value, static_cast<int>(dpi), USER_DEFAULT_SCREEN_DPI);
}

void HandleDpiChanged(HWND hwnd, LPARAM lParam) {
    const RECT* suggested = reinterpret_cast<const RECT*>(lParam);
    SetWindowPos(hwnd, nullptr,
                 suggested->left, suggested->top,
                 suggested->right - suggested->left,
                 suggested->bottom - suggested->top,
                 SWP_NOZORDER | SWP_NOACTIVATE);
    InvalidateRect(hwnd, nullptr, TRUE);
}

bool IsChromeFreezeMockEnabled() {
    wchar_t value[8]{};
    const DWORD length =
        GetEnvironmentVariableW(kChromeFreezeMockEnv, value, static_cast<DWORD>(std::size(value)));
    return length > 0 && length < std::size(value) && wcscmp(value, L"1") == 0;
}

// Mirrors Electron's requestSingleInstanceLock(): tests bypass the lock so
// separate test launches do not interfere with each other.
bool AcquireSingleInstanceLock() {
    if (IsChromeFreezeMockEnabled()) {
        return true;
    }

    g_singleInstanceMutex = CreateMutexW(nullptr, TRUE, kSingleInstanceMutexName);
    if (g_singleInstanceMutex == nullptr) {
        return true;  // Fail open: a lock error must not block the real instance.
    }

    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        CloseHandle(g_singleInstanceMutex);
        g_singleInstanceMutex = nullptr;
        return false;
    }

    return true;
}

// A duplicate launch must hand Chrome back before exiting, just like the
// Electron helper's ResumeAll/Cleanup path. ResumeAll also powers the startup
// crash-recovery pass, so both paths share the same safe implementation.
void ResumeHeldChromeProcesses() {
    ResumeAllChromeProcesses();
}

void PaintCenteredText(HWND hwnd, const wchar_t* text, int fontHeight) {
    PAINTSTRUCT paint{};
    HDC hdc = BeginPaint(hwnd, &paint);

    RECT clientRect{};
    GetClientRect(hwnd, &clientRect);

    // Pure white background.
    FillRect(hdc, &clientRect, reinterpret_cast<HBRUSH>(GetStockObject(WHITE_BRUSH)));

    const UINT dpi = GetDpiForWindow(hwnd);
    const int physicalFontHeight = (dpi == 0)
                                       ? fontHeight
                                       : ScaleForDpi(fontHeight, dpi);

    HFONT font = CreateFontW(
        -physicalFontHeight,              // Height in physical pixels (negative = character height)
        0,                                // Default width
        0, 0,                             // Default escapement/orientation
        FW_NORMAL,                        // Weight
        FALSE, FALSE, FALSE,              // No italic, underline, or strikeout
        DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE,
        L"Segoe UI");

    HFONT oldFont = static_cast<HFONT>(SelectObject(hdc, font));
    SetTextColor(hdc, RGB(0, 0, 0));
    SetBkMode(hdc, TRANSPARENT);

    DrawTextW(hdc, text, -1, &clientRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    SelectObject(hdc, oldFont);
    DeleteObject(font);

    EndPaint(hwnd, &paint);
}

void StopCountdown() {
    if (g_countDownWindow && IsWindow(g_countDownWindow)) {
        KillTimer(g_countDownWindow, kCountDownTimerId);
        ShowWindow(g_countDownWindow, SW_HIDE);
    }
}

void StartCountdown() {
    g_countdownSeconds = std::clamp(g_settings.countdownSeconds, 1, 3600);

    if (!g_isQuitting) {
        NotifyActiveViewHide();
    }
    HideSettingsWindows();
    if (g_mainWindow && IsWindow(g_mainWindow)) {
        ShowWindow(g_mainWindow, SW_HIDE);
    }
    if (g_countDownWindow && IsWindow(g_countDownWindow)) {
        KillTimer(g_countDownWindow, kCountDownTimerId);
        SetTimer(g_countDownWindow, kCountDownTimerId, 1000, nullptr);
        ShowWindow(g_countDownWindow, SW_SHOW);
        UpdateWindow(g_countDownWindow);
    }
}

TaskType GetNextTaskType() {
    std::vector<TaskType> enabled;
    if (g_settings.tasks.englishWord) enabled.push_back(TaskType::English);
    if (g_settings.tasks.multipleChoice) enabled.push_back(TaskType::Mcq);
    if (g_settings.tasks.japaneseWord) enabled.push_back(TaskType::Japanese);
    if (g_settings.tasks.dumbbellWorkout) enabled.push_back(TaskType::Dumbbell);

    if (enabled.empty()) {
        return TaskType::English;
    }

    const TaskType next = enabled[static_cast<size_t>(g_nextTaskIndex) % enabled.size()];
    g_nextTaskIndex = (g_nextTaskIndex + 1) % 1000000;
    return next;
}

const wchar_t* TaskAsset(TaskType task) {
    switch (task) {
    case TaskType::English: return L"english.html";
    case TaskType::Mcq: return L"mcq.html";
    case TaskType::Japanese: return L"japanese.html";
    case TaskType::Dumbbell: return L"dumbbell.html";
    }
    return L"english.html";
}

void NavigateMainToTask(TaskType task) {
    g_currentTask = task;
    if (!g_mainViewController) {
        return;
    }
    ComPtr<ICoreWebView2> webView;
    if (SUCCEEDED(g_mainViewController->get_CoreWebView2(&webView)) && webView) {
        const std::wstring url = AppAssetUrl(TaskAsset(task));
        webView->Navigate(url.c_str());
    }
}

void ShowMainWindow() {
    StopCountdown();
    if (g_mainWindow && IsWindow(g_mainWindow)) {
        ShowWindow(g_mainWindow, SW_MAXIMIZE);
        SetForegroundWindow(g_mainWindow);

        const TaskType task = GetNextTaskType();

        // WebView2 renders reliably only after its parent window has been shown.
        if (!g_mainWebViewInitialized) {
            g_mainWebViewInitialized = true;
            g_currentTask = task;
            InitializeMainWebView(g_mainWindow);
        } else {
            NavigateMainToTask(task);
        }
    }
}

void NotifyActiveViewHide() {
    if (!g_mainViewController) {
        return;
    }
    ComPtr<ICoreWebView2> webView;
    if (SUCCEEDED(g_mainViewController->get_CoreWebView2(&webView)) && webView) {
        webView->PostWebMessageAsJson(L"{\"type\":\"app:hide\"}");
    }
}

// Tray activation mirrors Electron's isVisible() ? hideMainWindow() : showMainWindow().
void ToggleMainWindow() {
    if (g_mainWindow && IsWindow(g_mainWindow) && IsWindowVisible(g_mainWindow)) {
        StartCountdown();
    } else {
        ShowMainWindow();
    }
}

// An intentional quit bypasses the tray close-interception path. If the main
// window is already gone, post the quit message directly so no invisible
// GetMessage loop is left behind.
void QuitApp() {
    g_isQuitting = true;
    if (g_mainWindow && IsWindow(g_mainWindow)) {
        DestroyWindow(g_mainWindow);
    } else {
        PostQuitMessage(0);
    }
}

void HideSettingsWindows() {
    if (g_settingWindow && IsWindow(g_settingWindow)) {
        ShowWindow(g_settingWindow, SW_HIDE);
    }
    if (g_ignoreWordsWindow && IsWindow(g_ignoreWordsWindow)) {
        ShowWindow(g_ignoreWordsWindow, SW_HIDE);
    }
}

void UpdateWebViewBounds(HWND hwnd, ComPtr<ICoreWebView2Controller>& controller) {
    if (!controller) {
        return;
    }
    RECT bounds{};
    GetClientRect(hwnd, &bounds);
    controller->put_Bounds(bounds);
}

using WebViewReady = std::function<void(ComPtr<ICoreWebView2>&)>;

std::wstring GetExecutableDirectory() {
    wchar_t exePath[MAX_PATH]{};
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);

    std::wstring path(exePath);
    const size_t slash = path.find_last_of(L"\\/");
    if (slash != std::wstring::npos) {
        path.resize(slash + 1);
    }
    return path;
}

std::wstring AppAssetUrl(const wchar_t* file) {
    return std::wstring(L"http://app/") + file;
}

int HexDigit(wchar_t ch) {
    if (ch >= L'0' && ch <= L'9') return ch - L'0';
    if (ch >= L'a' && ch <= L'f') return ch - L'a' + 10;
    if (ch >= L'A' && ch <= L'F') return ch - L'A' + 10;
    return -1;
}

std::wstring UrlDecode(const std::wstring& value) {
    std::wstring out;
    for (size_t i = 0; i < value.size(); ++i) {
        if (value[i] == L'%' && i + 2 < value.size()) {
            const int high = HexDigit(value[i + 1]);
            const int low = HexDigit(value[i + 2]);
            if (high >= 0 && low >= 0) {
                out.push_back(static_cast<wchar_t>((high << 4) | low));
                i += 2;
                continue;
            }
        }
        if (value[i] == L'+') {
            out.push_back(L' ');
        } else {
            out.push_back(value[i]);
        }
    }
    return out;
}

bool IsAppResourceUriAllowed(const std::wstring& uri) {
    constexpr wchar_t kPrefix[] = L"http://app/";
    if (uri.rfind(kPrefix, 0) != 0) {
        return true;  // Not an app:// request; WebView2 may serve it normally.
    }

    std::wstring relative = UrlDecode(uri.substr(std::size(kPrefix) - 1));
    const size_t query = relative.find_first_of(L"?#");
    if (query != std::wstring::npos) {
        relative.resize(query);
    }
    std::replace(relative.begin(), relative.end(), L'/', L'\\');

    std::error_code error;
    const std::filesystem::path root = std::filesystem::weakly_canonical(GetExecutableDirectory(), error);
    const std::filesystem::path target =
        std::filesystem::weakly_canonical(std::filesystem::path(GetExecutableDirectory()) / relative, error);
    if (error) {
        return false;
    }

    const std::wstring rootText = root.wstring();
    const std::wstring targetText = target.wstring();
    if (targetText.rfind(rootText, 0) != 0) {
        return false;
    }
    return targetText.size() == rootText.size() || targetText[rootText.size()] == L'\\';
}

bool EnvFlagEnabled(const wchar_t* name) {
    wchar_t value[8]{};
    const DWORD length = GetEnvironmentVariableW(name, value, static_cast<DWORD>(std::size(value)));
    return length > 0 && length < std::size(value) && wcscmp(value, L"1") == 0;
}

bool IsTestDataMode() {
    return EnvFlagEnabled(kUseTestDataEnv);
}

std::wstring GetDataDirectory() {
    if (IsTestDataMode()) {
        return std::wstring();
    }

    wchar_t home[MAX_PATH]{};
    const DWORD length = GetEnvironmentVariableW(L"USERPROFILE", home, static_cast<DWORD>(MAX_PATH));
    std::wstring directory = length > 0 ? std::wstring(home) : std::wstring(L"C:\\");
    if (!directory.empty() && directory.back() != L'\\') {
        directory.push_back(L'\\');
    }
    directory += L"pomodoro_lock\\";
    std::filesystem::create_directories(directory);
    return directory;
}

std::wstring Utf8ToWide(const std::string& text) {
    if (text.empty()) {
        return std::wstring();
    }
    const int length = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
    if (length <= 0) {
        return std::wstring();
    }
    std::vector<wchar_t> buffer(static_cast<size_t>(length));
    MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, buffer.data(), length);
    return std::wstring(buffer.data(), static_cast<size_t>(length - 1));
}

std::string WideToUtf8(const std::wstring& text) {
    if (text.empty()) {
        return std::string();
    }
    const int length = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (length <= 0) {
        return std::string();
    }
    std::vector<char> buffer(static_cast<size_t>(length));
    WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, buffer.data(), length, nullptr, nullptr);
    return std::string(buffer.data(), static_cast<size_t>(length - 1));
}

std::wstring JsonEscape(const std::wstring& value) {
    std::wstring out;
    for (wchar_t ch : value) {
        switch (ch) {
        case L'"': out += L"\\\""; break;
        case L'\\': out += L"\\\\"; break;
        case L'\n': out += L"\\n"; break;
        case L'\r': out += L"\\r"; break;
        case L'\t': out += L"\\t"; break;
        default:
            if (ch < 0x20) {
                static const wchar_t* digits = L"0123456789abcdef";
                out += L"\\u00";
                out.push_back(digits[(ch >> 4) & 0xF]);
                out.push_back(digits[ch & 0xF]);
            } else {
                out.push_back(ch);
            }
            break;
        }
    }
    return out;
}

std::wstring JsLiteral(const std::wstring& value) {
    return L"\"" + JsonEscape(value) + L"\"";
}

std::wstring Base64Encode(const std::vector<unsigned char>& bytes) {
    static const char table[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve(((bytes.size() + 2) / 3) * 4);

    size_t index = 0;
    while (index + 2 < bytes.size()) {
        const uint32_t value = (static_cast<uint32_t>(bytes[index]) << 16) |
                               (static_cast<uint32_t>(bytes[index + 1]) << 8) |
                               static_cast<uint32_t>(bytes[index + 2]);
        out.push_back(table[(value >> 18) & 0x3F]);
        out.push_back(table[(value >> 12) & 0x3F]);
        out.push_back(table[(value >> 6) & 0x3F]);
        out.push_back(table[value & 0x3F]);
        index += 3;
    }

    if (index < bytes.size()) {
        const size_t remaining = bytes.size() - index;
        const uint32_t value = static_cast<uint32_t>(bytes[index]) << 16 |
                               (remaining == 2 ? static_cast<uint32_t>(bytes[index + 1]) << 8 : 0);
        out.push_back(table[(value >> 18) & 0x3F]);
        out.push_back(table[(value >> 12) & 0x3F]);
        out.push_back(remaining == 2 ? table[(value >> 6) & 0x3F] : '=');
        out.push_back('=');
    }

    return Utf8ToWide(out);
}

uint32_t ReadLe32(const std::vector<unsigned char>& bytes, size_t offset) {
    if (offset + 4 > bytes.size()) {
        return 0;
    }
    return static_cast<uint32_t>(bytes[offset]) |
           (static_cast<uint32_t>(bytes[offset + 1]) << 8) |
           (static_cast<uint32_t>(bytes[offset + 2]) << 16) |
           (static_cast<uint32_t>(bytes[offset + 3]) << 24);
}

uint16_t ReadLe16(const std::vector<unsigned char>& bytes, size_t offset) {
    if (offset + 2 > bytes.size()) {
        return 0;
    }
    return static_cast<uint16_t>(bytes[offset] |
                                 (static_cast<uint16_t>(bytes[offset + 1]) << 8));
}

std::vector<unsigned char> ApplyWavGain(std::vector<unsigned char> wav, double gain) {
    if (!std::isfinite(gain) || gain == 1.0 || wav.size() < 12 ||
        std::memcmp(wav.data(), "RIFF", 4) != 0 ||
        std::memcmp(wav.data() + 8, "WAVE", 4) != 0) {
        return wav;
    }

    int format = -1;
    int bitsPerSample = -1;
    size_t dataOffset = std::wstring::npos;
    uint32_t dataSize = 0;

    size_t offset = 12;
    while (offset + 8 <= wav.size()) {
        const std::string id(reinterpret_cast<const char*>(wav.data() + offset), 4);
        const uint32_t size = ReadLe32(wav, offset + 4);
        const size_t start = offset + 8;
        if (id == "fmt " && size >= 16 && start + 16 <= wav.size()) {
            format = ReadLe16(wav, start);
            bitsPerSample = ReadLe16(wav, start + 14);
        }
        if (id == "data") {
            dataOffset = start;
            dataSize = size;
            break;
        }
        const size_t next = start + size + (size % 2);
        if (next <= offset) {
            break;
        }
        offset = next;
    }

    if (format != 1 || bitsPerSample != 16 || dataOffset == std::wstring::npos) {
        return wav;
    }

    const size_t end = std::min(dataOffset + dataSize, wav.size());
    for (size_t i = dataOffset; i + 1 < end; i += 2) {
        int16_t sample = 0;
        std::memcpy(&sample, wav.data() + i, sizeof(sample));
        const int32_t scaled = std::clamp(
            static_cast<int32_t>(std::llround(static_cast<double>(sample) * gain)),
            -32768, 32767);
        const int16_t clipped = static_cast<int16_t>(scaled);
        std::memcpy(wav.data() + i, &clipped, sizeof(clipped));
    }
    return wav;
}

bool SynthesizeTts(const std::wstring& text,
                   const std::wstring& lang,
                   std::wstring& wavBase64,
                   std::wstring& error) {
    error = kTtsUnavailableMessage;
    wavBase64.clear();

    HINTERNET session = WinHttpOpen(L"PomodoroLock TTS/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                    WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (session == nullptr) {
        return false;
    }
    WinHttpSetTimeouts(session, 5000, 5000, 300000, 300000);

    HINTERNET connect = WinHttpConnect(session, kTtsHost, kTtsPort, 0);
    if (connect == nullptr) {
        WinHttpCloseHandle(session);
        return false;
    }

    HINTERNET request = WinHttpOpenRequest(connect, L"POST", kTtsPath, nullptr,
                                           WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
    if (request == nullptr) {
        WinHttpCloseHandle(connect);
        WinHttpCloseHandle(session);
        return false;
    }

    const std::string body = "{\"input\":" + WideToUtf8(JsLiteral(text)) +
                             ",\"lang\":" + WideToUtf8(JsLiteral(lang)) +
                             ",\"voice\":\"female\"}";
    const DWORD bodySize = static_cast<DWORD>(body.size());
    WinHttpAddRequestHeaders(request, L"Content-Type: application/json\r\n",
                             static_cast<DWORD>(-1), WINHTTP_ADDREQ_FLAG_ADD);
    if (!WinHttpSendRequest(request, nullptr, 0,
                            reinterpret_cast<LPVOID>(const_cast<char*>(body.data())),
                            bodySize, bodySize, 0) ||
        !WinHttpReceiveResponse(request, nullptr)) {
        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connect);
        WinHttpCloseHandle(session);
        return false;
    }

    DWORD status = 0;
    DWORD statusSize = sizeof(status);
    WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                        WINHTTP_NO_HEADER_INDEX, &status, &statusSize, nullptr);
    if (status < 200 || status >= 300) {
        error = std::wstring(kTtsUnavailableMessage) + L" (HTTP " + std::to_wstring(status) + L")";
        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connect);
        WinHttpCloseHandle(session);
        return false;
    }

    std::vector<unsigned char> responseBytes;
    std::vector<unsigned char> buffer(16384);
    while (true) {
        DWORD read = 0;
        if (!WinHttpReadData(request, buffer.data(), static_cast<DWORD>(buffer.size()), &read) ||
            read == 0) {
            break;
        }
        responseBytes.insert(responseBytes.end(), buffer.begin(), buffer.begin() + read);
    }

    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connect);
    WinHttpCloseHandle(session);

    wavBase64 = Base64Encode(ApplyWavGain(std::move(responseBytes), g_settings.masterVolume));
    return true;
}

bool TryGetStringField(const std::wstring& json, const std::wstring& key, std::wstring& out) {
    const std::wstring needle = L"\"" + key + L"\":";
    const size_t start = json.find(needle);
    if (start == std::wstring::npos) {
        return false;
    }
    size_t index = start + needle.size();
    while (index < json.size() && std::isspace(static_cast<unsigned char>(json[index]))) {
        ++index;
    }
    if (index >= json.size() || json[index] != L'"') {
        return false;
    }
    ++index;
    out.clear();
    while (index < json.size()) {
        wchar_t ch = json[index++];
        if (ch == L'"') {
            return true;
        }
        if (ch == L'\\' && index < json.size()) {
            ch = json[index++];
            switch (ch) {
            case L'n': ch = L'\n'; break;
            case L'r': ch = L'\r'; break;
            case L't': ch = L'\t'; break;
            case L'"': break;
            case L'\\': break;
            default: break;
            }
        }
        out.push_back(ch);
    }
    return false;
}

bool TryGetNumberField(const std::wstring& json, const std::wstring& key, double& out) {
    const std::wstring needle = L"\"" + key + L"\":";
    const size_t start = json.find(needle);
    if (start == std::wstring::npos) {
        return false;
    }
    size_t index = start + needle.size();
    while (index < json.size() && std::isspace(static_cast<unsigned char>(json[index]))) {
        ++index;
    }
    size_t end = index;
    while (end < json.size()) {
        const wchar_t ch = json[end];
        if (!(ch == L'-' || ch == L'+' || ch == L'.' || (ch >= L'0' && ch <= L'9'))) {
            break;
        }
        ++end;
    }
    if (end == index) {
        return false;
    }
    out = wcstod(json.substr(index, end - index).c_str(), nullptr);
    return true;
}

bool TryGetBoolField(const std::wstring& json,
                     const std::wstring& key,
                     bool defaultValue = false) {
    const std::wstring needle = L"\"" + key + L"\":";
    const size_t start = json.find(needle);
    if (start == std::wstring::npos) {
        return defaultValue;
    }
    size_t index = start + needle.size();
    while (index < json.size() && std::isspace(static_cast<unsigned char>(json[index]))) {
        ++index;
    }
    if (json.compare(index, 4, L"true") == 0) {
        return true;
    }
    if (json.compare(index, 5, L"false") == 0) {
        return false;
    }
    return defaultValue;
}

const wchar_t* BoolLiteral(bool value) {
    return value ? L"true" : L"false";
}

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
        while (index < json.size() && json[index] != L'"') {
            value.push_back(json[index++]);
        }
        if (index >= json.size()) {
            return false;
        }
        ++index;
        out.push_back(value);
    }
    return false;
}

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
    while (begin < value.size() && std::isspace(static_cast<unsigned char>(value[begin]))) {
        ++begin;
    }
    size_t end = value.size();
    while (end > begin && std::isspace(static_cast<unsigned char>(value[end - 1]))) {
        --end;
    }
    return value.substr(begin, end - begin);
}

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
            if (std::isspace(static_cast<unsigned char>(ch))) {
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

void LoadSettings() {
    g_settings = AppSettings{};
    if (IsTestDataMode()) {
        return;
    }

    const std::filesystem::path path = std::filesystem::path(GetDataDirectory()) / L"settings.json";
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return;
    }
    const std::string bytes((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    const std::wstring json = Utf8ToWide(bytes);

    double number = 0.0;
    g_settings.tasks.englishWord = TryGetBoolField(json, L"englishWord", true);
    g_settings.tasks.multipleChoice = TryGetBoolField(json, L"multipleChoice", true);
    g_settings.tasks.japaneseWord = TryGetBoolField(json, L"japaneseWord", true);
    g_settings.tasks.dumbbellWorkout = TryGetBoolField(json, L"dumbbellWorkout", true);
    if (TryGetNumberField(json, L"countdownSeconds", number)) {
        g_settings.countdownSeconds = NormalizeCountdownSeconds(number);
    }
    if (TryGetNumberField(json, L"masterVolume", number)) {
        g_settings.masterVolume = NormalizeVolume(number);
    }
    g_settings.chromeFreeze = TryGetBoolField(json, L"chromeFreeze", false);
    g_settings.autoStart = TryGetBoolField(json, L"autoStart", false);
    g_settings.autoTheme = TryGetBoolField(json, L"autoTheme", false);
    std::wstring theme;
    if (TryGetStringField(json, L"theme", theme)) {
        g_settings.theme = NormalizeTheme(theme);
    }
    std::vector<std::wstring> words;
    if (TryGetStringArrayField(json, L"english", words)) {
        g_settings.ignoreWords.english = words;
    }
    if (TryGetStringArrayField(json, L"japanese", words)) {
        g_settings.ignoreWords.japanese = words;
    }
}

std::wstring JsonStringArray(const std::vector<std::wstring>& values) {
    std::wstring out = L"[";
    for (size_t i = 0; i < values.size(); ++i) {
        if (i != 0) {
            out += L",";
        }
        out += L"\"" + JsonEscape(values[i]) + L"\"";
    }
    out += L"]";
    return out;
}

void SaveSettings() {
    if (IsTestDataMode()) {
        return;
    }
    const std::filesystem::path path = std::filesystem::path(GetDataDirectory()) / L"settings.json";
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file) {
        return;
    }
    std::wstring json = L"{\n";
    json += L"  \"englishWord\": " + std::wstring(BoolLiteral(g_settings.tasks.englishWord)) + L",\n";
    json += L"  \"multipleChoice\": " + std::wstring(BoolLiteral(g_settings.tasks.multipleChoice)) + L",\n";
    json += L"  \"japaneseWord\": " + std::wstring(BoolLiteral(g_settings.tasks.japaneseWord)) + L",\n";
    json += L"  \"dumbbellWorkout\": " + std::wstring(BoolLiteral(g_settings.tasks.dumbbellWorkout)) + L",\n";
    json += L"  \"masterVolume\": " + std::to_wstring(g_settings.masterVolume) + L",\n";
    json += L"  \"countdownSeconds\": " + std::to_wstring(g_settings.countdownSeconds) + L",\n";
    json += L"  \"chromeFreeze\": " + std::wstring(BoolLiteral(g_settings.chromeFreeze)) + L",\n";
    json += L"  \"autoStart\": " + std::wstring(BoolLiteral(g_settings.autoStart)) + L",\n";
    json += L"  \"autoTheme\": " + std::wstring(BoolLiteral(g_settings.autoTheme)) + L",\n";
    json += L"  \"theme\": \"" + JsonEscape(g_settings.theme) + L"\",\n";
    json += L"  \"ignoreWords\": {\n";
    json += L"    \"english\": " + JsonStringArray(g_settings.ignoreWords.english) + L",\n";
    json += L"    \"japanese\": " + JsonStringArray(g_settings.ignoreWords.japanese) + L"\n";
    json += L"  }\n}\n";
    const std::string bytes = WideToUtf8(json);
    file.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
}

bool ExecuteSql(const char* sql) {
    char* error = nullptr;
    const int result = sqlite3_exec(g_db, sql, nullptr, nullptr, &error);
    if (error != nullptr) {
        sqlite3_free(error);
    }
    return result == SQLITE_OK;
}

bool EnsureColumn(const char* table, const char* column, const char* definition) {
    const std::string sql = std::string("PRAGMA table_info(") + table + ")";
    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(g_db, sql.c_str(), -1, &statement, nullptr) != SQLITE_OK) {
        return false;
    }
    bool found = false;
    while (sqlite3_step(statement) == SQLITE_ROW) {
        const unsigned char* name = sqlite3_column_text(statement, 1);
        if (name != nullptr && strcmp(reinterpret_cast<const char*>(name), column) == 0) {
            found = true;
            break;
        }
    }
    sqlite3_finalize(statement);
    if (found) {
        return true;
    }
    const std::string alter = std::string("ALTER TABLE ") + table + " ADD COLUMN " + column + " " + definition;
    return ExecuteSql(alter.c_str());
}

void InitializeDatabase() {
    const std::string path = IsTestDataMode()
                                 ? ":memory:"
                                 : WideToUtf8(GetDataDirectory() + L"pomodoro_lock.db");
    if (sqlite3_open(path.c_str(), &g_db) != SQLITE_OK) {
        g_db = nullptr;
        return;
    }

    ExecuteSql("CREATE TABLE IF NOT EXISTS english_word ("
               "id INTEGER PRIMARY KEY, word TEXT NOT NULL, meaning_vi TEXT NOT NULL, "
               "examples TEXT NOT NULL, examples_vi TEXT NOT NULL DEFAULT '[]')");
    ExecuteSql("CREATE TABLE IF NOT EXISTS mcq ("
               "id INTEGER PRIMARY KEY, question TEXT NOT NULL, answers TEXT NOT NULL, "
               "correct_answer_idx INTEGER NOT NULL, explanation TEXT NOT NULL)");
    ExecuteSql("CREATE TABLE IF NOT EXISTS japanese_word ("
               "id INTEGER PRIMARY KEY, kanji TEXT NOT NULL, hiragana TEXT NOT NULL, "
               "katakana TEXT NOT NULL, romaji TEXT NOT NULL DEFAULT '', meaning_en TEXT NOT NULL, "
               "examples TEXT NOT NULL, examples_en TEXT NOT NULL DEFAULT '[]', "
               "examples_vi TEXT NOT NULL DEFAULT '[]')");

    EnsureColumn("english_word", "examples_vi", "TEXT NOT NULL DEFAULT '[]'");
    EnsureColumn("japanese_word", "romaji", "TEXT NOT NULL DEFAULT ''");
    EnsureColumn("japanese_word", "examples_en", "TEXT NOT NULL DEFAULT '[]'");
    EnsureColumn("japanese_word", "examples_vi", "TEXT NOT NULL DEFAULT '[]'");

    auto runSeed = [](const char* sql) {
        char* error = nullptr;
        const int result = sqlite3_exec(g_db, sql, nullptr, nullptr, &error);
        if (error != nullptr) {
            sqlite3_free(error);
        }
        (void)result;
    };

    char* sql = sqlite3_mprintf(
        "INSERT INTO english_word (id, word, meaning_vi, examples, examples_vi) VALUES "
        "(0, 'apple', 'quả táo', "
        "'[\"he has an apple\",\"apple is a popular company\",\"an apple a day keep the doctor away\"]', "
        "'[\"anh ấy có một quả táo\",\"Apple là một công ty nổi tiếng\","
        "\"một quả táo mỗi ngày giúp không cần gặp bác sĩ\"]') "
        "ON CONFLICT(id) DO UPDATE SET word=excluded.word, meaning_vi=excluded.meaning_vi, "
        "examples=excluded.examples, examples_vi=excluded.examples_vi");
    runSeed(sql);
    sqlite3_free(sql);

    runSeed("INSERT OR IGNORE INTO mcq (id, question, answers, correct_answer_idx, explanation) "
            "VALUES (0, '1+1=?', '[\"1\",\"2\",\"3\",\"4\"]', 1, '1+1=2')");

    sql = sqlite3_mprintf(
        "INSERT INTO japanese_word "
        "(id, kanji, hiragana, katakana, romaji, meaning_en, examples, examples_en, examples_vi) "
        "VALUES (0, '桜', 'さくら', 'サクラ', 'sakura', 'cherry blossom', "
        "'[\"桜が咲きました。\",\"春に桜を見ます。\"]', "
        "'[\"The cherry blossoms bloomed.\",\"I see cherry blossoms in spring.\"]', "
        "'[\"Hoa anh đào đã nở.\",\"Tôi nhìn hoa anh đào vào mùa xuân.\"]') "
        "ON CONFLICT(id) DO UPDATE SET kanji=excluded.kanji, hiragana=excluded.hiragana, "
        "katakana=excluded.katakana, romaji=excluded.romaji, meaning_en=excluded.meaning_en, "
        "examples=excluded.examples, examples_en=excluded.examples_en, "
        "examples_vi=excluded.examples_vi");
    runSeed(sql);
    sqlite3_free(sql);
}

std::wstring QueryRowJson(const std::string& sql,
                          const std::vector<std::string>& parameters,
                          const std::vector<std::string>& rawJsonColumns) {
    if (g_db == nullptr) {
        return L"null";
    }
    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(g_db, sql.c_str(), -1, &statement, nullptr) != SQLITE_OK) {
        return L"null";
    }
    for (size_t i = 0; i < parameters.size(); ++i) {
        sqlite3_bind_text(statement, static_cast<int>(i + 1), parameters[i].c_str(),
                          static_cast<int>(parameters[i].size()), SQLITE_TRANSIENT);
    }

    std::wstring json = L"null";
    if (sqlite3_step(statement) == SQLITE_ROW) {
        json = L"{";
        const int columns = sqlite3_column_count(statement);
        for (int i = 0; i < columns; ++i) {
            if (i != 0) {
                json += L",";
            }
            const char* name = sqlite3_column_name(statement, i);
            json += L"\"" + JsonEscape(Utf8ToWide(name == nullptr ? "" : name)) + L"\":";
            const unsigned char* text = sqlite3_column_text(statement, i);
            const std::string value = text == nullptr ? "" : reinterpret_cast<const char*>(text);
            if (sqlite3_column_type(statement, i) == SQLITE_NULL) {
                json += L"null";
            } else if (std::find(rawJsonColumns.begin(), rawJsonColumns.end(),
                                 name == nullptr ? "" : name) != rawJsonColumns.end()) {
                json += Utf8ToWide(value);
            } else if (sqlite3_column_type(statement, i) == SQLITE_INTEGER ||
                       sqlite3_column_type(statement, i) == SQLITE_FLOAT) {
                json += Utf8ToWide(value);
            } else {
                json += L"\"" + JsonEscape(Utf8ToWide(value)) + L"\"";
            }
        }
        json += L"}";
    }
    sqlite3_finalize(statement);
    return json;
}

std::wstring LowerWide(const std::wstring& value) {
    std::wstring out = value;
    std::transform(out.begin(), out.end(), out.begin(),
                   [](wchar_t ch) { return static_cast<wchar_t>(std::towlower(ch)); });
    return out;
}

std::vector<std::string> LowerUtf8List(const std::vector<std::wstring>& values) {
    std::vector<std::string> out;
    for (const std::wstring& value : values) {
        const std::wstring lowered = LowerWide(value);
        if (!lowered.empty()) {
            out.push_back(WideToUtf8(lowered));
        }
    }
    return out;
}

std::wstring GetRandomEnglishWordJson() {
    const std::vector<std::string> ignored = LowerUtf8List(g_settings.ignoreWords.english);
    std::string sql = "SELECT * FROM english_word";
    if (!ignored.empty()) {
        sql += " WHERE LOWER(word) NOT IN (";
        for (size_t i = 0; i < ignored.size(); ++i) {
            if (i != 0) sql += ",";
            sql += "?";
        }
        sql += ")";
    }
    sql += " ORDER BY RANDOM() LIMIT 1";
    return QueryRowJson(sql, ignored, {"examples", "examples_vi"});
}

std::wstring GetEnglishWordJson(int id) {
    return QueryRowJson("SELECT * FROM english_word WHERE id = ?", {std::to_string(id)},
                        {"examples", "examples_vi"});
}

std::wstring GetRandomMcqJson() {
    return QueryRowJson("SELECT * FROM mcq ORDER BY RANDOM() LIMIT 1", {}, {"answers"});
}

std::wstring GetMcqJson(int id) {
    return QueryRowJson("SELECT * FROM mcq WHERE id = ?", {std::to_string(id)}, {"answers"});
}

std::wstring GetRandomJapaneseWordJson() {
    const std::vector<std::string> ignored = LowerUtf8List(g_settings.ignoreWords.japanese);
    std::string sql = "SELECT * FROM japanese_word";
    if (!ignored.empty()) {
        sql += " WHERE LOWER(kanji) NOT IN (";
        for (size_t i = 0; i < ignored.size(); ++i) {
            if (i != 0) sql += ",";
            sql += "?";
        }
        sql += ") AND LOWER(hiragana) NOT IN (";
        for (size_t i = 0; i < ignored.size(); ++i) {
            if (i != 0) sql += ",";
            sql += "?";
        }
        sql += ") AND LOWER(katakana) NOT IN (";
        for (size_t i = 0; i < ignored.size(); ++i) {
            if (i != 0) sql += ",";
            sql += "?";
        }
        sql += ") AND LOWER(romaji) NOT IN (";
        for (size_t i = 0; i < ignored.size(); ++i) {
            if (i != 0) sql += ",";
            sql += "?";
        }
        sql += ")";
    }
    sql += " ORDER BY RANDOM() LIMIT 1";

    std::vector<std::string> parameters;
    for (int repeat = 0; repeat < 4; ++repeat) {
        parameters.insert(parameters.end(), ignored.begin(), ignored.end());
    }
    return QueryRowJson(sql, parameters, {"examples", "examples_en", "examples_vi"});
}

std::wstring GetJapaneseWordJson(int id) {
    return QueryRowJson("SELECT * FROM japanese_word WHERE id = ?", {std::to_string(id)},
                        {"examples", "examples_en", "examples_vi"});
}

using NtProcessFn = LONG(NTAPI*)(HANDLE);

NtProcessFn GetNtProcessFunction(const char* name) {
    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    if (ntdll == nullptr) {
        return nullptr;
    }
    return reinterpret_cast<NtProcessFn>(GetProcAddress(ntdll, name));
}

bool SameStartTime(const FILETIME& left, const FILETIME& right) {
    return left.dwLowDateTime == right.dwLowDateTime &&
           left.dwHighDateTime == right.dwHighDateTime;
}

bool GetProcessStartTime(DWORD processId, FILETIME& startTime) {
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId);
    if (process == nullptr) {
        return false;
    }
    FILETIME creation{};
    FILETIME exit{};
    FILETIME kernel{};
    FILETIME user{};
    const BOOL ok = GetProcessTimes(process, &creation, &exit, &kernel, &user);
    CloseHandle(process);
    if (!ok) {
        return false;
    }
    startTime = creation;
    return true;
}

bool IsChromeProcessId(DWORD processId) {
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        return false;
    }
    PROCESSENTRY32W process{};
    process.dwSize = sizeof(process);
    bool found = false;
    if (Process32FirstW(snapshot, &process)) {
        do {
            if (process.th32ProcessID == processId) {
                found = _wcsicmp(process.szExeFile, kChromeProcessName) == 0;
                break;
            }
        } while (Process32NextW(snapshot, &process));
    }
    CloseHandle(snapshot);
    return found;
}

bool ChangeChromeProcessState(DWORD processId, bool suspend) {
    NtProcessFn function = GetNtProcessFunction(suspend ? "NtSuspendProcess" : "NtResumeProcess");
    if (function == nullptr) {
        return false;
    }
    HANDLE process = OpenProcess(PROCESS_SUSPEND_RESUME, FALSE, processId);
    if (process == nullptr) {
        return false;
    }
    const LONG status = function(process);
    CloseHandle(process);
    return status >= 0;
}

void ResumeAllChromeProcesses() {
    if (IsChromeFreezeMockEnabled()) {
        g_heldChromeProcesses.clear();
        return;
    }

    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        return;
    }
    PROCESSENTRY32W process{};
    process.dwSize = sizeof(process);
    const DWORD currentPid = GetCurrentProcessId();
    if (Process32FirstW(snapshot, &process)) {
        do {
            if (process.th32ProcessID != currentPid &&
                _wcsicmp(process.szExeFile, kChromeProcessName) == 0) {
                ChangeChromeProcessState(process.th32ProcessID, false);
            }
        } while (Process32NextW(snapshot, &process));
    }
    CloseHandle(snapshot);
    g_heldChromeProcesses.clear();
}

void FreezeChromeProcesses() {
    if (IsChromeFreezeMockEnabled()) {
        return;
    }

    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        return;
    }
    PROCESSENTRY32W process{};
    process.dwSize = sizeof(process);
    const DWORD currentPid = GetCurrentProcessId();
    if (Process32FirstW(snapshot, &process)) {
        do {
            const DWORD pid = process.th32ProcessID;
            if (pid == currentPid || _wcsicmp(process.szExeFile, kChromeProcessName) != 0) {
                continue;
            }
            FILETIME startTime{};
            if (!GetProcessStartTime(pid, startTime)) {
                continue;
            }
            const auto held = g_heldChromeProcesses.find(pid);
            if (held != g_heldChromeProcesses.end() && SameStartTime(held->second, startTime)) {
                continue;
            }
            if (ChangeChromeProcessState(pid, true)) {
                g_heldChromeProcesses[pid] = startTime;
            }
        } while (Process32NextW(snapshot, &process));
    }
    CloseHandle(snapshot);
}

void UnfreezeHeldChromeProcesses() {
    if (IsChromeFreezeMockEnabled()) {
        g_heldChromeProcesses.clear();
        return;
    }

    const auto held = g_heldChromeProcesses;
    for (const auto& entry : held) {
        FILETIME startTime{};
        if (!IsChromeProcessId(entry.first) || !GetProcessStartTime(entry.first, startTime) ||
            !SameStartTime(startTime, entry.second)) {
            continue;
        }
        ChangeChromeProcessState(entry.first, false);
    }
    g_heldChromeProcesses.clear();
}

void UnfreezeChromeOnExit() {
    if (g_chromeFreezeShutdownDone) {
        return;
    }
    g_chromeFreezeShutdownDone = true;
    if (g_mainWindow && IsWindow(g_mainWindow)) {
        KillTimer(g_mainWindow, kChromeFreezeTimerId);
    }
    UnfreezeHeldChromeProcesses();
}

void SyncChromeFreeze() {
    if (!g_mainWindow || !IsWindow(g_mainWindow)) {
        return;
    }
    if (g_settings.chromeFreeze) {
        FreezeChromeProcesses();
        SetTimer(g_mainWindow, kChromeFreezeTimerId, 2000, nullptr);
    } else {
        KillTimer(g_mainWindow, kChromeFreezeTimerId);
        UnfreezeHeldChromeProcesses();
    }
}

std::wstring GetLaunchExecutablePath() {
    wchar_t portable[MAX_PATH]{};
    const DWORD portableLength = GetEnvironmentVariableW(
        kPortableExecutableEnv, portable, static_cast<DWORD>(std::size(portable)));
    if (portableLength > 0 && portableLength < std::size(portable)) {
        return std::wstring(portable);
    }

    wchar_t executable[MAX_PATH]{};
    GetModuleFileNameW(nullptr, executable, MAX_PATH);
    return std::wstring(executable);
}

bool IsAutoStartDisabledByWindows(const std::wstring& valueName) {
    HKEY key = nullptr;
    if (RegOpenKeyExW(
            HKEY_CURRENT_USER,
            L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\StartupApproved\\Run",
            0, KEY_READ, &key) != ERROR_SUCCESS) {
        return false;
    }

    BYTE data[32]{};
    DWORD size = sizeof(data);
    DWORD type = 0;
    const LSTATUS result = RegQueryValueExW(key, valueName.c_str(), nullptr, &type, data, &size);
    RegCloseKey(key);
    return result == ERROR_SUCCESS && type == REG_BINARY && size > 0 && data[0] == 0x03;
}

void ApplyAutoStartPreference(bool enabled) {
    constexpr wchar_t kRunKey[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
    constexpr wchar_t kRunValue[] = L"PomodoroLock";
    if (IsAutoStartDisabledByWindows(kRunValue)) {
        return;
    }

    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kRunKey, 0, KEY_SET_VALUE, &key) != ERROR_SUCCESS) {
        return;
    }

    if (enabled) {
        const std::wstring command = L"\"" + GetLaunchExecutablePath() + L"\"";
        RegSetValueExW(key, kRunValue, 0, REG_SZ,
                       reinterpret_cast<const BYTE*>(command.c_str()),
                       static_cast<DWORD>((command.size() + 1) * sizeof(wchar_t)));
    } else {
        RegDeleteValueW(key, kRunValue);
    }
    RegCloseKey(key);
}

void SyncAutoStartOnStartup() {
    ApplyAutoStartPreference(g_settings.autoStart);
}

std::wstring BuildSettingsDataJson() {
    std::wstring json = L"{";
    json += L"\"englishWord\":" + std::wstring(BoolLiteral(g_settings.tasks.englishWord));
    json += L",\"multipleChoice\":" + std::wstring(BoolLiteral(g_settings.tasks.multipleChoice));
    json += L",\"japaneseWord\":" + std::wstring(BoolLiteral(g_settings.tasks.japaneseWord));
    json += L",\"dumbbellWorkout\":" + std::wstring(BoolLiteral(g_settings.tasks.dumbbellWorkout));
    json += L",\"masterVolume\":" + std::to_wstring(g_settings.masterVolume);
    json += L",\"countdownSeconds\":" + std::to_wstring(g_settings.countdownSeconds);
    json += L",\"chromeFreeze\":" + std::wstring(BoolLiteral(g_settings.chromeFreeze));
    json += L",\"autoStart\":" + std::wstring(BoolLiteral(g_settings.autoStart));
    json += L",\"autoTheme\":" + std::wstring(BoolLiteral(g_settings.autoTheme));
    json += L",\"theme\":" + JsLiteral(g_settings.theme);
    json += L",\"ignoreWords\":{\"english\":" + JsonStringArray(g_settings.ignoreWords.english) +
            L",\"japanese\":" + JsonStringArray(g_settings.ignoreWords.japanese) + L"}";
    json += L"}";
    return json;
}

std::wstring BuildThemeListJson() {
    struct ThemeInfo {
        const wchar_t* id;
        const wchar_t* name;
        const wchar_t* description;
    };
    static const ThemeInfo themes[] = {
        {L"paper", L"Paper", L"Warm neutral paper"},
        {L"brutal", L"Brutal", L"High-contrast primary colors"},
        {L"candy", L"Candy", L"Soft pink pastels"},
        {L"glass", L"Glass", L"Frosted translucent panels"},
        {L"midnight", L"Midnight", L"Deep navy and cool accents"},
        {L"notebook", L"Notebook", L"Lined notebook paper"},
        {L"sakura", L"Sakura", L"Cherry blossom pink"},
        {L"synthwave", L"Synthwave", L"Neon sunset glow"},
        {L"terminal", L"Terminal", L"Classic green screen"},
        {L"zen", L"Zen", L"Calm stone neutrals"},
    };

    std::wstring json = L"[";
    for (size_t i = 0; i < std::size(themes); ++i) {
        if (i != 0) {
            json += L",";
        }
        json += L"{\"id\":" + JsLiteral(themes[i].id) +
                L",\"name\":" + JsLiteral(themes[i].name) +
                L",\"description\":" + JsLiteral(themes[i].description) + L"}";
    }
    json += L"]";
    return json;
}

void ApplySettingsFromJson(const std::wstring& values, bool confirm) {
    double number = 0.0;
    g_settings.tasks.englishWord =
        TryGetBoolField(values, L"englishWord", g_settings.tasks.englishWord);
    g_settings.tasks.multipleChoice =
        TryGetBoolField(values, L"multipleChoice", g_settings.tasks.multipleChoice);
    g_settings.tasks.japaneseWord =
        TryGetBoolField(values, L"japaneseWord", g_settings.tasks.japaneseWord);
    g_settings.tasks.dumbbellWorkout =
        TryGetBoolField(values, L"dumbbellWorkout", g_settings.tasks.dumbbellWorkout);
    if (TryGetNumberField(values, L"countdownSeconds", number)) {
        g_settings.countdownSeconds = NormalizeCountdownSeconds(number);
    }
    if (TryGetNumberField(values, L"masterVolume", number)) {
        g_settings.masterVolume = NormalizeVolume(number);
    }
    g_settings.chromeFreeze = TryGetBoolField(values, L"chromeFreeze", g_settings.chromeFreeze);
    g_settings.autoStart = TryGetBoolField(values, L"autoStart", g_settings.autoStart);
    g_settings.autoTheme = TryGetBoolField(values, L"autoTheme", g_settings.autoTheme);

    std::wstring theme;
    if (TryGetStringField(values, L"theme", theme)) {
        g_settings.theme = NormalizeTheme(theme);
    }

    g_nextTaskIndex = 0;
    SaveSettings();
    SyncChromeFreeze();
    ApplyAutoStartPreference(g_settings.autoStart);
    BroadcastTheme(g_settings.theme);

    if (confirm) {
        StopCountdown();
        HideSettingsWindows();
        if (g_mainWindow && IsWindow(g_mainWindow)) {
            ShowWindow(g_mainWindow, SW_HIDE);
        }
    }
}

void SendSettingsToSettingsWebView() {
    if (!g_webViewController) {
        return;
    }
    ComPtr<ICoreWebView2> webView;
    if (SUCCEEDED(g_webViewController->get_CoreWebView2(&webView)) && webView) {
        const std::wstring json =
            L"{\"type\":\"settings:changed\",\"settings\":" + BuildSettingsDataJson() + L"}";
        webView->PostWebMessageAsJson(json.c_str());
    }
}

std::string BuildMainHtml() {
    std::string html = R"(<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<style>
  html, body { height: 100%; margin: 0; background: #fff; }
  .screen-frame { display: block; width: 100%; height: 100vh; border: 0; }
</style>
</head>
<body>
)";

    if (g_settings.tasks.englishWord) {
        html += R"(<iframe class="screen-frame" title="English word" src="english.html"></iframe>
)";
    }
    if (g_settings.tasks.japaneseWord) {
        html += R"(<iframe class="screen-frame" title="Japanese word" src="japanese.html"></iframe>
)";
    }
    if (g_settings.tasks.multipleChoice) {
        html += R"(<iframe class="screen-frame" title="Multiple choice" src="mcq.html"></iframe>
)";
    }
    if (g_settings.tasks.dumbbellWorkout) {
        html += R"(<iframe class="screen-frame" title="Dumbbell workout" src="dumbbell.html"></iframe>
)";
    }

    html += "</body>\n</html>";
    return html;
}

void UpdateMainContent() {
    if (!g_mainViewController) {
        return;
    }

    const std::filesystem::path shellPath =
        std::filesystem::path(GetExecutableDirectory()) / L"mainScreens.html";
    std::ofstream shell(shellPath, std::ios::binary | std::ios::trunc);
    if (!shell) {
        MessageBoxW(g_mainWindow, L"Failed to write mainScreens.html.",
                    L"Error", MB_ICONERROR | MB_OK);
        return;
    }
    shell << BuildMainHtml();
    shell.close();

    ComPtr<ICoreWebView2> webView;
    if (SUCCEEDED(g_mainViewController->get_CoreWebView2(&webView)) && webView) {
        webView->Navigate(shellPath.wstring().c_str());
    }
}

void RegisterWebViewBinding(ICoreWebView2Controller* controller,
                            ICoreWebView2* webView,
                            EventRegistrationToken token) {
    if (controller == nullptr || webView == nullptr) {
        return;
    }
    WebViewBinding binding;
    binding.controller = controller;
    binding.messageToken = token;
    g_webViewBindings.push_back(std::move(binding));
}

void UnregisterWebViewBinding(ICoreWebView2Controller* controller) {
    if (controller == nullptr) {
        return;
    }
    g_webViewBindings.erase(
        std::remove_if(g_webViewBindings.begin(), g_webViewBindings.end(),
                       [controller](const WebViewBinding& binding) {
                           return binding.controller.Get() == controller;
                       }),
        g_webViewBindings.end());
}

void BroadcastTheme(const std::wstring& theme) {
    const std::wstring message =
        L"{\"type\":\"theme:changed\",\"theme\":" + JsLiteral(NormalizeTheme(theme)) + L"}";
    for (const WebViewBinding& binding : g_webViewBindings) {
        ComPtr<ICoreWebView2> webView;
        if (SUCCEEDED(binding.controller->get_CoreWebView2(&webView)) && webView) {
            webView->PostWebMessageAsJson(message.c_str());
        }
    }
}

void SendHostReply(ICoreWebView2* sender,
                   const std::wstring& requestId,
                   const std::wstring& resultJson,
                   const std::wstring& error) {
    if (sender == nullptr || requestId.empty()) {
        return;
    }
    std::wstring json = L"{\"type\":\"response\",\"id\":" + JsLiteral(requestId) + L",";
    if (!error.empty()) {
        json += L"\"ok\":false,\"error\":" + JsLiteral(error);
    } else {
        json += L"\"ok\":true,\"result\":" + (resultJson.empty() ? L"null" : resultJson);
    }
    json += L"}";
    sender->PostWebMessageAsJson(json.c_str());
}

std::wstring BuildBootScript() {
    std::wstring script = LR"JS((function () {
  'use strict';
  var pending = {};
  var listeners = {};
  var seq = 0;

  function send(message) {
    if (window.chrome && window.chrome.webview) {
      window.chrome.webview.postMessage(message);
    }
  }

  function emit(type, data) {
    var callbacks = listeners[type] || [];
    for (var i = 0; i < callbacks.length; i++) {
      try { callbacks[i](data); } catch (error) { /* a view must not break the host */ }
    }
  }

  var api = {
    theme: "__THEME__",

    call: function (type, params) {
      return new Promise(function (resolve, reject) {
        var id = 'm' + (++seq);
        pending[id] = { resolve: resolve, reject: reject };
        var message = { type: type, id: id };
        if (params) {
          Object.keys(params).forEach(function (key) { message[key] = params[key]; });
        }
        send(message);
      });
    },

    send: function (type, params) {
      var message = { type: type };
      if (params) {
        Object.keys(params).forEach(function (key) { message[key] = params[key]; });
      }
      send(message);
    },

    on: function (type, callback) {
      if (!listeners[type]) listeners[type] = [];
      listeners[type].push(callback);
    }
  };

  window.app = api;
  try {
    if (document.documentElement) document.documentElement.dataset.theme = api.theme;
  } catch (error) { /* document may still be booting */ }

  api.on('theme:changed', function (data) {
    try {
      if (document.documentElement && data && data.theme) {
        document.documentElement.dataset.theme = data.theme;
      }
    } catch (error) { /* view may be closing */ }
  });

  if (window.chrome && window.chrome.webview) {
    window.chrome.webview.addEventListener('message', function (event) {
      var data = event.data;
      if (!data || !data.type) return;
      if (data.type === 'response') {
        var request = pending[data.id];
        if (!request) return;
        delete pending[data.id];
        if (data.ok) request.resolve(data.result);
        else request.reject(new Error(data.error || 'Host request failed'));
        return;
      }
      emit(data.type, data);
    });
  }
})();)JS";

    const size_t placeholder = script.find(L"__THEME__");
    if (placeholder != std::wstring::npos) {
        script.replace(placeholder, 9, JsLiteral(g_settings.theme));
    }
    return script;
}

void HandleHostMessage(ICoreWebView2* sender, const std::wstring& message) {
    std::wstring type;
    if (!TryGetStringField(message, L"type", type)) {
        return;
    }
    std::wstring requestId;
    TryGetStringField(message, L"id", requestId);

    auto reply = [&](const std::wstring& result, const std::wstring& error = std::wstring()) {
        SendHostReply(sender, requestId, result, error);
    };
    auto numberValue = [&](int fallback) {
        std::wstring text;
        if (!TryGetStringField(message, L"value", text) && !TryGetStringField(message, L"idValue", text)) {
            return fallback;
        }
        try {
            return std::stoi(text);
        } catch (...) {
            return fallback;
        }
    };

    if (type == L"word:getRandom") {
        reply(GetRandomEnglishWordJson());
    } else if (type == L"word:get") {
        reply(GetEnglishWordJson(numberValue(-1)));
    } else if (type == L"mcq:getRandom") {
        reply(GetRandomMcqJson());
    } else if (type == L"mcq:get") {
        reply(GetMcqJson(numberValue(-1)));
    } else if (type == L"japanese:getRandom") {
        reply(GetRandomJapaneseWordJson());
    } else if (type == L"japanese:get") {
        reply(GetJapaneseWordJson(numberValue(-1)));
    } else if (type == L"settings:get") {
        reply(BuildSettingsDataJson());
    } else if (type == L"settings:set") {
        ApplySettingsFromJson(message, false);
        reply(L"null");
    } else if (type == L"settings:confirm") {
        ApplySettingsFromJson(message, true);
        reply(L"null");
    } else if (type == L"settings:close") {
        BroadcastTheme(g_settings.theme);
        HideSettingsWindows();
    } else if (type == L"theme:get") {
        reply(JsLiteral(g_settings.theme));
    } else if (type == L"theme:list") {
        reply(BuildThemeListJson());
    } else if (type == L"theme:preview") {
        std::wstring theme;
        if (TryGetStringField(message, L"value", theme)) {
            BroadcastTheme(theme);
        }
    } else if (type == L"ignoreWords:open") {
        OpenIgnoreWordsWindow();
        reply(L"null");
    } else if (type == L"ignoreWords:get") {
        reply(L"{\"english\":" + JsonStringArray(g_settings.ignoreWords.english) +
              L",\"japanese\":" + JsonStringArray(g_settings.ignoreWords.japanese) + L"}");
    } else if (type == L"ignoreWords:save") {
        std::wstring englishText;
        std::wstring japaneseText;
        TryGetStringField(message, L"english", englishText);
        TryGetStringField(message, L"japanese", japaneseText);
        try {
            IgnoreWords words;
            words.english = ParseIgnoreWords(englishText, false);
            words.japanese = ParseIgnoreWords(japaneseText, true);
            g_settings.ignoreWords = std::move(words);
            SaveSettings();
            reply(L"{\"ok\":true,\"words\":{\"english\":" +
                  JsonStringArray(g_settings.ignoreWords.english) +
                  L",\"japanese\":" + JsonStringArray(g_settings.ignoreWords.japanese) + L"}}");
        } catch (...) {
            reply(L"{\"ok\":false,\"error\":" + JsLiteral(kIgnoreError) + L"}");
        }
    } else if (type == L"ignoreWords:close") {
        HideSettingsWindows();
    } else if (type == L"countdown:finished") {
        ShowMainWindow();
    } else if (type == L"app:exitInfo") {
        if (g_settings.autoTheme) {
            g_settings.theme = NextThemeId(g_settings.theme);
            SaveSettings();
            BroadcastTheme(g_settings.theme);
        }
        StartCountdown();
    } else if (type == L"tts:speak") {
        std::wstring text;
        std::wstring lang;
        TryGetStringField(message, L"text", text);
        TryGetStringField(message, L"lang", lang);
        if (lang.empty()) {
            lang = L"en";
        }
        std::wstring wav;
        std::wstring error;
        if (SynthesizeTts(text, lang, wav, error)) {
            reply(L"{\"ok\":true,\"wav\":" + JsLiteral(wav) + L"}");
        } else {
            reply(L"{\"ok\":false,\"error\":" + JsLiteral(error) + L"}");
        }
    } else {
        reply(std::wstring(), L"Unknown host message: " + type);
    }
}

void CreateWebView(HWND hwnd,
                   ComPtr<ICoreWebView2Controller>* controllerOut,
                   WebViewReady onReady) {
    HRESULT hr = CreateCoreWebView2EnvironmentWithOptions(
        nullptr, nullptr, nullptr,
        Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
            [hwnd, controllerOut, onReady](HRESULT result,
                                           ICoreWebView2Environment* environment) -> HRESULT {
                if (FAILED(result) || environment == nullptr) {
                    MessageBoxW(hwnd, L"WebView2 environment initialization failed.",
                                L"Error", MB_ICONERROR | MB_OK);
                    return result;
                }

                // Serve task assets from http://app/ so fetch() works for WASM/model files.
                ComPtr<ICoreWebView2_3> environment3;
                if (SUCCEEDED(environment->QueryInterface(IID_PPV_ARGS(&environment3))) &&
                    environment3) {
                    std::wstring folder = GetExecutableDirectory();
                    if (!folder.empty() && folder.back() == L'\\') {
                        folder.pop_back();
                    }
                    environment3->SetVirtualHostNameToFolderMapping(
                        L"app", folder.c_str(), COREWEBVIEW2_HOST_RESOURCE_ACCESS_KIND_ALLOW);
                }

                ComPtr<ICoreWebView2Environment> environmentRef = environment;
                return environment->CreateCoreWebView2Controller(
                    hwnd,
                    Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                        [hwnd, controllerOut, onReady, environmentRef](
                            HRESULT controllerResult,
                            ICoreWebView2Controller* controllerRaw) -> HRESULT {
                            if (FAILED(controllerResult) || controllerRaw == nullptr) {
                                MessageBoxW(hwnd, L"WebView2 controller initialization failed.",
                                            L"Error", MB_ICONERROR | MB_OK);
                                return controllerResult;
                            }

                            *controllerOut = controllerRaw;

                            ComPtr<ICoreWebView2Controller2> controller2;
                            if (SUCCEEDED(controllerRaw->QueryInterface(IID_PPV_ARGS(&controller2))) &&
                                controller2) {
                                COREWEBVIEW2_COLOR white{};
                                white.A = 255;
                                white.R = 255;
                                white.G = 255;
                                white.B = 255;
                                controller2->put_DefaultBackgroundColor(white);
                            }

                            UpdateWebViewBounds(hwnd, *controllerOut);

                            ComPtr<ICoreWebView2> webView;
                            if (SUCCEEDED((*controllerOut)->get_CoreWebView2(&webView)) && webView) {
                                // Allow camera capture for the workout task; deny other permissions.
                                ComPtr<ICoreWebView2_4> webView4;
                                if (SUCCEEDED(webView->QueryInterface(IID_PPV_ARGS(&webView4))) &&
                                    webView4) {
                                    webView4->add_PermissionRequested(
                                        Callback<ICoreWebView2PermissionRequestedEventHandler>(
                                            [](ICoreWebView2*,
                                               ICoreWebView2PermissionRequestedEventArgs* args)
                                                -> HRESULT {
                                                COREWEBVIEW2_PERMISSION_KIND kind{};
                                                args->get_PermissionKind(&kind);
                                                args->put_State(
                                                    kind == COREWEBVIEW2_PERMISSION_KIND_CAMERA
                                                        ? COREWEBVIEW2_PERMISSION_STATE_ALLOW
                                                        : COREWEBVIEW2_PERMISSION_STATE_DENY);
                                                return S_OK;
                                            }).Get(),
                                        nullptr);
                                }

                                // Explicit guard against paths escaping the app folder.
                                webView->AddWebResourceRequestedFilter(
                                    L"http://app/*", COREWEBVIEW2_WEB_RESOURCE_CONTEXT_ALL);
                                webView->add_WebResourceRequested(
                                    Callback<ICoreWebView2WebResourceRequestedEventHandler>(
                                        [environmentRef](
                                            ICoreWebView2*,
                                            ICoreWebView2WebResourceRequestedEventArgs* args)
                                            -> HRESULT {
                                            ComPtr<ICoreWebView2WebResourceRequest> request;
                                            if (FAILED(args->get_Request(&request)) || !request) {
                                                return S_OK;
                                            }
                                            LPWSTR uri = nullptr;
                                            request->get_Uri(&uri);
                                            if (uri != nullptr) {
                                                if (!IsAppResourceUriAllowed(uri)) {
                                                    ComPtr<ICoreWebView2Environment2> environment2;
                                                    if (SUCCEEDED(environmentRef->QueryInterface(
                                                            IID_PPV_ARGS(&environment2))) &&
                                                        environment2) {
                                                        ComPtr<ICoreWebView2WebResourceResponse> response;
                                                        environment2->CreateWebResourceResponse(
                                                            nullptr, 403, L"Forbidden", nullptr,
                                                            &response);
                                                        args->put_Response(response.Get());
                                                    }
                                                }
                                                CoTaskMemFree(uri);
                                            }
                                            return S_OK;
                                        }).Get(),
                                    nullptr);

                                // The bridge is installed before any document script runs so
                                // every view (including iframes) can call host services.
                                const std::wstring bootScript = BuildBootScript();
                                webView->AddScriptToExecuteOnDocumentCreated(
                                    bootScript.c_str(), nullptr);

                                EventRegistrationToken messageToken{};
                                webView->add_WebMessageReceived(
                                    Callback<ICoreWebView2WebMessageReceivedEventHandler>(
                                        [webView](
                                            ICoreWebView2*,
                                            ICoreWebView2WebMessageReceivedEventArgs* args)
                                            -> HRESULT {
                                            LPWSTR json = nullptr;
                                            if (SUCCEEDED(args->get_WebMessageAsJson(&json)) &&
                                                json != nullptr) {
                                                const std::wstring message(json);
                                                CoTaskMemFree(json);
                                                HandleHostMessage(webView.Get(), message);
                                            }
                                            return S_OK;
                                        }).Get(),
                                    &messageToken);

                                RegisterWebViewBinding(controllerOut->Get(), webView.Get(),
                                                       messageToken);
                                onReady(webView);
                            }
                            return S_OK;
                        }).Get());
            }).Get());

    if (FAILED(hr)) {
        MessageBoxW(hwnd,
                    L"Failed to start WebView2. Is the WebView2 runtime installed?",
                    L"Error", MB_ICONERROR | MB_OK);
    }
}

void InitializeWebView(HWND hwnd,
                       const wchar_t* htmlFile,
                       ComPtr<ICoreWebView2Controller>* controllerOut) {
    const std::wstring htmlPath = AppAssetUrl(htmlFile);
    CreateWebView(hwnd, controllerOut,
                  [htmlPath](ComPtr<ICoreWebView2>& webView) {
                      webView->Navigate(htmlPath.c_str());
                  });
}

void InitializeMainWebView(HWND hwnd) {
    CreateWebView(hwnd, &g_mainViewController,
                  [](ComPtr<ICoreWebView2>&) {
                      NavigateMainToTask(g_currentTask);
                  });
}

void OpenSettingWindow() {
    if (g_settingWindow && IsWindow(g_settingWindow)) {
        SendSettingsToSettingsWebView();
        ShowWindow(g_settingWindow, SW_SHOW);
        SetForegroundWindow(g_settingWindow);
        return;
    }

    UINT dpi = GetDpiForSystem();
    if (dpi == 0) {
        dpi = USER_DEFAULT_SCREEN_DPI;
    }

    g_settingWindow = CreateWindowExW(
        0,
        kSettingClass,
        kSettingTitle,
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        CW_USEDEFAULT, CW_USEDEFAULT,
        ScaleForDpi(kSettingWindowWidth, dpi),
        ScaleForDpi(kSettingWindowHeight, dpi),
        g_mainWindow,
        nullptr,
        g_hInstance,
        nullptr);

    if (g_settingWindow) {
        ShowWindow(g_settingWindow, SW_SHOW);
        UpdateWindow(g_settingWindow);
    }
}

void OpenIgnoreWordsWindow() {
    if (g_ignoreWordsWindow && IsWindow(g_ignoreWordsWindow)) {
        ShowWindow(g_ignoreWordsWindow, SW_SHOW);
        SetForegroundWindow(g_ignoreWordsWindow);
        return;
    }

    UINT dpi = GetDpiForSystem();
    if (dpi == 0) {
        dpi = USER_DEFAULT_SCREEN_DPI;
    }

    g_ignoreWordsWindow = CreateWindowExW(
        0,
        kIgnoreClass,
        kIgnoreTitle,
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        CW_USEDEFAULT, CW_USEDEFAULT,
        ScaleForDpi(kIgnoreWindowWidth, dpi),
        ScaleForDpi(kIgnoreWindowHeight, dpi),
        g_settingWindow,
        nullptr,
        g_hInstance,
        nullptr);

    if (g_ignoreWordsWindow) {
        ShowWindow(g_ignoreWordsWindow, SW_SHOW);
        UpdateWindow(g_ignoreWordsWindow);
    }
}

void ShowTrayMenu(HWND hwnd) {
    POINT cursor{};
    GetCursorPos(&cursor);

    // Required so the menu closes correctly when focus moves elsewhere.
    SetForegroundWindow(hwnd);

    HMENU menu = CreatePopupMenu();
    if (!menu) {
        return;
    }

    AppendMenuW(menu, MF_STRING, ID_TRAY_OPEN, L"Show App");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, ID_TRAY_SETTING, L"Settings");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, ID_TRAY_EXIT, L"Quit");

    UINT command = TrackPopupMenu(
        menu,
        TPM_RETURNCMD | TPM_RIGHTBUTTON | TPM_LEFTALIGN,
        cursor.x, cursor.y,
        0,
        hwnd,
        nullptr);

    DestroyMenu(menu);
    PostMessageW(hwnd, WM_NULL, 0, 0);

    switch (command) {
    case ID_TRAY_OPEN:
        ShowMainWindow();
        break;
    case ID_TRAY_SETTING:
        OpenSettingWindow();
        break;
    case ID_TRAY_EXIT:
        // Distinguish an intentional quit from closing the task window, which
        // only returns to the countdown.
        QuitApp();
        break;
    default:
        break;
    }
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE:
        g_mainWindow = hwnd;

        g_trayIcon.cbSize = sizeof(g_trayIcon);
        g_trayIcon.hWnd = hwnd;
        g_trayIcon.uID = kTrayIconId;
        g_trayIcon.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
        g_trayIcon.uCallbackMessage = kTrayMessage;
        g_trayIcon.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
        wcscpy_s(g_trayIcon.szTip, kTrayTip);

        if (!Shell_NotifyIconW(NIM_ADD, &g_trayIcon)) {
            MessageBoxW(hwnd, L"Failed to add the tray icon.", L"Error", MB_ICONERROR | MB_OK);
        }
        return 0;

    case kTrayMessage:
        switch (static_cast<UINT>(lParam)) {
        case WM_RBUTTONUP:
            ShowTrayMenu(hwnd);
            return 0;
        case WM_LBUTTONUP:
            ToggleMainWindow();
            return 0;
        default:
            return 0;
        }

    case WM_SIZE:
        // Minimize goes to the tray; use the tray icon or menu to bring it back.
        if (wParam == SIZE_MINIMIZED) {
            ShowWindow(hwnd, SW_HIDE);
            return 0;
        }
        UpdateWebViewBounds(hwnd, g_mainViewController);
        break;

    case WM_DPICHANGED:
        HandleDpiChanged(hwnd, lParam);
        return 0;

    case WM_TIMER:
        if (wParam == kChromeFreezeTimerId) {
            FreezeChromeProcesses();
        }
        return 0;

    case WM_CLOSE:
        if (g_isQuitting) {
            QuitApp();
        } else {
            // Closing MainWindow returns to a fresh 60s countdown instead of
            // ending the process; the tray icon keeps the app alive.
            StartCountdown();
        }
        return 0;

    case WM_DESTROY:
        if (g_ignoreWordsWindow && IsWindow(g_ignoreWordsWindow)) {
            DestroyWindow(g_ignoreWordsWindow);
        }
        if (g_settingWindow && IsWindow(g_settingWindow)) {
            DestroyWindow(g_settingWindow);
        }
        if (g_countDownWindow && IsWindow(g_countDownWindow)) {
            DestroyWindow(g_countDownWindow);
        }
        Shell_NotifyIconW(NIM_DELETE, &g_trayIcon);
        UnregisterWebViewBinding(g_mainViewController.Get());
        g_mainViewController.Reset();
        UnfreezeChromeOnExit();
        g_mainWindow = nullptr;
        g_countDownWindow = nullptr;
        g_ignoreWordsWindow = nullptr;
        PostQuitMessage(0);
        return 0;

    default:
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }

    return DefWindowProcW(hwnd, message, wParam, lParam);
}

LRESULT CALLBACK CountDownWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_DPICHANGED:
        HandleDpiChanged(hwnd, lParam);
        return 0;

    case WM_CREATE:
        g_countdownSeconds = std::clamp(g_settings.countdownSeconds, 1, 3600);
        SetTimer(hwnd, kCountDownTimerId, 1000, nullptr);
        return 0;

    case WM_PAINT: {
        std::wstring text = std::to_wstring(g_countdownSeconds);
        PaintCenteredText(hwnd, text.c_str(), kCountDownFontHeight);
        return 0;
    }

    case WM_TIMER:
        if (wParam == kCountDownTimerId && g_countdownSeconds > 0) {
            --g_countdownSeconds;
            if (g_countdownSeconds == 0) {
                // Countdown finished: switch back to MainWindow.
                ShowMainWindow();
            } else {
                InvalidateRect(hwnd, nullptr, TRUE);
            }
        }
        return 0;

    case WM_SIZE:
        if (wParam == SIZE_MINIMIZED) {
            ShowWindow(hwnd, SW_HIDE);
            return 0;
        }
        break;

    case WM_DESTROY:
        KillTimer(hwnd, kCountDownTimerId);
        g_countDownWindow = nullptr;
        return 0;

    default:
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }

    return DefWindowProcW(hwnd, message, wParam, lParam);
}

LRESULT CALLBACK SettingWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE:
        InitializeWebView(hwnd, L"settings.html", &g_webViewController);
        return 0;

    case WM_DPICHANGED:
        HandleDpiChanged(hwnd, lParam);
        return 0;

    case WM_SIZE:
        UpdateWebViewBounds(hwnd, g_webViewController);
        break;

    case WM_CLOSE:
        // Closing without confirming reverts a live theme preview.
        BroadcastTheme(g_settings.theme);
        HideSettingsWindows();
        return 0;

    case WM_DESTROY:
        UnregisterWebViewBinding(g_webViewController.Get());
        g_webViewController.Reset();
        g_settingWindow = nullptr;
        return 0;

    default:
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }

    return DefWindowProcW(hwnd, message, wParam, lParam);
}

LRESULT CALLBACK IgnoreWordsWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE:
        InitializeWebView(hwnd, L"ignoreWords.html", &g_ignoreWordsController);
        return 0;

    case WM_DPICHANGED:
        HandleDpiChanged(hwnd, lParam);
        return 0;

    case WM_SIZE:
        UpdateWebViewBounds(hwnd, g_ignoreWordsController);
        break;

    case WM_CLOSE:
        ShowWindow(hwnd, SW_HIDE);
        return 0;

    case WM_DESTROY:
        UnregisterWebViewBinding(g_ignoreWordsController.Get());
        g_ignoreWordsController.Reset();
        g_ignoreWordsWindow = nullptr;
        return 0;

    default:
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }

    return DefWindowProcW(hwnd, message, wParam, lParam);
}

}  // namespace

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow) {
    // Only one copy may own the frozen Chrome processes. A duplicate launch
    // releases them and exits without creating any windows.
    if (!AcquireSingleInstanceLock()) {
        ResumeHeldChromeProcesses();
        return 0;
    }

    g_hInstance = hInstance;

    // Settings and the database are ready before any window or WebView exists.
    LoadSettings();
    InitializeDatabase();
    SyncAutoStartOnStartup();

    // WebView2 requires a single-threaded COM apartment on the UI thread.
    const bool comInitialized = SUCCEEDED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED));

    // Avoid Windows DPI virtualization, which makes GDI text look blurry at 125%/150%.
    if (!SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)) {
        SetProcessDPIAware();  // Fallback for older Windows versions.
    }

    // Crash recovery: processes suspended by an earlier instance must be alive
    // again before this instance starts its own freeze cycle.
    ResumeAllChromeProcesses();

    WNDCLASSW windowClass{};
    windowClass.style = CS_HREDRAW | CS_VREDRAW;
    windowClass.lpfnWndProc = WndProc;
    windowClass.hInstance = hInstance;
    windowClass.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    windowClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
    windowClass.hbrBackground = reinterpret_cast<HBRUSH>(GetStockObject(WHITE_BRUSH));
    windowClass.lpszClassName = kWindowClass;

    if (!RegisterClassW(&windowClass)) {
        MessageBoxW(nullptr, L"RegisterClassW failed.", L"Error", MB_ICONERROR | MB_OK);
        return 1;
    }

    WNDCLASSW countDownClass{};
    countDownClass.style = CS_HREDRAW | CS_VREDRAW;
    countDownClass.lpfnWndProc = CountDownWndProc;
    countDownClass.hInstance = hInstance;
    countDownClass.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    countDownClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
    countDownClass.hbrBackground = reinterpret_cast<HBRUSH>(GetStockObject(WHITE_BRUSH));
    countDownClass.lpszClassName = kCountDownClass;

    if (!RegisterClassW(&countDownClass)) {
        MessageBoxW(nullptr, L"RegisterClassW for CountDownWindow failed.", L"Error", MB_ICONERROR | MB_OK);
        return 1;
    }

    WNDCLASSW settingClass{};
    settingClass.style = CS_HREDRAW | CS_VREDRAW;
    settingClass.lpfnWndProc = SettingWndProc;
    settingClass.hInstance = hInstance;
    settingClass.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    settingClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
    settingClass.hbrBackground = reinterpret_cast<HBRUSH>(GetStockObject(WHITE_BRUSH));
    settingClass.lpszClassName = kSettingClass;

    if (!RegisterClassW(&settingClass)) {
        MessageBoxW(nullptr, L"RegisterClassW for SettingWindows failed.", L"Error", MB_ICONERROR | MB_OK);
        return 1;
    }

    WNDCLASSW ignoreClass{};
    ignoreClass.style = CS_HREDRAW | CS_VREDRAW;
    ignoreClass.lpfnWndProc = IgnoreWordsWndProc;
    ignoreClass.hInstance = hInstance;
    ignoreClass.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    ignoreClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
    ignoreClass.hbrBackground = reinterpret_cast<HBRUSH>(GetStockObject(WHITE_BRUSH));
    ignoreClass.lpszClassName = kIgnoreClass;

    if (!RegisterClassW(&ignoreClass)) {
        MessageBoxW(nullptr, L"RegisterClassW for IgnoreWordsWindows failed.", L"Error", MB_ICONERROR | MB_OK);
        return 1;
    }

    // Keep the logical 1280x720 size at any DPI, with crisp physical-pixel rendering.
    UINT systemDpi = GetDpiForSystem();
    if (systemDpi == 0) {
        systemDpi = USER_DEFAULT_SCREEN_DPI;
    }

    RECT windowRect{0, 0, ScaleForDpi(kClientWidth, systemDpi),
                    ScaleForDpi(kClientHeight, systemDpi)};
    AdjustWindowRectExForDpi(&windowRect, WS_OVERLAPPEDWINDOW, FALSE, 0, systemDpi);
    const int windowWidth = windowRect.right - windowRect.left;
    const int windowHeight = windowRect.bottom - windowRect.top;

    HWND hwnd = CreateWindowExW(
        0,
        kWindowClass,
        kWindowTitle,
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        CW_USEDEFAULT, CW_USEDEFAULT,
        windowWidth, windowHeight,
        nullptr, nullptr,
        hInstance, nullptr);

    if (!hwnd) {
        MessageBoxW(nullptr, L"CreateWindowExW failed.", L"Error", MB_ICONERROR | MB_OK);
        return 1;
    }

    // Frameless logical 100x50 countdown in the bottom-right of the work area.
    const int countDownWidth = ScaleForDpi(kCountDownWidth, systemDpi);
    const int countDownHeight = ScaleForDpi(kCountDownHeight, systemDpi);

    MONITORINFO monitorInfo{};
    monitorInfo.cbSize = sizeof(monitorInfo);
    if (!GetMonitorInfoW(MonitorFromWindow(nullptr, MONITOR_DEFAULTTOPRIMARY), &monitorInfo)) {
        SetRect(&monitorInfo.rcWork, 0, 0,
                GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN));
    }

    const int countDownMargin = ScaleForDpi(kCountDownMargin, systemDpi);
    g_countDownWindow = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED,
        kCountDownClass,
        kCountDownTitle,
        WS_POPUP,
        monitorInfo.rcWork.right - countDownWidth - countDownMargin,
        monitorInfo.rcWork.bottom - countDownHeight - countDownMargin,
        countDownWidth, countDownHeight,
        nullptr, nullptr,
        hInstance, nullptr);

    if (!g_countDownWindow) {
        MessageBoxW(nullptr, L"CreateWindowExW for CountDownWindow failed.", L"Error", MB_ICONERROR | MB_OK);
    } else {
        SetLayeredWindowAttributes(g_countDownWindow, 0, kCountDownOpacity, LWA_ALPHA);
    }

    SyncChromeFreeze();

    // First launch: only the countdown is visible.
    (void)nCmdShow;
    ShowWindow(hwnd, SW_HIDE);

    if (g_countDownWindow) {
        ShowWindow(g_countDownWindow, SW_SHOW);
        UpdateWindow(g_countDownWindow);
    }

    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    UnfreezeChromeOnExit();

    if (comInitialized) {
        CoUninitialize();
    }
    return static_cast<int>(message.wParam);
}
