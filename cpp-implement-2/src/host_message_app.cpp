#include "app.h"

namespace wordlock {

void HandleAppMessage(const std::wstring& type, const std::wstring& message,
                      const ReplyFn& reply) {
    if (type == L"countdown:finished") {
        ShowMainWindow();
    } else if (type == L"app:exitInfo") {
        if (g_settings.autoTheme) {
            g_settings.theme = NextThemeId(g_settings.theme);
            SaveSettings();
            BroadcastTheme(g_settings.theme);
        }
        StartCountdown();
    } else if (type == L"tts:speak") {
        std::wstring text;
        std::wstring lang;
        TryGetStringField(message, L"text", text);
        TryGetStringField(message, L"lang", lang);
        if (lang.empty()) {
            lang = L"en";
        }
        std::wstring wav;
        std::wstring error;
        if (SynthesizeTts(text, lang, wav, error)) {
            reply(L"{\"ok\":true,\"wav\":" + JsLiteral(wav) + L"}");
        } else {
            reply(L"{\"ok\":false,\"error\":" + JsLiteral(error) + L"}");
        }
    } else {
        reply(std::wstring(), L"Unknown host message: " + type);
    }
}

}  // namespace wordlock
