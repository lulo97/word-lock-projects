#include "app.h"

namespace wordlock {
namespace {

constexpr int kMockSampleRate = 24000;
constexpr int kMockSampleCount = kMockSampleRate / 2;

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

// A short 16-bit PCM tone, so gain changes are observable in the base64 reply.
std::vector<unsigned char> MakeMockTtsWav() {
    std::vector<unsigned char> wav(44 + kMockSampleCount * sizeof(int16_t), 0);
    std::memcpy(wav.data(), "RIFF", 4);
    WriteLe32(wav, 4, static_cast<uint32_t>(wav.size() - 8));
    std::memcpy(wav.data() + 8, "WAVE", 4);
    std::memcpy(wav.data() + 12, "fmt ", 4);
    WriteLe32(wav, 16, 16);
    WriteLe16(wav, 20, 1);  // PCM
    WriteLe16(wav, 22, 1);  // mono
    WriteLe32(wav, 24, kMockSampleRate);
    WriteLe32(wav, 28, kMockSampleRate * static_cast<uint32_t>(sizeof(int16_t)));
    WriteLe16(wav, 32, static_cast<uint16_t>(sizeof(int16_t)));
    WriteLe16(wav, 34, 16);
    std::memcpy(wav.data() + 36, "data", 4);
    WriteLe32(wav, 40, static_cast<uint32_t>(kMockSampleCount * sizeof(int16_t)));

    for (int i = 0; i < kMockSampleCount; ++i) {
        const double angle = 2.0 * 3.14159265358979323846 * 220.0 * i / kMockSampleRate;
        const int16_t sample = static_cast<int16_t>(std::lround(12000.0 * std::sin(angle)));
        WriteLe16(wav, 44 + i * sizeof(int16_t), static_cast<uint16_t>(sample));
    }
    return wav;
}

}  // namespace

bool TryGetMockTtsWav(std::vector<unsigned char>& wav) {
    wav = MakeMockTtsWav();
    return !wav.empty();
}

}  // namespace wordlock
