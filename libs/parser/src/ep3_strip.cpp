#include "parser/ep3_strip.h"

namespace parser {

std::vector<uint8_t> stripEmulationPrevention(const uint8_t* data, size_t size) {
    // TODO: implement to pass tests
    // 提示:
    //   - 准备一个 output vector(可 reserve(size) 减少 realloc)
    //   - 用下标 i 扫:
    //       若 i+2 < size 且 data[i]==0 && data[i+1]==0 && data[i+2]==0x03:
    //           push 0x00, push 0x00,  i += 3   // 丢弃 0x03,跳到 0x03 之后
    //       否则:
    //           push data[i],          i += 1
    //   - 注意:i+2 < size 是"data[i+2] 是合法下标"的判断(末尾的 00 00 03 也会被剥)
    (void)data; (void)size;
    return {};
}

}  // namespace parser
