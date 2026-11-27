#include "app.h"

namespace wordlock {

std::wstring BuildSettingsDataJson() {
    std::wstring json = L"{";
    json += L"\"englishWord\":" + std::wstring(BoolLiteral(g_settings.tasks.englishWord));
    json += L",\"multipleChoice\":" + std::wstring(BoolLiteral(g_settings.tasks.multipleChoice));
    json += L",\"japaneseWord\":" + std::wstring(BoolLiteral(g_settings.tasks.japaneseWord));
    json += L",\"dumbbellWorkout\":" + std::wstring(BoolLiteral(g_settings.tasks.dumbbellWorkout));
    json += L",\"masterVolume\":" + std::to_wstring(g_settings.masterVolume);
    json += L",\"countdownSeconds\":" + std::to_wstring(g_settings.countdownSeconds);
    json += L",\"chromeFreeze\":" + std::wstring(BoolLiteral(g_settings.chromeFreeze));
    json += L",\"autoStart\":" + std::wstring(BoolLiteral(g_settings.autoStart));
    json += L",\"autoTheme\":" + std::wstring(BoolLiteral(g_settings.autoTheme));
    json += L",\"theme\":" + JsLiteral(g_settings.theme);
    json += L",\"ignoreWords\":{\"english\":" + JsonStringArray(g_settings.ignoreWords.english) +
            L",\"japanese\":" + JsonStringArray(g_settings.ignoreWords.japanese) + L"}";
    json += L"}";
    return json;
}

std::wstring BuildThemeListJson() {
    struct ThemeInfo {
        const wchar_t* id;
        const wchar_t* name;
        const wchar_t* description;
    };
    static const ThemeInfo themes[] = {
        {L"paper", L"Paper", L"Warm neutral paper"},
        {L"brutal", L"Brutal", L"High-contrast primary colors"},
        {L"candy", L"Candy", L"Soft pink pastels"},
        {L"glass", L"Glass", L"Frosted translucent panels"},
        {L"midnight", L"Midnight", L"Deep navy and cool accents"},
        {L"notebook", L"Notebook", L"Lined notebook paper"},
        {L"sakura", L"Sakura", L"Cherry blossom pink"},
        {L"synthwave", L"Synthwave", L"Neon sunset glow"},
        {L"terminal", L"Terminal", L"Classic green screen"},
        {L"zen", L"Zen", L"Calm stone neutrals"},
    };

    std::wstring json = L"[";
    for (size_t i = 0; i < std::size(themes); ++i) {
        if (i != 0) {
            json += L",";
        }
        json += L"{\"id\":" + JsLiteral(themes[i].id) +
                L",\"name\":" + JsLiteral(themes[i].name) +
                L",\"description\":" + JsLiteral(themes[i].description) + L"}";
    }
    json += L"]";
    return json;
}

}  // namespace wordlock
