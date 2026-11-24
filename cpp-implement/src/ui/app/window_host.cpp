#include "app/window_host.h"

#include "../../utils/debug_log.h"
#include "../../utils/set_icon.h"
#include "../../utils/set_tray.h"
#include "paint/canvas.h"
#include "paint/font_cache.h"

#include <algorithm>
#include <vector>

namespace ui {

namespace {

constexpr wchar_t kWindowClass[] = L"UI_New_WindowHost";
constexpr UINT_PTR kBlinkTimer = 0xFFFF0001;
bool g_classRegistered = false;
int g_windowCount = 0;

} // namespace

WindowHost::WindowHost() = default;

WindowHost::~WindowHost() {
    if (hwnd_)
        DestroyWindow(hwnd_);

    shutdownGraphics();
    clearFontCache();
}

void WindowHost::enableDpiAwareness() {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
}

bool WindowHost::create(const WindowOptions& opt, std::shared_ptr<Document> doc) {
    if (!doc)
        return false;

    doc_ = std::move(doc);
    opt_ = opt;

    enableDpiAwareness();

    if (!initGraphics())
        return false;

    HINSTANCE instance = GetModuleHandleW(nullptr);

    if (!g_classRegistered) {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = WindowHost::wndProc;
        wc.hInstance = instance;
        wc.lpszClassName = kWindowClass;
        wc.hCursor = LoadCursorA(nullptr, IDC_ARROW);
        wc.hbrBackground = nullptr;
        wc.style = CS_HREDRAW | CS_VREDRAW;

        if (!RegisterClassExW(&wc))
            return false;

        g_classRegistered = true;
    }

    UINT dpi = GetDpiForSystem();
    float scale = static_cast<float>(dpi) / 96.0f;
    int width = static_cast<int>(opt.width * scale);
    int height = static_cast<int>(opt.height * scale);

    if (opt.autoSizeToContent) {
        SIZE content = doc_->measureContent(width, scale);
        width = std::max(1, static_cast<int>(content.cx));
        height = std::max(1, static_cast<int>(content.cy));
    }

    int x = CW_USEDEFAULT;
    int y = CW_USEDEFAULT;

    if (opt.centered) {
        RECT work{};
        SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
        x = work.left + ((work.right - work.left) - width) / 2;
        y = work.top + ((work.bottom - work.top) - height) / 2;
    }

    DWORD style = opt.popup
        ? WS_POPUP
        : WS_OVERLAPPEDWINDOW;

    DWORD exStyle = 0;

    if (opt.topmost)
        exStyle |= WS_EX_TOPMOST;

    if (opt.toolWindow)
        exStyle |= WS_EX_TOOLWINDOW;

    RECT windowRect { 0, 0, width, height };
    AdjustWindowRectExForDpi(&windowRect, style, FALSE, exStyle, dpi);
    width = windowRect.right - windowRect.left;
    height = windowRect.bottom - windowRect.top;

    hwnd_ = CreateWindowExW(
        exStyle,
        kWindowClass,
        opt.title.c_str(),
        style,
        x, y, width, height,
        nullptr,
        nullptr,
        instance,
        this
    );

    if (!hwnd_)
        return false;

    ++g_windowCount;
    dpi_ = GetDpiForWindow(hwnd_);
    set_icon(hwnd_);

    SetTimer(hwnd_, kBlinkTimer, 530, nullptr);

    if (opt.visible)
        show(true);

    return true;
}

void WindowHost::show(bool visible) {
    if (!hwnd_)
        return;

    bool noActivate = opt_.noActivate;
    ShowWindow(
        hwnd_,
        visible ? (noActivate ? SW_SHOWNOACTIVATE : SW_SHOW) : SW_HIDE
    );

    if (visible) {
        UpdateWindow(hwnd_);

        if (!noActivate)
            SetFocus(hwnd_);
    }
}

void WindowHost::close() {
    if (hwnd_)
        PostMessageW(hwnd_, WM_CLOSE, 0, 0);
}

void WindowHost::setBounds(int x, int y, int w, int h) {
    if (!hwnd_)
        return;

    float scale = dpiScale();

    SetWindowPos(
        hwnd_, nullptr,
        x, y,
        static_cast<int>(w * scale),
        static_cast<int>(h * scale),
        SWP_NOZORDER | SWP_NOACTIVATE
    );
}

float WindowHost::dpiScale() const {
    return static_cast<float>(dpi_) / 96.0f;
}

void WindowHost::setTimer(UINT_PTR id, UINT intervalMs, std::function<void()> cb,
                          bool oneShot) {
    if (id == kBlinkTimer)
        ++id;

    UI_LOG("timer", L"hwnd=%p id=%llu set interval=%u ms oneShot=%d",
           hwnd_, (unsigned long long)id, intervalMs, oneShot);
    timers_[id] = std::move(cb);

    if (oneShot)
        oneShotTimers_.insert(id);
    else
        oneShotTimers_.erase(id);

    if (hwnd_)
        SetTimer(hwnd_, id, intervalMs, nullptr);
}

void WindowHost::killTimer(UINT_PTR id) {
    UI_LOG("timer", L"hwnd=%p id=%llu killed", hwnd_, (unsigned long long)id);
    timers_.erase(id);
    oneShotTimers_.erase(id);

    if (hwnd_)
        KillTimer(hwnd_, id);
}

std::vector<UINT_PTR> WindowHost::pendingOneShotIds() const {
    std::vector<UINT_PTR> ids(oneShotTimers_.begin(), oneShotTimers_.end());
    std::sort(ids.begin(), ids.end());
    return ids;
}

void WindowHost::fireOneShotTimers() {
    // Snapshot at call time: see fireOneShotTimers(snapshot).
    fireOneShotTimers(pendingOneShotIds());
}

void WindowHost::fireOneShotTimers(const std::vector<UINT_PTR>& ids) {
    // The ids were collected (possibly before any callback ran): a callback
    // kills its own timer (mutating the maps) and may enqueue further
    // one-shot timers, which fire on the next tick, like the real pump.
    for (UINT_PTR id : ids) {
        if (auto it = timers_.find(id); it != timers_.end()) {
            UI_LOG("timer", L"hwnd=%p id=%llu fired (test)", hwnd_, (unsigned long long)id);
            it->second();
        }
    }
}

LRESULT CALLBACK WindowHost::wndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    WindowHost* self = nullptr;

