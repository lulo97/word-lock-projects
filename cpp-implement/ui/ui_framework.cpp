#include "ui_framework.h"
#include "../utils/set_maximize.h"
#include "../utils/set_frameless_window.h"
#include "../utils/set_icon.h"
#include "../utils/set_tray.h"
#include <windowsx.h>
#include <cwctype>

static const UINT_PTR CARET_TIMER_ID = 1;
static const UINT CARET_BLINK_MS = 530;

static int clamp_int(int value, int lo, int hi) {
    if (value < lo) return lo;
    if (value > hi) return hi;
    return value;
}

UIWindow::UIWindow(const std::wstring& title, int width, int height) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    const wchar_t CLASS_NAME[] = L"UIFrameworkWindow";

    WNDCLASSW wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);

    RegisterClassW(&wc);

    hwnd = CreateWindowExW(
        0, CLASS_NAME, title.c_str(), WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, width, height,
        nullptr, nullptr, wc.hInstance, this
    );

    if (hwnd) {
        // Use the embedded icon; falls back to assets\icon.ico during
        // development if no resource was compiled into the executable.
        ::set_icon(hwnd);

        ShowWindow(hwnd, SW_SHOW);
        UpdateWindow(hwnd);
        SetFocus(hwnd);
    }
}

UIWindow::~UIWindow() {
    ::remove_tray(hwnd);

    document.reset();
    if (hwnd) DestroyWindow(hwnd);
}

void UIWindow::load_html(const std::wstring& html) {
    focused_input = nullptr;
    KillTimer(hwnd, CARET_TIMER_ID);

    document = parse_html(html);
    InvalidateRect(hwnd, nullptr, TRUE);
}

void UIWindow::bind_click(const std::wstring& id, std::function<void()> callback) {
    click_events[id] = callback;
}

void UIWindow::bind_input(
    const std::wstring& id,
    std::function<void(const std::wstring&)> callback
) {
    input_events[id] = callback;
}

Element* UIWindow::find_element_by_id(Element* node, const std::wstring& id) const {
    if (!node) return nullptr;
    if (node->id == id) return node;
    for (auto& child : node->children) {
        Element* found = find_element_by_id(child.get(), id);
        if (found) return found;
    }
    return nullptr;
}

void UIWindow::set_text(const std::wstring& id, const std::wstring& text) {
    if (!document) return;
    Element* el = find_element_by_id(document.get(), id);
    if (el) {
        el->text = text;

        if (el->tag == L"input") {
            el->caret = (int)text.size();
            el->scrollX = 0;
        }

        // Invalidate triggers a repaint. The layout engine will recalculate 
        // the size because the text changed.
        InvalidateRect(hwnd, nullptr, TRUE); 
    }
}

void UIWindow::set_input_value(
    const std::wstring& id,
    const std::wstring& value
) {
    if (!document) return;
    Element* el = find_element_by_id(document.get(), id);
    if (!el || el->tag != L"input") return;

    el->text = value;
    el->caret = (int)value.size();
    el->scrollX = 0;

    InvalidateRect(hwnd, nullptr, TRUE);
}

std::wstring UIWindow::get_input_value(
    const std::wstring& id
) const {
    if (!document) return L"";

    Element* el = find_element_by_id(document.get(), id);
    if (!el || el->tag != L"input") return L"";

    return el->text;
}

bool UIWindow::focus_input(const std::wstring& id) {
    if (!document) return false;

    Element* el = find_element_by_id(document.get(), id);
    if (!el || el->tag != L"input") return false;

    focus_input(el, -1);
    return true;
}

void UIWindow::set_bounds(int x, int y, int width, int height) {
    if (!hwnd) return;

    SetWindowPos(
        hwnd,
        nullptr,
        x,
        y,
        width,
        height,
        SWP_NOZORDER | SWP_NOACTIVATE
    );
}

void UIWindow::restore() {
    if (hwnd)
        ShowWindow(hwnd, SW_RESTORE);
}

void UIWindow::set_timer(
    UINT_PTR id,
    UINT elapseMs,
    std::function<void()> callback
) {
    timer_callbacks[id] = std::move(callback);
    SetTimer(hwnd, id, elapseMs, nullptr);
}

