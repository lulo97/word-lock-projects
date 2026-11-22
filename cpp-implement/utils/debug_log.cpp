#include "debug_log.h"
#include "core/utf.h"

#include <windows.h>
#include <cstdarg>
#include <fstream>
#include <mutex>
#include <string>

namespace dbg {

static std::mutex g_m;

bool enabled(const char* cat) {
    static std::string spec = [] {
        char buf[256];
        size_t n = 0;
        return getenv_s(&n, buf, sizeof buf, "KOTOBA_LOG") == 0 && n
            ? std::string(buf)
            : std::string();
    }();
    return spec == "all" || (!spec.empty() && spec.find(cat) != std::string::npos);
}

void log(const char* cat, const wchar_t* fmt, ...) {
    wchar_t msg[1024];
    va_list ap;
    va_start(ap, fmt);
    _vsnwprintf_s(msg, _TRUNCATE, fmt, ap);
    va_end(ap);

    std::lock_guard<std::mutex> lock(g_m);
    static std::ofstream f("kotoba_debug.log", std::ios::app | std::ios::binary);
    f << GetTickCount64() << " [" << cat << "] " << ui::wideToUtf8(msg) << "\n";
    f.flush();                                     // survive crashes
}

} // namespace dbg
