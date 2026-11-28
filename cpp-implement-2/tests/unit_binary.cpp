#include <catch2/catch_test_macros.hpp>

#include <cstring>
#include <limits>

#include "app.h"

using namespace wordlock;

namespace {

std::vector<unsigned char> MakeWav(const std::vector<int16_t>& samples) {
    std::vector<unsigned char> wav;
    const auto appendText = [&wav](const char* text) {
        wav.insert(wav.end(), text, text + 4);
    };
    const auto append32 = [&wav](uint32_t value) {
        for (int shift = 0; shift < 32; shift += 8) {
            wav.push_back(static_cast<unsigned char>((value >> shift) & 0xFF));
        }
    };
    const auto append16 = [&wav](uint16_t value) {
        wav.push_back(static_cast<unsigned char>(value & 0xFF));
        wav.push_back(static_cast<unsigned char>((value >> 8) & 0xFF));
    };

    appendText("RIFF");
    append32(36 + static_cast<uint32_t>(samples.size() * 2));
    appendText("WAVE");
    appendText("fmt ");
    append32(16);
    append16(1);  // PCM
    append16(1);  // mono
    append32(8000);
    append32(16000);
    append16(2);
    append16(16);
    appendText("data");
    append32(static_cast<uint32_t>(samples.size() * 2));
    for (int16_t sample : samples) {
        append16(static_cast<uint16_t>(sample));
    }
    return wav;
}

std::vector<int16_t> WavSamples(const std::vector<unsigned char>& wav) {
    std::vector<int16_t> samples;
    for (size_t i = 44; i + 1 < wav.size(); i += 2) {
        int16_t sample = 0;
        std::memcpy(&sample, wav.data() + i, sizeof(sample));
        samples.push_back(sample);
    }
    return samples;
}

}  // namespace

TEST_CASE("Base64Encode follows RFC 4648 padding", "[unit][binary]") {
    REQUIRE(Base64Encode({}).empty());
    REQUIRE(Base64Encode({'f'}) == L"Zg==");
    REQUIRE(Base64Encode({'f', 'o'}) == L"Zm8=");
    REQUIRE(Base64Encode({'f', 'o', 'o'}) == L"Zm9v");
    REQUIRE(Base64Encode({'f', 'o', 'o', 'b', 'a', 'r'}) == L"Zm9vYmFy");
}

TEST_CASE("WAV samples are scaled and clipped by ApplyWavGain", "[unit][binary]") {
    const auto wav = MakeWav({1000, -1000, 32767, -32768});
    REQUIRE(ReadLe32(wav, 0) == 0x46464952u);  // "RIFF"
    REQUIRE(ReadLe16(wav, 20) == 1);           // PCM format

    REQUIRE(ApplyWavGain(wav, 1.0) == wav);
    REQUIRE(ApplyWavGain(wav, std::numeric_limits<double>::quiet_NaN()) == wav);

    const auto quiet = ApplyWavGain(wav, 0.5);
    REQUIRE(WavSamples(quiet) == std::vector<int16_t>{500, -500, 16384, -16384});

    const auto loud = ApplyWavGain(wav, 2.0);
    REQUIRE(WavSamples(loud) == std::vector<int16_t>{2000, -2000, 32767, -32768});

    std::vector<unsigned char> truncated(wav.begin(), wav.begin() + 8);
    REQUIRE(ApplyWavGain(truncated, 0.5) == truncated);
}
