#pragma once

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

namespace wordlock {

using Microsoft::WRL::Callback;
using Microsoft::WRL::ComPtr;

constexpr int IDI_APP_ICON = 1;
HICON LoadAppIcon(int width = 0, int height = 0);

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
constexpr wchar_t kTrayTip[] = L"PomodoroLockCpp";

constexpr wchar_t kSingleInstanceMutexName[] = L"Local\\PomodoroLockCppSingleInstance";
constexpr wchar_t kChromeProcessName[] = L"chrome.exe";
constexpr wchar_t kChromeFreezeMockEnv[] = L"WORDLOCK_CHROME_FREEZE_MOCK";
constexpr wchar_t kTestChromeFreezeEnv[] = L"WORDLOCK_TEST_CHROME_FREEZE";
constexpr wchar_t kTestCountdownSecondsEnv[] = L"WORDLOCK_TEST_COUNTDOWN_SECONDS";
constexpr wchar_t kTestAutoThemeEnv[] = L"WORDLOCK_TEST_AUTO_THEME";
constexpr wchar_t kUseTestDataEnv[] = L"WORDLOCK_USE_TEST_DATA";
constexpr wchar_t kTestModeEnv[] = L"WORDLOCK_TEST_MODE";
constexpr wchar_t kTestHomeEnv[] = L"WORDLOCK_TEST_HOME";
constexpr wchar_t kTestDomDumpFileEnv[] = L"WORDLOCK_TEST_DOM_DUMP_FILE";
constexpr wchar_t kDumbbellMockEnv[] = L"WORDLOCK_DUMBBELL_MOCK";
constexpr wchar_t kTestOpenSettingsArg[] = L"--test-open-settings";
constexpr wchar_t kTestOpenDumbbellArg[] = L"--test-open-dumbbell";
constexpr wchar_t kTestOpenEnglishArg[] = L"--test-open-english";
constexpr wchar_t kTestOpenJapaneseArg[] = L"--test-open-japanese";
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
constexpr UINT kThemeChangedMessage = WM_APP + 3;
constexpr int kSettingWindowWidth = 780;
constexpr int kSettingWindowHeight = 780;
constexpr int kIgnoreWindowWidth = 560;
constexpr int kIgnoreWindowHeight = 620;

constexpr UINT kTrayMessage = WM_APP + 1;
constexpr UINT kTestQuitMessage = WM_APP + 2;
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

extern AppSettings g_settings;
extern sqlite3* g_db;
extern int g_nextTaskIndex;
extern TaskType g_currentTask;
extern int g_countdownSeconds;
extern POINT g_countDownPosition;
extern bool g_countDownPositionValid;

struct ThemePalette {
    COLORREF background;
    COLORREF text;
    const wchar_t* font;
    bool bold;
};

extern std::wstring g_countDownTheme;

struct WebViewBinding {
    ComPtr<ICoreWebView2Controller> controller;
    EventRegistrationToken messageToken{};
    std::wstring bootScriptId;
    bool bootScriptReady = false;
};

extern std::vector<WebViewBinding> g_webViewBindings;
extern std::unordered_map<DWORD, FILETIME> g_heldChromeProcesses;
extern bool g_chromeFreezeShutdownDone;

// Held for the lifetime of the winning instance; OS releases it on process exit.
extern HANDLE g_singleInstanceMutex;
extern HINSTANCE g_hInstance;
extern HWND g_mainWindow;
extern HWND g_countDownWindow;
extern HWND g_settingWindow;
extern HWND g_ignoreWordsWindow;
extern bool g_mainWebViewInitialized;
extern bool g_isQuitting;
extern NOTIFYICONDATAW g_trayIcon;
extern ComPtr<ICoreWebView2Controller> g_mainViewController;
extern ComPtr<ICoreWebView2Controller> g_webViewController;
extern ComPtr<ICoreWebView2Controller> g_ignoreWordsController;

LRESULT CALLBACK CountDownWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

constexpr wchar_t kTestCommandFileEnv[] = L"WORDLOCK_TEST_COMMAND_FILE";
constexpr wchar_t kTestMessageLogFileEnv[] = L"WORDLOCK_TEST_MESSAGE_LOG_FILE";
constexpr wchar_t kAppUriPrefix[] = L"https://app/";

using WebViewReady = std::function<void(ComPtr<ICoreWebView2>&)>;
using NtProcessFn = LONG(NTAPI*)(HANDLE);
struct ReplyFn {
    std::function<void(const std::wstring&, const std::wstring&)> fn;
    void operator()(const std::wstring& result,
                    const std::wstring& error = std::wstring()) const {
        fn(result, error);
    }
};

struct WindowClasses {
    std::wstring main;
    std::wstring countDown;
    std::wstring setting;
    std::wstring ignore;
};

std::wstring GetExecutableDirectory();
std::wstring AppAssetUrl(const wchar_t* file);
bool IsTestDataMode();
std::wstring GetDataDirectory();
int HexDigit(wchar_t ch);
std::wstring UrlDecode(const std::wstring& value);
bool IsAppResourceUri(const std::wstring& uri);
std::wstring AppRelativePathFromUri(const std::wstring& uri);
std::filesystem::path ResolveAppResourcePath(const std::wstring& relative);
bool IsAppResourceUriAllowed(const std::wstring& uri);
void ShowMainWindow();
void BringWindowToForeground(HWND hwnd);
void NotifyActiveViewHide();
void ToggleMainWindow();
void QuitApp();
void HideSettingsWindows();
void UpdateWebViewBounds(HWND hwnd, ComPtr<ICoreWebView2Controller>& controller);
std::wstring GetLaunchExecutablePath();
bool IsAutoStartDisabledByWindows(const std::wstring& valueName);
void RemoveLegacyAutoStartEntry(HKEY key, const std::wstring& launchPath);
void ApplyAutoStartPreference(bool enabled);
void SyncAutoStartOnStartup();
std::wstring Base64Encode(const std::vector<unsigned char>& bytes);
uint32_t ReadLe32(const std::vector<unsigned char>& bytes, size_t offset);
uint16_t ReadLe16(const std::vector<unsigned char>& bytes, size_t offset);
std::wstring BuildBootScript(const std::wstring& theme);
void ResumeAllChromeProcesses();
void FreezeChromeProcesses();
NtProcessFn GetNtProcessFunction(const char* name);
bool SameStartTime(const FILETIME& left, const FILETIME& right);
bool GetProcessStartTime(DWORD processId, FILETIME& startTime);
bool IsChromeProcessId(DWORD processId);
bool ChangeChromeProcessState(DWORD processId, bool suspend);
void UnfreezeHeldChromeProcesses();
void UnfreezeChromeOnExit();
void SyncChromeFreeze();
void StopCountdown();
void StartCountdown();
void AdvanceTheme();
void FinishCurrentTask();
void RememberCountDownPosition(HWND hwnd);
void ApplyCountDownPosition(HWND hwnd);
TaskType GetNextTaskType();
const wchar_t* TaskAsset(TaskType task);
void NavigateMainToTask(TaskType task);
LRESULT CALLBACK CountDownWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
bool ExecuteSql(const char* sql);
bool EnsureColumn(const char* table, const char* column, const char* definition);
void InitializeDatabase();
void SeedTestData();
void RecordWordShow(const std::string& language, int wordId);
std::wstring GetWordProgressJson();
std::wstring GetDatabaseInfoJson();
int ScaleForDpi(int value, UINT dpi);
void HandleDpiChanged(HWND hwnd, LPARAM lParam);
bool EnvFlagEnabled(const wchar_t* name);
bool IsTestModeEnabled();
std::wstring GetEnvironmentValue(const wchar_t* name);
std::wstring WindowClassForCurrentProcess(const wchar_t* base);
bool IsChromeFreezeMockEnabled();
bool AcquireSingleInstanceLock();
void ResumeHeldChromeProcesses();
void HandleHostMessage(ICoreWebView2* sender, const std::wstring& message);
void HandleAppMessage(const std::wstring& type, const std::wstring& message,
                      const ReplyFn& reply);
bool HandleContentMessage(const std::wstring& type, const std::wstring& message,
                          const ReplyFn& reply, int number);
bool HandleIgnoreWordsMessage(const std::wstring& type, const std::wstring& message,
                              const ReplyFn& reply);
void OpenIgnoreWordsWindow();
LRESULT CALLBACK IgnoreWordsWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
bool IsJapaneseScriptWord(const std::wstring& word);
std::vector<std::wstring> ParseIgnoreWords(const std::wstring& value, bool japanese);
bool TryGetStringArrayField(const std::wstring& json,
                            const std::wstring& key,
                            std::vector<std::wstring>& out);
bool TryGetStringField(const std::wstring& json, const std::wstring& key, std::wstring& out);
bool TryGetNumberField(const std::wstring& json, const std::wstring& key, double& out);
bool TryGetBoolField(const std::wstring& json,
                     const std::wstring& key,
                     bool defaultValue = false);
std::string BuildMainHtml();
void UpdateMainContent();
LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
const std::vector<std::wstring>& ThemeIds();
int NormalizeCountdownSeconds(double value);
double NormalizeVolume(double value);
std::wstring NormalizeTheme(const std::wstring& value);
std::wstring NextThemeId(const std::wstring& current);
std::wstring Trim(const std::wstring& value);
void PaintCenteredText(HWND hwnd, const wchar_t* text, int fontHeight,
                       const ThemePalette& palette);
ThemePalette GetThemePalette(const std::wstring& theme);
std::wstring QueryRowJson(const std::string& sql,
                          const std::vector<std::string>& parameters,
                          const std::vector<std::string>& rawJsonColumns);
std::wstring LowerWide(const std::wstring& value);
std::vector<std::string> LowerUtf8List(const std::vector<std::wstring>& values);
int RunMessageLoop();
int Run(HINSTANCE hInstance, int nCmdShow);
bool WantsTestSettingsOpen();
bool WantsTestDumbbellOpen();
bool WantsTestEnglishOpen();
bool WantsTestJapaneseOpen();
UINT SystemDpi();
HWND CreateMainWindow(HINSTANCE hInstance, const wchar_t* className, UINT dpi);
void CreateCountDownWindow(HINSTANCE hInstance, const wchar_t* className, UINT dpi);
bool RegisterWindowClass(HINSTANCE hInstance, const wchar_t* className,
                         WNDPROC procedure, const wchar_t* errorText);
bool RegisterAllWindowClasses(HINSTANCE hInstance, const WindowClasses& classes);
void ApplySettingsFromJson(const std::wstring& values, bool confirm);
void SendSettingsToSettingsWebView();
std::wstring BuildSettingsDataJson();
std::wstring BuildThemeListJson();
void LoadSettings();
std::wstring JsonStringArray(const std::vector<std::wstring>& values);
void SaveSettings();
void OpenSettingWindow();
LRESULT CALLBACK SettingWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
bool IsTestDomDumpEnabled();
void WriteTestDomDump(const std::wstring& contents);
void AppendTestMessageLog(const std::wstring& type);
void ExecuteTestCommand(ICoreWebView2* webView);
std::wstring Utf8ToWide(const std::string& text);
std::string WideToUtf8(const std::wstring& text);
std::wstring JsonEscape(const std::wstring& value);
std::wstring JsLiteral(const std::wstring& value);
const wchar_t* BoolLiteral(bool value);
void ShowTrayMenu(HWND hwnd);
bool SynthesizeTts(const std::wstring& text,
                   const std::wstring& lang,
                   std::wstring& wavBase64,
                   std::wstring& error);
std::vector<unsigned char> ApplyWavGain(std::vector<unsigned char> wav, double gain);
std::wstring ContentTypeForFile(const std::filesystem::path& path);
ComPtr<ICoreWebView2WebResourceResponse> CreateFileWebResourceResponse(
    ICoreWebView2Environment2* environment,
    const std::filesystem::path& file);
void RegisterWebViewBinding(ICoreWebView2Controller* controller,
                            ICoreWebView2* webView,
                            EventRegistrationToken token);
void UnregisterWebViewBinding(ICoreWebView2Controller* controller);
void BroadcastTheme(const std::wstring& theme);
void RememberBootScript(ICoreWebView2Controller* controller, LPCWSTR id);
void AddBootScript(ICoreWebView2* webView, ICoreWebView2Controller* controller,
                   const std::wstring& theme, WebViewReady onReady);
void RefreshBootScript(ICoreWebView2* webView, ICoreWebView2Controller* controller,
                       const std::wstring& theme);
void SendHostReply(ICoreWebView2* sender,
                   const std::wstring& requestId,
                   const std::wstring& resultJson,
                   const std::wstring& error);
void ConfigureWebViewPermissions(ICoreWebView2* webView);
void ConfigureControllerDefaults(ICoreWebView2Controller* controller);
void ConfigureWebViewResources(ICoreWebView2* webView,
                               ComPtr<ICoreWebView2Environment> environmentRef,
                               ComPtr<ICoreWebView2Environment2> environment2,
                               std::wstring assetFolder, bool hasVirtualHostMapping);
void InstallWebViewBridge(ICoreWebView2* webView, ICoreWebView2Controller* controller,
                          WebViewReady onReady);
void CreateWebView(HWND hwnd, ComPtr<ICoreWebView2Controller>* controllerOut,
                   WebViewReady onReady);
void InstallTestDomDump(ICoreWebView2* webView, const wchar_t* htmlFile);
void InstallTaskInputFocus(ICoreWebView2* webView, ICoreWebView2Controller* controller);
void InitializeWebView(HWND hwnd,
                       const wchar_t* htmlFile,
                       ComPtr<ICoreWebView2Controller>* controllerOut);
void InitializeMainWebView(HWND hwnd);
HRESULT HandleAppWebResourceRequest(
    ComPtr<ICoreWebView2Environment2> environment2,
    ComPtr<ICoreWebView2Environment> environmentRef,
    std::wstring assetFolder, bool hasVirtualHostMapping,
    ICoreWebView2WebResourceRequestedEventArgs* args);
std::wstring GetRandomEnglishWordJson();
std::wstring GetEnglishWordJson(int id);
std::wstring GetRandomMcqJson();
std::wstring GetMcqJson(int id);
std::wstring GetRandomJapaneseWordJson();
std::wstring GetJapaneseWordJson(int id);

}  // namespace wordlock
