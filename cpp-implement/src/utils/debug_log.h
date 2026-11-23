#pragma once

// File logger with categories. Writes to kotoba_debug.log in the working
// directory. Enabled per category via KOTOBA_LOG=all or KOTOBA_LOG=app,event,style.
// Each call flushes, so the log survives crashes.
namespace dbg {
bool enabled(const char* cat);
void log(const char* cat, const wchar_t* fmt, ...);
}

#define UI_LOG(cat, ...) do { if (dbg::enabled(cat)) dbg::log(cat, __VA_ARGS__); } while (0)
