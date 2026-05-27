#pragma once

#include <cstddef>
#include <cstdint>

namespace parser {

// MSB-first bit reader over a contiguous byte buffer.
// Supports H.264 unsigned/signed Exp-Golomb codes (ue/se, spec 9.1).
// On out-of-range read, sets internal error flag instead of throwing.
class BitReader {
public:
    BitReader(const uint8_t* data, size_t size_bytes);

    // 读 n 个 bit(MSB-first),n ∈ [1, 32]
    uint32_t readBits(int n);

    // ue(v) — H.264 9.1
    uint32_t readUE();

    // se(v) — H.264 9.1.1
    int32_t  readSE();

    // 当前 bit 位置(从开头算起)
    size_t   bitOffset() const;

    bool     hasError() const;

private:
    const uint8_t* data_;
    size_t  size_bytes_;
    size_t  bit_offset_ = 0;
    bool    error_ = false;
};

}  // namespace parser
