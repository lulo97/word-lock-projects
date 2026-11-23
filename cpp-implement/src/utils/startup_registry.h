#pragma once

#include <string>

// Start with Windows (M8).
//
// The app registers itself under
//   HKCU\Software\Microsoft\Windows\CurrentVersion\Run
// with the value
//   "C:\path\to\app.exe" --minimized
// so Windows launches it in the tray at sign-in (no admin rights needed).
//
// StartupRegistry is a seam: the real app uses WinStartupRegistry, while
// headless tests install FakeStartupRegistry through KotobaCppApp::hooks so
// `app init` never reads or writes the user's real registry.
class StartupRegistry {
public:
    virtual ~StartupRegistry() = default;

    // True if a Run value exists for this app.
    virtual bool isEnabled() const = 0;

    // The stored command line, or empty when the value is absent.
    virtual std::wstring command() const = 0;

    // Create/update (enabled) or delete (disabled) the Run value.
    // Returns false only on an actual registry error.
    virtual bool setEnabled(bool enabled, const std::wstring& command) = 0;
};

// The current process's executable, fully qualified.
std::wstring currentExePath();

// Quoted executable + --minimized, the value stored in the Run key.
std::wstring startupCommandLine(const std::wstring& exePath);

class WinStartupRegistry final : public StartupRegistry {
public:
    bool isEnabled() const override;
    std::wstring command() const override;
    bool setEnabled(bool enabled, const std::wstring& command) override;
};

// In-memory backend for headless feature tests.
class FakeStartupRegistry final : public StartupRegistry {
public:
    bool isEnabled() const override { return enabled_; }
    std::wstring command() const override { return command_; }
    bool setEnabled(bool enabled, const std::wstring& command) override;

    // Back to "no Run value", as if the user had never enabled the option.
    void reset() {
        enabled_ = false;
        command_.clear();
    }

private:
    bool enabled_ = false;
    std::wstring command_;
};
