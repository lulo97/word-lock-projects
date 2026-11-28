#include "app.h"

namespace wordlock {

bool SynthesizeTts(const std::wstring& text,
                   const std::wstring& lang,
                   std::wstring& wavBase64,
                   std::wstring& error) {
    error = kTtsUnavailableMessage;
    wavBase64.clear();

    const std::wstring normalized = NormalizeTtsText(text);
    const std::string langUtf8 = WideToUtf8(lang);
    const std::string textUtf8 = WideToUtf8(normalized);
    const bool cacheable = !normalized.empty();

    std::vector<unsigned char> wav;
    bool cacheHit = false;
    if (cacheable) {
        std::vector<unsigned char> ogg;
        if (TtsCacheGet(langUtf8, kTtsVoice, textUtf8, ogg)) {
            if (ConvertOggToWav(ogg, wav)) {
                cacheHit = true;
            } else {
                // A bad cached row must not break speech; drop it and refetch.
                AppendTestMessageLog(L"tts:cache-invalid");
                TtsCacheRemove(langUtf8, kTtsVoice, textUtf8);
                wav.clear();
            }
        }
    }

    AppendTestMessageLog(cacheHit ? L"tts:cache-hit" : L"tts:cache-miss");

    if (!cacheHit) {
        if (!FetchTtsWav(normalized, lang, wav, error)) {
            wavBase64.clear();
            return false;
        }
        if (cacheable) {
            std::vector<unsigned char> ogg;
            if (ConvertWavToOgg(wav, ogg)) {
                TtsCachePut(langUtf8, kTtsVoice, textUtf8, ogg);
            } else {
                // Speech still works without ffmpeg; it just is not cached.
                AppendTestMessageLog(L"tts:cache-skip");
            }
        }
    }

    error.clear();
    wavBase64 = Base64Encode(ApplyWavGain(std::move(wav), g_settings.masterVolume));
    return true;
}

}  // namespace wordlock
