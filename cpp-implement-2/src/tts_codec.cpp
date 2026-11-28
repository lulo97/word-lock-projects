#include "app.h"

namespace wordlock {

bool ConvertWavToOgg(const std::vector<unsigned char>& wav,
                     std::vector<unsigned char>& ogg) {
    ogg.clear();
    if (wav.empty()) {
        return false;
    }

    const TtsTempFiles files = MakeTtsTempFiles(L".wav", L".ogg");
    if (!WriteTtsTempFile(files.input, wav)) {
        return false;
    }

    const std::wstring arguments = L"-v quiet -y -i " + QuoteTtsPath(files.input) +
                                   L" -vn -c:a libvorbis -q:a 4 -f ogg " +
                                   QuoteTtsPath(files.output);
    if (!RunTtsFfmpeg(arguments) || !ReadTtsTempFile(files.output, ogg)) {
        ogg.clear();
        return false;
    }
    if (ogg.size() < 4 || std::memcmp(ogg.data(), "OggS", 4) != 0) {
        ogg.clear();
        return false;
    }
    return true;
}

bool ConvertOggToWav(const std::vector<unsigned char>& ogg,
                     std::vector<unsigned char>& wav) {
    wav.clear();
    if (ogg.size() < 4 || std::memcmp(ogg.data(), "OggS", 4) != 0) {
        return false;
    }

    const TtsTempFiles files = MakeTtsTempFiles(L".ogg", L".wav");
    if (!WriteTtsTempFile(files.input, ogg)) {
        return false;
    }

    const std::wstring arguments = L"-v quiet -y -i " + QuoteTtsPath(files.input) +
                                   L" -vn -c:a pcm_s16le -f wav " +
                                   QuoteTtsPath(files.output);
    if (!RunTtsFfmpeg(arguments) || !ReadTtsTempFile(files.output, wav)) {
        wav.clear();
        return false;
    }
    if (wav.size() < 12 || std::memcmp(wav.data(), "RIFF", 4) != 0 ||
        std::memcmp(wav.data() + 8, "WAVE", 4) != 0) {
        wav.clear();
        return false;
    }
    return true;
}

}  // namespace wordlock
