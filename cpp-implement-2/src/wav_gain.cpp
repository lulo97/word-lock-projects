#include "app.h"

namespace wordlock {

std::vector<unsigned char> ApplyWavGain(std::vector<unsigned char> wav, double gain) {
    if (!std::isfinite(gain) || gain == 1.0 || wav.size() < 12 ||
        std::memcmp(wav.data(), "RIFF", 4) != 0 ||
        std::memcmp(wav.data() + 8, "WAVE", 4) != 0) {
        return wav;
    }

    int format = -1;
    int bitsPerSample = -1;
    size_t dataOffset = std::wstring::npos;
    uint32_t dataSize = 0;

    size_t offset = 12;
    while (offset + 8 <= wav.size()) {
        const std::string id(reinterpret_cast<const char*>(wav.data() + offset), 4);
        const uint32_t size = ReadLe32(wav, offset + 4);
        const size_t start = offset + 8;
        if (id == "fmt " && size >= 16 && start + 16 <= wav.size()) {
            format = ReadLe16(wav, start);
            bitsPerSample = ReadLe16(wav, start + 14);
        }
        if (id == "data") {
            dataOffset = start;
            dataSize = size;
            break;
        }
        const size_t next = start + size + (size % 2);
        if (next <= offset) {
            break;
        }
        offset = next;
    }

    if (format != 1 || bitsPerSample != 16 || dataOffset == std::wstring::npos) {
        return wav;
    }

    const size_t end = std::min(dataOffset + dataSize, wav.size());
    for (size_t i = dataOffset; i + 1 < end; i += 2) {
        int16_t sample = 0;
        std::memcpy(&sample, wav.data() + i, sizeof(sample));
        const int32_t scaled = std::clamp(
            static_cast<int32_t>(std::llround(static_cast<double>(sample) * gain)),
            -32768, 32767);
        const int16_t clipped = static_cast<int16_t>(scaled);
        std::memcpy(wav.data() + i, &clipped, sizeof(clipped));
    }
    return wav;
}

}  // namespace wordlock
