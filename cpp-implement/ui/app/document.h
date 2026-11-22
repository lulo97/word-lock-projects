#pragma once
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include "dom/node.h"
#include "app/events.h"

namespace ui {

// Owns one HTML screen: tree + styles + layout + events. One Document per top-level window.
class Document {
public:
    Document();
    ~Document();

    // Parse html and build tree. In: html source. Out: true on success.
    bool load(const std::wstring& html);

    // Load html file (UTF-8). In: path. Out: true on success.
    bool loadFile(const std::wstring& path);

    // ----- lookup / mutation by id -----
    Element* byId(const std::wstring& id);
    void setText(const std::wstring& id, const std::wstring& text);
    void setClass(const std::wstring& id, const std::wstring& classes);
    void setValue(const std::wstring& id, const std::wstring& value);      // input/textarea/range
    std::wstring getValue(const std::wstring& id) const;
    void setChecked(const std::wstring& id, bool checked);
    void setVisible(const std::wstring& id, bool visible);                 // display:none toggle

    // ----- events -----
    void onClick(const std::wstring& id, std::function<void()> fn);
    void onInput(const std::wstring& id, std::function<void(const std::wstring&)> fn);
    void onChange(const std::wstring& id, std::function<void(float)> fn);  // range

    // ----- templates -----
    // Clone <template id=...> content into parent, replacing {{key}} with values.
    // In: templateId, parentId, key->value map. Out: first new element (or nullptr).
    Element* cloneTemplate(const std::wstring& templateId, const std::wstring& parentId,
                           const std::unordered_map<std::wstring, std::wstring>& values);
    void clearChildren(const std::wstring& parentId);

    // ----- pipeline -----
    // Intrinsic size of the page at a given width (for auto-sizing popup windows).
    // In: maxWidth px. Out: size px.
    SIZE measureContent(int maxWidth, float scale);

    void invalidateStyle();     // mark all dirty, repaint next frame
    void invalidateLayout();
    void invalidatePaint();
    bool needsPaint() const;

    // Resolve styles + layout if dirty for a given viewport.
    // In: width, height px, dpi scale. In: viewport + dirty flags updated.
    void ensureLayout(int width, int height, float scale);

    // Resolve styles + layout if dirty, then paint into hdc.
    // In: hdc, client rect, dpi scale. Out: pixels.
    void paint(HDC hdc, const RECT& client, float scale);

    // Render offscreen to a PNG file. In: path, width, height px, dpi scale,
    // overlay (debug boxes). Out: true on success.
    bool screenshot(const std::wstring& path, int w, int h, float scale, bool overlay = false);

    // Handle a window message. Out: true if handled (result in outResult).
    bool handleMessage(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, LRESULT& outResult);

    // Caret blink tick (called from timer). Out: true if repaint needed.
    bool tickBlink();

    Element* root() { return root_.get(); }
    EventState& events() { return events_; }

    // Dump the element tree as JSON to a file. In: path, maxDepth. Out: true on success.
    bool dumpTree(const std::wstring& path, int maxDepth = 99);

    // Check structural invariants (dangling pointers, duplicate ids, NaN rects,
    // unresolved placeholders). Out: list of problems (empty if fine).
    std::vector<std::wstring> validate();

private:
    std::unique_ptr<Element> root_;
    EventState events_;
    int viewportW_ = 0, viewportH_ = 0;
    float scale_ = 1.f;
    bool styleDirty_ = true, layoutDirty_ = true, paintDirty_ = true;
    std::unordered_map<std::wstring, std::function<void()>> clickHandlers_;
    std::unordered_map<std::wstring, std::function<void(const std::wstring&)>> inputHandlers_;
    std::unordered_map<std::wstring, std::function<void(float)>> changeHandlers_;
};

} // namespace ui
