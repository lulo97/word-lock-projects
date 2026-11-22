#include "app/document.h"

#include "app/bindings.h"
#include "controls/checkbox.h"
#include "controls/range_input.h"
#include "controls/text_area.h"
#include "controls/text_input.h"
#include "core/utf.h"
#include "dom/tree_ops.h"
#include "layout/layout.h"
#include "paint/canvas.h"
#include "paint/paint.h"
#include "parser/html_parser.h"
#include "style/resolve.h"
#include "style/variants.h"

#include "json.hpp"

#include <fstream>
#include <windowsx.h>

#include <algorithm>

namespace ui {

namespace {

bool nodeContains(const Node* ancestor, const Node* node) {
    const Node* current = node;

    while (current) {
        if (current == ancestor)
            return true;

        current = current->parent;
    }

    return false;
}

void clearEventPointersInside(EventState& events, const Node* node) {
    if (!node)
        return;

    if (events.hovered && nodeContains(node, events.hovered))
        events.hovered = nullptr;

    if (events.focused && nodeContains(node, events.focused))
        events.setFocus(nullptr);

    if (events.active && nodeContains(node, events.active))
        events.active = nullptr;

    if (events.captured && nodeContains(node, events.captured))
        events.captured = nullptr;
}

void initializeControl(Element& el) {
    if (!el.control && (el.tag == L"input" || el.tag == L"textarea" ||
                        el.tag == L"button")) {
        el.control = createControlFor(el);

        if (auto* input = dynamic_cast<TextInput*>(el.control.get()))
            input->init(el);
        else if (auto* area = dynamic_cast<TextArea*>(el.control.get()))
            area->init(el);
        else if (auto* range = dynamic_cast<RangeInput*>(el.control.get()))
            range->init(el);
        else if (auto* check = dynamic_cast<Checkbox*>(el.control.get()))
            check->init(el);
    }
}

} // namespace

Document::Document() {
    events_.onClickAny = [this](Element& el) {
        Element* current = &el;

        while (current) {
            if (!current->id.empty()) {
                auto it = clickHandlers_.find(current->id);

                if (it != clickHandlers_.end()) {
                    it->second();
                    return;
                }
            }

            current = dynamic_cast<Element*>(current->parent);
        }
    };
}

Document::~Document() = default;

bool Document::load(const std::wstring& html) {
    root_ = parseHtml(html);

    if (!root_)
        return false;

    events_ = EventState();
    events_.onClickAny = [this](Element& el) {
        Element* current = &el;

        while (current) {
            if (!current->id.empty()) {
                auto it = clickHandlers_.find(current->id);

                if (it != clickHandlers_.end()) {
                    it->second();
                    return;
                }
            }

            current = dynamic_cast<Element*>(current->parent);
        }
    };

    walk(*root_, [&](Element& el) {
        initializeControl(el);
        return true;
    });

    // Wire control callbacks to Document::onInput / onChange by id.
    walk(*root_, [&](Element& el) {
        if (auto* input = dynamic_cast<TextInput*>(el.control.get())) {
            std::wstring id = el.id;
            input->onChange = [this, id](const std::wstring& value) {
                auto it = inputHandlers_.find(id);

                if (it != inputHandlers_.end())
                    it->second(value);

                paintDirty_ = true;
            };
        }
        else if (auto* area = dynamic_cast<TextArea*>(el.control.get())) {
            std::wstring id = el.id;
            area->onChange = [this, id](const std::wstring& value) {
                auto it = inputHandlers_.find(id);

                if (it != inputHandlers_.end())
                    it->second(value);

                layoutDirty_ = true;
                paintDirty_ = true;
            };
        }
        else if (auto* range = dynamic_cast<RangeInput*>(el.control.get())) {
            std::wstring id = el.id;
            range->onChange = [this, id](float value) {
                auto it = changeHandlers_.find(id);

                if (it != changeHandlers_.end())
                    it->second(value);

                paintDirty_ = true;
            };
        }

        return true;
    });

    // Focus the first autofocus control.
    walk(*root_, [&](Element& el) {
        if (el.hasAttr(L"autofocus") && el.control) {
            events_.setFocus(&el);
            return false;
        }

        return true;
    });

    styleDirty_ = true;
    layoutDirty_ = true;
    paintDirty_ = true;
    return true;
}

bool Document::loadFile(const std::wstring& path) {
    std::ifstream file(path, std::ios::binary);

    if (!file)
        return false;

    std::string bytes(
        (std::istreambuf_iterator<char>(file)),
        std::istreambuf_iterator<char>()
    );

    return load(utf8ToWide(bytes));
}

Element* Document::byId(const std::wstring& id) {
    return root_ ? findById(*root_, id) : nullptr;
}

void Document::setText(const std::wstring& id, const std::wstring& text) {
    Element* el = byId(id);

    if (!el)
        return;

    clearEventPointersInside(events_, el);
    ui::clearChildren(*el);
    auto node = std::make_unique<TextNode>();
    node->text = text;
    node->parent = el;
    el->children.push_back(std::move(node));

    markDirty(*el, true, true);
    styleDirty_ = true;
    layoutDirty_ = true;
    paintDirty_ = true;
}

void Document::setClass(const std::wstring& id, const std::wstring& classes) {
    Element* el = byId(id);

    if (!el)
        return;

    el->classList = splitWhitespace(classes);
    markDirty(*el, true, true);
    styleDirty_ = true;
    layoutDirty_ = true;
    paintDirty_ = true;
}

void Document::setValue(const std::wstring& id, const std::wstring& value) {
    Element* el = byId(id);

    if (!el)
        return;

    if (auto* input = dynamic_cast<TextInput*>(el->control.get())) {
        input->setValue(value);
        input->scrollX = 0;
    }
    else if (auto* area = dynamic_cast<TextArea*>(el->control.get())) {
        area->setValue(value);
        area->scrollY = 0;
    }
    else if (auto* range = dynamic_cast<RangeInput*>(el->control.get())) {
        float parsed = 0;

        if (parseFloat(value, parsed))
            range->setValue(parsed);
    }
    else {
        for (auto& attr : el->attrs) {
            if (attr.first == L"value") {
                attr.second = value;
                break;
            }
        }
    }

    layoutDirty_ = true;
    paintDirty_ = true;
}

std::wstring Document::getValue(const std::wstring& id) const {
    if (!root_)
        return L"";

    Element* el = findById(*root_, id);

    if (!el)
        return L"";

    if (auto* input = dynamic_cast<TextInput*>(el->control.get()))
        return input->value;

    if (auto* area = dynamic_cast<TextArea*>(el->control.get()))
        return area->value;

    if (auto* range = dynamic_cast<RangeInput*>(el->control.get())) {
        wchar_t buffer[64]{};
        swprintf_s(buffer, L"%.2f", range->value);
        return buffer;
    }

    return el->attr(L"value");
}

void Document::setChecked(const std::wstring& id, bool checked) {
    Element* el = byId(id);

    if (!el)
        return;

    if (auto* check = dynamic_cast<Checkbox*>(el->control.get()))
        check->setChecked(checked);

    paintDirty_ = true;
}

void Document::setVisible(const std::wstring& id, bool visible) {
    Element* el = byId(id);

    if (!el)
        return;

    auto it = std::find_if(
        el->attrs.begin(), el->attrs.end(),
        [](const std::pair<std::wstring, std::wstring>& attr) {
            return attr.first == L"hidden";
        }
    );

    if (visible) {
        if (it != el->attrs.end())
            el->attrs.erase(it);
    }
    else if (it == el->attrs.end()) {
        el->attrs.emplace_back(L"hidden", L"");
    }

    markDirty(*el, true, true);
    styleDirty_ = true;
    layoutDirty_ = true;
    paintDirty_ = true;
}

void Document::onClick(const std::wstring& id, std::function<void()> fn) {
    clickHandlers_[id] = std::move(fn);

    if (Element* el = byId(id))
        bindClick(*el, [this, id] {
            auto it = clickHandlers_.find(id);

            if (it != clickHandlers_.end())
                it->second();
        });
}

void Document::onInput(
    const std::wstring& id,
    std::function<void(const std::wstring&)> fn
) {
    inputHandlers_[id] = std::move(fn);
}

void Document::onChange(const std::wstring& id, std::function<void(float)> fn) {
    changeHandlers_[id] = std::move(fn);
}

Element* Document::cloneTemplate(
    const std::wstring& templateId,
    const std::wstring& parentId,
    const std::unordered_map<std::wstring, std::wstring>& values
) {
    Element* tpl = byId(templateId);
    Element* parent = byId(parentId);

    if (!tpl || !parent)
        return nullptr;

    std::vector<std::unique_ptr<Node>> nodes = instantiateTemplate(*tpl, values);
    Element* first = nullptr;

    for (auto& node : nodes) {
        Element* element = dynamic_cast<Element*>(node.get());

        if (element) {
            if (!first)
                first = element;

            initializeControl(*element);
        }

        appendChild(*parent, std::move(node));
    }

    styleDirty_ = true;
    layoutDirty_ = true;
    paintDirty_ = true;
    return first;
}

void Document::clearChildren(const std::wstring& parentId) {
    Element* parent = byId(parentId);

    if (!parent)
        return;

    clearEventPointersInside(events_, parent);
    ui::clearChildren(*parent);
    layoutDirty_ = true;
    paintDirty_ = true;
}

SIZE Document::measureContent(int maxWidth, float scale) {
    if (!root_)
        return { 0, 0 };

    ResolveContext ctx;
    ctx.viewportWidth = static_cast<int>(maxWidth / std::max(0.1f, scale));
    ctx.viewportHeight = 10000;
    resolveTree(*root_, ctx, true);

    layoutDocument(
        *root_,
        static_cast<float>(maxWidth),
        10000.f * scale,
        scale
    );

    Element* body = nullptr;
    std::vector<Element*> bodies = findByTag(*root_, L"body");

    if (!bodies.empty())
        body = bodies.front();

    if (body) {
        // If the body has an explicit width/height, size to the body itself.
        if (body->style.width.unit == Length::Unit::Px &&
            body->style.height.unit == Length::Unit::Px) {
            return {
                static_cast<LONG>(body->box.borderRect.w),
                static_cast<LONG>(body->box.borderRect.h)
            };
        }

        // Sum all visible children to get total content size.
        float padX = (body->style.padding.left + body->style.padding.right) * scale;
        float padY = (body->style.padding.top + body->style.padding.bottom) * scale;
        float totalW = 0, totalH = 0;
        bool any = false;

        for (auto& childPtr : body->children) {
            Element* content = dynamic_cast<Element*>(childPtr.get());

            if (!content || content->style.display == Display::None)
                continue;

            totalW = std::max(totalW, content->box.borderRect.w);
            totalH += content->box.borderRect.h;
            any = true;
        }

        if (any) {
            return {
                static_cast<LONG>(totalW + padX),
                static_cast<LONG>(totalH + padY)
            };
        }
    }

    return {
        static_cast<LONG>(root_->box.borderRect.w),
        static_cast<LONG>(root_->box.borderRect.h)
    };
}

void Document::invalidateStyle() {
    if (!root_)
        return;

    walk(*root_, [](Element& el) {
        el.styleDirty = true;
        el.layoutDirty = true;
        return true;
    });

    styleDirty_ = true;
    layoutDirty_ = true;
    paintDirty_ = true;
}

void Document::invalidateLayout() {
    if (!root_)
        return;

    walk(*root_, [](Element& el) {
        el.layoutDirty = true;
        return true;
    });

    layoutDirty_ = true;
    paintDirty_ = true;
}

void Document::invalidatePaint() {
    paintDirty_ = true;
}

bool Document::needsPaint() const {
    return paintDirty_ || layoutDirty_ || styleDirty_;
}

void Document::paint(HDC hdc, const RECT& client, float scale) {
    if (!root_)
        return;

    int width = client.right - client.left;
    int height = client.bottom - client.top;

    if (width <= 0 || height <= 0)
        return;

    scale_ = std::max(0.1f, scale);

    if (viewportW_ != width || viewportH_ != height) {
        viewportW_ = width;
        viewportH_ = height;
        layoutDirty_ = true;
    }

    if (styleDirty_) {
        ResolveContext ctx;
        ctx.viewportWidth = static_cast<int>(width / scale_);
        ctx.viewportHeight = static_cast<int>(height / scale_);
        resolveTree(*root_, ctx, true);
        styleDirty_ = false;
        layoutDirty_ = true;
    }

    if (layoutDirty_) {
        layoutDocument(*root_, static_cast<float>(width), static_cast<float>(height), scale_);
        layoutDirty_ = false;
    }

    RECT fill = client;
    FillRect(hdc, &fill, static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH)));

    Canvas canvas(hdc, width, height);
    paintDocument(canvas, *root_, scale_);
    paintDirty_ = false;
}

