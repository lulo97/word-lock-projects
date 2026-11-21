#pragma once

#include <windows.h>
#include <string>
#include <functional>
#include <map>
#include <memory>
#include "html.h"

class UIWindow {
public:
    UIWindow(const std::wstring& title, int width, int height);
    ~UIWindow();

    void load_html(const std::wstring& html);
    void bind_click(const std::wstring& id, std::function<void()> callback);
    void bind_input(const std::wstring& id, std::function<void(const std::wstring&)> callback);
    void set_text(const std::wstring& id, const std::wstring& text);
    void set_input_value(const std::wstring& id, const std::wstring& value);
    std::wstring get_input_value(const std::wstring& id) const;
    bool focus_input(const std::wstring& id);
    void set_bounds(int x, int y, int width, int height);
    void restore();
    void set_timer(UINT_PTR id, UINT elapseMs, std::function<void()> callback);
    void kill_timer(UINT_PTR id);
    void set_key_handler(std::function<bool(WPARAM)> handler);
    void set_maximize();
    void set_frameless();
    void set_framed();
    bool set_tray();
    void remove_tray();
    void run();

private:
    HWND hwnd;
    std::unique_ptr<Element> document;
    std::map<std::wstring, std::function<void()>> click_events;
    std::map<std::wstring, std::function<void(const std::wstring&)>> input_events;
    std::map<UINT_PTR, std::function<void()>> timer_callbacks;
    std::function<bool(WPARAM)> key_handler;
    Element* focused_input = nullptr;

    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    
    void handle_paint();
    void handle_click(int x, int y);
    void handle_char(wchar_t c);
    void handle_keydown(WPARAM key);
    void focus_input(Element* el, int x);
    void blur_input();
    int caret_from_x(Element* el, int x) const;
    void fire_input_event(Element* el);
    Element* find_element_by_id(Element* node, const std::wstring& id) const;
};