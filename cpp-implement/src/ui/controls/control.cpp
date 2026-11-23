#include "controls/control.h"

#include "controls/button.h"
#include "controls/checkbox.h"
#include "controls/range_input.h"
#include "controls/text_area.h"
#include "controls/text_input.h"
#include "dom/node.h"

#include <memory>

namespace ui {

std::unique_ptr<Control> createControlFor(Element& el) {
    if (el.tag == L"input") {
        std::wstring type = el.attr(L"type");

        if (type == L"range")
            return std::make_unique<RangeInput>();

        if (type == L"checkbox")
            return std::make_unique<Checkbox>();

        return std::make_unique<TextInput>();
    }

    if (el.tag == L"textarea")
        return std::make_unique<TextArea>();

    if (el.tag == L"button")
        return std::make_unique<Button>();

    return nullptr;
}

} // namespace ui
