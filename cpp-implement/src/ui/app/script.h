#pragma once

#include <functional>
#include <map>
#include <string>
#include <vector>
#include "app/document.h"

namespace ui {

// Per-document command interpreter shared by the headless runner (ui_test),
// the feature-test runner (app_test) and the running app's debug
// command-file poller.
//
// Commands live in a single registry (addCommand): the built-in document
// commands (size, dpi, click, hover, focus, type, ime, key, wheel, dump,
// shot, assert, click-at, click-char, golden, include, expect-fail) are
// registered in the constructor, and KotobaCppApp registers its own
// `app`/`window` commands via registerCommands(). Unknown commands and
// short argument lists are failures, never silent passes.
//
// `load` is intentionally NOT part of the shared surface: the headless
// runner loads its own HTML, the running app owns its windows.
//
// Document::validate() is checked after every step; problems count as
// failures and appear in summary().
//
// Failure lines are reported as "FAIL <file>:<line>: ..." so they are
// clickable in VS/VS Code. The driver loop owns the line counter and calls
// setLocation() before each top-level line; `include` sets it for the lines
// it executes.
class ScriptRunner {
public:
    using CmdFn = std::function<void(const std::vector<std::wstring>&, const std::wstring&)>;

    ScriptRunner();
    ScriptRunner(Document& doc, int w, int h, float scale);

    // (Re)bind to a document + viewport. In: doc, W, H px, dpi scale.
    // Optional liveness probe: when it reports false, the host that owns
    // the document was destroyed by an app action (e.g. the info window's
    // #close runs start_countdown, which destroys the host). The runner
    // then unbinds and skips post-step validation instead of running
    // against a dead document.
    void bind(Document& doc, int w, int h, float scale,
              std::function<bool()> alive = nullptr);

    // Forget the bound document. Subsequent document commands fail with a
    // clear message instead of running against a stale document (e.g. a
    // `window` that was never created).
    void unbind();

    // Run one command line. In: line. Failures counted, see summary().
    void run(const std::wstring& line);

    // Set the location reported in FAIL messages. In: file, line (1-based).
    // The driver loop calls this before each top-level line; `include`
    // calls it for the lines it executes.
    void setLocation(const std::wstring& file, int line);

    // Register a command. In: name, minimum argument count (after the
    // command word), usage string (printed on arity failure), handler
    // receiving (tokens, original line).
    void addCommand(const std::wstring& name, size_t minArgs, const std::wstring& usage, CmdFn fn);

    // Resolve styles + layout if dirty for the bound viewport.
    void layout();

    // Record a failure (used by embedders for out-of-band errors like
    // a failed load). In: message.
    void fail(const std::wstring& msg);

    // Record a non-failure note in the report (e.g. `app state`).
    void note(const std::wstring& msg);

    // Clear accumulated notes/failures so a long-lived runner (the running
    // app's debug poller) reports each poll independently instead of
    // re-printing every earlier poll's output.
    void resetReport();

    // Compare the layout snapshot against a golden file, reporting every
    // mismatch. In: path, update (true re-records the baseline; UPDATE_GOLDEN=1
    // in the environment does the same, so updating is a deliberate runner
    // flag rather than a hand-edited steps line).
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

    // One report: each failure/note line plus a final OK/FAILED line.
    std::wstring summary() const;

private:
    Document* doc_ = nullptr;
    std::function<bool()> alive_;
    int W_ = 0, H_ = 0;
    float scale_ = 1.f;
    int failures_ = 0;
    int lineNo_ = 0;
    std::wstring file_;
    std::vector<std::wstring> messages_;
    std::wstring scriptDir_;
    int includeDepth_ = 0;

    struct Cmd { size_t minArgs; std::wstring usage; CmdFn fn; };
    std::map<std::wstring, Cmd> cmds_;

    void registerBuiltinCommands();
    void settle();
    Element* need(const std::wstring& sel);
    void mouse(Element& el, bool click);
    void keyCmd(const std::vector<std::wstring>& t);
    void assertCmd(const std::vector<std::wstring>& t);
    void includeCmd(const std::vector<std::wstring>& t);
};

} // namespace ui