void UIWindow::kill_timer(UINT_PTR id) {
    timer_callbacks.erase(id);
    KillTimer(hwnd, id);
}

void UIWindow::set_key_handler(
    std::function<bool(WPARAM)> handler
) {
    key_handler = std::move(handler);
}

void UIWindow::set_maximize() {
    ::set_maximize(hwnd);
}

void UIWindow::set_frameless() {
    ::set_frameless_window(hwnd);
}

void UIWindow::set_framed() {
    ::set_framed_window(hwnd);
}

bool UIWindow::set_tray() {
    return ::set_tray(hwnd);
}

void UIWindow::remove_tray() {
    ::remove_tray(hwnd);
}

void UIWindow::fire_input_event(Element* el) {
    if (!el || el->id.empty()) return;

    auto it = input_events.find(el->id);
    if (it != input_events.end())
        it->second(el->text);
}

void UIWindow::focus_input(Element* el, int x) {
    if (!el) return;

    if (focused_input && focused_input != el)
        focused_input->focused = false;

    focused_input = el;
    el->focused = true;
    el->caretVisible = true;

    if (x >= 0)
        el->caret = caret_from_x(el, x);

    SetTimer(hwnd, CARET_TIMER_ID, CARET_BLINK_MS, nullptr);
    InvalidateRect(hwnd, nullptr, FALSE);
}

void UIWindow::blur_input() {
    if (!focused_input) return;

    focused_input->focused = false;
    focused_input->scrollX = 0;
    focused_input = nullptr;

    KillTimer(hwnd, CARET_TIMER_ID);
    InvalidateRect(hwnd, nullptr, FALSE);
}

int UIWindow::caret_from_x(Element* el, int x) const {
    if (!el) return 0;

    HDC hdc = GetDC(hwnd);
    if (!hdc) return (int)el->text.size();

    HFONT font = make_ui_font(el->fontSize);
    HFONT oldFont = (HFONT)SelectObject(hdc, font);

    int left = el->rect.left + el->padX - el->scrollX;

    int best = 0;
    int bestDistance = abs(x - left);

    for (size_t i = 1; i <= el->text.size(); ++i) {
        SIZE size{};

        GetTextExtentPoint32W(
            hdc,
            el->text.c_str(),
            (int)i,
            &size
        );

        int cx = left + size.cx;
        int distance = abs(x - cx);

        if (distance < bestDistance) {
            bestDistance = distance;
            best = (int)i;
        }
    }

    SelectObject(hdc, oldFont);
    DeleteObject(font);
    ReleaseDC(hwnd, hdc);

    return best;
}

void UIWindow::handle_char(wchar_t c) {
    Element* el = focused_input;
    if (!el) return;

    bool changed = false;

    if (c == L'\b') {
        int caret = clamp_int(el->caret, 0, (int)el->text.size());

        if (caret > 0) {
            el->text.erase(caret - 1, 1);
            el->caret = caret - 1;
            changed = true;
        }
    }
    else if (c == L'\r' || c == L'\n') {
        fire_input_event(el);
        return;
    }
    else if (c == 27) { // Escape
        blur_input();
        return;
    }
    else if (c >= 32 && c != 127) {

        if (el->inputType == L"number" &&
            !(iswdigit(c) || c == L'.' ||
              c == L'-' || c == L'+')) {
            return;
        }

        int caret = clamp_int(el->caret, 0, (int)el->text.size());

        el->text.insert(caret, 1, c);
        el->caret = caret + 1;
        changed = true;
    }

    el->caretVisible = true;
    SetTimer(hwnd, CARET_TIMER_ID, CARET_BLINK_MS, nullptr);

    InvalidateRect(hwnd, nullptr, FALSE);

    if (changed)
        fire_input_event(el);
}

