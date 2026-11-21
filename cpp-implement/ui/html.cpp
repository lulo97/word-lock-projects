
#include "html.h"

#include <sstream>
#include <cstdlib>
#include <algorithm>
#include <cwctype>

// --------------------------------------------------
// Helpers
// --------------------------------------------------

static std::wstring trim(const std::wstring& s) {
    size_t a = s.find_first_not_of(L" \t\r\n");

    if (a == std::wstring::npos)
        return L"";

    size_t b = s.find_last_not_of(L" \t\r\n");

    return s.substr(a, b - a + 1);
}

static std::wstring collapse_whitespace(const std::wstring& s) {
    std::wstring out;
    bool pendingSpace = false;

    for (wchar_t c : s) {
        if (iswspace(c)) {
            if (!out.empty())
                pendingSpace = true;
        }
        else {
            if (pendingSpace) {
                out += L' ';
                pendingSpace = false;
            }

            out += c;
        }
    }

    return out;
}

static void normalize_text(Element& e) {
    // Input values may contain meaningful spaces, so leave them alone.
    if (e.tag != L"input")
        e.text = collapse_whitespace(e.text);

    for (auto& child : e.children)
        normalize_text(*child);
}

// Read a quoted/unquoted attribute value from the inside of a tag.
static std::wstring get_attr(
    const std::wstring& tagText,
    const std::wstring& name
) {
    size_t pos = 0;

    while ((pos = tagText.find(name, pos)) != std::wstring::npos) {
        bool boundary =
            pos == 0 ||
            iswspace(tagText[pos - 1]);

        size_t after = pos + name.size();

        if (boundary && after < tagText.size() &&
            tagText[after] == L'=') {

            size_t q = after + 1;

            while (q < tagText.size() && iswspace(tagText[q]))
                ++q;

            if (q < tagText.size() &&
                (tagText[q] == L'"' || tagText[q] == L'\'')) {

                wchar_t quote = tagText[q];
                size_t close = tagText.find(quote, q + 1);

                if (close != std::wstring::npos)
                    return tagText.substr(
                        q + 1, close - q - 1
                    );
            }
            else if (q < tagText.size()) {
                size_t end = tagText.find_first_of(
                    L" \t\r\n/>", q
                );

                return tagText.substr(
                    q,
                    end == std::wstring::npos
                        ? tagText.size() - q
                        : end - q
                );
            }
        }

        pos = after;
    }

    return L"";
}

static int spacing(const std::wstring& value) {
    if (value == L"0") return 0;
    if (value == L"1") return 4;
    if (value == L"2") return 8;
    if (value == L"3") return 12;
    if (value == L"4") return 16;
    if (value == L"5") return 20;
    if (value == L"6") return 24;
    if (value == L"8") return 32;
    if (value == L"10") return 40;
    if (value == L"12") return 48;

    return 0;
}

HFONT make_ui_font(int size) {
    return CreateFontW(
        -size, 0, 0, 0,
        FW_NORMAL,
        FALSE, FALSE, FALSE,
        DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE,
        L"Segoe UI"
    );
}

// Insert hard line breaks inside words that are wider than the available
// space, so long unbroken text (e.g. a huge pasted token) still wraps.
static std::wstring break_long_words(
    HDC hdc,
    const std::wstring& text,
    int maxWidth
) {
    if (maxWidth <= 0 || text.empty())
        return text;

    std::wstring out;
    out.reserve(text.size() + 8);

    size_t i = 0;

    while (i < text.size()) {

        if (iswspace(text[i])) {
            out += text[i];
            ++i;
            continue;
        }

        size_t start = i;

        while (i < text.size() && !iswspace(text[i]))
            ++i;

        std::wstring word = text.substr(start, i - start);

        SIZE wordSize{};

        GetTextExtentPoint32W(
            hdc,
            word.c_str(),
            (int)word.size(),
            &wordSize
        );

        if (wordSize.cx <= maxWidth) {
            out += word;
            continue;
        }

        // Break the overlong word into pieces that fit.
        std::wstring chunk;
        int chunkWidth = 0;

        for (wchar_t c : word) {

            SIZE charSize{};

            GetTextExtentPoint32W(
                hdc,
                &c,
                1,
                &charSize
            );

            if (chunkWidth + charSize.cx > maxWidth &&
                !chunk.empty()) {

                out += chunk;
                out += L'\n';

                chunk.clear();
                chunkWidth = 0;
            }

            chunk += c;
            chunkWidth += charSize.cx;
        }

        out += chunk;
    }

    return out;
}

