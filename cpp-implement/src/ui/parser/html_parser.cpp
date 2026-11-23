#include "parser/html_parser.h"

#include "core/utf.h"

#include <algorithm>
#include <cwctype>

namespace ui {

namespace {

bool isSpace(wchar_t c) {
    return c == L' ' || c == L'\t' || c == L'\r' || c == L'\n' || c == L'\f';
}

size_t findCaseInsensitive(const std::wstring& haystack, const std::wstring& needle, size_t start) {
    if (needle.empty())
        return start;

    for (size_t i = start; i + needle.size() <= haystack.size(); ++i) {
        bool match = true;

        for (size_t k = 0; k < needle.size(); ++k) {
            if (towlower(haystack[i + k]) != towlower(needle[k])) {
                match = false;
                break;
            }
        }

        if (match)
            return i;
    }

    return std::wstring::npos;
}

std::wstring tagNameFromInner(const std::wstring& inner) {
    std::wstring s = trim(inner);

    if (!s.empty() && s[0] == L'/')
        s.erase(s.begin());

    size_t end = 0;

    while (end < s.size() && !isSpace(s[end]) && s[end] != L'/')
        ++end;

    return toLower(s.substr(0, end));
}

void removeWhitespaceOnlyText(Element& el) {
    for (auto it = el.children.begin(); it != el.children.end();) {
        Node* node = it->get();

        if (node->kind == Node::Kind::Text) {
            const std::wstring& text = static_cast<TextNode*>(node)->text;
            bool onlySpace = std::all_of(text.begin(), text.end(), isSpace);

            if (onlySpace) {
                it = el.children.erase(it);
                continue;
            }
        }
        else {
            removeWhitespaceOnlyText(*static_cast<Element*>(node));
        }

        ++it;
    }
}

} // namespace

bool isVoidTag(const std::wstring& tag) {
    return tag == L"input" || tag == L"br" || tag == L"img" ||
           tag == L"hr" || tag == L"meta" || tag == L"link" ||
           tag == L"path" || tag == L"area" || tag == L"base" ||
           tag == L"col" || tag == L"embed" || tag == L"source" ||
           tag == L"track" || tag == L"wbr";
}

bool isRawTextTag(const std::wstring& tag) {
    return tag == L"style" || tag == L"script" || tag == L"title";
}

size_t findTagEnd(const std::wstring& html, size_t pos) {
    if (pos >= html.size())
        return std::wstring::npos;

    wchar_t quote = 0;

    for (size_t i = pos + 1; i < html.size(); ++i) {
        wchar_t c = html[i];

        if (quote) {
            if (c == quote)
                quote = 0;
        }
        else if (c == L'"' || c == L'\'') {
            quote = c;
        }
        else if (c == L'>') {
            return i;
        }
    }

    return std::wstring::npos;
}

std::vector<std::pair<std::wstring, std::wstring>> parseAttributes(const std::wstring& tagInner) {
    std::vector<std::pair<std::wstring, std::wstring>> attrs;

    size_t i = 0;

    // Skip the tag name.
    while (i < tagInner.size() && !isSpace(tagInner[i]) && tagInner[i] != L'/')
        ++i;

    while (i < tagInner.size()) {
        while (i < tagInner.size() && isSpace(tagInner[i]))
            ++i;

        if (i >= tagInner.size() || tagInner[i] == L'/')
            break;

        size_t nameStart = i;

        while (i < tagInner.size() && !isSpace(tagInner[i]) &&
               tagInner[i] != L'=' && tagInner[i] != L'/')
            ++i;

        std::wstring name = toLower(tagInner.substr(nameStart, i - nameStart));

        while (i < tagInner.size() && isSpace(tagInner[i]))
            ++i;

        std::wstring value;

        if (i < tagInner.size() && tagInner[i] == L'=') {
            ++i;

            while (i < tagInner.size() && isSpace(tagInner[i]))
                ++i;

            if (i < tagInner.size() && (tagInner[i] == L'"' || tagInner[i] == L'\'')) {
                wchar_t quote = tagInner[i++];
                size_t valueStart = i;

                while (i < tagInner.size() && tagInner[i] != quote)
                    ++i;

                value = tagInner.substr(valueStart, i - valueStart);

                if (i < tagInner.size())
                    ++i;
            }
            else {
                size_t valueStart = i;

                while (i < tagInner.size() && !isSpace(tagInner[i]) &&
                       tagInner[i] != L'/')
                    ++i;

                value = tagInner.substr(valueStart, i - valueStart);
            }
        }

        if (!name.empty())
            attrs.emplace_back(std::move(name), decodeEntities(value));
    }

    return attrs;
}

std::unique_ptr<Element> parseHtml(const std::wstring& html) {
    auto root = std::make_unique<Element>();
    root->tag = L"root";

    std::vector<Element*> stack;
    stack.push_back(root.get());

    size_t pos = 0;

    while (pos < html.size()) {
        if (html[pos] != L'<') {
            size_t end = html.find(L'<', pos);

            if (end == std::wstring::npos)
                end = html.size();

            std::wstring text = decodeEntities(html.substr(pos, end - pos));

            if (!text.empty()) {
                auto node = std::make_unique<TextNode>();
                node->text = text;
                node->parent = stack.back();
                stack.back()->children.push_back(std::move(node));
            }

            pos = end;
            continue;
        }

        // Comments end at "-->" not at the first '>'.
        if (html.compare(pos, 4, L"<!--") == 0) {
            size_t end = html.find(L"-->", pos + 4);
            pos = end == std::wstring::npos ? html.size() : end + 3;
            continue;
        }

        size_t end = findTagEnd(html, pos);

        if (end == std::wstring::npos)
            break;

        std::wstring inner = trim(html.substr(pos + 1, end - pos - 1));
        pos = end + 1;

        if (inner.empty())
            continue;

        if (inner[0] == L'!' || inner[0] == L'?')
            continue;

        bool closing = inner[0] == L'/';
        std::wstring tag = tagNameFromInner(inner);

        if (tag.empty())
            continue;

        if (closing) {
            for (size_t i = stack.size(); i > 1; --i) {
                if (stack[i - 1]->tag == tag) {
                    stack.resize(i - 1);
                    break;
                }
            }

            continue;
        }

        // The document head contributes no visual content.
        if (tag == L"head") {
            size_t headEnd = findCaseInsensitive(html, L"</head", pos);

            if (headEnd == std::wstring::npos) {
                pos = html.size();
            }
            else {
                size_t closeEnd = findTagEnd(html, headEnd);
                pos = closeEnd == std::wstring::npos ? html.size() : closeEnd + 1;
            }

            continue;
        }

        if (isRawTextTag(tag)) {
            std::wstring closeTag = L"</" + tag;
            size_t rawEnd = findCaseInsensitive(html, closeTag, pos);

            if (rawEnd == std::wstring::npos) {
                pos = html.size();
            }
            else {
                size_t closeEnd = findTagEnd(html, rawEnd);
                pos = closeEnd == std::wstring::npos ? html.size() : closeEnd + 1;
            }

            continue;
        }

        auto element = std::make_unique<Element>();
        element->tag = tag;

        size_t nameEnd = 0;
        std::wstring trimmedInner = trim(inner);

        while (nameEnd < trimmedInner.size() &&
               !isSpace(trimmedInner[nameEnd]) &&
               trimmedInner[nameEnd] != L'/')
            ++nameEnd;

        element->attrs = parseAttributes(trimmedInner);

        for (const auto& attr : element->attrs) {
            if (attr.first == L"id")
                element->id = attr.second;
            else if (attr.first == L"class")
                element->classList = splitWhitespace(attr.second);
        }

        bool selfClosing = !inner.empty() && inner.back() == L'/';

        Element* raw = element.get();
        raw->parent = stack.back();
        stack.back()->children.push_back(std::move(element));

        if (!selfClosing && !isVoidTag(tag))
            stack.push_back(raw);
    }

    removeWhitespaceOnlyText(*root);

    return root;
}

} // namespace ui
