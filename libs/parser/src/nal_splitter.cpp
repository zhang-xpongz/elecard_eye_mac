#include "parser/nal_splitter.h"

namespace parser {

std::vector<NALUnit> NALSplitter::split(const uint8_t* data, size_t size) {
    // TODO: implement to pass tests
    // 提示:
    //   1) 早返回:data==nullptr 或 size<3 → 返回空 vector
    //   2) 单次扫描,找所有 0x00 0x00 0x01 匹配位置
    //      - 匹配后跳过 3 字节避免重复 match
    //   3) 对每个匹配位置 i:
    //        - 若 i>0 且 data[i-1]==0x00 → 4-byte 起始码,byte_offset = i-1
    //          否则                       → 3-byte 起始码,byte_offset = i
    //        - payload_offset = i + 3
    //   4) 第二轮(或在第一轮里推进时)填 size:
    //        - 非最后一个 NAL:size = 下一个 NAL.byte_offset - 本 NAL.payload_offset
    //        - 最后一个 NAL:  size = buffer_size - payload_offset
    //   5) nal_unit_type:
    //        - 有 payload(payload_offset < size)→ data[payload_offset] & 0x1F
    //        - 否则                              → 0
    (void)data; (void)size;
    return {};
}

}  // namespace parser
