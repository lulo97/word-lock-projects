#include <catch2/catch_test_macros.hpp>

#include "app.h"

using namespace wordlock;

namespace {

void WriteLe16(std::vector<unsigned char>& bytes, size_t offset, uint16_t value) {
    bytes[offset] = static_cast<unsigned char>(value & 0xff);
    bytes[offset + 1] = static_cast<unsigned char>((value >> 8) & 0xff);
}

void WriteLe32(std::vector<unsigned char>& bytes, size_t offset, uint32_t value) {
    bytes[offset] = static_cast<unsigned char>(value & 0xff);
    bytes[offset + 1] = static_cast<unsigned char>((value >> 8) & 0xff);
    bytes[offset + 2] = static_cast<unsigned char>((value >> 16) & 0xff);
    bytes[offset + 3] = static_cast<unsigned char>((value >> 24) & 0xff);
}

std::vector<unsigned char> MakeToneWav() {
    constexpr int sampleRate = 24000;
    constexpr int sampleCount = sampleRate / 2;
    std::vector<unsigned char> wav(44 + sampleCount * sizeof(int16_t), 0);
    std::memcpy(wav.data(), "RIFF", 4);
    WriteLe32(wav, 4, static_cast<uint32_t>(wav.size() - 8));
    std::memcpy(wav.data() + 8, "WAVE", 4);
    std::memcpy(wav.data() + 12, "fmt ", 4);
    WriteLe32(wav, 16, 16);
    WriteLe16(wav, 20, 1);
    WriteLe16(wav, 22, 1);
    WriteLe32(wav, 24, sampleRate);
    WriteLe32(wav, 28, sampleRate * static_cast<uint32_t>(sizeof(int16_t)));
    WriteLe16(wav, 32, static_cast<uint16_t>(sizeof(int16_t)));
    WriteLe16(wav, 34, 16);
    std::memcpy(wav.data() + 36, "data", 4);
    WriteLe32(wav, 40, static_cast<uint32_t>(sampleCount * sizeof(int16_t)));
    for (int i = 0; i < sampleCount; ++i) {
        const double angle = 2.0 * 3.14159265358979323846 * 220.0 * i / sampleRate;
        const int16_t sample = static_cast<int16_t>(std::lround(12000.0 * std::sin(angle)));
        WriteLe16(wav, 44 + i * sizeof(int16_t), static_cast<uint16_t>(sample));
    }
    return wav;
}

}  // namespace

TEST_CASE("FFmpeg converts TTS WAV to compact OGG and back", "[unit][tts][codec]") {
    const std::vector<unsigned char> wav = MakeToneWav();
    std::vector<unsigned char> ogg;
    REQUIRE(ConvertWavToOgg(wav, ogg));
    REQUIRE(ogg.size() >= 4);
    REQUIRE(std::memcmp(ogg.data(), "OggS", 4) == 0);
    REQUIRE(ogg.size() < wav.size());

    std::vector<unsigned char> decoded;
    REQUIRE(ConvertOggToWav(ogg, decoded));
    REQUIRE(decoded.size() >= 12);
    REQUIRE(std::memcmp(decoded.data(), "RIFF", 4) == 0);
    REQUIRE(std::memcmp(decoded.data() + 8, "WAVE", 4) == 0);

    const std::vector<unsigned char> louder = ApplyWavGain(decoded, 2.0);
    REQUIRE(louder != decoded);
}
