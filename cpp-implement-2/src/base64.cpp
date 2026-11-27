#include "app.h"

namespace wordlock {

std::wstring Base64Encode(const std::vector<unsigned char>& bytes) {
    static const char table[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve(((bytes.size() + 2) / 3) * 4);

    size_t index = 0;
    while (index + 2 < bytes.size()) {
        const uint32_t value = (static_cast<uint32_t>(bytes[index]) << 16) |
                               (static_cast<uint32_t>(bytes[index + 1]) << 8) |
                               static_cast<uint32_t>(bytes[index + 2]);
        out.push_back(table[(value >> 18) & 0x3F]);
        out.push_back(table[(value >> 12) & 0x3F]);
        out.push_back(table[(value >> 6) & 0x3F]);
        out.push_back(table[value & 0x3F]);
        index += 3;
    }

    if (index < bytes.size()) {
        const size_t remaining = bytes.size() - index;
        const uint32_t value = static_cast<uint32_t>(bytes[index]) << 16 |
                               (remaining == 2 ? static_cast<uint32_t>(bytes[index + 1]) << 8 : 0);
        out.push_back(table[(value >> 18) & 0x3F]);
        out.push_back(table[(value >> 12) & 0x3F]);
        out.push_back(remaining == 2 ? table[(value >> 6) & 0x3F] : '=');
        out.push_back('=');
    }

    return Utf8ToWide(out);
}

uint32_t ReadLe32(const std::vector<unsigned char>& bytes, size_t offset) {
    if (offset + 4 > bytes.size()) {
        return 0;
    }
    return static_cast<uint32_t>(bytes[offset]) |
           (static_cast<uint32_t>(bytes[offset + 1]) << 8) |
           (static_cast<uint32_t>(bytes[offset + 2]) << 16) |
           (static_cast<uint32_t>(bytes[offset + 3]) << 24);
}

uint16_t ReadLe16(const std::vector<unsigned char>& bytes, size_t offset) {
    if (offset + 2 > bytes.size()) {
        return 0;
    }
    return static_cast<uint16_t>(bytes[offset] |
                                 (static_cast<uint16_t>(bytes[offset + 1]) << 8));
}

}  // namespace wordlock

