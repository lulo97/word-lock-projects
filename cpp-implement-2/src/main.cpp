#include <windows.h>
#include <shellapi.h>
#include <objbase.h>
#include <wrl.h>
#include <wrl/client.h>
#include <WebView2.h>

#include <cwchar>
#include <functional>
#include <string>

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
constexpr wchar_t kTrayTip[] = L"MainWindow";

constexpr int kCountDownWidth = 100;
constexpr int kCountDownHeight = 50;
constexpr int kCountDownFontHeight = 28;
constexpr int kCountDownStartSeconds = 60;
constexpr UINT_PTR kCountDownTimerId = 1;
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
    bool dumbbellWorkout = false;
};

struct AppSettings {
    TaskSettings tasks;
};

AppSettings g_settings;
int g_countdownSeconds = kCountDownStartSeconds;
HINSTANCE g_hInstance = nullptr;
HWND g_mainWindow = nullptr;
HWND g_countDownWindow = nullptr;
HWND g_settingWindow = nullptr;
HWND g_ignoreWordsWindow = nullptr;
bool g_mainWebViewInitialized = false;
NOTIFYICONDATAW g_trayIcon{};
ComPtr<ICoreWebView2Controller> g_mainViewController;
ComPtr<ICoreWebView2Controller> g_webViewController;
ComPtr<ICoreWebView2Controller> g_ignoreWordsController;
EventRegistrationToken g_settingsMessageToken{};

LRESULT CALLBACK CountDownWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK SettingWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK IgnoreWordsWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
void OpenIgnoreWordsWindow();
void HideSettingsWindows();
void InitializeMainWebView(HWND hwnd);
void UpdateMainContent();

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
    g_countdownSeconds = kCountDownStartSeconds;

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