// --------------------------------------------------
// Tailwind parser
// --------------------------------------------------

static void parse_classes(Element& e) {
    std::wstringstream ss(e.classes);
    std::wstring token;

    while (ss >> token) {

        // Background colors
        if (token == L"bg-white") {
            e.bg = RGB(255, 255, 255);
            e.hasBg = true;
        }
        else if (token == L"bg-black") {
            e.bg = RGB(0, 0, 0);
            e.hasBg = true;
        }
        else if (token == L"bg-blue-600") {
            e.bg = RGB(37, 99, 235);
            e.hasBg = true;
        }
        else if (token == L"bg-gray-100") {
            e.bg = RGB(243, 244, 246);
            e.hasBg = true;
        }
        else if (token == L"bg-gray-200") {
            e.bg = RGB(229, 231, 235);
            e.hasBg = true;
        }
        else if (token == L"bg-red-500") {
            e.bg = RGB(239, 68, 68);
            e.hasBg = true;
        }

        // Text colors
        else if (token == L"text-black")
            e.color = RGB(0, 0, 0);

        else if (token == L"text-white")
            e.color = RGB(255, 255, 255);

        else if (token == L"text-gray-600")
            e.color = RGB(75, 85, 99);

        else if (token == L"text-blue-600")
            e.color = RGB(37, 99, 235);

        else if (token == L"text-green-600")
            e.color = RGB(22, 163, 74);

        // Font sizes
        else if (token == L"text-xs") e.fontSize = 12;
        else if (token == L"text-sm") e.fontSize = 14;
        else if (token == L"text-base") e.fontSize = 16;
        else if (token == L"text-lg") e.fontSize = 18;
        else if (token == L"text-xl") e.fontSize = 20;
        else if (token == L"text-2xl") e.fontSize = 24;
        else if (token == L"text-3xl") e.fontSize = 30;

        // Padding
        else if (token.rfind(L"p-", 0) == 0) {
            e.padX = e.padY = spacing(token.substr(2));
        }
        else if (token.rfind(L"px-", 0) == 0)
            e.padX = spacing(token.substr(3));

        else if (token.rfind(L"py-", 0) == 0)
            e.padY = spacing(token.substr(3));

        // Margins
        else if (token.rfind(L"mt-", 0) == 0)
            e.marginTop = spacing(token.substr(3));

        else if (token.rfind(L"mb-", 0) == 0)
            e.marginBottom = spacing(token.substr(3));

        // Flexbox
        else if (token == L"flex")
            e.flex = true;

        else if (token == L"flex-row") {
            e.flex = true;
            e.flexDirection = FlexDirection::Row;
        }
        else if (token == L"flex-col") {
            e.flex = true;
            e.flexDirection = FlexDirection::Column;
        }
        else if (token == L"flex-1")
            e.grow = true;

        else if (token == L"justify-start")
            e.justifyContent = JustifyContent::Start;

        else if (token == L"justify-center")
            e.justifyContent = JustifyContent::Center;

        else if (token == L"justify-end")
            e.justifyContent = JustifyContent::End;

        else if (token == L"justify-between")
            e.justifyContent = JustifyContent::Between;

        else if (token == L"justify-around")
            e.justifyContent = JustifyContent::Around;

        else if (token == L"justify-evenly")
            e.justifyContent = JustifyContent::Evenly;

        else if (token == L"items-stretch")
            e.alignItems = AlignItems::Stretch;

        else if (token == L"items-start")
            e.alignItems = AlignItems::Start;

        else if (token == L"items-center")
            e.alignItems = AlignItems::Center;

        else if (token == L"items-end")
            e.alignItems = AlignItems::End;

        else if (token == L"h-screen")
            e.fullHeight = true;

        // Width
        else if (token == L"w-full")
            e.widthPercent = 100;

        else if (token == L"w-1/2")
            e.widthPercent = 50;

        else if (token == L"w-1/3")
            e.widthPercent = 33;

        else if (token == L"w-2/3")
            e.widthPercent = 66;

        else if (token == L"w-1/4")
            e.widthPercent = 25;

        else if (token == L"w-3/4")
            e.widthPercent = 75;

        else if (token.rfind(L"w-[", 0) == 0 &&
                 token.size() > 4 &&
                 token.back() == L']') {

            std::wstring value = token.substr(
                3, token.size() - 4
            );

            if (value.size() > 2 &&
                value.substr(value.size() - 2) == L"px") {
                value = value.substr(0, value.size() - 2);
            }

            int pixels = _wtoi(value.c_str());

            if (pixels > 0)
                e.width = pixels;
        }

        else if (token.rfind(L"gap-", 0) == 0)
            e.gap = spacing(token.substr(4));

        // Rounded corners
        else if (token == L"rounded")
            e.radius = 6;

        else if (token == L"rounded-lg")
            e.radius = 10;

        else if (token == L"rounded-xl")
            e.radius = 14;
    }

    // Default button appearance
    if (e.tag == L"button" && !e.hasBg) {
        e.bg = RGB(37, 99, 235);
        e.hasBg = true;
        e.color = RGB(255, 255, 255);
        e.padX = 20;
        e.padY = 10;
        e.radius = 6;
    }

    // Default input appearance
    if (e.tag == L"input") {
        if (!e.hasBg) {
            e.bg = RGB(255, 255, 255);
            e.hasBg = true;
        }

        if (e.padX == 0 && e.padY == 0) {
            e.padX = 10;
            e.padY = 8;
        }

        if (e.radius == 0)
            e.radius = 6;
    }
}

