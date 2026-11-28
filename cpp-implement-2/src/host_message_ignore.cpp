#include "app.h"

namespace wordlock {

bool HandleIgnoreWordsMessage(const std::wstring& type, const std::wstring& message,
                              const ReplyFn& reply) {
    if (type == L"ignoreWords:open") {
        OpenIgnoreWordsWindow();
        reply(L"null");
    } else if (type == L"ignoreWords:get") {
        reply(L"{\"english\":" + JsonStringArray(g_settings.ignoreWords.english) +
              L",\"japanese\":" + JsonStringArray(g_settings.ignoreWords.japanese) + L"}");
    } else if (type == L"ignoreWords:save") {
        std::wstring englishText;
        std::wstring japaneseText;
        TryGetStringField(message, L"english", englishText);
        TryGetStringField(message, L"japanese", japaneseText);
        try {
            IgnoreWords words;
            words.english = ParseIgnoreWords(englishText, false);
            words.japanese = ParseIgnoreWords(japaneseText, true);
            g_settings.ignoreWords = std::move(words);
            SaveSettings();
            reply(L"{\"ok\":true,\"words\":{\"english\":" +
                  JsonStringArray(g_settings.ignoreWords.english) +
                  L",\"japanese\":" + JsonStringArray(g_settings.ignoreWords.japanese) + L"}}");
        } catch (...) {
            reply(L"{\"ok\":false,\"error\":" + JsLiteral(kIgnoreError) + L"}");
        }
    } else if (type == L"ignoreWords:close") {
        // Save and Cancel close only this dialog; the parent settings window
        // must stay open. The X button uses the same WM_CLOSE path.
        HideIgnoreWordsWindow();
    } else {
        return false;
    }
    return true;
}

}  // namespace wordlock
