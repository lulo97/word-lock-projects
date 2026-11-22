#include "app/events.h"

#include "dom/tree_ops.h"
#include "layout/position.h"
#include "../../utils/debug_log.h"

#include <algorithm>

namespace ui {

namespace {

bool inside(const Element& el, float x, float y) {
    return el.box.borderRect.contains(x, y);
}

Element* hitRecursive(Element& el, float x, float y) {
    if (el.style.display == Display::None || !el.style.pointerEvents)
        return nullptr;

    if (!inside(el, x, y))
        return nullptr;

    if ((el.style.overflowX != Overflow::Visible ||
         el.style.overflowY != Overflow::Visible) &&
        !el.box.paddingRect().contains(x, y))
        return nullptr;

    std::vector<Element*> children = paintOrderChildren(el);
    float childX = x + el.box.scrollX;
    float childY = y + el.box.scrollY;

    for (auto it = children.rbegin(); it != children.rend(); ++it) {
        if (Element* hit = hitRecursive(**it, childX, childY))
            return hit;
    }

    return &el;
}

Element* nearestWithControl(Element* el) {
    Element* current = el;

    while (current) {
        if (current->control)
            return current;

        current = dynamic_cast<Element*>(current->parent);
    }

    return nullptr;
}

Element* interactiveAncestor(Element* el) {
    Element* current = el;

    while (current) {
        if (current->control || current->tag == L"button" ||
            current->tag == L"input" || current->tag == L"textarea" ||
            !current->id.empty())
            return current;

        current = dynamic_cast<Element*>(current->parent);
    }

    return el;
}

void setHoverChain(Element* el, bool hovered) {
    Element* current = el;

    while (current) {
        if (hovered)
            current->stateFlags |= StateHover;
        else
            current->stateFlags &= ~StateHover;

        current = dynamic_cast<Element*>(current->parent);
    }
}

bool containsElement(const Element* ancestor, const Element* el) {
    const Element* current = el;

    while (current) {
        if (current == ancestor)
            return true;

        current = dynamic_cast<const Element*>(current->parent);
    }

    return false;
}

} // namespace

Element* EventState::hitTest(Element& root, float x, float y) {
    return hitRecursive(root, x, y);
}

bool EventState::onMouseMove(Element& root, float x, float y, bool leftDown) {
    Element* target = hitTest(root, x, y);
    bool changed = false;

    if (target != hovered) {
        if (hovered)
            setHoverChain(hovered, false);

        hovered = target;

        if (hovered)
            setHoverChain(hovered, true);

        changed = true;
    }

    if (leftDown && captured && captured->control) {
        MouseEvent event;
        event.x = x;
        event.y = y;
        event.button = 0;
        event.move = true;
        changed |= captured->control->onMouse(*captured, event);
    }

    return changed;
}

bool EventState::onMouseDown(Element& root, float x, float y) {
    Element* target = hitTest(root, x, y);

    if (target)
        UI_LOG("event", L"down (%.0f,%.0f) -> <%s#%s>", x, y, target->tag.c_str(), target->id.c_str());
    else
        UI_LOG("event", L"down (%.0f,%.0f) -> miss", x, y);

    if (!target)
        return false;

    Element* interactive = interactiveAncestor(target);
    active = interactive;

    if (interactive->control && interactive->control->focusable())
        setFocus(interactive);

    captured = nearestWithControl(target);

    if (captured) {
        MouseEvent event;
        event.x = x;
        event.y = y;
        event.button = 0;
        event.down = true;

        captured->control->onMouse(*captured, event);
    }

    return true;
}

bool EventState::onMouseUp(Element& root, float x, float y) {
    Element* target = hitTest(root, x, y);
    bool repaint = false;

    if (captured) {
        MouseEvent event;
        event.x = x;
        event.y = y;
        event.button = 0;
        event.up = true;
        repaint |= captured->control
            ? captured->control->onMouse(*captured, event)
            : false;
    }

    Element* click = active;

    if (click && target && containsElement(click, target)) {
        if (onClickAny)
            onClickAny(*click);

        repaint = true;
    }

    active = nullptr;
    captured = nullptr;
    return repaint;
}

bool EventState::onWheel(Element& root, float x, float y, int delta) {
    Element* target = hitTest(root, x, y);

    while (target) {
        if (target->style.overflowY == Overflow::Auto &&
            target->box.maxScrollY() > 0) {
            target->box.scrollY -= delta / 120.0f * 40.0f;
            target->box.scrollY = std::max(
                0.f,
                std::min(target->box.maxScrollY(), target->box.scrollY)
            );
            return true;
        }

        target = dynamic_cast<Element*>(target->parent);
    }

    return false;
}

bool EventState::onKeyDown(Element& root, const KeyEvent& e) {
    if (e.vk == VK_TAB) {
        std::vector<Element*> order = focusOrder(root);

        if (order.empty())
            return false;

        auto it = std::find(order.begin(), order.end(), focused);
        int index = it == order.end() ? 0 : static_cast<int>(it - order.begin());

        if (e.shift)
            index = (index - 1 + static_cast<int>(order.size())) % static_cast<int>(order.size());
        else
            index = (index + 1) % static_cast<int>(order.size());

        setFocus(order[static_cast<size_t>(index)]);
        return true;
    }

    if (e.vk == VK_ESCAPE && focused) {
        setFocus(nullptr);
        return true;
    }

    if (focused && focused->control)
        return focused->control->onKey(*focused, e);

    return false;
}

bool EventState::onChar(Element& root, wchar_t ch) {
    (void)root;

    if (focused && focused->control)
        return focused->control->onChar(*focused, ch);

    return false;
}

void EventState::setFocus(Element* el) {
    if (focused == el)
        return;

    if (focused) {
        focused->stateFlags &= ~StateFocus;

        if (focused->control)
            focused->control->onFocus(*focused, false);
    }

    focused = el;

    if (focused) {
        focused->stateFlags |= StateFocus;

        if (focused->control)
            focused->control->onFocus(*focused, true);
    }
}

std::vector<Element*> EventState::focusOrder(Element& root) const {
    std::vector<Element*> out;

    walk(root, [&](Element& el) {
        if (el.style.display == Display::None)
            return false;

        if (el.control && el.control->focusable())
            out.push_back(&el);
        else if (el.tag == L"button")
            out.push_back(&el);

        return true;
    });

    return out;
}

CursorKind EventState::currentCursor() const {
    Element* current = hovered;

    while (current) {
        if (current->control)
            return current->control->cursor();

        if (current->style.cursor != CursorKind::Arrow)
            return current->style.cursor;

        if (current->tag == L"button")
            return CursorKind::Hand;

        current = dynamic_cast<Element*>(current->parent);
    }

    return CursorKind::Arrow;
}

} // namespace ui