// --------------------------------------------------
// HTML parser
// --------------------------------------------------

std::unique_ptr<Element> parse_html(
    const std::wstring& html
) {
    auto root = std::make_unique<Element>();
    root->tag = L"root";

    std::vector<Element*> stack;
    stack.push_back(root.get());

    size_t pos = 0;

    while (pos < html.size()) {

        if (html[pos] == L'<') {

            size_t end = html.find(L'>', pos);

            if (end == std::wstring::npos)
                break;

            std::wstring tagText =
                trim(html.substr(pos + 1, end - pos - 1));

            if (tagText.empty()) {
                pos = end + 1;
                continue;
            }

            // Closing tag
            if (tagText[0] == L'/') {
                if (stack.size() > 1)
                    stack.pop_back();

                pos = end + 1;
                continue;
            }

            // Ignore comments and declarations
            if (tagText[0] == L'!' || tagText[0] == L'?') {
                pos = end + 1;
                continue;
            }

            bool selfClosing = tagText.back() == L'/';

            if (selfClosing)
                tagText.pop_back();

            size_t space = tagText.find_first_of(L" \t\r\n");

            std::wstring tag = tagText.substr(0, space);

            std::transform(
                tag.begin(), tag.end(),
                tag.begin(), towlower
            );

            // Supported elements
            if (tag == L"div" ||
                tag == L"p" ||
                tag == L"button" ||
                tag == L"input") {

                auto node = std::make_unique<Element>();
                node->tag = tag;

                node->classes = get_attr(tagText, L"class");
                node->id = get_attr(tagText, L"id");

                if (tag == L"input") {
                    node->placeholder =
                        get_attr(tagText, L"placeholder");

                    node->text = get_attr(tagText, L"value");
                    node->caret = (int)node->text.size();

                    node->inputType =
                        get_attr(tagText, L"type");

                    if (node->inputType.empty())
                        node->inputType = L"text";

                    // <input> is a void element
                    selfClosing = true;
                }

                parse_classes(*node);

                Element* ptr = node.get();

                stack.back()->children.push_back(
                    std::move(node)
                );

                if (!selfClosing)
                    stack.push_back(ptr);
            }

            pos = end + 1;
        }
        else {
            size_t end = html.find(L'<', pos);

            if (end == std::wstring::npos)
                end = html.size();

            stack.back()->text +=
                html.substr(pos, end - pos);

            pos = end;
        }
    }

    normalize_text(*root);

    return root;
}

// --------------------------------------------------
// Layout engine
// --------------------------------------------------

static int intrinsic_width(Element& e, HDC hdc, int maxWidth) {
    if (maxWidth <= 0)
        maxWidth = 1;

    if (e.widthPercent > 0)
        return max(0, maxWidth * e.widthPercent / 100);

    if (e.width >= 0)
        return min(e.width, maxWidth);

    HFONT font = make_ui_font(e.fontSize);
    HFONT old = (HFONT)SelectObject(hdc, font);

    int content = 0;

    if (!e.text.empty() && e.tag != L"input") {
        SIZE size{};

        GetTextExtentPoint32W(
            hdc,
            e.text.c_str(),
            (int)e.text.size(),
            &size
        );

        content = max(content, (int)size.cx);
    }

    if (e.tag == L"input")
        content = max(content, 200);

    int childMax = max(1, maxWidth - e.padX * 2);

    for (auto& child : e.children) {
        content = max(
            content,
            intrinsic_width(*child, hdc, childMax)
        );
    }

    SelectObject(hdc, old);
    DeleteObject(font);

    int width = content + e.padX * 2;

    return min(max(0, width), maxWidth);
}