void ShowMainWindow() {
    StopCountdown();
    if (g_mainWindow && IsWindow(g_mainWindow)) {
        ShowWindow(g_mainWindow, SW_RESTORE);
        SetForegroundWindow(g_mainWindow);

        // WebView2 renders reliably only after its parent window has been shown.
        if (!g_mainWebViewInitialized) {
            g_mainWebViewInitialized = true;
            InitializeMainWebView(g_mainWindow);
        } else {
            UpdateMainContent();
        }
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

const wchar_t* BoolLiteral(bool value) {
    return value ? L"true" : L"false";
}

std::wstring BuildSettingsJson() {
    std::wstring json = L"{\"type\":\"applySettings\",\"tasks\":{";
    json += L"\"englishWord\":";
    json += BoolLiteral(g_settings.tasks.englishWord);
    json += L",\"multipleChoice\":";
    json += BoolLiteral(g_settings.tasks.multipleChoice);
    json += L",\"japaneseWord\":";
    json += BoolLiteral(g_settings.tasks.japaneseWord);
    json += L",\"dumbbellWorkout\":";
    json += BoolLiteral(g_settings.tasks.dumbbellWorkout);
    json += L"}}";
    return json;
}

void SendSettingsToSettingsWebView() {
    if (!g_webViewController) {
        return;
    }
    ComPtr<ICoreWebView2> webView;
    if (SUCCEEDED(g_webViewController->get_CoreWebView2(&webView)) && webView) {
        const std::wstring json = BuildSettingsJson();
        webView->PostWebMessageAsJson(json.c_str());
    }
}

std::wstring BuildMainHtml() {
    std::wstring body;
    if (g_settings.tasks.englishWord) {
        body += L"<div>English</div>";
    }
    if (g_settings.tasks.japaneseWord) {
        body += L"<div>japanese</div>";
    }
    if (g_settings.tasks.multipleChoice) {
        body += L"<div>mcq</div>";
    }
    if (g_settings.tasks.dumbbellWorkout) {
        body += L"<div>dumbbell</div>";
    }

    return LR"(<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<style>
  html, body { height: 100%; }
  body {
    margin: 0;
    background: #fff;
    color: #000;
    font-family: "Segoe UI", system-ui, sans-serif;
    font-size: 48px;
    display: flex;
    flex-direction: column;
    align-items: center;
    justify-content: center;
    gap: 18px;
  }
</style>
</head>
<body>)" + body + LR"(</body>
</html>)";
}

void UpdateMainContent() {
    if (!g_mainViewController) {
        return;
    }
    ComPtr<ICoreWebView2> webView;
    if (SUCCEEDED(g_mainViewController->get_CoreWebView2(&webView)) && webView) {
        const std::wstring html = BuildMainHtml();
        webView->NavigateToString(html.c_str());
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

                return environment->CreateCoreWebView2Controller(
                    hwnd,
                    Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                        [hwnd, controllerOut, onReady](
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

void HandleSettingsWebMessage(const std::wstring& message) {
    if (message.find(L"openIgnoreWords") != std::wstring::npos) {
        OpenIgnoreWordsWindow();
        return;
    }

    if (message.find(L"confirmSettings") != std::wstring::npos) {
        g_settings.tasks.englishWord =
            message.find(L"\"englishWord\":true") != std::wstring::npos;
        g_settings.tasks.multipleChoice =
            message.find(L"\"multipleChoice\":true") != std::wstring::npos;
        g_settings.tasks.japaneseWord =
            message.find(L"\"japaneseWord\":true") != std::wstring::npos;
        g_settings.tasks.dumbbellWorkout =
            message.find(L"\"dumbbellWorkout\":true") != std::wstring::npos;

        HideSettingsWindows();
        ShowMainWindow();
        return;
    }

    if (message.find(L"closeSettings") != std::wstring::npos) {
        HideSettingsWindows();
    }
}

void InitializeWebView(HWND hwnd,
                       const wchar_t* htmlFile,
                       ComPtr<ICoreWebView2Controller>* controllerOut,
                       bool registerSettingsMessages) {
    wchar_t exePath[MAX_PATH]{};
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);

    std::wstring htmlPath = exePath;
    const size_t slash = htmlPath.find_last_of(L"\\/");
    if (slash != std::wstring::npos) {
        htmlPath.resize(slash + 1);
    }
    htmlPath += htmlFile;

    CreateWebView(hwnd, controllerOut,
                  [htmlPath, registerSettingsMessages](ComPtr<ICoreWebView2>& webView) {
                      if (registerSettingsMessages) {
                          webView->add_WebMessageReceived(
                              Callback<ICoreWebView2WebMessageReceivedEventHandler>(
                                  [](ICoreWebView2*,
                                     ICoreWebView2WebMessageReceivedEventArgs* args) -> HRESULT {
                                      LPWSTR json = nullptr;
                                      if (SUCCEEDED(args->get_WebMessageAsJson(&json)) && json) {
                                          std::wstring message(json);
                                          CoTaskMemFree(json);
                                          HandleSettingsWebMessage(message);
                                      }
                                      return S_OK;
                                  }).Get(),
                              &g_settingsMessageToken);

                          SendSettingsToSettingsWebView();
                      }
                      webView->Navigate(htmlPath.c_str());
                  });
}

void InitializeMainWebView(HWND hwnd) {
    CreateWebView(hwnd, &g_mainViewController,
                  [](ComPtr<ICoreWebView2>&) {
                      UpdateMainContent();
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

    AppendMenuW(menu, MF_STRING, ID_TRAY_OPEN, L"Open App");
    AppendMenuW(menu, MF_STRING, ID_TRAY_SETTING, L"Setting");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, ID_TRAY_EXIT, L"Exit");

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
        DestroyWindow(hwnd);
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
        case WM_LBUTTONDBLCLK:
            ShowMainWindow();
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

    case WM_CLOSE:
        // Closing MainWindow returns to a fresh 60s countdown.
        StartCountdown();
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
        g_mainViewController.Reset();
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
        g_countdownSeconds = kCountDownStartSeconds;
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
        InitializeWebView(hwnd, L"settings.html", &g_webViewController, true);
        return 0;

    case WM_DPICHANGED:
        HandleDpiChanged(hwnd, lParam);
        return 0;

    case WM_SIZE:
        UpdateWebViewBounds(hwnd, g_webViewController);
        break;

    case WM_CLOSE:
        // Only the tray menu can show it again.
        HideSettingsWindows();
        return 0;

    case WM_DESTROY:
        if (g_webViewController) {
            ComPtr<ICoreWebView2> webView;
            if (SUCCEEDED(g_webViewController->get_CoreWebView2(&webView)) && webView) {
                webView->remove_WebMessageReceived(g_settingsMessageToken);
            }
        }
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
        InitializeWebView(hwnd, L"ignoreWords.html", &g_ignoreWordsController, false);
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
    g_hInstance = hInstance;

    // WebView2 requires a single-threaded COM apartment on the UI thread.
    const bool comInitialized = SUCCEEDED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED));

    // Avoid Windows DPI virtualization, which makes GDI text look blurry at 125%/150%.
    if (!SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)) {
        SetProcessDPIAware();  // Fallback for older Windows versions.
    }

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

    g_countDownWindow = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        kCountDownClass,
        kCountDownTitle,
        WS_POPUP,
        monitorInfo.rcWork.right - countDownWidth,
        monitorInfo.rcWork.bottom - countDownHeight,
        countDownWidth, countDownHeight,
        nullptr, nullptr,
        hInstance, nullptr);

    if (!g_countDownWindow) {
        MessageBoxW(nullptr, L"CreateWindowExW for CountDownWindow failed.", L"Error", MB_ICONERROR | MB_OK);
    }

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

    if (comInitialized) {
        CoUninitialize();
    }
    return static_cast<int>(message.wParam);
}
