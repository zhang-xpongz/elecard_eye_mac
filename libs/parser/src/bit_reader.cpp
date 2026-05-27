#include "parser/bit_reader.h"

namespace parser {

BitReader::BitReader(const uint8_t* data, size_t size_bytes)
    : data_(data), size_bytes_(size_bytes) {
}

uint32_t BitReader::readBits(int n) {
    // 提示:
    //   - MSB-first:第 0 bit 是 data_[0] 的最高位 (data_[0] >> 7) & 1
    //   - 越界(bit_offset_ + n > size_bytes_*8)时 error_ = true,返回 0
    //   - 一位一位读最简单;追求性能可一次取一个 byte / uint64_t 缓冲
    if (error_ || n <= 0 || n > 32 || bit_offset_ + static_cast<size_t>(n) > size_bytes_ * 8) {
        error_ = true;
        return 0;
    }
    
    uint32_t value = 0;
    for (int i = 0; i < n; ++i) {
        const size_t byte_idx = bit_offset_ / 8;
        const int bit_idx = 7 - (bit_offset_ % 8);
        const uint32_t bit = (data_[byte_idx] >> bit_idx) & 1u;
        value = (value << 1) | bit;
        ++bit_offset_;
    }

    return value;
}

uint32_t BitReader::readUE() {
    // 算法(H.264 9.1):
    //   1) 数 leading zeros(直到读到第一个 1),记为 k
    //   2) 再读 k 个 bit,称为 V
    //   3) 返回 (1 << k) - 1 + V
    // 边界:k 太大(>32)直接 error_ = true 返回 0
    uint32_t leading_zeros = 0;
    while (!error_ && leading_zeros < 32) {
        const uint32_t b = readBits(1);
        if (error_) return 0;
        if (b == 1) break;
        ++leading_zeros;
    }

    if (leading_zeros >= 32) {
        error_ = true;
        return 0;
    }

    uint32_t v = 0;
    if (leading_zeros > 0) {
        v = readBits(leading_zeros);
        if (error_) return 0;
    }
    return (1u << leading_zeros) - 1u + v;
}

int32_t BitReader::readSE() {
    // 映射(H.264 9.1.1):
    //   k = readUE()
    //   if (k & 1) → +(k+1)/2    (k=1→+1, k=3→+2, k=5→+3, ...)
    //   else       → -(k/2)      (k=0→0,  k=2→-1, k=4→-2, ...)
    uint32_t k = readUE();
    if (error_) return 0;
    if (k & 1u) return static_cast<int32_t>((k + 1u) >> 1);
    else return -static_cast<int32_t>(k >> 1);
}

size_t BitReader::bitOffset() const { return bit_offset_; }
bool   BitReader::hasError() const  { return error_; }

}  // namespace parser