static int layout_element(
    Element& e,
    HDC hdc,
    int x,
    int y,
    int availableWidth,
    int availableHeight,
    int forcedWidth = -1
);

static int layout_flex_row(
    Element& e,
    HDC hdc,
    int contentX,
    int contentY,
    int innerWidth,
    int availableHeight,
    int fixedInnerHeight
) {
    const int count = (int)e.children.size();

    if (count == 0)
        return fixedInnerHeight >= 0 ? fixedInnerHeight : 0;

    std::vector<int> widths(count, 0);
    std::vector<int> heights(count, 0);

    int growCount = 0;
    int nonGrowWidth = 0;

    for (int i = 0; i < count; ++i) {
        Element& child = *e.children[i];

        if (child.grow) {
            ++growCount;
        }
        else {
            widths[i] = intrinsic_width(
                child, hdc, innerWidth
            );

            nonGrowWidth += widths[i];
        }
    }

    const int baseGaps = e.gap * (count - 1);
    const int freeForGrow =
        innerWidth - nonGrowWidth - baseGaps;

    if (growCount > 0) {
        int each = freeForGrow > 0
            ? freeForGrow / growCount
            : 0;

        for (int i = 0; i < count; ++i) {
            if (e.children[i]->grow)
                widths[i] = each;
        }
    }

    int sumWidths = 0;

    for (int width : widths)
        sumWidths += width;

    int maxHeight = 0;

    for (int i = 0; i < count; ++i) {
        Element& child = *e.children[i];

        int childHeightLimit =
            fixedInnerHeight >= 0
                ? fixedInnerHeight
                : availableHeight;

        layout_element(
            child,
            hdc,
            0,
            0,
            widths[i],
            childHeightLimit,
            widths[i]
        );

        heights[i] = max(
            0,
            (int)(child.rect.bottom - child.rect.top)
        );

        maxHeight = max(maxHeight, heights[i]);
    }

    const int innerHeight =
        fixedInnerHeight >= 0
            ? fixedInnerHeight
            : maxHeight;

    const int freeSpace = max(
        0,
        innerWidth - sumWidths - baseGaps
    );

    int startX = contentX;
    int spacing = e.gap;

    switch (e.justifyContent) {
    case JustifyContent::Center:
        startX += freeSpace / 2;
        break;

    case JustifyContent::End:
        startX += freeSpace;
        break;

    case JustifyContent::Between:
        if (count > 1)
            spacing += freeSpace / (count - 1);
        break;

    case JustifyContent::Around:
        spacing += freeSpace / count;
        startX += (freeSpace / count) / 2;
        break;

    case JustifyContent::Evenly:
        spacing += freeSpace / (count + 1);
        startX += freeSpace / (count + 1);
        break;

    default:
        break;
    }

    int currentX = startX;

    for (int i = 0; i < count; ++i) {
        Element& child = *e.children[i];

        int itemHeight = heights[i];
        int offset = 0;

        if (e.alignItems == AlignItems::Center)
            offset = (innerHeight - itemHeight) / 2;
        else if (e.alignItems == AlignItems::End)
            offset = innerHeight - itemHeight;

        if (offset < 0)
            offset = 0;

        layout_element(
            child,
            hdc,
            currentX,
            contentY + offset,
            widths[i],
            availableHeight,
            widths[i]
        );

        if (e.alignItems == AlignItems::Stretch &&
            itemHeight < innerHeight) {
            child.rect.bottom = contentY + innerHeight;
        }

        currentX += widths[i] + spacing;
    }

    return innerHeight;
}