    if (msg == WM_NCCREATE) {
        CREATESTRUCTW* create = reinterpret_cast<CREATESTRUCTW*>(lp);
        self = static_cast<WindowHost*>(create->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));

        if (self)
            self->hwnd_ = hwnd;
    }
    else {
        self = reinterpret_cast<WindowHost*>(
            GetWindowLongPtrW(hwnd, GWLP_USERDATA)
        );
    }

    if (self)
        return self->handle(msg, wp, lp);

    return DefWindowProcW(hwnd, msg, wp, lp);
}

LRESULT WindowHost::handle(UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_PAINT:
        onPaint();
        return 0;

    case WM_ERASEBKGND:
        return 1;

    case WM_SIZE:
        onSize(LOWORD(lp), HIWORD(lp));
        return 0;

    case WM_DPICHANGED: {
        UINT dpi = HIWORD(wp);
        const RECT* suggested = reinterpret_cast<const RECT*>(lp);

        if (suggested) {
            SetWindowPos(
                hwnd_, nullptr,
                suggested->left, suggested->top,
                suggested->right - suggested->left,
                suggested->bottom - suggested->top,
                SWP_NOZORDER | SWP_NOACTIVATE
            );
        }

        onDpiChanged(dpi, suggested);
        return 0;
    }

    case WM_TIMER:
        if (wp == kBlinkTimer) {
            if (doc_ && doc_->tickBlink())
                InvalidateRect(hwnd_, nullptr, FALSE);

            return 0;
        }

        if (auto it = timers_.find(wp); it != timers_.end()) {
            UI_LOG("timer", L"hwnd=%p id=%llu fired", hwnd_, (unsigned long long)wp);
            auto callback = it->second;
            callback();

            if (doc_ && doc_->needsPaint())
                InvalidateRect(hwnd_, nullptr, FALSE);

            return 0;
        }

        break;

    case WM_SETCURSOR:
        if (LOWORD(lp) == HTCLIENT) {
            applyCursor();
            return TRUE;
        }

        return DefWindowProcW(hwnd_, msg, wp, lp);

    case WM_ACTIVATE:
        if (LOWORD(wp) == WA_INACTIVE && onDeactivate)
            onDeactivate();

        break;

    case WM_KEYDOWN:
        if (onKeyDown && onKeyDown(static_cast<UINT>(wp)))
            return 0;
        break;

    case WM_TRAY_CALLBACK:
        handle_tray_message(hwnd_, lp);
        return 0;

    case WM_ENDSESSION:
        if (wp && onEndSession)
            onEndSession();
        break;

    case WM_CLOSE:
        if (onClose)
            onClose();

        DestroyWindow(hwnd_);
        return 0;

    case WM_DESTROY:
        KillTimer(hwnd_, kBlinkTimer);

        for (const auto& timer : timers_)
            KillTimer(hwnd_, timer.first);

        timers_.clear();
        hwnd_ = nullptr;

        if (g_windowCount > 0)
            --g_windowCount;

        if (g_windowCount == 0)
            PostQuitMessage(0);

        return 0;

    case WM_LBUTTONDOWN:
        SetCapture(hwnd_);
        break;

    case WM_LBUTTONUP:
        ReleaseCapture();
        break;

    default:
        break;
    }

    if (doc_) {
        LRESULT result = 0;

        if (doc_->handleMessage(hwnd_, msg, wp, lp, result)) {
            if (msg == WM_MOUSEMOVE) {
                TRACKMOUSEEVENT track{};
                track.cbSize = sizeof(track);
                track.dwFlags = TME_LEAVE;
                track.hwndTrack = hwnd_;
                TrackMouseEvent(&track);
            }

            if (doc_->needsPaint())
                InvalidateRect(hwnd_, nullptr, FALSE);

            return result;
        }
    }

    return DefWindowProcW(hwnd_, msg, wp, lp);
}

