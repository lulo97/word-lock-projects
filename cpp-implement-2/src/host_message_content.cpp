#include "app.h"

namespace wordlock {

bool HandleContentMessage(const std::wstring& type, const std::wstring& message,
                          const ReplyFn& reply, int number) {
    if (type == L"word:getRandom") {
        reply(GetRandomEnglishWordJson());
    } else if (type == L"word:get") {
        reply(GetEnglishWordJson(number));
    } else if (type == L"word:correct") {
        RecordWordShow("english", number);
        reply(L"{\"ok\":true}");
    } else if (type == L"japanese:correct") {
        RecordWordShow("japanese", number);
        reply(L"{\"ok\":true}");
    } else if (type == L"progress:get") {
        reply(GetWordProgressJson());
    } else if (type == L"db:info") {
        reply(GetDatabaseInfoJson());
    } else if (type == L"mcq:getRandom") {
        reply(GetRandomMcqJson());
    } else if (type == L"mcq:get") {
        reply(GetMcqJson(number));
    } else if (type == L"japanese:getRandom") {
        reply(GetRandomJapaneseWordJson());
    } else if (type == L"japanese:get") {
        reply(GetJapaneseWordJson(number));
    } else if (type == L"settings:get") {
        reply(BuildSettingsDataJson());
    } else if (type == L"settings:set") {
        ApplySettingsFromJson(message, false);
        reply(L"null");
    } else if (type == L"settings:confirm") {
        ApplySettingsFromJson(message, true);
        reply(L"null");
    } else if (type == L"settings:close") {
        BroadcastTheme(g_settings.theme);
        HideSettingsWindows();
    } else if (type == L"theme:get") {
        reply(JsLiteral(g_settings.theme));
    } else if (type == L"theme:list") {
        reply(BuildThemeListJson());
    } else if (type == L"theme:preview") {
        std::wstring theme;
        if (TryGetStringField(message, L"value", theme)) {
            BroadcastTheme(theme);
        }
    } else {
        return false;
    }
    return true;
}

}  // namespace wordlock
