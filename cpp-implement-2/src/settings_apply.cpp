#include "app.h"

namespace wordlock {

void ApplySettingsFromJson(const std::wstring& values, bool confirm) {
    double number = 0.0;
    g_settings.tasks.englishWord =
        TryGetBoolField(values, L"englishWord", g_settings.tasks.englishWord);
    g_settings.tasks.multipleChoice =
        TryGetBoolField(values, L"multipleChoice", g_settings.tasks.multipleChoice);
    g_settings.tasks.japaneseWord =
        TryGetBoolField(values, L"japaneseWord", g_settings.tasks.japaneseWord);
    g_settings.tasks.dumbbellWorkout =
        TryGetBoolField(values, L"dumbbellWorkout", g_settings.tasks.dumbbellWorkout);
    if (TryGetNumberField(values, L"countdownSeconds", number)) {
        g_settings.countdownSeconds = NormalizeCountdownSeconds(number);
    }
    if (TryGetNumberField(values, L"masterVolume", number)) {
        g_settings.masterVolume = NormalizeVolume(number);
    }
    g_settings.chromeFreeze = TryGetBoolField(values, L"chromeFreeze", g_settings.chromeFreeze);
    g_settings.autoStart = TryGetBoolField(values, L"autoStart", g_settings.autoStart);
    g_settings.autoTheme = TryGetBoolField(values, L"autoTheme", g_settings.autoTheme);

    std::wstring theme;
    if (TryGetStringField(values, L"theme", theme)) {
        g_settings.theme = NormalizeTheme(theme);
    }

    g_nextTaskIndex = 0;
    SaveSettings();
    SyncChromeFreeze();
    ApplyAutoStartPreference(g_settings.autoStart);
    BroadcastTheme(g_settings.theme);

    if (confirm) {
        StartCountdown();
    }
}

void SendSettingsToSettingsWebView() {
    if (!g_webViewController) {
        return;
    }
    ComPtr<ICoreWebView2> webView;
    if (SUCCEEDED(g_webViewController->get_CoreWebView2(&webView)) && webView) {
        const std::wstring json =
            L"{\"type\":\"settings:changed\",\"settings\":" + BuildSettingsDataJson() + L"}";
        webView->PostWebMessageAsJson(json.c_str());
    }
}

}  // namespace wordlock