void WindowHost::onPaint() {
    PAINTSTRUCT ps{};
    HDC hdc = BeginPaint(hwnd_, &ps);

    RECT client{};
    GetClientRect(hwnd_, &client);

    int width = client.right - client.left;
    int height = client.bottom - client.top;

    if (width <= 0 || height <= 0) {
        EndPaint(hwnd_, &ps);
        return;
    }

    HDC mem = CreateCompatibleDC(hdc);
    HBITMAP bitmap = CreateCompatibleBitmap(hdc, width, height);
    HGDIOBJ oldBitmap = SelectObject(mem, bitmap);

    RECT fill = client;
    FillRect(mem, &fill, static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH)));

    if (doc_)
        doc_->paint(mem, client, dpiScale());

    BitBlt(hdc, 0, 0, width, height, mem, 0, 0, SRCCOPY);

    SelectObject(mem, oldBitmap);
    DeleteObject(bitmap);
    DeleteDC(mem);
    EndPaint(hwnd_, &ps);
}

void WindowHost::onSize(int w, int h) {
    (void)w;
    (void)h;

    if (doc_) {
        LRESULT result = 0;
        doc_->handleMessage(hwnd_, WM_SIZE, 0, MAKELPARAM(w, h), result);
    }

    InvalidateRect(hwnd_, nullptr, TRUE);
}

void WindowHost::onDpiChanged(UINT dpi, const RECT* suggested) {
    (void)suggested;
    dpi_ = dpi;

    if (doc_) {
        LRESULT result = 0;
        doc_->handleMessage(hwnd_, WM_DPICHANGED, MAKEWPARAM(dpi, dpi), 0, result);
    }

    clearFontCache();
    InvalidateRect(hwnd_, nullptr, TRUE);
}

void WindowHost::applyCursor() {
    CursorKind kind = doc_ ? doc_->events().currentCursor() : CursorKind::Arrow;
    LPCSTR id = IDC_ARROW;

    if (kind == CursorKind::Hand)
        id = IDC_HAND;
    else if (kind == CursorKind::IBeam)
        id = IDC_IBEAM;

    SetCursor(LoadCursorA(nullptr, id));
}

} // namespace ui