static int layout_flex_column(
    Element& e,
    HDC hdc,
    int contentX,
    int contentY,
    int innerWidth,
    int availableHeight,
    int fixedInnerHeight
) {
    const int count = (int)e.children.size();

    if (count == 0)
        return fixedInnerHeight >= 0 ? fixedInnerHeight : 0;

    std::vector<int> widths(count, 0);
    std::vector<int> heights(count, 0);

    int growCount = 0;
    int nonGrowHeight = 0;

    for (int i = 0; i < count; ++i) {
        Element& child = *e.children[i];

        int assignedWidth = innerWidth;

        if (child.widthPercent > 0)
            assignedWidth = max(
                0,
                innerWidth * child.widthPercent / 100
            );
        else if (child.width >= 0)
            assignedWidth = min(child.width, innerWidth);
        else if (e.alignItems != AlignItems::Stretch)
            assignedWidth = intrinsic_width(
                child, hdc, innerWidth
            );

        widths[i] = assignedWidth;

        if (child.grow) {
            ++growCount;
            continue;
        }

        layout_element(
            child,
            hdc,
            0,
            0,
            assignedWidth,
            availableHeight,
            assignedWidth
        );

        heights[i] = max(
            0,
            (int)(child.rect.bottom + child.marginBottom)
        );

        nonGrowHeight += heights[i];
    }

    const int baseGaps = e.gap * (count - 1);
    int innerHeight = fixedInnerHeight;

    if (innerHeight >= 0 && growCount > 0) {
        int freeForGrow =
            innerHeight - nonGrowHeight - baseGaps;

        int each = freeForGrow > 0
            ? freeForGrow / growCount
            : 0;

        for (int i = 0; i < count; ++i) {
            if (!e.children[i]->grow)
                continue;

            heights[i] = each;

            layout_element(
                *e.children[i],
                hdc,
                0,
                0,
                widths[i],
                each,
                widths[i]
            );
        }
    }
    else if (innerHeight < 0) {
        innerHeight = nonGrowHeight + baseGaps;

        for (int i = 0; i < count; ++i) {
            if (!e.children[i]->grow)
                continue;

            layout_element(
                *e.children[i],
                hdc,
                0,
                0,
                widths[i],
                availableHeight,
                widths[i]
            );

            heights[i] = max(
                0,
                (int)(e.children[i]->rect.bottom +
                      e.children[i]->marginBottom)
            );

            innerHeight += heights[i];
        }
    }

    int sumHeights = 0;

    for (int height : heights)
        sumHeights += height;

    int freeSpace = 0;
    int spacing = e.gap;
    int startY = contentY;

    if (fixedInnerHeight >= 0) {
        freeSpace = max(
            0,
            innerHeight - sumHeights - baseGaps
        );

        switch (e.justifyContent) {
        case JustifyContent::Center:
            startY += freeSpace / 2;
            break;

        case JustifyContent::End:
            startY += freeSpace;
            break;

        case JustifyContent::Between:
            if (count > 1)
                spacing += freeSpace / (count - 1);
            break;

        case JustifyContent::Around:
            spacing += freeSpace / count;
            startY += (freeSpace / count) / 2;
            break;

        case JustifyContent::Evenly:
            spacing += freeSpace / (count + 1);
            startY += freeSpace / (count + 1);
            break;

        default:
            break;
        }
    }

    int currentY = startY;

    for (int i = 0; i < count; ++i) {
        Element& child = *e.children[i];

        int childX = contentX;

        if (e.alignItems == AlignItems::Center) {
            childX += (innerWidth - widths[i]) / 2;
        }
        else if (e.alignItems == AlignItems::End) {
            childX += innerWidth - widths[i];
        }

        layout_element(
            child,
            hdc,
            childX,
            currentY,
            widths[i],
            availableHeight,
            widths[i]
        );

        currentY += heights[i] + spacing;
    }

    if (fixedInnerHeight >= 0)
        return fixedInnerHeight;

    int contentHeight = currentY - spacing - contentY;

    return max(0, contentHeight);
}

