#pragma once

#include "parser/nal_unit.h"

#include <vector>

namespace parser {

// =============================================================================
// NALSplitter — Annex-B 字节流 NAL Unit 切割器
// =============================================================================
//
// 输入一段连续的 H.264 Annex-B 格式字节流(`.h264` 裸流 / 从 MP4 经
// `h264_mp4toannexb` 提取的 ES),扫描所有起始码 (00 00 01 或 00 00 00 01),
// 输出 NAL Unit 边界列表(`std::vector<NALUnit>`)。
//
// 不做的事:
//   - 不剥离 emulation prevention (00 00 03)。RBSP 解析阶段才需要
//   - 不解析 nal header 之外的语法
//   - 不修改输入数据
//
// 线程安全:无成员状态,可被多线程并发调用
//
class NALSplitter {
public:
    // -------------------------------------------------------------------------
    // split — 把 Annex-B 字节流切成 NAL Unit 边界列表
    //
    // 扫描算法概述:
    //   1) 在 data 里寻找字节模式 `00 00 01`
    //   2) 如果该模式紧邻在一个 0x00 之后,起始码视为 4-byte 形式
    //   3) 当前 NAL 的 payload 从起始码后第一个字节开始
    //   4) 当前 NAL 的 size 由"下一个起始码 byte_offset"或"buffer 末尾"决定
    //
    // 参数:
    //   data    Annex-B 字节流起始指针。允许 nullptr(此时 size 必须为 0)
    //   size    字节数
    //
    // 返回:
    //   NAL Unit 列表,按字节流出现顺序排列。
    //   特殊情况:
    //     - data==nullptr / size==0 / size<3 → 空 vector
    //     - 完全没有起始码                    → 空 vector
    //     - 起始码后无 payload 字节            → 仍记 1 个 NAL,size=0,
    //                                          nal_unit_type=0
    //     - 截断尾部(最后一个 NAL 字节不完整)→ 记 1 个 NAL,size 反映可用
    //                                          字节数
    //
    // 副作用:
    //   无。不修改 data 指向的内存。
    //
    // 参考:H.264 spec Annex B (Byte stream format)
    std::vector<NALUnit> split(const uint8_t* data, size_t size);
};

}  // namespace parser
