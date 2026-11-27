#include "app.h"

namespace wordlock {

void LoadSettings() {
    g_settings = AppSettings{};
    if (IsTestDataMode()) {
        g_settings.chromeFreeze = EnvFlagEnabled(kTestChromeFreezeEnv);
        const std::wstring seconds = GetEnvironmentValue(kTestCountdownSecondsEnv);
        if (!seconds.empty()) {
            try {
                g_settings.countdownSeconds = NormalizeCountdownSeconds(std::stod(seconds));
            } catch (...) {
                // Keep the default when a test supplies an invalid value.
            }
        }
        return;
    }

    const std::filesystem::path path = std::filesystem::path(GetDataDirectory()) / L"settings.json";
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return;
    }
    const std::string bytes((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    const std::wstring json = Utf8ToWide(bytes);

    double number = 0.0;
    g_settings.tasks.englishWord = TryGetBoolField(json, L"englishWord", true);
    g_settings.tasks.multipleChoice = TryGetBoolField(json, L"multipleChoice", true);
    g_settings.tasks.japaneseWord = TryGetBoolField(json, L"japaneseWord", true);
    g_settings.tasks.dumbbellWorkout = TryGetBoolField(json, L"dumbbellWorkout", true);
    if (TryGetNumberField(json, L"countdownSeconds", number)) {
        g_settings.countdownSeconds = NormalizeCountdownSeconds(number);
    }
    if (TryGetNumberField(json, L"masterVolume", number)) {
        g_settings.masterVolume = NormalizeVolume(number);
    }
    g_settings.chromeFreeze = TryGetBoolField(json, L"chromeFreeze", false);
    g_settings.autoStart = TryGetBoolField(json, L"autoStart", false);
    g_settings.autoTheme = TryGetBoolField(json, L"autoTheme", false);
    std::wstring theme;
    if (TryGetStringField(json, L"theme", theme)) {
        g_settings.theme = NormalizeTheme(theme);
    }
    std::vector<std::wstring> words;
    if (TryGetStringArrayField(json, L"english", words)) {
        g_settings.ignoreWords.english = words;
    }
    if (TryGetStringArrayField(json, L"japanese", words)) {
        g_settings.ignoreWords.japanese = words;
    }
}

std::wstring JsonStringArray(const std::vector<std::wstring>& values) {
    std::wstring out = L"[";
    for (size_t i = 0; i < values.size(); ++i) {
        if (i != 0) {
            out += L",";
        }
        out += L"\"" + JsonEscape(values[i]) + L"\"";
    }
    out += L"]";
    return out;
}

void SaveSettings() {
    if (IsTestDataMode()) {
        return;
    }
    const std::filesystem::path path = std::filesystem::path(GetDataDirectory()) / L"settings.json";
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file) {
        return;
    }
    std::wstring json = L"{\n";
    json += L"  \"englishWord\": " + std::wstring(BoolLiteral(g_settings.tasks.englishWord)) + L",\n";
    json += L"  \"multipleChoice\": " + std::wstring(BoolLiteral(g_settings.tasks.multipleChoice)) + L",\n";
    json += L"  \"japaneseWord\": " + std::wstring(BoolLiteral(g_settings.tasks.japaneseWord)) + L",\n";
    json += L"  \"dumbbellWorkout\": " + std::wstring(BoolLiteral(g_settings.tasks.dumbbellWorkout)) + L",\n";
    json += L"  \"masterVolume\": " + std::to_wstring(g_settings.masterVolume) + L",\n";
    json += L"  \"countdownSeconds\": " + std::to_wstring(g_settings.countdownSeconds) + L",\n";
    json += L"  \"chromeFreeze\": " + std::wstring(BoolLiteral(g_settings.chromeFreeze)) + L",\n";
    json += L"  \"autoStart\": " + std::wstring(BoolLiteral(g_settings.autoStart)) + L",\n";
    json += L"  \"autoTheme\": " + std::wstring(BoolLiteral(g_settings.autoTheme)) + L",\n";
    json += L"  \"theme\": \"" + JsonEscape(g_settings.theme) + L"\",\n";
    json += L"  \"ignoreWords\": {\n";
    json += L"    \"english\": " + JsonStringArray(g_settings.ignoreWords.english) + L",\n";
    json += L"    \"japanese\": " + JsonStringArray(g_settings.ignoreWords.japanese) + L"\n";
    json += L"  }\n}\n";
    const std::string bytes = WideToUtf8(json);
    file.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
}

}  // namespace wordlock