static int layout_element(
    Element& e,
    HDC hdc,
    int x,
    int y,
    int availableWidth,
    int availableHeight,
    int forcedWidth
) {
    int top = y + e.marginTop;
    int width = availableWidth;

    if (forcedWidth >= 0)
        width = forcedWidth;
    else if (e.widthPercent > 0)
        width = max(0, availableWidth * e.widthPercent / 100);
    else if (e.width >= 0)
        width = min(e.width, availableWidth);

    int contentX = x + e.padX;
    int contentY = top + e.padY;

    int innerWidth = max(1, width - e.padX * 2);

    int fixedInnerHeight = -1;

    if (e.fullHeight) {
        int targetOuter = max(
            0,
            availableHeight - e.marginTop - e.marginBottom
        );

        fixedInnerHeight = max(
            0,
            targetOuter - e.padY * 2
        );
    }

    HFONT font = make_ui_font(e.fontSize);
    HFONT old = (HFONT)SelectObject(hdc, font);

    int contentHeight = 0;

    if (e.tag == L"input") {
        TEXTMETRICW tm{};
        GetTextMetricsW(hdc, &tm);

        contentHeight = max(tm.tmHeight, e.fontSize + 4);
    }
    else if (e.tag == L"button") {
        SIZE size{};

        GetTextExtentPoint32W(
            hdc,
            e.text.c_str(),
            (int)e.text.size(),
            &size
        );

        int naturalWidth = (int)size.cx + e.padX * 2;

        if (forcedWidth < 0 &&
            e.width < 0 &&
            e.widthPercent == 0) {
            width = min(availableWidth, naturalWidth);
        }

        if (e.grow && forcedWidth >= 0) {
            contentHeight = max(
                (int)size.cy,
                max(0, width - e.padY * 2)
            );
        }
        else {
            contentHeight = (int)size.cy;
        }
    }
    else if (e.tag == L"p") {
        int wrapWidth = max(1, width - e.padX * 2);

        e.wrappedText = break_long_words(
            hdc, e.text, wrapWidth
        );

        RECT r = { 0, 0, wrapWidth, 0 };

        DrawTextW(
            hdc,
            e.wrappedText.c_str(),
            -1,
            &r,
            DT_CALCRECT | DT_WORDBREAK
        );

        contentHeight = r.bottom;
    }
    else {
        if (e.flex) {
            if (e.flexDirection == FlexDirection::Row) {
                contentHeight = layout_flex_row(
                    e,
                    hdc,
                    contentX,
                    contentY,
                    innerWidth,
                    availableHeight,
                    fixedInnerHeight
                );
            }
            else {
                contentHeight = layout_flex_column(
                    e,
                    hdc,
                    contentX,
                    contentY,
                    innerWidth,
                    availableHeight,
                    fixedInnerHeight
                );
            }
        }
        else {
            int currentY = contentY;

            for (auto& child : e.children) {
                int remainingHeight = max(
                    0,
                    availableHeight - (currentY - y)
                );

                currentY = layout_element(
                    *child,
                    hdc,
                    contentX,
                    currentY,
                    innerWidth,
                    remainingHeight
                );
            }

            contentHeight = currentY - contentY;

            if (fixedInnerHeight >= 0)
                contentHeight = fixedInnerHeight;
        }
    }

    SelectObject(hdc, old);
    DeleteObject(font);

    if (e.tag == L"button")
        contentHeight = max(contentHeight, e.fontSize + 4);

    int height = contentHeight + e.padY * 2;

    if (e.fullHeight)
        height = fixedInnerHeight + e.padY * 2;

    e.rect = {
        x,
        top,
        x + width,
        top + height
    };

    return e.rect.bottom + e.marginBottom;
}

// --------------------------------------------------
// Drawing
// --------------------------------------------------

