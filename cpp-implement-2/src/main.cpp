#include <windows.h>
#include <shellapi.h>
#include <objbase.h>
#include <wrl.h>
#include <wrl/client.h>
#include <WebView2.h>

#include <cwchar>
#include <string>

namespace {

using Microsoft::WRL::Callback;
using Microsoft::WRL::ComPtr;

constexpr int kClientWidth = 1280;
constexpr int kClientHeight = 720;
constexpr wchar_t kWindowClass[] = L"MainWindow";
constexpr wchar_t kCountDownClass[] = L"CountDownWindow";
constexpr wchar_t kSettingClass[] = L"SettingWindows";
constexpr wchar_t kWindowTitle[] = L"MainWindow";
constexpr wchar_t kCountDownTitle[] = L"CountDownWindow";
constexpr wchar_t kSettingTitle[] = L"SettingWindows";
constexpr wchar_t kTrayTip[] = L"MainWindow";

constexpr int kCountDownWidth = 100;
constexpr int kCountDownHeight = 50;
constexpr int kCountDownFontHeight = 28;
constexpr int kCountDownStartSeconds = 60;
constexpr UINT_PTR kCountDownTimerId = 1;
constexpr int kSettingWindowWidth = 780;
constexpr int kSettingWindowHeight = 780;

constexpr UINT kTrayMessage = WM_APP + 1;
constexpr UINT kTrayIconId = 1;

constexpr UINT ID_TRAY_OPEN = 1;
constexpr UINT ID_TRAY_SETTING = 2;
constexpr UINT ID_TRAY_EXIT = 3;

struct AppSettings {
    std::wstring greeting = L"Hello World";
    int fontHeight = 48;
};

AppSettings g_settings;
int g_countdownSeconds = kCountDownStartSeconds;
HINSTANCE g_hInstance = nullptr;
HWND g_mainWindow = nullptr;
HWND g_countDownWindow = nullptr;
HWND g_settingWindow = nullptr;
NOTIFYICONDATAW g_trayIcon{};
ComPtr<ICoreWebView2Controller> g_webViewController;

LRESULT CALLBACK CountDownWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK SettingWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

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

    if (g_settingWindow && IsWindow(g_settingWindow)) {
        ShowWindow(g_settingWindow, SW_HIDE);
    }
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
    }
}

void UpdateWebViewBounds(HWND hwnd) {
    if (!g_webViewController) {
        return;
    }
    RECT bounds{};
    GetClientRect(hwnd, &bounds);
    g_webViewController->put_Bounds(bounds);
}

void InitializeWebView(HWND hwnd) {
    wchar_t exePath[MAX_PATH]{};
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);

    std::wstring htmlPath = exePath;
    const size_t slash = htmlPath.find_last_of(L"\\/");
    if (slash != std::wstring::npos) {
        htmlPath.resize(slash + 1);
    }
    htmlPath += L"settings.html";

    HRESULT hr = CreateCoreWebView2EnvironmentWithOptions(
        nullptr, nullptr, nullptr,
        Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
            [hwnd, htmlPath](HRESULT result, ICoreWebView2Environment* environment) -> HRESULT {
                if (FAILED(result) || environment == nullptr) {
                    MessageBoxW(hwnd, L"WebView2 environment initialization failed.",
                                L"Error", MB_ICONERROR | MB_OK);
                    return result;
                }

                return environment->CreateCoreWebView2Controller(
                    hwnd,
                    Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                        [hwnd, htmlPath](HRESULT controllerResult,
                                         ICoreWebView2Controller* controller) -> HRESULT {
                            if (FAILED(controllerResult) || controller == nullptr) {
                                MessageBoxW(hwnd, L"WebView2 controller initialization failed.",
                                            L"Error", MB_ICONERROR | MB_OK);
                                return controllerResult;
                            }

                            g_webViewController = controller;

                            ComPtr<ICoreWebView2Controller2> controller2;
                            if (SUCCEEDED(controller->QueryInterface(IID_PPV_ARGS(&controller2))) &&
                                controller2) {
                                COREWEBVIEW2_COLOR white{};
                                white.A = 255;
                                white.R = 255;
                                white.G = 255;
                                white.B = 255;
                                controller2->put_DefaultBackgroundColor(white);
                            }

                            RECT bounds{};
                            GetClientRect(hwnd, &bounds);
                            g_webViewController->put_Bounds(bounds);

                            ComPtr<ICoreWebView2> webView;
                            if (SUCCEEDED(g_webViewController->get_CoreWebView2(&webView)) && webView) {
                                webView->Navigate(htmlPath.c_str());
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

void OpenSettingWindow() {
    if (g_settingWindow && IsWindow(g_settingWindow)) {
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
        WS_OVERLAPPEDWINDOW,
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
        break;

    case WM_DPICHANGED:
        HandleDpiChanged(hwnd, lParam);
        return 0;

    case WM_PAINT:
        PaintCenteredText(hwnd, g_settings.greeting.c_str(), g_settings.fontHeight);
        return 0;

    case WM_CLOSE:
        // Closing MainWindow returns to a fresh 60s countdown.
        StartCountdown();
        return 0;

    case WM_DESTROY:
        if (g_settingWindow && IsWindow(g_settingWindow)) {
            DestroyWindow(g_settingWindow);
        }
        if (g_countDownWindow && IsWindow(g_countDownWindow)) {
            DestroyWindow(g_countDownWindow);
        }
        Shell_NotifyIconW(NIM_DELETE, &g_trayIcon);
        g_mainWindow = nullptr;
        g_countDownWindow = nullptr;
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
        InitializeWebView(hwnd);
        return 0;

    case WM_DPICHANGED:
        HandleDpiChanged(hwnd, lParam);
        return 0;

    case WM_SIZE:
        UpdateWebViewBounds(hwnd);
        break;

    case WM_CLOSE:
        // Only the tray menu can show it again.
        ShowWindow(hwnd, SW_HIDE);
        return 0;

    case WM_DESTROY:
        g_webViewController.Reset();
        g_settingWindow = nullptr;
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
        WS_OVERLAPPEDWINDOW,
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
