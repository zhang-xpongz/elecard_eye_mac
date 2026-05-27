#pragma once

#include <cstddef>
#include <cstdint>

namespace parser {

// =============================================================================
// NALUnit — 一个 NAL Unit 在 Annex-B 字节流中的位置描述(纯 POD)
// =============================================================================
//
// 本结构只是定位元信息,不持有数据。要拿 payload 字节,回到原 buffer:
//
//     const uint8_t* p   = original_data + nal.payload_offset;
//     size_t         len = nal.size;
//
// 字段不剥离 emulation prevention(那是 RBSP 解析阶段的事)。
//
struct NALUnit {
    // 在原 buffer 中起始码的第一个字节偏移。
    //   - 3-byte 起始码 (00 00 01)     → byte_offset 指向第一个 00
    //   - 4-byte 起始码 (00 00 00 01)  → byte_offset 指向第一个 00(含 leading 00)
    size_t  byte_offset;

    // 跳过起始码后,payload 的第一个字节偏移(即 NAL header byte 位置)。
    //   payload_offset = byte_offset + 起始码长度(3 或 4)
    size_t  payload_offset;

    // payload 字节数。**含** NAL header byte,**不含** 起始码本身。
    // 也不剥离 emulation prevention bytes(原样保留 00 00 03 序列)。
    // 截断尾部时可能为 0。
    size_t  size;

    // payload[0] 的低 5 bit,即 H.264 nal_unit_type 字段。
    // 常见值(H.264 Table 7-1):
    //    1 = non-IDR slice
    //    5 = IDR slice
    //    6 = SEI
    //    7 = SPS
    //    8 = PPS
    //    9 = AUD (Access Unit Delimiter)
    //   12 = filler data
    // 当 payload 为空(size==0)时为 0。
    uint8_t nal_unit_type;
};

}  // namespace parser