static void draw_element(
    HDC hdc,
    Element& e
) {
    if (e.hasBg || e.tag == L"input") {

        COLORREF bg = e.hasBg ? e.bg : RGB(255, 255, 255);

        HBRUSH brush = CreateSolidBrush(bg);

        COLORREF borderColor = bg;
        int borderWidth = 1;

        if (e.tag == L"input") {
            borderColor = e.focused
                ? RGB(37, 99, 235)
                : RGB(209, 213, 219);

            borderWidth = e.focused ? 2 : 1;
        }

        HPEN pen = CreatePen(PS_SOLID, borderWidth, borderColor);

        HGDIOBJ oldBrush = SelectObject(hdc, brush);
        HGDIOBJ oldPen = SelectObject(hdc, pen);

        if (e.radius > 0) {
            RoundRect(
                hdc,
                e.rect.left,
                e.rect.top,
                e.rect.right,
                e.rect.bottom,
                e.radius,
                e.radius
            );
        }
        else {
            FillRect(hdc, &e.rect, brush);
        }

        SelectObject(hdc, oldBrush);
        SelectObject(hdc, oldPen);

        DeleteObject(brush);
        DeleteObject(pen);
    }

    HFONT font = make_ui_font(e.fontSize);
    HFONT oldFont = (HFONT)SelectObject(hdc, font);

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, e.color);

    RECT textRect = e.rect;

    textRect.left += e.padX;
    textRect.right -= e.padX;
    textRect.top += e.padY;
    textRect.bottom -= e.padY;

    if (e.tag == L"input") {

        bool showingPlaceholder = e.text.empty();

        const std::wstring& shown =
            showingPlaceholder ? e.placeholder : e.text;

        int caret = e.caret;

        if (caret < 0)
            caret = 0;

        if (caret > (int)e.text.size())
            caret = (int)e.text.size();

        SIZE prefixSize{};

        if (caret > 0) {
            GetTextExtentPoint32W(
                hdc,
                e.text.c_str(),
                caret,
                &prefixSize
            );
        }

        int visibleWidth = textRect.right - textRect.left;

        // Keep the caret inside the visible area by scrolling the text.
        if (!showingPlaceholder && visibleWidth > 0) {

            if (prefixSize.cx - e.scrollX > visibleWidth)
                e.scrollX = prefixSize.cx - visibleWidth;

            if (prefixSize.cx - e.scrollX < 0)
                e.scrollX = prefixSize.cx;
        }
        else if (showingPlaceholder) {
            e.scrollX = 0;
        }

        if (e.scrollX < 0)
            e.scrollX = 0;

        if (showingPlaceholder)
            SetTextColor(hdc, RGB(156, 163, 175));

        if (!shown.empty()) {

            // Clip horizontally so scrolled text cannot paint over padding.
            int saved = SaveDC(hdc);

            IntersectClipRect(
                hdc,
                textRect.left,
                textRect.top,
                textRect.right,
                textRect.bottom
            );

            RECT drawRect = textRect;

            drawRect.left -= e.scrollX;

            DrawTextW(
                hdc,
                shown.c_str(),
                -1,
                &drawRect,
                DT_SINGLELINE | DT_VCENTER |
                DT_LEFT | DT_NOPREFIX
            );

            RestoreDC(hdc, saved);
        }

        // Caret
        if (e.focused && e.caretVisible) {

            int cx = textRect.left - e.scrollX + prefixSize.cx;

            if (cx < textRect.left)
                cx = textRect.left;

            if (cx > textRect.right - 1)
                cx = textRect.right - 1;

            TEXTMETRICW tm{};
            GetTextMetricsW(hdc, &tm);

            int centerY =
                (textRect.top + textRect.bottom) / 2;

            int topY = centerY - tm.tmHeight / 2;
            int bottomY = topY + tm.tmHeight;

            HPEN caretPen = CreatePen(
                PS_SOLID, 1, RGB(17, 24, 39)
            );

            HGDIOBJ oldPen = SelectObject(hdc, caretPen);

            MoveToEx(hdc, cx, topY, nullptr);
            LineTo(hdc, cx, bottomY);

            SelectObject(hdc, oldPen);
            DeleteObject(caretPen);
        }
    }
    else if (!e.text.empty()) {

        UINT flags = DT_WORDBREAK | DT_NOPREFIX;

        if (e.tag == L"button")
            flags = DT_SINGLELINE | DT_CENTER | DT_VCENTER;

        const std::wstring& shown =
            (e.tag == L"p" && !e.wrappedText.empty())
                ? e.wrappedText
                : e.text;

        DrawTextW(
            hdc,
            shown.c_str(),
            -1,
            &textRect,
            flags
        );
    }

    SelectObject(hdc, oldFont);
    DeleteObject(font);

    for (auto& child : e.children)
        draw_element(hdc, *child);
}

// --------------------------------------------------
// Public renderer
// --------------------------------------------------

void render_html(
    HDC hdc,
    Element& root,
    const RECT& area
) {
    layout_element(
        root,
        hdc,
        area.left,
        area.top,
        area.right - area.left,
        area.bottom - area.top
    );

    draw_element(hdc, root);
}

// --------------------------------------------------
// Mouse hit testing
// --------------------------------------------------

Element* hit_test(
    Element& root,
    int x,
    int y
) {
    for (auto it = root.children.rbegin();
         it != root.children.rend(); ++it) {

        Element* result = hit_test(**it, x, y);

        if (result)
            return result;
    }

    if ((root.tag == L"button" ||
         root.tag == L"input") &&
        x >= root.rect.left &&
        x <= root.rect.right &&
        y >= root.rect.top &&
        y <= root.rect.bottom) {

        return &root;
    }

    return nullptr;
}