bool Document::handleMessage(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, LRESULT& outResult) {
    outResult = 0;

    if (!root_)
        return false;

    switch (msg) {
    case WM_MOUSEMOVE: {
        float x = static_cast<float>(GET_X_LPARAM(lp));
        float y = static_cast<float>(GET_Y_LPARAM(lp));
        bool changed = events_.onMouseMove(*root_, x, y, (wp & MK_LBUTTON) != 0);

        if (changed) {
            styleDirty_ = true;
            paintDirty_ = true;
        }

        return true;
    }

    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP: {
        float x = static_cast<float>(GET_X_LPARAM(lp));
        float y = static_cast<float>(GET_Y_LPARAM(lp));
        bool down = msg == WM_LBUTTONDOWN;
        bool changed = down
            ? events_.onMouseDown(*root_, x, y)
            : events_.onMouseUp(*root_, x, y);

        styleDirty_ = true;
        paintDirty_ = true;
        (void)changed;
        return true;
    }

    case WM_MOUSEWHEEL: {
        POINT point { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
        ScreenToClient(hwnd, &point);

        if (events_.onWheel(*root_, static_cast<float>(point.x),
                            static_cast<float>(point.y),
                            GET_WHEEL_DELTA_WPARAM(wp))) {
            paintDirty_ = true;
        }

        return true;
    }

    case WM_KEYDOWN: {
        KeyEvent event;
        event.vk = static_cast<UINT>(wp);
        event.shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
        event.ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;

        if (events_.onKeyDown(*root_, event)) {
            styleDirty_ = true;
            paintDirty_ = true;
            return true;
        }

        return false;
    }

    case WM_CHAR:
        if (events_.onChar(*root_, static_cast<wchar_t>(wp))) {
            paintDirty_ = true;
            return true;
        }
        return false;

    case WM_MOUSELEAVE:
        events_.onMouseMove(*root_, -1000000.f, -1000000.f, false);
        styleDirty_ = true;
        paintDirty_ = true;
        return true;

    case WM_SIZE:
        viewportW_ = LOWORD(lp);
        viewportH_ = HIWORD(lp);
        layoutDirty_ = true;
        paintDirty_ = true;
        return true;

    case WM_DPICHANGED:
        scale_ = static_cast<float>(HIWORD(wp)) / 96.0f;
        layoutDirty_ = true;
        paintDirty_ = true;
        return true;

    default:
        return false;
    }
}

bool Document::tickBlink() {
    if (events_.focused && events_.focused->control) {
        if (events_.focused->control->onBlinkTick()) {
            paintDirty_ = true;
            return true;
        }
    }

    return false;
}

namespace {

std::string hex(Color c) {
    char b[16];
    sprintf_s(b, "#%02x%02x%02x%02x", c.r, c.g, c.b, c.a);
    return b;
}

bool isDescendant(const Element* anc, const Element* e) {
    for (; e; e = dynamic_cast<const Element*>(e->parent))
        if (e == anc) return true;
    return false;
}

nlohmann::json toJson(Element& el, EventState& ev, Element& root, int depth, int maxDepth) {
    using J = nlohmann::json;
    J j;
    j["tag"] = wideToUtf8(el.tag);
    if (!el.id.empty()) j["id"] = wideToUtf8(el.id);

    J cls = J::array();
    for (auto& c : el.classList) cls.push_back(wideToUtf8(c));
    j["class"] = cls;

    J issues = J::array();
    for (auto& i : findClassIssues(el.classList))
        issues.push_back({ {"token", wideToUtf8(i.token)}, {"reason", wideToUtf8(i.reason)} });
    if (!issues.empty()) j["classIssues"] = issues;

    const RectF& r = el.box.borderRect;
    j["rect"] = { r.x, r.y, r.w, r.h };
    j["scroll"] = { el.box.scrollX, el.box.scrollY, el.box.maxScrollY() };

    const ComputedStyle& s = el.style;
    j["style"] = {
        {"display", (int)s.display}, {"position", (int)s.position},
        {"dir", (int)s.dir}, {"justify", (int)s.justify}, {"align", (int)s.align},
        {"grow", s.grow}, {"shrink", s.shrink}, {"gap", { s.gapX, s.gapY }},
        {"color", hex(s.color)}, {"bg", hex(s.background)},
        {"fontSize", s.fontSize}, {"opacity", s.opacity}, {"z", s.zIndex},
        {"overflow", { (int)s.overflowX, (int)s.overflowY }},
        {"pointerEvents", s.pointerEvents}
    };

    j["state"] = {
        {"hover", (bool)(el.stateFlags & StateHover)},
        {"focus", (bool)(el.stateFlags & StateFocus)},
        {"styleDirty", el.styleDirty}, {"layoutDirty", el.layoutDirty}
    };

    // Why won't it click? Hit-test the element's own center.
    if (s.display != Display::None && r.w > 0 && r.h > 0) {
        Element* hit = EventState::hitTest(root, r.x + r.w / 2, r.y + r.h / 2);
        if (hit && !isDescendant(&el, hit)) {
            j["occludedBy"] = wideToUtf8(hit->tag) +
                (hit->id.empty() ? "" : "#" + wideToUtf8(hit->id));
        } else if (!hit) {
            j["notHittable"] = true;
        }
    }

    if (el.control) {
        J c; c["type"] = typeid(*el.control).name();
        if (auto* t = dynamic_cast<TextInput*>(el.control.get()))
            c["value"] = wideToUtf8(t->value), c["caret"] = t->caret, c["scrollX"] = t->scrollX;
        if (auto* a = dynamic_cast<TextArea*>(el.control.get()))
            c["value"] = wideToUtf8(a->value), c["caret"] = a->caret;
        if (auto* r2 = dynamic_cast<RangeInput*>(el.control.get())) c["value"] = r2->value;
        if (auto* k = dynamic_cast<Checkbox*>(el.control.get())) c["checked"] = k->checked;
        j["control"] = c;
    }

    std::wstring txt;
    for (auto& ch : el.children)
        if (ch->kind == Node::Kind::Text) txt += static_cast<TextNode*>(ch.get())->text;
    if (!txt.empty()) j["text"] = wideToUtf8(txt.substr(0, 80));

    if (depth < maxDepth) {
        J kids = J::array();
        for (auto& ch : el.children)
            if (ch->kind == Node::Kind::Element)
                kids.push_back(toJson(*static_cast<Element*>(ch.get()), ev, root, depth + 1, maxDepth));
        if (!kids.empty()) j["children"] = kids;
    }
    return j;
}

} // namespace

bool Document::dumpTree(const std::wstring& path, int maxDepth) {
    if (!root_)
        return false;

    nlohmann::json out;
    out["header"] = {
        {"viewport", { viewportW_, viewportH_ }}, {"scale", scale_},
        {"dirty", { {"style", styleDirty_}, {"layout", layoutDirty_}, {"paint", paintDirty_} }},
        {"focused", events_.focused ? wideToUtf8(events_.focused->id) : ""},
        {"hovered", events_.hovered ? wideToUtf8(events_.hovered->id) : ""}
    };
    out["tree"] = toJson(*root_, events_, *root_, 0, maxDepth);

    std::ofstream f(path, std::ios::binary);
    if (!f) return false;
    f << out.dump(2);
    return true;
}

} // namespace ui
