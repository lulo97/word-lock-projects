#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <functional>
#include <memory>
#include <string>
#include <unordered_set>
#include "app/document.h"

namespace ui {

struct WindowOptions {
    std::wstring title;
    int width = 400, height = 300;       // logical px (96 dpi)
    bool popup = false;                  // WS_POPUP, no frame
    bool topmost = false;
    bool toolWindow = false;             // WS_EX_TOOLWINDOW (no taskbar)
    bool autoSizeToContent = false;      // size window from Document::measureContent
    bool centered = true;
    bool visible = true;
    bool noActivate = false;             // overlay popup that must not take focus
};

// Win32 window that hosts one Document: message loop glue, double buffer, DPI, timers, cursor.
class WindowHost {
public:
    WindowHost();
    ~WindowHost();

    // Register class + create window. In: options, document (shared ownership). Out: true on success.
    bool create(const WindowOptions& opt, std::shared_ptr<Document> doc);

    void show(bool visible);
    void close();
    HWND hwnd() const { return hwnd_; }
    Document& document() { return *doc_; }

    // Move/resize in logical px. In: x, y, w, h.
    void setBounds(int x, int y, int w, int h);

    // Current DPI scale (dpi/96). Out: scale.
    float dpiScale() const;

    // Start a timer. In: id, intervalMs, callback, oneShot (one-shot timers
    // kill themselves after firing; they are the ones tick() in app tests
    // fires on behalf of the message pump). Out: none.
    void setTimer(UINT_PTR id, UINT intervalMs, std::function<void()> cb,
                  bool oneShot = false);
    void killTimer(UINT_PTR id);

    // Test seams (no message pump in app_test): fire every pending one-shot
    // timer once, in id order, simulating one WM_TIMER pass. Repeating timers
    // are not fired (the app drives those via its own ticks).
    void fireOneShotTimers();

    // Number of pending timers (for `app assert timers == N`).
    size_t pendingTimers() const { return timers_.size(); }

    // Callback when window closes / loses focus (context menu dismissal).
    std::function<void()> onClose;
    std::function<void()> onDeactivate;
    std::function<bool(UINT)> onKeyDown;  // return true to consume the key

    // Call once in main: enable per-monitor DPI v2 awareness.
    static void enableDpiAwareness();

private:
    static LRESULT CALLBACK wndProc(HWND, UINT, WPARAM, LPARAM);
    LRESULT handle(UINT msg, WPARAM wp, LPARAM lp);
    void onPaint();                 // double buffered: memory DC -> Document::paint -> BitBlt
    void onSize(int w, int h);      // relayout
    void onDpiChanged(UINT dpi, const RECT* suggested);
    void applyCursor();

    HWND hwnd_ = nullptr;
    std::shared_ptr<Document> doc_;
    WindowOptions opt_;
    UINT dpi_ = 96;
    std::unordered_map<UINT_PTR, std::function<void()>> timers_;
    std::unordered_set<UINT_PTR> oneShotTimers_;   // ids marked one-shot
};

} // namespace ui
