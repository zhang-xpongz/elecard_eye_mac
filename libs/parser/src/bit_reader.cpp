#include "parser/bit_reader.h"

namespace parser {

BitReader::BitReader(const uint8_t* data, size_t size_bytes)
    : data_(data), size_bytes_(size_bytes) {
    // 骨架占位:抑制 -Werror=unused-private-field。实现后可删。
    (void)data_; (void)size_bytes_;
}

uint32_t BitReader::readBits(int /*n*/) {
    // TODO: implement to pass tests
    // 提示:
    //   - MSB-first:第 0 bit 是 data_[0] 的最高位 (data_[0] >> 7) & 1
    //   - 越界(bit_offset_ + n > size_bytes_*8)时 error_ = true,返回 0
    //   - 一位一位读最简单;追求性能可一次取一个 byte / uint64_t 缓冲
    return 0;
}

uint32_t BitReader::readUE() {
    // TODO: implement using readBits
    // 算法(H.264 9.1):
    //   1) 数 leading zeros(直到读到第一个 1),记为 k
    //   2) 再读 k 个 bit,称为 V
    //   3) 返回 (1 << k) - 1 + V
    // 边界:k 太大(>32)直接 error_ = true 返回 0
    return 0;
}

int32_t BitReader::readSE() {
    // TODO: implement using readUE
    // 映射(H.264 9.1.1):
    //   k = readUE()
    //   if (k & 1) → +(k+1)/2    (k=1→+1, k=3→+2, k=5→+3, ...)
    //   else       → -(k/2)      (k=0→0,  k=2→-1, k=4→-2, ...)
    return 0;
}

size_t BitReader::bitOffset() const { return bit_offset_; }
bool   BitReader::hasError() const  { return error_; }

}  // namespace parser