void UIWindow::handle_keydown(WPARAM key) {
    Element* el = focused_input;
    if (!el) return;

    bool changed = false;

    switch (key) {
    case VK_LEFT:
        el->caret = clamp_int(
            el->caret - 1, 0, (int)el->text.size()
        );
        break;

    case VK_RIGHT:
        el->caret = clamp_int(
            el->caret + 1, 0, (int)el->text.size()
        );
        break;

    case VK_HOME:
        el->caret = 0;
        break;

    case VK_END:
        el->caret = (int)el->text.size();
        break;

    case VK_DELETE:
        if (el->caret < (int)el->text.size()) {
            el->text.erase(el->caret, 1);
            changed = true;
        }
        break;

    default:
        return;
    }

    el->caretVisible = true;
    SetTimer(hwnd, CARET_TIMER_ID, CARET_BLINK_MS, nullptr);

    InvalidateRect(hwnd, nullptr, FALSE);

    if (changed)
        fire_input_event(el);
}

void UIWindow::run() {
    MSG msg = {};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
}

void UIWindow::handle_paint() {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);

    RECT client;
    GetClientRect(hwnd, &client);

    // Double buffering to prevent flicker
    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP memBitmap = CreateCompatibleBitmap(hdc, client.right, client.bottom);
    HBITMAP oldBitmap = (HBITMAP)SelectObject(memDC, memBitmap);

    FillRect(memDC, &client, (HBRUSH)GetStockObject(WHITE_BRUSH));

    if (document) {
        RECT area = { 20, 20, client.right - 20, client.bottom - 20 };
        render_html(memDC, *document, area);
    }

    BitBlt(hdc, 0, 0, client.right, client.bottom, memDC, 0, 0, SRCCOPY);

    SelectObject(memDC, oldBitmap);
    DeleteObject(memBitmap);
    DeleteDC(memDC);

    EndPaint(hwnd, &ps);
}

void UIWindow::handle_click(int x, int y) {
    if (!document) return;

    Element* clicked = hit_test(*document, x, y);

    if (clicked && clicked->tag == L"input") {
        focus_input(clicked, x);
        return;
    }

    blur_input();

    if (clicked && !clicked->id.empty()) {
        auto it = click_events.find(clicked->id);
        if (it != click_events.end()) {
            it->second(); // Execute the callback
        }
    }
}

LRESULT CALLBACK UIWindow::WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    UIWindow* self = nullptr;

    if (msg == WM_NCCREATE) {
        CREATESTRUCT* pCreate = (CREATESTRUCT*)lParam;
        self = (UIWindow*)pCreate->lpCreateParams;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)self);
    } else {
        self = (UIWindow*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
    }

    if (self) {
        switch (msg) {
        case WM_PAINT:
            self->handle_paint();
            return 0;
        case WM_LBUTTONUP:
            self->handle_click(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            return 0;
        case WM_CHAR:
            self->handle_char((wchar_t)wParam);
            return 0;
        case WM_KEYDOWN:
            if (self->key_handler &&
                self->key_handler(wParam)) {
                return 0;
            }

            self->handle_keydown(wParam);
            return 0;
        case WM_TIMER:
            {
                auto it = self->timer_callbacks.find(wParam);

                if (it != self->timer_callbacks.end()) {
                    auto callback = it->second;
                    callback();
                    return 0;
                }
            }

            if (wParam == CARET_TIMER_ID && self->focused_input) {
                self->focused_input->caretVisible =
                    !self->focused_input->caretVisible;

                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;
        case WM_TRAY_CALLBACK:
            handle_tray_message(hwnd, lParam);
            return 0;
        case WM_KILLFOCUS:
            KillTimer(hwnd, CARET_TIMER_ID);

            if (self->focused_input) {
                self->focused_input->caretVisible = false;
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;
        case WM_SETFOCUS:
            if (self->focused_input) {
                self->focused_input->caretVisible = true;
                SetTimer(hwnd, CARET_TIMER_ID, CARET_BLINK_MS, nullptr);
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;
        case WM_ERASEBKGND:
            return 1;
        case WM_SIZE:
            InvalidateRect(hwnd, nullptr, TRUE);
            return 0;
        case WM_DESTROY:
            KillTimer(hwnd, CARET_TIMER_ID);
            ::remove_tray(hwnd);
            PostQuitMessage(0);
            return 0;
        }
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}