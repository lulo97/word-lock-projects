#pragma once

#include <windows.h>
#include <string>
#include <vector>
#include <memory>

enum class FlexDirection {
    Row,
    Column
};

enum class JustifyContent {
    Start,
    Center,
    End,
    Between,
    Around,
    Evenly
};

enum class AlignItems {
    Stretch,
    Start,
    Center,
    End
};

struct Element {
    std::wstring tag;
    std::wstring id;      // ADDED: Element ID
    std::wstring text;
    std::wstring classes;

    std::vector<std::unique_ptr<Element>> children;

    RECT rect{};

    COLORREF bg = RGB(255, 255, 255);
    COLORREF color = RGB(0, 0, 0);

    bool hasBg = false;

    bool flex = false;
    FlexDirection flexDirection = FlexDirection::Row;
    JustifyContent justifyContent = JustifyContent::Start;
    AlignItems alignItems = AlignItems::Stretch;
    bool grow = false;        // flex-1
    bool fullHeight = false;  // h-screen

    int fontSize = 16;
    int padX = 0;
    int padY = 0;
    int marginTop = 0;
    int marginBottom = 0;
    int gap = 0;
    int radius = 0;

    int width = -1;         // explicit width in pixels
    int widthPercent = 0;   // 1..100 (% of available width)

    // Input support
    std::wstring placeholder;
    std::wstring inputType = L"text";
    int caret = 0;
    bool focused = false;
    bool caretVisible = true;

    // Horizontal scroll offset (pixels) for long input text.
    int scrollX = 0;

    // Display copy for wrapped paragraph text.
    std::wstring wrappedText;
};

HFONT make_ui_font(int size);

std::unique_ptr<Element> parse_html(const std::wstring& html);
void render_html(HDC hdc, Element& root, const RECT& area);
Element* hit_test(Element& root, int x, int y);