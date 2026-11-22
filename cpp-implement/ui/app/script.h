#pragma once

#include <string>
#include <vector>
#include "app/document.h"

namespace ui {

// Per-document command interpreter shared by the headless runner (ui_test)
// and the running app's debug command-file poller.
//
// Commands: size, dpi, shot [overlay], click, hover, focus, type, ime,
//           key, wheel, dump, assert
// `load` is intentionally NOT part of the shared surface: the headless
// runner loads its own HTML, the running app owns its windows.
//
// Document::validate() is checked after every step; problems count as
// failures and appear in summary().
class ScriptRunner {
public:
    ScriptRunner() = default;
    ScriptRunner(Document& doc, int w, int h, float scale);

    // (Re)bind to a document + viewport. In: doc, W, H px, dpi scale.
    void bind(Document& doc, int w, int h, float scale);

    // Run one command line. In: line. Failures counted, see summary().
    void run(const std::wstring& line);

    // Resolve styles + layout if dirty for the bound viewport.
    void layout();

    // Record a failure (used by embedders for out-of-band errors like
    // a failed load). In: message.
    void fail(const std::wstring& msg);

    // Compare the layout snapshot against a golden file, reporting every
    // mismatch. In: path, update (true re-records the baseline).
    // Only writes when update is true; a missing file otherwise fails.
    void golden(const std::wstring& path, bool update);

    // Resolve command file paths relative to the running script's directory,
    // so case files stay self-contained ("golden expected.layout.json").
    // Absolute paths (drive letter or root) are returned as-is.
    void setScriptDir(const std::wstring& dir);
    std::wstring resolvePath(const std::wstring& path) const;

    // Split a line on spaces; "quoted text" tokens may contain spaces.
    static std::vector<std::wstring> tokens(const std::wstring& line);

    // Out: number of failures so far.
    int failures() const { return failures_; }

    // One report: each failure line plus a final OK/FAILED line.
    std::wstring summary() const;

private:
    Document* doc_ = nullptr;
    int W_ = 0, H_ = 0;
    float scale_ = 1.f;
    int failures_ = 0, lineNo_ = 0;
    std::vector<std::wstring> messages_;
    std::wstring scriptDir_;

    void settle();
    Element* need(const std::wstring& sel);
    void mouse(Element& el, bool click);
    void key(const std::wstring& name);
    void assertCmd(const std::vector<std::wstring>& t);
};

} // namespace ui
