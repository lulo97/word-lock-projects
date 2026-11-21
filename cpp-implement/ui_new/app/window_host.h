#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <functional>
#include <memory>
#include <string>
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

    // Start a repeating timer. In: id, intervalMs, callback. Out: none.
    void setTimer(UINT_PTR id, UINT intervalMs, std::function<void()> cb);
    void killTimer(UINT_PTR id);

    // Callback when window closes / loses focus (context menu dismissal).
    std::function<void()> onClose;
    std::function<void()> onDeactivate;

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
};

} // namespace ui
